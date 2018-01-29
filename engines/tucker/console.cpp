/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "tucker/console.h"
#include "tucker/tucker.h"

namespace Tucker {

#define REGISTER_CONSOLE_CMD(cmd, method) \
	registerCmd(cmd, WRAP_METHOD(TuckerConsole, method))

TuckerConsole::TuckerConsole(TuckerEngine *vm) :  _vm(vm) {
	assert(_vm);

	_flags = fDebugNone;

	REGISTER_CONSOLE_CMD("audio", cmdAudio);
	REGISTER_CONSOLE_CMD("box",   cmdBox);
	REGISTER_CONSOLE_CMD("exec",  cmdExec);
	REGISTER_CONSOLE_CMD("room",  cmdRoom);
	REGISTER_CONSOLE_CMD("show",  cmdShow);
	REGISTER_CONSOLE_CMD("flags", cmdFlags);
}

TuckerConsole::~TuckerConsole() {
}

void TuckerConsole::_executeInstructions(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	Common::String buf = Common::String::vformat(fmt, va) + ",end";
	const uint8 *gameInstructions  = _vm->_tableInstructionsPtr;
	const uint8 *debugInstructions = (const uint8 *)buf.c_str();
	_vm->_tableInstructionsPtr = debugInstructions;
	while (_vm->executeTableInstruction() != 2);
	_vm->_tableInstructionsPtr = gameInstructions;
	va_end(va);
}

bool TuckerConsole::cmdExec(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("%s <instruction(s)>\n", argv[0]);
		return true;
	}

	_executeInstructions("%s", argv[1]);
	return false;
}

bool TuckerConsole::cmdRoom(int argc, const char **argv) {
	if (argc < 1 || argc > 2) {
		debugPrintf("%s [room_number]\n", argv[0]);
		debugPrintf("If no parameters are given, prints the current room.\n");
		debugPrintf("Otherwise changes to the specified room number.\n");
		return true;
	}

	if (argc == 1) {
		debugPrintf("Current room: %d\n", _vm->_locationNum);
		return true;
	}

	const int locationNum = atoi(argv[1]);
	if (locationNum == 0 /*TuckerEngine::kRoomNone*/ || locationNum > 98 /*TuckerEngine::kRoomLast*/) {
		debugPrintf("Room '%i' out of valid range [1, %i]\n", locationNum, 98/*TuckerEngine::kRoomLast*/);
		return true;
	}

	_executeInstructions("loc,%02i,fw,00", locationNum);
	return false;
}

bool TuckerConsole::_onOffHandler(int argc, const char **argv, const ConsoleCommand *table, const char *usage, bool invert = false) {
	switch (argc) {
		case 1: // list all
			for (int i = 0; table[i].type; ++i) {
				debugPrintf("%s: %s\n",
					table[i].type,
					(_flags & table[i].flag) == table[i].flag ? (invert ? "off" : "on") : (invert ? "on" : "off")
				);
			}
			return true;
			break;

		case 2: // list single
			for (int i = 0; table[i].type; ++i) {
				if (!strcmp(argv[1], table[i].type)) {
					debugPrintf("%s: %s\n",
						table[i].type,
						(_flags & table[i].flag) == table[i].flag ? (invert ? "off" : "on") : (invert ? "on" : "off")
					);
					return true;
				}
			}
			break;

		case 3: // toggle
			for (int i = 0; table[i].type; ++i) {
				if (!strcmp(argv[1], table[i].type)) {
					bool on;
					if (!strcmp(argv[2], "on")) {
						on = true;
						if (invert)
							_flags &= ~table[i].flag;
						else
							_flags |= table[i].flag;
					}
					else if (!strcmp(argv[2], "off")) {
						on = false;
						if (invert)
							_flags |= table[i].flag;
						else
							_flags &= ~table[i].flag;
					}

					if (table[i].callback)
						(this->*(table[i].callback))(on);

					debugPrintf("%s: %s\n",
						table[i].type,
						(_flags & table[i].flag) == table[i].flag ? (invert ? "off" : "on") : (invert ? "on" : "off")
					);
					return !table[i].closeConsole;
				}
			}
			break;

		default:
			break;
	}

	debugPrintf("Usage: %s %s\n", argv[0], usage);
	return true;
}

bool TuckerConsole::cmdShow(int argc, const char **argv) {
	static const ConsoleCommand showTable[] = {
		{ "bud",        fHideBud,        true, nullptr },
		{ "sprites",    fHideSprites,    true, nullptr },
		{ "animations", fHideAnimations, true, nullptr },
		{ "text",       fHideText,       true, nullptr },
		{ "all",        fHideAll,        true, nullptr },
		{ 0,            0,               0,     0       }
	};

	return _onOffHandler(argc, argv, showTable, "<bud|sprites|animations|text|all> [on|off]", true);
}

bool TuckerConsole::cmdBox(int argc, const char **argv) {
	static const ConsoleCommand boxTable[] = {
		{ "bud",        fBoxBud,        true, nullptr },
		{ "sprites",    fBoxSprites,    true, nullptr },
		{ "animations", fBoxAnimations, true, nullptr },
		{ "text",       fBoxText,       true, nullptr },
		{ "all",        fBoxAll,        true, nullptr },
		{ 0,            0,              0,     0 }
	};

	return _onOffHandler(argc, argv, boxTable, "<bud|sprites|animations|text|all> [on|off]");
}

bool TuckerConsole::cmdAudio(int argc, const char **argv) {
	static const ConsoleCommand audioTable[] = {
		{ "sound",  fMuteSound,  true, &TuckerConsole::_cmdAudioCallback },
		{ "music",  fMuteMusic,  true, &TuckerConsole::_cmdAudioCallback },
		{ "speech", fMuteSpeech, true, &TuckerConsole::_cmdAudioCallback },
		{ "all",    fMuteAll,    true, &TuckerConsole::_cmdAudioCallback },
		{ 0,        0,           0,     0 }
	};

	return _onOffHandler(argc, argv, audioTable, "<sound|music|speech|all> [on|off]", true);
}

void TuckerConsole::_cmdAudioCallback(const bool on)
{
	// Make sure that even music stops when turned off
	_vm->stopSounds();
	_vm->playSounds();
}

bool TuckerConsole::cmdFlags(int argc, const char **argv) {
	int flag;
	switch (argc) {
		case 1:
			for (int i = 0; i < _vm->kFlagsTableSize; ++i) {
				debugPrintf("%i = %i\n", i, _vm->_flagsTable[i]);
			}
			break;

		case 2:
			flag = atoi(argv[1]);
			debugPrintf("%i = %i\n", flag, _vm->_flagsTable[flag]);
			break;

		case 3:
			flag  = atoi(argv[1]);
			int value = atoi(argv[2]);
			debugPrintf("%i => %i\n", flag, value);
			_vm->_flagsTable[flag] = value;
			break;
	}

	return true;
}

} // End of namespace Tucker
