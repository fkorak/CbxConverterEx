#pragma once

#include "LogWindow.g.h"

namespace winrt::CbxConverter::implementation
{
	struct LogWindow : LogWindowT<LogWindow>
	{
		LogWindow() = default;
		void InitializeComponent();

		void OnCopy(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnClear(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnCopyAccelerator(Microsoft::UI::Xaml::Input::KeyboardAccelerator const&, Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& e);

	private:
		void Poll();

		Windows::Foundation::Collections::IObservableVector<IInspectable> lines;
		Microsoft::UI::Dispatching::DispatcherQueueTimer timer{ nullptr };
		uint64_t lastSeq = 0;
	};
}

namespace winrt::CbxConverter::factory_implementation
{
	struct LogWindow : LogWindowT<LogWindow, implementation::LogWindow>
	{
	};
}
