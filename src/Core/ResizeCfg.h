#pragma once

namespace cbx
{

struct ResizeCfg
{
	enum Mode
	{
		ModeRegular = 0,	///< same, proportional resizing for all images
		ModeOnOversize		///< resize only if image exceeds threshold
	} mode = ModeRegular;
	enum ModeOnOversizeSub
	{
		ModeOnOversizeWiderOrHigher = 0,
		ModeOnOversizeWider,
		ModeOnOversizeHigher
	} modeOnOversize = ModeOnOversizeWiderOrHigher;
	int resizePct = 100;
	int resizeThreshold = 1366;
	int resizeTarget = 1366;

	/** Calculate target size for image; returns false if image should not be resized */
	bool GetTargetSize(int width, int height, int& newWidth, int& newHeight) const
	{
		newWidth = width;
		newHeight = height;
		if (width <= 0 || height <= 0)
			return false;
		if (mode == ModeRegular)
		{
			if (resizePct == 100 || resizePct <= 0)
				return false;
			newWidth = width * resizePct / 100;
			newHeight = height * resizePct / 100;
		}
		else
		{
			int maxDimension = 0;
			switch (modeOnOversize)
			{
			case ModeOnOversizeWider:
				maxDimension = width;
				break;
			case ModeOnOversizeHigher:
				maxDimension = height;
				break;
			default:
				maxDimension = width > height ? width : height;
				break;
			}
			if (maxDimension < resizeThreshold || resizeTarget <= 0)
				return false;
			float scaling = static_cast<float>(resizeTarget) / static_cast<float>(maxDimension);
			newWidth = static_cast<int>(scaling * width + 0.5f);
			newHeight = static_cast<int>(scaling * height + 0.5f);
		}
		if (newWidth < 1)
			newWidth = 1;
		if (newHeight < 1)
			newHeight = 1;
		return newWidth != width || newHeight != height;
	}
};

}	// namespace cbx
