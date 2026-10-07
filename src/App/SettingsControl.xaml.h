#pragma once

#include "SettingsControl.g.h"
#include "Settings.h"

namespace winrt::CbxConverter::implementation
{
	struct SettingsControl : SettingsControlT<SettingsControl>
	{
		SettingsControl() = default;

		void Load(const cbx::Settings& settings, HWND owner);
		void Save(cbx::Settings& settings);

		void OnSectionChanged(Microsoft::UI::Xaml::Controls::SelectorBar const&, Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const&);
		void OnFormatChanged(IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
		void OnAddRule(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnBrowse7z(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnBrowseGs(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnBrowseTmpDir(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnBrowseOutDir(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnDirectoryOptionChanged(IInspectable const&, IInspectable const&);

	private:
		struct RuleRow
		{
			Microsoft::UI::Xaml::Controls::Grid grid{ nullptr };
			Microsoft::UI::Xaml::Controls::NumberBox width{ nullptr };
			Microsoft::UI::Xaml::Controls::NumberBox pct{ nullptr };
		};
		void AddRuleRow(int width, int pct);
		void UpdateDirectoriesPage();
		void Update7zHint();

		HWND owner = nullptr;
		std::vector<RuleRow> rules;
		unsigned int maxRules = 10;
	};
}

namespace winrt::CbxConverter::factory_implementation
{
	struct SettingsControl : SettingsControlT<SettingsControl, implementation::SettingsControl>
	{
	};
}
