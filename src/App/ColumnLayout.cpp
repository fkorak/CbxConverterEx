#include "pch.h"
#include "ColumnLayout.h"
#if __has_include("ColumnLayout.g.cpp")
#include "ColumnLayout.g.cpp"
#endif
#if __has_include("ColumnGripper.g.cpp")
#include "ColumnGripper.g.cpp"
#endif

#include <sstream>

namespace winrt::CbxConverter::implementation
{
	void ColumnLayout::SetWidth(size_t column, double width)
	{
		if (column >= COUNT)
			return;
		width = std::round(std::max(width, MIN_WIDTH));
		if (widths[column] == width)
			return;
		widths[column] = width;
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(L"Col" + std::to_wstring(column)));
	}

	std::wstring ColumnLayout::ToString() const
	{
		std::wstring s;
		for (size_t i = 0; i < COUNT; i++)
		{
			if (i)
				s += L',';
			s += std::to_wstring(static_cast<int>(widths[i]));
		}
		return s;
	}

	void ColumnLayout::FromString(const std::wstring& s)
	{
		std::wstringstream ss(s);
		std::wstring part;
		for (size_t i = 0; i < COUNT && std::getline(ss, part, L','); i++)
		{
			try
			{
				double w = std::stod(part);
				if (w >= MIN_WIDTH && w <= 5000)
					SetWidth(i, w);
			}
			catch (...)
			{
			}
		}
	}

	ColumnGripper::ColumnGripper()
	{
		ProtectedCursor(Microsoft::UI::Input::InputSystemCursor::Create(Microsoft::UI::Input::InputSystemCursorShape::SizeWestEast));
	}
}
