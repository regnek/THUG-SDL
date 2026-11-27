#include <windows.h>

#include "game.h"
#include "bigworld.h"
#include "Logger/Logger.h"

namespace Game {
	/******************************************************************/
	/*                                                                */
	/*  Is this the game the mod was written against?                 */
	/*                                                                */
	/******************************************************************/

	// GameNet::Manager and its InNetGame (the script command InNetGame asks it)
	static void** const kNetManager = (void**)0x0076914C;
	typedef bool(__thiscall* InNetGame_t)(void* net);
	static const InNetGame_t InNetGame = (InNetGame_t)0x004C6B50;

	static const Expect kExpect[] = {
		{ 0x0041E750, 12, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x0D, 0xC0, 0xB6, 0x69, 0x00, 0x50, 0xE8 }, "Obj::ResolveToObject" },
		{ 0x0041BDC0, 12, { 0x8B, 0x81, 0xF0, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x12, 0x8B, 0x4C }, "CCompositeObject::GetComponent" },
		{ 0x00413650, 12, { 0x8B, 0x44, 0x24, 0x04, 0x50, 0xE8, 0x46, 0xFD, 0xFF, 0xFF, 0x83, 0xC4 }, "Script::GetArray" },
		{ 0x00408350, 12, { 0x83, 0x79, 0x08, 0x01, 0x8B, 0x01, 0x74, 0x07, 0x8B, 0x4C, 0x24, 0x04 }, "CArray::GetStructure" },
		{ 0x004110B0, 12, { 0x51, 0x53, 0x56, 0x8B, 0x31, 0x32, 0xDB, 0x85, 0xF6, 0x89, 0x4C, 0x24 }, "CStruct::GetChecksum" },
		{ 0x004124D0, 12, { 0x6A, 0xFF, 0x68, 0xD6, 0x19, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "CStruct::AddVector" },
		{ 0x004133A0, 12, { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0xC1, 0x25, 0xFF, 0x0F, 0x00, 0x00, 0x56 }, "Script::LookUpSymbol" },
		{ 0x00409F90, 13, { 0x8B, 0x44, 0x24, 0x0C, 0x8B, 0x4C, 0x24, 0x08, 0x8B, 0x54, 0x24, 0x04, 0x6A }, "Script::ParseQB, from memory" },
		{ 0x0040FD00, 12, { 0x6A, 0xFF, 0x68, 0x5B, 0x18, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "Script::RunScript" },
		{ 0x004320E0, 12, { 0x81, 0xEC, 0x4C, 0x05, 0x00, 0x00, 0xA1, 0xB0, 0x4A, 0x69, 0x00, 0x33 }, "PlayAnim's start of an animation" },
		{ 0x00430650, 15, { 0x8B, 0x41, 0x38, 0x03, 0x44, 0x24, 0x04, 0x6A, 0x00, 0x50, 0xE8, 0x11, 0x7F, 0x04, 0x00 }, "animation exists" },
		{ 0x00527F90, 11, { 0x8B, 0x0D, 0x4C, 0x91, 0x76, 0x00, 0xE9, 0xB5, 0xEB, 0xF9, 0xFF }, "the script command InNetGame" },
		{ 0x004C6B50, 12, { 0x56, 0x8B, 0xF1, 0xF6, 0x86, 0x38, 0x01, 0x00, 0x00, 0x0C, 0x74, 0x24 }, "GameNet::Manager::InNetGame" },
		{ 0x00528DAA, 6, { 0x8B, 0x8E, 0x98, 0x03, 0x00, 0x00 }, "LevelIs: the level loaded" },
	};

	bool Ready() {
		static int s_ready = 0;         // 1: as expected, -1: not
		if (s_ready == 0) {
			s_ready = 1;
			for (size_t i = 0; i < sizeof(kExpect) / sizeof(kExpect[0]); i++) {
				if (!Matches(kExpect[i])) {
					Note("Flickit: the game's code at %08X (%s) is not what was expected\n", kExpect[i].address, kExpect[i].what);
					s_ready = -1;
				}
			}
		}
		return s_ready > 0;
	}

	/******************************************************************/
	/*                                                                */
	/*  The folder of the exe, and whole files                        */
	/*                                                                */
	/******************************************************************/

	const char* ExeDir() {
		static char s_dir[MAX_PATH] = "";
		static bool s_asked = false;
		if (!s_asked) {
			s_asked = true;
			const DWORD n = GetModuleFileNameA(NULL, s_dir, MAX_PATH);
			char* slash = n && n < MAX_PATH ? strrchr(s_dir, '\\') : 0;
			if (slash) {
				slash[1] = 0;
			}
			else {
				s_dir[0] = 0;
			}
		}
		return s_dir;
	}

	bool ReadWhole(const char* path, std::vector<uint8_t>* out, uint64_t most) {
		out->clear();
		HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (f == INVALID_HANDLE_VALUE) {
			return false;
		}
		LARGE_INTEGER size;
		bool ok = GetFileSizeEx(f, &size) && size.QuadPart >= 0 && (uint64_t)size.QuadPart <= most && size.QuadPart < 0x7FFFFFFF;
		if (ok && size.QuadPart > 0) {
			out->resize((size_t)size.QuadPart);
			DWORD got = 0;
			ok = ReadFile(f, &(*out)[0], (DWORD)size.QuadPart, &got, NULL) && got == (DWORD)size.QuadPart;
			if (!ok) {
				out->clear();
			}
		}
		CloseHandle(f);
		return ok;
	}

	/******************************************************************/
	/*                                                                */
	/*  Notes                                                         */
	/*                                                                */
	/******************************************************************/

	// flickit_watchdog.txt next to the exe: every line is added to it and the file closed again, so that it is
	// whole whatever happens next. One that has grown past 2 MB is put aside when a run starts, and each run of
	// the game starts with a line saying so and which build it is.
	static char s_notePath[MAX_PATH + 32] = "";
	static int s_noteFile = 0;                        // 1: notes go there, -1: they cannot

	static void Append(const char* text) {
		HANDLE f = CreateFileA(s_notePath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (f == INVALID_HANDLE_VALUE) {
			return;
		}
		DWORD put = 0;
		WriteFile(f, text, (DWORD)strlen(text), &put, NULL);
		CloseHandle(f);
	}

	static void FileLine(const char* text) {
		if (s_noteFile == 0) {
			s_noteFile = -1;
			const char* dir = ExeDir();
			if (dir[0]) {
				Str::Format(s_notePath, sizeof(s_notePath), "%sflickit_watchdog.txt", dir);
				WIN32_FILE_ATTRIBUTE_DATA a;
				if (GetFileAttributesExA(s_notePath, GetFileExInfoStandard, &a) && (a.nFileSizeHigh || a.nFileSizeLow > 2u * 1024u * 1024u)) {
					char old[MAX_PATH + 32];
					Str::Format(old, sizeof(old), "%sflickit_watchdog.old.txt", dir);
					MoveFileExA(s_notePath, old, MOVEFILE_REPLACE_EXISTING);
				}
				SYSTEMTIME t;
				GetLocalTime(&t);
				char head[160];
#ifdef FLICKIT_BUILD
				const char* build = FLICKIT_BUILD;
#else
				const char* build = "a local build";
#endif
				Str::Format(head, sizeof(head), "\r\n=== %04u-%02u-%02u: the first note of this run of the game (Flickit %.12s) ===\r\n", t.wYear, t.wMonth, t.wDay, build);
				s_noteFile = 1;
				Append(head);
			}
		}
		if (s_noteFile > 0) {
			Append(text);
		}
	}

	void NoteV(const char* format, va_list args) {
		char text[1024];
		size_t n = Str::FormatV(text, sizeof(text), format, args);
		while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r')) {
			text[--n] = 0;
		}
		Log::TypedLog(CHN_SDL, "%s\n", text);
		SYSTEMTIME t;
		GetLocalTime(&t);
		char line[1100];
		Str::Format(line, sizeof(line), "%02u:%02u:%02u.%03u %s\r\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, text);
		FileLine(line);
	}

	void Note(const char* format, ...) {
		va_list args;
		va_start(args, format);
		NoteV(format, args);
		va_end(args);
	}

	void NoteLines(const char* const* lines, int count) {
		static char block[12 * 1100];
		SYSTEMTIME t;
		GetLocalTime(&t);
		size_t used = 0;
		block[0] = 0;
		for (int i = 0; i < count; i++) {
			Log::TypedLog(CHN_SDL, "%s\n", lines[i]);
			used += Str::Format(block + used, sizeof(block) - used, "%02u:%02u:%02u.%03u %.1000s\r\n", t.wHour, t.wMinute, t.wSecond,
				t.wMilliseconds, lines[i]);
		}
		if (used) {
			FileLine(block);
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  Scripts, the net, the level, the clock                        */
	/*                                                                */
	/******************************************************************/

	bool RunScriptOf(uint32_t name, void* params, void* object) {
		if (!IsScript(name)) {
			return false;
		}
		RunGameScript(name, params, object, 0);
		return true;
	}

	bool InNet() {
		void* net = *kNetManager;
		return net && InNetGame(net);
	}

	uint32_t LevelScript() {
		const uint8_t* skate = *kSkate;
		return skate ? *(const uint32_t*)(skate + SKATE_LEVEL) : 0;
	}

	LevelKey Level() {
		LevelKey key;
		key.loads = BigWorld::NodeLoads();
		key.nodes = GetGlobalArray(kNodeArray, 0);
		key.count = key.nodes ? *(const uint32_t*)((const uint8_t*)key.nodes + 8) : 0;
		key.level = LevelScript();
		return key;
	}

	double Seconds() {
		static double s_perSecond = 0.0;
		LARGE_INTEGER v;
		if (s_perSecond <= 0.0) {
			QueryPerformanceFrequency(&v);
			s_perSecond = v.QuadPart > 0 ? (double)v.QuadPart : 1000.0;
		}
		QueryPerformanceCounter(&v);
		return (double)v.QuadPart / s_perSecond;
	}

	double FrameTimer::Step() {
		const double now = Seconds();
		const double passed = last > 0.0 ? now - last : 0.0;
		last = now;
		return passed;
	}
}
