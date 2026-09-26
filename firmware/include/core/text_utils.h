#pragma once
#include <functional>
#include <string>

// Truncates text to fit within maxWidth, appending "." if truncated.
// ASCII only (0x20-0x7E); non-ASCII bytes replaced with '?'.
// Uses widthCallback to measure text width (e.g., canvas.textWidth()).
std::string truncateText(const std::string& text, int maxWidth,
                         std::function<int(const char*)> widthCallback);
