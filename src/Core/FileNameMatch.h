#pragma once

#include <string_view>

namespace cbx
{

/** DOS-like wildcard match (* and ?), case-insensitive */
bool MatchPattern(std::wstring_view name, std::wstring_view pattern);

/** Match against multiple patterns separated with semicolon, e.g. "*.webp;*.nfo" */
bool MatchMultiplePatterns(std::wstring_view name, std::wstring_view patterns);

}	// namespace cbx
