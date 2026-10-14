#include "app/background_effect.h"

namespace oui_paint
{

BackgroundEffect::BackgroundEffect(olagarro::oui::Application& application, const PluginHost& host, Effect effect,
                                   const WorkImage& source, EffectContext context,
                                   std::function<void(const Values&, const WorkImage&)> done,
                                   std::function<void(const std::string&)> failed)
    : m_application{application}, m_effect{std::move(effect)}, m_source{source}, m_context{std::move(context)},
      m_done{std::move(done)}, m_failed{std::move(failed)}
{
    m_thread = std::thread([this, &host] { run(host); });
}

BackgroundEffect::~BackgroundEffect()
{
    static_cast<void>(stop());
}

void BackgroundEffect::request(Values values)
{
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        m_pending = std::move(values);
    }
    m_wanted.notify_one();
}

std::optional<std::pair<BackgroundEffect::Values, std::shared_ptr<const WorkImage>>> BackgroundEffect::stop()
{
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        m_stopping = true;
        m_pending.reset();
    }
    m_wanted.notify_one();
    if(m_thread.joinable())
    {
        m_thread.join();
    }
    m_alive.reset();
    return std::move(m_last);
}

void BackgroundEffect::run(const PluginHost& host)
{
    std::unique_ptr<PluginHost> copy;
    std::string problem;
    try
    {
        // Made here, before the first request is taken: the host it copies is not used while the dialog is open.
        copy = host.copy();
    }
    catch(const std::exception& failure)
    {
        problem = failure.what();
    }
    std::weak_ptr<int> alive;
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        alive = m_alive;
    }
    while(problem.empty())
    {
        Values values;
        {
            std::unique_lock<std::mutex> lock{m_mutex};
            m_wanted.wait(lock, [this] { return m_stopping || m_pending.has_value(); });
            if(m_stopping)
            {
                return;
            }
            values = std::move(*m_pending);
            m_pending.reset();
        }
        try
        {
            auto result{copy->apply(copy->same_effect(m_effect), m_source, values, m_context)};
            std::lock_guard<std::mutex> lock{m_mutex};
            m_last = std::pair{values, std::make_shared<const WorkImage>(std::move(result))};
            m_application.post(
                [this, alive, values]
                {
                    // Not stopped, so m_last is still this result or a newer one, and the thread only replaces it under
                    // the lock.
                    if(alive.expired())
                    {
                        return;
                    }
                    std::optional<std::pair<Values, std::shared_ptr<const WorkImage>>> shown;
                    {
                        std::lock_guard<std::mutex> lock{m_mutex};
                        if(!m_last || m_last->first != values)
                        {
                            return;
                        }
                        shown = m_last;
                    }
                    m_done(shown->first, *shown->second);
                });
        }
        catch(const std::exception& failure)
        {
            problem = failure.what();
        }
    }
    m_application.post(
        [this, alive, problem]
        {
            if(!alive.expired())
            {
                m_failed(problem);
            }
        });
}

} // namespace oui_paint
