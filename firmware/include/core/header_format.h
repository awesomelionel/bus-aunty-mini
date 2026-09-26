#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

#include "core/text_utils.h"

// Builds and truncates a header string from stop name, page indicator, and age.
// Priority: always keep age, drop page indicator first, then trim name.
// width_callback measures text width (e.g., canvas.textWidth()).
// Returns: formatted header that fits in maxWidth.
template<typename WidthFunc>
std::string buildHeader(const std::string& stopName, size_t currentPage,
                        size_t totalPages, uint32_t dataAgeMs, int maxWidth,
                        WidthFunc widthCallback) {
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
    
    // Truncate name to fit with age (always keep age)
    // Calculate width available for name
    int ageWidth = widthCallback(ageSuffix.c_str());
    int nameMaxWidth = maxWidth - ageWidth;
    
    // Always keep age, even if it means name becomes a single char or empty
    if (nameMaxWidth <= 0) {
        return ageSuffix;  // Only age fits
    }
    
    std::string truncatedName = truncateText(stopName, nameMaxWidth, widthCallback);
    return truncatedName + ageSuffix;
}
