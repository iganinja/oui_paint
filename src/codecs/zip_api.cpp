#include "codecs/zip_api.h"

#include <miniz.h>

namespace oui_paint::zip_api
{

std::optional<std::string> zip(const std::vector<Entry>& entries)
{
    mz_zip_archive archive{};
    if(!mz_zip_writer_init_heap(&archive, 0, 0))
    {
        return std::nullopt;
    }
    for(const auto& entry : entries)
    {
        const auto level{entry.stored ? MZ_NO_COMPRESSION : MZ_DEFAULT_LEVEL};
        if(!mz_zip_writer_add_mem(&archive, entry.name.c_str(), entry.bytes.data(), entry.bytes.size(),
                                  static_cast<mz_uint>(level)))
        {
            mz_zip_writer_end(&archive);
            return std::nullopt;
        }
    }
    void* buffer{};
    size_t size{};
    if(!mz_zip_writer_finalize_heap_archive(&archive, &buffer, &size))
    {
        mz_zip_writer_end(&archive);
        return std::nullopt;
    }
    std::string result{static_cast<const char*>(buffer), size};
    mz_zip_writer_end(&archive);
    return result;
}

std::optional<std::string> unzip(std::string_view zip, std::string_view name)
{
    mz_zip_archive archive{};
    if(!mz_zip_reader_init_mem(&archive, zip.data(), zip.size(), 0))
    {
        return std::nullopt;
    }
    size_t size{};
    const std::string wanted{name};
    auto* const data{mz_zip_reader_extract_file_to_heap(&archive, wanted.c_str(), &size, 0)};
    std::optional<std::string> result;
    if(data != nullptr)
    {
        result = std::string{static_cast<const char*>(data), size};
        mz_free(data);
    }
    mz_zip_reader_end(&archive);
    return result;
}

} // namespace oui_paint::zip_api
