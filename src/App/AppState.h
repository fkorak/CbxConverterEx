#pragma once

#include "Engine.h"
#include "Settings.h"

#include <filesystem>
#include <memory>

namespace app
{

/** Application-wide state shared by windows and dialogs (UI thread only) */
struct AppState
{
	static AppState& Get();

	std::filesystem::path exeDir;
	std::filesystem::path iniPath;
	cbx::Settings settings;
	std::unique_ptr<cbx::Engine> engine;

	std::filesystem::path tmpPath;	///< resolved from settings
	std::filesystem::path outPath;	///< resolved from settings

	void Init();
	/** Apply settings to directories, logging and engine (engine is reconfigured when idle) */
	void ApplySettings();
	void SaveSettings();
	cbx::JobConfig MakeJobConfig() const;

	bool engineReconfigurePending = false;
};

}	// namespace app
