#include "pch.h"
#include "LogWindow.xaml.h"
#if __has_include("LogWindow.g.cpp")
#include "LogWindow.g.cpp"
#endif

#include "AppState.h"
#include "LogBuffer.h"
#include "StringUtils.h"
#include "resource.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace std::chrono_literals;

namespace winrt::CbxConverter::implementation
{
	void LogWindow::InitializeComponent()
	{
		LogWindowT::InitializeComponent();
		lines = single_threaded_observable_vector<IInspectable>();
		LogList().ItemsSource(lines);

		HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
		if (icon)
			AppWindow().SetIcon(Microsoft::UI::GetIconIdFromIcon(icon));
		UINT dpi = 96;
		HWND hwnd = nullptr;
		if (SUCCEEDED(try_as<::IWindowNative>()->get_WindowHandle(&hwnd)) && hwnd)
			dpi = GetDpiForWindow(hwnd);
		AppWindow().Resize({ MulDiv(900, dpi, 96), MulDiv(500, dpi, 96) });
		AppWindow().TitleBar().PreferredTheme(Microsoft::UI::Windowing::TitleBarTheme::UseDefaultAppMode);

		Poll();
		timer = DispatcherQueue().CreateTimer();
		timer.Interval(200ms);
		timer.Tick([this](auto&&, auto&&) { Poll(); });
		timer.Start();
		Closed([this](auto&&, auto&&) { timer.Stop(); });
	}

	void LogWindow::Poll()
	{
		std::vector<std::string> fresh;
		lastSeq = app::LogBuffer::Get().Fetch(lastSeq, fresh);
		if (fresh.empty())
			return;
		for (const auto& line : fresh)
			lines.Append(box_value(cbx::FromUtf8(line)));
		size_t limit = app::AppState::Get().settings.logging.maxUiLogLines;
		while (lines.Size() > limit)
			lines.RemoveAt(0);
		if (AutoScrollCheck().IsChecked().Value() && lines.Size())
			LogList().ScrollIntoView(lines.GetAt(lines.Size() - 1));
	}

	void LogWindow::OnCopy(IInspectable const&, RoutedEventArgs const&)
	{
		// selected lines, or everything if nothing is selected
		std::wstring text;
		auto selected = LogList().SelectedItems();
		auto source = selected.Size() ? selected : lines.as<Windows::Foundation::Collections::IVector<IInspectable>>();
		for (auto const& item : source)
		{
			text += unbox_value<hstring>(item);
			text += L"\r\n";
		}
		Windows::ApplicationModel::DataTransfer::DataPackage package;
		package.SetText(text);
		Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
	}

	void LogWindow::OnCopyAccelerator(Input::KeyboardAccelerator const&, Input::KeyboardAcceleratorInvokedEventArgs const& e)
	{
		OnCopy(nullptr, nullptr);
		e.Handled(true);
	}

	void LogWindow::OnClear(IInspectable const&, RoutedEventArgs const&)
	{
		app::LogBuffer::Get().Clear();
		lines.Clear();
	}
}
