#include "CrashHandler.h"

#ifndef _WIN32

#include <csignal>
#include <cstdio>
#include <execinfo.h>
#include <initializer_list>
#include <unistd.h>

namespace oc4::host
{
	namespace
	{
		void onCrash(int signal)
		{
			std::fprintf(stderr, "[oc4core] crash: signal %d\n", signal);
			void* frames[64];
			backtrace_symbols_fd(frames, backtrace(frames, 64), STDERR_FILENO);
			std::signal(signal, SIG_DFL);
			std::raise(signal);
		}
	}

	void installCrashHandler()
	{
		for (int signal : { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT })
			std::signal(signal, onCrash);
	}
}

#else

#include <windows.h>

#include <dbghelp.h>

#include <cstdio>
#include <cstring>

namespace oc4::host
{
	namespace
	{
		LONG WINAPI onCrash(EXCEPTION_POINTERS* info)
		{
			const EXCEPTION_RECORD* rec = info->ExceptionRecord;
			std::fprintf(stderr, "\n[oc4core] crash: exception 0x%08lX at %p", rec->ExceptionCode, rec->ExceptionAddress);
			if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
				std::fprintf(stderr, " (%s %p)", rec->ExceptionInformation[0] ? "writing" : "reading", reinterpret_cast<void*>(rec->ExceptionInformation[1]));
			std::fprintf(stderr, "\n");

			HANDLE process = GetCurrentProcess();
			SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
			SymInitialize(process, nullptr, TRUE);

			CONTEXT ctx = *info->ContextRecord;
			STACKFRAME64 frame{};
			frame.AddrPC.Offset = ctx.Rip;
			frame.AddrFrame.Offset = ctx.Rbp;
			frame.AddrStack.Offset = ctx.Rsp;
			frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;

			alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512];
			auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
			for (int i = 0; i < 40; ++i)
			{
				if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &ctx, nullptr, SymFunctionTableAccess64,
						SymGetModuleBase64, nullptr) ||
					frame.AddrPC.Offset == 0)
					break;
				const DWORD64 pc = frame.AddrPC.Offset;
				char module[MAX_PATH] = "?";
				if (const DWORD64 base = SymGetModuleBase64(process, pc))
					GetModuleFileNameA(reinterpret_cast<HMODULE>(base), module, MAX_PATH);
				const char* moduleName = std::strrchr(module, '\\') ? std::strrchr(module, '\\') + 1 : module;

				symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
				symbol->MaxNameLen = 511;
				DWORD64 displacement = 0;
				IMAGEHLP_LINE64 line{ sizeof(IMAGEHLP_LINE64) };
				DWORD lineDisplacement = 0;
				if (SymFromAddr(process, pc, &displacement, symbol))
				{
					if (SymGetLineFromAddr64(process, pc, &lineDisplacement, &line))
						std::fprintf(stderr, "  #%-2d %s!%s  %s:%lu\n", i, moduleName, symbol->Name, line.FileName, line.LineNumber);
					else
						std::fprintf(stderr, "  #%-2d %s!%s+0x%llx\n", i, moduleName, symbol->Name, displacement);
				}
				else
					std::fprintf(stderr, "  #%-2d %s+%p\n", i, moduleName, reinterpret_cast<void*>(pc));
			}
			std::fflush(stderr);
			return EXCEPTION_EXECUTE_HANDLER;
		}
	}

	void installCrashHandler() { SetUnhandledExceptionFilter(onCrash); }
}

#endif
