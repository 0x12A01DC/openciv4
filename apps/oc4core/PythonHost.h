#pragma once

// The engine's Python side (CvDLLPythonIFaceBase): embeds Python 2.7, imports
// game scripts from the asset layers (mod > BtS > Warlords > vanilla, loose and
// FPK) by bare module name like the original engine, publishes the DLL's classes
// into CvPythonExtensions and runs the callbacks the DLL asks for.

#include "generated/IFaceStubs.h"

#include "core/AssetResolver.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oc4::host
{
	class PythonHost : public stubs::PythonIFace
	{
	public:
		explicit PythonHost(const AssetResolver& assets);
		~PythonHost();

		// pythonHome: folder with Lib/ (the Python 2.7 standard library).
		void start(const std::filesystem::path& pythonHome);
		// Runs source in __main__; prints and returns false on an exception.
		bool run(const std::string& source);
		size_t scriptCount() const { return m_modules.size(); }

		// Map scripts live next to the Assets folders (PublicMaps); call highest layer first, before start().
		void addMapScriptDir(const std::filesystem::path& dir);
		void setMapScriptModule(std::string module) { m_mapScript = std::move(module); }
		// sys.executable for scripts (BUG finds the game's folder from it); call before start().
		void setExecutable(const std::filesystem::path& exe) { m_executable = exe.string(); }

		// CvPythonExtensions.CyPythonMgr().allowDefaultImpl() from scripts.
		static void allowDefaultImpl() { s_usingDefaultImpl = true; }

		// The interpreter for another thread (TurnWorker): the window's thread lets go of it
		// (its state, to take it back with), the other one enters and leaves it around its work.
		static void* releaseInterpreter();
		static void takeInterpreter(void* state);
		static int enterThread();
		static void leaveThread(int entered);

		// CvDLLPythonIFaceBase
		bool isInitialized() override { return m_started; }
		const char* getMapScriptModule() override { return m_mapScript.c_str(); }
		PyObject* MakeFunctionArgs(void** args, int argc) override;
		bool moduleExists(const char* moduleName, bool bLoadIfNecessary) override;
		bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg) override;
		bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, long* result) override;
		bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, CvString* result) override;
		bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, CvWString* result) override;
		bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, std::vector<byte>* pList) override;
		bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, std::vector<int>* pIntList) override;
		bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, int* pIntList, int* iListSize) override;
		bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, std::vector<float>* pFloatList) override;
		bool callPythonFunction(const char* szModName, const char* szFxnName, int iArg, long* result) override;
		bool pythonUsingDefaultImpl() override { return s_usingDefaultImpl; }

		struct Script
		{
			AssetLocation location;
			bool package = false;
		};
		const Script* findScript(const std::string& moduleName) const;
		std::string readScript(const Script& script) const;

	private:
		// New reference to the call result, or null (exception printed) / missing function.
		PyObject* call(const char* moduleName, const char* fxnName, void* fxnArg, bool& found);
		void indexScripts();
		// Interned Python string for a module or function name (cached, no allocation once seen).
		PyObject* intern(const char* name);

		struct TextHash
		{
			using is_transparent = void;
			size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
		};

		const AssetResolver& m_assets;
		std::unordered_map<std::string, Script> m_modules; // lower-case module name
		std::vector<std::filesystem::path> m_mapScriptDirs;
		std::unordered_map<std::string, PyObject*, TextHash, std::equal_to<>> m_interned;
		std::string m_mapScript;
		std::string m_home, m_executable;
		bool m_started = false;
		static inline bool s_usingDefaultImpl = false;
	};
}
