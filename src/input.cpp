#pragma once

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <SDL.h>
#include <patch.h>
#include <config.h>
#include "keycodes.h"
#include "flickit.h"
#include "flickit_game.h"
#include "flickit_pose.h"
#include <QB/LazyStruct.h>
#include <QB/Qb.h>
#include <Util/sb.h>

/******************************************************************/
/*                                                                */
/*  Definitions													  */
/******************************************************************/

#define MAX_PLAYERS 2

enum EEditorState
{
	vINACTIVE = 0xd443a2bc,		// "off"
	vEDITING = 0x7eca21e5,		// "edit"
	vTEST_PLAY = 0xa40f0b8a,	// "test_play"
	vREGULAR_PLAY = 0xcd3682c1,	// "regular_play"

	// use this as parameter only
	vKEEP_SAME_STATE = 0,
};

struct EdCParkEditorInstance
{
	char unk[100];
	float m_camAngle;
	float m_camAngleVert;
	float m_camDist;
	char unk2[8];
	uint32_t m_cursor_state;
	uint32_t v_start_cb;
	uint32_t v_stop_cb;
	uint32_t mp_park_manager;
	uint32_t m_cursor_pos;
	uint32_t mp_cursor;
	uint32_t mp_menu_manager;
	uint32_t m_last_time;
	EEditorState m_state;
	bool m_paused;
	char unk3[7];
	bool ScriptSetParkEditorTimeOfDay;

};

struct CGoalManagerInstance
{
	char unk[76];
	uint32_t GetLevelStructureName;
};

struct SkateInstance
{
	/* singleton of Skate::Instance() */
	char unk[920];
	uint32_t m_cur_level;
	uint32_t m_prev_level;
	bool m_new_record;
	char unk2[7];
	CGoalManagerInstance GetGoalManager;
};

typedef struct {
	uint32_t vtablePtr;
	uint32_t node;
	uint32_t type;
	uint32_t port;
	uint32_t slot;
	uint32_t isValid;
	uint32_t unk_24;
	uint8_t controlData[32];	// PS2 format control data
	uint8_t vibrationData_align[32];
	uint8_t vibrationData_direct[32];
	uint8_t vibrationData_max[32];
	uint8_t vibrationData_oldDirect[32];
	uint32_t unk5;
	uint32_t actuatorsDisabled;
	uint32_t capabilities;
	uint32_t unk6;
	uint32_t num_actuators;
	uint32_t unk7;
	uint32_t state;
	uint32_t test;
	uint32_t index;
	uint32_t isPluggedIn;
	uint32_t unplugged_counter;
	uint32_t unplugged_retry;
	uint32_t pressed;
	uint32_t start_or_a_pressed;

} device;

struct playerslot {
	SDL_GameController* controller;
	uint8_t lockedOut;
	//uint64_t signin;	// time until controller can be used after sign-in
	//SDL_GameControllerButton lockedButton;	// button that player used to sign-in, to be ignored until release
};

// Separate menu controls
uint8_t* isMenu = (uint8_t*)0x0076A7A7;

// For pause buffer
uint8_t* otherIsMenu = (uint8_t*)0x0076A7A4;
uint8_t isInMenu = 0;

// Check for key_on_screen flag: If a text entry prompt is on-screen, keybinds don't interfere with text entry confirmation/cancellation
uint8_t* keyboard_on_screen = (uint8_t*)0x0076A7A6;

// Create-A-Goal should technically be detected as a menu. By default it uses default keybindings
uint8_t* isCAG = (uint8_t*)0x0076A7A4; //0x006A0350;

SDL_GameController** controllerList;
uint8_t isUsingKeyboard = 1;
EdCParkEditorInstance* ParkEd;
CGoalManagerInstance* GoalMan;
SkateInstance* Skate;
int buffer = 0;
bool bSpace_released = true;
bool bReturn_released = true;
bool bBackspace_released = true;
bool bEscape_released = true;
bool instances_initialized = false;
char* executableDirectory3[MAX_PATH];
uint8_t numplayers = 0;
int controllerCount;
int controllerListSize;
struct modsettings inputsettings;
struct keybinds keybinds;
struct controllerbinds padbinds;
struct playerslot players[MAX_PLAYERS] = { { NULL, 0 }, { NULL, 0 } };
int* shouldQuit = (int*)0x007CEC8C;

typedef bool __cdecl ScriptObjectExists_NativeCall(Script::LazyStruct* params);
ScriptObjectExists_NativeCall* ScriptObjectExists_Native = (ScriptObjectExists_NativeCall*)(0x0041FD90);

typedef CGoalManagerInstance* __cdecl GameGetGoalManager_NativeCall();
GameGetGoalManager_NativeCall* GameGetGoalManager_Native = (GameGetGoalManager_NativeCall*)(0x004EC8D0);

typedef void (key_input)(int32_t key, uint32_t param);
key_input* m_keyinput = (key_input*)0x00581BF0;


/******************************************************************/
/*                                                                */
/*  SDL setup													  */
/******************************************************************/

void addController(int idx) {
	SDL_GameController* controller = SDL_GameControllerOpen(idx);

	SDL_GameControllerSetPlayerIndex(controller, -1);

	if (controller) {
		if (controllerCount == controllerListSize) {
			int tmpSize = controllerListSize + 1;
			SDL_GameController** tmp = (SDL_GameController**)realloc(controllerList, sizeof(SDL_GameController*) * tmpSize);
			if (!tmp) {
				return;
			}

			controllerListSize = tmpSize;
			controllerList = tmp;
		}
		controllerList[controllerCount] = controller;
		controllerCount++;
	}
}

void addplayer(SDL_GameController* controller) {
	if (numplayers < 2) {
		// Find open slot
		uint8_t found = 0;
		int i = 0;
		for (; i < MAX_PLAYERS; i++) {
			if (!players[i].controller) {
				found = 1;
				break;
			}
		}
		if (found) {
			SDL_GameControllerSetPlayerIndex(controller, i);
			players[i].controller = controller;

			if (numplayers > 0) {
				players[i].lockedOut = 1;
				SDL_JoystickRumble(SDL_GameControllerGetJoystick(controller), 0xffff, 0xffff, 250);
			}
			numplayers++;
			Log::TypedLog(CHN_SDL, "Added player %d: %s\n", i + 1, SDL_GameControllerName(controller));	
		}
	}
	else {
		Log::TypedLog(CHN_SDL, "Already two players, not adding\n");
	}
}

void pruneplayers() {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (players[i].controller && !SDL_GameControllerGetAttached(players[i].controller)) {
			Log::TypedLog(CHN_SDL, "Pruned player %d\n", i + 1);

			players[i].controller = NULL;
			numplayers--;
			Log::TypedLog(CHN_SDL, "Remaining players: %d\n", numplayers);
		}
	}
}

void removeController(SDL_GameController* controller) {
	Log::TypedLog(CHN_SDL, "Controller \"%s\" disconnected\n", SDL_GameControllerName(controller));

	int i = 0;

	while (i < controllerCount && controllerList[i] != controller) {
		i++;
	}

	if (controllerList[i] == controller) {
		SDL_GameControllerClose(controller);

		int playerIdx = SDL_GameControllerGetPlayerIndex(controller);
		if (playerIdx != -1) {
			Log::TypedLog(CHN_SDL, "Removed player %d\n", playerIdx + 1);
			players[playerIdx].controller = NULL;
			numplayers--;
		}

		pruneplayers();

		for (; i < controllerCount - 1; i++) {
			controllerList[i] = controllerList[i + 1];
		}
		controllerCount--;
	}
	else {
		Log::TypedLog(CHN_SDL, "Did not find disconnected controller in list\n");
	}
}

void initSDLControllers() {
	Log::TypedLog(CHN_SDL, "Initializing Controller Input\n");

	controllerCount = 0;
	controllerListSize = 1;
	controllerList = (SDL_GameController**)malloc(sizeof(SDL_GameController*) * controllerListSize);
	int detected = 0;

	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (SDL_IsGameController(i)) {
			addController(i);
			//if (!(detected)) Log::TypedLog(CHN_SDL, "Detected controller \"%s\"\n", SDL_GameControllerNameForIndex(i));
			//detected = 1;
		}
	}
}

uint8_t axisAbs(uint8_t val) {
	if (val > 127) {
		// Positive, just remove top bit
		return val & 0x7F;
	}
	else {
		// Negative
		return ~val & 0x7F;
	}
}

uint8_t getButton(SDL_GameController* controller, controllerButton button) {
	if (button == CONTROLLER_BUTTON_LEFTTRIGGER) {
		uint8_t pressure = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7;
		return pressure > 0x80;
	}
	else if (button == CONTROLLER_BUTTON_RIGHTTRIGGER) {
		uint8_t pressure = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7;
		return pressure > 0x80;
	}
	else {
		return SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)button);
	}
}

/******************************************************************/
/*                                                                */
/*  Flickit: Skate-style right stick controls (see flickit.h)     */
/******************************************************************/

#define FLICKIT_SECTION "Flickit"

Flickit::Settings flickitSettings;
Flickit::Recognizer flickitRecognizers[MAX_PLAYERS];
bool flickitOn[MAX_PLAYERS] = { true, true };            // switched in a level with the toggle button
bool flickitToggleDown[MAX_PLAYERS] = { false, false };
Flickit::PatternSet flickitPatterns;                     // stick patterns from the pattern file, if there is one

// What flickitPoll answers
enum { FLICKIT_OFF = 0, FLICKIT_ON = 1, FLICKIT_PAUSED = 2 };

// The game's side of the grinds (flickit_game.cpp). Only the first player's skater is looked at.
bool flickitSeesSkater = false;                          // on this frame the skater's state was read
bool flickitPaused = false;                              // the skater is off the board and Flickit stands down
uint32_t flickitGrindSerial = 0;                         // for the log: the last grind the game reported
int flickitGrindKind = Flickit::GRIND_UNKNOWN;
bool flickitGrindReady = false;
uint32_t flickitRailSince = 0;
bool flickitOnRail = false;
bool flickitNoReportLogged = false;
int flickitDragClass = -1;                               // for the log: what the drag on the rail was last reported for
int flickitDragSurface = -1;
bool flickitRailTimed = false;                           // for the log: the stay on the rail as the game's rail code saw it
float flickitRailSeconds = 0.0f;
float flickitRailSpeedIn = 0.0f;
float flickitRailSpeed = 0.0f;
uint32_t flickitCatchSerial = 0;                         // for the log: the last rail that caught the board by itself, and the last that did not
uint32_t flickitMissSerial = 0;
bool flickitRolling = false;                             // the board rolls on the ground: a push button pushes
bool flickitOllieWas = false;                            // the Ollie button on the frame before
bool flickitOlliePush = false;                           // the press of the Ollie button that is down is a push: it began while the board rolled
bool flickitFlipPush = false;                            // the Flip button is down and has been a push since the board last rolled under it
bool flickitPushHeld = false;                            // a push button is down
uint32_t flickitPushSerial = 0;                          // for the log: the last push
uint32_t flickitFootSerial = 0;                          // for the log: the last word on the feet in a push animation
uint32_t flickitBlindSerial = 0;                         // for the log: the last time the pushes went over to the clock
uint32_t flickitCutSerial = 0;                           // for the log: the last animation of the game's a push cut short
uint32_t flickitWaySerial = 0;                           // for the log: the last push animation that gave way to something before it
uint32_t flickitPoseSerial = 0;                          // for the log: the last crouch that showed the stick's depth
uint32_t flickitPoseLostSerial = 0;                      // for the log: the last time the game dropped the pose kept for a crouch
uint32_t flickitPoseMissSerial = 0;                      // for the log: the last crouch that found no pose to stand in
uint32_t flickitPoseUnderSerial = 0;                     // for the log: the last standing pose started under a crouch
uint32_t flickitStanceSerial = 0;                        // for the log: the last change of stance the stick asked for
bool flickitMirrored = false;                            // for the log: the flicks were read mirrored on the last frame
FlickitGame::Skater flickitFlipSkater;                   // the skater as last read, for the log of a held flip trick
uint32_t flickitFlipToldLevel = 0;                       // the last held flip trick the log said had no air for another rotation
uint32_t flickitSpinSerial = 0;                          // for the log: the last air Flickit turned the skater in
uint32_t flickitFrameAt = 0;                             // SDL's clock at the last frame handed to the game hooks
bool flickitFrameSeen = false;

// Reads the stick pattern file named in the ini (default: skater.pat) from the folder the ini is in.
// Without the file, or with Patterns=0, Flickit decides tricks from its built-in directions.
void loadFlickitPatterns(const char* configFile, int enabled) {
	flickitPatterns.Clear();
	if (!enabled) {
		return;
	}

	char name[MAX_PATH];
	GetPrivateProfileString(FLICKIT_SECTION, "PatternFile", "skater.pat", name, sizeof(name), configFile);

	int dirLength = 0;
	for (int i = 0; configFile[i] != '\0'; i++) {
		if (configFile[i] == '\\' || configFile[i] == '/') {
			dirLength = i + 1;
		}
	}
	char path[MAX_PATH * 2];
	sprintf_s(path, sizeof(path), "%.*s%s", dirLength, configFile, name);

	FILE* f = nullptr;
	if (fopen_s(&f, path, "rb") != 0 || !f) {
		Log::TypedLog(CHN_SDL, "Flickit: no pattern file at %s, using the built-in directions\n", path);
		return;
	}
	static char text[256 * 1024];
	const size_t length = fread(text, 1, sizeof(text), f);
	fclose(f);

	const int count = flickitPatterns.Parse(text, length);
	int tricks = 0;
	for (int i = 0; i < count; i++) {
		if (flickitPatterns.At(i).known) {
			tricks++;
		}
	}
	Log::TypedLog(CHN_SDL, "Flickit: %d stick patterns read from %s, %d of them name a trick\n", count, path, tricks);
}

void loadFlickitSettings() {
	char configFile[MAX_PATH];
	getConfigFilePath(configFile);

	Flickit::Settings s;
	Flickit::SetDefaults(s);

	s.enabled = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Enabled", s.enabled, configFile);
	s.toggleButton = (int)GetPrivateProfileInt(FLICKIT_SECTION, "ToggleButton", s.toggleButton, configFile);
	s.reloadKey = (int)GetPrivateProfileInt(FLICKIT_SECTION, "ReloadKey", s.reloadKey, configFile);
	s.debug = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Debug", s.debug, configFile);

	s.kickflipSide = (int)GetPrivateProfileInt(FLICKIT_SECTION, "KickflipSide", s.kickflipSide, configFile);
	s.switchMirror = (int)GetPrivateProfileInt(FLICKIT_SECTION, "SwitchMirror", s.switchMirror, configFile);
	s.nollie = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Nollie", s.nollie, configFile);
	s.nollieTaps = (int)GetPrivateProfileInt(FLICKIT_SECTION, "NollieTaps", s.nollieTaps, configFile);
	s.triggerGrabs = (int)GetPrivateProfileInt(FLICKIT_SECTION, "TriggerGrabs", s.triggerGrabs, configFile);
	s.autoGrind = (int)GetPrivateProfileInt(FLICKIT_SECTION, "AutoGrind", s.autoGrind, configFile);

	s.deadzone = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Deadzone", s.deadzone, configFile);
	s.engage = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Engage", s.engage, configFile);
	s.release = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Release", s.release, configFile);
	s.flick = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Flick", s.flick, configFile);
	s.snap = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Snap", s.snap, configFile);

	s.flickWindow = (int)GetPrivateProfileInt(FLICKIT_SECTION, "FlickWindow", s.flickWindow, configFile);
	s.settle = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Settle", s.settle, configFile);
	s.dwell = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Dwell", s.dwell, configFile);
	s.afterPop = (int)GetPrivateProfileInt(FLICKIT_SECTION, "AfterPop", s.afterPop, configFile);
	s.dirLead = (int)GetPrivateProfileInt(FLICKIT_SECTION, "DirLead", s.dirLead, configFile);
	s.pulse = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Pulse", s.pulse, configFile);
	s.autoGrindDelay = (int)GetPrivateProfileInt(FLICKIT_SECTION, "AutoGrindDelay", s.autoGrindDelay, configFile);
	s.autoGrindHold = (int)GetPrivateProfileInt(FLICKIT_SECTION, "AutoGrindHold", s.autoGrindHold, configFile);

	s.patterns = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Patterns", s.patterns, configFile);
	s.missTicks = (int)GetPrivateProfileInt(FLICKIT_SECTION, "MissTicks", s.missTicks, configFile);

	s.gameHooks = (int)GetPrivateProfileInt(FLICKIT_SECTION, "GameHooks", s.gameHooks, configFile);
	s.grindCatch = (int)GetPrivateProfileInt(FLICKIT_SECTION, "GrindCatch", s.grindCatch, configFile);
	s.grindPick = (int)GetPrivateProfileInt(FLICKIT_SECTION, "GrindPick", s.grindPick, configFile);
	s.grindSwitch = (int)GetPrivateProfileInt(FLICKIT_SECTION, "GrindSwitch", s.grindSwitch, configFile);
	s.grindBalance = (int)GetPrivateProfileInt(FLICKIT_SECTION, "GrindBalance", s.grindBalance, configFile);
	s.grindMirror = (int)GetPrivateProfileInt(FLICKIT_SECTION, "GrindMirror", s.grindMirror, configFile);
	s.pauseOffBoard = (int)GetPrivateProfileInt(FLICKIT_SECTION, "PauseOffBoard", s.pauseOffBoard, configFile);
	s.tilt = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Tilt", s.tilt, configFile);
	s.tiltSettle = (int)GetPrivateProfileInt(FLICKIT_SECTION, "TiltSettle", s.tiltSettle, configFile);
	s.switchHold = (int)GetPrivateProfileInt(FLICKIT_SECTION, "SwitchHold", s.switchHold, configFile);
	s.railTap = (int)GetPrivateProfileInt(FLICKIT_SECTION, "RailTap", s.railTap, configFile);
	s.flickSpeed = (int)GetPrivateProfileInt(FLICKIT_SECTION, "FlickSpeed", s.flickSpeed, configFile);
	s.grindDrag = (int)GetPrivateProfileInt(FLICKIT_SECTION, "GrindDrag", s.grindDrag, configFile);
	s.grindExit = (int)GetPrivateProfileInt(FLICKIT_SECTION, "GrindExit", s.grindExit, configFile);
	s.grindBoost = (int)GetPrivateProfileInt(FLICKIT_SECTION, "GrindBoost", s.grindBoost, configFile);
	s.grindReach = (int)GetPrivateProfileInt(FLICKIT_SECTION, "GrindReach", s.grindReach, configFile);
	s.airGravity = (int)GetPrivateProfileInt(FLICKIT_SECTION, "AirGravity", s.airGravity, configFile);
	s.jumpPop = (int)GetPrivateProfileInt(FLICKIT_SECTION, "JumpPop", s.jumpPop, configFile);
	s.crouchPose = (int)GetPrivateProfileInt(FLICKIT_SECTION, "CrouchPose", s.crouchPose, configFile);
	s.push = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Push", s.push, configFile);
	s.pushOnOllie = (int)GetPrivateProfileInt(FLICKIT_SECTION, "PushOnOllie", s.pushOnOllie, configFile);
	s.pushOverTurn = (int)GetPrivateProfileInt(FLICKIT_SECTION, "PushOverTurn", s.pushOverTurn, configFile);
	s.pushMeasure = (int)GetPrivateProfileInt(FLICKIT_SECTION, "PushMeasure", s.pushMeasure, configFile);
	s.pushFootDown = (int)GetPrivateProfileInt(FLICKIT_SECTION, "PushFootDown", s.pushFootDown, configFile);
	s.pushFootUp = (int)GetPrivateProfileInt(FLICKIT_SECTION, "PushFootUp", s.pushFootUp, configFile);
	s.coast = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Coast", s.coast, configFile);
	s.steering = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Steering", s.steering, configFile);
	s.spin = (int)GetPrivateProfileInt(FLICKIT_SECTION, "Spin", s.spin, configFile);

	Flickit::Sanitize(s);
	flickitSettings = s;

	// The grinds and the jumps need changes in the game's code. They go in once and do nothing on
	// frames where Flickit does not ask for them, so switching the settings off later is enough.
	bool hooks = false;
	if (s.enabled && s.gameHooks) {
		hooks = FlickitGame::Install();
	}

	loadFlickitPatterns(configFile, s.enabled && s.patterns);

	for (int i = 0; i < MAX_PLAYERS; i++) {
		flickitRecognizers[i].Reset();
		flickitRecognizers[i].UsePatterns(flickitPatterns.Count() > 0 ? &flickitPatterns : nullptr);
	}

	Log::TypedLog(CHN_SDL, "Flickit %s: kickflip side %d, switch mirror %d, trigger grabs %d, auto grind %d, nollie %d, tricks from %s\n",
		s.enabled ? "enabled" : "disabled", s.kickflipSide, s.switchMirror, s.triggerGrabs, s.autoGrind, s.nollie,
		flickitPatterns.Count() > 0 ? "stick patterns" : "built-in directions");
	if (s.enabled) {
		if (hooks) {
			Log::TypedLog(CHN_SDL, "Flickit grinds: rails catch by themselves %d, the stick picks the grind %d, switches on the rail %d, balance %d, mirrored %d\n",
				s.grindCatch, s.grindPick, s.grindSwitch, s.grindBalance, s.grindMirror);
			if (s.grindDrag > 0) {
				Log::TypedLog(CHN_SDL, "Flickit rails: drag %d%% of Skate's, %s when the speed is gone, %s for landing on a rail\n",
					s.grindDrag, s.grindExit ? "the board steps off" : "the board stays", s.grindBoost ? "the game's speed boost" : "no speed boost");
			}
			else {
				Log::TypedLog(CHN_SDL, "Flickit rails: no drag, %s\n", s.grindExit ? "the board steps off a rail it has stopped on" : "the board stays on a rail it has stopped on");
			}
			if (s.grindCatch && s.grindReach > 0) {
				const float scale = (float)s.grindReach * 0.01f;
				Log::TypedLog(CHN_SDL, "Flickit rails: a rail catches the board from %.0f units beside it, %.0f above it and %.0f below (the game's own reach: 20 to 40 all around; hold the Grind button for that)\n",
					Flickit::kReachLevel * scale, Flickit::kReachDown * scale, Flickit::kReachUp * scale);
			}
			else if (s.grindCatch) {
				Log::TypedLog(CHN_SDL, "Flickit rails: a rail catches the board at the game's own reach, 20 to 40 units all around\n");
			}
			{
				// What an ollie off flat ground comes to: the Ollie button tapped, and held with the best ollie stat
				const Flickit::FlatJump tap = Flickit::JumpOnFlat(Flickit::kThugPopTap, s.airGravity, s.jumpPop);
				const Flickit::FlatJump held = Flickit::JumpOnFlat(Flickit::kThugPopBest, s.airGravity, s.jumpPop);
				Log::TypedLog(CHN_SDL, "Flickit jumps: gravity in the air %.0f units/s2 (%s), pop %.0f%% of the game's (%s): an ollie rises %.0f to %.0f units in %.2f to %.2f s (the game's own: 45 to 75 units in 0.52 to 0.67 s)\n",
					Flickit::kThugAirGravity * Flickit::AirGravityFactor(s.airGravity), s.airGravity > 0 ? "Skate's is 784" : "the game's own",
					100.0f * Flickit::JumpPopFactor(s.jumpPop), s.jumpPop > 0 ? "Skate's is 69" : "the game's own",
					tap.height, held.height, tap.seconds, held.seconds);
			}
			if (s.push > 0) {
				const float units = Flickit::kUnitsPerMetre * (float)s.push * 0.01f;
				Log::TypedLog(CHN_SDL, "Flickit rolling: a push adds %.0f (a tap) to %.0f units/s (the button held %.2f s) up to %.0f units/s, on the Flip button%s. One push is one play of the game's push animation, and the speed comes in while its foot pushes: %s from %d%% to %d%% of the animation; %s (the game's own: he kicks by himself, up to 394 to 675)\n",
					Flickit::kPushTap * units, Flickit::kPushMost * units, Flickit::kPushFullHold, Flickit::kPushTopSpeed * units,
					s.pushOnOllie ? " and the Ollie button" : "",
					s.pushMeasure ? "where the feet are is watched in every push animation, and until one has been watched its foot counts as pushing" : "its foot counts as pushing",
					s.pushFootDown, s.pushFootUp,
					s.pushOverTurn ? "the game shows a push in a turn too" : "in a turn the game shows the turn, and there is no push");
			}
			else {
				Log::TypedLog(CHN_SDL, "Flickit rolling: the push is the game's own\n");
			}
			if (s.coast > 0) {
				Log::TypedLog(CHN_SDL, "Flickit rolling: the board loses %.0f units/s2 at 100 units/s and %.0f at 700 (the game's own: 6 and 294, and nothing with its auto kick off)\n",
					Flickit::CoastLoss(100.0f / Flickit::kUnitsPerMetre) * Flickit::kUnitsPerMetre * (float)s.coast * 0.01f,
					Flickit::CoastLoss(700.0f / Flickit::kUnitsPerMetre) * Flickit::kUnitsPerMetre * (float)s.coast * 0.01f);
			}
			else {
				Log::TypedLog(CHN_SDL, "Flickit rolling: the friction is the game's own\n");
			}
			if (s.steering > 0) {
				Log::TypedLog(CHN_SDL, "Flickit rolling: with the stick at its end the board turns %.1f rad/s at 100 units/s, %.1f at 300, %.1f at 700; with the stick halfway %.1f, %.1f and %.1f (the game's own: %.1f at any speed)\n",
					Flickit::TurnRate(1.0f, 100.0f, s.steering), Flickit::TurnRate(1.0f, 300.0f, s.steering), Flickit::TurnRate(1.0f, 700.0f, s.steering),
					Flickit::TurnRate(0.5f, 100.0f, s.steering), Flickit::TurnRate(0.5f, 300.0f, s.steering), Flickit::TurnRate(0.5f, 700.0f, s.steering),
					Flickit::kThugGroundTurn);
			}
			else {
				Log::TypedLog(CHN_SDL, "Flickit rolling: the turning is the game's own\n");
			}
			if (s.spin > 0) {
				const float share = (float)s.spin * 0.01f;
				Log::TypedLog(CHN_SDL, "Flickit spin: in the air the stick turns the skater by how far it is pushed and how hard it was flicked: at its end %.1f rad/s at once, %.1f after 0.25 s, %.1f after 1 s for a full flick, %.0f percent of that for a stick that is just held (the game's own: %.2f to %.2f rad/s for Left or Right, after %.1f s)\n",
					Flickit::SpinRate(0.0f) * share, Flickit::SpinRate(0.25f) * share, Flickit::SpinRate(1.0f) * share, 100.0f * Flickit::kSpinLeast,
					Flickit::kThugAirSpinWorst, Flickit::kThugAirSpinBest, Flickit::kThugAirSpinWait);
			}
			else {
				Log::TypedLog(CHN_SDL, "Flickit spin: the turning in the air is the game's own\n");
			}
		}
		else {
			Log::TypedLog(CHN_SDL, "Flickit grinds: %s, the Grind button is held after a pop instead\n",
				s.gameHooks ? "the game hooks could not go in" : "game hooks are switched off");
		}
	}
}

static const char* flickitTiltText(const Flickit::Tilt& t, char* buf, size_t size) {
	static const char* first[] = { "-", "up", "down", "side" };
	if (!t.valid) {
		sprintf_s(buf, size, "stick at rest");
	}
	else {
		sprintf_s(buf, size, "stick first %s%s, now %s", first[t.first & 3],
			t.first == Flickit::TILT_SIDE ? (t.firstLeft ? " (left)" : " (right)") : "", Flickit::DirName(t.cur));
	}
	return buf;
}

// What the game did with the last rail and what its grind script says, for the log
static void flickitLogGame(const FlickitGame::Skater& sk, uint32_t now) {
	FlickitGame::Pick pick;
	if (FlickitGame::TakePick(pick)) {
		char tilt[64];
		if (pick.started == pick.grind) {
			Log::TypedLog(CHN_SDL, "Flickit: rail on the %s, %s: %s%s\n", pick.railOnLeft ? "left" : "right",
				flickitTiltText(pick.tilt, tilt, sizeof(tilt)), Flickit::GrindName(pick.grind), pick.byName ? " (by script name)" : "");
		}
		else {
			Log::TypedLog(CHN_SDL, "Flickit: rail on the %s, %s: %s, which this game's scripts do not have, so %s\n", pick.railOnLeft ? "left" : "right",
				flickitTiltText(pick.tilt, tilt, sizeof(tilt)), Flickit::GrindName(pick.grind), Flickit::GrindName(pick.started));
		}
	}

	// Rails that caught the board by themselves: how far they had to reach. And the one that did not.
	if (sk.catchSerial != flickitCatchSerial) {
		flickitCatchSerial = sk.catchSerial;
		const Flickit::RailPass& c = sk.caught;
		const float side = c.side < 0.0f ? -c.side : c.side;
		const float lift = c.lift < 0.0f ? -c.lift : c.lift;
		const float past = c.past < 0.0f ? -c.past : c.past;
		const char* where = c.lift < 0.0f ? "above" : "below";
		if (past >= 0.5f) {
			Log::TypedLog(CHN_SDL, "Flickit: the rail took the board from %.0f units beyond its end, %.0f beside it and %.0f %s it (%.0f%% of the reach)\n",
				past, side, lift, where, c.miss * 100.0f);
		}
		else {
			Log::TypedLog(CHN_SDL, "Flickit: the rail took the board from %.0f units beside it and %.0f %s it (%.0f%% of the reach)\n",
				side, lift, where, c.miss * 100.0f);
		}
	}
	if (sk.missSerial != flickitMissSerial) {
		flickitMissSerial = sk.missSerial;
		const Flickit::RailPass& m = sk.missed;
		const float side = m.side < 0.0f ? -m.side : m.side;
		const float lift = m.lift < 0.0f ? -m.lift : m.lift;
		const float past = m.past < 0.0f ? -m.past : m.past;
		const char* where = m.lift < 0.0f ? "under" : "over";
		// the setting that would have caught it; beyond what the setting can be, nothing is said
		const int would = (int)(m.miss * (float)flickitSettings.grindReach) + 1;
		if (m.away) {
			Log::TypedLog(CHN_SDL, "Flickit: no grind. The nearest rail was %.0f units beside the board and %.0f %s it, and the board had passed it\n",
				side, lift, where);
		}
		else if (would <= Flickit::kReachMostPercent) {
			if (past >= 0.5f) {
				Log::TypedLog(CHN_SDL, "Flickit: no grind. The nearest rail ended %.0f units from the board, %.0f beside it and %.0f %s it, %.0f%% of the reach (about GrindReach=%d would have caught it)\n",
					past, side, lift, where, m.miss * 100.0f, would);
			}
			else {
				Log::TypedLog(CHN_SDL, "Flickit: no grind. The nearest rail was %.0f units beside the board and %.0f %s it, %.0f%% of the reach (about GrindReach=%d would have caught it)\n",
					side, lift, where, m.miss * 100.0f, would);
			}
		}
	}

	if (sk.pushSerial != flickitPushSerial) {
		flickitPushSerial = sk.pushSerial;
		Log::TypedLog(CHN_SDL, "Flickit: push with %s (%.2f s), the foot pushed from %.0f%% to %.0f%% of it (%s); the button was down %.0f ms: %.0f units/s asked for, speed %.0f -> %.0f (the push added %.0f)\n",
			sk.pushAnim, sk.pushLength, sk.pushDown * 100.0f, sk.pushUp * 100.0f,
			sk.pushSource == FlickitGame::PUSH_MEASURED ? "watched" : (sk.pushSource == FlickitGame::PUSH_BORROWED ? "watched in another push animation" : "as set"),
			sk.pushHeld * 1000.0f, sk.pushAsked, sk.pushFrom, sk.pushTo, sk.pushAdded);
	}
	if (sk.cutSerial != flickitCutSerial) {
		flickitCutSerial = sk.cutSerial;
		Log::TypedLog(CHN_SDL, "Flickit: the push cut the game's %s short, %.0f%% into it\n", sk.cutAnim, sk.cutShare * 100.0f);
	}
	if (sk.poseSerial != flickitPoseSerial) {
		flickitPoseSerial = sk.poseSerial;
		const char* name = FlickitPose::AnimName(sk.poseFrom);
		if (name) {
			Log::TypedLog(CHN_SDL, "Flickit: the crouch goes as deep as the stick, from %s\n", name);
		}
		else {
			Log::TypedLog(CHN_SDL, "Flickit: the crouch goes as deep as the stick, from animation %08X\n", sk.poseFrom);
		}
	}
	if (sk.poseLostSerial != flickitPoseLostSerial) {
		flickitPoseLostSerial = sk.poseLostSerial;
		Log::TypedLog(CHN_SDL, "Flickit: the game started an animation without a blend in the crouch: for the rest of it he crouches as the game has it\n");
	}
	if (sk.poseUnderSerial != flickitPoseUnderSerial) {
		flickitPoseUnderSerial = sk.poseUnderSerial;
		const char* crouch = FlickitPose::AnimName(sk.poseUnderCrouch);
		const char* stand = FlickitPose::AnimName(sk.poseUnderStand);
		Log::TypedLog(CHN_SDL, "Flickit: the crouch had nothing standing to blend with (he is in %s): %s is started under it, and the crouch goes as deep as the stick\n",
			crouch ? crouch : "a crouch", stand ? stand : "the idle");
	}
	if (sk.stanceSerial != flickitStanceSerial) {
		flickitStanceSerial = sk.stanceSerial;
		if (sk.stanceTo == Flickit::STANCE_NOLLIE) {
			Log::TypedLog(CHN_SDL, "Flickit: the stick crouches up: nollie stance%s\n", sk.stancePressure ? " (out of the game's pressure stance)" : "");
		}
		else if (sk.stanceLate) {
			Log::TypedLog(CHN_SDL, "Flickit: the crouch up ended and he is still in nollie on the ground: out of nollie stance\n");
		}
		else {
			Log::TypedLog(CHN_SDL, "Flickit: out of nollie stance\n");
		}
	}
	if (sk.poseMissSerial != flickitPoseMissSerial) {
		flickitPoseMissSerial = sk.poseMissSerial;
		const char* in = FlickitPose::AnimName(sk.poseMissAnim);
		const char* last = FlickitPose::AnimName(sk.poseMissLast);
		Log::TypedLog(CHN_SDL, "Flickit: the crouch could not follow the stick: he is in %s (%08X), %d channel%s, blending %d; last stood in %s (%08X)\n",
			in ? in : "animation", sk.poseMissAnim, sk.poseMissChannels, sk.poseMissChannels == 1 ? "" : "s", sk.poseMissBlending,
			last ? last : (sk.poseMissLast ? "animation" : "nothing seen"), sk.poseMissLast);
	}
	if (sk.waySerial != flickitWaySerial) {
		flickitWaySerial = sk.waySerial;
		Log::TypedLog(CHN_SDL, "Flickit: the push in %s gave way to %s, %.0f%% into it\n", sk.wayAnim,
			sk.wayTo == Flickit::GIVE_CROUCH ? "the crouch" : "Down", sk.wayShare * 100.0f);
	}
	if (sk.blindSerial != flickitBlindSerial) {
		flickitBlindSerial = sk.blindSerial;
		Log::TypedLog(CHN_SDL, "Flickit: the game was told to play a push three times and no push animation was seen on the skater (he is in animation %08X). Pushes go by the clock until one is seen\n", sk.blindAnim);
	}
	if (sk.footSerial != flickitFootSerial) {
		flickitFootSerial = sk.footSerial;
		const char* toe = sk.foot.left ? "Bone_Toe_L" : "Bone_Toe_R";
		if (sk.foot.why == Flickit::FOOT_OK && sk.foot.swept) {
			Log::TypedLog(CHN_SDL, "Flickit: watched the feet in %s (%.2f s, %d looks): %s goes down to %.1f, %.1f below where the other foot stands, and is on the ground from %.0f%% to %.0f%% of the animation. It goes back %.1f units from %.0f%% to %.0f%%: pushes get their speed as it goes\n",
				sk.footAnim, sk.footLength, sk.foot.samples, toe, sk.foot.floor, sk.foot.drop,
				sk.foot.lowFrom * 100.0f, sk.foot.lowTo * 100.0f, sk.foot.sweep, sk.foot.profile.down * 100.0f, sk.foot.profile.up * 100.0f);
		}
		else if (sk.foot.why == Flickit::FOOT_OK) {
			Log::TypedLog(CHN_SDL, "Flickit: watched the feet in %s (%.2f s, %d looks): %s goes down to %.1f, %.1f below where the other foot stands, and is on the ground from %.0f%% to %.0f%% of the animation, but goes back only %.1f units: pushes get their speed while it is on the ground\n",
				sk.footAnim, sk.footLength, sk.foot.samples, toe, sk.foot.floor, sk.foot.drop,
				sk.foot.lowFrom * 100.0f, sk.foot.lowTo * 100.0f, sk.foot.sweep);
		}
		else if (sk.foot.why == Flickit::FOOT_SHORT) {
			Log::TypedLog(CHN_SDL, "Flickit: watched the feet in %s (%.2f s, %d looks) and found no foot pushing: %s goes %.1f below where the other foot stands, but is on the ground only from %.0f%% to %.0f%% of the animation and goes back %.1f units. Pushes with it go by another push animation or by PushFootDown and PushFootUp\n",
				sk.footAnim, sk.footLength, sk.foot.samples, toe, sk.foot.drop, sk.foot.lowFrom * 100.0f, sk.foot.lowTo * 100.0f, sk.foot.sweep);
		}
		else {
			Log::TypedLog(CHN_SDL, "Flickit: watched the feet in %s (%.2f s, %d looks) and found no foot pushing: %s (the lower foot gets %.1f below where the other stands). Pushes with it go by another push animation or by PushFootDown and PushFootUp\n",
				sk.footAnim, sk.footLength, sk.foot.samples,
				sk.foot.why == Flickit::FOOT_NO_DROP ? "neither foot gets below the other" : "the feet could not be read on enough frames",
				sk.foot.drop);
		}
		if (sk.footTraceLooks > 0 && sk.footTrace) {
			// Every look at the feet in this animation, the first time it was watched whole:
			// four looks to a line
			Log::TypedLog(CHN_SDL, "Flickit: the toes in %s, look by look (share of the animation: left x y z / right x y z, y is up):\n", sk.footAnim);
			char line[512];
			int used = 0;
			for (int i = 0; i < sk.footTraceLooks; i++) {
				const float* look = sk.footTrace + 7 * i;
				used += snprintf(line + used, sizeof(line) - used, "%s%.3f: %.1f %.1f %.1f / %.1f %.1f %.1f",
					used ? " | " : "  ", look[0], look[1], look[2], look[3], look[4], look[5], look[6]);
				if (used >= (int)sizeof(line)) {
					used = (int)sizeof(line) - 1;
				}
				if (i % 4 == 3 || i == sk.footTraceLooks - 1) {
					Log::TypedLog(CHN_SDL, "%s\n", line);
					used = 0;
				}
			}
		}
	}
	if (sk.spinSerial != flickitSpinSerial) {
		flickitSpinSerial = sk.spinSerial;
		// an air in which the stick asked for next to nothing is not worth a line
		if (sk.spinTurned > 0.05f || sk.spinTurned < -0.05f) {
			Log::TypedLog(CHN_SDL, "Flickit: spin, %.2f s in the air, flick %.0f%%: %.0f degrees to the %s asked for\n",
				sk.spinSeconds, sk.spinFlick * 100.0f, (sk.spinTurned < 0.0f ? -sk.spinTurned : sk.spinTurned) * 57.29578f, sk.spinTurned < 0.0f ? "left" : "right");
		}
	}

	const bool onRail = (sk.state == Flickit::STATE_RAIL);
	if (onRail && !flickitOnRail) {
		flickitRailSince = now;
	}
	flickitOnRail = onRail;
	if (!onRail) {
		if (flickitRailTimed) {
			Log::TypedLog(CHN_SDL, "Flickit: off the rail after %.1f s, speed %.0f -> %.0f\n", flickitRailSeconds, flickitRailSpeedIn, flickitRailSpeed);
		}
		flickitRailTimed = false;
		flickitDragClass = -1;
		flickitDragSurface = -1;
		return;
	}
	if (sk.railSeconds > 0.0f) {
		// The game's rail code has run with the drag or the step-off on
		if (!flickitRailTimed || sk.railSeconds < flickitRailSeconds) {
			flickitRailTimed = true;
			flickitRailSpeedIn = sk.railSpeed;
		}
		flickitRailSeconds = sk.railSeconds;
		flickitRailSpeed = sk.railSpeed;
		if (flickitSettings.grindDrag > 0 && (sk.dragClass != flickitDragClass || sk.railSurface != flickitDragSurface)) {
			flickitDragClass = sk.dragClass;
			flickitDragSurface = sk.railSurface;
			const float scale = Flickit::kUnitsPerMetre * (float)flickitSettings.grindDrag * 0.01f;
			Log::TypedLog(CHN_SDL, "Flickit: on a %s (terrain %d), %s: the drag starts at %.0f and grows to %.0f units/s2\n",
				Flickit::SurfaceName(sk.railSurface), sk.railTerrain, Flickit::DragClassName(sk.dragClass),
				Flickit::SkateDrag(sk.dragClass, sk.railSurface, 0.0f) * scale, Flickit::SkateDrag(sk.dragClass, sk.railSurface, 60.0f) * scale);
		}
	}
	if (sk.grindSerial != flickitGrindSerial || sk.grind != flickitGrindKind || sk.grindReady != flickitGrindReady) {
		if (sk.grind != Flickit::GRIND_UNKNOWN && (sk.grind != flickitGrindKind || sk.grindSerial != flickitGrindSerial)) {
			Log::TypedLog(CHN_SDL, "Flickit: the game's grind script reports a %s\n",
				sk.grind == Flickit::GRIND_OTHER ? "grind the stick does not switch from" : Flickit::GrindName(sk.grind));
		}
		if (sk.grindReady && !flickitGrindReady) {
			Log::TypedLog(CHN_SDL, "Flickit: the grind takes switches now\n");
		}
		flickitGrindSerial = sk.grindSerial;
		flickitGrindKind = sk.grind;
		flickitGrindReady = sk.grindReady;
	}
	if (sk.grind == Flickit::GRIND_UNKNOWN && !flickitNoReportLogged && (int32_t)(now - flickitRailSince) > 2000) {
		// The Grind script calls IsPs2 15 frames in. If that never arrives, another script set is running.
		flickitNoReportLogged = true;
		Log::TypedLog(CHN_SDL, "Flickit: two seconds on a rail and no word from the game's grind script. Switching grinds with the stick needs it; everything else works without\n");
	}
}

// Reads the camera stick and the triggers and asks the recognizer which buttons to hold on this frame.
// Answers FLICKIT_ON when Flickit drives this controller on this frame, FLICKIT_PAUSED when it would but
// the skater is off the board, FLICKIT_OFF otherwise.
int flickitPoll(device* dev, SDL_GameController* controller, Flickit::Output* out) {
	if (!flickitSettings.enabled || dev->port >= MAX_PLAYERS) {
		return FLICKIT_OFF;
	}
	const int port = (int)dev->port;

	// On / off switch, for anything that wants the camera stick back
	if (flickitSettings.toggleButton >= 0 && flickitSettings.toggleButton < SDL_CONTROLLER_BUTTON_MAX) {
		const bool down = SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)flickitSettings.toggleButton) != 0;
		if (down && !flickitToggleDown[port]) {
			flickitOn[port] = !flickitOn[port];
			flickitRecognizers[port].Reset();
			SDL_GameControllerRumble(controller, 0x3000, 0x3000, flickitOn[port] ? 200 : 60);
			Log::TypedLog(CHN_SDL, "Flickit %s for player %d\n", flickitOn[port] ? "on" : "off", port + 1);
		}
		flickitToggleDown[port] = down;
	}
	if (!flickitOn[port]) {
		return FLICKIT_OFF;
	}

	const bool leftStick = (padbinds.camera == CONTROLLER_STICK_LEFT);

	Flickit::Input in;
	in.now = SDL_GetTicks();
	in.rx = (float)SDL_GameControllerGetAxis(controller, leftStick ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f;
	in.ry = -(float)SDL_GameControllerGetAxis(controller, leftStick ? SDL_CONTROLLER_AXIS_LEFTY : SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;	// SDL: down is positive
	in.leftTrigger = getButton(controller, CONTROLLER_BUTTON_LEFTTRIGGER) != 0;
	in.rightTrigger = getButton(controller, CONTROLLER_BUTTON_RIGHTTRIGGER) != 0;

	// What the skater is doing, where the game can be asked (the first player's skater only).
	// Without it the recognizer works from the controller alone, as it did before the grinds.
	if (port == 0) {
		FlickitGame::Skater skater;
		flickitSeesSkater = flickitSettings.gameHooks && FlickitGame::ReadSkater(skater);
		if (flickitSeesSkater) {
			const bool paused = flickitSettings.pauseOffBoard && !skater.skating;
			if (paused != flickitPaused) {
				flickitPaused = paused;
				Log::TypedLog(CHN_SDL, paused ? "Flickit: off the board, the stick and the triggers are the game's again\n" : "Flickit: back on the board\n");
			}
			if (paused) {
				return FLICKIT_PAUSED;
			}
			flickitRolling = skater.rolling;
			in.state = skater.state;
			in.railCatch = FlickitGame::Installed() && flickitSettings.grindCatch;
			in.keepsStance = FlickitGame::Installed();
			in.switched = skater.switched;
			in.grind = skater.grind;
			in.grindReady = skater.grindReady;
			in.grindSerial = skater.grindSerial;
			in.railOff = skater.railOff;
			in.offSide = skater.offSide;
			in.flipMore = skater.flipMore;
			in.flipLevel = skater.flipLevel;
			flickitFlipSkater = skater;
			if (flickitSettings.debug) {
				flickitLogGame(skater, in.now);
			}
		}
		else {
			flickitPaused = false;
		}
	}

	flickitRecognizers[port].Update(in, flickitSettings, *out);

	if (flickitSettings.debug && port == 0 && out->mirrored != flickitMirrored) {
		// Riding switch turns the flicks round, as in Skate: the kickflip is on the other side
		Log::TypedLog(CHN_SDL, "Flickit: %s: the kickflip is the flick up-%s now\n",
			in.switched ? "riding switch" : "riding regular", out->mirrored ? "left" : "right");
	}
	if (port == 0) {
		flickitMirrored = out->mirrored;
	}

	if (flickitSettings.debug) {
		const char* where = (out->state == Flickit::STATE_UNKNOWN) ? "" : (out->state == Flickit::STATE_RAIL) ? ", on a rail" : (out->state == Flickit::STATE_AIR) ? ", in the air" : "";
		if (out->switchTo != Flickit::GRIND_UNKNOWN) {
			Log::TypedLog(CHN_SDL, "Flickit: on the rail, switching to the %s\n", Flickit::GrindName(out->switchTo));
		}
		if (out->stepStarted) {
			Log::TypedLog(CHN_SDL, "Flickit: out of speed on the rail, stepping off %s\n",
				out->stepSide < 0 ? "to the left, where the board came from" : out->stepSide > 0 ? "to the right, where the board came from" : "wherever the game finds room");
		}
		if (out->popStrength >= 0.0f) {
			Log::TypedLog(CHN_SDL, "Flickit: pop out of a crouch %.0f%% deep\n", out->popStrength * 100.0f);
		}
		if (out->standUp) {
			Log::TypedLog(CHN_SDL, "Flickit: out of the crouch without a flick: he stands up\n");
		}
		if (port == 0 && flickitSeesSkater) {
			const FlickitGame::Skater& sk = flickitFlipSkater;
			if (out->flipAgain) {
				Log::TypedLog(CHN_SDL, "Flickit: the stick is held: one more rotation, %s (%.2f s of air left, %.2f s needed)\n", sk.flipNext, sk.flipAirLeft, sk.flipNeed);
			}
			else if (out->holding && sk.flipDecide && !sk.flipMore && sk.flipLevel != flickitFlipToldLevel) {
				flickitFlipToldLevel = sk.flipLevel;
				Log::TypedLog(CHN_SDL, "Flickit: the stick is held, but there is not air enough for a %s (%.2f s left, %.2f s needed)\n", sk.flipNext, sk.flipAirLeft, sk.flipNeed);
			}
		}
		if (out->gesture == Flickit::GESTURE_GRAB) {
			Log::TypedLog(CHN_SDL, "Flickit: grab, %s trigger\n", out->slot == Flickit::DIR_L ? "left" : "right");
		}
		else if (out->gesture != Flickit::GESTURE_NONE && out->pattern) {
			Log::TypedLog(CHN_SDL, "Flickit: pattern %s, speed %.2f, %d ticks: %s%s\n", out->pattern, out->speed, out->ticks,
				out->gesture == Flickit::GESTURE_FLIP ? Flickit::SlotName(out->slot) : "ollie", where);
		}
		else if (out->gesture != Flickit::GESTURE_NONE) {
			Log::TypedLog(CHN_SDL, "Flickit: %s %s -> %s after %u ms: %s%s\n",
				out->windup == Flickit::WIND_NOLLIE ? "nollie" : "ollie",
				Flickit::DirName(out->start), Flickit::DirName(out->end), (unsigned int)out->elapsed,
				out->gesture == Flickit::GESTURE_FLIP ? Flickit::SlotName(out->slot) : "no flip", where);
		}
	}
	return FLICKIT_ON;
}

// Tells the game's rail code what Flickit wants on this frame. Runs after the real buttons are in the pad
// data and before Flickit's own, so the Grind button seen here is the player's.
void flickitGameFrame(device* dev, const Flickit::Output& out) {
	if (dev->port != 0 || !flickitSeesSkater || !FlickitGame::Installed()) {
		return;
	}
	FlickitGame::Frame frame;
	frame.standUp = out.standUp;
	frame.popStrength = out.popStrength;
	frame.crouchPose = flickitSettings.crouchPose ? out.crouchPose : -1.0f;
	frame.stance = out.stance;
	frame.catchRails = flickitSettings.grindCatch != 0;
	frame.pickGrind = flickitSettings.grindPick != 0;
	frame.noBalance = flickitSettings.grindBalance == 0;
	frame.mirror = flickitSettings.grindMirror != 0;
	frame.classicGrind = (dev->controlData[3] & (0x01 << 4)) != 0;
	frame.tilt = flickitRecognizers[0].GetTilt();
	frame.drag = flickitSettings.grindDrag;
	frame.stepOff = flickitSettings.grindExit != 0;
	frame.boost = flickitSettings.grindBoost != 0;
	frame.reach = flickitSettings.grindReach;
	frame.gravity = flickitSettings.airGravity;
	frame.pop = flickitSettings.jumpPop;
	frame.push = flickitSettings.push;
	frame.pushHeld = flickitPushHeld;
	frame.pushOverTurn = flickitSettings.pushOverTurn != 0;
	frame.pushMeasure = flickitSettings.pushMeasure != 0;
	frame.pushFootDown = (float)flickitSettings.pushFootDown * 0.01f;
	frame.pushFootUp = (float)flickitSettings.pushFootUp * 0.01f;
	frame.coast = flickitSettings.coast;
	frame.steering = flickitSettings.steering;
	frame.spin = flickitSettings.spin;
	{
		// How long ago the last frame was, for what is timed between looks at the controller.
		// A first frame, and one after a long pause, counts as a 60th of a second.
		const uint32_t now = SDL_GetTicks();
		const uint32_t passed = now - flickitFrameAt;
		frame.seconds = (flickitFrameSeen && passed <= 250) ? (float)passed * 0.001f : 1.0f / 60.0f;
		flickitFrameAt = now;
		flickitFrameSeen = true;
	}
	// The movement stick as the game is about to see it, the d-pad and the keys as a stick at its end.
	// A stick at rest is not quite at its centre: the first fifth of the way counts as none, which is
	// what the console Skate ran on did before the game saw the stick.
	frame.steer = ((float)dev->controlData[6] - 128.0f) / 127.0f;
	{
		const float rest = 0.2f;
		const float side = frame.steer < 0.0f ? -1.0f : 1.0f;
		const float out = (frame.steer * side - rest) / (1.0f - rest);
		frame.steer = out <= 0.0f ? 0.0f : (out >= 1.0f ? side : side * out);
	}
	if (dev->controlData[2] & (0x01 << 7)) {
		frame.steer = -1.0f;
	}
	else if (dev->controlData[2] & (0x01 << 5)) {
		frame.steer = 1.0f;
	}
	FlickitGame::SetFrame(frame);
}

// The push buttons. Skate pushes on two buttons, one for each foot. Here they are the Flip button,
// which is THUG's own button for a kick, and the Ollie button, which has nothing to do on the
// ground while Ollie is on the stick.
//
// The Flip button pushes whenever it is down while the board rolls on the ground. The Ollie button
// pushes if its press begins there: one that is held from the air into a landing is the game's
// crouch for the next ollie, as it always was. Either stays a push until the button is let go.
//
// A press that is a push is not passed on to the game, neither as Ollie nor as Flip: the push is
// Flickit's, and the game would take the Flip button for the start of a flip trick as well.
// The game's scripts hear of the push from the game hooks, to play its animation.
void flickitPushButtons(device* dev, bool on, bool ollieDown, bool flipDown) {
	if (dev->port != 0) {
		return;
	}
	const bool pushes = on && flickitSeesSkater && flickitRolling && flickitSettings.push > 0 && FlickitGame::Installed();
	if (!ollieDown || !on) {
		flickitOlliePush = false;
	}
	else if (!flickitOllieWas) {
		flickitOlliePush = pushes && flickitSettings.pushOnOllie != 0;
	}
	if (!flipDown || !on) {
		flickitFlipPush = false;
	}
	else if (pushes) {
		flickitFlipPush = true;
	}
	flickitOllieWas = ollieDown;
	flickitPushHeld = flickitOlliePush || flickitFlipPush;
}

// Merges what the recognizer asked for into the pad data. Runs after the real buttons, d-pad and sticks.
void flickitApply(device* dev, const Flickit::Output& o) {
	if (o.grind) {
		dev->controlData[3] |= 0x01 << 4;
		dev->controlData[12] = 0xff;
	}
	if (o.grab) {
		dev->controlData[3] |= 0x01 << 5;
		dev->controlData[13] = 0xff;
	}
	if (o.ollie) {
		dev->controlData[3] |= 0x01 << 6;
		dev->controlData[14] = 0xff;
	}
	if (o.flip) {
		dev->controlData[3] |= 0x01 << 7;
		dev->controlData[15] = 0xff;
	}
	if (o.nollie) {
		dev->controlData[3] |= 0x01 << 0;
		dev->controlData[18] = 0xff;
	}
	if (o.stepOff) {
		// The game's own button for getting off a rail: Revert / Switch (R2 on its PlayStation pad)
		dev->controlData[3] |= 0x01 << 1;
		dev->controlData[19] = 0xff;
		// With Left or Right held the skater steps off to that side. A direction the player holds wins.
		const bool playerSide = (dev->controlData[2] & ((0x01 << 5) | (0x01 << 7))) != 0
			|| dev->controlData[6] < 0x80 - 0x30 || dev->controlData[6] > 0x80 + 0x30;
		if (!playerSide && o.stepSide < 0) {
			dev->controlData[2] |= 0x01 << 7;
			dev->controlData[9] = 0xFF;
		}
		else if (!playerSide && o.stepSide > 0) {
			dev->controlData[2] |= 0x01 << 5;
			dev->controlData[8] = 0xFF;
		}
	}

	if (o.ownsDirection) {
		// For the few frames of a trick press the game must see exactly one direction:
		// drop whatever the d-pad, the keyboard and the movement stick are doing.
		dev->controlData[2] &= 0x0F;
		dev->controlData[8] = 0;
		dev->controlData[9] = 0;
		dev->controlData[10] = 0;
		dev->controlData[11] = 0;
		dev->controlData[6] = 0x80;
		dev->controlData[7] = 0x80;

		const bool up = (o.dir == Flickit::DIR_U || o.dir == Flickit::DIR_UL || o.dir == Flickit::DIR_UR);
		const bool down = (o.dir == Flickit::DIR_D || o.dir == Flickit::DIR_DL || o.dir == Flickit::DIR_DR);
		const bool left = (o.dir == Flickit::DIR_L || o.dir == Flickit::DIR_UL || o.dir == Flickit::DIR_DL);
		const bool right = (o.dir == Flickit::DIR_R || o.dir == Flickit::DIR_UR || o.dir == Flickit::DIR_DR);

		if (up) {
			dev->controlData[2] |= 0x01 << 4;
			dev->controlData[10] = 0xFF;
		}
		if (right) {
			dev->controlData[2] |= 0x01 << 5;
			dev->controlData[8] = 0xFF;
		}
		if (down) {
			dev->controlData[2] |= 0x01 << 6;
			dev->controlData[11] = 0xFF;
		}
		if (left) {
			dev->controlData[2] |= 0x01 << 7;
			dev->controlData[9] = 0xFF;
		}
	}
}

void getStick(SDL_GameController* controller, controllerStick stick, uint8_t* xOut, uint8_t* yOut) {
	uint8_t result_x, result_y;

	if (stick == CONTROLLER_STICK_LEFT) {
		result_x = (uint8_t)((SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) >> 8) + 128);
		result_y = (uint8_t)((SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) >> 8) + 128);
	}
	else if (stick == CONTROLLER_STICK_RIGHT) {
		result_x = inputsettings.disableRXplayer1 ? 128 : ((inputsettings.invertRXplayer1 && !(ParkEd->m_state == EEditorState::vEDITING)) ? 255 - (uint8_t)((SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX) >> 8) + 128) : (uint8_t)((SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX) >> 8) + 128));
		result_y = inputsettings.disableRYplayer1 ? 128 : ((inputsettings.invertRYplayer1 && !(ParkEd->m_state == EEditorState::vEDITING)) ? 255 - (uint8_t)((SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY) >> 8) + 128) : (uint8_t)((SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY) >> 8) + 128));
	}
	else {
		result_x = 0x80;
		result_y = 0x80;
	}

	if (axisAbs(result_x) > axisAbs(*xOut)) {
		*xOut = result_x;
	}

	if (axisAbs(result_y) > axisAbs(*yOut)) {
		*yOut = result_y;
	}
}

void __cdecl set_actuators(int port, uint16_t high, uint16_t low) {
	for (int i = 0; i < controllerCount; i++) {
		if (SDL_GameControllerGetAttached(controllerList[i]) && SDL_GameControllerGetPlayerIndex(controllerList[i]) == port) {
			SDL_JoystickRumble(SDL_GameControllerGetJoystick(controllerList[i]), low, high, 0);
		}
	}
}

uint32_t convert_SDL_to_OIS_keycode(uint8_t sdlKeyCode) {
	// Lookup the SDL keycode in the map
	auto it = SDL_to_OIS_map.find(sdlKeyCode);
	if (it != SDL_to_OIS_map.end()) {
		// Return the corresponding OIS keycode
		return it->second;
	}
	else {
		// If no mapping found, return an unspecified keycode
		return (uint32_t)OIS_KC_UNASSIGNED;
	}
}

void setUsingKeyboard(uint8_t usingKeyboard) {
	isUsingKeyboard = usingKeyboard;
}

bool shouldUseMenuControls() {
	return (*isMenu && !(ParkEd->m_state == EEditorState::vEDITING) || *isCAG || ((ParkEd->m_state == EEditorState::vEDITING) && ParkEd->m_paused));
}


/******************************************************************/
/*                                                                */
/*  Device polling												  */
/******************************************************************/

void pollController(device* dev, SDL_GameController* controller) {

	bool flickit = false;			// true on frames where Flickit drives this controller
	int flickitMode = FLICKIT_OFF;
	Flickit::Output flickitOut;

	if (SDL_GameControllerGetAttached(controller)) {
		dev->isValid = 1;
		dev->isPluggedIn = 1;

		// We don't check for keyboard_onscreen since the onscreen keyboard is displayed
		if (ParkEd) {

			if (shouldUseMenuControls()) {

				/********************************************************************/
				/*																	*/
				/*  Menu controls													*/
				/********************************************************************/

				if (getButton(controller, padbinds.menu)) {
					dev->controlData[2] |= 0x01 << 3;
				}
				if (getButton(controller, padbinds.grind)) {
					dev->controlData[3] |= 0x01 << 4;
				}
				if (getButton(controller, padbinds.grab)) {
					dev->controlData[3] |= 0x01 << 5;
				}
				if (getButton(controller, padbinds.ollie)) {
					dev->controlData[3] |= 0x01 << 6;
				}
				if (getButton(controller, padbinds.kick)) {
					dev->controlData[3] |= 0x01 << 7;
				}
				if (*isCAG) {
					if (inputsettings.isPs2Controls) {
						if (getButton(controller, padbinds.leftSpin)) {
							dev->controlData[3] |= 0x01 << 3;
						}
						if (getButton(controller, padbinds.rightSpin)) {
							dev->controlData[20] |= 0x01 << 0;
						}
						if (getButton(controller, padbinds.nollie)) {
							dev->controlData[3] |= 0x01 << 2;
						}
						if (getButton(controller, padbinds.switchRevert)) {
							dev->controlData[20] |= 0x01 << 1;
						}
					}
					else {
						if (getButton(controller, padbinds.leftSpin)) {
							dev->controlData[3] |= 0x01 << 2;
						}
						if (getButton(controller, padbinds.rightSpin)) {
							dev->controlData[3] |= 0x01 << 3;
						}
						if (getButton(controller, padbinds.caveman)) {
							dev->controlData[20] |= 0x01 << 1;
						}
						if (getButton(controller, padbinds.caveman2)) {
							dev->controlData[20] |= 0x01 << 0;
						}
					}
				}
				else {
					if (inputsettings.isPs2Controls) {
						if (getButton(controller, padbinds.leftSpin)) {
							dev->controlData[3] |= 0x01 << 2;
						}
						if (getButton(controller, padbinds.rightSpin)) {
							dev->controlData[3] |= 0x01 << 3;
						}
						if (getButton(controller, padbinds.nollie)) {
							dev->controlData[20] |= 0x01 << 1;
						}
						if (getButton(controller, padbinds.switchRevert)) {
							dev->controlData[20] |= 0x01 << 0;
						}
					}
					else {
						if (getButton(controller, padbinds.leftSpin)) {
							dev->controlData[3] |= 0x01 << 2;
						}
						if (getButton(controller, padbinds.rightSpin)) {
							dev->controlData[3] |= 0x01 << 3;
						}
						if (getButton(controller, padbinds.caveman)) {
							dev->controlData[20] |= 0x01 << 1;
						}
						if (getButton(controller, padbinds.caveman2)) {
							dev->controlData[20] |= 0x01 << 0;
						}
					}
				}
				if (SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)padbinds.up)) {
					dev->controlData[2] |= 0x01 << 4;
				}
				if (SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)padbinds.right)) {
					dev->controlData[2] |= 0x01 << 5;
				}
				if (SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)padbinds.down)) {
					dev->controlData[2] |= 0x01 << 6;
				}
				if (SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)padbinds.left)) {
					dev->controlData[2] |= 0x01 << 7;
				}
				getStick(controller, padbinds.camera, &(dev->controlData[4]), &(dev->controlData[5]));
				getStick(controller, padbinds.movement, &(dev->controlData[6]), &(dev->controlData[7]));
			}
			else {
				if (ParkEd->m_state == EEditorState::vEDITING) {

					/******************************************************************/
					/*                                                                */
					/*  Gameplay (CAP editing) controls								  */
					/******************************************************************/

					if (getButton(controller, padbinds.menu)) {
						dev->controlData[2] |= 0x01 << 3;
					}
					if (getButton(controller, padbinds.grind)) {
						dev->controlData[3] |= 0x01 << 4;
					}
					if (getButton(controller, padbinds.grab)) {
						dev->controlData[3] |= 0x01 << 5;
					}
					if (getButton(controller, padbinds.ollie)) {
						dev->controlData[3] |= 0x01 << 6;
					}
					if (getButton(controller, padbinds.kick)) {
						dev->controlData[3] |= 0x01 << 7;
					}
					if (inputsettings.isPs2Controls) {
						if (getButton(controller, padbinds.leftSpin)) {
							dev->controlData[3] |= 0x01 << 2;
						}
						if (getButton(controller, padbinds.rightSpin)) {
							dev->controlData[20] |= 0x01 << 0;
						}
						if (getButton(controller, padbinds.nollie)) {
							dev->controlData[3] |= 0x01 << 0;
						}
						if (getButton(controller, padbinds.switchRevert)) {
							dev->controlData[20] |= 0x01 << 1;
						}
					}
					else {
						if (getButton(controller, padbinds.leftSpin)) {
							dev->controlData[3] |= 0x01 << 0; // Lower
						}
						if (getButton(controller, padbinds.rightSpin)) {
							dev->controlData[3] |= 0x01 << 2; // Raise
						}
						if (getButton(controller, padbinds.caveman)) {
							dev->controlData[20] |= 0x01 << 1; // White
						}
						if (getButton(controller, padbinds.caveman2)) {
							dev->controlData[20] |= 0x01 << 0; // Black
						}
					}
				}
				else {

					/******************************************************************/
					/*                                                                */
					/*  Gameplay (in level) controls								  */
					/******************************************************************/

					flickitMode = flickitPoll(dev, controller, &flickitOut);
					flickit = (flickitMode == FLICKIT_ON);

					// Buttons
					if (getButton(controller, padbinds.menu)) {
						dev->controlData[2] |= 0x01 << 3;
					}
					if (getButton(controller, padbinds.cameraToggle)) {
						dev->controlData[2] |= 0x01 << 0;
					}
					// The Flickit on/off button is not passed on as swivel lock
					if (getButton(controller, padbinds.cameraSwivelLock) && !(flickitSettings.enabled && (int)padbinds.cameraSwivelLock == flickitSettings.toggleButton)) {
						dev->controlData[2] |= 0x01 << 2;
					}
					if (getButton(controller, padbinds.grind)) {
						dev->controlData[3] |= 0x01 << 4;
						dev->controlData[12] = 0xff;
					}
					if (getButton(controller, padbinds.grab)) {
						dev->controlData[3] |= 0x01 << 5;
						dev->controlData[13] = 0xff;
					}
					{
						// With Flickit's push a press of the Ollie or the Flip button on the ground is a
						// push, and not the game's to see
						const bool ollieDown = getButton(controller, padbinds.ollie) != 0;
						const bool flipDown = getButton(controller, padbinds.kick) != 0;
						flickitPushButtons(dev, flickit, ollieDown, flipDown);
						if (ollieDown && !(dev->port == 0 && flickitOlliePush)) {
							dev->controlData[3] |= 0x01 << 6;
							dev->controlData[14] = 0xff;
						}
						if (flipDown && !(dev->port == 0 && flickitFlipPush)) {
							dev->controlData[3] |= 0x01 << 7;
							dev->controlData[15] = 0xff;
						}
					}
					// Shoulders
					if (flickit && flickitSettings.triggerGrabs) {
						// The triggers belong to the grabs. The bumpers take over what the triggers did,
						// doubled up the way the original Xbox pad had it:
						// left = spin left + nollie, right = spin right + revert / switch, both = caveman
						const bool leftBumper = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
						const bool rightBumper = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
						if (leftBumper) {
							dev->controlData[3] |= 0x01 << 2;
							dev->controlData[16] = 0xff;
							dev->controlData[3] |= 0x01 << 0;
							dev->controlData[18] = 0xff;
						}
						if (rightBumper) {
							dev->controlData[3] |= 0x01 << 3;
							dev->controlData[17] = 0xff;
							dev->controlData[3] |= 0x01 << 1;
							dev->controlData[19] = 0xff;
						}
						if (leftBumper && rightBumper) {
							dev->controlData[20] |= 0x01 << 0;
						}
					}
					else if (inputsettings.isPs2Controls) {
						if (getButton(controller, padbinds.leftSpin)) {
							dev->controlData[3] |= 0x01 << 2;
							dev->controlData[16] = 0xff;
						}
						if (getButton(controller, padbinds.rightSpin)) {
							dev->controlData[3] |= 0x01 << 3;
							dev->controlData[17] = 0xff;
						}
						if (getButton(controller, padbinds.nollie)) {
							dev->controlData[3] |= 0x01 << 0;
							dev->controlData[18] = 0xff;
						}
						if (getButton(controller, padbinds.switchRevert)) {
							dev->controlData[3] |= 0x01 << 1;
							dev->controlData[19] = 0xff;
						}
						//Two button caveman on spinkeys
						if ((getButton(controller, padbinds.leftSpin)) && getButton(controller, padbinds.rightSpin)) {
							dev->controlData[20] |= 0x01 << 0;
						}
					}
					else // Xbox controls
					{
						if (getButton(controller, padbinds.leftSpin)) {
							dev->controlData[3] |= 0x01 << 2;
							dev->controlData[16] = 0xff;
							dev->controlData[3] |= 0x01 << 0;
							dev->controlData[18] = 0xff;
						}
						if (getButton(controller, padbinds.rightSpin)) {
							dev->controlData[3] |= 0x01 << 3;
							dev->controlData[17] = 0xff;
							dev->controlData[3] |= 0x01 << 1;
							dev->controlData[19] = 0xff;
						}
						if (getButton(controller, padbinds.caveman)) {
							dev->controlData[20] |= 0x01 << 1;
						}
						if (getButton(controller, padbinds.caveman2)) {
							dev->controlData[20] |= 0x01 << 0;
						}
						// Off the board Flickit stands down and the triggers are the game's again, but
						// what got the skater off the board gets him back on: both bumpers
						if (flickitMode == FLICKIT_PAUSED && flickitSettings.triggerGrabs
							&& SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
							&& SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
							dev->controlData[20] |= 0x01 << 0;
						}
					}
				}
				// D-Pad
				if (SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)padbinds.up)) {
					dev->controlData[2] |= 0x01 << 4;
					dev->controlData[10] = 0xFF;
					//dev->controlData[7] = 0;
				}
				if (SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)padbinds.right)) {
					dev->controlData[2] |= 0x01 << 5;
					dev->controlData[8] = 0xFF;
					//dev->controlData[6] = 255;
				}
				if (SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)padbinds.down)) {
					dev->controlData[2] |= 0x01 << 6;
					dev->controlData[11] = 0xFF;
					//dev->controlData[7] = 255;
				}
				if (SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)padbinds.left)) {
					dev->controlData[2] |= 0x01 << 7;
					dev->controlData[9] = 0xFF;
					//dev->controlData[6] = 0;
				}
				// Sticks
				if (!flickit) {
					getStick(controller, padbinds.camera, &(dev->controlData[4]), &(dev->controlData[5]));
				}
				getStick(controller, padbinds.movement, &(dev->controlData[6]), &(dev->controlData[7]));

				// With Flickit the camera stick is the board: it never reaches the game as camera input
				if (flickit) {
					flickitGameFrame(dev, flickitOut);
					flickitApply(dev, flickitOut);
				}
			}
		}
		else if (!ParkEd) { // Only needed to be able to skip intro movies manually. The ParkEd instance isn't initialized yet when movies are displayed
			if (getButton(controller, padbinds.ollie)) {
				dev->controlData[2] |= 0x01 << 3;
			}
		}
	}

	// Menus, the park editor and pauses drop any half finished gesture
	if (!flickit && dev->port < MAX_PLAYERS) {
		flickitRecognizers[dev->port].Reset();
	}
}

void pollKeyboard(device* dev) {

	dev->isValid = 1;
	dev->isPluggedIn = 1;

	uint8_t* keyboardState = (uint8_t*)SDL_GetKeyboardState(NULL);

	// Add a bit of delay for certain key presses
	if (buffer > 0)
		buffer--;

	/********************************************************************/
	/*  STATIC MENU KEYS												*/
	/*																	*/
	/*																	*/
	/*  spinleft = rotate left(2)										*/
	/*  spinright = rotate right(1)										*/
	/*  Grind->R														*/
	/*  Flip->E															*/
	/*  caveman1->ß zoom out											*/
	/*  caveman2->´ zoom in												*/
	/*  Arrow keys for menu												*/
	/*  S/X->camera up down												*/
	/*  Y/C->camera left right											*/
	/*  Keybind for Ollie is disabled									*/
	/*																	*/
	/*  ParkEd singleton is initialized after polling begins			*/
	/*  When a keyboard is on screen, use m_keyinput instead			*/
	/********************************************************************/

	if (!*keyboard_on_screen && ParkEd) {

		// Setup for pause buffer
		isInMenu = ((*isMenu || *otherIsMenu) && !(ParkEd->m_state == EEditorState::vEDITING));

		if (shouldUseMenuControls()) {

			/********************************************************************/
			/*																	*/
			/*  Menu controls													*/
			/********************************************************************/

			if (keyboardState[SDL_SCANCODE_SPACE] && bSpace_released) {
				dev->controlData[2] |= 0x01 << 3;
				buffer = 20;
				bSpace_released = false;
			}
			if (keyboardState[SDL_SCANCODE_ESCAPE] && bEscape_released && !buffer) {
				// In menus, ESC simulates the grab button
				// The ParkEditor is detected as a menu so ESC doesn't work there when only checking for the menu flag
				// That's why the ParkEd state is checked as well
				dev->controlData[3] |= 0x01 << 5;
				bEscape_released = false;
				buffer = 20;
			}
			if (keyboardState[SDL_SCANCODE_BACKSPACE] && bBackspace_released) {
				dev->controlData[3] |= 0x01 << 5;
				bBackspace_released = false;
			}
			if (keyboardState[SDL_SCANCODE_RETURN] && bReturn_released) {
				dev->controlData[3] |= 0x01 << 6;
				bReturn_released = false;
			}
			if (keyboardState[SDL_SCANCODE_E]) {
				dev->controlData[3] |= 0x01 << 7;
			}
			if (keyboardState[SDL_SCANCODE_R]) {
				dev->controlData[3] |= 0x01 << 4;
			}
			if (keyboardState[SDL_SCANCODE_MINUS]) {
				dev->controlData[20] |= 0x01 << 0; // black
			}
			if (keyboardState[SDL_SCANCODE_EQUALS]) {
				dev->controlData[20] |= 0x01 << 1; // white
			}
			if (keyboardState[SDL_SCANCODE_S]) {
				dev->controlData[5] = 0;
			}
			if (keyboardState[SDL_SCANCODE_X]) {
				dev->controlData[5] = 255;
			}
			if (keyboardState[SDL_SCANCODE_Z]) {
				dev->controlData[4] = 0;
			}
			if (keyboardState[SDL_SCANCODE_C]) {
				dev->controlData[4] = 255;
			}
			if (keyboardState[SDL_SCANCODE_UP] || keyboardState[keybinds.up]) {
				dev->controlData[7] = 0;
			}
			if (keyboardState[SDL_SCANCODE_DOWN] || keyboardState[keybinds.down]) {
				dev->controlData[7] = 255;
			}
			if (keyboardState[SDL_SCANCODE_LEFT] || keyboardState[keybinds.left]) {
				dev->controlData[6] = 0;
			}
			if (keyboardState[SDL_SCANCODE_RIGHT] || keyboardState[keybinds.right]) {
				dev->controlData[6] = 255;
			}
			if (keyboardState[SDL_SCANCODE_1]) {
				dev->controlData[3] |= 0x01 << 2;
			}
			if (keyboardState[SDL_SCANCODE_2]) {
				dev->controlData[3] |= 0x01 << 3;
			}
		}
		else {
			if (ParkEd->m_state == EEditorState::vEDITING) {

				/******************************************************************/
				/*                                                                */
				/*  Gameplay (ParkEd editing) PC controls						  */
				/******************************************************************/

				if (keyboardState[SDL_SCANCODE_ESCAPE] && !buffer) {
					dev->controlData[2] |= 0x01 << 3;
					buffer = 20;
				}
				if (keyboardState[keybinds.grind]) {
					dev->controlData[3] |= 0x01 << 4;
				}
				if (keyboardState[keybinds.grab]) {
					dev->controlData[3] |= 0x01 << 5;
				}
				if (keyboardState[keybinds.ollie] && !buffer) {
					dev->controlData[3] |= 0x01 << 6;
					buffer = 20;
				}
				if (keyboardState[keybinds.kick]) {
					dev->controlData[3] |= 0x01 << 7;
				}
				if (inputsettings.isPs2Controls) {
					if (keyboardState[keybinds.rightSpin]) {
						dev->controlData[20] |= 0x01 << 0; // Raise
					}
					if (keyboardState[keybinds.leftSpin]) {
						dev->controlData[3] |= 0x01 << 2; // Lower
					}
					if (keyboardState[keybinds.nollie]) {
						dev->controlData[3] |= 0x01 << 0;
					}
					if (keyboardState[keybinds.switchRevert]) {
						dev->controlData[20] |= 0x01 << 1;
					}
				}
				else {
					if (keyboardState[keybinds.rightSpin]) {
						dev->controlData[3] |= 0x01 << 2; // Raise
					}
					if (keyboardState[keybinds.leftSpin]) {
						dev->controlData[3] |= 0x01 << 0; // Lower
					}
					if (keyboardState[keybinds.caveman]) { // white L1 swapped to act as black on kb
						dev->controlData[20] |= 0x01 << 0;
					}
					if (keyboardState[keybinds.caveman2]) { // black R1 swapped to act as white on kb
						 dev->controlData[20] |= 0x01 << 1;
					}
				}
			}
			else {

				/******************************************************************/
				/*                                                                */
				/*  Gameplay (in level) controls								  */
				/******************************************************************/

				if (keyboardState[SDL_SCANCODE_ESCAPE] && bEscape_released && !buffer) {
					dev->controlData[2] |= 0x01 << 3;
					bEscape_released = false;
					buffer = 20;
				}
				if (keyboardState[keybinds.cameraToggle]) {
					dev->controlData[2] |= 0x01 << 0;
				}
				if (keyboardState[keybinds.cameraSwivelLock]) {
					dev->controlData[2] |= 0x01 << 2;
				}
				if (keyboardState[keybinds.grind]) {
					dev->controlData[3] |= 0x01 << 4;
					dev->controlData[12] = 0xff;
				}
				if (keyboardState[keybinds.grab]) {
					dev->controlData[3] |= 0x01 << 5;
					dev->controlData[13] = 0xff;
				}
				if (keyboardState[keybinds.ollie]) {
					dev->controlData[3] |= 0x01 << 6;
					dev->controlData[14] = 0xff;
				}
				if (keyboardState[keybinds.kick]) {
					dev->controlData[3] |= 0x01 << 7;
					dev->controlData[15] = 0xff;
				}
				// Shoulders
				if (inputsettings.isPs2Controls) {
					if (keyboardState[keybinds.rightSpin]) {
						dev->controlData[3] |= 0x01 << 3;
						dev->controlData[17] = 0xff;
					}
					if (keyboardState[keybinds.switchRevert]) {
						dev->controlData[3] |= 0x01 << 1;
						dev->controlData[19] = 0xff;
					}
					if (keyboardState[keybinds.leftSpin]) {
						dev->controlData[3] |= 0x01 << 2;
						dev->controlData[16] = 0xff;
					}
					if (keyboardState[keybinds.nollie]) {
						dev->controlData[3] |= 0x01 << 0;
						dev->controlData[18] = 0xff;
					}
					if (keyboardState[keybinds.leftSpin] && keyboardState[keybinds.nollie]) {
						dev->controlData[20] |= 0x01 << 0;
					}
				}
				else {
					// Merge Switch/Revert + Right Spin for PC controls
					if (keyboardState[keybinds.rightSpin]) {
						// Right spin
						dev->controlData[3] |= 0x01 << 3;
						dev->controlData[17] = 0xff;
						// Revert
						dev->controlData[3] |= 0x01 << 1;
						dev->controlData[19] = 0xff;
					}
					// Merge Nollie + Left Spin for PC controls
					if (keyboardState[keybinds.leftSpin]) {
						// Spin left
						dev->controlData[3] |= 0x01 << 2;
						dev->controlData[16] = 0xff;
						// Nollie
						dev->controlData[3] |= 0x01 << 0;
						dev->controlData[18] = 0xff;
					}
					if (keyboardState[keybinds.caveman]) {
						dev->controlData[20] |= 0x01 << 1;
					}
					if (keyboardState[keybinds.caveman2]) {
						dev->controlData[20] |= 0x01 << 0;
					}
				}
			}
			// ParkEditor item control
			if (keyboardState[keybinds.item_up]) {
				dev->controlData[2] |= 0x01 << 4;
				dev->controlData[10] = 0xFF;
			}
			if (keyboardState[keybinds.item_right]) {
				dev->controlData[2] |= 0x01 << 5;
				dev->controlData[8] = 0xFF;
			}
			if (keyboardState[keybinds.item_down]) {
				dev->controlData[2] |= 0x01 << 6;
				dev->controlData[11] = 0xFF;
			}
			if (keyboardState[keybinds.item_left]) {
				dev->controlData[2] |= 0x01 << 7;
				dev->controlData[9] = 0xFF;
			}
			// Sticks - NOTE: Because these keys are very rarely used/important, SOCD handling is just to cancel
			// right
			// x
			if (keyboardState[keybinds.cameraLeft] && !keyboardState[keybinds.cameraRight]) {
				dev->controlData[4] = 0;
			}
			if (keyboardState[keybinds.cameraRight] && !keyboardState[keybinds.cameraLeft]) {
				dev->controlData[4] = 255;
			}
			// y
			if (keyboardState[keybinds.cameraUp] && !keyboardState[keybinds.cameraDown]) {
				dev->controlData[5] = 0;
			}
			if (keyboardState[keybinds.cameraDown] && !keyboardState[keybinds.cameraUp]) {
				dev->controlData[5] = 255;
			}
			// left
			// x
			if (keyboardState[keybinds.left] && !keyboardState[keybinds.right]) {
				dev->controlData[6] = 0;
			}
			if (keyboardState[keybinds.right] && !keyboardState[keybinds.left]) {
				dev->controlData[6] = 255;
			}
			// y
			if (keyboardState[keybinds.up] && !keyboardState[keybinds.down]) {
				dev->controlData[7] = 0;
			}
			if (keyboardState[keybinds.down] && !keyboardState[keybinds.up]) {
				dev->controlData[7] = 255;
			}
		}
	}
	else if (!ParkEd) { // Only needed to be able to skip intro movies manually. The ParkEd instance isn't initialized yet when movies are displayed
		if (keyboardState[keybinds.ollie] || keyboardState[SDL_SCANCODE_ESCAPE]) {
			dev->controlData[2] |= 0x01 << 3;
		}
	}
}

// EXTREME HACK: keep pause buffer behavior for speedrunning
// when transitioning from menu to game and pause is held, pulse off for one frame
// this only applies to controller at the moment, the bind switching makes it a bit too complex for keyboard while not breaking the CAS menu
// Thanks PARTYMANX
uint8_t prev_menu_button_pause_buffer = 0;

void doPauseBuffer(device* dev) {

	uint8_t buttonPressed = (dev->controlData[2] & (0x01 << 3)) != 0;

	if (isInMenu == 1 && buttonPressed && prev_menu_button_pause_buffer == 1) {
		dev->controlData[2] &= ~(0x01 << 3);
	}

	if (isInMenu && buttonPressed) {
		prev_menu_button_pause_buffer = 1;
	}
	else {
		prev_menu_button_pause_buffer = 0;
	}
}


/******************************************************************/
/*                                                                */
/* Text input													  */
/******************************************************************/

void do_key_input(SDL_KeyCode key) {
	if (!*keyboard_on_screen) {
		if (key == SDLK_RETURN) {
			m_keyinput(0x19, 0);
		}
		else if (key == SDLK_F1) {
			//0x1B already taken by ESC, lookup is patched to detect F1
			m_keyinput(0x1F, 0);
		}
		else if (key == SDLK_F2) {
			m_keyinput(0x1C, 0);
		}
		else if (key == SDLK_F3) {
			m_keyinput(0x1D, 0);
		}
		else if (key == SDLK_F4 && !(uint8_t*)SDL_GetKeyboardState(NULL)[0xE2]) {
			m_keyinput(0x1E, 0);
		}
		return;
	}

	int32_t key_out = 0;
	uint8_t modstate = SDL_GetModState();
	uint8_t shift = SDL_GetModState() & KMOD_SHIFT;
	uint8_t caps = SDL_GetModState() & KMOD_CAPS;

	if (key == SDLK_RETURN) {
		key_out = 0x0D;    // CR
	}
	else if (key == SDLK_BACKSPACE) {
		key_out = 0x08;    // BS
	}
	else if (key == SDLK_ESCAPE && bEscape_released) {
		bEscape_released = false;
		buffer = 20;
		key_out = 0x1B;    // ESC
	}
	else if (key == SDLK_KP_ENTER) {
		key_out = 0x0D;
	}
	else {
		key_out = -1;
	}
	if (key_out != -1) {
		m_keyinput(key_out, 0);
	}
}

void do_text_input(char* text) {

	m_keyinput(text[0], 0);
	/*
	if (strlen(text) == 1) {
		m_keyinput(text[0], 0);
	}
	else {
		Log::TypedLog(CHN_SDL, "Input text '%s' > 1 byte!!\n");
	}
	*/
}


/******************************************************************/
/*                                                                */
/* Event processing												  */
/******************************************************************/

void handleInputEvent(SDL_Event* e) {
	switch (e->type) {
	case SDL_CONTROLLERDEVICEADDED:
		if (SDL_IsGameController(e->cdevice.which)) {
			Log::TypedLog(CHN_SDL, "Adding controller: %d\n", e->cdevice.which);
			addController(e->cdevice.which);
		}
		else {
			Log::TypedLog(CHN_SDL, "Not a game controller: %s\n", SDL_JoystickNameForIndex(e->cdevice.which));
		}
		return;
	case SDL_CONTROLLERDEVICEREMOVED: {
		SDL_GameController* controller = SDL_GameControllerFromInstanceID(e->cdevice.which);
		if (controller) {
			Log::TypedLog(CHN_SDL, "Removed controller\n");
			removeController(controller);
		}
		return;
	}
	case SDL_JOYDEVICEADDED:
		Log::TypedLog(CHN_SDL, "Joystick added: %s\n", SDL_JoystickNameForIndex(e->jdevice.which));
		return;
	case SDL_KEYDOWN:
		// Re-read the [Flickit] section of the ini while the game is running
		if (flickitSettings.reloadKey > 0 && (int)e->key.keysym.scancode == flickitSettings.reloadKey && !e->key.repeat) {
			loadFlickitSettings();
		}
		setUsingKeyboard(1);
		do_key_input((SDL_KeyCode)e->key.keysym.sym);
		return;
	case SDL_KEYUP:
		if (e->key.keysym.scancode == SDL_SCANCODE_SPACE && !bSpace_released)
			bSpace_released = true;
		else if (e->key.keysym.scancode == SDL_SCANCODE_BACKSPACE && !bBackspace_released)
			bBackspace_released = true;
		else if (e->key.keysym.scancode == SDL_SCANCODE_ESCAPE && !bEscape_released)
			bEscape_released = true;
		else if (e->key.keysym.scancode == SDL_SCANCODE_RETURN && !bReturn_released)
			bReturn_released = true;
		return;
	case SDL_CONTROLLERBUTTONDOWN: {
		SDL_GameController* controller = SDL_GameControllerFromInstanceID(e->cdevice.which);

		int idx = SDL_GameControllerGetPlayerIndex(controller);
		if (idx == -1) {
			addplayer(controller);
		}
		else if (players[idx].lockedOut) {
			players[idx].lockedOut++;
		}
		setUsingKeyboard(0);
		return;
	}
	case SDL_CONTROLLERBUTTONUP: {
		SDL_GameController* controller = SDL_GameControllerFromInstanceID(e->cdevice.which);

		int idx = SDL_GameControllerGetPlayerIndex(controller);
		if (idx != -1 && players[idx].lockedOut) {
			players[idx].lockedOut--;
		}
		return;
	}
	case SDL_CONTROLLERAXISMOTION:
		setUsingKeyboard(0);
		return;
	case SDL_TEXTINPUT:
		do_text_input(e->text.text);
		return;
	default:
		return;
	}
}

void handleQuitEvent(SDL_Event* e) {
	switch (e->type) {
	case SDL_QUIT: {
		*shouldQuit = 1;
		return;
	}
	default:
		return;
	}
}

void fastExit() {
	if (inputsettings.savewindowposition)
		dumpWindowPosition();
	exit(0);
}

void __cdecl processController(device* dev) {
	// Cheating: replace type with index
	//dev->type = 70;

	dev->capabilities = 0x0003;
	dev->num_actuators = 2;
	dev->vibrationData_max[0] = 255;
	dev->vibrationData_max[1] = 255;
	dev->state = 2;
	//dev->actuatorsDisabled = 0;

	handleEvents();
	
	dev->isValid = 0;
	dev->isPluggedIn = 0;

	dev->controlData[0] = 0;
	dev->controlData[1] = 0x70;

	// Buttons bitmap
	// This bitmap may not work how you would expect. each bit is 1 if the button is *up*, not down
	// The original code sets this initial value at 0xff and XOR's each button bit with the map
	// This scheme cannot be continuously composited, so instead we OR each button with the bitmap and bitwise negate it after all controllers are processed
	dev->controlData[2] = 0x00;
	dev->controlData[3] = 0x00;

	// Buttons
	dev->controlData[12] = 0;
	dev->controlData[13] = 0;
	dev->controlData[14] = 0;
	dev->controlData[15] = 0;

	// shoulders
	dev->controlData[16] = 0;
	dev->controlData[17] = 0;
	dev->controlData[18] = 0;
	dev->controlData[19] = 0;

	// d-pad
	dev->controlData[8] = 0;
	dev->controlData[9] = 0;
	dev->controlData[10] = 0;
	dev->controlData[11] = 0;

	// sticks
	dev->controlData[4] = 127;
	dev->controlData[5] = 127;
	dev->controlData[6] = 127;
	dev->controlData[7] = 127;

	dev->controlData[20] = 0;

	if (dev->port == 0) {
		dev->isValid = 1;
		dev->isPluggedIn = 1;

		// The game's rail code does nothing for Flickit on this frame unless the controller poll below says so
		FlickitGame::BeginFrame();

		if (!*keyboard_on_screen) {
			pollKeyboard(dev);
		}
	}
	if (dev->port < MAX_PLAYERS) {
		if (players[dev->port].controller && !players[dev->port].lockedOut) {
			pollController(dev, players[dev->port].controller);
		}
	}

	if (dev->port == 0) {
		doPauseBuffer(dev);
	}

	dev->controlData[2] = ~dev->controlData[2];
	dev->controlData[3] = ~dev->controlData[3];

	if (0xFFFF ^ ((dev->controlData[2] << 8) | dev->controlData[3])) {
		dev->pressed = 1;
	}
	else {
		dev->pressed = 0;
	}
	if (~dev->controlData[2] & 0x01 << 3 || ~dev->controlData[3] & 0x01 << 6) {
		dev->start_or_a_pressed = 1;
	}
	else {
		dev->start_or_a_pressed = 0;
	}
}


/******************************************************************/
/*                                                                */
/* Initialization												  */
/******************************************************************/

void __stdcall initManager() {
	GetModuleFileName(NULL, (LPSTR)&executableDirectory3, MAX_PATH);

	// Find last slash
	char* exe = strrchr((LPSTR)executableDirectory3, '\\');
	if (exe) {
		*(exe + 1) = '\0';
	}

	// Init SDL here
	SDL_Init(SDL_INIT_GAMECONTROLLER);
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "false");

	char controllerDbPath[MAX_PATH];
	int result = sprintf_s(controllerDbPath, MAX_PATH, "%s%s", executableDirectory3, "gamecontrollerdb.txt");

	if (result) {
		result = SDL_GameControllerAddMappingsFromFile(controllerDbPath);
		if (result) {
			Log::TypedLog(CHN_SDL, "Loaded mappings\n");
		}
		else {
			Log::TypedLog(CHN_SDL, "Failed to load %s\n", controllerDbPath);
		}

	}
	loadSettings(&inputsettings);
	loadKeyBinds(&keybinds);
	loadControllerBinds(&padbinds);
	loadFlickitSettings();

	initSDLControllers();

	registerEventHandler(handleInputEvent);

	// Fast exit
	registerEventHandler(handleQuitEvent);
	patchCall((void*)0x004BFF1E, fastExit);

	if (inputsettings.isPs2Controls) {
		patchPs2Buttons();
	}
	
	// Add missing key info: Since we don't use the launcher, no registry values for our keybinds are set.
	// The game normally loads keybinds found in the registry and stores them at these addresses (starting at 0x007D1160).
	// These patched values simulate the launcher's behavior of storing its defined keybinds in memory so that they can be displayed in game 
	// Without this, no user-defined keybinds would be displayed for things like the edit tricks menu (e.g. "<- + KP4" would only be "<- +   ")
	patchDWord((void*)0x007D1160, convert_SDL_to_OIS_keycode(keybinds.ollie));
	patchDWord((void*)0x007D1164, convert_SDL_to_OIS_keycode(keybinds.grab));
	patchDWord((void*)0x007D1168, convert_SDL_to_OIS_keycode(keybinds.kick));
	patchDWord((void*)0x007D116C, convert_SDL_to_OIS_keycode(keybinds.grind));
	patchDWord((void*)0x007D1180, convert_SDL_to_OIS_keycode(keybinds.leftSpin));
	patchDWord((void*)0x007D1184, convert_SDL_to_OIS_keycode(keybinds.rightSpin));
	patchDWord((void*)0x007D1178, convert_SDL_to_OIS_keycode(keybinds.caveman));
	patchDWord((void*)0x007D117C, convert_SDL_to_OIS_keycode(keybinds.caveman2));
}

void patchPs2Buttons() {
	Log::TypedLog(CHN_SDL, "Patching PS2 Buttons\n");
	patchByte((void*)(0x00452226 + 2), 0x05);	// Change PC platform to gamecube. This just makes it default to ps2 controls. Needed for rail DD on R2

	// walk acid drop.
	// R2 on PS2
	patchBytesM((void*)0x00463773, (BYTE*)"\x0F\x85\x09\x00\x00\x00", 6);

	//in_air_acid_drop
	patchBytesM((void*)0x0056CAA8, (BYTE*)"\x75\x09", 2);

	//in_air_to_break
	patchBytesM((void*)0x0056C00D, (BYTE*)"\x75\x09", 2);

	//r2l2_air_recover
	patchBytesM((void*)0x0056BB6F, (BYTE*)"\x75\x07", 2);

	//??
	patchNop((void*)0x0046376C, 13);

	//??
	patchNop((void*)0x0046383C, 13);

	//r2l2_break_vert1
	patchBytesM((void*)0x00565488, (BYTE*)"\x0F\x85\x0D\x00\x00\x00", 6);

	//r2l2_break_vert2
	patchBytesM((void*)0x005654A2, (BYTE*)"\x0F\x85\x0D\x00\x00\x00", 6);
}

void initializeInstance() {

	if (!instances_initialized) {
		ParkEd = (EdCParkEditorInstance*)*(uint32_t*)(0x007C992C);
		Skate = (SkateInstance*)*(uint32_t*)(0x0076A788);
		//GoalMan = GameGetGoalManager_Native(); //Points to start of GoalMan. Not needed (yet)

		if (ParkEd && Skate)
			instances_initialized = true;
	}
	else {
		// Gross? Callee cleans up
		patchCall((void*)0x00408123, (void*)0x004A4F00);
	}
}

void patchInput() {

	// Patch SIO::Device
	// Process
	patchThisToCdecl((void*)0x00581A20, &processController);
	patchBytesM((void*)(0x00581A20 + 7), (BYTE*)"\xC2\x04\x00", 3); //ret 4

	// set_actuator
	// Don't call read_data in activate_actuators
	patchNop((void*)0x00581730, 7); //call_device_read
	patchJump((void*)0x005C4200, set_actuators);

	// Initialize ParkEdInstance handle here
	patchCall((void*)0x00408123, initializeInstance);

	// Init input patch - nop direct input setup
	patchNop((void*)0x005C56AB, 47);
	patchCall((void*)(0x005C56AB + 5), &initManager);

	// Some config call relating to the dinput devices
	patchNop((void*)0x004C084D, 5);

	// Patch F1 button for taunt
	patchByte((void*)(0x004C82E5+1), 0x1F);

	// Init events
	initEvents();
	patchJump((void*)0x005C30E0, handleEvents);
}