#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

#include "core/backoff_scheduler.h"
#include "core/night_window.h"
#include "core/text_utils.h"

// Header for the arrivals screen.
// At a data age of kHeaderStaleNoteMs or more, the time of UpdatedAt is
// appended as " as of HH:MM". When that does not fit it shortens to " HH:MM".
// The page indicator is dropped before the stop name is trimmed, and the
// time is kept until the name has been trimmed away.
template <typename WidthFunc>
std::string buildHeader(const std::string& stopName, size_t currentPage,
                        size_t totalPages, uint32_t dataAgeMs,
                        int64_t updatedAtEpoch, int maxWidth,
                        WidthFunc widthCallback) {
    std::string longSuffix;
    std::string shortSuffix;
    if (dataAgeMs >= kHeaderStaleNoteMs && updatedAtEpoch >= 0) {
        const std::string hm = formatLocalHm(updatedAtEpoch);
        if (!hm.empty()) {
            longSuffix = " as of " + hm;
            shortSuffix = " " + hm;
        }
    }

    char pageIndicator[16];
    std::snprintf(pageIndicator, sizeof(pageIndicator), " (%zu/%zu)",
                  currentPage + 1, totalPages);

    if (longSuffix.empty()) {
        const std::string full = stopName + pageIndicator;
        if (widthCallback(full.c_str()) <= maxWidth) {
            return full;
        }
        return truncateText(stopName, maxWidth, widthCallback);
    }

    const std::string withPage = stopName + pageIndicator + longSuffix;
    if (widthCallback(withPage.c_str()) <= maxWidth) {
        return withPage;
    }
    const std::string withLong = stopName + longSuffix;
    if (widthCallback(withLong.c_str()) <= maxWidth) {
        return withLong;
    }
    const std::string withShort = stopName + shortSuffix;
    if (widthCallback(withShort.c_str()) <= maxWidth) {
        return withShort;
    }

    const int suffixWidth = widthCallback(shortSuffix.c_str());
    const int nameMaxWidth = maxWidth - suffixWidth;
    if (nameMaxWidth <= 0) {
        return shortSuffix;
    }
    return truncateText(stopName, nameMaxWidth, widthCallback) + shortSuffix;
}
