#pragma once

#include "effects/plugins.h"

#include <oui/application.h>

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace oui_paint
{

// An effect made again on a thread of its own each time its values change, so a dialog never waits for it: with the
// latest values only, what was asked for in between skipped. Each result goes to `done` on the application's thread,
// and what went wrong to `failed`, after which nothing more is made. The thread has its own copy of the plugins, as an
// engine runs on the thread that made it. `source` must outlive it.
class BackgroundEffect
{
  public:
    using Values = std::map<std::string, double>;

    BackgroundEffect(olagarro::oui::Application& application, const PluginHost& host, Effect effect,
                     const WorkImage& source, EffectContext context,
                     std::function<void(const Values& values, const WorkImage& result)> done,
                     std::function<void(const std::string& problem)> failed);
    // Waits for what is being made, if anything.
    ~BackgroundEffect();
    BackgroundEffect(const BackgroundEffect&) = delete;
    BackgroundEffect& operator=(const BackgroundEffect&) = delete;

    // Makes it with these values once what is being made is done.
    void request(Values values);
    // Waits for what is being made and makes nothing more; the last result made, with its values, which `done` may
    // not have been given yet.
    [[nodiscard]] std::optional<std::pair<Values, std::shared_ptr<const WorkImage>>> stop();

  private:
    void run(const PluginHost& host);

    olagarro::oui::Application& m_application;
    Effect m_effect;
    const WorkImage& m_source;
    EffectContext m_context;
    std::function<void(const Values&, const WorkImage&)> m_done;
    std::function<void(const std::string&)> m_failed;

    std::mutex m_mutex;
    std::condition_variable m_wanted;
    std::optional<Values> m_pending;
    bool m_stopping{false};
    std::optional<std::pair<Values, std::shared_ptr<const WorkImage>>> m_last;
    // Gone once stopped, for the results posted that the application has not run yet.
    std::shared_ptr<int> m_alive{std::make_shared<int>(0)};
    std::thread m_thread;
};

} // namespace oui_paint
