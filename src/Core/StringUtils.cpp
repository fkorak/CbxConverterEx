#include "StringUtils.h"

#include <windows.h>
#include <cwctype>

namespace cbx
{

std::string ToUtf8(std::wstring_view s)
{
	if (s.empty())
		return {};
	int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
	std::string out(len, '\0');
	WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
	return out;
}

std::wstring FromUtf8(std::string_view s)
{
	if (s.empty())
		return {};
	int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
	std::wstring out(len, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
	return out;
}

std::wstring Trim(std::wstring_view s)
{
	size_t b = 0, e = s.size();
	while (b < e && std::iswspace(s[b]))
		b++;
	while (e > b && std::iswspace(s[e - 1]))
		e--;
	return std::wstring(s.substr(b, e - b));
}

std::wstring ToLower(std::wstring_view s)
{
	std::wstring out(s);
	if (!out.empty())
		CharLowerBuffW(out.data(), static_cast<DWORD>(out.size()));
	return out;
}

bool EqualsNoCase(std::wstring_view a, std::wstring_view b)
{
	return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

}	// namespace cbx
