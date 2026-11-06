#include "GameCoreImports.h"

#include <windows.h>
#include <delayimp.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>

namespace oc4::host
{
	namespace
	{
		HMODULE g_gameCore = nullptr;
		HMODULE gameCore() { return g_gameCore; }

		FARPROC WINAPI delayLoadHook(unsigned notify, PDelayLoadInfo info)
		{
			return notify == dliNotePreLoadLibrary && _stricmp(info->szDll, OC4_GAME_CORE_DLL) == 0 ? reinterpret_cast<FARPROC>(g_gameCore) : nullptr;
		}

		// Points the game core's imports of the named functions at replacements.
		template <size_t N>
		void patchImports(HMODULE module, const char* const (&names)[N], const void* const (&replacements)[N])
		{
			auto* base = reinterpret_cast<BYTE*>(module);
			if (!base)
				return;
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
			const IMAGE_DATA_DIRECTORY& imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
			if (!imports.Size)
				return;
			for (auto* dll = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress); dll->Name; ++dll)
			{
				const auto* name = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + dll->OriginalFirstThunk);
				auto* slot = reinterpret_cast<IMAGE_THUNK_DATA*>(base + dll->FirstThunk);
				for (; name->u1.AddressOfData; ++name, ++slot)
				{
					if (IMAGE_SNAP_BY_ORDINAL(name->u1.Ordinal))
						continue;
					const char* fn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + name->u1.AddressOfData)->Name;
					const auto it = std::find_if(std::begin(names), std::end(names), [&](const char* n) { return std::strcmp(n, fn) == 0; });
					if (it == std::end(names))
						continue;
					DWORD protect = 0;
					VirtualProtect(&slot->u1.Function, sizeof(slot->u1.Function), PAGE_READWRITE, &protect);
					slot->u1.Function = reinterpret_cast<ULONG_PTR>(replacements[it - std::begin(names)]);
					VirtualProtect(&slot->u1.Function, sizeof(slot->u1.Function), protect, &protect);
				}
			}
		}

		// ---------------------------------------------------------------- message boxes
		int answer(UINT type)
		{
			static const int buttons[][3] = {
				{ IDOK, IDOK, IDOK },             // MB_OK
				{ IDOK, IDCANCEL, IDCANCEL },     // MB_OKCANCEL
				{ IDIGNORE, IDIGNORE, IDIGNORE }, // MB_ABORTRETRYIGNORE: carry on
				{ IDYES, IDNO, IDCANCEL },        // MB_YESNOCANCEL
				{ IDYES, IDNO, IDNO },            // MB_YESNO
				{ IDRETRY, IDCANCEL, IDCANCEL },  // MB_RETRYCANCEL
			};
			const UINT kind = type & MB_TYPEMASK, button = (type & MB_DEFMASK) >> 8;
			return kind < std::size(buttons) && button < 3 ? buttons[kind][button] : IDOK;
		}

		int WINAPI messageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type)
		{
			std::fprintf(stderr, "[oc4core] game core message box \"%s\": %s\n", caption ? caption : "", text ? text : "");
			return answer(type);
		}

		int WINAPI messageBoxW(HWND, LPCWSTR text, LPCWSTR caption, UINT type)
		{
			std::fprintf(stderr, "[oc4core] game core message box \"%ls\": %ls\n", caption ? caption : L"", text ? text : L"");
			return answer(type);
		}

		LPTOP_LEVEL_EXCEPTION_FILTER WINAPI setUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER) { return nullptr; }

		// ---------------------------------------------------------------- module paths
		std::wstring g_exePath, g_gameCorePath;
		std::string g_exePathA, g_gameCorePathA;

		template <class Char>
		DWORD copyPath(const std::basic_string<Char>& path, Char* buffer, DWORD size)
		{
			// GetModuleFileName's contract: truncate with a terminator and report the size.
			if (size == 0)
			{
				SetLastError(ERROR_INSUFFICIENT_BUFFER);
				return 0;
			}
			const DWORD n = std::min<DWORD>(static_cast<DWORD>(path.size()), size - 1);
			std::copy_n(path.data(), n, buffer);
			buffer[n] = 0;
			if (path.size() >= size)
			{
				SetLastError(ERROR_INSUFFICIENT_BUFFER);
				return size;
			}
			SetLastError(ERROR_SUCCESS);
			return n;
		}

		bool isExe(HMODULE module) { return module == nullptr || module == GetModuleHandleW(nullptr); }

		DWORD WINAPI getModuleFileNameA(HMODULE module, LPSTR buffer, DWORD size)
		{
			if (isExe(module))
				return copyPath(g_exePathA, buffer, size);
			if (module == gameCore())
				return copyPath(g_gameCorePathA, buffer, size);
			return GetModuleFileNameA(module, buffer, size);
		}

		DWORD WINAPI getModuleFileNameW(HMODULE module, LPWSTR buffer, DWORD size)
		{
			if (isExe(module))
				return copyPath(g_exePath, buffer, size);
			if (module == gameCore())
				return copyPath(g_gameCorePath, buffer, size);
			return GetModuleFileNameW(module, buffer, size);
		}

		std::string narrow(const std::wstring& s)
		{
			const int n = WideCharToMultiByte(CP_ACP, 0, s.c_str(), int(s.size()), nullptr, 0, nullptr, nullptr);
			std::string out(size_t(n), '\0');
			WideCharToMultiByte(CP_ACP, 0, s.c_str(), int(s.size()), out.data(), n, nullptr, nullptr);
			return out;
		}
	}

	namespace
	{
		UINT WINAPI getACP() { return 1252; }

		// What the executable imports from the game core that it does not export (a delay-loaded name would fail at its first call).
		std::string missingExports(HMODULE core)
		{
			auto* base = reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
			const IMAGE_DATA_DIRECTORY& delayed = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
			std::string missing;
			for (auto* dll = reinterpret_cast<const IMAGE_DELAYLOAD_DESCRIPTOR*>(base + delayed.VirtualAddress); delayed.Size && dll->DllNameRVA; ++dll)
			{
				if (_stricmp(reinterpret_cast<const char*>(base + dll->DllNameRVA), OC4_GAME_CORE_DLL) != 0)
					continue;
				for (auto* name = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + dll->ImportNameTableRVA); name->u1.AddressOfData; ++name)
				{
					const char* fn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + name->u1.AddressOfData)->Name;
					if (!IMAGE_SNAP_BY_ORDINAL(name->u1.Ordinal) && !GetProcAddress(core, fn))
						missing += std::string("\n  ") + fn;
				}
			}
			return missing;
		}
	}

	std::filesystem::path loadGameCore(const std::optional<std::filesystem::path>& modAssets)
	{
		namespace fs = std::filesystem;
		const fs::path name = OC4_GAME_CORE_DLL;
		wchar_t exe[MAX_PATH]{};
		GetModuleFileNameW(nullptr, exe, MAX_PATH);
		fs::path file = fs::path(exe).parent_path() / name;
		if (std::error_code ec; modAssets && fs::is_regular_file(*modAssets / name, ec))
			file = fs::absolute(*modAssets / name, ec);
		g_gameCore = LoadLibraryExW(file.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (!g_gameCore)
			throw std::runtime_error("cannot load the game core " + file.string() + " (error " + std::to_string(GetLastError()) + ")");
		if (const std::string missing = missingExports(g_gameCore); !missing.empty())
			throw std::runtime_error("the game core " + file.string() + " does not export what the engine uses:" + missing);

		static const char* const names[] = { "MessageBoxA", "MessageBoxW", "SetUnhandledExceptionFilter", "GetACP" };
		static const void* const replacements[] = { reinterpret_cast<const void*>(&messageBoxA), reinterpret_cast<const void*>(&messageBoxW),
			reinterpret_cast<const void*>(&setUnhandledExceptionFilter), reinterpret_cast<const void*>(&getACP) };
		patchImports(g_gameCore, names, replacements);
		return file;
	}

	void setGameCoreModulePaths(const std::filesystem::path& exe, const std::filesystem::path& gameCoreDll)
	{
		g_exePath = exe.lexically_normal().make_preferred().wstring();
		g_gameCorePath = gameCoreDll.lexically_normal().make_preferred().wstring();
		g_exePathA = narrow(g_exePath);
		g_gameCorePathA = narrow(g_gameCorePath);
		static const char* const names[] = { "GetModuleFileNameA", "GetModuleFileNameW" };
		static const void* const replacements[] = { reinterpret_cast<const void*>(&getModuleFileNameA),
			reinterpret_cast<const void*>(&getModuleFileNameW) };
		patchImports(gameCore(), names, replacements);
	}
}

extern "C" const PfnDliHook __pfnDliNotifyHook2 = oc4::host::delayLoadHook;
