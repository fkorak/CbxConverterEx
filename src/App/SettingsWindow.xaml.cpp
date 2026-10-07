#include "pch.h"
#include "SettingsWindow.xaml.h"
#if __has_include("SettingsWindow.g.cpp")
#include "SettingsWindow.g.cpp"
#endif

#include "SettingsControl.xaml.h"
#include "Dialogs.h"
#include "resource.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Windowing;

namespace winrt::CbxConverter::implementation
{
	void SettingsWindow::Show(HWND ownerWindow, cbx::Settings& s, std::function<void(bool)> callback)
	{
		owner = ownerWindow;
		settings = &s;
		onClose = std::move(callback);
		HWND hwnd = app::GetWindowHandle(*this);
		get_self<SettingsControl>(SettingsPanel())->Load(s, hwnd);

		HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
		if (icon)
			AppWindow().SetIcon(Microsoft::UI::GetIconIdFromIcon(icon));
		AppWindow().TitleBar().PreferredTheme(TitleBarTheme::UseDefaultAppMode);
		if (auto presenter = AppWindow().Presenter().try_as<OverlappedPresenter>())
		{
			presenter.IsMinimizable(false);
			presenter.IsMaximizable(false);
		}

		// size 900 x 780 (DIP), limited to work area, centered over owner
		UINT dpi = GetDpiForWindow(owner);
		RECT ownerRect{};
		GetWindowRect(owner, &ownerRect);
		auto area = DisplayArea::GetFromWindowId(Microsoft::UI::GetWindowIdFromWindow(owner), DisplayAreaFallback::Nearest).WorkArea();
		int w = std::min(MulDiv(900, dpi, 96), area.Width);
		int h = std::min(MulDiv(780, dpi, 96), area.Height);
		int x = (ownerRect.left + ownerRect.right - w) / 2;
		int y = (ownerRect.top + ownerRect.bottom - h) / 2;
		x = std::clamp(x, area.X, area.X + area.Width - w);
		y = std::clamp(y, area.Y, area.Y + area.Height - h);
		AppWindow().MoveAndResize({ x, y, w, h });

		// modal: owned by main window (stays above it), main window disabled while open
		SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
		EnableWindow(owner, FALSE);
		Closed([this](auto&&, auto&&) {
			EnableWindow(owner, TRUE);
			SetForegroundWindow(owner);
			if (onClose)
				onClose(applied);
		});
		Activate();
	}

	void SettingsWindow::OnApply(IInspectable const&, RoutedEventArgs const&)
	{
		get_self<SettingsControl>(SettingsPanel())->Save(*settings);
		applied = true;
		Close();
	}

	void SettingsWindow::OnCancel(IInspectable const&, RoutedEventArgs const&)
	{
		Close();
	}
}
