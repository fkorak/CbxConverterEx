#include "pch.h"
#include "ResizeControl.xaml.h"
#if __has_include("ResizeControl.g.cpp")
#include "ResizeControl.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::CbxConverter::implementation
{
	namespace
	{
		int ToInt(NumberBox const& box, int def)
		{
			double v = box.Value();
			return std::isnan(v) ? def : static_cast<int>(v + 0.5);
		}
	}

	void ResizeControl::Load(const cbx::ResizeCfg& cfg)
	{
		ModeCombo().SelectedIndex(cfg.mode);
		OversizeCombo().SelectedIndex(cfg.modeOnOversize);
		PctBox().Value(cfg.resizePct);
		ThresholdBox().Value(cfg.resizeThreshold);
		TargetBox().Value(cfg.resizeTarget);
		OnModeChanged(nullptr, nullptr);
	}

	cbx::ResizeCfg ResizeControl::Save()
	{
		cbx::ResizeCfg cfg;
		cfg.mode = static_cast<cbx::ResizeCfg::Mode>(std::max(0, ModeCombo().SelectedIndex()));
		cfg.modeOnOversize = static_cast<cbx::ResizeCfg::ModeOnOversizeSub>(std::max(0, OversizeCombo().SelectedIndex()));
		cfg.resizePct = ToInt(PctBox(), 100);
		cfg.resizeThreshold = ToInt(ThresholdBox(), cfg.resizeThreshold);
		cfg.resizeTarget = ToInt(TargetBox(), cfg.resizeTarget);
		return cfg;
	}

	void ResizeControl::OnModeChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		bool regular = ModeCombo().SelectedIndex() != cbx::ResizeCfg::ModeOnOversize;
		RegularPanel().Visibility(regular ? Visibility::Visible : Visibility::Collapsed);
		OversizePanel().Visibility(regular ? Visibility::Collapsed : Visibility::Visible);
	}

	void ResizeControl::OnPresetClick(IInspectable const& sender, RoutedEventArgs const&)
	{
		PctBox().Value(std::stoi(unbox_value<hstring>(sender.as<Button>().Tag()).c_str()));
	}
}
