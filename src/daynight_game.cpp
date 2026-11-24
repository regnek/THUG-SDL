#include <windows.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "daynight.h"
#include "daynight_game.h"
#include "flickit_daynight_qb.h"
#include "bigworld.h"
#include "flickit.h"
#include "flickit_game.h"
#include "QB/LazyStruct.h"
#include "Logger/Logger.h"

#define FLICKIT_SECTION "Flickit"

namespace DayNightGame {
	/******************************************************************/
	/*                                                                */
	/*  The game's own functions                                      */
	/*                                                                */
	/******************************************************************/

	// Script::ParseQB for a file in memory, as the game reads a cutscene's scripts
	typedef void(__cdecl* ParseQBFromMemory_t)(const char* name, uint8_t* data, int flag);
	static const ParseQBFromMemory_t ParseQBFromMemory = (ParseQBFromMemory_t)0x00409F90;
	// A symbol by its name: its type at +1 (7 a script)
	typedef uint8_t* (__cdecl* LookUpSymbol_t)(uint32_t name);
	static const LookUpSymbol_t LookUpSymbol = (LookUpSymbol_t)0x004133A0;
	// Script::RunScript: the name of a script, its parameters, the object it runs on
	typedef bool(__cdecl* RunScript_t)(uint32_t name, void* params, void* object, void* object2);
	static const RunScript_t RunGameScript = (RunScript_t)0x0040FD00;
	// GameNet::Manager::InNetGame
	typedef bool(__thiscall* InNetGame_t)(void* net);
	static const InNetGame_t InNetGame = (InNetGame_t)0x004C6B50;
	static void** const kNetManager = (void**)0x0076914C;
	static uint8_t** const kSkate = (uint8_t**)0x0076A788;
	static const uint32_t SKATE_LEVEL = 0x398;     // the level's load script (LevelIs)
	enum { SYMBOL_SCRIPT = 7 };

	struct Expect {
		uint32_t address;
		int size;
		uint8_t bytes[16];
		const char* what;
	};
	static const Expect kExpect[] = {
		{ 0x00409F90, 13, { 0x8B, 0x44, 0x24, 0x0C, 0x8B, 0x4C, 0x24, 0x08, 0x8B, 0x54, 0x24, 0x04, 0x6A }, "Script::ParseQB, from memory" },
		{ 0x004133A0, 12, { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0xC1, 0x25, 0xFF, 0x0F, 0x00, 0x00, 0x56 }, "Script::LookUpSymbol" },
		{ 0x0040FD00, 12, { 0x6A, 0xFF, 0x68, 0x5B, 0x18, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "Script::RunScript" },
		{ 0x004C6B50, 12, { 0x56, 0x8B, 0xF1, 0xF6, 0x86, 0x38, 0x01, 0x00, 0x00, 0x0C, 0x74, 0x24 }, "GameNet::Manager::InNetGame" },
		// SetSceneColor: color (whole), sky (whole, the colour's by default; for outdoor), lightgroup
		{ 0x00524E40, 10, { 0x68, 0x40, 0xBB, 0x63, 0x00, 0x8B, 0xCE, 0xE8, 0xB4, 0xC5 }, "SetSceneColor: color" },
	};

	/******************************************************************/
	/*                                                                */
	/*  State                                                         */
	/*                                                                */
	/******************************************************************/

	static bool s_on = true;                 // [Flickit] DayNight
	static float s_dayMinutes = 24.0f;       // [Flickit] DayMinutes
	static float s_start = 10.0f;            // [Flickit] DayStart
	static int s_key = 62;                   // [Flickit] DayKey (F5)
	static bool s_clockSet = false;
	static DayNight::Clock s_clock;
	static int s_ready = 0;                  // 1: the game is as expected, -1: not
	static int s_parsed = 0;                 // 1: the game has the scripts, -1: it did not take them
	static LARGE_INTEGER s_lastFrame;
	static uint32_t s_levelLoads = 0xFFFFFFFFu;
	static uint32_t s_level = 0;
	static float s_sinceLook = 1.0e9f;
	static float s_sinceScene = 1.0e9f;
	static int s_sceneColor = -1, s_skyColor = -1;   // as last given to the level's geometry and sky
	static int s_night = -1, s_evening = -1, s_headlights = -1;   // as last told to the game (-1: not yet in this level)
	static const char* s_loggedPart = 0;
	static bool s_shown = false;              // the level has had a look of Flickit's (to take back when it goes off)
	static volatile LONG s_skip = 0;         // DayKey presses not yet counted

	struct Names {
		uint32_t lookScript, lightsScript, headlightsOn, headlightsOff;
		uint32_t scene, color, sky, night, evening, time, gap;
		uint32_t ambientRed, ambientGreen, ambientBlue, ambientMod;
		uint32_t heading[2], pitch[2], red[2], green[2], blue[2], mod[2];
	};
	static Names s_n;

	// A line for the log and flickit_watchdog.txt (the log is only written with the console on)
	static void Note(const char* format, ...) {
		char text[400];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		text[sizeof(text) - 1] = 0;
		FlickitGame::WatchNote(text);
	}

	static void InitNames() {
		s_n.lookScript = Flickit::NameChecksum("Flickit_DayNightLook");
		s_n.lightsScript = Flickit::NameChecksum("Flickit_DayNightLights");
		s_n.headlightsOn = Flickit::NameChecksum("CarTOD_TurnOnHeadlights");
		s_n.headlightsOff = Flickit::NameChecksum("CarTOD_TurnOffHeadlights");
		s_n.scene = Flickit::NameChecksum("scene");
		s_n.color = Flickit::NameChecksum("color");
		s_n.sky = Flickit::NameChecksum("sky");
		s_n.night = Flickit::NameChecksum("night");
		s_n.evening = Flickit::NameChecksum("evening");
		s_n.time = Flickit::NameChecksum("time");
		s_n.gap = Flickit::NameChecksum("gap");
		s_n.ambientRed = Flickit::NameChecksum("ambient_red");
		s_n.ambientGreen = Flickit::NameChecksum("ambient_green");
		s_n.ambientBlue = Flickit::NameChecksum("ambient_blue");
		s_n.ambientMod = Flickit::NameChecksum("ambient_mod_factor");
		for (int i = 0; i < 2; i++) {
			char name[32];
			sprintf_s(name, sizeof(name), "heading_%d", i);
			s_n.heading[i] = Flickit::NameChecksum(name);
			sprintf_s(name, sizeof(name), "pitch_%d", i);
			s_n.pitch[i] = Flickit::NameChecksum(name);
			sprintf_s(name, sizeof(name), "red_%d", i);
			s_n.red[i] = Flickit::NameChecksum(name);
			sprintf_s(name, sizeof(name), "green_%d", i);
			s_n.green[i] = Flickit::NameChecksum(name);
			sprintf_s(name, sizeof(name), "blue_%d", i);
			s_n.blue[i] = Flickit::NameChecksum(name);
			sprintf_s(name, sizeof(name), "mod_factor_%d", i);
			s_n.mod[i] = Flickit::NameChecksum(name);
		}
	}

	static bool Ready() {
		if (s_ready == 0) {
			s_ready = 1;
			for (size_t i = 0; i < sizeof(kExpect) / sizeof(kExpect[0]); i++) {
				if (memcmp((const void*)kExpect[i].address, kExpect[i].bytes, (size_t)kExpect[i].size) != 0) {
					Note("Day and night: the game's code at %08X (%s) is not what was expected; no day and night\n",
						kExpect[i].address, kExpect[i].what);
					s_ready = -1;
				}
			}
			InitNames();
		}
		return s_ready > 0;
	}

	static bool Parsed() {
		if (s_parsed == 0) {
			static uint8_t data[sizeof(flickit_daynight_qb)];
			memcpy(data, flickit_daynight_qb, sizeof(data));
			ParseQBFromMemory("scripts\\flickit\\daynight.qb", data, 1);
			const uint8_t* look = LookUpSymbol(s_n.lookScript);
			if (look && look[1] == SYMBOL_SCRIPT) {
				s_parsed = 1;
				Note("Day and night: the game has Flickit's day and night scripts (%u bytes)\n", (unsigned)sizeof(data));
			}
			else {
				s_parsed = -1;
				Note("Day and night: the game did not take Flickit's day and night scripts; no day and night\n");
			}
		}
		return s_parsed > 0;
	}

	static float FrameSeconds() {
		LARGE_INTEGER now, freq;
		QueryPerformanceCounter(&now);
		QueryPerformanceFrequency(&freq);
		float s = 0.0f;
		if (s_lastFrame.QuadPart && freq.QuadPart) {
			s = (float)((double)(now.QuadPart - s_lastFrame.QuadPart) / (double)freq.QuadPart);
		}
		s_lastFrame = now;
		return s;
	}

	static const char* PartOfDay(float hour) {
		return hour < 5.0f ? "night" : hour < 8.0f ? "dawn" : hour < 17.0f ? "day" : hour < 20.25f ? "dusk" : "night";
	}

	/******************************************************************/
	/*                                                                */
	/*  The look                                                      */
	/*                                                                */
	/******************************************************************/

	static void AddLight(Script::LazyStruct* p, uint32_t red, uint32_t green, uint32_t blue, const DayNight::Light& l) {
		p->AddInteger(red, (int)(l.rgb[0] + 0.5f));
		p->AddInteger(green, (int)(l.rgb[1] + 0.5f));
		p->AddInteger(blue, (int)(l.rgb[2] + 0.5f));
	}

	// The level's look for this hour (Flickit_DayNightLook). The colours of its geometry and sky go to the sectors
	// one by one, so they go only when they change, and every 10 seconds besides (a script of the level's own may
	// have changed them); the lights the skater and the rest are lit by are a few numbers, so every time.
	static void Look(float hour, bool fresh) {
		const DayNight::Look look = DayNight::LookAt(hour);
		const int color = DayNight::Packed(look.level), sky = DayNight::Packed(look.sky);
		const bool scene = fresh || color != s_sceneColor || sky != s_skyColor || s_sinceScene >= 10.0f;
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (!p) {
			return;
		}
		p->AddInteger(s_n.scene, scene ? 1 : 0);
		p->AddInteger(s_n.color, color);
		p->AddInteger(s_n.sky, sky);
		AddLight(p, s_n.ambientRed, s_n.ambientGreen, s_n.ambientBlue, look.ambient);
		p->AddFloat(s_n.ambientMod, look.ambient.mod);
		for (int i = 0; i < 2; i++) {
			p->AddInteger(s_n.heading[i], (int)DayNight::kHeading[i]);
			p->AddInteger(s_n.pitch[i], (int)DayNight::kPitch[i]);
			AddLight(p, s_n.red[i], s_n.green[i], s_n.blue[i], look.dir[i]);
			p->AddFloat(s_n.mod[i], look.dir[i].mod);
		}
		RunGameScript(s_n.lookScript, p, 0, 0);
		Script::LazyStruct::s_free(p);
		if (scene) {
			s_sceneColor = color;
			s_skyColor = sky;
			s_sinceScene = 0.0f;
		}
	}

	// The level's own night (Flickit_DayNightLights): its lights for the night and the evening, coming on or going
	// off one after another over frames (0: at once), its lit windows and its sounds for the night
	static void Lights(bool night, bool evening, int frames) {
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (!p) {
			return;
		}
		p->AddInteger(s_n.night, night ? 1 : 0);
		p->AddInteger(s_n.evening, evening ? 1 : 0);
		p->AddInteger(s_n.time, frames);
		p->AddInteger(s_n.gap, frames + 1);
		RunGameScript(s_n.lightsScript, p, 0, 0);
		Script::LazyStruct::s_free(p);
	}

	/******************************************************************/
	/*                                                                */
	/*  The frame                                                     */
	/*                                                                */
	/******************************************************************/

	// Off again (DayNight=0 and the ini read again): the level as at midday, its night lights and the
	// headlights off
	static void TakeBack() {
		s_shown = false;
		Look(12.0f, true);
		Lights(false, false, 0);
		RunGameScript(s_n.headlightsOff, 0, 0, 0);
		s_night = s_evening = s_headlights = -1;
		s_sinceLook = 1.0e9f;
		Note("Day and night: off; the level as at midday\n");
	}

	void Frame() {
		if (!Ready() || !Parsed()) {
			return;
		}
		const float seconds = FrameSeconds();
		void* net = *kNetManager;
		if (net && InNetGame(net)) {
			return;
		}
		if (!s_on) {
			if (s_shown) {
				TakeBack();
			}
			return;
		}
		s_clock.Advance(seconds);
		const LONG skips = InterlockedExchange(&s_skip, 0);
		if (skips > 0) {
			s_clock.SetHour(s_clock.Hour() + 2.0f * (float)skips);
			s_sinceLook = 1.0e9f;
			Note("Day and night: the clock put on to %02d:%02d\n", (int)s_clock.Hour(),
				(int)(fmodf(s_clock.Hour(), 1.0f) * 60.0f));
		}
		// a new level: everything told again, at once
		const uint32_t loads = BigWorld::NodeLoads();
		const uint32_t level = *kSkate ? *(const uint32_t*)(*kSkate + SKATE_LEVEL) : 0;
		const bool fresh = loads != s_levelLoads || level != s_level;
		if (fresh) {
			s_levelLoads = loads;
			s_level = level;
			s_night = s_evening = s_headlights = -1;
			s_sinceLook = 1.0e9f;
		}
		const float hour = s_clock.Hour();
		const float step = seconds > 0.0f && seconds < 0.25f ? seconds : 0.0f;
		s_sinceLook += step;
		s_sinceScene += step;
		if (s_sinceLook >= 1.0f) {
			s_sinceLook = 0.0f;
			Look(hour, fresh);
			s_shown = true;
		}
		const int night = DayNight::NightLights(hour) ? 1 : 0, evening = DayNight::EveningLights(hour) ? 1 : 0;
		const int headlights = DayNight::Headlights(hour) ? 1 : 0;
		const bool lightsChange = night != s_night || evening != s_evening || headlights != s_headlights;
		if (night != s_night || evening != s_evening) {
			// at once in a new level; at their hour, one after another over four seconds
			Lights(night != 0, evening != 0, s_night < 0 ? 0 : 240);
			s_night = night;
			s_evening = evening;
		}
		if (headlights != s_headlights) {
			RunGameScript(headlights ? s_n.headlightsOn : s_n.headlightsOff, 0, 0, 0);
			s_headlights = headlights;
		}
		const char* part = PartOfDay(hour);
		if (part != s_loggedPart || lightsChange) {
			s_loggedPart = part;
			Note("Day and night: %02d:%02d, %s%s%s%s\n", (int)hour, (int)(fmodf(hour, 1.0f) * 60.0f), part,
				night ? "; night lights on" : "", evening ? "; evening lights on" : "", headlights ? "; headlights on" : "");
		}
	}

	int Key() {
		return s_on ? s_key : 0;
	}

	void KeyPressed() {
		InterlockedIncrement(&s_skip);
	}

	/******************************************************************/
	/*                                                                */
	/*  Settings                                                      */
	/*                                                                */
	/******************************************************************/

	void AtScriptsLoaded() {
		if (Ready()) {
			Parsed();
		}
	}

	void Configure(const char* configFile) {
		s_on = GetPrivateProfileIntA(FLICKIT_SECTION, "DayNight", 1, configFile) != 0;
		int minutes = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "DayMinutes", 24, configFile);
		minutes = minutes < 0 ? 0 : (minutes > 1440 ? 1440 : minutes);
		int start = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "DayStart", 10, configFile);
		start = start < 0 ? 0 : (start > 23 ? 23 : start);
		s_key = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "DayKey", 62, configFile);
		const bool changed = !s_clockSet || (float)minutes != s_dayMinutes || (float)start != s_start;
		s_dayMinutes = (float)minutes;
		s_start = (float)start;
		if (changed) {
			// (the clock keeps its hour when the ini is read again, unless the start or the day's length changed)
			s_clock.Set(s_start, s_dayMinutes);
			s_clockSet = true;
			s_sinceLook = 1.0e9f;
		}
		if (s_on) {
			Note("Day and night: on; a day goes by in %d minutes of play (0: the clock stands still), from %02d:00; key %d puts it two hours on\n",
				minutes, start, s_key);
		}
		else {
			Note("Day and night: off ([Flickit] DayNight=0)\n");
		}
	}
}
