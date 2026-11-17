// Offline tests for Flickit's stick patterns: the loader, the matcher and their use by the recognizer.
//
//   g++ -std=c++11 -Wall -Wextra -I src tests/flickit_pattern_test.cpp src/flickit.cpp -o flickit_pattern_test
//   ./flickit_pattern_test              the made-up patterns below
//   ./flickit_pattern_test skater.pat   also draws every pattern of that file and reports what was recognized
//
// The patterns in this file are invented for the tests. No game data is used or needed.

#include "flickit.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Flickit;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failures++; std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR(a, b) (std::fabs((double)(a) - (double)(b)) < 1e-3)

static const char* kPatterns =
	"global_tolerance_dist 0.4\n"
	"global_tolerance_time 10.0\n"
	"global_anticipation_delay 15\n"
	"\n"
	"pattern Ollie\n\ttolerance_dist 0.5\n\tclock 6 1.0\n\tclock 12 1.0\n"
	"pattern Kickflip\n\tmin_time 3\n\tclock 6 0.7\n\tclock 2 1.0\n"
	"pattern Heelflip\n\tclock 6 0.7\n\tclock 10 1.0\n"
	"pattern PopShuvit\n\ttolerance_dist 0.35\n\tclock 6 1.0\n\tclock 4 1.0\n\tclock 2 1.0\n"
	"pattern FSPopShuvit\n\ttolerance_dist 0.35\n\tclock 6 1.0\n\tclock 8 1.0\n\tclock 10 1.0\n"
	"pattern VarialKickflip\n\tcoord -0.7 0.7\n\tclock 6 1.0\n\tclock 2 1.0\n"
	"pattern Hardflip\n\tcoord 0.7 0.7\n\tclock 6 1.0\n\tclock 1 1.0\n"
	"pattern 360Flip\n\tclock 9 1.0\n\tcoord -0.7 0.7\n\tclock 6 1.0\n\tclock 2 1.0\n"
	"pattern Nollie\n\ttolerance_dist 0.5\n\tclock 12 1.0\n\tclock 6 1.0\n"
	"pattern N_Kickflip\n\tclock 12 0.7\n\tclock 4 1.0\n"
	"pattern Wave\n\tclock 3 1.0\n\tclock 9 1.0\n"
	"pattern Centered\n\tclock 0 1.0\n\tclock 3 0.5\n"
	"pattern Broken\n\tclock 6 1.0\n";

// Clock face position in file coordinates (y down)
static void Clock(double hour, double radius, float& x, float& y) {
	const double a = hour * 3.14159265358979 / 6.0;
	x = (float)(std::sin(a) * radius);
	y = (float)(-std::cos(a) * radius);
}

static const Pattern* Find(const PatternSet& set, const char* name) {
	for (int i = 0; i < set.Count(); i++) {
		if (std::strcmp(set.At(i).name, name) == 0) {
			return &set.At(i);
		}
	}
	return 0;
}

static void testParser() {
	PatternSet set;
	const int n = set.Parse(kPatterns, std::strlen(kPatterns));
	CHECK(n == 12);                       // "Broken" has one point and is dropped
	CHECK(Find(set, "Broken") == 0);

	const Pattern* ollie = Find(set, "Ollie");
	CHECK(ollie && ollie->count == 2 && NEAR(ollie->tolerance, 0.5));
	CHECK(ollie && NEAR(ollie->x[0], 0.0) && NEAR(ollie->y[0], 1.0));    // 6 o'clock is down, down is +y
	CHECK(ollie && NEAR(ollie->x[1], 0.0) && NEAR(ollie->y[1], -1.0));
	CHECK(ollie && ollie->known && ollie->slot == DIR_NONE && !ollie->nollie);

	const Pattern* kick = Find(set, "Kickflip");
	CHECK(kick && kick->count == 2 && NEAR(kick->tolerance, 0.4));         // the number after min_time is skipped
	CHECK(kick && NEAR(kick->y[0], 0.7) && NEAR(kick->x[1], 0.866) && NEAR(kick->y[1], -0.5));
	CHECK(kick && kick->slot == DIR_L && kick->taps == 1);

	const Pattern* varial = Find(set, "VarialKickflip");
	CHECK(varial && varial->count == 3 && NEAR(varial->x[0], -0.7) && NEAR(varial->y[0], 0.7));
	CHECK(varial && varial->slot == DIR_DL && varial->taps == 1);

	const Pattern* tre = Find(set, "360Flip");
	CHECK(tre && tre->count == 4 && tre->slot == DIR_DL && tre->taps == 2);
	CHECK(tre && NEAR(tre->x[0], -1.0) && NEAR(tre->y[0], 0.0));          // 9 o'clock is left

	const Pattern* nk = Find(set, "N_Kickflip");
	CHECK(nk && nk->known && nk->nollie && nk->slot == DIR_L);
	const Pattern* nollie = Find(set, "Nollie");
	CHECK(nollie && nollie->known && nollie->nollie && nollie->slot == DIR_NONE);
	const Pattern* wave = Find(set, "Wave");
	CHECK(wave && !wave->known);
	const Pattern* centered = Find(set, "Centered");
	CHECK(centered && NEAR(centered->x[0], 0.0) && NEAR(centered->y[0], 0.0) && NEAR(centered->x[1], 0.5));

	// names
	Pattern p;
	std::strcpy(p.name, "Laserflip");
	CHECK(NamePattern(p) && p.slot == DIR_DR && p.taps == 2 && !p.nollie);
	std::strcpy(p.name, "N_FS360PopShuvit");
	CHECK(NamePattern(p) && p.slot == DIR_U && p.taps == 2 && p.nollie);
	std::strcpy(p.name, "90_Kickflip");
	CHECK(!NamePattern(p));
	std::strcpy(p.name, "N_Ollie");
	CHECK(!NamePattern(p) && !p.nollie);

	// limits and odd input
	PatternSet empty;
	CHECK(empty.Parse("", 0) == 0);
	CHECK(empty.Parse("pattern", 7) == 0);
	CHECK(empty.Parse("coord 1 1 clock 3 1 tolerance_dist 2", 36) == 0);   // points before any pattern
	std::string many;
	for (int i = 0; i < MAX_PATTERNS + 20; i++) {
		many += "pattern Kickflip clock 6 1 clock 2 1 clock 3 1 clock 4 1 clock 5 1 clock 6 1 clock 7 1 clock 8 1 clock 9 1 clock 10 1\n";
	}
	PatternSet full;
	CHECK(full.Parse(many.c_str(), many.size()) == MAX_PATTERNS);
	CHECK(full.At(0).count == MAX_PATTERN_POINTS);
	std::string longName = "pattern " + std::string(200, 'x') + " clock 6 1 clock 12 1";
	PatternSet odd;
	CHECK(odd.Parse(longName.c_str(), longName.size()) == 1 && std::strlen(odd.At(0).name) == MAX_PATTERN_NAME - 1);
}

// Feeds the matcher: rest at one clock position, then `gap` ticks in the center, then the listed positions.
struct Drawn {
	bool matched;
	std::string name;
	PatternMatch match;
};

static Drawn draw(const PatternSet& set, const std::vector<double>& hours, int gap, int missTicks, bool nollie, int rest = 30) {
	PatternMatcher m;
	Drawn d;
	d.matched = false;
	float x, y;
	PatternMatch pm;
	for (int i = 0; i < 20; i++) {
		m.Tick(set, 0.0f, 0.0f, missTicks, nollie, pm);
	}
	for (size_t h = 0; h < hours.size(); h++) {
		Clock(hours[h], 1.0, x, y);
		const int ticks = (h == 0) ? rest : 1;
		for (int i = 0; i < ticks && !d.matched; i++) {
			if (m.Tick(set, x, y, missTicks, nollie, pm)) {
				d.matched = true;
				d.match = pm;
				d.name = set.At(pm.index).name;
			}
		}
		if (h == 0) {
			for (int i = 0; i < gap && !d.matched; i++) {
				if (m.Tick(set, 0.0f, 0.0f, missTicks, nollie, pm)) {
					d.matched = true;
					d.match = pm;
					d.name = set.At(pm.index).name;
				}
			}
		}
	}
	return d;
}

static void testMatcher() {
	PatternSet set;
	set.Parse(kPatterns, std::strlen(kPatterns));

	Drawn d = draw(set, { 6, 2 }, 0, 10, false);
	CHECK(d.matched && d.name == "Kickflip");
	CHECK(d.matched && d.match.ticks == 2 && NEAR(d.match.speed, 1.0));        // resting on the start is not counted

	d = draw(set, { 6, 2 }, 0, 10, false, 500);
	CHECK(d.matched && d.match.ticks == 2);

	d = draw(set, { 6, 2 }, 2, 10, false);                                       // 4 ticks over 2 points
	CHECK(d.matched && d.match.ticks == 4 && NEAR(d.match.speed, (4.4 - 2.0) / (4.4 - 1.75)));
	d = draw(set, { 6, 2 }, 7, 10, false);                                       // 9 ticks: slow
	CHECK(d.matched && NEAR(d.match.speed, 0.0));
	d = draw(set, { 6, 2 }, 10, 10, false);                                      // exactly at the limit
	CHECK(d.matched);
	d = draw(set, { 6, 2 }, 11, 10, false);                                      // one tick too many: forgotten
	CHECK(!d.matched);

	d = draw(set, { 6, 12 }, 1, 10, false);
	CHECK(d.matched && d.name == "Ollie");
	d = draw(set, { 6, 10 }, 1, 10, false);
	CHECK(d.matched && d.name == "Heelflip");

	// Along the rim both the kickflip (2 points) and the shuvit (3 points) are completed: the longer one wins
	d = draw(set, { 6, 5, 4, 3, 2 }, 0, 10, false);
	CHECK(d.matched && d.name == "PopShuvit");
	d = draw(set, { 6, 7, 8, 9, 10 }, 0, 10, false);
	CHECK(d.matched && d.name == "FSPopShuvit");
	d = draw(set, { 7.5, 6, 2 }, 0, 10, false);
	CHECK(d.matched && d.name == "VarialKickflip");
	d = draw(set, { 9, 7.5, 6, 2 }, 0, 10, false);
	CHECK(d.matched && d.name == "360Flip");
	d = draw(set, { 4.5, 6, 1 }, 0, 10, false);
	CHECK(d.matched && d.name == "Hardflip");

	// The kickflip (tolerance 0.4) is complete a moment before the shuvit (0.35) that ends in the same
	// place. With the shuvit one point short, the kickflip waits for it.
	{
		PatternMatcher slow;
		PatternMatch pm;
		float x, y;
		std::string got;
		int reports = 0;
		const double path[] = { 6, 6, 5.5, 5, 4.5, 4, 3.5, 3, 2.75, 2.7, 2.65, 2.6, 2.5, 2.4, 2.3, 2.2, 2.1, 2, 2, 2 };
		for (size_t i = 0; i < sizeof(path) / sizeof(path[0]); i++) {
			Clock(path[i], 1.0, x, y);
			if (slow.Tick(set, x, y, 10, false, pm)) {
				got = set.At(pm.index).name;
				reports++;
			}
		}
		CHECK(reports == 1 && got == "PopShuvit");
	}

	// Patterns that start with the stick up only exist when nollies are on
	d = draw(set, { 12, 4 }, 1, 10, false);
	CHECK(!d.matched);
	d = draw(set, { 12, 4 }, 1, 10, true);
	CHECK(d.matched && d.name == "N_Kickflip");
	d = draw(set, { 12, 6 }, 1, 10, true);
	CHECK(d.matched && d.name == "Nollie");

	// Names without a trick are never reported
	d = draw(set, { 3, 9 }, 1, 10, true);
	CHECK(!d.matched);

	// After a match everything starts over: the end of one gesture is not the middle of another
	PatternMatcher m;
	PatternMatch pm;
	float x, y;
	Clock(6, 1.0, x, y);
	m.Tick(set, x, y, 10, false, pm);
	Clock(2, 1.0, x, y);
	CHECK(m.Tick(set, x, y, 10, false, pm));
	CHECK(!m.Tick(set, x, y, 10, false, pm));
	CHECK(!m.Tick(set, x, y, 10, false, pm));
}

// ---- the whole recognizer, fed frame by frame ----

struct Run {
	int pops;            // Ollie released
	int flipPresses;     // rising edges of Flip
	int flipDir;         // direction held with the last Flip press
	bool dirWithEveryFlip;
	int gestures;
	int lastGesture;
	int lastSlot;
	std::string lastPattern;
	float lastSpeed;
	bool everOllie;
};

struct Step {
	float x, y;   // file coordinates: y down
	int ms;       // time to travel there in a straight line
	int hold;     // then stay this long
};

static Run play(Recognizer& rec, const Settings& cfg, const std::vector<Step>& steps, int hz) {
	Run r;
	r.pops = r.flipPresses = r.gestures = 0;
	r.flipDir = DIR_NONE;
	r.dirWithEveryFlip = true;
	r.lastGesture = GESTURE_NONE;
	r.lastSlot = DIR_NONE;
	r.lastSpeed = 0.0f;
	r.everOllie = false;

	double t = 1000.0;
	const double dt = 1000.0 / hz;
	float cx = 0.0f, cy = 0.0f;
	bool prevOllie = false, prevFlip = false;

	std::vector<Step> all(steps);
	Step home = { 0.0f, 0.0f, 40, 900 };
	all.insert(all.begin(), Step{ 0.0f, 0.0f, 0, 120 });
	all.push_back(home);

	for (size_t s = 0; s < all.size(); s++) {
		const Step& st = all[s];
		const float sx = cx, sy = cy;
		const double total = st.ms + st.hold;
		for (double e = 0.0; e < total; e += dt) {
			const double k = st.ms > 0 ? (e < st.ms ? e / st.ms : 1.0) : 1.0;
			cx = (float)(sx + (st.x - sx) * k);
			cy = (float)(sy + (st.y - sy) * k);
			Input in;
			in.now = (uint32_t)t;
			in.rx = cx;
			in.ry = -cy;    // the recognizer takes y up
			in.leftTrigger = false;
			in.rightTrigger = false;
			Output out;
			rec.Update(in, cfg, out);
			if (out.ollie) {
				r.everOllie = true;
			}
			if (prevOllie && !out.ollie) {
				r.pops++;
			}
			if (out.flip && !prevFlip) {
				r.flipPresses++;
				r.flipDir = out.dir;
				if (!out.ownsDirection || out.dir == DIR_NONE) {
					r.dirWithEveryFlip = false;
				}
			}
			if (out.gesture != GESTURE_NONE) {
				r.gestures++;
				r.lastGesture = out.gesture;
				r.lastSlot = out.slot;
				r.lastPattern = out.pattern ? out.pattern : "";
				r.lastSpeed = out.speed;
			}
			prevOllie = out.ollie;
			prevFlip = out.flip;
			t += dt;
		}
		cx = st.x;
		cy = st.y;
	}
	return r;
}

static Step at(double hour, double radius, int ms, int hold) {
	Step s;
	Clock(hour, radius, s.x, s.y);
	s.ms = ms;
	s.hold = hold;
	return s;
}

static void testRecognizer(int hz) {
	PatternSet set;
	set.Parse(kPatterns, std::strlen(kPatterns));
	Settings cfg;
	SetDefaults(cfg);
	Sanitize(cfg);

	{   // kickflip: down, flick to 2 o'clock
		Recognizer rec;
		rec.UsePatterns(&set);
		Run r = play(rec, cfg, { at(6, 1.0, 60, 300), at(2, 1.0, 35, 120) }, hz);
		CHECK(r.everOllie && r.pops == 1);
		CHECK(r.flipPresses == 1 && r.flipDir == DIR_L && r.dirWithEveryFlip);
		CHECK(r.gestures == 1 && r.lastGesture == GESTURE_FLIP && r.lastPattern == "Kickflip");
		CHECK(r.lastSpeed > 0.5f);
		CHECK(!rec.Busy() || cfg.autoGrind);
	}
	{   // a slow flick still does the trick, with a low speed
		Recognizer rec;
		rec.UsePatterns(&set);
		Run r = play(rec, cfg, { at(6, 1.0, 60, 300), at(2, 1.0, 300, 120) }, hz);
		CHECK(r.flipPresses == 1 && r.flipDir == DIR_L);
		CHECK(r.lastPattern == "Kickflip" && r.lastSpeed < 0.5f);
	}
	{   // plain ollie: down, flick straight up
		Recognizer rec;
		rec.UsePatterns(&set);
		Run r = play(rec, cfg, { at(6, 1.0, 60, 300), at(12, 1.0, 35, 120) }, hz);
		CHECK(r.pops == 1 && r.flipPresses == 0);
		CHECK(r.gestures == 1 && r.lastGesture == GESTURE_POP && r.lastPattern == "Ollie");
	}
	{   // letting go: no pattern, so no pop (as in Skate): Ollie goes up and he stands up again
		Recognizer rec;
		rec.UsePatterns(&set);
		Run r = play(rec, cfg, { at(6, 1.0, 60, 300) }, hz);
		CHECK(r.flipPresses == 0);
		CHECK(r.gestures == 0);
	}
	{   // 360 flip: from the left around to down, flick to 2 o'clock: the varial slot, tapped twice
		Recognizer rec;
		rec.UsePatterns(&set);
		Run r = play(rec, cfg, { at(9, 1.0, 60, 150), at(7.5, 1.0, 40, 0), at(6, 1.0, 40, 0), at(2, 1.0, 35, 120) }, hz);
		CHECK(r.pops == 1);
		CHECK(r.flipPresses == 2 && r.flipDir == DIR_DL && r.dirWithEveryFlip);
		CHECK(r.lastPattern == "360Flip");
	}
	{   // shuvit along the rim: it ends inside the wind-up margin on some paths, so the pattern itself pops
		Recognizer rec;
		rec.UsePatterns(&set);
		Run r = play(rec, cfg, { at(6, 1.0, 60, 300), at(4, 1.0, 40, 0), at(2, 1.0, 40, 120) }, hz);
		CHECK(r.pops == 1 && r.flipPresses == 1 && r.flipDir == DIR_D && r.lastPattern == "PopShuvit");
	}
	{   // KickflipSide = 1 mirrors the patterns: the kickflip is now at 10 o'clock
		Recognizer rec;
		rec.UsePatterns(&set);
		Settings mirrored = cfg;
		mirrored.kickflipSide = 1;
		Run r = play(rec, mirrored, { at(6, 1.0, 60, 300), at(10, 1.0, 35, 120) }, hz);
		CHECK(r.flipPresses == 1 && r.flipDir == DIR_L && r.lastPattern == "Kickflip");
	}
	{   // nollies are off by default: holding the stick up does nothing, and the patterns that start
		// there are not followed. Ending low is an ordinary crouch, so letting go is a plain ollie.
		Recognizer rec;
		rec.UsePatterns(&set);
		Run up = play(rec, cfg, { at(12, 1.0, 60, 300) }, hz);
		CHECK(up.pops == 0 && up.flipPresses == 0 && up.gestures == 0);
		Run r = play(rec, cfg, { at(12, 1.0, 60, 300), at(4, 1.0, 35, 120) }, hz);
		CHECK(r.flipPresses == 0 && r.lastPattern.empty());
	}
	{   // nollies on
		Recognizer rec;
		rec.UsePatterns(&set);
		Settings n = cfg;
		n.nollie = 1;
		Run r = play(rec, n, { at(12, 1.0, 60, 300), at(4, 1.0, 35, 120) }, hz);
		CHECK(r.pops == 1 && r.flipPresses == 1 && r.flipDir == DIR_L && r.lastPattern == "N_Kickflip");
	}
	{   // Patterns = 0 in the settings, or no set: the built-in directions decide, as before
		Recognizer rec;
		rec.UsePatterns(&set);
		Settings off = cfg;
		off.patterns = 0;
		Run r = play(rec, off, { at(6, 1.0, 60, 300), at(1.5, 1.0, 35, 120) }, hz);
		CHECK(r.flipPresses == 1 && r.flipDir == DIR_L && r.lastPattern.empty());
		Recognizer plain;
		Run q = play(plain, cfg, { at(6, 1.0, 60, 300), at(1.5, 1.0, 35, 120) }, hz);
		CHECK(q.flipPresses == 1 && q.flipDir == DIR_L && q.lastPattern.empty());
		PatternSet none;
		Recognizer emptySet;
		emptySet.UsePatterns(&none);
		Run e = play(emptySet, cfg, { at(6, 1.0, 60, 300), at(1.5, 1.0, 35, 120) }, hz);
		CHECK(e.flipPresses == 1 && e.flipDir == DIR_L);
	}
	{   // two tricks in a row with one recognizer
		Recognizer rec;
		rec.UsePatterns(&set);
		Run a = play(rec, cfg, { at(6, 1.0, 60, 300), at(2, 1.0, 35, 120) }, hz);
		Run b = play(rec, cfg, { at(6, 1.0, 60, 300), at(10, 1.0, 35, 120) }, hz);
		CHECK(a.flipDir == DIR_L && b.flipPresses == 1 && b.flipDir == DIR_R && b.lastPattern == "Heelflip");
	}
}

// Draws every pattern of a real file the way a thumb would and reports how many come back as themselves.
static int drawFile(const char* path) {
	std::FILE* f = std::fopen(path, "rb");
	if (!f) {
		std::printf("cannot open %s\n", path);
		return 1;
	}
	std::string text;
	char buf[4096];
	size_t got;
	while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0) {
		text.append(buf, got);
	}
	std::fclose(f);

	PatternSet set;
	const int n = set.Parse(text.c_str(), text.size());
	std::printf("%s: %d patterns\n", path, n);
	Settings cfg;
	SetDefaults(cfg);
	cfg.nollie = 1;
	Sanitize(cfg);

	int same = 0, sameTrick = 0, other = 0, nothing = 0, unknown = 0;
	for (int i = 0; i < n; i++) {
		const Pattern& p = set.At(i);
		if (!p.known) {
			unknown++;
			continue;
		}
		std::vector<Step> steps;
		for (int k = 0; k < p.count; k++) {
			Step s;
			s.x = p.x[k];
			s.y = p.y[k];
			s.ms = (k == 0) ? 60 : 35;
			s.hold = (k == 0) ? 250 : (k == p.count - 1 ? 120 : 0);
			steps.push_back(s);
		}
		Recognizer rec;
		rec.UsePatterns(&set);
		Run r = play(rec, cfg, steps, 60);
		const Pattern* got = r.lastPattern.empty() ? 0 : Find(set, r.lastPattern.c_str());
		const int wantPresses = p.slot == DIR_NONE ? 0 : p.taps;
		if (got && std::strcmp(got->name, p.name) == 0 && r.flipPresses == wantPresses) {
			same++;
		}
		else if (got && got->slot == p.slot && got->taps == p.taps && got->nollie == p.nollie) {
			sameTrick++;
		}
		else if (r.gestures == 0 && r.pops == 0) {
			nothing++;
			std::printf("  #%d %-20s nothing happened (start %.2f, %.2f)\n", i, p.name, p.x[0], p.y[0]);
		}
		else {
			other++;
			std::printf("  #%d %-20s came out as %s, %d flip presses, %d pops\n", i, p.name,
				r.lastPattern.empty() ? "(no pattern)" : r.lastPattern.c_str(), r.flipPresses, r.pops);
		}
	}
	std::printf("recognized as itself: %d, as the same trick: %d, as something else: %d, nothing: %d, names without a trick: %d\n",
		same, sameTrick, other, nothing, unknown);
	return 0;
}

int main(int argc, char** argv) {
	testParser();
	testMatcher();
	const int rates[] = { 30, 60, 120, 144, 240 };
	for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
		const int before = g_failures;
		testRecognizer(rates[i]);
		std::printf("  %3d Hz: %s\n", rates[i], g_failures == before ? "ok" : "FAILED");
	}
	std::printf("%d checks, %d failures\n", g_checks, g_failures);
	if (argc > 1) {
		drawFile(argv[1]);
	}
	return g_failures ? 1 : 0;
}
