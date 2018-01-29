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

#ifndef TUCKER_CONSOLE_H
#define TUCKER_CONSOLE_H

#include "gui/debugger.h"

namespace Tucker {

class TuckerEngine;

enum DebugFlags {
	fDebugNone      = 0,

	fHideBud        = 1 <<  0,
	fHideSprites    = 1 <<  1,
	fHideAnimations = 1 <<  2,
	fHideText       = 1 <<  3,
	fHideAll        = fHideBud | fHideSprites | fHideAnimations | fHideText,

	fBoxBud         = 1 <<  4,
	fBoxSprites     = 1 <<  5,
	fBoxAnimations  = 1 <<  6,
	fBoxText        = 1 <<  7,
	fBoxAll         = fBoxBud | fBoxSprites | fBoxAnimations | fBoxText,

	fMuteSound      = 1 <<  8,
	fMuteMusic      = 1 <<  9,
	fMuteSpeech     = 1 << 10,
	fMuteAll        = fMuteSound | fMuteMusic | fMuteSpeech,

	fFooBar         = 1 << 11,
	fFooQuux        = 1 << 12
};

class TuckerConsole : public GUI::Debugger {
friend class TuckerEngine;

typedef struct {
	const char *type;
	const int  flag;
	const bool closeConsole;
	void (TuckerConsole::*callback)(bool on);
} ConsoleCommand;

public:
	TuckerConsole(TuckerEngine *vm);
	virtual ~TuckerConsole(void);

	bool cmdAudio(int argc, const char **argv);
	bool cmdBox(int argc, const char **argv);
	bool cmdExec(int argc, const char **argv);
	bool cmdRoom(int argc, const char **argv);
	bool cmdShow(int argc, const char **argv);
	bool cmdFlags(int argc, const char **argv);

protected:
	int _flags;

private:
	TuckerEngine *_vm;

	void _cmdAudioCallback(bool on);
	bool _onOffHandler(int argc, const char **argv, const ConsoleCommand *table, const char *usage, bool invert);
	void _executeInstructions(const char *fmt, ...);
};

} // End of namespace Tucker

#endif
