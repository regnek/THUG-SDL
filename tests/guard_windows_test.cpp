// The guard on Windows itself, outside the game: src/guard_game.cpp as it is, watching a word of this program's
// instead of the game's pointer (GUARD_TEST). A 32-bit program like the game, so that it runs the way the game
// does on a 64-bit Windows. CMake builds it as guard_windows_test; the CI runs it.
//
// It writes over the word from this thread, from a thread that was there before the guard started, from one
// that starts later, and from one the breakpoint is not on yet, and looks at what the guard did each time.
#define GUARD_TEST
#define NOMINMAX
#include "guard_game.cpp"

#include <string>
#include <vector>

#ifdef _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

volatile uint32_t g_guardTestWord = 0;
const uint8_t* const g_guardTestScript = 0;

static std::vector<std::string> s_lines;

void GuardTestLog(const char* text) {
	printf("%s", text);
}

namespace FlickitGame {
	void WatchNote(const char* text) {
		printf("%s", text);
		s_lines.push_back(text);
	}

	bool SkaterWhereabouts(Whereabouts&) {
		return false;
	}
}

// Objects the guard takes for a manager, or not: by their first word
struct Fake {
	uint32_t table;
	uint32_t rest[3];
};
static Fake s_manager = { 0x7AB1E000, { 1, 2, 3 } };
static Fake s_other = { 0x7AB1E000, { 4, 5, 6 } };
static Fake s_notOne = { 0x12345678, { 7, 8, 9 } };

static int s_checks = 0;
static int s_failed = 0;

static void Check(bool ok, const char* what) {
	s_checks++;
	printf("%s: %s\n", ok ? "ok" : "FAILED", what);
	if (!ok) {
		s_failed++;
	}
}

static bool Logged(const char* part, size_t from = 0) {
	for (size_t i = from; i < s_lines.size(); i++) {
		if (s_lines[i].find(part) != std::string::npos) {
			return true;
		}
	}
	return false;
}

static uint32_t Address(const void* p) {
	return (uint32_t)(uintptr_t)p;
}

NOINLINE static void WriteWord(uint32_t v) {
	g_guardTestWord = v;
}

NOINLINE static void WriteHalf(uint16_t v) {
	*(volatile uint16_t*)&g_guardTestWord = v;
}

struct Job {
	HANDLE go;
	uint32_t value;
	bool announce;          // tell the guard it started, as DllMain does in the game
};

static DWORD WINAPI Writer(LPVOID p) {
	Job* j = (Job*)p;
	if (j->announce) {
		GuardGame::ThreadStarted();
	}
	WaitForSingleObject(j->go, INFINITE);
	WriteWord(j->value);
	return 0;
}

#ifdef _MSC_VER
static bool AccessViolationReachesItsHandler() {
	__try {
		*(volatile int*)0 = 1;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return true;
	}
	return false;
}
#endif

// Frames until a line with part in it is written, at most for ms
static bool FramesUntil(const char* part, DWORD ms) {
	const size_t from = s_lines.size();
	const DWORD start = GetTickCount();
	while (GetTickCount() - start < ms) {
		GuardGame::Frame();
		if (Logged(part, from)) {
			return true;
		}
		Sleep(20);
	}
	return false;
}

int main() {
	setvbuf(stdout, NULL, _IONBF, 0);
	g_guardTestWord = Address(&s_manager);
	const uint32_t manager = Address(&s_manager);

	// A thread from before the guard starts
	Job early = { CreateEventA(NULL, TRUE, FALSE, NULL), 0x003B003A, false };
	HANDLE earlyThread = CreateThread(NULL, 0, Writer, &early, 0, NULL);

	GuardGame::AtStartup("guard_windows_test_none.ini");
	GuardGame::Frame();
	Check(Logged("Guard: the breakpoint is on in"), "the first pass is done");
	const bool works = Logged("Guard: on:");
	Check(works, "the test write was caught");
	if (!works) {
		printf("%d checks, %d failed\n", s_checks, s_failed);
		return 1;
	}

	// This thread
	WriteWord(0x003B003A);
	Check(g_guardTestWord == manager, "a write over it from this thread is put back at once");
	GuardGame::Frame();
	Check(Logged("CAUGHT (1): 0x003B003A written over"), "and written down at the next frame");
	Check(Logged("guard_windows_test.exe+"), "with where the write was");
	Check(!Logged("returns on the stack, nearest first: none found"), "and the returns on the stack");
	Check(Logged("eax ") && Logged("the code: "), "and the registers and the code");

	WriteHalf(0x003A);
	Check(g_guardTestWord == manager, "a 16-bit write over half of it is put back");

	// A thread that was there before
	SetEvent(early.go);
	WaitForSingleObject(earlyThread, 5000);
	Check(g_guardTestWord == manager, "a write from a thread that was there before the guard started is put back");
	GuardGame::Frame();
	Check(Logged("CAUGHT (3)"), "and written down");

	// A thread that starts later and says so (in the game, DllMain does): the breakpoint after the next pass
	Job late = { CreateEventA(NULL, TRUE, FALSE, NULL), 0x003B003A, true };
	HANDLE lateThread = CreateThread(NULL, 0, Writer, &late, 0, NULL);
	Sleep(100);
	Check(FramesUntil("more thread", 3000), "a pass gives a thread that started later the breakpoint");
	SetEvent(late.go);
	WaitForSingleObject(lateThread, 5000);
	Check(g_guardTestWord == manager, "and its write is put back");

	// A thread the breakpoint is not on yet: the look once a frame finds the change
	Job unseen = { CreateEventA(NULL, TRUE, TRUE, NULL), 0x00500050, false };
	HANDLE unseenThread = CreateThread(NULL, 0, Writer, &unseen, 0, NULL);
	WaitForSingleObject(unseenThread, 5000);
	if (g_guardTestWord == 0x00500050) {
		GuardGame::Frame();
		Check(g_guardTestWord == manager, "a write the breakpoint did not see is put back at the look once a frame");
		Check(Logged("the breakpoint did not see it written"), "and that is written down");
	}
	else {
		Check(g_guardTestWord == manager, "a new thread had the breakpoint from its start, and its write was put back");
	}

	// Another manager: taken, and garbage after it goes back to that one
	const uint32_t other = Address(&s_other);
	WriteWord(other);
	Check(g_guardTestWord == other, "another manager written over it is taken");
	WriteWord(0x003B003A);
	Check(g_guardTestWord == other, "and garbage after it goes back to that one");
	GuardGame::Frame();
	Check(Logged("so it is taken as the one now"), "both written down");

	// The same value: left as it is
	WriteWord(other);
	GuardGame::Frame();
	Check(g_guardTestWord == other && Logged("it is what it was"), "the same value written over it is left");

	// An object that is not a manager
	WriteWord(Address(&s_notOne));
	Check(g_guardTestWord == other, "an object that is not a manager is put back");

	// Reads are not caught
	GuardGame::Frame();
	const size_t lines = s_lines.size();
	volatile uint32_t sum = 0;
	for (int i = 0; i < 1000; i++) {
		sum += g_guardTestWord;
	}
	GuardGame::Frame();
	Check(s_lines.size() == lines, "reading it is not caught");

#ifdef _MSC_VER
	Check(AccessViolationReachesItsHandler(), "an access violation in a __try still reaches its __except");
#endif

	// Many writes, a frame after each: past the reports a session writes in full, one in a hundred
	for (int i = 0; i < 24; i++) {
		WriteWord(0x00020000u + (uint32_t)i);
		GuardGame::Frame();
	}
	Check(g_guardTestWord == other, "each of them is put back");
	Check(Logged("from here on only one in a hundred is written down"), "and past the reports a session writes in full, one in a hundred");

	// More writes in one frame than there is room for notes
	for (int i = 0; i < 40; i++) {
		WriteWord(0x00010000u + (uint32_t)i);
	}
	Check(g_guardTestWord == other, "forty writes in a row are all put back");
	GuardGame::Frame();
	Check(Logged("notes were written over before they could be written down"), "the notes that had no room are counted");

	printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
