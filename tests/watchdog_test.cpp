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

static void TestSquare() {
	const float id[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 50000, 20, -9000, 1 };
	Check(Watchdog::Square(id), "the identity, wherever it stands, is square");
	const float c = 0.8f, s = 0.6f;
	const float turned[16] = { c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1 };
	Check(Watchdog::Square(turned), "turned about up: square");
	float m[16];
	std::memcpy(m, turned, sizeof(m));
	m[4] = m[8];
	m[5] = m[9];
	m[6] = m[10];
	Check(!Watchdog::Square(m), "up the same as forward: collapsed");
	std::memcpy(m, turned, sizeof(m));
	m[4] = m[5] = m[6] = 0;
	Check(!Watchdog::Square(m), "a row of nothing: collapsed");
	std::memcpy(m, turned, sizeof(m));
	m[0] *= 1.15f;
	m[2] *= 1.15f;
	Check(!Watchdog::Square(m), "a row 15% long: not square");
	m[0] = c * 1.05f;
	m[2] = -s * 1.05f;
	Check(Watchdog::Square(m), "5% long: square enough");
	std::memcpy(m, id, sizeof(m));
	m[1] = 0.15f;
	Check(!Watchdog::Square(m), "two rows leaning together");
	m[1] = -0.15f;
	Check(!Watchdog::Square(m), "or apart");
	m[1] = std::numeric_limits<float>::quiet_NaN();
	Check(!Watchdog::Square(m), "a NaN is not square");
	m[1] = 0.0f;
	m[15] = std::numeric_limits<float>::quiet_NaN();
	Check(Watchdog::Square(m), "the fourth row is not looked at");

	float view[16], proj[16];
	std::memcpy(view, id, sizeof(view));
	std::memcpy(proj, id, sizeof(proj));
	proj[14] = -4.0f;
	Check(Watchdog::ViewSane(view, proj), "a view and a projection");
	view[13] = std::numeric_limits<float>::quiet_NaN();
	Check(!Watchdog::ViewSane(view, proj), "a NaN in the view's position");
	view[13] = 0.0f;
	proj[0] = std::numeric_limits<float>::infinity();
	Check(!Watchdog::ViewSane(view, proj), "infinity in the projection");
	proj[0] = 1.0f;
	view[12] = 3.0e6f;
	Check(Watchdog::ViewSane(view, proj), "positions past a million are still a view");
	view[12] = 2.0e7f;
	Check(!Watchdog::ViewSane(view, proj), "past ten million they are not");
}

static void TestViewPolicy() {
	Watchdog::ViewPolicy p;
	Check(p.Frame(true, true) == Watchdog::VIEW_KEEP, "a whole view: keep");
	Check(p.Frame(false, true) == Watchdog::VIEW_CAMERA, "the first bad frame: the camera back");
	int restarts = 0, firstAt = 0, secondAt = 0, gaveUp = 0, gaveUpAt = 0;
	for (int f = 2; f <= 400; f++) {
		const Watchdog::ViewAction a = p.Frame(false, true);
		if (a == Watchdog::VIEW_RESTART) {
			restarts++;
			(restarts == 1 ? firstAt : secondAt) = f;
		}
		if (a == Watchdog::VIEW_GIVE_UP) {
			gaveUp++;
			gaveUpAt = f;
		}
		if (a == Watchdog::VIEW_CAMERA) {
			Check(false, "the camera only on the first bad frame");
		}
	}
	Check(restarts == 2 && firstAt == Watchdog::ViewPolicy::kFirst && secondAt == firstAt + Watchdog::ViewPolicy::kBetween,
		"two restarts: after ten bad frames, and a second later");
	Check(gaveUp == 1 && gaveUpAt == secondAt + Watchdog::ViewPolicy::kBetween, "then it gives up, once, a second after the second");
	Check(p.Frame(true, true) == Watchdog::VIEW_KEEP && p.Bad() == 0, "whole again");
	Check(p.Frame(false, true) == Watchdog::VIEW_CAMERA, "bad again: the camera first");
	int more = 0;
	for (int f = 2; f <= 200; f++) {
		more += p.Frame(false, true) == Watchdog::VIEW_RESTART;
	}
	Check(more == 0, "no more restarts until it has been whole for two seconds");
	for (int f = 0; f < Watchdog::ViewPolicy::kForgive; f++) {
		p.Frame(true, true);
	}
	p.Frame(false, true);
	for (int f = 2; f <= Watchdog::ViewPolicy::kFirst; f++) {
		more += p.Frame(false, true) == Watchdog::VIEW_RESTART;
	}
	Check(more == 1 && p.Restarts() == 1, "after two seconds whole they count from nothing");

	Watchdog::ViewPolicy q;
	q.Frame(false, true);
	int at = 0;
	for (int f = 2; f <= 50 && !at; f++) {
		if (q.Frame(false, f >= 30) == Watchdog::VIEW_RESTART) {
			at = f;
		}
	}
	Check(at == 30, "no restart while the skater himself is not whole: the first frame he is");
}

static void TestBlackout() {
	Watchdog::Blackout b;
	Check(b.Frame(100) == 0, "the first count only starts it");
	Check(b.Frame(150) == 0 && b.Frames() == 0, "drawn: nothing");
	int fired = 0, firedAt = 0;
	for (int f = 1; f <= 100; f++) {
		if (b.Frame(150) == 1) {
			fired++;
			firedAt = f;
		}
	}
	Check(fired == 1 && firedAt == Watchdog::Blackout::kFrames, "undrawn: told once, at half a second");
	Check(b.Frame(151) == -100 && b.Frames() == 0, "drawn again: told how long it was");
	for (int f = 1; f < Watchdog::Blackout::kFrames; f++) {
		b.Frame(151);
	}
	Check(b.Frame(152) == 0, "a gap shorter than half a second is not told");
	b.Reset();
	Check(b.Frame(152) == 0 && b.Frame(152) == 0 && b.Frames() == 1, "a reset starts it from the next count");
}

int main() {
	TestSane();
	TestHistory();
	TestPolicy();
	TestCamera();
	TestSquare();
	TestViewPolicy();
	TestBlackout();
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
