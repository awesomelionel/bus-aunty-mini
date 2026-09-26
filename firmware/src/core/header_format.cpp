#include "core/header_format.h"

#include <cstdio>

#include "core/text_utils.h"

std::string buildHeader(const std::string& stopName, size_t currentPage,
                        size_t totalPages, uint32_t dataAgeMs, int maxWidth,
                        std::function<int(const char*)> widthCallback) {
    // Build age suffix (empty if data is fresh)
    std::string ageSuffix;
    if (dataAgeMs > 60000) {
        uint32_t ageSec = dataAgeMs / 1000;
        char buf[16];
        if (ageSec < 120) {
            std::snprintf(buf, sizeof(buf), " %us", static_cast<unsigned>(ageSec));
        } else {
            std::snprintf(buf, sizeof(buf), " %um",
                         static_cast<unsigned>(ageSec / 60));
        }
        ageSuffix = buf;
    }
    
    // Build page indicator
    char pageIndicator[16];
    std::snprintf(pageIndicator, sizeof(pageIndicator), " (%zu/%zu)",
                 currentPage + 1, totalPages);
    
    // Try full header: name + page + age
    std::string full = stopName + pageIndicator + ageSuffix;
    if (widthCallback(full.c_str()) <= maxWidth) {
        return full;
    }
    
    // Drop page indicator, keep age: name + age
    std::string withoutPage = stopName + ageSuffix;
    if (widthCallback(withoutPage.c_str()) <= maxWidth) {
        return withoutPage;
    }
    
    // Truncate name to fit with age
    // Calculate width available for name
    int ageWidth = widthCallback(ageSuffix.c_str());
    int nameMaxWidth = maxWidth - ageWidth;
    
    if (nameMaxWidth < 10) {
        // Not enough room for name + age, just show truncated name without age
        return truncateText(stopName, maxWidth, widthCallback);
    }
    
    std::string truncatedName = truncateText(stopName, nameMaxWidth, widthCallback);
    return truncatedName + ageSuffix;
}
