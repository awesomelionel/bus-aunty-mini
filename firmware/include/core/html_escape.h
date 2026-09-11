// firmware/include/core/html_escape.h
#pragma once
#include <string>

// Escapes the five characters that would break an HTML attribute or text
// node, and leaves everything else — including UTF-8 — untouched.
std::string escapeHtml(const std::string& raw);
