#include "pch.h"
#include "AppState.h"
#include "LogBuffer.h"
#include "Log.h"
#include "StringUtils.h"

namespace fs = std::filesystem;

namespace app
{

AppState& AppState::Get()
{
	static AppState state;
	return state;
}

void AppState::Init()
{
	wchar_t exe[MAX_PATH];
	GetModuleFileNameW(nullptr, exe, MAX_PATH);
	exeDir = fs::path(exe).parent_path();
	iniPath = fs::path(exe).replace_extension(L".ini");
	std::error_code ec;
	fs::path legacyIni = exeDir / L"CbxConverter.ini";	// original CbxConverter in same directory
	if (!fs::exists(iniPath, ec) && fs::exists(legacyIni, ec))
	{
		settings.Read(legacyIni);
		settings.Write(iniPath);
	}
	else
	{
		settings.Read(iniPath);
	}

	cbx::Log::Instance().SetCallback([](const std::string& line) { LogBuffer::Get().Add(line); });
	engine = std::make_unique<cbx::Engine>(settings.worker.threadCount, settings.worker.priority);
	ApplySettings();
}

void AppState::ApplySettings()
{
	auto resolve = [&](cbx::Settings::Directories::DirectoryType type, const std::wstring& custom, const wchar_t* def) {
		fs::path p = (type == cbx::Settings::Directories::DirectoryTypeCustom && !custom.empty()) ? fs::path(custom) : exeDir / def;
		std::error_code ec;
		fs::create_directories(p, ec);
		if (ec)
			cbx::LOG("ERROR: Failed to create directory {}: {}", cbx::ToUtf8(p.native()), ec.message());
		return p;
	};
	tmpPath = resolve(settings.directories.tmpDirectoryType, settings.directories.customTmpDirectory, L"tmp");
	outPath = resolve(settings.directories.outDirectoryType, settings.directories.customOutDirectory, L"out");

	cbx::Log::Instance().SetFile(settings.logging.logToFile ? fs::path(iniPath).replace_extension(L".log") : fs::path());
	LogBuffer::Get().SetLimit(settings.logging.maxUiLogLines);

	engineReconfigurePending = !engine->Reconfigure(settings.worker.threadCount, settings.worker.priority);
	if (engineReconfigurePending)
		cbx::LOG("New worker thread settings will be applied when current jobs are finished");
}

void AppState::SaveSettings()
{
	if (!settings.Write(iniPath))
		cbx::LOG("Failed to write settings to {}", cbx::ToUtf8(iniPath.native()));
}

cbx::JobConfig AppState::MakeJobConfig() const
{
	return cbx::JobConfig::FromSettings(settings, outPath);
}

}	// namespace app
