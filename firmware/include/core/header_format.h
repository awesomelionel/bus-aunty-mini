#pragma once
#include <cstdint>
#include <functional>
#include <string>

// Builds and truncates a header string from stop name, page indicator, and age.
// Priority: always keep age, drop page indicator first, then trim name.
// width_callback measures text width (e.g., canvas.textWidth()).
// Returns: formatted header that fits in maxWidth.
std::string buildHeader(const std::string& stopName, size_t currentPage,
                        size_t totalPages, uint32_t dataAgeMs, int maxWidth,
                        std::function<int(const char*)> widthCallback);
