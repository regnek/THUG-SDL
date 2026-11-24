// Flickit's players in the game (see players_game.h).
#include <windows.h>
#include <math.h>
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
		{ 0x0041D5D1, 8, { 0xC7, 0x44, 0xC6, 0x48, 0x21, 0x16, 0xDA, 0xEA }, "the FloatingLabel component, for the game's objects" },
		{ 0x004370BB, 6, { 0x56, 0x68, 0x52, 0x95, 0x7F, 0x72 }, "FloatingLabel: its screen element" },
		{ 0x0043727F, 5, { 0x68, 0x00, 0x19, 0x49, 0x4B }, "FloatingLabel: the label where the object is" },
	};

	/******************************************************************/
	/*                                                                */
	/*  State                                                         */
	/*                                                                */
	/******************************************************************/

	enum { kMaxPlayers = 8 };

	struct Names {
		uint32_t nodeArray, animation;
		uint32_t makeScript, killScript, labelScript, promptScript, promptOffScript, sayScript, wordsScript, recScript, recOffScript;
		uint32_t id, label, name, pos, show, text, time;
		uint32_t player[kMaxPlayers];
	};
	static Names s_n;

	static const uint32_t kMinRunMs = 3000;          // a run shorter than this is not kept
	static const uint32_t kRemakeMs = 2000;          // a player gone (a restart of the level's objects) comes back after this
	static const int kMostMakes = 20;                // and no more often than this in one level
	static const uint32_t kPromptLingerMs = 400;
	static const float kBlend = 0.15f;               // seconds: a new animation of his blends in over this
	static const float kLabelFar = 1500.0f;          // a player farther than this has no name on the screen
	static const float kBody = 20.0f;                // how far, for the cars: from the edge of him
	static const float kNone = 1.0e9f;
	static const size_t kNoFrame = (size_t)-1;

	static bool s_on = true;                 // [Flickit] FakePlayers
	static float s_reach = 90.0f;            // [Flickit] PlayerReach
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
	struct Player {
		bool used;
		std::string file;                    // the run's file, in flickit_runs
		Players::Run run;
		Players::Identity who;
		uint32_t id;                         // the name of his object
		uint32_t label;                      // the name of his label (another each time he is made)
		uint32_t made;                       // how many times he was made
		uint32_t madeAt;                     // GetTickCount() then
		uint32_t t;                          // ms into his run
		size_t applied;                      // the frame of the run whose animation he plays (kNoFrame: none yet)
		bool labelShown;
		bool missingSaid;                    // an animation he has not was told of
		float pos[3];
	};
	static Player s_players[kMaxPlayers];
	static int s_nearIndex = -1;             // the player in reach, the nearest, on this frame
	static float s_nearAway = kNone;
	static int s_promptFor = -1;             // the player the prompt is up for
	static uint32_t s_promptSeen = 0;
	static bool s_down = false;              // Circle on this frame, before the cars (his own press)
	static bool s_downWas = false;
	static bool s_released = true;           // let go since it opened a card
	static uint32_t s_labels = 0;            // labels made: each has a name of its own
	static LARGE_INTEGER s_lastFrame;        // zero: no frame yet

	/******************************************************************/
	/*                                                                */
	/*  Checks                                                        */
	/*                                                                */
	/******************************************************************/

	static void InitNames() {
		s_n.nodeArray = Flickit::NameChecksum("NodeArray");
		s_n.animation = Flickit::NameChecksum("Animation");
		s_n.makeScript = Flickit::NameChecksum("Flickit_PlayerMake");
		s_n.killScript = Flickit::NameChecksum("Flickit_PlayerKill");
		s_n.labelScript = Flickit::NameChecksum("Flickit_PlayerLabelShow");
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
		s_n.text = Flickit::NameChecksum("text");
		s_n.time = Flickit::NameChecksum("time");
		for (int i = 0; i < kMaxPlayers; i++) {
			char name[32];
			sprintf_s(name, sizeof(name), "Flickit_Player%d", i);
			s_n.player[i] = Flickit::NameChecksum(name);
		}
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
			const uint8_t* make = LookUpSymbol(s_n.makeScript);
			const uint8_t* rec = LookUpSymbol(s_n.recScript);
			if (make && make[1] == SYMBOL_SCRIPT && rec && rec[1] == SYMBOL_SCRIPT) {
				s_parsed = 1;
				Log::TypedLog(CHN_SDL, "Players: on; %s records a run, each run of the level is a player (the last %d); Circle within %.0f shows his card; the game has Flickit's player scripts (%u bytes)\n",
					s_runKey > 0 ? SDL_GetScancodeName((SDL_Scancode)s_runKey) : "no key", kMaxPlayers, s_reach, (unsigned)sizeof(data));
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

	// A name of its own for each label made: the label of a player who goes is taken away with him at the end
	// of the frame, by its name, and must not be his successor's
	static uint32_t NewLabelId() {
		char name[48];
		sprintf_s(name, sizeof(name), "Flickit_PlayerLabel%u", ++s_labels);
		return Flickit::NameChecksum(name);
	}

	static void Kill(Player& p) {
		if (p.used && ResolveToObject(p.id)) {
			Script::LazyStruct* params = Script::LazyStruct::s_create();
			if (params) {
				params->AddChecksum(s_n.id, p.id);
				RunGameScript(s_n.killScript, params, 0, 0);
				Script::LazyStruct::s_free(params);
			}
		}
		p.used = false;
		p.run.frames.clear();
	}

	static void KillAll() {
		PromptOff();
		for (int i = 0; i < kMaxPlayers; i++) {
			Kill(s_players[i]);
		}
		s_nearIndex = -1;
		s_nearAway = kNone;
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
		p.id = s_n.player[i];
		p.label = 0;
		p.made = 0;
		p.madeAt = 0;
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

	// Him made, at where his run is
	static void Make(int i, const float pos[3], uint32_t now) {
		Player& p = s_players[i];
		p.made++;
		p.madeAt = now;
		p.label = NewLabelId();
		p.applied = kNoFrame;
		p.labelShown = true;
		Script::LazyStruct* params = Script::LazyStruct::s_create();
		if (!params) {
			return;
		}
		params->AddChecksum(s_n.id, p.id);
		params->AddChecksum(s_n.label, p.label);
		params->AddString(s_n.name, p.who.name);
		StructAddVector(params, s_n.pos, pos[0], pos[1], pos[2]);
		const bool ran = RunGameScript(s_n.makeScript, params, 0, 0);
		Script::LazyStruct::s_free(params);
		const bool there = ResolveToObject(p.id) != 0;
		if (p.made == 1 || !there) {
			Log::TypedLog(CHN_SDL, "Players: %s %s at (%.0f %.0f %.0f)%s\n", p.who.name,
				!ran ? "could not be made (Flickit_PlayerMake did not run)" : there ? "is in the level" : "was not made",
				pos[0], pos[1], pos[2], p.made > 1 ? " again" : "");
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
				}
				else if (f.anim && !p.missingSaid) {
					p.missingSaid = true;
					Log::TypedLog(CHN_SDL, "Players: %s has no animation %08X (of the skater's); he stays in the one before\n", p.who.name, f.anim);
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

	// Each player on along his run, made if he is not there (one a frame), and the nearest in reach
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
			uint8_t* obj = (uint8_t*)ResolveToObject(p.id);
			if (!obj) {
				if (madeOne || (p.madeAt && now - p.madeAt < kRemakeMs) || p.made >= (uint32_t)kMostMakes) {
					continue;
				}
				madeOne = true;
				Make(i, smp.pos, now);
				obj = (uint8_t*)ResolveToObject(p.id);
				if (!obj) {
					continue;
				}
			}
			Place(obj, smp);
			Animate(p, obj, smp);
			memcpy(p.pos, smp.pos, sizeof(p.pos));
			const float away = Away(p.pos, s.pos);
			const bool show = p.labelShown ? away < kLabelFar + 200.0f : away < kLabelFar;
			if (show != p.labelShown) {
				ShowLabel(p, show);
			}
			if (s.free && !s.driving && away < s_reach && fabsf(p.pos[1] - s.pos[1]) < 120.0f) {
				const float edge = away > kBody ? away - kBody : 0.0f;
				if (edge < s_nearAway) {
					s_nearIndex = i;
					s_nearAway = edge;
				}
			}
		}
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
		s_nearIndex = -1;
		s_nearAway = kNone;
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
		s_down = s.take && *s.take != 0 && !DriveGame::FlickitGrab();
	}

	float NearestAway() {
		return s_nearIndex >= 0 ? s_nearAway : kNone;
	}

	void AfterCars(const DriveGame::Skater& s, float carAway) {
		if (!s_on || s_ready <= 0 || s_parsed <= 0 || !s.take) {
			return;
		}
		const uint32_t now = GetTickCount();
		const bool chosen = s_nearIndex >= 0 && s.free && !s.driving && s_nearAway < carAway && !WorldMapGame::CardOpen();
		if (chosen) {
			s_promptSeen = now;
			if (s_promptFor != s_nearIndex) {
				PromptOff();
				Prompt(s_nearIndex);
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
		if (pressed && s_released && chosen) {
			s_released = false;
			*s.take = 0;                         // the game does not see the press that opens a card
			PromptOff();
			const Player& p = s_players[s_nearIndex];
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
		int reach = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "PlayerReach", 90, configFile);
		reach = reach < 20 ? 20 : (reach > 400 ? 400 : reach);
		s_reach = (float)reach;
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
