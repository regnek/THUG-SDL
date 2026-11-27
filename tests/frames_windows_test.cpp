// The watch on the frames on Windows itself, outside the game: src/Flickit/frames_game.cpp as it is, watching
// this program, which plays the game's part: it marks frames, spends their time in functions of its own, and
// lets the sampler stop it and look. A 32-bit program like the game. CMake builds it as frames_windows_test;
// the CI runs it.
//
// Frames that are fast, then slow, with one that hangs in a Sleep; then something is "switched". What the
// watch wrote is looked at: the stretches told, the time found in this program's own code and called from the
// very place it was, the Sleep found in Windows' code.
#define FRAMES_PERIOD_SECONDS 2.0
#define NOMINMAX
#include "Flickit/frames_game.cpp"

#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#define NOINLINE __declspec(noinline)
#define RETURN_ADDRESS() _ReturnAddress()
#else
#define NOINLINE __attribute__((noinline))
#define RETURN_ADDRESS() __builtin_return_address(0)
#endif

static std::vector<std::string> s_notes;

namespace Game {
	void Note(const char* format, ...) {
		char text[1024];
		va_list args;
		va_start(args, format);
		Str::FormatV(text, sizeof(text), format, args);
		va_end(args);
		printf("%s", text);
		s_notes.push_back(text);
	}
	void NoteLines(const char* const* lines, int count) {
		for (int i = 0; i < count; i++) {
			printf("%s\n", lines[i]);
			s_notes.push_back(lines[i]);
		}
	}
}

namespace FlickitGame {
	bool SkaterWhereabouts(Whereabouts&) {
		return false;
	}
}

namespace CamRelGame {
	bool On() {
		return true;
	}
}

static int s_checks = 0;
static int s_failed = 0;

static void Check(bool ok, const char* what) {
	s_checks++;
	if (!ok) {
		s_failed++;
		printf("FAILED: %s\n", what);
	}
}

static double s_ticksPerMs = 0.0;
static volatile uint32_t s_sink = 0;
static void* volatile s_workBack = 0;       // where Busy goes back to in Work

// Spends that long, here and nowhere else
NOINLINE static void Busy(double ms) {
	s_workBack = RETURN_ADDRESS();
	const uint64_t until = __rdtsc() + (uint64_t)(ms * s_ticksPerMs);
	while (__rdtsc() < until) {
		s_sink++;
	}
}

// The game's work of a frame
NOINLINE static void Work(double ms) {
	Busy(ms);
	s_sink += 3;
}

// A draw call's time, as Direct3D would spend it
NOINLINE static void Spin(double ms) {
	const uint64_t until = __rdtsc() + (uint64_t)(ms * s_ticksPerMs);
	while (__rdtsc() < until) {
		s_sink++;
	}
}

NOINLINE static void Hang(DWORD ms) {
	Sleep(ms);
	s_sink += 5;
}

static void Frame(double before, double drawing, double after, bool hang) {
	FramesGame::Boundary();
	Work(before);
	if (hang) {
		Hang(300);
	}
	for (int i = 0; i < 20; i++) {
		const uint64_t start = FramesGame::Tick();
		const uint64_t in = FramesGame::Tick();
		Spin(drawing / 20.0);
		const uint64_t took = FramesGame::Tick() - in;
		if (FramesGame::Timing()) {
			FramesGame::Drawn(start, took, i % 2 == 0);
		}
	}
	FramesGame::RenderEnd();
	Work(after);
}

static double Seconds() {
	LARGE_INTEGER q, f;
	QueryPerformanceCounter(&q);
	QueryPerformanceFrequency(&f);
	return (double)q.QuadPart / (double)f.QuadPart;
}

static int Count(const char* text) {
	int n = 0;
	for (size_t i = 0; i < s_notes.size(); i++) {
		n += s_notes[i].find(text) != std::string::npos ? 1 : 0;
	}
	return n;
}

// The first note with all three texts, or an empty one
static std::string Find(const char* one, const char* other, const char* third = "") {
	for (size_t i = 0; i < s_notes.size(); i++) {
		if (s_notes[i].find(one) != std::string::npos && s_notes[i].find(other) != std::string::npos && s_notes[i].find(third) != std::string::npos) {
			return s_notes[i];
		}
	}
	return std::string();
}

// The share in front of a name in a list like "61.2% the game; 30.1% ntdll.dll", or -1
static double ShareOf(const std::string& line, const char* name) {
	const size_t at = line.find(std::string("% ") + name);
	if (at == std::string::npos) {
		return -1.0;
	}
	size_t from = at;
	while (from > 0 && (line[from - 1] == '.' || (line[from - 1] >= '0' && line[from - 1] <= '9'))) {
		from--;
	}
	return atof(line.substr(from, at - from).c_str());
}

int main() {
	timeBeginPeriod(1);
	{
		const double t0 = Seconds();
		const uint64_t c0 = __rdtsc();
		while (Seconds() - t0 < 0.05) {
		}
		s_ticksPerMs = (double)(__rdtsc() - c0) / ((Seconds() - t0) * 1000.0);
	}
	printf("the processor's clock: %.0f ticks a millisecond\n", s_ticksPerMs);
	FramesGame::Configure("no such file.ini");

	// (the stretches are two seconds long here)
	// fast frames: about 5 ms each
	double until = Seconds() + 4.5;
	while (Seconds() < until) {
		Frame(2.0, 2.0, 1.0, false);
	}
	// slow ones: about 30 ms each, and one of them hangs for 300 ms more
	until = Seconds() + 6.6;
	bool hung = false;
	const double hangAt = Seconds() + 3.0;
	while (Seconds() < until) {
		const bool hang = !hung && Seconds() > hangAt;
		hung = hung || hang;
		Frame(18.0, 9.0, 3.0, hang);
	}
	// something is switched; then fast frames again
	FramesGame::Switching();
	until = Seconds() + 3.3;
	while (Seconds() < until) {
		Frame(2.0, 2.0, 1.0, false);
	}
	// (what the sampler still has in words gets written at a frame's start)
	for (int i = 0; i < 100; i++) {
		Frame(1.0, 1.0, 0.5, false);
		Sleep(2);
	}

	Check(Count("Frames: watching the frames, and where the game's thread is about every millisecond") == 1, "the sampler started");
	Check(Count("a stretch to compare with") >= 2, "the first fast stretch and the one after the switch are told though they are not slow");
	Check(Count("a slow stretch") >= 2 && Count("a slow stretch") <= 5, "the slow stretches are told");
	const std::string slow = Find("a slow stretch", "frames in");
	Check(slow.find("meshes drawn a frame") != std::string::npos && slow.find("before the level is drawn") != std::string::npos
		&& slow.find("relative to the meshes' middles") != std::string::npos, "a slow stretch has its draws and its parts");
	Check(Count("the longest 3") >= 1 || Count("the longest 4") >= 1, "the frame that hung is the longest of its stretch (300 ms and more)");

	// where the time went: in this program ("the game"), and called from the place in Work that Busy goes back to
	char back[40];
	Str::Format(back, sizeof(back), "exe %08X", (unsigned)(uintptr_t)s_workBack);
	int stretches = 0, inGame = 0, calledFromWork = 0;
	for (size_t i = 0; i + 2 < s_notes.size(); i++) {
		if (s_notes[i].find("Frames: all of it") == 0 && s_notes[i].find("by module") != std::string::npos) {
			stretches++;
			inGame += ShareOf(s_notes[i], "the game") >= 50.0 ? 1 : 0;
			calledFromWork += s_notes[i + 2].find("called from") != std::string::npos && s_notes[i + 2].find(back) != std::string::npos ? 1 : 0;
		}
	}
	printf("%d stretches with looks; in %d most of the time was in this program; in %d it was called from %s\n", stretches, inGame, calledFromWork, back);
	Check(stretches >= 3, "the looks of the stretches are told");
	Check(inGame >= stretches - 1 && inGame >= 2, "most of the time was found in this program's own code");
	Check(calledFromWork >= stretches - 1 && calledFromWork >= 2, "and called from the very place in Work");

	// the frame that hung: looks late in a frame, in Windows' code, called from this program
	Check(!Find("Frames: past ", "by module").empty(), "the looks late in a long frame are told");
	Check(!Find("Frames: past ", "by module", "ntdll.dll").empty() || !Find("Frames: past ", "by module", "wow64").empty()
		|| !Find("Frames: past ", "by module", "KERNELBASE.dll").empty() || !Find("Frames: past ", "by module", "kernel32.dll").empty(),
		"a frame that hangs in a Sleep is found in Windows' code");
	const std::string lateFrom = Find("Frames: past ", "called from");
	Check(lateFrom.find("exe ") != std::string::npos, "and its caller in this program is found on the stack");

	printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
