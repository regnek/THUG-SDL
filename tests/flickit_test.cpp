// Offline tests for src/flickit.cpp. No game, SDL or Windows needed:
//   g++ -std=c++11 -Wall -Wextra -I src tests/flickit_test.cpp src/flickit.cpp -o flickit_test && ./flickit_test
//
// A simulated right stick is sampled once per frame, the way input.cpp polls it,
// and the buttons the recognizer asks for are checked frame by frame.

#include "flickit.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

using namespace Flickit;

static int g_fuzzFlips = 0;
static int g_checks = 0;
static int g_failures = 0;
static std::string g_case;

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failures++; printf("  FAIL [%s] line %d: %s\n", g_case.c_str(), __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { g_checks++; long long va = (long long)(a), vb = (long long)(b); if (va != vb) { g_failures++; printf("  FAIL [%s] line %d: %s == %s (%lld vs %lld)\n", g_case.c_str(), __LINE__, #a, #b, va, vb); } } while (0)

static const float kPi = 3.14159265f;

struct Press {
	int first;     // frame index of the first frame the button is down
	int frames;    // how many frames it stays down
	int dir;       // direction held on the first frame
	int leadFrames; // frames right before with the same direction and the button up
};

struct Sim {
	Recognizer rec;
	Settings cfg;
	double hz;
	double clock;      // ms, fractional
	uint32_t epoch;
	float x, y;
	bool lt, rt;
	std::vector<Output> outs;
	std::vector<uint32_t> times;

	explicit Sim(double rate = 60.0, uint32_t start = 1000, bool centered = true) : hz(rate), clock(0.0), epoch(start), x(0), y(0), lt(false), rt(false) {
		SetDefaults(cfg);
		if (centered) {
			hold(50);     // the stick rests at the center before anything happens, as it does in play
		}
	}

	uint32_t now() const { return epoch + (uint32_t)floor(clock + 0.5); }

	void step() {
		Input in;
		in.now = now();
		in.rx = x;
		in.ry = y;
		in.leftTrigger = lt;
		in.rightTrigger = rt;
		Output o;
		rec.Update(in, cfg, o);
		outs.push_back(o);
		times.push_back(in.now);
		clock += 1000.0 / hz;
	}

	int framesFor(double ms) const {
		int n = (int)floor(ms * hz / 1000.0 + 0.5);
		return n < 1 ? 1 : n;
	}

	void hold(double ms) {
		const int n = framesFor(ms);
		for (int i = 0; i < n; i++) step();
	}

	// Straight line to (tx, ty), arriving after ms
	void moveTo(float tx, float ty, double ms) {
		const int n = framesFor(ms);
		const float sx = x, sy = y;
		for (int i = 1; i <= n; i++) {
			const float f = (float)i / (float)n;
			x = sx + (tx - sx) * f;
			y = sy + (ty - sy) * f;
			step();
		}
	}

	// Along the rim to the given angle (degrees, 0 = right, 90 = up), the short way round
	void arcTo(float angleDeg, double ms, float radius = 1.0f) {
		const int n = framesFor(ms);
		const float a0 = atan2f(y, x) * 180.0f / kPi;
		float delta = angleDeg - a0;
		while (delta > 180.0f) delta -= 360.0f;
		while (delta < -180.0f) delta += 360.0f;
		for (int i = 1; i <= n; i++) {
			const float a = (a0 + delta * (float)i / (float)n) * kPi / 180.0f;
			x = radius * cosf(a);
			y = radius * sinf(a);
			step();
		}
	}

	void polar(float angleDeg, float radius = 1.0f) {
		x = radius * cosf(angleDeg * kPi / 180.0f);
		y = radius * sinf(angleDeg * kPi / 180.0f);
	}

	void spring(double ms = 40) { moveTo(0, 0, ms); }

	// --- analysis ---
	std::vector<Press> presses(bool Output::*button) const {
		std::vector<Press> v;
		for (size_t i = 0; i < outs.size(); i++) {
			const bool down = outs[i].*button;
			const bool prev = i > 0 && outs[i - 1].*button;
			if (down && !prev) {
				Press p;
				p.first = (int)i;
				p.frames = 0;
				p.dir = outs[i].dir;
				size_t j = i;
				while (j < outs.size() && outs[j].*button) { p.frames++; j++; }
				p.leadFrames = 0;
				int k = (int)i - 1;
				while (k >= 0 && outs[k].dir == p.dir && p.dir != DIR_NONE && !(outs[k].*button)) { p.leadFrames++; k--; }
				v.push_back(p);
			}
		}
		return v;
	}

	int firstFrame(bool Output::*button) const {
		for (size_t i = 0; i < outs.size(); i++) if (outs[i].*button) return (int)i;
		return -1;
	}

	// frame on which the held Ollie is let go for the n-th time
	int popFrame(int nth = 0) const {
		int seen = 0;
		for (size_t i = 1; i < outs.size(); i++) {
			if (outs[i - 1].ollie && !outs[i].ollie) {
				if (seen == nth) return (int)i;
				seen++;
			}
		}
		return -1;
	}

	std::vector<Output> gestures() const {
		std::vector<Output> v;
		for (size_t i = 0; i < outs.size(); i++) if (outs[i].gesture != GESTURE_NONE) v.push_back(outs[i]);
		return v;
	}

	double msBetween(int a, int b) const { return (double)(int32_t)(times[b] - times[a]); }
};

static const float D = 270, DL = 225, DR = 315, U = 90, UL = 135, UR = 45, L = 180, R = 0;

// Pull to a wind-up angle, hold, flick in a straight line to an end angle, spring back
static void flick(Sim& s, float fromDeg, float toDeg, double holdMs = 250, double flickMs = 35, double endHoldMs = 0) {
	const float a = fromDeg * kPi / 180.0f;
	s.moveTo(cosf(a), sinf(a), 60);
	s.hold(holdMs);
	const float b = toDeg * kPi / 180.0f;
	s.moveTo(cosf(b), sinf(b), flickMs);
	if (endHoldMs > 0) s.hold(endHoldMs);
	s.spring();
	s.hold(400);
}

static void expectFlip(Sim& s, int slot, int start, int end) {
	std::vector<Press> flips = s.presses(&Output::flip);
	CHECK_EQ(flips.size(), 1);
	if (flips.size() == 1) {
		CHECK_EQ(flips[0].dir, slot);
		CHECK(flips[0].frames >= 2);
		CHECK(flips[0].leadFrames >= 1);
		const int pop = s.popFrame();
		CHECK(pop >= 0);
		if (pop >= 0) {
			CHECK(flips[0].first > pop);                      // never on the pop frame
			CHECK(flips[0].first - flips[0].leadFrames > pop); // direction also after the pop
			CHECK(s.msBetween(pop, flips[0].first) <= 150.0);  // and soon enough to finish the flip
		}
	}
	std::vector<Output> g = s.gestures();
	CHECK_EQ(g.size(), 1);
	if (g.size() == 1) {
		CHECK_EQ(g[0].gesture, GESTURE_FLIP);
		CHECK_EQ(g[0].start, start);
		CHECK_EQ(g[0].end, end);
		CHECK_EQ(g[0].slot, slot);
	}
}

static void testHelpers() {
	g_case = "helpers";
	CHECK_EQ(Sector(1, 0), DIR_R);
	CHECK_EQ(Sector(1, 1), DIR_UR);
	CHECK_EQ(Sector(0, 1), DIR_U);
	CHECK_EQ(Sector(-1, 1), DIR_UL);
	CHECK_EQ(Sector(-1, 0), DIR_L);
	CHECK_EQ(Sector(-1, -1), DIR_DL);
	CHECK_EQ(Sector(0, -1), DIR_D);
	CHECK_EQ(Sector(1, -1), DIR_DR);
	CHECK_EQ(Sector(1, -0.2f), DIR_R);     // just below the axis is still "right"
	CHECK(InWindupHalf(270, WIND_OLLIE, 0));
	CHECK(InWindupHalf(225, WIND_OLLIE, 0));
	CHECK(!InWindupHalf(180, WIND_OLLIE, 0));
	CHECK(!InWindupHalf(90, WIND_OLLIE, 0));
	CHECK(InWindupHalf(90, WIND_NOLLIE, 0));
	CHECK(InWindupHalf(195, WIND_OLLIE, 10));
	CHECK(!InWindupHalf(190, WIND_OLLIE, 10));

	// The published skate. table, with kickflip to the upper right
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_D, DIR_U, 0), DIR_NONE);
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_D, DIR_UR, 0), DIR_L);    // Kickflip
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_D, DIR_UL, 0), DIR_R);    // Heelflip
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_D, DIR_R, 0), DIR_D);     // BS Pop Shove-It
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_D, DIR_L, 0), DIR_U);     // FS shove-it slot
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_DL, DIR_UR, 0), DIR_DL);  // Varial Kickflip
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_DR, DIR_UL, 0), DIR_DR);  // Varial Heelflip
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_DR, DIR_U, 0), DIR_UL);   // Hardflip
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_DL, DIR_U, 0), DIR_UR);   // Inward Heelflip
	// Mirrored
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_D, DIR_UL, 1), DIR_L);
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_D, DIR_UR, 1), DIR_R);
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_DR, DIR_UL, 1), DIR_DL);
	CHECK_EQ(MapFlip(WIND_OLLIE, DIR_D, DIR_L, 1), DIR_D);
	// Nollie: same table upside down
	CHECK_EQ(MapFlip(WIND_NOLLIE, DIR_U, DIR_D, 0), DIR_NONE);
	CHECK_EQ(MapFlip(WIND_NOLLIE, DIR_U, DIR_DR, 0), DIR_L);
	CHECK_EQ(MapFlip(WIND_NOLLIE, DIR_U, DIR_DL, 0), DIR_R);
	CHECK_EQ(MapFlip(WIND_NOLLIE, DIR_UL, DIR_DR, 0), DIR_DL);
}

static void testOllie(double hz) {
	g_case = "plain ollie";
	Sim s(hz);
	flick(s, D, U);
	const int first = s.firstFrame(&Output::ollie);
	const int pop = s.popFrame();
	CHECK(first >= 0);
	CHECK(pop > first);
	CHECK(s.msBetween(first, pop) >= 200.0);              // held through the wind-up
	CHECK_EQ(s.presses(&Output::flip).size(), 0);
	CHECK_EQ(s.presses(&Output::ollie).size(), 1);
	std::vector<Output> g = s.gestures();
	CHECK_EQ(g.size(), 1);
	if (g.size() == 1) {
		CHECK_EQ(g[0].gesture, GESTURE_POP);
		CHECK_EQ(g[0].start, DIR_D);
		CHECK_EQ(g[0].end, DIR_U);
	}
	for (size_t i = 0; i < s.outs.size(); i++) {
		CHECK(!s.outs[i].ownsDirection);
		CHECK_EQ(s.outs[i].dir, DIR_NONE);
	}
	// Grind hold: starts about 200 ms after the pop, lasts about 1500 ms
	s.hold(2000);
	std::vector<Press> grinds = s.presses(&Output::grind);
	CHECK_EQ(grinds.size(), 1);
	if (grinds.size() == 1 && pop >= 0) {
		const double delay = s.msBetween(pop, grinds[0].first);
		const double held = grinds[0].frames * 1000.0 / hz;
		CHECK(delay >= 190.0 && delay <= 260.0);
		CHECK(held >= 1400.0 && held <= 1600.0);
	}
	CHECK(!s.rec.Busy());
}

static void testFlips(double hz) {
	struct Case { const char* name; float from, to; int slot, start, end; };
	const Case cases[] = {
		{ "kickflip", D, UR, DIR_L, DIR_D, DIR_UR },
		{ "heelflip", D, UL, DIR_R, DIR_D, DIR_UL },
		{ "varial kickflip", DL, UR, DIR_DL, DIR_DL, DIR_UR },
		{ "varial heelflip", DR, UL, DIR_DR, DIR_DR, DIR_UL },
		{ "hardflip", DR, U, DIR_UL, DIR_DR, DIR_U },
		{ "inward heelflip", DL, U, DIR_UR, DIR_DL, DIR_U },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		g_case = cases[i].name;
		// flick and let go, flick and hold at the top, and a very fast flick
		{ Sim s(hz); flick(s, cases[i].from, cases[i].to); expectFlip(s, cases[i].slot, cases[i].start, cases[i].end); }
		{ Sim s(hz); flick(s, cases[i].from, cases[i].to, 250, 35, 200); expectFlip(s, cases[i].slot, cases[i].start, cases[i].end); }
		{ Sim s(hz); flick(s, cases[i].from, cases[i].to, 250, 1); expectFlip(s, cases[i].slot, cases[i].start, cases[i].end); }
		// a short, soft flick that only reaches 65 percent
		{
			Sim s(hz);
			const float a = cases[i].from * kPi / 180.0f, b = cases[i].to * kPi / 180.0f;
			s.moveTo(cosf(a), sinf(a), 60);
			s.hold(250);
			s.moveTo(0.65f * cosf(b), 0.65f * sinf(b), 45);
			s.spring();
			s.hold(400);
			expectFlip(s, cases[i].slot, cases[i].start, cases[i].end);
		}
	}
}

static void testShoveIts(double hz) {
	g_case = "pop shove-it drag";
	{
		Sim s(hz);
		s.moveTo(0, -1, 60);
		s.hold(250);
		s.arcTo(R, 90);      // drag along the rim to the right
		s.hold(150);
		s.spring();
		s.hold(400);
		expectFlip(s, DIR_D, DIR_D, DIR_R);
	}
	g_case = "fs shove-it drag";
	{
		Sim s(hz);
		s.moveTo(0, -1, 60);
		s.hold(250);
		s.arcTo(L, 90);
		s.hold(150);
		s.spring();
		s.hold(400);
		expectFlip(s, DIR_U, DIR_D, DIR_L);
	}
	g_case = "drag and let go at once";
	{
		Sim s(hz);
		s.moveTo(0, -1, 60);
		s.hold(250);
		s.arcTo(R, 70);
		s.spring();
		s.hold(400);
		expectFlip(s, DIR_D, DIR_D, DIR_R);
	}
	// A kickflip thrown as an arc around the rim passes "right" on the way: still a kickflip
	g_case = "kickflip around the rim";
	{
		Sim s(hz);
		s.moveTo(0, -1, 60);
		s.hold(250);
		s.arcTo(UR, 50);
		s.spring();
		s.hold(400);
		expectFlip(s, DIR_L, DIR_D, DIR_UR);
	}
}

static void testMirror() {
	g_case = "mirrored kickflip";
	Sim s;
	s.cfg.kickflipSide = 1;
	flick(s, D, UL);
	expectFlip(s, DIR_L, DIR_D, DIR_UL);
}

static void testSlowRelease(double hz) {
	g_case = "slow release";
	Sim s(hz);
	s.moveTo(0, -1, 60);
	s.hold(250);
	s.moveTo(0, 0, 300);     // ease the stick back
	s.hold(600);
	CHECK(s.popFrame() >= 0);
	CHECK_EQ(s.presses(&Output::flip).size(), 0);
	std::vector<Output> g = s.gestures();
	CHECK_EQ(g.size(), 1);
	if (g.size() == 1) CHECK_EQ(g[0].gesture, GESTURE_POP);
}

static void testStartDwell() {
	g_case = "start follows where the stick rests";
	Sim s;
	s.moveTo(0, -1, 60);
	s.hold(150);
	s.arcTo(DL, 40);        // shift the wind-up to down-left and rest there
	s.hold(150);
	const float b = UR * kPi / 180.0f;
	s.moveTo(cosf(b), sinf(b), 35);
	s.spring();
	s.hold(400);
	expectFlip(s, DIR_DL, DIR_DL, DIR_UR);   // varial kickflip, not kickflip
}

static void testJitterNearSectorEdge() {
	g_case = "jitter at the edge of the wind-up";
	Sim s;
	// Rest around 200 degrees: a few degrees past the edge of "down-left", inside the tolerance
	s.moveTo(cosf(215 * kPi / 180), sinf(215 * kPi / 180), 60);
	s.hold(100);
	for (int i = 0; i < 60; i++) {
		s.polar(i % 2 ? 198.0f : 206.0f);
		s.step();
	}
	CHECK_EQ(s.popFrame(), -1);            // still crouched
	CHECK(s.outs.back().ollie);
	// well past the tolerance it does pop
	s.polar(185.0f);
	s.step();
	CHECK(!s.outs.back().ollie);
}

static void testOvershootIsNotNollie() {
	g_case = "overshoot is not a nollie wind-up";
	Sim s;
	s.cfg.nollie = 1;
	flick(s, D, U, 250, 35, 400);         // flick up and keep the stick up
	CHECK_EQ(s.presses(&Output::ollie).size(), 1);
	CHECK_EQ(s.presses(&Output::nollie).size(), 0);
}

static void testNollie(double hz) {
	g_case = "nollie kickflip";
	Sim s(hz);
	s.cfg.nollie = 1;
	s.cfg.nollieTaps = 2;
	s.moveTo(0, 1, 60);
	s.hold(350);
	const float b = DR * kPi / 180.0f;
	s.moveTo(cosf(b), sinf(b), 35);
	s.spring();
	s.hold(400);
	std::vector<Press> taps = s.presses(&Output::nollie);
	CHECK_EQ(taps.size(), 2);
	for (size_t i = 0; i < taps.size(); i++) CHECK(taps[i].frames >= 2);
	if (taps.size() == 2) CHECK(taps[1].first - (taps[0].first + taps[0].frames) >= 2);
	CHECK_EQ(s.presses(&Output::ollie).size(), 1);
	std::vector<Press> flips = s.presses(&Output::flip);
	CHECK_EQ(flips.size(), 1);
	if (flips.size() == 1) CHECK_EQ(flips[0].dir, DIR_L);
	// With nollie switched off, pushing the stick up does nothing. Yanking it down from there is
	// an ordinary pull into the wind-up, so the most it can turn into is a plain ollie.
	g_case = "nollie off";
	Sim t(hz);
	t.moveTo(0, 1, 60);
	t.hold(350);
	CHECK_EQ(t.presses(&Output::ollie).size(), 0);
	t.moveTo(cosf(b), sinf(b), 35);
	t.spring();
	t.hold(400);
	CHECK(t.presses(&Output::ollie).size() <= 1);
	CHECK_EQ(t.presses(&Output::flip).size(), 0);
	CHECK_EQ(t.presses(&Output::nollie).size(), 0);
}

static void testGrabs(double hz) {
	g_case = "left trigger grab";
	{
		Sim s(hz);
		s.hold(100);
		s.lt = true;
		s.hold(500);
		s.lt = false;
		s.hold(200);
		std::vector<Press> g = s.presses(&Output::grab);
		CHECK_EQ(g.size(), 1);
		if (g.size() == 1) {
			CHECK_EQ(g[0].dir, DIR_L);
			CHECK(g[0].leadFrames >= 1);
			const double held = g[0].frames * 1000.0 / hz;
			CHECK(held >= 430.0 && held <= 520.0);       // stays down while the trigger is held
		}
		// the direction is only owned for the short press, not for the whole grab
		int owned = 0;
		for (size_t i = 0; i < s.outs.size(); i++) owned += s.outs[i].ownsDirection ? 1 : 0;
		CHECK(owned * 1000.0 / hz <= 120.0);
		CHECK(!s.rec.Busy());
	}
	g_case = "right trigger tap";
	{
		Sim s(hz);
		s.hold(100);
		s.rt = true;
		s.step();
		s.rt = false;
		s.hold(300);
		std::vector<Press> g = s.presses(&Output::grab);
		CHECK_EQ(g.size(), 1);
		if (g.size() == 1) {
			CHECK_EQ(g[0].dir, DIR_R);
			CHECK(g[0].frames >= 2);
		}
	}
	g_case = "second trigger switches the grab";
	{
		Sim s(hz);
		s.hold(100);
		s.lt = true;
		s.hold(300);
		s.rt = true;
		s.hold(300);
		s.lt = false; s.rt = false;
		s.hold(200);
		std::vector<Press> g = s.presses(&Output::grab);
		CHECK_EQ(g.size(), 2);                          // released and pressed again
		if (g.size() == 2) {
			CHECK_EQ(g[0].dir, DIR_L);
			CHECK_EQ(g[1].dir, DIR_R);
		}
	}
	g_case = "grabs off";
	{
		Sim s(hz);
		s.cfg.triggerGrabs = 0;
		s.lt = true;
		s.hold(300);
		CHECK_EQ(s.presses(&Output::grab).size(), 0);
	}
}

// A trigger pulled right around the pop: the grab press and the pop must not interleave
static void testGrabAroundPop() {
	g_case = "grab around the pop";
	for (int offset = -3; offset <= 3; offset++) {
		Sim s;
		s.moveTo(0, -1, 60);
		s.hold(250);
		const float b = UR * kPi / 180.0f;
		for (int f = -3; f <= 12; f++) {
			if (f == offset) s.lt = true;
			if (f == 0) { s.x = cosf(b); s.y = sinf(b); }       // instant flick
			if (f == 3) { s.x = 0; s.y = 0; }
			s.step();
		}
		s.hold(300);
		s.lt = false;
		s.hold(2000);       // long enough for the grind hold to run out
		std::vector<Press> flips = s.presses(&Output::flip);
		std::vector<Press> grabs = s.presses(&Output::grab);
		CHECK_EQ(flips.size(), 1);
		CHECK_EQ(grabs.size(), 1);
		CHECK_EQ(s.presses(&Output::ollie).size(), 1);
		// the two presses never overlap their direction frames
		if (flips.size() == 1 && grabs.size() == 1) {
			CHECK_EQ(flips[0].dir, DIR_L);
			CHECK_EQ(grabs[0].dir, DIR_L);
			const int fa = flips[0].first - flips[0].leadFrames, fb = flips[0].first + flips[0].frames;
			const int ga = grabs[0].first - grabs[0].leadFrames;
			int gb = grabs[0].first;
			while (gb < (int)s.outs.size() && s.outs[gb].ownsDirection && s.outs[gb].grab) gb++;
			CHECK(fb <= ga || gb <= fa);
			// Ollie is released before the flip's direction goes down
			const int pop = s.popFrame();
			CHECK(pop >= 0 && pop < fa);
		}
		// and Ollie never changes while either press is being entered
		for (int which = 0; which < 2; which++) {
			const std::vector<Press>& v = which == 0 ? flips : grabs;
			for (size_t i = 0; i < v.size(); i++) {
				for (int f = v[i].first - v[i].leadFrames; f <= v[i].first; f++) {
					if (f >= 1) CHECK_EQ(s.outs[f - 1].ollie, s.outs[f].ollie);
				}
			}
		}
		CHECK(!s.rec.Busy());
	}
}

static void testFlipWhileGrabbing() {
	g_case = "flick during a grab";
	Sim s;
	s.rt = true;
	s.hold(200);
	flick(s, D, UR);
	s.rt = false;
	s.hold(200);
	std::vector<Press> flips = s.presses(&Output::flip);
	CHECK_EQ(flips.size(), 1);
	if (flips.size() == 1) CHECK_EQ(flips[0].dir, DIR_L);
}

static void testDoubleFlick(double hz) {
	g_case = "two flicks in a row";
	Sim s(hz);
	for (int i = 0; i < 2; i++) {
		s.moveTo(0, -1, 40);
		s.hold(90);
		const float b = UR * kPi / 180.0f;
		s.moveTo(cosf(b), sinf(b), 30);
		s.spring(30);
		s.hold(40);
	}
	s.hold(400);
	std::vector<Press> flips = s.presses(&Output::flip);
	CHECK_EQ(flips.size(), 2);
	if (flips.size() == 2) {
		CHECK_EQ(flips[0].dir, DIR_L);
		CHECK_EQ(flips[1].dir, DIR_L);
		CHECK(flips[1].first - (flips[0].first + flips[0].frames) >= 1);   // a released frame in between
	}
	CHECK_EQ(s.presses(&Output::ollie).size(), 2);
}

static void testGrindRules() {
	g_case = "auto grind off";
	{
		Sim s;
		s.cfg.autoGrind = 0;
		flick(s, D, UR);
		s.hold(2000);
		CHECK_EQ(s.presses(&Output::grind).size(), 0);
	}
	g_case = "crouching again ends the grind hold";
	{
		Sim s;
		flick(s, D, U, 250, 35, 0);        // ends with 400 ms of rest: the hold is running
		CHECK(s.outs.back().grind);
		s.moveTo(0, -1, 60);
		s.hold(300);
		CHECK(!s.outs.back().grind);
		CHECK(s.outs.back().ollie);
	}
}

static void testRepull() {
	g_case = "ease off and pull again";
	Sim s;
	s.moveTo(0, -1, 60);
	s.hold(200);
	s.moveTo(0, -0.3f, 50);       // eases below the release point: that is a pop
	s.hold(34);
	s.moveTo(0, -1, 50);          // and straight back down: crouching again
	s.hold(200);
	CHECK_EQ(s.presses(&Output::ollie).size(), 2);
	CHECK(s.outs.back().ollie);
	CHECK_EQ(s.presses(&Output::flip).size(), 0);
	// there is at least one frame with Ollie up between the two holds
	std::vector<Press> o = s.presses(&Output::ollie);
	if (o.size() == 2) CHECK(o[1].first - (o[0].first + o[0].frames) >= 1);
}

static void testIdleNoise() {
	g_case = "resting stick";
	Sim s;
	unsigned seed = 12345;
	for (int i = 0; i < 600; i++) {
		seed = seed * 1664525u + 1013904223u;
		s.x = (float)((int)((seed >> 16) % 2001) - 1000) / 1000.0f * 0.15f;
		seed = seed * 1664525u + 1013904223u;
		s.y = (float)((int)((seed >> 16) % 2001) - 1000) / 1000.0f * 0.15f;
		s.step();
		const Output& o = s.outs.back();
		CHECK(!o.ollie && !o.flip && !o.grab && !o.grind && !o.nollie && !o.ownsDirection);
	}
	CHECK(!s.rec.Busy());
}

static void testNeedsCenterFirst() {
	g_case = "stick already down when Flickit starts";
	Sim s(60.0, 1000, false);
	s.x = 0; s.y = -1;
	s.hold(300);
	CHECK_EQ(s.presses(&Output::ollie).size(), 0);     // no crouch until the stick has been centered once
	s.spring();
	s.hold(50);
	s.moveTo(0, -1, 60);
	s.hold(100);
	CHECK(s.outs.back().ollie);
}

static void testClockWrap() {
	g_case = "millisecond counter wraps";
	Sim s(60.0, 0xFFFFFF00u);     // wraps about a quarter second in
	flick(s, D, UR);
	expectFlip(s, DIR_L, DIR_D, DIR_UR);
	s.hold(2000);
	CHECK_EQ(s.presses(&Output::grind).size(), 1);
	CHECK(!s.rec.Busy());
}

static void testReset() {
	g_case = "reset drops everything";
	Sim s;
	s.lt = true;
	s.moveTo(0, -1, 60);
	s.hold(200);
	CHECK(s.rec.Busy());
	s.rec.Reset();
	CHECK(!s.rec.Busy());
	s.lt = false;
	s.x = 0; s.y = 0;
	s.step();
	const Output& o = s.outs.back();
	CHECK(!o.ollie && !o.flip && !o.grab && !o.grind && !o.ownsDirection);
}

// Random stick and trigger movement: whatever comes in, the output has to stay well formed
static void testFuzz(double hz, unsigned seed) {
	g_case = "fuzz";
	Sim s(hz);
	s.cfg.nollie = (seed & 1) ? 1 : 0;
	const int total = (int)(120.0 * hz);       // two minutes
	float tx = 0, ty = 0;
	for (int i = 0; i < total; i++) {
		seed = seed * 1664525u + 1013904223u;
		const unsigned r = seed >> 8;
		if (r % 9 == 0) {
			// new target: center, rim or anywhere
			const unsigned kind = (r >> 8) % 4;
			const float a = (float)((r >> 12) % 360) * kPi / 180.0f;
			const float m = kind == 0 ? 0.0f : (kind == 1 ? 1.0f : (float)((r >> 21) % 100) / 100.0f);
			tx = m * cosf(a);
			ty = m * sinf(a);
		}
		const float speed = 0.05f + (float)((r >> 4) % 100) / 100.0f;   // fraction of the way per frame
		s.x += (tx - s.x) * speed;
		s.y += (ty - s.y) * speed;
		if (r % 97 == 0) s.lt = !s.lt;
		if (r % 89 == 0) s.rt = !s.rt;
		s.step();
	}
	// let everything run out
	s.x = 0; s.y = 0; s.lt = false; s.rt = false;
	s.hold(3000);

	int bad = 0;
	for (size_t i = 0; i < s.outs.size(); i++) {
		const Output& o = s.outs[i];
		if (o.ownsDirection != (o.dir != DIR_NONE)) bad++;
		if (o.flip && o.dir == DIR_NONE) bad++;
		if (o.dir < DIR_NONE || o.dir > DIR_DR) bad++;
	}
	CHECK_EQ(bad, 0);

	std::vector<Press> flips = s.presses(&Output::flip);
	int shortPress = 0, noLead = 0;
	for (size_t i = 0; i < flips.size(); i++) {
		if (flips[i].frames < 2) shortPress++;
		if (flips[i].leadFrames < 1) noLead++;
	}
	CHECK_EQ(shortPress, 0);
	CHECK_EQ(noLead, 0);
	g_fuzzFlips += (int)flips.size();          // main() checks that the fuzz really exercised it

	// From the frame a direction goes down to the frame its button goes down, no other button changes:
	// the game sees direction and button back to back.
	int disturbed = 0;
	std::vector<Press> grabs = s.presses(&Output::grab);
	for (int which = 0; which < 2; which++) {
		const std::vector<Press>& v = which == 0 ? flips : grabs;
		for (size_t i = 0; i < v.size(); i++) {
			if (v[i].dir == DIR_NONE) continue;          // a grab kept down by the trigger, not a fresh press
			const int from = v[i].first - v[i].leadFrames;
			for (int f = from; f <= v[i].first; f++) {
				if (f < 1) continue;
				const Output& a = s.outs[f - 1];
				const Output& b = s.outs[f];
				if (a.ollie != b.ollie || a.grind != b.grind || a.nollie != b.nollie) disturbed++;
				if (which == 0 && a.grab != b.grab) disturbed++;
			}
		}
	}
	CHECK_EQ(disturbed, 0);

	const Output& last = s.outs.back();
	CHECK(!last.ollie && !last.flip && !last.grab && !last.grind && !last.nollie && !last.ownsDirection);
	CHECK(!s.rec.Busy());
}

int main() {
	testHelpers();
	const double rates[] = { 30.0, 60.0, 120.0, 144.0, 240.0 };
	for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
		const double hz = rates[i];
		const int before = g_failures;
		testOllie(hz);
		testFlips(hz);
		testShoveIts(hz);
		testSlowRelease(hz);
		testNollie(hz);
		testGrabs(hz);
		testDoubleFlick(hz);
		testFuzz(hz, 0xC0FFEEu + (unsigned)i);
		testFuzz(hz, 0xBADF00Du + (unsigned)i);
		printf("%5.0f Hz: %s\n", hz, g_failures == before ? "ok" : "FAILED");
	}
	testMirror();
	testStartDwell();
	testJitterNearSectorEdge();
	testOvershootIsNotNollie();
	testFlipWhileGrabbing();
	testGrabAroundPop();
	testGrindRules();
	testRepull();
	testIdleNoise();
	testNeedsCenterFirst();
	testClockWrap();
	testReset();

	g_case = "fuzz coverage";
	CHECK(g_fuzzFlips > 100);

	printf("%d flip presses seen in the fuzz runs\n", g_fuzzFlips);
	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
