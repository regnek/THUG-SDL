#include <windows.h>
#include <tlhelp32.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "guard.h"
#include "guard_game.h"
#include "flickit_game.h"

#define FLICKIT_SECTION "Flickit"

#ifndef GUARD_TEST
#include "Logger/Logger.h"
#else
// tests/guard_windows_test.cpp: this same code, watching words of that program's instead of the game's
extern volatile uint32_t g_guardTestWord;
extern volatile uint32_t g_guardTestWord2;
extern const uint8_t* const g_guardTestScript;
void GuardTestLog(const char* text);
#endif

namespace GuardGame {
	// A word watched: where it is, what the object it points to has first (its table of functions), the game's
	// own writes to it (by the instruction after each: a write breakpoint stops after the write), its name
	struct Word {
		volatile uint32_t* address;
		uint32_t table;
		const uint32_t* gamesOwn;
		int ownCount;
		const char* name;
	};
#ifndef GUARD_TEST
	// The object manager pointer (its constructor, 0x00420560, puts the table 0x0062E8F8 there): made at
	// 0x00420EB1 / 0x00420EB9, gone at 0x0041F955. And 8 bytes before it the component manager pointer (its
	// constructor 0x0041CFF0, table 0x0062E668): made at 0x0041D781 / 0x0041D789 (Instance, 0x0041D750), never
	// gone. The same two 16-bit numbers (58, 59) have been written over the one (the crash at 0x0041FBB3) and over
	// the other (at 0x0041CBC0, from 0x004515E2). The refcounts after each (+4) change at every Instance(): not
	// watched.
	static const uint32_t kObjectsOwn[] = { 0x00420EB6, 0x00420EBE, 0x0041F95F };
	static const uint32_t kComponentsOwn[] = { 0x0041D786, 0x0041D78E };
	static const Word kWords[] = {
		{ (volatile uint32_t*)0x0069B6C0, 0x0062E8F8, kObjectsOwn, 3, "object manager pointer (0x0069B6C0)" },
		{ (volatile uint32_t*)0x0069B6B8, 0x0062E668, kComponentsOwn, 2, "component manager pointer (0x0069B6B8)" },
	};
	// The game's code
	static const uint32_t kCodeStart = 0x00401000;
	static const uint32_t kCodeEnd = 0x0062C3EF;
	// The script running (the crash box shows it too): its name at +0xBC, its caller's at +0x60
	static const uint8_t* const* const kScript = (const uint8_t* const*)0x0069842C;
#else
	static const uint32_t kNoneOwn[] = { 0 };
	static const Word kWords[] = {
		{ &g_guardTestWord, 0x7AB1E000, kNoneOwn, 1, "object manager pointer (the test's)" },
		{ &g_guardTestWord2, 0x7AB1E004, kNoneOwn, 1, "component manager pointer (the test's)" },
	};
	static const uint32_t kCodeStart = 0;
	static const uint32_t kCodeEnd = 0;
	static const uint8_t* const* const kScript = &g_guardTestScript;
#endif

	enum { kWords_ = sizeof(kWords) / sizeof(kWords[0]) };
	static volatile uint32_t* const kManager = kWords[0].address;     // the first: the guard starts once it is there

	enum State { kOff, kWaiting, kOn, kBroken };
	static State s_state = kOff;
	static volatile uint32_t s_good[kWords_];   // what each word should be
	static PVOID s_handler = 0;
	static uint32_t s_dllStart = 0, s_dllEnd = 0;
	static bool s_works = false;                // the test write was caught

	// Writes of our own: the test, and putting it back (by the thread that does it)
	static volatile DWORD s_testing = 0;
	static volatile LONG s_testSeen = 0;
	static volatile DWORD s_puttingBack = 0;

	// The passes over the threads, and threads that started since the last one
	static volatile LONG s_newThread = 0;
	static volatile DWORD s_helper = 0;         // the thread of the pass (its own start is not a new thread)
	static volatile LONG s_passBusy = 0;
	static volatile LONG s_passDone = 0;
	static DWORD s_lastPass = 0;
	static int s_threadsOn = 0;

	// What the breakpoint caught, for the next frame to write down
	struct Note {
		volatile LONG seq;      // which note this is, once it is filled in (-1 while it is being)
		int word;               // which word (kWords)
		int action;             // Guard::Action
		bool gamesOwn;
		bool full;              // with the registers, the code and the stack
		uint32_t eip;           // the instruction after the write
		uint32_t now;
		uint32_t before;
		uint32_t thread;
		uint32_t tick;
		uint32_t script, caller;
		uint32_t regs[8];       // eax ebx ecx edx esi edi ebp esp
		uint8_t code[24];       // from 16 bytes before eip to 8 after it
		bool codeKnown;
		uint32_t calls[24];     // words on the stack that are where a call returns to, nearest first
		int callCount;
	};
	static const int kNotes = 16;
	static Note s_notes[kNotes];
	static volatile LONG s_taken = 0;           // notes taken (the ring goes round)
	static LONG s_written = 0;                  // notes written down or lost
	static const LONG kFull = 40;               // past this many, a note is only where and what
	static int s_reports = 0;                   // written down in full
	static const int kReports = 24;
	static LONG s_lost = 0;
	static volatile LONG s_putBacks = 0;

	/******************************************************************/
	/*                                                                */
	/*  Memory, without touching what is not there                    */
	/******************************************************************/

	static bool Readable(uint32_t address, uint32_t n) {
		const uint32_t end = address + n;
		if (end < address || address < 0x10000) {
			return false;
		}
		uint32_t a = address;
		while (a < end) {
			MEMORY_BASIC_INFORMATION m;
			if (!VirtualQuery((LPCVOID)(uintptr_t)a, &m, sizeof(m)) || m.State != MEM_COMMIT) {
				return false;
			}
			const DWORD p = m.Protect;
			const DWORD reads = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
			if ((p & (PAGE_NOACCESS | PAGE_GUARD)) || !(p & reads)) {
				return false;
			}
			const uint32_t next = (uint32_t)(uintptr_t)m.BaseAddress + (uint32_t)m.RegionSize;
			if (next <= a) {
				return false;   // the top of the address space
			}
			a = next;
		}
		return true;
	}

	static bool IsManager(int word, uint32_t v) {
		return v && !(v & 3) && Readable(v, 4) && *(const uint32_t*)(uintptr_t)v == kWords[word].table;
	}

	static bool InGameCode(uint32_t a) {
		return a >= kCodeStart && a < kCodeEnd;
	}

	static bool InOurCode(uint32_t a) {
		return a >= s_dllStart && a < s_dllEnd;
	}

	// Is a word of the stack where a call returns to? In the game's code, the DLL's, or any other code
	static bool CallReturn(uint32_t v) {
		if (v < 0x10007) {
			return false;
		}
		if (InGameCode(v) || InOurCode(v)) {
			if (!(InGameCode(v - 7) || InOurCode(v - 7))) {
				return false;
			}
		}
		else {
			MEMORY_BASIC_INFORMATION m;
			const DWORD runs = PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
			if (!VirtualQuery((LPCVOID)(uintptr_t)v, &m, sizeof(m)) || m.State != MEM_COMMIT || !(m.Protect & runs) || (m.Protect & PAGE_GUARD)) {
				return false;
			}
			if (v - 7 < (uint32_t)(uintptr_t)m.BaseAddress && !Readable(v - 7, 7)) {
				return false;
			}
		}
		return Guard::AfterCall((const uint8_t*)(uintptr_t)(v - 7));
	}

	/******************************************************************/
	/*                                                                */
	/*  The breakpoint going off                                      */
	/******************************************************************/

	static void TakeNote(int word, const CONTEXT* c, Guard::Action action, bool own, uint32_t now, uint32_t before) {
		const LONG n = InterlockedIncrement(&s_taken) - 1;
		Note& note = s_notes[n % kNotes];
		note.seq = -1;
		note.word = word;
		note.action = (int)action;
		note.gamesOwn = own;
		note.eip = c->Eip;
		note.now = now;
		note.before = before;
		note.thread = GetCurrentThreadId();
		note.tick = GetTickCount();
		note.script = note.caller = 0;
		note.full = !own && n < kFull;
		note.codeKnown = false;
		note.callCount = 0;
		if (note.full) {
			const uint8_t* s = *kScript;
			if (s && Readable((uint32_t)(uintptr_t)s, 0xC0)) {
				note.script = *(const uint32_t*)(s + 0xBC);
				note.caller = *(const uint32_t*)(s + 0x60);
			}
			note.regs[0] = c->Eax;
			note.regs[1] = c->Ebx;
			note.regs[2] = c->Ecx;
			note.regs[3] = c->Edx;
			note.regs[4] = c->Esi;
			note.regs[5] = c->Edi;
			note.regs[6] = c->Ebp;
			note.regs[7] = c->Esp;
			if (Readable(c->Eip - 16, sizeof(note.code))) {
				memcpy(note.code, (const void*)(uintptr_t)(c->Eip - 16), sizeof(note.code));
				note.codeKnown = true;
			}
			// The thread that wrote is this one, and the stack it wrote with is ours, above where we are on it
			const NT_TIB* tib = (const NT_TIB*)NtCurrentTeb();
			const uint32_t base = (uint32_t)(uintptr_t)tib->StackBase;
			const uint32_t limit = (uint32_t)(uintptr_t)tib->StackLimit;
			const uint32_t sp = c->Esp;
			if (sp >= limit && sp < base && !(sp & 3)) {
				const uint32_t end = sp + 0x1000 < base ? sp + 0x1000 : base;
				for (uint32_t p = sp; p + 4 <= end && note.callCount < (int)(sizeof(note.calls) / sizeof(note.calls[0])); p += 4) {
					const uint32_t v = *(const uint32_t*)(uintptr_t)p;
					if (CallReturn(v)) {
						note.calls[note.callCount++] = v;
					}
				}
			}
		}
		InterlockedExchange(&note.seq, n);
	}

	// A write to one of the words, caught: the game's own, another manager, or something else (put back)
	static void Caught(int i, const CONTEXT* c, uint32_t now) {
		bool own = false;
		for (int k = 0; k < kWords[i].ownCount; k++) {
			own = own || c->Eip == kWords[i].gamesOwn[k];
		}
		const uint32_t before = s_good[i];
		Guard::Write w;
		w.seen = true;
		w.gamesOwn = own;
		w.now = now;
		w.before = before;
		w.nowIsManager = !own && IsManager(i, now);
		w.beforeIsManager = !own && IsManager(i, before);
		const Guard::Action action = Guard::Decide(w);
		if (action == Guard::kAccept) {
			s_good[i] = now;
		}
		else if (action == Guard::kPutBack) {
			s_puttingBack = GetCurrentThreadId();
			*kWords[i].address = before;
			s_puttingBack = 0;
			InterlockedIncrement(&s_putBacks);
		}
		TakeNote(i, c, action, own, now, before);
	}

	static LONG CALLBACK OnException(EXCEPTION_POINTERS* info) {
		const DWORD code = info->ExceptionRecord->ExceptionCode;
		if (code != EXCEPTION_SINGLE_STEP && code != 0x4000001E) {     // and STATUS_WX86_SINGLE_STEP, to be safe
			return EXCEPTION_CONTINUE_SEARCH;
		}
		CONTEXT* c = info->ContextRecord;
		const DWORD dr[4] = { c->Dr0, c->Dr1, c->Dr2, c->Dr3 };
		bool fired[kWords_];
		uint32_t now[kWords_];
		bool any = false;
		for (int i = 0; i < kWords_; i++) {
			now[i] = *kWords[i].address;
			const bool ours = Guard::HasBreakpoint(dr[i], c->Dr7, (uint32_t)(uintptr_t)kWords[i].address, i);
			fired[i] = (c->Dr6 & (1u << i)) != 0 || (ours && now[i] != s_good[i]);
			any = any || fired[i];
		}
		if (!any) {
			return EXCEPTION_CONTINUE_SEARCH;
		}
		c->Dr6 &= ~0x0000400Fu;                 // breakpoints 0-3 and the single step: seen
		const DWORD me = GetCurrentThreadId();
		if (s_testing == me) {
			s_testSeen = 1;
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		if (s_puttingBack == me) {
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		for (int i = 0; i < kWords_; i++) {
			if (fired[i]) {
				Caught(i, c, now[i]);
			}
		}
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	/******************************************************************/
	/*                                                                */
	/*  Putting the breakpoint on every thread                        */
	/******************************************************************/

	// A thread cannot change its own debug registers while it runs, so another one does it: it stops each
	// thread of the game in turn, sets them, and lets it go on. Nothing in between asks for memory or a lock,
	// so a thread stopped while it holds one does not matter. The first pass the game's thread waits for (the
	// test write comes after it); the later ones it does not, so that they cost the game no frame.
	struct Pass {
		int threads;            // of the game, but for the one doing this
		int added;              // given the breakpoint now
		int had;                // had it already
		int failed;
		DWORD error;            // the first error
	};
	static Pass s_pass;

	static DWORD WINAPI PassThreads(LPVOID) {
		Pass p;
		memset(&p, 0, sizeof(p));
		const DWORD me = GetCurrentThreadId();
		const DWORD process = GetCurrentProcessId();
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (snap == INVALID_HANDLE_VALUE) {
			p.error = GetLastError();
			s_pass = p;
			InterlockedExchange(&s_passDone, 1);
			return 0;
		}
		THREADENTRY32 t;
		t.dwSize = sizeof(t);
		for (BOOL more = Thread32First(snap, &t); more; t.dwSize = sizeof(t), more = Thread32Next(snap, &t)) {
			if (t.dwSize < FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(t.th32OwnerProcessID)
				|| t.th32OwnerProcessID != process || t.th32ThreadID == me) {
				continue;
			}
			p.threads++;
			HANDLE h = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, t.th32ThreadID);
			if (!h) {
				p.failed++;
				p.error = p.error ? p.error : GetLastError();
				continue;
			}
			if (SuspendThread(h) == (DWORD)-1) {
				p.failed++;
				p.error = p.error ? p.error : GetLastError();
				CloseHandle(h);
				continue;
			}
			CONTEXT c;
			memset(&c, 0, sizeof(c));
			c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
			bool ok = GetThreadContext(h, &c) != 0;
			const DWORD a0 = (DWORD)(uintptr_t)kWords[0].address;
			const DWORD a1 = kWords_ > 1 ? (DWORD)(uintptr_t)kWords[1].address : 0;
			if (ok && Guard::HasBreakpoint(c.Dr0, c.Dr7, a0, 0) && (kWords_ < 2 || Guard::HasBreakpoint(c.Dr1, c.Dr7, a1, 1))) {
				p.had++;
			}
			else if (ok) {
				c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
				c.Dr0 = a0;
				c.Dr7 = Guard::WithBreakpoint(c.Dr7, 0);
				if (kWords_ > 1) {
					c.Dr1 = a1;
					c.Dr7 = Guard::WithBreakpoint(c.Dr7, 1);
				}
				ok = SetThreadContext(h, &c) != 0;
				if (ok) {
					p.added++;
				}
			}
			if (!ok) {
				p.failed++;
				p.error = p.error ? p.error : GetLastError();
			}
			ResumeThread(h);
			CloseHandle(h);
		}
		CloseHandle(snap);
		s_pass = p;
		InterlockedExchange(&s_passDone, 1);
		return 0;
	}

	// Starts a pass; with wait, until it is done (at most 5 seconds). The thread is made stopped and its id
	// kept before it goes, so that its own start (DllMain) is known for what it is.
	static bool StartPass(bool wait) {
		s_passBusy = 1;
		s_passDone = 0;
		s_lastPass = GetTickCount();
		DWORD id = 0;
		HANDLE h = CreateThread(NULL, 0, PassThreads, NULL, CREATE_SUSPENDED, &id);
		if (!h) {
			s_passBusy = 0;
			return false;
		}
		s_helper = id;
		ResumeThread(h);
		const DWORD waited = wait ? WaitForSingleObject(h, 5000) : WAIT_OBJECT_0;
		CloseHandle(h);
		return waited == WAIT_OBJECT_0;
	}

	/******************************************************************/
	/*                                                                */
	/*  The notes                                                     */
	/******************************************************************/

	// Where an address is: the game's code as it is, anything else as a module and how far into it
	static void Where(uint32_t a, char* out, size_t size) {
		if (InGameCode(a)) {
			sprintf_s(out, size, "%08X", a);
			return;
		}
		HMODULE m = 0;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(uintptr_t)a, &m) && m) {
			char path[MAX_PATH];
			const DWORD n = GetModuleFileNameA(m, path, MAX_PATH);
			const char* name = n && n < MAX_PATH ? strrchr(path, '\\') : 0;
			sprintf_s(out, size, "%s+%X", name ? name + 1 : n && n < MAX_PATH ? path : "?", a - (uint32_t)(uintptr_t)m);
			return;
		}
		sprintf_s(out, size, "%08X (in no module)", a);
	}

	static void Line(const char* format, ...) {
		char text[1024];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		text[sizeof(text) - 1] = 0;
		FlickitGame::WatchNote(text);
	}

	static const char* Skater() {
		static char text[128];
		FlickitGame::Whereabouts w;
		if (!FlickitGame::SkaterWhereabouts(w)) {
			return "the skater not seen yet";
		}
		sprintf_s(text, sizeof(text), "the skater %s at (%.0f %.0f %.0f), %u ms before this note", w.driving ? "driving" : "not driving",
			w.pos[0], w.pos[1], w.pos[2], GetTickCount() - w.tick);
		return text;
	}

	static void Report(const Note& n) {
		char where[MAX_PATH + 32];
		Where(n.eip, where, sizeof(where));
		const char* name = kWords[n.word].name;
		if (n.gamesOwn) {
			if (n.now) {
				Line("Guard: the game set its %s to 0x%08X\n", name, n.now);
			}
			else {
				Line("Guard: the game cleared its %s (it was 0x%08X)\n", name, n.before);
			}
			return;
		}
		const LONG count = (LONG)s_reports + 1;
		if (s_reports >= kReports || !n.full) {
			if (s_reports == kReports) {
				Line("Guard: more writes over the watched pointers: from here on only one in a hundred is written down\n");
				s_reports++;
			}
			static LONG s_more = 0;
			if (s_more++ % 100 == 0) {
				Line("Guard: the %s written over again with 0x%08X, before %s (%ld put back in all)\n", name, n.now, where, s_putBacks);
			}
			return;
		}
		s_reports++;
		const char* done = n.action == Guard::kPutBack ? "put back"
			: n.action == Guard::kAccept ? "it points to an object manager, so it is taken as the one now"
			: n.now == n.before ? "it is what it was" : "there was nothing to put back";
		Line("Guard: CAUGHT (%ld): 0x%08X written over the game's %s (0x%08X) by the instruction before %s, in thread %u; %s\n",
			count, n.now, name, n.before, where, n.thread, done);
		Line("Guard:   %s; script %08X from %08X\n", Skater(), n.script, n.caller);
		Line("Guard:   eax %08X ebx %08X ecx %08X edx %08X esi %08X edi %08X ebp %08X esp %08X\n",
			n.regs[0], n.regs[1], n.regs[2], n.regs[3], n.regs[4], n.regs[5], n.regs[6], n.regs[7]);
		if (n.codeKnown) {
			char before[16 * 3 + 1], after[8 * 3 + 1];
			for (int i = 0; i < 16; i++) {
				sprintf_s(before + 3 * i, sizeof(before) - 3 * i, "%02X ", n.code[i]);
			}
			for (int i = 0; i < 8; i++) {
				sprintf_s(after + 3 * i, sizeof(after) - 3 * i, "%02X ", n.code[16 + i]);
			}
			Line("Guard:   the code: %s| %s(the write ends at the bar)\n", before, after);
		}
		char calls[24 * (MAX_PATH + 2)];
		calls[0] = 0;
		size_t used = 0;
		for (int i = 0; i < n.callCount && used + 64 < sizeof(calls); i++) {
			char one[MAX_PATH + 32];
			Where(n.calls[i], one, sizeof(one));
			const int put = sprintf_s(calls + used, sizeof(calls) - used, "%s%s", i ? " " : "", one);
			if (put < 0) {
				break;
			}
			used += (size_t)put;
		}
		Line("Guard:   returns on the stack, nearest first: %s\n", n.callCount ? calls : "none found");
	}

	static void WriteNotes() {
		while (s_written < s_taken) {
			const LONG i = s_written;
			const Note& n = s_notes[i % kNotes];
			const LONG seq = n.seq;
			if (seq != i) {
				if (seq > i) {
					s_lost++;
					s_written++;
					continue;
				}
				break;          // still being filled in
			}
			Report(n);
			s_written++;
		}
		static LONG s_lostTold = 0;
		if (s_lost != s_lostTold) {
			Line("Guard: %ld notes were written over before they could be written down (too many in one frame)\n", s_lost - s_lostTold);
			s_lostTold = s_lost;
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  Starting, and once a frame                                    */
	/******************************************************************/

	// What a pass did, once it is done
	static void Tell(bool first) {
		const Pass p = s_pass;
		s_passDone = 0;
		s_passBusy = 0;
		s_threadsOn = p.had + p.added;
		if (!p.threads) {
			if (first) {
				Line("Guard: the game's threads could not be listed (error %lu)\n", p.error);
			}
			return;
		}
		if (first) {
			Line("Guard: the breakpoint is on in %d of the game's %d threads%s\n", p.added + p.had, p.threads,
				p.failed ? " (the rest could not be given it)" : "");
		}
		else if (p.added) {
			Line("Guard: the breakpoint is on in %d more thread%s (%d in all)\n", p.added, p.added == 1 ? "" : "s", s_threadsOn);
		}
		if (p.failed && (first || p.added)) {
			Line("Guard: %d threads could not be given it (error %lu)\n", p.failed, p.error);
		}
	}

	static void Start(uint32_t now) {
		for (int i = 0; i < kWords_; i++) {
			s_good[i] = *kWords[i].address;
		}
		s_good[0] = now;
		HMODULE self = 0;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&Frame, &self) && self) {
			const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)self;
			const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)((const uint8_t*)self + dos->e_lfanew);
			s_dllStart = (uint32_t)(uintptr_t)self;
			s_dllEnd = s_dllStart + nt->OptionalHeader.SizeOfImage;
		}
		// First in line, ahead of THUG-SDL's own handler (made when the DLL loaded), which takes a single step
		// for nothing and goes on
		s_handler = AddVectoredExceptionHandler(1, OnException);
		if (!s_handler) {
			Line("Guard: no exception handler could be added; off\n");
			s_state = kBroken;
			return;
		}
		s_state = kOn;
		if (!StartPass(true) || !s_passDone) {
			Line("Guard: the pass that puts the breakpoints on the game's threads did not finish; the pointers are only looked at once a frame from here on\n");
			s_state = kBroken;
			return;
		}
		Tell(true);
		// A test for each word: written with what it is. Its breakpoint should go off for it, here.
		int caught = 0;
		for (int i = 0; i < kWords_; i++) {
			s_testSeen = 0;
			s_testing = GetCurrentThreadId();
			*kWords[i].address = s_good[i];
			s_testing = 0;
			caught += s_testSeen ? 1 : 0;
			if (!s_testSeen) {
				Line("Guard: the breakpoint on the %s did not go off for a test write\n", kWords[i].name);
			}
		}
		s_works = caught == kWords_;
		if (s_works) {
			Line("Guard: on: the game's object manager pointer (0x0069B6C0, now 0x%08X) and component manager pointer (0x0069B6B8, now 0x%08X) "
				"have write breakpoints; a test write to each was caught. A write from anywhere but the game's own places is written down here and put back\n",
				s_good[0], kWords_ > 1 ? s_good[1] : 0);
		}
		else {
			Line("Guard: the breakpoints did not go off for every test write: this system does not give the game all its breakpoints. "
				"The pointers are still looked at once a frame and put back if they have become something else\n");
		}
	}

	void AtStartup(const char* configFile) {
		const bool on = GetPrivateProfileIntA(FLICKIT_SECTION, "Guard", 1, configFile) != 0;
		s_state = on ? kWaiting : kOff;
		const char* text = on ? "Guard: on (the game's object manager and component manager pointers get write breakpoints once the game has made them; its notes also go to flickit_watchdog.txt)\n"
			: "Guard: off\n";
#ifndef GUARD_TEST
		Log::TypedLog(CHN_SDL, "%s", text);
#else
		GuardTestLog(text);
#endif
	}

	void ThreadStarted() {
		if (GetCurrentThreadId() != s_helper) {
			s_newThread = 1;
		}
	}

	void Frame() {
		if (s_state == kOff) {
			return;
		}
		WriteNotes();
		if (s_state == kWaiting) {
			const uint32_t first = *kManager;
			if (IsManager(0, first)) {
				Start(first);
			}
			return;
		}
		for (int i = 0; i < kWords_; i++) {
			const uint32_t now = *kWords[i].address;
			if (now == s_good[i]) {
				continue;
			}
			// Changed, and the breakpoint did not see it
			const uint32_t before = s_good[i];
			Guard::Write w;
			w.seen = false;
			w.gamesOwn = false;
			w.now = now;
			w.before = before;
			w.nowIsManager = IsManager(i, now);
			w.beforeIsManager = IsManager(i, before);
			const Guard::Action action = Guard::Decide(w);
			if (action == Guard::kPutBack) {
				s_puttingBack = GetCurrentThreadId();
				*kWords[i].address = before;
				s_puttingBack = 0;
				InterlockedIncrement(&s_putBacks);
			}
			else {
				s_good[i] = now;    // taken, or nothing to go back to
			}
			static int s_unseen = 0;
			if (s_unseen++ < kReports) {
				Line("Guard: the %s was 0x%08X at the look once a frame, not 0x%08X, and the breakpoint did not see it written%s; %s. %s\n",
					kWords[i].name, now, before, s_works ? " (a thread it is not on, or the system)" : "",
					action == Guard::kPutBack ? "put back" : action == Guard::kAccept ? "taken as the one now" : "nothing to put back", Skater());
			}
		}
		if (s_state != kOn) {
			return;
		}
		if (s_passDone) {
			Tell(false);
		}
		const DWORD tick = GetTickCount();
		if (!s_passBusy && ((s_newThread && tick - s_lastPass >= 500) || tick - s_lastPass >= 10000)) {
			s_newThread = 0;
			if (!StartPass(false)) {
				Line("Guard: no thread could be made for a pass over the game's threads; the pointer is only looked at once a frame from here on\n");
				s_state = kBroken;
			}
		}
	}
}
