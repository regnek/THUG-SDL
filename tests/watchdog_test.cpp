// Tests for watchdog.h: what counts as a number, the history, the policy of tries, and the camera check.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src tests/watchdog_test.cpp -o watchdog_test

#include "watchdog.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static int s_checks = 0;
static int s_failed = 0;

static void Check(bool ok, const char* what) {
	s_checks++;
	if (!ok) {
		s_failed++;
		std::printf("FAILED: %s\n", what);
	}
}

static void TestSane() {
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	float v[3] = { 1.0f, -2.0f, 30000.0f };
	Check(Watchdog::Sane(v, 3), "ordinary numbers are sane");
	v[1] = nan;
	Check(!Watchdog::Sane(v, 3), "a NaN is not");
	v[1] = -nan;
	Check(!Watchdog::Sane(v, 3), "nor a NaN with its sign bit set");
	v[1] = inf;
	Check(!Watchdog::Sane(v, 3), "nor infinity");
	v[1] = -inf;
	Check(!Watchdog::Sane(v, 3), "nor minus infinity");
	v[1] = 2.0e6f;
	Check(!Watchdog::Sane(v, 3), "nor a number past the farthest");
	v[1] = -2.0e6f;
	Check(!Watchdog::Sane(v, 3), "on either side");
	v[1] = 0.0f;
	Check(Watchdog::Sane(v, 3), "back to sane");
	Check(Watchdog::Sane(v, 0), "no numbers at all are sane");
	float big[16];
	for (int i = 0; i < 16; i++) {
		big[i] = (float)i;
	}
	Check(Watchdog::Sane(big, 16), "a whole matrix");
	big[15] = nan;
	Check(Watchdog::Sane(big, 12), "the first three rows only do not see the fourth");
	Check(!Watchdog::Sane(big, 16), "all four do");
	// a NaN bit pattern made from an integer, as a pointer read as a float would be
	uint32_t bits = 0x7FC01234u;
	float f;
	std::memcpy(&f, &bits, 4);
	Check(!Watchdog::Sane(&f, 1), "a NaN pattern from an integer");
}

static Watchdog::Sample Make(uint32_t frame) {
	Watchdog::Sample s;
	std::memset(&s, 0, sizeof(s));
	s.frame = frame;
	s.pos[0] = (float)frame;
	return s;
}

static void TestHistory() {
	Watchdog::History<4> h;
	Check(h.Count() == 0 && h.Back(0) == 0, "empty");
	h.Push(Make(1));
	Check(h.Count() == 1 && h.Back(0) && h.Back(0)->frame == 1, "one");
	Check(h.Back(1) == 0 && h.Back(-1) == 0, "nothing before the first, nothing after the newest");
	for (uint32_t i = 2; i <= 9; i++) {
		h.Push(Make(i));
	}
	Check(h.Count() == 4, "holds four");
	bool order = true;
	for (int b = 0; b < 4; b++) {
		order = order && h.Back(b) && h.Back(b)->frame == 9u - (uint32_t)b;
	}
	Check(order, "newest first going back");
	Check(h.Back(4) == 0, "no fifth");
	h.Clear();
	Check(h.Count() == 0 && h.Back(0) == 0, "cleared");
	h.Push(Make(20));
	Check(h.Back(0)->frame == 20, "fills again after a clear");
}

static void TestPolicy() {
	Watchdog::Policy p;
	Check(p.Frame(true) == Watchdog::KEEP, "whole: keep");
	Check(p.Frame(false) == Watchdog::RESTORE, "first bad frame: restore");
	Check(p.Frame(false) == Watchdog::RESET, "the restore did not hold: reset");
	Check(p.Frame(false) == Watchdog::RESET, "once more");
	Check(p.Frame(false) == Watchdog::GIVE_UP, "then give up");
	Check(p.Frame(false) == Watchdog::NOTHING, "and stay given up");
	Check(p.Frame(false) == Watchdog::NOTHING, "for as long as it lasts");
	// whole again, but not for long: the tries are still counted
	for (int i = 0; i < Watchdog::Policy::kForgive - 1; i++) {
		p.Frame(true);
	}
	Check(p.Frame(false) == Watchdog::NOTHING, "less than two seconds whole: still given up");
	for (int i = 0; i < Watchdog::Policy::kForgive; i++) {
		p.Frame(true);
	}
	Check(p.Tries() == 0, "two seconds whole: the tries start again");
	Check(p.Frame(false) == Watchdog::RESTORE, "a new bad frame after that: restore first");
	Check(p.Frame(true) == Watchdog::KEEP, "the restore held");
	for (int i = 0; i < 30; i++) {
		p.Frame(true);
	}
	Check(p.Frame(false) == Watchdog::RESET, "bad again soon after: the restore was not enough last time either");
}

static void TestCamera() {
	const float skater[3] = { 1000.0f, 50.0f, -2000.0f };
	float near[3] = { 1200.0f, 150.0f, -2200.0f };
	float far[3] = { 9000.0f, 50.0f, -2000.0f };
	float m[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	Watchdog::CameraCheck c;
	for (int i = 0; i < Watchdog::CameraCheck::kFrames - 1; i++) {
		c.Feed(skater, near, m);
	}
	Check(!c.Decided() && !c.Trusted(), "not decided before its frames are in");
	c.Feed(skater, near, m);
	Check(c.Decided() && c.Trusted(), "near with unit rows: trusted");

	Watchdog::CameraCheck d;
	for (int i = 0; i < Watchdog::CameraCheck::kFrames; i++) {
		d.Feed(skater, far, m);
	}
	Check(d.Decided() && !d.Trusted(), "far from the skater: not the camera");

	Watchdog::CameraCheck e;
	float bad[16] = { 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1 };
	for (int i = 0; i < Watchdog::CameraCheck::kFrames; i++) {
		e.Feed(skater, near, bad);
	}
	Check(!e.Trusted(), "rows that are not unit length: not a camera matrix");

	Watchdog::CameraCheck f;
	for (int i = 0; i < Watchdog::CameraCheck::kFrames; i++) {
		f.Feed(skater, i % 20 == 0 ? far : near, m);
	}
	Check(f.Trusted(), "a frame in twenty off (a cut): still trusted");
	Watchdog::CameraCheck g;
	for (int i = 0; i < Watchdog::CameraCheck::kFrames; i++) {
		g.Feed(skater, i % 5 == 0 ? far : near, m);
	}
	Check(!g.Trusted(), "one in five off: not");
	g.Reset();
	Check(!g.Decided(), "reset: undecided again");
}

int main() {
	TestSane();
	TestHistory();
	TestPolicy();
	TestCamera();
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
