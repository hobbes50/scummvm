/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SWORD1_SWORD1_H
#define SWORD1_SWORD1_H

#include "engines/engine.h"
#include "common/error.h"
#include "common/keyboard.h"
#include "common/rect.h"
#include "common/util.h"
#include "sword1/sworddefs.h"
#include "sword1/console.h"
#include "sword1/detection.h"

/**
 * This is the namespace of the Sword1 engine.
 *
 * Status of this engine: ???
 *
 * Games using this engine:
 * - Broken Sword: The Shadow of the Templars
 */
namespace Sword1 {

enum ControlPanelMode {
	CP_NORMAL = 0,
	CP_DEATHSCREEN,
	CP_THEEND,
	CP_NEWGAME
};

class Screen;
class Sound;
class Logic;
class Mouse;
class ResMan;
class ObjectMan;
class Menu;
class Music;
class Control;

struct SystemVars {
	bool    runningFromCd;
	uint32  currentCD;          // starts at zero, then either 1 or 2 depending on section being played
	uint32  justRestoredGame;   // see main() in sword.c & New_screen() in gtm_core.c

	uint8   controlPanelMode;   // 1 death screen version of the control panel, 2 = successful end of game, 3 = force restart
	bool    forceRestart;
	bool    wantFade;           // when true => fade during scene change, else cut.
	bool   playSpeech;
	bool   showText;
	uint8   language;
	bool    isDemo;
	bool    isSpanishDemo;
	Common::Platform platform;
	Common::Language realLanguage;
	bool isLangRtl;
};

class SwordEngine : public Engine {
	friend class SwordConsole;
public:
	SwordEngine(OSystem *syst, const SwordGameDescription *gameDesc);
	~SwordEngine() override;
	static SystemVars _systemVars;
	void reinitialize();

	uint32 _features;

	bool mouseIsActive();

	static bool isMac() { return _systemVars.platform == Common::kPlatformMacintosh; }
	static bool isPsx() { return _systemVars.platform == Common::kPlatformPSX; }
	static bool isWindows() { return _systemVars.platform == Common::kPlatformWindows ; }

protected:
	// Engine APIs
	Common::Error init();
	Common::Error go();
	Common::Error run() override {
		Common::Error err;
		err = init();
		if (err.getCode() != Common::kNoError)
			return err;
		return go();
	}
	bool hasFeature(EngineFeature f) const override;
	void syncSoundSettings() override;

	Common::Error loadGameState(int slot) override;
	bool canLoadGameStateCurrently() override;
	Common::Error saveGameState(int slot, const Common::String &desc, bool isAutosave = false) override;
	bool canSaveGameStateCurrently() override;
	Common::String getSaveStateName(int slot) const override {
		return Common::String::format("sword1.%03d", slot);
	}
private:
	void delay(int32 amount);

	void checkCdFiles();
	void checkCd();
	void showFileErrorMsg(uint8 type, bool *fileExists);
	void flagsToBool(bool *dest, uint8 flags);

	void reinitRes(); //Reinits the resources after a GMM load

	uint8 mainLoop();

	Common::Point _mouseCoord;
	uint16 _mouseState;
	Common::KeyState _keyPressed;

	ResMan      *_resMan;
	ObjectMan   *_objectMan;
	Screen      *_screen;
	Mouse       *_mouse;
	Logic       *_logic;
	Sound       *_sound;
	Menu        *_menu;
	Music       *_music;
	Control     *_control;
	static const uint8  _cdList[TOTAL_SECTIONS];
	static const CdFile _pcCdFileList[];
	static const CdFile _macCdFileList[];
	static const CdFile _psxCdFileList[];
};

} // End of namespace Sword1

#endif // SWORD1_SWORD1_H
