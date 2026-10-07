#include "FileNameMatch.h"
#include "StringUtils.h"

namespace cbx
{

namespace
{

bool Match(std::wstring_view s, std::wstring_view p)
{
	// iterative matcher with single backtrack point for '*'
	size_t si = 0, pi = 0, starP = std::wstring_view::npos, starS = 0;
	while (si < s.size())
	{
		if (pi < p.size() && (p[pi] == L'?' || p[pi] == s[si]))
		{
			si++;
			pi++;
		}
		else if (pi < p.size() && p[pi] == L'*')
		{
			starP = pi++;
			starS = si;
		}
		else if (starP != std::wstring_view::npos)
		{
			pi = starP + 1;
			si = ++starS;
		}
		else
		{
			return false;
		}
	}
	while (pi < p.size() && p[pi] == L'*')
		pi++;
	return pi == p.size();
}

}	// namespace

bool MatchPattern(std::wstring_view name, std::wstring_view pattern)
{
	return Match(ToLower(name), ToLower(pattern));
}

bool MatchMultiplePatterns(std::wstring_view name, std::wstring_view patterns)
{
	size_t start = 0;
	while (start <= patterns.size())
	{
		size_t end = patterns.find(L';', start);
		if (end == std::wstring_view::npos)
			end = patterns.size();
		std::wstring p = Trim(patterns.substr(start, end - start));
		if (!p.empty() && MatchPattern(name, p))
			return true;
		start = end + 1;
	}
	return false;
}

}	// namespace cbx
