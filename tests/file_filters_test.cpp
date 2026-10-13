#include "app/file_filters.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(FileFilters, PatternsAreSeparatedAsOuiSeparatesThem)
{
    for(const auto* const filters : {&image_filters(), &palette_filters()})
    {
        for(const auto& filter : *filters)
        {
            std::size_t start{};
            while(start <= filter.patterns.size())
            {
                auto end{filter.patterns.find(';', start)};
                if(end == std::string::npos)
                {
                    end = filter.patterns.size();
                }
                const auto pattern{filter.patterns.substr(start, end - start)};
                EXPECT_EQ(pattern.rfind("*.", 0), 0u) << filter.name << ": \"" << pattern << "\"";
                EXPECT_EQ(pattern.find(' '), std::string::npos) << filter.name << ": \"" << pattern << "\"";
                start = end + 1;
            }
        }
    }
}
