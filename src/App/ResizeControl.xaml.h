#pragma once

#include "ResizeControl.g.h"
#include "ResizeCfg.h"

namespace winrt::CbxConverter::implementation
{
	struct ResizeControl : ResizeControlT<ResizeControl>
	{
		ResizeControl() = default;

		void Load(const cbx::ResizeCfg& cfg);
		cbx::ResizeCfg Save();

		void OnModeChanged(IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
		void OnPresetClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const&);
	};
}

namespace winrt::CbxConverter::factory_implementation
{
	struct ResizeControl : ResizeControlT<ResizeControl, implementation::ResizeControl>
	{
	};
}
