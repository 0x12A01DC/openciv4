## openciv4: the engine-side classes of CvPythonExtensions.
##
## In the original game Civ4BeyondSword.exe adds these to the module the DLL publishes
## into. CyFractal wraps a DLL object and is registered from C++ (EngineClasses.cpp);
## everything here is plain Python. Classes the headless host has no use for yet are
## stubs: each method returns 0, and outside graphics/UI also logs its first call (with
## OC4_STUB_LOG set, the graphics and UI ones too: what the scripts still miss).

import re
import sys
import oc4host
import CvPythonExtensions as _cpe


# Like the original engine: print goes to PythonDbg.log, errors also to PythonErr.log.
class _LogWriter(object):
	def __init__(self, logName, echo=None):
		self.logName = logName
		self.echo = echo

	def write(self, text):
		oc4host.log(self.logName, text)
		if self.echo:
			self.echo.write(text)

	def flush(self):
		pass

sys.stdout = _LogWriter('PythonDbg.log')
sys.stderr = _LogWriter('PythonErr.log', sys.__stderr__)


# The original event scripts sort (value, city) tuples. On a tie Python 2 compares the objects
# by memory address, so a game's outcome would depend on where they happen to live (Influenza
# picks the cities it hits this way). Ties go by the game's own ids instead.
def _orderBy(cls, key):
	# (scripts also test `None == city`: anything else keeps Python's own comparison)
	def compare(a, b):
		if isinstance(a, cls) and isinstance(b, cls):
			return cmp(key(a), key(b))
		return cmp(id(a), id(b))
	cls.__cmp__ = compare

_orderBy(_cpe.CyCity, lambda c: (c.getOwner(), c.getID()))
_orderBy(_cpe.CyPlot, lambda p: (p.getX(), p.getY()))
_orderBy(_cpe.CyPlayer, lambda p: p.getID())


class _Stub(object):
	_silent = False
	_logAll = __import__('os').environ.get('OC4_STUB_LOG') is not None

	def __init__(self, *args, **kwargs):
		pass

	def __getattr__(self, name):
		if name.startswith('__'):
			raise AttributeError(name)
		what = type(self).__name__ + '.' + name
		silent = self._silent and not self._logAll
		def method(*args, **kwargs):
			if not silent:
				oc4host.unimplemented(what)
			return 0
		return method


# Globals the scripts use unqualified (from CvPythonExtensions import *).
true = True
false = False

def NiTextOut(text):
	pass


class CyPythonMgr(object):
	def allowDefaultImpl(self):
		oc4host.allow_default_impl()

	# CvAppInterface.init() points sys.stdout/stderr at these (CvUtil.RedirectDebug/Error).
	def debugMsg(self, msg):
		oc4host.log('PythonDbg.log', msg)

	def errorMsg(self, msg):
		oc4host.log('PythonErr.log', msg)
		sys.__stderr__.write(msg.encode('utf-8', 'replace') if isinstance(msg, unicode) else msg)

	debugMsgWide = debugMsg
	errorMsgWide = errorMsg


class CyTranslator(object):
	_tags = re.compile(r'<[^>]*>')

	def getText(self, key, args=()):
		return oc4host.get_text(key, args)

	def getObjectText(self, key, form):
		return oc4host.get_object_text(key, form)

	def getColorText(self, key, args, color):
		return self.changeTextColor(self.getText(key, args), color)

	def changeTextColor(self, text, color):
		# The DLL's SETCOLR/ENDCOLR markup with the colour info's RGBA in 0..255.
		c = _cpe.CyGlobalContext().getColorInfo(color).getColor()
		return u"<color=%d,%d,%d,%d>%s</color>" % (int(c.r * 255), int(c.g * 255), int(c.b * 255), int(c.a * 255), text)

	def stripHTML(self, text):
		return self._tags.sub(u'', text)


class FracVals(object):
	FRAC_WRAP_X = 0x0001
	FRAC_WRAP_Y = 0x0002
	FRAC_PERCENT = 0x0004
	FRAC_POLAR = 0x0008
	FRAC_CENTER_RIFT = 0x0010
	FRAC_INVERT_HEIGHTS = 0x0020
	DEFAULT_FRAC_X_EXP = 7
	DEFAULT_FRAC_Y_EXP = 6

_cpe.CyFractal.FracVals = FracVals


# Input enums: the host raises no input events yet, the values only need to differ.
class NotifyCode(object):
	# in the order the original engine registers them (values assumed to count up from 0)
	NOTIFY_CLICKED, NOTIFY_DBL_CLICKED, NOTIFY_FOCUS, NOTIFY_UNFOCUS, NOTIFY_CURSOR_MOVE_ON, NOTIFY_CURSOR_MOVE_OFF, \
	NOTIFY_CHARACTER, NOTIFY_SCROLL_UP, NOTIFY_SCROLL_DOWN, NOTIFY_NEW_HORIZONTAL_STOP, NOTIFY_NEW_VERTICAL_STOP, \
	NOTIFY_LISTBOX_ITEM_SELECTED, NOTIFY_FLYOUT_ITEM_SELECTED, NOTIFY_MOUSEMOVE, NOTIFY_MOUSEWHEELUP, NOTIFY_MOUSEWHEELDOWN, \
	NOTIFY_LINKEXECUTE, NOTIFY_MOVIE_DONE, NOTIFY_SLIDER_NEWSTOP, NOTIFY_TABLE_HEADER_SELECTED = range(20)


class MouseFlags(object):
	pass

# The names the original engine registers, in its order. Their values are not known (the
# Steam executable's code is encrypted); distinct bits do, since no mouse input reaches a
# headless engine (scripts use them as dictionary keys).
for _i, _name in enumerate(['STATE', 'LBUTTON', 'RBUTTON', 'SHIFT', 'CONTROL', 'MBUTTON', 'XBUTTON1', 'XBUTTON2', 'EVENT',
		'MOUSEMOVE', 'LBUTTONDOWN', 'LBUTTONUP', 'LBUTTONDBLCLK', 'RBUTTONDOWN', 'RBUTTONUP', 'RBUTTONDBLCLK', 'MBUTTONDOWN',
		'MBUTTONUP', 'MBUTTONDBLCLK', 'MOUSEWHEELUP', 'MOUSEWHEELDOWN', 'XBUTTONDOWN', 'XBUTTONUP', 'XBUTTONDBLCLK', 'CLICKED',
		'DBLCLICKED', 'RELEASED']):
	setattr(MouseFlags, 'MOUSE_' + _name, 1 << _i)


class InputTypes(object):
	pass

# The keys the original engine registers, in its order (DirectInput's, digits and letters
# first); values assumed to count up.
for _i, _name in enumerate(['ESCAPE'] + [str(d) for d in range(10)] + ['MINUS'] + [chr(c) for c in range(ord('A'), ord('Z') + 1)] + (
		'EQUALS BACKSPACE TAB LBRACKET RBRACKET RETURN LCONTROL SEMICOLON APOSTROPHE GRAVE LSHIFT BACKSLASH COMMA PERIOD SLASH '
		'RSHIFT NUMPADSTAR LALT SPACE CAPSLOCK F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 NUMLOCK SCROLL NUMPAD7 NUMPAD8 NUMPAD9 NUMPADMINUS '
		'NUMPAD4 NUMPAD5 NUMPAD6 NUMPADPLUS NUMPAD1 NUMPAD2 NUMPAD3 NUMPAD0 NUMPADPERIOD F11 F12 NUMPADEQUALS AT UNDERLINE COLON '
		'NUMPADENTER RCONTROL VOLUMEDOWN VOLUMEUP NUMPADCOMMA NUMPADSLASH SYSRQ RALT PAUSE HOME UP PGUP LEFT RIGHT END DOWN PGDN '
		'INSERT DELETE').split()):
	setattr(InputTypes, 'KB_' + _name, _i + 1)


# Engine classes the host does not implement yet. Graphics and UI have nothing to do
# headless; the others log their first use.
for _name in ['CyAudioGame', 'CyCamera', 'CyEngine', 'CyGFlyoutMenu', 'CyGInterfaceScreen', 'CyGTabCtrl', 'CyGlobeLayerManager',
		'CyInterface', 'CyInterfaceScreen', 'CyPopup', 'CyPopupInfo', 'CyUnitEntity']:
	globals()[_name] = type(_name, (_Stub,), {'_silent': True})
for _name in ['CyDiplomacy', 'CyMessageControl', 'CyPitboss', 'CyStatistics', 'CyUserProfile', 'CyVariableSystem', 'CyGet']:
	globals()[_name] = type(_name, (_Stub,), {})

# No tech splash popups without a screen.
CyInterface.noTechSplash = lambda self: True


for _name in ['true', 'false', 'NiTextOut', 'CyPythonMgr', 'CyTranslator', 'NotifyCode', 'MouseFlags', 'InputTypes'] +[n for n, v in globals().items() if isinstance(v, type) and issubclass(v, _Stub) and v is not _Stub]:
	if not hasattr(_cpe, _name):
		setattr(_cpe, _name, globals()[_name])


# CyMap.rebuild as the WorldBuilder's scripts call it (CvWBDesc.applyMap): the wraps as 0 or 1,
# no custom map options as None; the binding takes bools and an int there.
if hasattr(_cpe, 'CyMap') and hasattr(_cpe.CyMap, 'rebuild'):
	_cyMapRebuild = _cpe.CyMap.rebuild
	def _rebuild(self, iGridW, iGridH, iTop, iBottom, bWrapX, bWrapY, eWorldSize, eClimate, eSeaLevel, iNumCustom=0, aeCustom=None):
		return _cyMapRebuild(self, int(iGridW), int(iGridH), int(iTop), int(iBottom), bool(bWrapX), bool(bWrapY), int(eWorldSize), int(eClimate),
			int(eSeaLevel), int(iNumCustom or 0) if aeCustom is not None else 0, 0)
	_cpe.CyMap.rebuild = _rebuild
