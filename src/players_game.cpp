// Flickit's players in the game (see players_game.h).
#include <windows.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include <SDL.h>

#include "players.h"
#include "players_game.h"
#include "flickit_players_qb.h"
#include "bigworld.h"
#include "drive_game.h"
#include "flickit.h"
#include "flickit_game.h"
#include "watchdog.h"
#include "worldmap_game.h"
#include "QB/LazyStruct.h"
#include "Logger/Logger.h"

#define FLICKIT_SECTION "Flickit"

namespace PlayersGame {
	/******************************************************************/
	/*                                                                */
	/*  The game's own functions                                      */
	/*                                                                */
	/******************************************************************/

	// Obj::ResolveToObject: an object by its name, or 0
	typedef void* (__cdecl* ResolveToObject_t)(uint32_t id);
	static const ResolveToObject_t ResolveToObject = (ResolveToObject_t)0x0041E750;
	// A component of an object by the checksum of its type, or 0
	typedef void* (__thiscall* GetComponent_t)(void* object, uint32_t type);
	static const GetComponent_t GetComponent = (GetComponent_t)0x0041BDC0;
	// A global array by its name (the level's NodeArray), or 0
	typedef void* (__cdecl* GetGlobalArray_t)(uint32_t name, int assertIfMissing);
	static const GetGlobalArray_t GetGlobalArray = (GetGlobalArray_t)0x00413650;
	// A symbol by its name: its type at +1 (7 a script)
	typedef uint8_t* (__cdecl* LookUpSymbol_t)(uint32_t name);
	static const LookUpSymbol_t LookUpSymbol = (LookUpSymbol_t)0x004133A0;
	// Script::CStruct::AddVector
	typedef void(__thiscall* StructAddVector_t)(void* s, uint32_t name, float x, float y, float z);
	static const StructAddVector_t StructAddVector = (StructAddVector_t)0x004124D0;
	// Script::ParseQB for a file in memory; Script::RunScript (the name of a script, its parameters, its object)
	typedef void(__cdecl* ParseQBFromMemory_t)(const char* name, uint8_t* data, int flag);
	static const ParseQBFromMemory_t ParseQBFromMemory = (ParseQBFromMemory_t)0x00409F90;
	typedef bool(__cdecl* RunScript_t)(uint32_t name, void* params, void* object, void* object2);
	static const RunScript_t RunGameScript = (RunScript_t)0x0040FD00;
	// GameNet::Manager::InNetGame
	typedef bool(__thiscall* InNetGame_t)(void* net);
	static const InNetGame_t InNetGame = (InNetGame_t)0x004C6B50;
	static void** const kNetManager = (void**)0x0076914C;
	// CAnimationComponent: start an animation, what PlayAnim does once it has read its parameters (the name,
	// whether to tell a net game, from and to in seconds, how it loops, the blend's time, its speed); and
	// whether the object has an animation of that name. FlickitGame uses both on the skater.
	typedef void(__thiscall* PlayAnimCore_t)(void* anim, uint32_t name, int net, float from, float to, int loop, float blend, float speed);
	static const PlayAnimCore_t PlayAnimCore = (PlayAnimCore_t)0x004320E0;
	typedef bool(__thiscall* AnimExists_t)(void* anim, uint32_t name);
	static const AnimExists_t AnimExists = (AnimExists_t)0x00430650;

	enum {
		// an object: where it is, its matrix (left, up, the way it faces, a fourth row), the one it is drawn with
		OBJECT_POSITION = 0x40,
		OBJECT_MATRIX = 0x60,
		OBJECT_DISPLAY_MATRIX = 0xB0,
		// its flags (bit 0: dead) and a composite object's (bit 0: not updated, 1: paused, 2: suspended)
		OBJECT_FLAGS = 0x08,
		COMPOSITE_FLAGS = 0xF4,
		// CAnimationComponent: its first blend channel (the animation it is in; with none, the head of the
		// list itself), its flags (Obj_Flip turns bit 29 over). Bit 29 mirrors the animations started after
		// it is set: PlayAnim gives it to the new channel (0x004322CE), which keeps it; the channels before
		// are as they were.
		ANIM_FIRST = 0x24,
		ANIM_FLAGS = 0x3C,
		// a blend channel, as PlaySequence (0x00470440) sets it: from and to in seconds, the earlier first,
		// where it is, its speed a 60th, backwards (-1) or not (0), how it loops, its animation's name
		CHANNEL_SELF = 0x04,
		CHANNEL_LOW = 0x14,
		CHANNEL_TIME = 0x18,
		CHANNEL_HIGH = 0x1C,
		CHANNEL_SPEED = 0x20,
		CHANNEL_BACKWARDS = 0x24,
		CHANNEL_LOOP = 0x28,
		CHANNEL_NAME = 0x30,
	};
	static const uint32_t ANIM_FLIPPED = 1u << 29;
	enum { SYMBOL_SCRIPT = 7 };

	struct Expect {
		uint32_t address;
		int size;
		uint8_t bytes[17];
		const char* what;
	};
	static const Expect kExpect[] = {
		{ 0x0041E750, 12, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x0D, 0xC0, 0xB6, 0x69, 0x00, 0x50, 0xE8 }, "Obj::ResolveToObject" },
		{ 0x0041BDC0, 12, { 0x8B, 0x81, 0xF0, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x12, 0x8B, 0x4C }, "CCompositeObject::GetComponent" },
		{ 0x00413650, 12, { 0x8B, 0x44, 0x24, 0x04, 0x50, 0xE8, 0x46, 0xFD, 0xFF, 0xFF, 0x83, 0xC4 }, "Script::GetArray" },
		{ 0x004133A0, 12, { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0xC1, 0x25, 0xFF, 0x0F, 0x00, 0x00, 0x56 }, "Script::LookUpSymbol" },
		{ 0x004124D0, 12, { 0x6A, 0xFF, 0x68, 0xD6, 0x19, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "CStruct::AddVector" },
		{ 0x00409F90, 13, { 0x8B, 0x44, 0x24, 0x0C, 0x8B, 0x4C, 0x24, 0x08, 0x8B, 0x54, 0x24, 0x04, 0x6A }, "Script::ParseQB, from memory" },
		{ 0x0040FD00, 12, { 0x6A, 0xFF, 0x68, 0x5B, 0x18, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "Script::RunScript" },
		{ 0x004C6B50, 12, { 0x56, 0x8B, 0xF1, 0xF6, 0x86, 0x38, 0x01, 0x00, 0x00, 0x0C, 0x74, 0x24 }, "GameNet::Manager::InNetGame" },
		{ 0x004320E0, 12, { 0x81, 0xEC, 0x4C, 0x05, 0x00, 0x00, 0xA1, 0xB0, 0x4A, 0x69, 0x00, 0x33 }, "PlayAnim's start of an animation" },
		{ 0x00430650, 15, { 0x8B, 0x41, 0x38, 0x03, 0x44, 0x24, 0x04, 0x6A, 0x00, 0x50, 0xE8, 0x11, 0x7F, 0x04, 0x00 }, "animation exists" },
		{ 0x00430E90, 12, { 0x8B, 0x4E, 0x3C, 0x8B, 0x40, 0x08, 0x8B, 0x56, 0x0C, 0xC1, 0xE9, 0x1D }, "Obj_Flip: bit 29 of the animation component's flags" },
		{ 0x00430EC1, 7, { 0x8B, 0x71, 0x24, 0x83, 0x7E, 0x04, 0xFF }, "the animation component's first blend channel" },
		{ 0x004322CE, 11, { 0x8B, 0x46, 0x3C, 0x8B, 0x11, 0x8B, 0x52, 0x18, 0xC1, 0xE8, 0x1D }, "PlayAnim: bit 29 to the new channel" },
		{ 0x00470463, 6, { 0xD9, 0x59, 0x14, 0xD9, 0x51, 0x18 }, "the blend channel: from and where it is" },
		{ 0x00470484, 3, { 0xD9, 0x59, 0x1C }, "the blend channel: to" },
		{ 0x004704A8, 17, { 0x89, 0x41, 0x24, 0x8B, 0x44, 0x24, 0x10, 0x89, 0x41, 0x28, 0xC6, 0x41, 0x2C, 0x00, 0x89, 0x51, 0x30 }, "the blend channel: backwards, its loop, its name" },
		{ 0x00470410, 10, { 0x51, 0xD9, 0x41, 0x20, 0xDC, 0x0D, 0xA8, 0xEE, 0x62, 0x00 }, "the blend channel: its speed" },
	};

	/******************************************************************/
	/*                                                                */
	/*  State                                                         */
	/*                                                                */
	/******************************************************************/

	enum { kMaxPlayers = 8 };

	struct Names {
		uint32_t nodeArray, animation;
		uint32_t spawnScript, killScript, labelAtScript, labelScript, labelPickScript, promptScript, promptOffScript, sayScript, wordsScript, recScript, recOffScript;
		uint32_t id, label, name, pos, show, picked, text, time;
	};
	static Names s_n;

	static const uint32_t kMinRunMs = 3000;          // a run shorter than this is not kept
	static const uint32_t kFindMs = 3000;            // a player made is looked for this long, then made again
	static const int kMostMisses = 3;                // and given up on after this many that were not found
	static const uint32_t kRemakeMs = 2000;          // a player gone (a restart of the level's objects) comes back after this
	static const int kMostMakes = 20;                // and no more often than this in one level
	static const float kLabelUp = 108.0f;            // his name this high over his feet
	static const uint32_t kPromptLingerMs = 400;
	static const float kBlend = 0.15f;               // seconds: a new animation of his blends in over this
	static const float kLabelFar = 1500.0f;          // a player farther than this has no name on the screen
	static const float kBody = 20.0f;                // how far, for the cars: from the edge of him
	static const float kNone = 1.0e9f;
	static const size_t kNoFrame = (size_t)-1;

	static bool s_on = true;                 // [Flickit] FakePlayers
	// [Flickit] PlayerReach (anywhere around him, from the player's edge), PlayerAimReach (in front, where the
	// camera looks), PlayerLockMs (the one picked stays picked this long once he has gone past)
	static Players::AimSettings s_aimSet = { 150.0f, 600.0f, 0.7f, 300.0f, 20.0f, 4000 };
	static int s_runKey = 63;                // [Flickit] RunKey (SDL_SCANCODE_F6)
	static int s_ready = 0;                  // 1: the game is as expected, -1: not
	static int s_parsed = 0;                 // 1: the scripts are the game's, -1: they could not be
	static char s_dir[MAX_PATH] = "";        // flickit_runs\ next to the exe

	struct LevelKey {
		uint32_t loads;
		const void* nodes;
		uint32_t count;
		uint32_t level;
		bool operator!=(const LevelKey& o) const {
			return loads != o.loads || nodes != o.nodes || count != o.count || level != o.level;
		}
	};
	static LevelKey s_level = { 0, 0, 0, 0 };
	static uint8_t** const kSkate = (uint8_t**)0x0076A788;
	static const uint32_t SKATE_LEVEL = 0x398;

	// The recording
	static bool s_keyAsked = false;          // the key was pressed: start or stop on the next frame
	static bool s_recording = false;
	static Players::Run s_rec;
	static uint32_t s_recMs = 0;             // ms of the game since it started (its frames, not the pauses)
	static int s_recShown = -1;              // the seconds REC shows
	static const void* s_recChannel = 0;     // the skater's first blend channel on the frame before, its animation,
	static uint32_t s_recAnim = 0;           // and whether it was mirrored when it started (bit 29 then)
	static bool s_recFlipped = false;

	// The players
	enum { kNotMade, kSpawned, kLive, kGaveUp };
	struct Player {
		bool used;
		std::string file;                    // the run's file, in flickit_runs
		Players::Run run;
		Players::Identity who;
		int state;                           // kNotMade, kSpawned (his script is to make him), kLive, kGaveUp
		uint32_t id;                         // the name of his object (another each time he is made)
		uint32_t label;                      // the name of his label
		uint32_t made;                       // how many times he was made in this level
		uint32_t madeAt;                     // GetTickCount() then
		int misses;                          // times he was made and not found
		uint32_t liveAt;                     // when he was found
		bool reported;                       // how he is, written down once a while after he was found
		int plays, noAnims;                  // animations started on him, and ones he has not
		uint32_t t;                          // ms into his run
		size_t applied;                      // the frame of the run whose animation he plays (kNoFrame: none yet)
		bool labelShown;
		bool missingSaid;                    // an animation he has not was told of
		float pos[3];
	};
	static Player s_players[kMaxPlayers];
	static Players::AimState s_aim = { -1, 0 };
	static int s_target = -1;                // who Circle is for on this frame (-1: nobody)
	static float s_targetAway = kNone;       // how far he is, from his edge
	static int s_lit = -1;                   // whose name is lit (the one picked)
	static uint32_t s_litLabel = 0;
	static float s_skater[3] = { 0, 0, 0 };  // where the skater is, on this frame
	static float s_lookX = 0.0f, s_lookZ = 0.0f;     // where the camera looks along the ground (a unit; 0: not known)
	static float s_rightX = 0.0f, s_rightZ = 0.0f;   // and its right
	static int s_promptFor = -1;             // the player the prompt is up for
	static uint32_t s_promptSeen = 0;
	static bool s_down = false;              // Circle on this frame, before the cars (his own press)
	static bool s_downWas = false;
	static bool s_released = true;           // let go since it opened a card
	static uint32_t s_names = 0;             // names given to players and labels: each one new
	static LARGE_INTEGER s_lastFrame;        // zero: no frame yet

	/******************************************************************/
	/*                                                                */
	/*  Checks                                                        */
	/*                                                                */
	/******************************************************************/

	static void InitNames() {
		s_n.nodeArray = Flickit::NameChecksum("NodeArray");
		s_n.animation = Flickit::NameChecksum("Animation");
		s_n.spawnScript = Flickit::NameChecksum("Flickit_PlayerSpawn");
		s_n.killScript = Flickit::NameChecksum("Flickit_PlayerKill");
		s_n.labelAtScript = Flickit::NameChecksum("Flickit_PlayerLabelAt");
		s_n.labelScript = Flickit::NameChecksum("Flickit_PlayerLabelShow");
		s_n.labelPickScript = Flickit::NameChecksum("Flickit_PlayerLabelPick");
		s_n.promptScript = Flickit::NameChecksum("Flickit_PlayerPrompt");
		s_n.promptOffScript = Flickit::NameChecksum("Flickit_PlayerPromptOff");
		s_n.sayScript = Flickit::NameChecksum("Flickit_PlayersSay");
		s_n.wordsScript = Flickit::NameChecksum("Flickit_PlayerCardWords");
		s_n.recScript = Flickit::NameChecksum("Flickit_RunRec");
		s_n.recOffScript = Flickit::NameChecksum("Flickit_RunRecOff");
		s_n.id = Flickit::NameChecksum("id");
		s_n.label = Flickit::NameChecksum("label");
		s_n.name = Flickit::NameChecksum("name");
		s_n.pos = Flickit::NameChecksum("pos");
		s_n.show = Flickit::NameChecksum("show");
		s_n.picked = Flickit::NameChecksum("picked");
		s_n.text = Flickit::NameChecksum("text");
		s_n.time = Flickit::NameChecksum("time");
	}

	static bool Ready() {
		if (s_ready == 0) {
			s_ready = 1;
			for (size_t i = 0; i < sizeof(kExpect) / sizeof(kExpect[0]); i++) {
				if (memcmp((const void*)(uintptr_t)kExpect[i].address, kExpect[i].bytes, (size_t)kExpect[i].size) != 0) {
					Log::TypedLog(CHN_SDL, "Players: the game's code at %08X (%s) is not what was expected; the players are off\n",
						kExpect[i].address, kExpect[i].what);
					s_ready = -1;
				}
			}
			InitNames();
			if (s_n.animation != 0x72AD7B23 || s_n.nodeArray != 0xC472ECC5) {
				Log::TypedLog(CHN_SDL, "Players: the checksums of names are not the game's; the players are off\n");
				s_ready = -1;
			}
		}
		return s_ready > 0;
	}

	// The scripts of players.q, given to the game once
	static bool Parsed() {
		if (s_parsed == 0) {
			static uint8_t data[sizeof(flickit_players_qb)];
			memcpy(data, flickit_players_qb, sizeof(data));
			ParseQBFromMemory("scripts\\flickit\\players.qb", data, 1);
			const uint8_t* make = LookUpSymbol(s_n.spawnScript);
			const uint8_t* rec = LookUpSymbol(s_n.recScript);
			if (make && make[1] == SYMBOL_SCRIPT && rec && rec[1] == SYMBOL_SCRIPT) {
				s_parsed = 1;
				Log::TypedLog(CHN_SDL, "Players: on; %s records a run, each run of the level is a player (the last %d); Circle shows the card of the one he looks at within %.0f, or of one within %.0f of him (picked for %u ms after); the game has Flickit's player scripts (%u bytes)\n",
					s_runKey > 0 ? SDL_GetScancodeName((SDL_Scancode)s_runKey) : "no key", kMaxPlayers, s_aimSet.aimReach, s_aimSet.reach,
					s_aimSet.lockMs, (unsigned)sizeof(data));
			}
			else {
				s_parsed = -1;
				Log::TypedLog(CHN_SDL, "Players: the game did not take Flickit's player scripts; the players are off\n");
			}
		}
		return s_parsed > 0;
	}

	static bool InNet() {
		void* net = *kNetManager;
		return net && InNetGame(net);
	}

	// ms of the game since the frame before, at most a tenth of a second (a load, a hitch)
	static uint32_t FrameMs() {
		LARGE_INTEGER now, freq;
		QueryPerformanceCounter(&now);
		QueryPerformanceFrequency(&freq);
		double ms = 0.0;
		if (s_lastFrame.QuadPart && freq.QuadPart) {
			ms = (double)(now.QuadPart - s_lastFrame.QuadPart) * 1000.0 / (double)freq.QuadPart;
		}
		s_lastFrame = now;
		return ms < 0.0 ? 0 : (ms > 100.0 ? 100 : (uint32_t)(ms + 0.5));
	}

	// A line for the log and for flickit_watchdog.txt (which is written whatever the log settings)
	static void Note(const char* format, ...) {
		char text[320];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		text[sizeof(text) - 1] = 0;
		Log::TypedLog(CHN_SDL, "%s\n", text);
		FlickitGame::WatchNote(text);
	}

	static const char* KeyName() {
		const char* k = s_runKey > 0 ? SDL_GetScancodeName((SDL_Scancode)s_runKey) : "";
		return k && k[0] ? k : "the run key";
	}

	/******************************************************************/
	/*                                                                */
	/*  Words on the screen                                           */
	/*                                                                */
	/******************************************************************/

	static void RunWithText(uint32_t script, const char* text, int ms) {
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (!p) {
			return;
		}
		p->AddString(s_n.text, (char*)text);
		if (ms > 0) {
			p->AddInteger(s_n.time, ms);
		}
		RunGameScript(script, p, 0, 0);
		Script::LazyStruct::s_free(p);
	}

	static void Say(const char* text, int ms = 3000) {
		RunWithText(s_n.sayScript, text, ms);
	}

	static void PromptOff() {
		if (s_promptFor >= 0) {
			RunGameScript(s_n.promptOffScript, 0, 0, 0);
		}
		s_promptFor = -1;
	}

	static void Prompt(int i) {
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (!p) {
			return;
		}
		p->AddString(s_n.name, s_players[i].who.name);
		RunGameScript(s_n.promptScript, p, 0, 0);
		Script::LazyStruct::s_free(p);
		s_promptFor = i;
	}

	static void Clock(uint32_t ms, char* out, size_t size) {
		const uint32_t s = ms / 1000;
		sprintf_s(out, size, "%u:%02u", s / 60, s % 60);
	}

	// The card as words, where Flickit's own cannot be drawn
	static void CardWords(const Players::Identity& p) {
		char sponsors[96];
		if (p.sponsor == Players::kUnsponsored) {
			strcpy_s(sponsors, sizeof(sponsors), "No sponsors yet");
		}
		else {
			sprintf_s(sponsors, sizeof(sponsors), "%s: %s, %s", Players::SponsorName(p.sponsor), p.board, p.shoes);
		}
		const int* s = p.stats;
		char text[512];
		sprintf_s(text, sizeof(text), "%s\\n%s\\nStory: %s, %d goals\\nAir %d  Run %d  Ollie %d  Speed %d  Spin %d\\nSwitch %d  Flip %d  Rail %d  Lip %d  Manual %d",
			p.name, sponsors, p.level, p.goals, s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7], s[8], s[9]);
		RunWithText(s_n.wordsScript, text, 0);
	}

	/******************************************************************/
	/*                                                                */
	/*  The runs' files                                               */
	/*                                                                */
	/******************************************************************/

	static bool ReadWhole(const std::string& path, std::vector<uint8_t>* out) {
		HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (f == INVALID_HANDLE_VALUE) {
			return false;
		}
		LARGE_INTEGER size;
		bool ok = GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart <= (LONGLONG)(16 + Players::kMaxFrames * Players::kFrameBytes);
		if (ok) {
			out->resize((size_t)size.QuadPart);
			DWORD got = 0;
			ok = ReadFile(f, &(*out)[0], (DWORD)size.QuadPart, &got, NULL) && got == (DWORD)size.QuadPart;
		}
		CloseHandle(f);
		return ok;
	}

	static bool WriteWhole(const std::string& path, const std::vector<uint8_t>& data) {
		HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if (f == INVALID_HANDLE_VALUE) {
			return false;
		}
		DWORD put = 0;
		const bool ok = WriteFile(f, &data[0], (DWORD)data.size(), &put, NULL) && put == (DWORD)data.size();
		CloseHandle(f);
		if (!ok) {
			DeleteFileA(path.c_str());
		}
		return ok;
	}

	/******************************************************************/
	/*                                                                */
	/*  The players                                                   */
	/*                                                                */
	/******************************************************************/

	// A name of its own for each player made and each label: a name is never used twice, so a player going
	// (the game takes an object away at the end of the frame) is never his successor
	static uint32_t NewName(const char* kind, int i) {
		char name[48];
		sprintf_s(name, sizeof(name), "Flickit_%s%d_%u", kind, i, ++s_names);
		return Flickit::NameChecksum(name);
	}

	// His object and his label away (whether they are there or not: the script looks)
	static void Unmake(Player& p) {
		if (s_lit >= 0 && &s_players[s_lit] == &p) {
			s_lit = -1;
		}
		if (p.state == kNotMade || p.state == kGaveUp || !p.id) {
			return;
		}
		Script::LazyStruct* params = Script::LazyStruct::s_create();
		if (params) {
			params->AddChecksum(s_n.id, p.id);
			params->AddChecksum(s_n.label, p.label);
			RunGameScript(s_n.killScript, params, 0, 0);
			Script::LazyStruct::s_free(params);
		}
		p.state = kNotMade;
	}

	static void Kill(Player& p) {
		if (p.used) {
			Unmake(p);
		}
		if (s_aim.target >= 0 && &s_players[s_aim.target] == &p) {
			s_aim.target = -1;
		}
		p.used = false;
		p.run.frames.clear();
	}

	static void KillAll() {
		PromptOff();
		for (int i = 0; i < kMaxPlayers; i++) {
			Kill(s_players[i]);
		}
		s_target = -1;
		s_targetAway = kNone;
		s_aim.target = -1;
	}

	static void Use(int i, const std::string& file, const Players::Run& run) {
		Player& p = s_players[i];
		p.used = true;
		p.file = file;
		p.run = run;
		// made up from the run's file, another if a player here has that name already
		const uint32_t seed = Players::SeedOf(file.c_str());
		for (uint32_t k = 0; k < 32; k++) {
			p.who = Players::FakeIdentity(seed + k);
			bool taken = false;
			for (int j = 0; j < kMaxPlayers; j++) {
				taken = taken || (j != i && s_players[j].used && strcmp(s_players[j].who.name, p.who.name) == 0);
			}
			if (!taken) {
				break;
			}
		}
		p.state = kNotMade;
		p.id = 0;
		p.label = 0;
		p.made = 0;
		p.madeAt = 0;
		p.misses = 0;
		p.liveAt = 0;
		p.reported = false;
		p.plays = 0;
		p.noAnims = 0;
		p.t = 0;
		p.applied = kNoFrame;
		p.labelShown = true;
		p.missingSaid = false;
		memcpy(p.pos, run.frames[0].pos, sizeof(p.pos));
	}

	// The runs of this level in flickit_runs, the newest kMaxPlayers, each a player (made on the frames after)
	static void LoadRuns(uint32_t level) {
		struct Found {
			std::string file;
			Players::Run run;
		};
		std::vector<Found> found;
		int files = 0, others = 0, bad = 0;
		WIN32_FIND_DATAA fd;
		HANDLE h = FindFirstFileA((std::string(s_dir) + "*.run").c_str(), &fd);
		if (h != INVALID_HANDLE_VALUE) {
			do {
				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					continue;
				}
				files++;
				std::vector<uint8_t> bytes;
				Found f;
				f.file = fd.cFileName;
				if (!ReadWhole(std::string(s_dir) + f.file, &bytes) || !Players::ReadRun(&bytes[0], bytes.size(), &f.run)) {
					bad++;
					Log::TypedLog(CHN_SDL, "Players: flickit_runs\\%s is not a run of Flickit's; it is left out\n", f.file.c_str());
					continue;
				}
				if (f.run.level != level) {
					others++;
					continue;
				}
				found.push_back(f);
			} while (FindNextFileA(h, &fd));
			FindClose(h);
		}
		std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.file < b.file; });
		const size_t first = found.size() > (size_t)kMaxPlayers ? found.size() - kMaxPlayers : 0;
		int n = 0;
		for (size_t k = first; k < found.size(); k++) {
			Use(n++, found[k].file, found[k].run);
		}
		Log::TypedLog(CHN_SDL, "Players: %d runs in flickit_runs, %d of this level (%08X), %d of others%s; %d players here\n",
			files, (int)found.size(), level, others, bad ? ", some not runs" : "", n);
		for (int i = 0; i < n; i++) {
			const Player& p = s_players[i];
			char clock[16];
			Clock(p.run.Length(), clock, sizeof(clock));
			Log::TypedLog(CHN_SDL, "Players: %s skates %s (%s, %u frames): %s, %s%s%s, story at %s\n", p.who.name, p.file.c_str(), clock,
				(unsigned)p.run.frames.size(), Players::SponsorName(p.who.sponsor), p.who.board[0] ? p.who.board : "no board sponsor",
				p.who.shoes[0] ? ", " : "", p.who.shoes, p.who.level);
		}
	}

	// A new run is a player at once: in a free place, or in the place of the oldest
	static int AddRun(const std::string& file, const Players::Run& run) {
		int slot = -1;
		for (int i = 0; i < kMaxPlayers && slot < 0; i++) {
			if (!s_players[i].used) {
				slot = i;
			}
		}
		if (slot < 0) {
			slot = 0;
			for (int i = 1; i < kMaxPlayers; i++) {
				if (s_players[i].file < s_players[slot].file) {
					slot = i;
				}
			}
			if (s_promptFor == slot) {
				PromptOff();
			}
			Kill(s_players[slot]);
		}
		Use(slot, file, run);
		return slot;
	}

	// Him made, at where his run is: his script is spawned, and makes him with the game's other scripts
	static void Make(int i, const float pos[3], uint32_t now) {
		Player& p = s_players[i];
		p.made++;
		p.madeAt = now;
		p.state = kSpawned;
		p.id = NewName("Player", i);
		p.label = NewName("PlayerLabel", i);
		p.applied = kNoFrame;
		p.labelShown = true;
		p.reported = false;
		p.plays = 0;
		p.noAnims = 0;
		Script::LazyStruct* params = Script::LazyStruct::s_create();
		if (!params) {
			return;
		}
		params->AddChecksum(s_n.id, p.id);
		params->AddChecksum(s_n.label, p.label);
		params->AddString(s_n.name, p.who.name);
		StructAddVector(params, s_n.pos, pos[0], pos[1], pos[2]);
		const bool ran = RunGameScript(s_n.spawnScript, params, 0, 0);
		Script::LazyStruct::s_free(params);
		if (p.made <= 3 || !ran) {
			Note("Players: %s made (object %08X) at (%.0f %.0f %.0f)%s", p.who.name, p.id, pos[0], pos[1], pos[2],
				!ran ? ": Flickit_PlayerSpawn did not run" : p.made > 1 ? ", again" : "");
		}
	}

	// Where his run has him, the way it has him turned
	static void Place(uint8_t* obj, const Players::Sample& smp) {
		float* pos = (float*)(obj + OBJECT_POSITION);
		pos[0] = smp.pos[0];
		pos[1] = smp.pos[1];
		pos[2] = smp.pos[2];
		pos[3] = 1.0f;
		float rows[3][3];
		Players::RowsFromQuat(smp.quat, rows);
		const int at[2] = { OBJECT_MATRIX, OBJECT_DISPLAY_MATRIX };
		for (int k = 0; k < 2; k++) {
			float* m = (float*)(obj + at[k]);
			for (int r = 0; r < 3; r++) {
				m[r * 4 + 0] = rows[r][0];
				m[r * 4 + 1] = rows[r][1];
				m[r * 4 + 2] = rows[r][2];
				m[r * 4 + 3] = 0.0f;
			}
		}
	}

	// The animation his run has him in: started when it starts in the run (another, the other foot forward,
	// again from its start), mirrored as it was, at the time it was at
	static void Animate(Player& p, uint8_t* obj, const Players::Sample& smp) {
		uint8_t* anim = (uint8_t*)GetComponent(obj, s_n.animation);
		if (!anim) {
			return;
		}
		const Players::Frame& f = p.run.frames[smp.frame];
		if (p.applied == kNoFrame || smp.frame != p.applied) {
			const bool fresh = p.applied == kNoFrame || smp.frame < p.applied;   // new, or round to the start again
			if (fresh || Players::AnimRestarted(p.run.frames[p.applied], f)) {
				if (f.anim && AnimExists(anim, f.anim)) {
					// mirrored or not as it was: the channel takes bit 29 as it starts
					uint32_t* flags = (uint32_t*)(anim + ANIM_FLAGS);
					*flags = (f.flags & Players::kFlipped) ? (*flags | ANIM_FLIPPED) : (*flags & ~ANIM_FLIPPED);
					PlayAnimCore(anim, f.anim, 0, f.animFrom, f.animTo, Players::LoopOf(f.flags), fresh ? 0.0f : kBlend, f.animSpeed);
					p.plays++;
				}
				else if (f.anim) {
					p.noAnims++;
					if (!p.missingSaid) {
						p.missingSaid = true;
						Note("Players: %s has no animation %08X (of the skater's); he stays in the one before", p.who.name, f.anim);
					}
				}
			}
			p.applied = smp.frame;
		}
		uint8_t* channel = *(uint8_t**)(anim + ANIM_FIRST);
		if (channel && *(const int32_t*)(channel + CHANNEL_SELF) != -1 && *(const uint32_t*)(channel + CHANNEL_NAME) == f.anim) {
			*(float*)(channel + CHANNEL_TIME) = smp.animTime;
		}
	}

	static void ShowLabel(Player& p, bool show) {
		Script::LazyStruct* params = Script::LazyStruct::s_create();
		if (!params) {
			return;
		}
		params->AddChecksum(s_n.label, p.label);
		if (show) {
			params->AddInteger(s_n.show, 1);
		}
		RunGameScript(s_n.labelScript, params, 0, 0);
		Script::LazyStruct::s_free(params);
		p.labelShown = show;
	}

	static float Away(const float a[3], const float b[3]) {
		const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
		return sqrtf(dx * dx + dy * dy + dz * dz);
	}

	static void LabelAt(const Player& p) {
		Script::LazyStruct* params = Script::LazyStruct::s_create();
		if (!params) {
			return;
		}
		params->AddChecksum(s_n.label, p.label);
		StructAddVector(params, s_n.pos, p.pos[0], p.pos[1] + kLabelUp, p.pos[2]);
		RunGameScript(s_n.labelAtScript, params, 0, 0);
		Script::LazyStruct::s_free(params);
	}

	// How he is, once, a few seconds after he was found: for the watchdog's file
	static void Report(Player& p, const uint8_t* obj) {
		p.reported = true;
		const float* at = (const float*)(obj + OBJECT_POSITION);
		const uint8_t* anim = (const uint8_t*)GetComponent((void*)obj, s_n.animation);
		const uint8_t* channel = anim ? *(const uint8_t* const*)(anim + ANIM_FIRST) : 0;
		const bool playing = channel && *(const int32_t*)(channel + CHANNEL_SELF) != -1;
		Note("Players: %s at (%.0f %.0f %.0f), %u ms into his run; animation %08X at %.2f s; %d animations started on him, %d he has not; flags %X/%X; label %s",
			p.who.name, at[0], at[1], at[2], p.t % (p.run.Length() ? p.run.Length() : 1),
			playing ? *(const uint32_t*)(channel + CHANNEL_NAME) : 0, playing ? *(const float*)(channel + CHANNEL_TIME) : 0.0f,
			p.plays, p.noAnims, *(const uint32_t*)(obj + OBJECT_FLAGS), *(const uint32_t*)(obj + COMPOSITE_FLAGS),
			ResolveToObject(p.label) ? "there" : "missing");
	}

	// Each player on along his run: made if he is not there (one a frame), looked for once his script has run,
	// moved and his animation played once he is there; and the nearest in reach
	static void PlayFrame(const DriveGame::Skater& s, uint32_t ms, uint32_t now) {
		bool madeOne = false;
		for (int i = 0; i < kMaxPlayers; i++) {
			Player& p = s_players[i];
			if (!p.used) {
				continue;
			}
			p.t += ms;
			Players::Sample smp;
			if (!Players::SampleRun(p.run, p.t, &smp)) {
				continue;
			}
			uint8_t* obj = 0;
			if (p.state == kNotMade) {
				if (!madeOne && p.made < (uint32_t)kMostMakes && (!p.madeAt || now - p.madeAt >= kRemakeMs)) {
					madeOne = true;
					Make(i, smp.pos, now);
				}
				continue;
			}
			if (p.state == kGaveUp) {
				continue;
			}
			obj = (uint8_t*)ResolveToObject(p.id);
			if (p.state == kSpawned) {
				if (!obj) {
					if (now - p.madeAt > kFindMs) {
						p.misses++;
						// his label is made by the same script, after him: whether it ran
						Note("Players: %s (object %08X) was not there %u ms after he was made (%d of %d); his label %s", p.who.name, p.id,
							now - p.madeAt, p.misses, kMostMisses, ResolveToObject(p.label) ? "was (his script ran)" : "was not either");
						Unmake(p);
						p.madeAt = now - kRemakeMs;      // made again on the next frame, under a new name
						if (p.misses >= kMostMisses) {
							p.state = kGaveUp;
							Note("Players: %s is given up on in this level", p.who.name);
						}
					}
					continue;
				}
				p.state = kLive;
				p.liveAt = now;
				Note("Players: %s is there (object %08X), %u ms after he was made", p.who.name, p.id, now - p.madeAt);
			}
			if (!obj) {
				// gone (a restart of the level's objects): made again a moment later
				Note("Players: %s (object %08X) went", p.who.name, p.id);
				Unmake(p);
				p.madeAt = now;
				continue;
			}
			Place(obj, smp);
			Animate(p, obj, smp);
			memcpy(p.pos, smp.pos, sizeof(p.pos));
			if (!p.reported && now - p.liveAt > 3000) {
				Report(p, obj);
			}
			const float away = Away(p.pos, s.pos);
			const bool show = p.labelShown ? away < kLabelFar + 200.0f : away < kLabelFar;
			if (show != p.labelShown) {
				ShowLabel(p, show);
			}
			if (show) {
				LabelAt(p);
			}
		}
	}

	// Where the camera looks along the ground: from its eye to the skater, and its right from its view; without
	// it, the way he faces (and his right as the camera has it: +x to the right of +z)
	static void Look(const DriveGame::Skater& s) {
		float eye[3], view[16], rx, rz;
		if (FlickitGame::CameraView(eye, view)) {
			const float dx = s.pos[0] - eye[0], dz = s.pos[2] - eye[2];
			const float len = sqrtf(dx * dx + dz * dz);
			if (len > 1.0f && Players::CameraRight(eye, view, &rx, &rz)) {
				s_lookX = dx / len;
				s_lookZ = dz / len;
				s_rightX = rx;
				s_rightZ = rz;
				return;
			}
		}
		FlickitGame::Whereabouts w;
		if (FlickitGame::SkaterWhereabouts(w)) {
			s_lookX = sinf(w.heading);
			s_lookZ = cosf(w.heading);
			s_rightX = cosf(w.heading);
			s_rightZ = -sinf(w.heading);
			return;
		}
		s_lookX = s_lookZ = s_rightX = s_rightZ = 0.0f;
	}

	// The name of the one picked lit, the one before's as the others again
	static void Light(int i) {
		const uint32_t label = i >= 0 ? s_players[i].label : 0;
		if (i == s_lit && label == s_litLabel) {
			return;
		}
		Script::LazyStruct* params;
		if (s_lit >= 0 && s_players[s_lit].used && s_players[s_lit].label == s_litLabel && (params = Script::LazyStruct::s_create()) != NULL) {
			params->AddChecksum(s_n.label, s_litLabel);
			RunGameScript(s_n.labelPickScript, params, 0, 0);
			Script::LazyStruct::s_free(params);
		}
		s_lit = i;
		s_litLabel = label;
		if (i >= 0 && (params = Script::LazyStruct::s_create()) != NULL) {
			params->AddChecksum(s_n.label, label);
			params->AddInteger(s_n.picked, 1);
			RunGameScript(s_n.labelPickScript, params, 0, 0);
			Script::LazyStruct::s_free(params);
		}
	}

	// Who Circle is for: the one he is looking at, or right by him, or the one picked a moment ago
	static void Pick(const DriveGame::Skater& s, uint32_t now) {
		Players::AimPlayer around[kMaxPlayers];
		for (int i = 0; i < kMaxPlayers; i++) {
			const Player& p = s_players[i];
			around[i].present = p.used && p.state == kLive && !s.driving;
			around[i].dx = p.pos[0] - s.pos[0];
			around[i].dy = p.pos[1] - s.pos[1];
			around[i].dz = p.pos[2] - s.pos[2];
		}
		float edge = kNone;
		s_target = Players::Aim(around, kMaxPlayers, s_lookX, s_lookZ, now, s_aimSet, &s_aim, &edge);
		s_targetAway = s_target >= 0 ? edge : kNone;
		Light(s_target);
	}

	/******************************************************************/
	/*                                                                */
	/*  The recording                                                 */
	/*                                                                */
	/******************************************************************/

	// The skater on this frame: where he is, the way he is drawn turned, the animation he is in
	static bool ReadFrame(uint8_t* skater, uint32_t t, Players::Frame* f) {
		memset(f, 0, sizeof(*f));
		f->t = t;
		const float* pos = (const float*)(skater + OBJECT_POSITION);
		const float* m = (const float*)(skater + OBJECT_DISPLAY_MATRIX);
		if (!Watchdog::Sane(pos, 3) || !Watchdog::Sane(m, 12)) {
			return false;
		}
		memcpy(f->pos, pos, sizeof(f->pos));
		const float rows[3][3] = { { m[0], m[1], m[2] }, { m[4], m[5], m[6] }, { m[8], m[9], m[10] } };
		Players::QuatFromRows(rows, f->quat);
		f->quat[3] = f->quat[3] == f->quat[3] ? f->quat[3] : 1.0f;
		const uint8_t* anim = (const uint8_t*)GetComponent(skater, s_n.animation);
		const uint8_t* channel = anim ? *(const uint8_t* const*)(anim + ANIM_FIRST) : 0;
		if (channel && *(const int32_t*)(channel + CHANNEL_SELF) != -1) {
			const float low = *(const float*)(channel + CHANNEL_LOW);
			const float high = *(const float*)(channel + CHANNEL_HIGH);
			const float time = *(const float*)(channel + CHANNEL_TIME);
			const float speed = *(const float*)(channel + CHANNEL_SPEED) * 60.0f;
			const bool backwards = *(const int32_t*)(channel + CHANNEL_BACKWARDS) != 0;
			const int loop = *(const int32_t*)(channel + CHANNEL_LOOP);
			const float numbers[4] = { low, high, time, speed };
			if (Watchdog::Sane(numbers, 4, 1.0e5f)) {
				f->anim = *(const uint32_t*)(channel + CHANNEL_NAME);
				f->animTime = time;
				f->animFrom = backwards ? high : low;
				f->animTo = backwards ? low : high;
				f->animSpeed = speed;
				f->flags = (backwards ? Players::kBackwards : 0) | Players::LoopFlags(loop >= 0 && loop <= 3 ? loop : 0);
			}
		}
		// mirrored as his animation is: as bit 29 was when it started (a flip after that is for the next)
		const uint32_t name = channel && *(const int32_t*)(channel + CHANNEL_SELF) != -1 ? *(const uint32_t*)(channel + CHANNEL_NAME) : 0;
		if (channel != s_recChannel || name != s_recAnim) {
			s_recChannel = channel;
			s_recAnim = name;
			s_recFlipped = anim && (*(const uint32_t*)(anim + ANIM_FLAGS) & ANIM_FLIPPED) != 0;
		}
		if (s_recFlipped) {
			f->flags |= Players::kFlipped;
		}
		return true;
	}

	static void StartRecording(uint32_t level) {
		s_rec.level = level;
		s_rec.frames.clear();
		s_rec.frames.reserve(30 * 60);
		s_recMs = 0;
		s_recShown = -1;
		s_recChannel = 0;
		s_recAnim = 0;
		s_recFlipped = false;
		s_recording = true;
		char text[96];
		sprintf_s(text, sizeof(text), "Recording a run. %s again to stop.", KeyName());
		Say(text, 2500);
		Log::TypedLog(CHN_SDL, "Players: recording a run in the level %08X\n", level);
	}

	// The run saved as flickit_runs\run_<date>_<time>.run, and a player skates it at once
	static void StopRecording(const char* why) {
		s_recording = false;
		RunGameScript(s_n.recOffScript, 0, 0, 0);
		char clock[16];
		Clock(s_rec.Length(), clock, sizeof(clock));
		if (s_rec.frames.size() < 2 || s_rec.Length() < kMinRunMs) {
			Say("Too short to keep: a run is 3 seconds at least.");
			Log::TypedLog(CHN_SDL, "Players: the run stopped (%s) after %s: too short to keep\n", why, clock);
			s_rec.frames.clear();
			return;
		}
		CreateDirectoryA(s_dir, NULL);
		SYSTEMTIME st;
		GetLocalTime(&st);
		char stamp[32], file[48];
		sprintf_s(stamp, sizeof(stamp), "run_%04u%02u%02u_%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		std::string path;
		bool saved = false;
		for (int k = 0; k < 10 && !saved; k++) {
			// (another of the same second gets a letter)
			if (k) {
				sprintf_s(file, sizeof(file), "%s_%c.run", stamp, (char)('a' + k));
			}
			else {
				sprintf_s(file, sizeof(file), "%s.run", stamp);
			}
			path = std::string(s_dir) + file;
			saved = WriteWhole(path, Players::WriteRun(s_rec));
		}
		if (!saved) {
			Say("Could not save the run in flickit_runs.");
			Log::TypedLog(CHN_SDL, "Players: could not write the run to %s (error %u)\n", path.c_str(), (unsigned)GetLastError());
			s_rec.frames.clear();
			return;
		}
		const int slot = AddRun(file, s_rec);
		const Players::Identity& who = s_players[slot].who;
		char text[160];
		sprintf_s(text, sizeof(text), "Run saved (%s). %s skates it now, from where you started.", clock, who.name);
		Say(text, 4000);
		Log::TypedLog(CHN_SDL, "Players: the run stopped (%s): %s, %u frames, saved as flickit_runs\\%s; %s skates it\n", why, clock,
			(unsigned)s_rec.frames.size(), file, who.name);
		s_rec.frames.clear();
	}

	static void RecordFrame(const DriveGame::Skater& s, uint32_t ms) {
		if (s.driving) {
			StopRecording("he got in a car");
			return;
		}
		if (!s_rec.frames.empty()) {
			s_recMs += ms;
		}
		Players::Frame f;
		if (ReadFrame(s.object, s_recMs, &f) && Players::Keep(s_rec, f)) {
			s_rec.frames.push_back(f);
		}
		if (s_rec.frames.size() >= Players::kMaxFrames) {
			StopRecording("five minutes, the most a run has");
			return;
		}
		const int seconds = (int)(s_recMs / 1000);
		if (seconds != s_recShown) {
			s_recShown = seconds;
			char clock[16], text[32];
			Clock(s_recMs, clock, sizeof(clock));
			sprintf_s(text, sizeof(text), "REC %s", clock);
			RunWithText(s_n.recScript, text, 0);
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  The frame                                                     */
	/*                                                                */
	/******************************************************************/

	static bool AnyUsed() {
		for (int i = 0; i < kMaxPlayers; i++) {
			if (s_players[i].used) {
				return true;
			}
		}
		return false;
	}

	void Frame(const DriveGame::Skater& s) {
		s_target = -1;
		s_targetAway = kNone;
		s_down = false;
		if (!s_on) {
			// switched off while there were players (the reload key)
			if (s_ready > 0 && s_parsed > 0 && (s_recording || AnyUsed())) {
				if (s_recording) {
					StopRecording("the players were switched off");
				}
				KillAll();
			}
			s_keyAsked = false;
			return;
		}
		if (!FlickitGame::Installed() || !Ready() || !s.object || !s.pos || !Parsed()) {
			return;
		}
		const uint32_t ms = FrameMs();
		const uint32_t now = GetTickCount();
		LevelKey key;
		key.loads = BigWorld::NodeLoads();
		key.nodes = GetGlobalArray(s_n.nodeArray, 0);
		key.count = key.nodes ? *(const uint32_t*)((const uint8_t*)key.nodes + 8) : 0;
		key.level = *kSkate ? *(const uint32_t*)(*kSkate + SKATE_LEVEL) : 0;
		if (key != s_level) {
			if (s_recording) {
				StopRecording("the level changed");
			}
			KillAll();
			s_level = key;
			if (!InNet()) {
				LoadRuns(key.level);
			}
		}
		if (InNet()) {
			if (s_recording) {
				StopRecording("a net game");
			}
			if (AnyUsed()) {
				KillAll();
			}
			s_keyAsked = false;
			return;
		}
		if (s_keyAsked) {
			s_keyAsked = false;
			if (s_recording) {
				StopRecording(KeyName());
			}
			else if (s.driving) {
				Say("Get out of the car to record a run.");
			}
			else {
				StartRecording(key.level);
			}
		}
		if (s_recording) {
			RecordFrame(s, ms);
		}
		PlayFrame(s, ms, now);
		memcpy(s_skater, s.pos, sizeof(s_skater));
		Look(s);
		Pick(s, now);
		s_down = s.take && *s.take != 0 && !DriveGame::FlickitGrab();
	}

	float NearestAway() {
		return s_target >= 0 ? s_targetAway : kNone;
	}

	void ListPlayers(std::vector<Listed>& out) {
		out.clear();
		if (!s_on || s_ready <= 0 || s_parsed <= 0) {
			return;
		}
		for (int i = 0; i < kMaxPlayers; i++) {
			const Player& p = s_players[i];
			if (!p.used || p.state != kLive) {
				continue;
			}
			Listed l;
			l.slot = i;
			l.who = p.who;
			const float dx = p.pos[0] - s_skater[0], dz = p.pos[2] - s_skater[2];
			l.away = Away(p.pos, s_skater);
			l.bearing = (s_lookX != 0.0f || s_lookZ != 0.0f) ? Players::Bearing(s_lookX, s_lookZ, s_rightX, s_rightZ, dx, dz) : 0.0f;
			l.picked = i == s_target;
			out.push_back(l);
		}
		std::sort(out.begin(), out.end(), [](const Listed& a, const Listed& b) { return a.away < b.away; });
	}

	void AfterCars(const DriveGame::Skater& s, float carAway) {
		if (!s_on || s_ready <= 0 || s_parsed <= 0 || !s.take) {
			return;
		}
		const uint32_t now = GetTickCount();
		const bool chosen = s_target >= 0 && !s.driving && s_targetAway < carAway && !WorldMapGame::CardOpen();
		if (chosen) {
			s_promptSeen = now;
			if (s_promptFor != s_target) {
				PromptOff();
				Prompt(s_target);
			}
		}
		else if (s_promptFor >= 0 && (now - s_promptSeen > kPromptLingerMs || !s_players[s_promptFor].used)) {
			PromptOff();
		}
		const bool pressed = s_down && !s_downWas;
		s_downWas = s_down;
		if (!s_down) {
			s_released = true;
		}
		// on the ground: Circle in the air is the game's (a grab)
		if (pressed && s_released && chosen && s.free) {
			s_released = false;
			*s.take = 0;                         // the game does not see the press that opens a card
			PromptOff();
			const Player& p = s_players[s_target];
			if (WorldMapGame::AskCard(p.who)) {
				Log::TypedLog(CHN_SDL, "Players: Circle by %s: his card\n", p.who.name);
			}
			else {
				CardWords(p.who);
				Log::TypedLog(CHN_SDL, "Players: Circle by %s: his card in words (the map's card cannot be drawn)\n", p.who.name);
			}
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  Settings                                                      */
	/*                                                                */
	/******************************************************************/

	int RunKey() {
		return s_on ? s_runKey : 0;
	}

	void RunKeyPressed() {
		s_keyAsked = true;
	}

	void AtScriptsLoaded() {
		if (s_on && FlickitGame::Installed() && Ready()) {
			Parsed();
		}
	}

	void Configure(const char* configFile) {
		s_on = GetPrivateProfileIntA(FLICKIT_SECTION, "FakePlayers", 1, configFile) != 0;
		int reach = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "PlayerReach", 150, configFile);
		reach = reach < 20 ? 20 : (reach > 400 ? 400 : reach);
		s_aimSet.reach = (float)reach;
		int aim = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "PlayerAimReach", 600, configFile);
		aim = aim < 100 ? 100 : (aim > 3000 ? 3000 : aim);
		s_aimSet.aimReach = (float)aim;
		int lock = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "PlayerLockMs", 4000, configFile);
		lock = lock < 0 ? 0 : (lock > 15000 ? 15000 : lock);
		s_aimSet.lockMs = (uint32_t)lock;
		int key = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "RunKey", 63, configFile);
		s_runKey = key > 0 && key < SDL_NUM_SCANCODES ? key : 0;
		if (!s_dir[0]) {
			const DWORD n = GetModuleFileNameA(NULL, s_dir, MAX_PATH);
			char* slash = n && n < MAX_PATH ? strrchr(s_dir, '\\') : NULL;
			if (slash && (size_t)(slash - s_dir) + 15 < MAX_PATH) {
				strcpy_s(slash + 1, MAX_PATH - (size_t)(slash + 1 - s_dir), "flickit_runs\\");
			}
			else {
				strcpy_s(s_dir, sizeof(s_dir), "flickit_runs\\");
			}
		}
		if (!s_on) {
			Log::TypedLog(CHN_SDL, "Players: off ([Flickit] FakePlayers=0)\n");
		}
		else if (!FlickitGame::Installed()) {
			Log::TypedLog(CHN_SDL, "Players: off (they need Flickit's hooks in the game: GameHooks=1)\n");
		}
	}
}
