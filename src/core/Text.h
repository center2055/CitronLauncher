#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace citron::text {

std::wstring toWide(std::string_view utf8);
std::string toUtf8(std::wstring_view wide);
std::string lower(std::string_view s);
std::string trim(std::string_view s);
std::vector<std::string> split(std::string_view s, char sep);
bool startsWith(std::string_view s, std::string_view prefix);
bool endsWith(std::string_view s, std::string_view suffix);
bool equalsIgnoreCase(std::string_view a, std::string_view b);
bool containsIgnoreCase(std::string_view haystack, std::string_view needle);

}
