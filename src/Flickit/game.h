#pragma once
// The running game, as far as more than one part of the mod needs it.
//
// The parts of the mod that reach into the game (flickit_game, drive_game, players_game, daynight_game,
// worldmap_game, guard_game) each know the game's functions and places they alone use. What several of them
// use is here, once: those functions and places, the check that the game's code is what they were read from,
// and the things they all do the same way: a note in flickit_watchdog.txt, the folder of the exe, a whole file
// read, a script run by its name, whether this is a net game, which level is loaded, how long a frame took.
//
// Everything here is written against the one PC executable of THUG (THUG.exe / THUG-SDL.exe, the same code).
// Ready() compares the code at every address used here with what is expected there; a part of the mod asks it
// before it uses any of this, and stays off if it says no.
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "str.h"

namespace Game {
	/******************************************************************/
	/*                                                                */
	/*  The game's own functions and places                           */
	/*                                                                */
	/******************************************************************/

	// Skate::Instance(), the module that owns the skaters: 0 until the game has made it. The checksum of the
	// loaded level's load script (load_NJ ...) is in it, where the script command LevelIs reads it.
	static uint8_t** const kSkate = (uint8_t**)0x0076A788;
	enum { SKATE_LEVEL = 0x398 };

	// Obj::ResolveToObject: an object by its name, or 0
	typedef void* (__cdecl* ResolveToObject_t)(uint32_t id);
	static const ResolveToObject_t ResolveToObject = (ResolveToObject_t)0x0041E750;
	// A component of an object by the checksum of its type's name, or 0
	typedef void* (__thiscall* GetComponent_t)(void* object, uint32_t type);
	static const GetComponent_t GetComponent = (GetComponent_t)0x0041BDC0;
	static const uint32_t kSkaterCorePhysics = 0x5BD63B29;    // SkaterCorePhysics

	// A global array by its name (the level's NodeArray, an array of structures), or 0: its size is at +8. A
	// structure in an array.
	static const uint32_t kNodeArray = 0xC472ECC5;            // NodeArray
	typedef void* (__cdecl* GetGlobalArray_t)(uint32_t name, int assertIfMissing);
	static const GetGlobalArray_t GetGlobalArray = (GetGlobalArray_t)0x00413650;
	typedef void* (__thiscall* ArrayStructure_t)(void* array, uint32_t index);
	static const ArrayStructure_t ArrayStructure = (ArrayStructure_t)0x00408350;
	// Script::CStruct: a checksum by its name (in the structures a node names as well); a vector put in
	typedef bool(__thiscall* StructChecksum_t)(void* s, uint32_t name, uint32_t* out, int assertIfMissing);
	static const StructChecksum_t StructChecksum = (StructChecksum_t)0x004110B0;
	typedef void(__thiscall* StructAddVector_t)(void* s, uint32_t name, float x, float y, float z);
	static const StructAddVector_t StructAddVector = (StructAddVector_t)0x004124D0;

	// A symbol of the game's scripts by its name, or 0: its type at +1, its value at +0xC
	typedef uint8_t* (__cdecl* LookUpSymbol_t)(uint32_t name);
	static const LookUpSymbol_t LookUpSymbol = (LookUpSymbol_t)0x004133A0;
	enum {
		SYMBOL_SCRIPT = 7,
		SYMBOL_STRUCTURE = 10,
	};
	// Whether the game's scripts have a symbol of that name and kind; a script of that name
	inline bool IsSymbol(uint32_t name, int kind) {
		const uint8_t* symbol = LookUpSymbol(name);
		return symbol && symbol[1] == kind;
	}
	inline bool IsScript(uint32_t name) {
		return IsSymbol(name, SYMBOL_SCRIPT);
	}
	// Script::ParseQB for a file in memory, as the game reads a cutscene's scripts (it changes the bytes)
	typedef void(__cdecl* ParseQBFromMemory_t)(const char* name, uint8_t* data, int flag);
	static const ParseQBFromMemory_t ParseQBFromMemory = (ParseQBFromMemory_t)0x00409F90;
	// Script::RunScript: the name of a script, its parameters, the object it runs on. What it answers is no
	// word on whether there was such a script (RunScriptOf is).
	typedef bool(__cdecl* RunScript_t)(uint32_t name, void* params, void* object, void* object2);
	static const RunScript_t RunGameScript = (RunScript_t)0x0040FD00;

	// CAnimationComponent: start an animation, what PlayAnim does once it has read its parameters: the name,
	// whether to tell the other players of a net game, where it starts and ends (seconds), how it loops (0 once,
	// 1 Cycle, 2 PingPong, 3 wobble), the blend's time, and its speed. And whether the object has an animation
	// of that name.
	typedef void(__thiscall* PlayAnimCore_t)(void* anim, uint32_t name, int net, float from, float to, int loop, float blend, float speed);
	static const PlayAnimCore_t PlayAnimCore = (PlayAnimCore_t)0x004320E0;
	typedef bool(__thiscall* AnimExists_t)(void* anim, uint32_t name);
	static const AnimExists_t AnimExists = (AnimExists_t)0x00430650;

	/******************************************************************/
	/*                                                                */
	/*  Is this the game the mod was written against?                 */
	/*                                                                */
	/******************************************************************/

	// A place in the game's code and the bytes expected there
	struct Expect {
		uint32_t address;
		int size;
		uint8_t bytes[25];
		const char* what;
	};
	inline bool Matches(const Expect& e) {
		return memcmp((const void*)(uintptr_t)e.address, e.bytes, (size_t)e.size) == 0;
	}

	// Everything above is where it is expected (compared once; what differs goes to the log)
	bool Ready();

	/******************************************************************/
	/*                                                                */
	/*  What every part of the mod does the same way                  */
	/*                                                                */
	/******************************************************************/

	// A line for flickit_watchdog.txt next to the exe (with the time of day) and for the game's log. The file is
	// the only record that lasts: the log is there only with [Logger] Console=1. The text ends in a newline as
	// the log's lines do (one that is missing is added, and none is doubled).
	void Note(const char* format, ...) STR_PRINTF(1, 2);
	void NoteV(const char* format, va_list args);

	// The folder the game's exe is in, with its last backslash; "" when it cannot be had
	const char* ExeDir();

	// A whole file, if it is no bigger than most. False when it cannot be read.
	bool ReadWhole(const char* path, std::vector<uint8_t>* out, uint64_t most);

	// Runs a script of the game's by its name; false when the game has no script of that name
	bool RunScriptOf(uint32_t name, void* params = 0, void* object = 0);

	// GameNet::Manager::InNetGame
	bool InNet();

	// The level: the checksum of its load script (0 before the game has one), and what changes whenever a
	// level's objects are new: BigWorld's count of level loads (0 while BigNodeNames is off), the NodeArray
	// and its size, and that checksum
	uint32_t LevelScript();
	struct LevelKey {
		uint32_t loads;
		const void* nodes;
		uint32_t count;
		uint32_t level;
		bool operator!=(const LevelKey& o) const {
			return loads != o.loads || nodes != o.nodes || count != o.count || level != o.level;
		}
	};
	LevelKey Level();

	// The computer's clock, in seconds; and the time from one look at it to the next (0 at the first)
	double Seconds();
	struct FrameTimer {
		double last;        // 0: no look yet
		double Step();
	};
}
