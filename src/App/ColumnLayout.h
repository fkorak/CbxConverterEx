#pragma once

#include "ColumnLayout.g.h"
#include "ColumnGripper.g.h"

#include <array>

namespace winrt::CbxConverter::implementation
{
	struct ColumnLayout : ColumnLayoutT<ColumnLayout>
	{
		static constexpr size_t COUNT = 9;
		static constexpr std::array<double, COUNT> DEFAULT_WIDTHS = { 280, 76, 60, 72, 150, 96, 170, 80, 70 };
		static constexpr double MIN_WIDTH = 32;

		ColumnLayout() = default;

		Microsoft::UI::Xaml::GridLength Col0() const { return Get(0); }
		Microsoft::UI::Xaml::GridLength Col1() const { return Get(1); }
		Microsoft::UI::Xaml::GridLength Col2() const { return Get(2); }
		Microsoft::UI::Xaml::GridLength Col3() const { return Get(3); }
		Microsoft::UI::Xaml::GridLength Col4() const { return Get(4); }
		Microsoft::UI::Xaml::GridLength Col5() const { return Get(5); }
		Microsoft::UI::Xaml::GridLength Col6() const { return Get(6); }
		Microsoft::UI::Xaml::GridLength Col7() const { return Get(7); }
		Microsoft::UI::Xaml::GridLength Col8() const { return Get(8); }

		double Width(size_t column) const { return widths[column]; }
		void SetWidth(size_t column, double width);
		void Reset(size_t column) { SetWidth(column, DEFAULT_WIDTHS[column]); }

		/** Persisted form: comma separated widths in DIP; invalid / missing entries use defaults */
		std::wstring ToString() const;
		void FromString(const std::wstring& s);

		winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
		{
			return propertyChanged.add(handler);
		}
		void PropertyChanged(winrt::event_token const& token) noexcept
		{
			propertyChanged.remove(token);
		}

	private:
		Microsoft::UI::Xaml::GridLength Get(size_t column) const
		{
			return Microsoft::UI::Xaml::GridLengthHelper::FromPixels(widths[column]);
		}

		std::array<double, COUNT> widths = DEFAULT_WIDTHS;
		winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged;
	};

	struct ColumnGripper : ColumnGripperT<ColumnGripper>
	{
		ColumnGripper();
	};
}

namespace winrt::CbxConverter::factory_implementation
{
	struct ColumnGripper : ColumnGripperT<ColumnGripper, implementation::ColumnGripper>
	{
	};
}
