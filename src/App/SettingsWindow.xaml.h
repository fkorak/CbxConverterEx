#pragma once

#include "SettingsWindow.g.h"
#include "Settings.h"

#include <functional>

namespace winrt::CbxConverter::implementation
{
	/** Modal settings window (owner is disabled while it is open) */
	struct SettingsWindow : SettingsWindowT<SettingsWindow>
	{
		SettingsWindow() = default;

		/** \param onClose called after window is closed, with true if settings were applied */
		void Show(HWND owner, cbx::Settings& settings, std::function<void(bool applied)> onClose);

		void OnApply(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnCancel(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		HWND owner = nullptr;
		cbx::Settings* settings = nullptr;
		std::function<void(bool)> onClose;
		bool applied = false;
	};
}

namespace winrt::CbxConverter::factory_implementation
{
	struct SettingsWindow : SettingsWindowT<SettingsWindow, implementation::SettingsWindow>
	{
	};
}
