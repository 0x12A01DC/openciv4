## openciv4: the turn log (Ctrl+Tab, the button by the minimap).
##
## The original engine draws it itself: the active player's messages of the last turns (the
## game core keeps them, CvPlayer::getGameMessages, and drops the old ones), each with its
## date, the newest last. Here it is a screen on oc4screens' widgets; a click on a message
## that has a plot looks there.

import CvPythonExtensions as _cpe
import oc4host
import oc4screens

TURN_LOG = 31001 # its python file id (none of the game's screens has it)

_state = {'shown': False, 'messages': []}


def _screen():
	return oc4screens.CyGInterfaceScreen('OpenCiv4TurnLog', TURN_LOG)


def isShown():
	return _state['shown']


def toggle():
	if _state['shown']:
		hide()
	else:
		show()


def hide():
	if _state['shown']:
		_state['shown'] = False
		oc4screens.reset(TURN_LOG)
		oc4screens.hide(TURN_LOG)


def show():
	_state['shown'] = True
	refresh()


def refresh():
	"""(again, when the game core says the log changed)"""
	if not _state['shown']:
		return
	screen = _screen()
	oc4screens.reset(TURN_LOG)
	W, H = screen.getXResolution(), screen.getYResolution()
	screen.setDimensions(0, 0, W, H)
	screen.showScreen(_cpe.PopupStates.POPUPSTATE_IMMEDIATE, False)
	screen.setCloseOnEscape(True)
	x, y, w = 8, 64, min(640, W / 2)
	h = max(200, H - y - 250)
	screen.addPanel('TurnLogPanel', u'', u'', True, False, x, y, w, h, _cpe.PanelStyles.PANEL_STYLE_MAIN)
	title = _cpe.CyTranslator().getText('TXT_KEY_EVENT_LOG', ())
	screen.setLabel('TurnLogTitle', 'Background', u'<font=3b>' + title + u'</font>', 1 << 2, x + w / 2, y + 10, 0,
		_cpe.FontTypes.TITLE_FONT, _cpe.WidgetTypes.WIDGET_GENERAL, -1, -1)
	screen.setText('TurnLogClose', 'Background', u'<font=2>X</font>', 1 << 1, x + w - 14, y + 8, 0, _cpe.FontTypes.TITLE_FONT,
		_cpe.WidgetTypes.WIDGET_GENERAL, -1, -1)
	screen.addListBoxGFC('TurnLogList', u'', x + 12, y + 40, w - 24, h - 52, _cpe.TableStyles.TABLE_STYLE_STANDARD)
	game = _cpe.CyGame()
	texts = _cpe.CyGameTextMgr()
	messages = oc4host.turnlog_messages()
	_state['messages'] = messages
	for i, (turn, text, px, py, kind) in enumerate(messages):
		date = texts.getDateStr(turn, False, game.getCalendar(), game.getStartYear(), game.getGameSpeedType())
		screen.appendListBoxString('TurnLogList', u'<color=180,180,180,255>%s:</color> %s' % (date, text), _cpe.WidgetTypes.WIDGET_GENERAL, i, -1,
			1 << 0)
	# (the newest, at the bottom, in view)
	w = screen._s.get('TurnLogList')
	if w is not None:
		w.offset = 1 << 30


def handleInput(args):
	notify, data, function = args[0], args[1], args[5]
	if function == 'TurnLogClose' and notify == 0:
		hide()
		return 1
	if function == 'TurnLogList' and notify == 11:
		messages = _state['messages']
		if 0 <= data < len(messages) and messages[data][2] >= 0:
			oc4host.turnlog_look(messages[data][2], messages[data][3])
		return 1
	return 0
