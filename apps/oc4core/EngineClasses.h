#pragma once

// Classes of CvPythonExtensions that the original engine (not the DLL) implements.
// CyFractal wraps the DLL's CvFractal and is registered here with pybind11, sharing
// the type registry with the DLL's bindings (so it accepts the DLL's CyRandom). The
// rest are Python, see python/oc4engine.py.

namespace oc4::host
{
	class TextManager;
	class Utility;

	// Call after DLLPublishToPython.
	void registerEngineClasses(Utility& utility);
}
