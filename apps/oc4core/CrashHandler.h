#pragma once

namespace oc4::host
{
	// Prints the exception and a symbolised stack trace to stderr on a crash.
	void installCrashHandler();
}
