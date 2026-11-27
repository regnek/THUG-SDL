// The watch on the game's frames (frames_game.h, frames.h).
//
// The game's thread marks its frames (Boundary, RenderEnd, the mesh draw's tally) and sums them up. A second
// thread, the sampler, stops the game's thread about once a millisecond, reads where it is and copies the top
// of its stack, lets it go again, and counts the place. While it has the game's thread stopped it takes no
// lock and asks Windows for nothing but the thread's registers and that copy: whatever lock the stopped
// thread holds stays out of the way. Every ten seconds the game's thread hands the sampler what the frames came to; the sampler puts
// that and its own counts into lines of text (finding the modules' names: the loader's lock, taken while
// nothing is stopped) and the game's thread writes them to the notes, since the notes are its to write.
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "frames.h"
#include "frames_game.h"
#include "camrel_game.h"
#include "flickit_game.h"
#include "game.h"

#define FLICKIT_SECTION "Flickit"

#ifndef FRAMES_PERIOD_SECONDS
#define FRAMES_PERIOD_SECONDS 10.0      // (tests/frames_windows_test.cpp makes its stretches shorter)
#endif

namespace FramesGame {
	DrawTally g_draw = { 0, 0, 0, 0, 0, 0 };

	static const double kPeriodSeconds = FRAMES_PERIOD_SECONDS;
	static const size_t kScanWords = 4096;          // of the stack, from its top, looked through for a caller
	static const uint32_t kGapUs = 5000000;         // this far into a "frame" the thread is loading or away (Frames::kGapMs)

	/******************************************************************/
	/*  What both threads see                                         */
	/******************************************************************/

	struct Range {
		uintptr_t start, end;
	};
	// The game's and the mod's code (set before the sampler starts)
	static Range s_code[12];
	static int s_codeCount = 0;
	static uintptr_t s_exeBase = 0, s_exeEnd = 0, s_modBase = 0, s_modEnd = 0;

	static HANDLE s_mainThread = NULL;              // the game's thread, to stop and to read
	static volatile uintptr_t s_stackBase = 0;      // its stack: from the limit up to the base
	static volatile uintptr_t s_stackLimit = 0;
	static volatile uint32_t s_frameStartUs = 0;    // when its frame began, in microseconds (they wrap)
	static volatile uint32_t s_longAfterUs = 33400; // this far into a frame, the frame has run long
	static volatile LONG s_sampling = 0;

	// The hand-over: 0 nothing; 1 the game's thread has filled s_asked and asks; 2 the sampler has filled the
	// lines for the game's thread to write
	static volatile LONG s_report = 0;
	struct Asked {
		Frames::Period period;
		double seconds;
		bool tell;              // put it in words (else only start counting anew)
		bool slow;
		double longAfter;       // ms into a frame from which a look counted as one of a frame run long
		bool skaterKnown;
		float pos[3];
		bool driving;
		bool relative;          // the level is drawn relative to the meshes' middles
	};
	static Asked s_asked;
	static char s_lines[8][1000];
	static int s_lineCount = 0;

	static inline bool InCode(uint32_t a) {
		// (nearly every word of a stack is in neither image: two looks, and it is out)
		if (!(a >= s_exeBase && a < s_exeEnd) && !(a >= s_modBase && a < s_modEnd)) {
			return false;
		}
		for (int i = 0; i < s_codeCount; i++) {
			if (a >= s_code[i].start && a < s_code[i].end) {
				return true;
			}
		}
		return false;
	}

	static inline bool Window(uint32_t a, uint8_t* out) {
		for (int i = 0; i < s_codeCount; i++) {
			if (a >= s_code[i].start + 7 && a < s_code[i].end) {
				memcpy(out, (const void*)(uintptr_t)(a - 7), 8);
				return true;
			}
		}
		return false;
	}

	static double s_qpcPerSecond = 0.0;

	static uint32_t NowUs() {
		LARGE_INTEGER q;
		QueryPerformanceCounter(&q);
		return (uint32_t)(uint64_t)((double)q.QuadPart * 1000000.0 / s_qpcPerSecond);
	}

	/******************************************************************/
	/*  The sampler's side                                            */
	/******************************************************************/

	// Where the thread was (64 bytes to a place), and where that was called from (the address a call comes
	// back to), over the whole stretch and in the frames that had run long
	struct Seen {
		Frames::Counts<8192> at;
		Frames::Counts<4096> from;
	};
	static Seen s_all, s_long;
	static uint32_t s_stack[kScanWords];        // the top of the game's thread's stack, as it was at a look

	struct Module {
		uintptr_t base, end;
		char name[40];
	};
	static Module s_modules[64];
	static int s_moduleCount = 0;

	// The module an address is in (found once, then kept), or -1
	static int ModuleOf(uintptr_t a) {
		for (int i = 0; i < s_moduleCount; i++) {
			if (a >= s_modules[i].base && a < s_modules[i].end) {
				return i;
			}
		}
		if (s_moduleCount >= (int)(sizeof(s_modules) / sizeof(s_modules[0]))) {
			return -1;
		}
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQuery((const void*)a, &mbi, sizeof(mbi)) || mbi.Type != MEM_IMAGE || !mbi.AllocationBase) {
			return -1;
		}
		const uint8_t* base = (const uint8_t*)mbi.AllocationBase;
		const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
		if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) {
			return -1;
		}
		const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE) {
			return -1;
		}
		char path[MAX_PATH];
		const DWORD n = GetModuleFileNameA((HMODULE)mbi.AllocationBase, path, sizeof(path));
		if (n == 0 || n >= sizeof(path)) {
			return -1;
		}
		const char* slash = strrchr(path, '\\');
		Module& m = s_modules[s_moduleCount];
		m.base = (uintptr_t)base;
		m.end = m.base + nt->OptionalHeader.SizeOfImage;
		Str::Copy(m.name, sizeof(m.name), slash ? slash + 1 : path);
		if (a < m.base || a >= m.end) {
			return -1;
		}
		return s_moduleCount++;
	}

	// An address in words: the game's own as it is, the mod's and another module's from the module's start
	static void NameOf(uintptr_t a, char* out, size_t size) {
		if (a >= s_exeBase && a < s_exeEnd) {
			Str::Format(out, size, "exe %08X", (unsigned)a);
			return;
		}
		if (a >= s_modBase && a < s_modEnd) {
			Str::Format(out, size, "mod+%X", (unsigned)(a - s_modBase));
			return;
		}
		const int m = ModuleOf(a);
		if (m >= 0) {
			Str::Format(out, size, "%s+%X", s_modules[m].name, (unsigned)(a - s_modules[m].base));
			return;
		}
		Str::Format(out, size, "? %08X", (unsigned)a);
	}

	static void NamePlace(uint32_t key, char* out, size_t size) {
		NameOf((uintptr_t)key << 6, out, size);
	}
	static void NameCaller(uint32_t key, char* out, size_t size) {
		NameOf((uintptr_t)key, out, size);
	}

	// The samples by module, the most first: "41.0% d3d9.dll; 22.5% the game; ..."
	static size_t Modules(const Frames::Counts<8192>& at, char* out, size_t size) {
		enum { kExe = 64, kMod = 65, kOther = 66, kKinds = 67 };
		uint32_t counts[kKinds];
		memset(counts, 0, sizeof(counts));
		at.ForEach([&counts](uint32_t key, uint32_t count) {
			const uintptr_t a = (uintptr_t)key << 6;
			int kind = kOther;
			if (a >= s_exeBase && a < s_exeEnd) {
				kind = kExe;
			}
			else if (a >= s_modBase && a < s_modEnd) {
				kind = kMod;
			}
			else {
				const int m = ModuleOf(a);
				if (m >= 0) {
					kind = m;
				}
			}
			counts[kind] += count;
		});
		size_t used = 0;
		out[0] = 0;
		const uint32_t total = at.Total() - at.Lost();
		for (int shown = 0; shown < 8 && total; shown++) {
			int best = -1;
			for (int k = 0; k < kKinds; k++) {
				if (counts[k] && (best < 0 || counts[k] > counts[best])) {
					best = k;
				}
			}
			if (best < 0) {
				break;
			}
			const char* name = best == kExe ? "the game" : best == kMod ? "the mod" : best == kOther ? "elsewhere" : s_modules[best].name;
			used += Str::Format(out + used, size - used, "%s%.1f%% %s", shown ? "; " : "", 100.0 * counts[best] / total, name);
			counts[best] = 0;
		}
		return used;
	}

	static void Line(const char* format, ...) STR_PRINTF(1, 2);
	static void Line(const char* format, ...) {
		if (s_lineCount >= (int)(sizeof(s_lines) / sizeof(s_lines[0]))) {
			return;
		}
		va_list args;
		va_start(args, format);
		Str::FormatV(s_lines[s_lineCount++], sizeof(s_lines[0]), format, args);
		va_end(args);
	}

	// Three lines for one set of counts: by module, the places, the callers
	static void Tell(const Seen& seen, const char* which) {
		char text[900];
		Modules(seen.at, text, sizeof(text));
		Line("Frames: %s, %u looks at the game's thread, by module: %s", which, seen.at.Total(), text);
		Frames::Entry top[14];
		int n = seen.at.Top(top, 14);
		Frames::FormatTop(text, sizeof(text), top, n, seen.at.Total(), NamePlace);
		Line("Frames: %s, the places it was at most (64 bytes each): %s", which, text);
		n = seen.from.Top(top, 12);
		Frames::FormatTop(text, sizeof(text), top, n, seen.at.Total(), NameCaller);
		Line("Frames: %s, called from (the last call in the game's or the mod's code): %s", which, text);
	}

	// What the game's thread asked for: the stretch in words, and the counts start anew
	static void Report() {
		const Asked& a = s_asked;
		s_lineCount = 0;
		if (a.tell) {
			char text[700], where[120] = "";
			Frames::FormatPeriod(text, sizeof(text), a.period, a.seconds);
			if (a.skaterKnown) {
				Str::Format(where, sizeof(where), "; he was at (%.0f %.0f %.0f)%s", a.pos[0], a.pos[1], a.pos[2], a.driving ? ", driving" : "");
			}
			Line("Frames: %s: %s%s; the level drawn %s", a.slow ? "a slow stretch" : "a stretch to compare with", text, where,
				a.relative ? "relative to the meshes' middles" : "as the game does");
			if (s_all.at.Total() >= 50) {
				Tell(s_all, "all of it");
			}
			if (s_long.at.Total() >= 20) {
				char which[80];
				Str::Format(which, sizeof(which), "past %.0f ms into a frame", a.longAfter);
				Tell(s_long, which);
			}
		}
		s_all.at.Clear();
		s_all.from.Clear();
		s_long.at.Clear();
		s_long.from.Clear();
		InterlockedExchange(&s_report, s_lineCount ? 2 : 0);
	}

	static DWORD WINAPI Sampler(void*) {
		for (;;) {
			Sleep(1);
			if (s_report == 1) {
				Report();
			}
			const HANDLE thread = s_mainThread;
			if (!thread || !g_draw.on) {
				continue;
			}
			const uint32_t began = s_frameStartUs;
			const uint32_t into = NowUs() - began;
			if (into > kGapUs) {
				continue;       // no frame for five seconds: a load, the window away
			}
			if (SuspendThread(thread) == (DWORD)-1) {
				continue;
			}
			// (the game's thread stands still from here: no lock, and nothing asked of Windows but its registers
			// and a copy of the top of its stack, which fails rather than faults if that is not there)
			CONTEXT c;
			c.ContextFlags = CONTEXT_CONTROL;
			uint32_t at = 0;
			SIZE_T copied = 0;
			if (GetThreadContext(thread, &c)) {
				at = (uint32_t)c.Eip;
				const uintptr_t sp = ((uintptr_t)c.Esp + 3) & ~(uintptr_t)3;
				const uintptr_t base = s_stackBase, limit = s_stackLimit;
				if (sp >= limit && sp < base) {
					const size_t bytes = base - sp < sizeof(s_stack) ? base - sp : sizeof(s_stack);
					if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)sp, s_stack, bytes, &copied)) {
						copied = 0;
					}
				}
			}
			ResumeThread(thread);
			if (!at) {
				continue;
			}
			const uint32_t from = Frames::Caller(s_stack, copied / 4, InCode, Window);
			s_all.at.Add(at >> 6);
			if (from) {
				s_all.from.Add(from);
			}
			if (into > s_longAfterUs) {
				s_long.at.Add(at >> 6);
				if (from) {
					s_long.from.Add(from);
				}
			}
		}
	}

	/******************************************************************/
	/*  The game's thread's side                                      */
	/******************************************************************/

	static bool s_wanted = true;
	static bool s_started = false;
	static bool s_failed = false;
	static DWORD s_mainId = 0;
	static Frames::Pace s_pace;
	static LONGLONG s_lastQpc = 0, s_periodQpc = 0;
	static uint64_t s_lastTsc = 0, s_renderEndTsc = 0;
	static Frames::Teller s_teller;
	static bool s_switched = false;

	static void AddCode(HMODULE module, uintptr_t* start, uintptr_t* end) {
		const uint8_t* base = (const uint8_t*)module;
		const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
		if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE) {
			return;
		}
		const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE) {
			return;
		}
		*start = (uintptr_t)base;
		*end = (uintptr_t)base + nt->OptionalHeader.SizeOfImage;
		const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
		for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; i++) {
			if ((section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) && s_codeCount < (int)(sizeof(s_code) / sizeof(s_code[0]))) {
				const uintptr_t from = (uintptr_t)base + section[i].VirtualAddress;
				const uintptr_t size = section[i].Misc.VirtualSize ? section[i].Misc.VirtualSize : section[i].SizeOfRawData;
				if (size && from + size <= *end) {
					s_code[s_codeCount].start = from;
					s_code[s_codeCount].end = from + size;
					s_codeCount++;
				}
			}
		}
	}

	// At the first frame: the code to look for callers in, the game's thread, the sampler
	static void Start() {
		s_started = true;
		LARGE_INTEGER f;
		if (!QueryPerformanceFrequency(&f) || f.QuadPart <= 0) {
			s_failed = true;
			return;
		}
		s_qpcPerSecond = (double)f.QuadPart;
		AddCode(GetModuleHandleA(NULL), &s_exeBase, &s_exeEnd);
		HMODULE mod = NULL;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&Start, &mod)) {
			AddCode(mod, &s_modBase, &s_modEnd);
		}
		s_mainId = GetCurrentThreadId();
		s_mainThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, s_mainId);
		HANDLE sampler = s_mainThread && s_codeCount ? CreateThread(NULL, 0, Sampler, NULL, 0, NULL) : NULL;
		if (sampler) {
			SetThreadPriority(sampler, THREAD_PRIORITY_ABOVE_NORMAL);
			CloseHandle(sampler);
			InterlockedExchange(&s_sampling, 1);
		}
		Game::Note("Frames: watching the frames%s; a stretch of %.0f seconds that ran slow is written here, %d of them at most ([Flickit] FrameWatch=0 turns the watch off)\n",
			sampler ? ", and where the game's thread is about every millisecond" : " (the thread that looks where the game's thread is could not be started: the frames only)",
			kPeriodSeconds, (int)Frames::Teller::kMost);
	}

	// The stretch is over: what it came to, and whether it is told
	static void EndPeriod(LONGLONG now) {
		const Frames::Period p = s_pace.Sum();
		const double seconds = (double)(now - s_periodQpc) / s_qpcPerSecond;
		s_pace.Reset();
		s_periodQpc = now;
		const double longAfter = s_longAfterUs / 1000.0;
		if (p.frames >= (uint32_t)Frames::Teller::kMinFrames) {
			s_longAfterUs = (uint32_t)(Frames::LongAfter(p) * 1000.0);
		}
		if (s_sampling && s_report != 0) {
			return;             // the last one is not written yet: this one goes untold
		}
		const int told = s_teller.Tell(p);
		const bool tell = told != Frames::Teller::NO, slow = told == Frames::Teller::SLOW;
		if (s_sampling) {
			Asked& a = s_asked;
			a.period = p;
			a.seconds = seconds;
			a.tell = tell;
			a.slow = slow;
			a.longAfter = longAfter;
			FlickitGame::Whereabouts w;
			memset(&w, 0, sizeof(w));
			a.skaterKnown = FlickitGame::SkaterWhereabouts(w) && GetTickCount() - w.tick < 2000;
			memcpy(a.pos, w.pos, sizeof(a.pos));
			a.driving = w.driving;
			a.relative = CamRelGame::On();
			InterlockedExchange(&s_report, 1);
		}
		else if (tell) {
			char text[700];
			Frames::FormatPeriod(text, sizeof(text), p, seconds);
			Game::Note("Frames: %s: %s\n", slow ? "a slow stretch" : "a stretch to compare with", text);
		}
		if (slow && s_teller.Spent()) {
			Game::Note("Frames: the slow stretch of now is the last one written in this run of the game\n");
		}
	}

	void Configure(const char* configFile) {
		s_wanted = GetPrivateProfileIntA(FLICKIT_SECTION, "FrameWatch", 1, configFile) != 0;
		if (!s_wanted) {
			g_draw.on = 0;      // (the sampler looks no more; a watch that was running stops counting)
		}
	}

	void Boundary() {
		if (!s_wanted || s_failed) {
			return;
		}
		const uint64_t tsc = __rdtsc();
		if (!s_started) {
			Start();
			if (s_failed) {
				return;
			}
		}
		LARGE_INTEGER q;
		QueryPerformanceCounter(&q);
		if (GetCurrentThreadId() != s_mainId) {
			return;             // the pad read from another thread: not the frames this watches
		}
		const NT_TIB* tib = (const NT_TIB*)NtCurrentTeb();
		s_stackBase = (uintptr_t)tib->StackBase;
		s_stackLimit = (uintptr_t)tib->StackLimit;
		// what the sampler has put in words: to the notes, which are this thread's to write
		if (s_report == 2) {
			const char* lines[sizeof(s_lines) / sizeof(s_lines[0])];
			for (int i = 0; i < s_lineCount; i++) {
				lines[i] = s_lines[i];
			}
			Game::NoteLines(lines, s_lineCount);
			InterlockedExchange(&s_report, 0);
		}
		if (s_switched) {
			// something was switched in the frame that ends here: that frame is in no stretch, and a new stretch
			// starts now (the sampler's counts too), once the sampler is done with the last one
			if (!s_sampling || s_report == 0) {
				s_switched = false;
				s_pace.Reset();
				s_periodQpc = q.QuadPart;
				s_teller.Next();
				if (s_sampling) {
					s_asked.tell = false;
					InterlockedExchange(&s_report, 1);
				}
			}
		}
		else if (s_lastQpc && tsc > s_lastTsc) {
			Frames::FrameTimes f;
			f.whole = (double)(q.QuadPart - s_lastQpc) * 1000.0 / s_qpcPerSecond;
			f.before = f.drawing = f.after = f.inDraw = f.ownDraw = -1.0;
			f.draws = g_draw.draws;
			f.shifted = g_draw.shifted;
			// (the processor's ticks in ms, by this frame's own length)
			const double ms = f.whole / (double)(tsc - s_lastTsc);
			if (g_draw.draws && g_draw.first > s_lastTsc && s_renderEndTsc > g_draw.first && tsc > s_renderEndTsc) {
				f.before = (double)(g_draw.first - s_lastTsc) * ms;
				f.drawing = (double)(s_renderEndTsc - g_draw.first) * ms;
				f.after = (double)(tsc - s_renderEndTsc) * ms;
				f.inDraw = (double)g_draw.inCall * ms;
				f.ownDraw = (double)g_draw.own * ms;
			}
			s_pace.Frame(f);
			if ((double)(q.QuadPart - s_periodQpc) >= kPeriodSeconds * s_qpcPerSecond) {
				EndPeriod(q.QuadPart);
			}
		}
		else {
			s_periodQpc = q.QuadPart;
		}
		s_lastQpc = q.QuadPart;
		s_lastTsc = tsc;
		s_renderEndTsc = 0;
		g_draw.draws = 0;
		g_draw.shifted = 0;
		g_draw.first = 0;
		g_draw.inCall = 0;
		g_draw.own = 0;
		g_draw.on = 1;
		s_frameStartUs = (uint32_t)(uint64_t)((double)q.QuadPart * 1000000.0 / s_qpcPerSecond);
	}

	void RenderEnd() {
		if (g_draw.on) {
			s_renderEndTsc = __rdtsc();
		}
	}

	void Switching() {
		if (!s_wanted || s_failed || !s_started || !s_lastQpc || GetCurrentThreadId() != s_mainId) {
			return;
		}
		if (!s_switched && (!s_sampling || s_report == 0)) {
			LARGE_INTEGER q;
			QueryPerformanceCounter(&q);
			s_teller.Next();
			EndPeriod(q.QuadPart);
		}
		s_switched = true;
	}
}
