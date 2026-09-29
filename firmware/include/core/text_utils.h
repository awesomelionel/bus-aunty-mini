#pragma once
#include <string>

// The display fonts only cover printable ASCII, so text is reduced to that
// before it is measured or drawn. Curly quotes (U+2018/U+2019, which the feed
// uses in "Michael's" and phone keyboards type for an apostrophe) become a
// plain '; any other non-ASCII character becomes one '?'.
inline std::string toDisplayAscii(const std::string& text) {
    std::string cleaned;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == 0xE2 && i + 2 < text.size() &&
            static_cast<unsigned char>(text[i + 1]) == 0x80 &&
            (static_cast<unsigned char>(text[i + 2]) == 0x98 ||
             static_cast<unsigned char>(text[i + 2]) == 0x99)) {
            cleaned += '\'';
            i += 2;
            continue;
        }
        if (c >= 0x20 && c <= 0x7E) {
            cleaned += text[i];
        } else if ((c & 0xC0) != 0x80) {
            // Not a continuation byte (0x80-0xBF), so start of a multi-byte char
            cleaned += '?';
        }
        // Skip continuation bytes
    }
    return cleaned;
}

// Truncates text to fit within maxWidth, appending "." if truncated.
// Cleaned with toDisplayAscii first.
// Uses widthCallback to measure text width (e.g., canvas.textWidth()).
// `breakOnWords` stops at the last word that fits. The inline destination
// passes false: a short first word ("St.") would otherwise leave most of
// the gutter blank.
template<typename WidthFunc>
std::string truncateText(const std::string& text, int maxWidth,
                         WidthFunc widthCallback, bool breakOnWords = true) {
    if (text.empty()) {
        return text;
    }
    
    const std::string cleaned = toDisplayAscii(text);

    if (widthCallback(cleaned.c_str()) <= maxWidth) {
        return cleaned;
    }
    
    // Try cutting at word boundaries. Skipped when the caller would rather
    // fill the width than stop after a short first word.
    size_t lastSpace = 0;
    if (breakOnWords) {
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
        // Strip a trailing '.' or space so the cut is "Serangoon." and
        // not "Serangoon .".
        std::string prefix = cleaned.substr(0, i);
        while (!prefix.empty() &&
               (prefix.back() == '.' || prefix.back() == ' ')) {
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
                while (!prefix.empty() &&
                       (prefix.back() == '.' || prefix.back() == ' ')) {
                    prefix.pop_back();
                }
                if (prefix.empty()) {
                    return ".";
                }
                // Re-check that it fits
                candidate = prefix + ".";
                while (widthCallback(candidate.c_str()) > maxWidth && !prefix.empty()) {
                    prefix.pop_back();
                    while (!prefix.empty() &&
                           (prefix.back() == '.' || prefix.back() == ' ')) {
                        prefix.pop_back();
                    }
                    candidate = prefix.empty() ? "." : prefix + ".";
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
        while (!result.empty() &&
               (result.back() == '.' || result.back() == ' ')) {
            result.pop_back();
        }
        if (result.empty()) {
            return ".";
        }
        result += ".";
    }
    return result;
}
