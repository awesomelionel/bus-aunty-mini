#include "core/text_utils.h"

std::string truncateText(const std::string& text, int maxWidth,
                         std::function<int(const char*)> widthCallback) {
    if (text.empty()) {
        return text;
    }
    
    // Clean non-ASCII bytes (replace with '?')
    std::string cleaned;
    for (char c : text) {
        if (static_cast<unsigned char>(c) >= 0x20 &&
            static_cast<unsigned char>(c) <= 0x7E) {
            cleaned += c;
        } else {
            cleaned += '?';
        }
    }
    
    if (widthCallback(cleaned.c_str()) <= maxWidth) {
        return cleaned;
    }
    
    // Try cutting at word boundaries
    size_t lastSpace = 0;
    for (size_t i = 0; i < cleaned.size(); ++i) {
        if (cleaned[i] == ' ') {
            std::string candidate = cleaned.substr(0, i) + ".";
            if (widthCallback(candidate.c_str()) <= maxWidth) {
                lastSpace = i;
            } else {
                break;
            }
        }
    }
    
    if (lastSpace > 0) {
        return cleaned.substr(0, lastSpace) + ".";
    }
    
    // Hard cut if one word is too long
    for (size_t i = 1; i < cleaned.size(); ++i) {
        std::string candidate = cleaned.substr(0, i) + ".";
        if (widthCallback(candidate.c_str()) > maxWidth) {
            if (i > 1) {
                return cleaned.substr(0, i - 1) + ".";
            }
            return ".";
        }
    }
    
    // Fits without truncation
    return cleaned + ".";
}
