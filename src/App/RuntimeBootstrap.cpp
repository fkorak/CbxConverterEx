// Application entry point.
//
// The app uses the shared Windows App Runtime (framework-dependent build, keeps the
// app small). Before any WinUI code runs, the runtime is located with the bootstrapper;
// if it is missing or too old, the user is offered to download and install it
// (official Microsoft installer, silent mode) and the app then starts normally.

#include "pch.h"
#include "resource.h"

#ifndef CBX_SELF_CONTAINED

#include <MddBootstrap.h>
#include <WindowsAppSDK-VersionInfo.h>
#include <commctrl.h>
#include <urlmon.h>

#include <atomic>


#pragma comment(lib, "urlmon.lib")

namespace
{

#if defined(_M_ARM64)
constexpr const wchar_t* kArch = L"arm64";
#else
constexpr const wchar_t* kArch = L"x64";
#endif

const std::wstring kRuntimeName = std::format(L"Windows App Runtime {}.{}", WINDOWSAPPSDK_RELEASE_MAJOR, WINDOWSAPPSDK_RELEASE_MINOR);
const std::wstring kInstallerUrl = std::format(L"https://aka.ms/windowsappsdk/{}.{}/latest/windowsappruntimeinstall-{}.exe",
	WINDOWSAPPSDK_RELEASE_MAJOR, WINDOWSAPPSDK_RELEASE_MINOR, kArch);
const std::wstring kInfoUrl = L"https://learn.microsoft.com/windows/apps/windows-app-sdk/downloads";

HRESULT InitializeRuntime()
{
	PACKAGE_VERSION minVersion{};
	minVersion.Version = WINDOWSAPPSDK_RUNTIME_VERSION_UINT64;
	return MddBootstrapInitialize2(WINDOWSAPPSDK_RELEASE_MAJORMINOR, WINDOWSAPPSDK_RELEASE_VERSION_TAG_W, minVersion,
		MddBootstrapInitializeOptions_None);
}

HRESULT CALLBACK LinkCallback(HWND, UINT msg, WPARAM, LPARAM lParam, LONG_PTR)
{
	if (msg == TDN_HYPERLINK_CLICKED)
		ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(lParam), nullptr, nullptr, SW_SHOWNORMAL);
	return S_OK;
}

bool AskToInstall()
{
	std::wstring content = CBX_APP_NAME L" needs the " + kRuntimeName +
		L" from Microsoft. It is installed once and shared by all apps using it (about 120 MB download).";
	std::wstring footer = L"<a href=\"" + kInfoUrl + L"\">More information / manual download</a>";
	TASKDIALOG_BUTTON buttons[] = { { IDOK, L"Install now" }, { IDCANCEL, L"Exit" } };
	TASKDIALOGCONFIG cfg{ sizeof(cfg) };
	cfg.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT;
	cfg.pszWindowTitle = CBX_APP_NAME;
	cfg.pszMainIcon = TD_INFORMATION_ICON;
	cfg.pszMainInstruction = L"Windows App Runtime required";
	cfg.pszContent = content.c_str();
	cfg.pszFooter = footer.c_str();
	cfg.pszFooterIcon = nullptr;
	cfg.pButtons = buttons;
	cfg.cButtons = ARRAYSIZE(buttons);
	cfg.nDefaultButton = IDOK;
	cfg.pfCallback = LinkCallback;
	int button = IDCANCEL;
	if (FAILED(TaskDialogIndirect(&cfg, &button, nullptr, nullptr)))
		return false;
	return button == IDOK;
}

/** Download + silent installation, executed in worker thread; progress shown by task dialog */
struct Installer
{
	enum Stage
	{
		Downloading,
		Installing,
		Finished
	};
	std::atomic<Stage> stage = Downloading;
	std::atomic<ULONG> progress = 0;
	std::atomic<ULONG> progressMax = 0;
	std::atomic<bool> cancel = false;
	std::wstring error;	///< valid when finished

	void Run()
	{
		if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
		{
			Finish(L"COM initialization failed");
			return;
		}
		wchar_t tmp[MAX_PATH];
		GetTempPathW(MAX_PATH, tmp);
		std::wstring file = std::wstring(tmp) + L"WindowsAppRuntimeInstall-" + kArch + L".exe";

		struct Callback : IBindStatusCallback
		{
			Installer* owner;
			explicit Callback(Installer* o) : owner(o) {}
			STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
			{
				if (riid == IID_IUnknown || riid == IID_IBindStatusCallback)
				{
					*ppv = static_cast<IBindStatusCallback*>(this);
					return S_OK;
				}
				*ppv = nullptr;
				return E_NOINTERFACE;
			}
			STDMETHODIMP_(ULONG) AddRef() override { return 1; }
			STDMETHODIMP_(ULONG) Release() override { return 1; }
			STDMETHODIMP OnStartBinding(DWORD, IBinding*) override { return S_OK; }
			STDMETHODIMP GetPriority(LONG*) override { return E_NOTIMPL; }
			STDMETHODIMP OnLowResource(DWORD) override { return S_OK; }
			STDMETHODIMP OnProgress(ULONG progress, ULONG progressMax, ULONG, LPCWSTR) override
			{
				owner->progress = progress;
				owner->progressMax = progressMax;
				return owner->cancel ? E_ABORT : S_OK;
			}
			STDMETHODIMP OnStopBinding(HRESULT, LPCWSTR) override { return S_OK; }
			STDMETHODIMP GetBindInfo(DWORD* flags, BINDINFO*) override
			{
				*flags = BINDF_GETNEWESTVERSION | BINDF_NOWRITECACHE;
				return S_OK;
			}
			STDMETHODIMP OnDataAvailable(DWORD, DWORD, FORMATETC*, STGMEDIUM*) override { return S_OK; }
			STDMETHODIMP OnObjectAvailable(REFIID, IUnknown*) override { return S_OK; }
		} callback(this);

		HRESULT hr = URLDownloadToFileW(nullptr, kInstallerUrl.c_str(), file.c_str(), 0, &callback);
		if (cancel)
		{
			DeleteFileW(file.c_str());
			Finish(L"cancelled");
			CoUninitialize();
			return;
		}
		if (FAILED(hr))
		{
			Finish(std::format(L"Download failed (0x{:08X}).", static_cast<unsigned>(hr)));
			CoUninitialize();
			return;
		}

		stage = Installing;
		SHELLEXECUTEINFOW sei{ sizeof(sei) };
		sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
		sei.lpVerb = L"open";
		sei.lpFile = file.c_str();
		sei.lpParameters = L"--quiet";
		sei.nShow = SW_HIDE;
		if (!ShellExecuteExW(&sei) || !sei.hProcess)
		{
			DWORD err = GetLastError();
			Finish(err == ERROR_CANCELLED ? L"cancelled" : std::format(L"Could not start installer (error {}).", err));
		}
		else
		{
			WaitForSingleObject(sei.hProcess, INFINITE);
			DWORD exitCode = 0;
			GetExitCodeProcess(sei.hProcess, &exitCode);
			CloseHandle(sei.hProcess);
			Finish(exitCode == 0 ? L"" : std::format(L"Installer failed (exit code 0x{:08X}).", exitCode));
		}
		DeleteFileW(file.c_str());
		CoUninitialize();
	}

	void Finish(std::wstring err)
	{
		error = std::move(err);
		stage = Finished;
	}
};

HRESULT CALLBACK ProgressCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM, LONG_PTR refData)
{
	auto* inst = reinterpret_cast<Installer*>(refData);
	switch (msg)
	{
	case TDN_CREATED:
		SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(0, 1000));
		break;
	case TDN_TIMER:
		switch (inst->stage.load())
		{
		case Installer::Downloading:
		{
			ULONG max = inst->progressMax, cur = inst->progress;
			if (max)
			{
				SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_POS, static_cast<WPARAM>(1000ull * cur / max), 0);
				std::wstring text = std::format(L"Downloading {}... {} / {} MB", kRuntimeName, cur >> 20, max >> 20);
				SendMessageW(hwnd, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, reinterpret_cast<LPARAM>(text.c_str()));
			}
			break;
		}
		case Installer::Installing:
		{
			static bool switched = false;
			if (!switched)
			{
				switched = true;
				SendMessageW(hwnd, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
				SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 30);
				std::wstring text = L"Installing " + kRuntimeName + L"...";
				SendMessageW(hwnd, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, reinterpret_cast<LPARAM>(text.c_str()));
				SendMessageW(hwnd, TDM_ENABLE_BUTTON, IDCANCEL, FALSE);	// installation cannot be interrupted
			}
			break;
		}
		case Installer::Finished:
			SendMessageW(hwnd, TDM_CLICK_BUTTON, IDCANCEL, 0);
			break;
		}
		break;
	case TDN_BUTTON_CLICKED:
		if (wParam == IDCANCEL)
		{
			if (inst->stage == Installer::Finished)
				return S_OK;	// close dialog
			if (inst->stage != Installer::Downloading)
				return S_FALSE;	// keep dialog open
			inst->cancel = true;
			return S_FALSE;	// closes when worker thread finishes
		}
		break;
	}
	return S_OK;
}

/** \return empty string on success, "cancelled" or error description */
std::wstring DownloadAndInstall()
{
	Installer inst;
	HANDLE worker = CreateThread(nullptr, 0, [](LPVOID p) -> DWORD { static_cast<Installer*>(p)->Run(); return 0; }, &inst, 0, nullptr);
	if (!worker)
		return L"Could not start installation thread.";

	TASKDIALOG_BUTTON buttons[] = { { IDCANCEL, L"Cancel" } };
	TASKDIALOGCONFIG cfg{ sizeof(cfg) };
	cfg.dwFlags = TDF_SHOW_PROGRESS_BAR | TDF_CALLBACK_TIMER | TDF_SIZE_TO_CONTENT;
	cfg.dwCommonButtons = 0;
	cfg.pszWindowTitle = CBX_APP_NAME;
	cfg.pszMainInstruction = L"Installing Windows App Runtime";
	cfg.pszContent = L"Connecting...";
	cfg.pButtons = buttons;
	cfg.cButtons = ARRAYSIZE(buttons);
	cfg.pfCallback = ProgressCallback;
	cfg.lpCallbackData = reinterpret_cast<LONG_PTR>(&inst);
	TaskDialogIndirect(&cfg, nullptr, nullptr, nullptr);

	WaitForSingleObject(worker, INFINITE);
	CloseHandle(worker);
	return inst.error;
}

void ShowError(const std::wstring& text)
{
	std::wstring footer = L"<a href=\"" + kInfoUrl + L"\">Download the " + kRuntimeName + L" manually</a>";
	TASKDIALOGCONFIG cfg{ sizeof(cfg) };
	cfg.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT;
	cfg.pszWindowTitle = CBX_APP_NAME;
	cfg.pszMainIcon = TD_ERROR_ICON;
	cfg.pszMainInstruction = L"Windows App Runtime could not be installed";
	cfg.pszContent = text.c_str();
	cfg.pszFooter = footer.c_str();
	cfg.dwCommonButtons = TDCBF_CLOSE_BUTTON;
	cfg.pfCallback = LinkCallback;
	TaskDialogIndirect(&cfg, nullptr, nullptr, nullptr);
}

/** Make Windows App Runtime available to this process, installing it if needed */
bool EnsureRuntime()
{
	if (SUCCEEDED(InitializeRuntime()))
		return true;
	if (!AskToInstall())
		return false;
	std::wstring error = DownloadAndInstall();
	if (error == L"cancelled")
		return false;
	if (!error.empty())
	{
		ShowError(error);
		return false;
	}
	HRESULT hr = InitializeRuntime();
	if (FAILED(hr))
	{
		ShowError(std::format(L"The runtime was installed, but could not be loaded (0x{:08X}).", static_cast<unsigned>(hr)));
		return false;
	}
	return true;
}

}	// namespace

int __stdcall wXamlGeneratedMain();

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	if (!EnsureRuntime())
		return 1;
	int rc = wXamlGeneratedMain();
	MddBootstrapShutdown();
	return rc;
}

#else	// CBX_SELF_CONTAINED: runtime is shipped with the application

int __stdcall wXamlGeneratedMain();

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	return wXamlGeneratedMain();
}

#endif
