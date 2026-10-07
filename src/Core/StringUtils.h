#pragma once

#include <string>
#include <string_view>

namespace cbx
{

std::string ToUtf8(std::wstring_view s);
std::wstring FromUtf8(std::string_view s);

std::wstring Trim(std::wstring_view s);
std::wstring ToLower(std::wstring_view s);
bool EqualsNoCase(std::wstring_view a, std::wstring_view b);

}	// namespace cbx
