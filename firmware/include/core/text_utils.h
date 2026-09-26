#pragma once
#include <string>

// Truncates text to fit within maxWidth, appending "." if truncated.
// ASCII only (0x20-0x7E); non-ASCII bytes replaced with '?'.
// Uses widthCallback to measure text width (e.g., canvas.textWidth()).
template<typename WidthFunc>
std::string truncateText(const std::string& text, int maxWidth,
                         WidthFunc widthCallback) {
    if (text.empty()) {
        return text;
    }
    
    // Clean non-ASCII bytes: skip UTF-8 continuation bytes (0x80-0xBF),
    // replace non-ASCII starting bytes with one '?'
    std::string cleaned;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c >= 0x20 && c <= 0x7E) {
            cleaned += text[i];
        } else if ((c & 0xC0) != 0x80) {
            // Not a continuation byte (0x80-0xBF), so start of a multi-byte char
            cleaned += '?';
        }
        // Skip continuation bytes
    }
    
    if (widthCallback(cleaned.c_str()) <= maxWidth) {
        return cleaned;
    }
    
    // Try cutting at word boundaries
    size_t lastSpace = 0;
    for (size_t i = 0; i < cleaned.size(); ++i) {
        if (cleaned[i] == ' ') {
            // Strip trailing '.' and spaces before appending "."
            std::string prefix = cleaned.substr(0, i);
            while (!prefix.empty() && 
                   (prefix.back() == '.' || prefix.back() == ' ')) {
                prefix.pop_back();
            }
            std::string candidate = prefix + ".";
            if (widthCallback(candidate.c_str()) <= maxWidth) {
                lastSpace = i;
            } else {
                break;
            }
        }
    }
    
    if (lastSpace > 0) {
        std::string prefix = cleaned.substr(0, lastSpace);
        while (!prefix.empty() && 
               (prefix.back() == '.' || prefix.back() == ' ')) {
            prefix.pop_back();
        }
        return prefix + ".";
    }
    
    // Hard cut if one word is too long
    for (size_t i = 1; i < cleaned.size(); ++i) {
        // Strip trailing '.' before appending
        std::string prefix = cleaned.substr(0, i);
        while (!prefix.empty() && prefix.back() == '.') {
            prefix.pop_back();
        }
        if (prefix.empty()) {
            return ".";
        }
        std::string candidate = prefix + ".";
        if (widthCallback(candidate.c_str()) > maxWidth) {
            if (i > 1) {
                // Back up and try again
                prefix = cleaned.substr(0, i - 1);
                while (!prefix.empty() && prefix.back() == '.') {
                    prefix.pop_back();
                }
                if (prefix.empty()) {
                    return ".";
                }
                // Re-check that it fits
                candidate = prefix + ".";
                while (widthCallback(candidate.c_str()) > maxWidth && !prefix.empty()) {
                    prefix.pop_back();
                    candidate = prefix + ".";
                }
                if (prefix.empty()) {
                    return ".";
                }
                return candidate;
            }
            return ".";
        }
    }
    
    // Last resort: append "." to full cleaned string, re-check width
    std::string result = cleaned + ".";
    while (widthCallback(result.c_str()) > maxWidth && result.size() > 1) {
        result.pop_back();  // Remove last char before "."
        result.pop_back();  // Remove "."
        if (!result.empty() && result.back() == '.') {
            result.pop_back();  // Strip trailing "." again
        }
        if (result.empty()) {
            return ".";
        }
        result += ".";
    }
    return result;
}
