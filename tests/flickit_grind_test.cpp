// Offline tests for the grind side of src/flickit.cpp: the board tilt in the air, the flick-only pop
// on a rail, the switches between 50-50, 5-0 and nosegrind, the drag on a rail and stepping off it
// when the speed is gone. No game, SDL or Windows needed:
//   g++ -std=c++11 -Wall -Wextra -I src tests/flickit_grind_test.cpp src/flickit.cpp -o flickit_grind_test && ./flickit_grind_test
// Run it from the top of the repository, or pass the folder scripts/qb/source/scripts/game/skater: the rows the mod
// hands the game are checked against the game's own grind table in grindlist.q.
//
// The skater's state, which the mod reads from the game, is scripted here. For the switches a
// small stand-in for the game watches the buttons the way THUG's grind script does: two presses in
// order within 300 ms, taken only once the grind has settled, the new grind reported 15 frames later.
// It also takes the button that steps off a rail, from 16 frames into a grind on.

#include "flickit.h"

#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

using namespace Flickit;

static int g_checks = 0;
static int g_failures = 0;
static std::string g_case;

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failures++; printf("  FAIL [%s] line %d: %s\n", g_case.c_str(), __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { g_checks++; long long va = (long long)(a), vb = (long long)(b); if (va != vb) { g_failures++; printf("  FAIL [%s] line %d: %s == %s (%lld vs %lld)\n", g_case.c_str(), __LINE__, #a, #b, va, vb); } } while (0)

static const float kPi = 3.14159265f;
static const float D = 270, DL = 225, DR = 315, U = 90, UL = 135, UR = 45, L = 180, R = 0;

struct Press {
	int first;
	int frames;
	int dir;
};

// What THUG does with the buttons while a grind runs, as far as the switches go
struct FakeGame {
	bool on;
	int kind;              // the grind that is running
	bool extraTricks;      // the grind script has set its extra tricks: switches are taken
	int settleFrames;      // frames from the start of a grind to that point
	int sinceStart;        // frames since the grind started
	bool buggyNoseEntry;   // the nosegrind entry that is not ignored while in a nosegrind (the BS table in the game)
	bool deaf;             // presses get lost
	bool noRelease;        // the button that steps off the rail does nothing

	// What the mod has been told (it hears of a new grind 15 frames after it started)
	int reportedKind;
	bool reportedReady;
	uint32_t serial;

	struct Event { int button; uint32_t time; };   // 0 flip, 1 grab, 2 grind
	std::vector<Event> events;
	bool prev[3];
	int switches;
	int restarts;          // the same grind started again (the bug above, if the mod ever triggers it)

	// Stepping off the rail: the game takes a new press of that button from 16 frames into a grind
	bool prevStep;
	bool tookStep;         // on this frame
	int stepOffs;
	int stepSide;          // the side that was held with the last one

	FakeGame() : on(false), kind(GRIND_UNKNOWN), extraTricks(false), settleFrames(45), sinceStart(0), buggyNoseEntry(true), deaf(false), noRelease(false),
		reportedKind(GRIND_UNKNOWN), reportedReady(false), serial(0), switches(0), restarts(0),
		prevStep(false), tookStep(false), stepOffs(0), stepSide(0) {
		prev[0] = prev[1] = prev[2] = false;
	}

	void land(int grind) {
		on = true;
		kind = grind;
		extraTricks = false;
		sinceStart = 0;
		reportedKind = GRIND_UNKNOWN;
		reportedReady = false;
		serial++;
		events.clear();
	}

	void leave() { on = false; }

	bool inOrder(int a, int b, uint32_t now) const {
		// the latest press of b, and a press of a before it, both within 300 ms
		for (int i = (int)events.size() - 1; i >= 0; i--) {
			if (events[i].button != b) continue;
			if ((int32_t)(now - events[i].time) > 300) return false;
			for (int j = i - 1; j >= 0; j--) {
				if (events[j].button == a && (int32_t)(now - events[j].time) <= 300) return true;
			}
			return false;
		}
		return false;
	}

	void start(int grind) {
		if (grind == kind) restarts++; else switches++;
		kind = grind;
		extraTricks = false;
		sinceStart = 0;
		events.clear();
	}

	void frame(const Output& o, uint32_t now) {
		const bool cur[3] = { o.flip, o.grab, o.grind };
		for (int b = 0; b < 3; b++) {
			if (cur[b] && !prev[b]) {
				Event e; e.button = b; e.time = now;
				events.push_back(e);
			}
			prev[b] = cur[b];
		}
		tookStep = false;
		if (on && o.stepOff && !prevStep && !noRelease && sinceStart >= 16) {
			tookStep = true;
			stepOffs++;
			stepSide = o.stepSide;
		}
		prevStep = o.stepOff;
		if (!on) return;

		if (extraTricks && !deaf) {
			if (inOrder(0, 1, now) && kind != GRIND_FIVE_O) start(GRIND_FIVE_O);
			else if (inOrder(2, 1, now) && (kind != GRIND_NOSEGRIND || buggyNoseEntry)) start(GRIND_NOSEGRIND);
			else if (inOrder(2, 2, now) && kind != GRIND_FIFTY) start(GRIND_FIFTY);
			else if (inOrder(1, 1, now)) start(GRIND_CROOKED);
			else if (inOrder(0, 0, now)) start(GRIND_SMITH);
			else if (inOrder(2, 0, now)) start(GRIND_NOSESLIDE);
			else if (inOrder(0, 2, now)) start(GRIND_TAILSLIDE);
			else if (inOrder(1, 0, now)) start(GRIND_BLUNTSLIDE);
			else if (inOrder(1, 2, now)) start(GRIND_NOSEBLUNT);
		}

		sinceStart++;
		if (sinceStart == 15) {
			// the first of the two points where the grind script reports in
			serial += (reportedReady ? 1 : 0);
			reportedKind = (kind == GRIND_FIFTY || kind == GRIND_FIVE_O || kind == GRIND_NOSEGRIND) ? kind : GRIND_OTHER;
			reportedReady = false;
		}
		if (sinceStart == settleFrames) {
			extraTricks = true;
			reportedReady = true;
		}
	}
};

struct Sim {
	Recognizer rec;
	Settings cfg;
	double hz;
	double clock;
	uint32_t epoch;
	float x, y;
	bool lt, rt;
	int state;
	bool railCatch;
	bool railOff;          // the game says the board is out of speed on its rail
	int offSide;
	FakeGame game;
	std::vector<Output> outs;
	std::vector<uint32_t> times;
	std::vector<int> states;
	std::vector<Tilt> tilts;

	explicit Sim(double rate = 60.0, int startState = STATE_GROUND) : hz(rate), clock(0.0), epoch(5000), x(0), y(0), lt(false), rt(false), state(startState), railCatch(true),
		railOff(false), offSide(0) {
		SetDefaults(cfg);
		// a grind takes its switches about three quarters of a second in
		game.settleFrames = (int)(0.75 * rate);
		if (game.settleFrames < 17) game.settleFrames = 17;
		hold(50);
	}

	uint32_t now() const { return epoch + (uint32_t)floor(clock + 0.5); }

	void step() {
		Input in;
		in.now = now();
		in.rx = x;
		in.ry = y;
		in.leftTrigger = lt;
		in.rightTrigger = rt;
		in.state = state;
		in.railCatch = railCatch;
		in.grind = game.reportedKind;
		in.grindReady = game.reportedReady;
		in.grindSerial = game.serial;
		in.railOff = railOff;
		in.offSide = offSide;
		Output o;
		rec.Update(in, cfg, o);
		outs.push_back(o);
		times.push_back(in.now);
		states.push_back(state);
		tilts.push_back(rec.GetTilt());
		game.frame(o, in.now);
		// As in the game: letting go of Ollie on a rail is the jump off it
		if (state == STATE_RAIL && outs.size() >= 2 && outs[outs.size() - 2].ollie && !o.ollie) {
			state = STATE_AIR;
			game.leave();
		}
		// and the button that steps off the rail puts the skater on the ground next to it
		if (state == STATE_RAIL && game.tookStep) {
			state = STATE_GROUND;
			game.leave();
		}
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

	void toward(float angleDeg, double ms, float radius = 1.0f) {
		moveTo(radius * cosf(angleDeg * kPi / 180.0f), radius * sinf(angleDeg * kPi / 180.0f), ms);
	}

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

	void spring(double ms = 40) { moveTo(0, 0, ms); }

	// The board touches a rail: the game starts the grind the tilt asks for
	int land(bool railOnLeft = true) {
		const int grind = GrindFromTilt(rec.GetTilt(), railOnLeft);
		state = STATE_RAIL;
		game.land(grind);
		return grind;
	}

	void landOn(int grind) {
		state = STATE_RAIL;
		game.land(grind);
	}

	std::vector<Press> presses(bool Output::*button, size_t from = 0) const {
		std::vector<Press> v;
		for (size_t i = from; i < outs.size(); i++) {
			const bool down = outs[i].*button;
			const bool prev = i > from && outs[i - 1].*button;
			if (down && !prev) {
				Press p;
				p.first = (int)i;
				p.frames = 0;
				p.dir = outs[i].dir;
				size_t j = i;
				while (j < outs.size() && outs[j].*button) { p.frames++; j++; }
				v.push_back(p);
			}
		}
		return v;
	}

	std::vector<Output> gestures(size_t from = 0) const {
		std::vector<Output> v;
		for (size_t i = from; i < outs.size(); i++) if (outs[i].gesture != GESTURE_NONE) v.push_back(outs[i]);
		return v;
	}

	int count(bool Output::*button, size_t from = 0) const {
		int n = 0;
		for (size_t i = from; i < outs.size(); i++) if (outs[i].*button) n++;
		return n;
	}

	double msBetween(int a, int b) const { return (double)(int32_t)(times[b] - times[a]); }
	size_t mark() const { return outs.size(); }
};

// ---------------------------------------------------------------------------------------------
// The tilt table
// ---------------------------------------------------------------------------------------------

static Tilt tiltOf(int first, bool firstLeft, int cur) {
	Tilt t;
	t.valid = true;
	t.first = first;
	t.firstLeft = firstLeft;
	t.cur = cur;
	return t;
}

static void testTiltTable() {
	g_case = "tilt table";
	for (int side = 0; side < 2; side++) {
		const bool railLeft = side == 0;
		// "toward" and "away" as stick directions for this rail
		const int T = railLeft ? DIR_L : DIR_R, A = railLeft ? DIR_R : DIR_L;
		const int UT = railLeft ? DIR_UL : DIR_UR, UA = railLeft ? DIR_UR : DIR_UL;
		const int DT = railLeft ? DIR_DL : DIR_DR, DA = railLeft ? DIR_DR : DIR_DL;

		// nothing held
		Tilt none = tiltOf(TILT_UP, false, DIR_U);
		none.valid = false;
		CHECK_EQ(GrindFromTilt(none, railLeft), GRIND_FIFTY);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_NONE, false, DIR_NONE), railLeft), GRIND_FIFTY);

		// straight up and down
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_UP, false, DIR_U), railLeft), GRIND_NOSEGRIND);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_DOWN, true, DIR_D), railLeft), GRIND_FIVE_O);

		// the diagonals
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_UP, railLeft, UT), railLeft), GRIND_CROOKED);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_UP, !railLeft, UA), railLeft), GRIND_OVERCROOK);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_DOWN, !railLeft, DA), railLeft), GRIND_SALAD);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_DOWN, railLeft, DT), railLeft), GRIND_FIVE_O);

		// to a side and stay there
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_SIDE, railLeft, T), railLeft), GRIND_BOARDLIP);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_SIDE, !railLeft, A), railLeft), GRIND_BOARDLIP);

		// up or down first, then rolled to a side
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_UP, false, T), railLeft), GRIND_NOSESLIDE);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_UP, false, A), railLeft), GRIND_NOSEBLUNT);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_DOWN, false, T), railLeft), GRIND_TAILSLIDE);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_DOWN, false, A), railLeft), GRIND_BLUNTSLIDE);
		// ... and a little past the side
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_UP, false, DT), railLeft), GRIND_NOSESLIDE);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_UP, false, DA), railLeft), GRIND_NOSEBLUNT);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_DOWN, false, UT), railLeft), GRIND_TAILSLIDE);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_DOWN, false, UA), railLeft), GRIND_BLUNTSLIDE);
		// ... or all the way to the other end: where the stick is wins
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_UP, false, DIR_D), railLeft), GRIND_FIVE_O);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_DOWN, false, DIR_U), railLeft), GRIND_NOSEGRIND);

		// to a side first, then rolled up or down: the side of the first push decides
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_SIDE, railLeft, DIR_U), railLeft), GRIND_FEEBLE);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_SIDE, railLeft, UT), railLeft), GRIND_FEEBLE);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_SIDE, railLeft, UA), railLeft), GRIND_FEEBLE);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_SIDE, !railLeft, DIR_U), railLeft), GRIND_SMITH);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_SIDE, !railLeft, UA), railLeft), GRIND_SMITH);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_SIDE, railLeft, DIR_D), railLeft), GRIND_CROOKED);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_SIDE, railLeft, DT), railLeft), GRIND_CROOKED);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_SIDE, !railLeft, DIR_D), railLeft), GRIND_OVERCROOK);
		CHECK_EQ(GrindFromTilt(tiltOf(TILT_SIDE, !railLeft, DA), railLeft), GRIND_OVERCROOK);
	}

	// Every combination gives a grind the game side knows how to start
	for (int first = TILT_NONE; first <= TILT_SIDE; first++) {
		for (int cur = DIR_NONE; cur <= DIR_DR; cur++) {
			for (int bits = 0; bits < 4; bits++) {
				const int g = GrindFromTilt(tiltOf(first, (bits & 1) != 0, cur), (bits & 2) != 0);
				CHECK(g >= GRIND_FIFTY && g <= GRIND_NOSEBLUNT);
				CHECK(GrindName(g)[0] != 'u');
			}
		}
	}

	// Mirrored input, mirrored rail: the same grind
	for (int first = TILT_UP; first <= TILT_SIDE; first++) {
		for (int cur = DIR_R; cur <= DIR_DR; cur++) {
			static const int mirror[8] = { DIR_L, DIR_UL, DIR_U, DIR_UR, DIR_R, DIR_DR, DIR_D, DIR_DL };
			for (int fl = 0; fl < 2; fl++) {
				CHECK_EQ(GrindFromTilt(tiltOf(first, fl != 0, cur), true), GrindFromTilt(tiltOf(first, fl == 0, mirror[cur]), false));
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Starting the grind in the game: the rows of THUG's own grind table
// ---------------------------------------------------------------------------------------------

static std::string lower(const std::string& in) {
	std::string out = in;
	for (size_t i = 0; i < out.size(); i++) if (out[i] >= 'A' && out[i] <= 'Z') out[i] = (char)(out[i] - 'A' + 'a');
	return out;
}

static bool readFile(const std::string& path, std::string& out) {
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	char buf[4096];
	size_t n;
	out.clear();
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
	fclose(f);
	return true;
}

// The words between the [ and the matching ] that follow "name =", brackets of nested arrays included
static std::vector<std::string> arrayWords(const std::string& text, const std::string& name) {
	std::vector<std::string> words;
	size_t pos = text.find("\n" + name + " =");
	if (pos == std::string::npos) return words;
	pos = text.find('[', pos);
	if (pos == std::string::npos) return words;
	int depth = 0;
	std::string cur;
	for (size_t i = pos; i < text.size(); i++) {
		const char c = text[i];
		if (c == '[' || c == ']' || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '"') {
			if (!cur.empty()) { words.push_back(cur); cur.clear(); }
			if (c == '[') { depth++; words.push_back("["); }
			if (c == ']') { depth--; words.push_back("]"); if (depth == 0) break; }
		}
		else {
			cur += c;
		}
	}
	return words;
}

static void testNameChecksum() {
	g_case = "name checksum";
	// Values that stand in the game's code
	CHECK_EQ(NameChecksum("grind"), 0x255ED86Fu);
	CHECK_EQ(NameChecksum("Grind"), 0x255ED86Fu);
	CHECK_EQ(NameChecksum("slide"), 0x8D10119Du);
	CHECK_EQ(NameChecksum("GrindTrickList"), 0x2AB3341Du);
	CHECK_EQ(NameChecksum("SkaterState"), 0x43686585u);
	CHECK_EQ(NameChecksum("SkaterFlipAndRotate"), 0x488AF07Au);
	CHECK_EQ(NameChecksum("SKATERCOREPHYSICS"), 0x5BD63B29u);
}

static void testGrindTable(const std::string& scriptDir) {
	g_case = "grind table";
	std::string list, scripts;
	const bool haveList = readFile(scriptDir + "/grindlist.q", list);
	const bool haveScripts = readFile(scriptDir + "/grindscripts.q", scripts);
	CHECK(haveList);
	CHECK(haveScripts);
	if (!haveList || !haveScripts) {
		printf("  the game's grind scripts were not found in %s (pass the folder as the first argument)\n", scriptDir.c_str());
		return;
	}

	// Template1: the sixteen endings
	std::vector<std::string> t1 = arrayWords(list, "Template1");
	std::vector<std::string> endings;
	for (size_t i = 0; i < t1.size(); i++) if (t1[i] != "[" && t1[i] != "]") endings.push_back(t1[i]);
	CHECK_EQ(endings.size(), 16);

	// GrindTrickList: nine rows of sixteen scripts
	std::vector<std::string> words = arrayWords(list, "GrindTrickList");
	std::vector<std::vector<std::string> > rows;
	int depth = 0;
	for (size_t i = 0; i < words.size(); i++) {
		if (words[i] == "[") { depth++; if (depth == 2) rows.push_back(std::vector<std::string>()); }
		else if (words[i] == "]") depth--;
		else if (depth == 2) rows.back().push_back(lower(words[i]));
	}
	CHECK_EQ(rows.size(), 9);
	for (size_t r = 0; r < rows.size(); r++) CHECK_EQ(rows[r].size(), 16);
	if (endings.size() != 16 || rows.size() != 9) return;

	const std::string lowerScripts = lower(scripts);

	struct Want { int grind; const char* word; const char* other; };
	const Want wants[] = {
		{ GRIND_FIVE_O, "trick_5_0", "trick_nosegrind" },         // ridden backwards the game turns one into the other
		{ GRIND_NOSEGRIND, "trick_nosegrind", "trick_5_0" },
		{ GRIND_CROOKED, "trick_crooked", 0 },
		{ GRIND_OVERCROOK, "trick_ngcrook", 0 },
		{ GRIND_FEEBLE, "trick_feeble", 0 },
		{ GRIND_SMITH, "trick_smith", 0 },
		{ GRIND_TAILSLIDE, "trick_tailslide", 0 },
		{ GRIND_NOSESLIDE, "trick_noseslide", 0 },
		{ GRIND_BOARDLIP, "trick_boardslide", "trick_lipslide" },
	};

	for (int sub = 0; sub < 16; sub++) {
		const bool left = (sub & 1) != 0, parallel = (sub & 2) != 0, backwards = (sub & 4) != 0, flipped = (sub & 8) != 0;

		// the endings the mod builds are the game's
		CHECK(std::string(GrindSuffix(left, backwards, flipped)) == endings[sub]);

		for (size_t w = 0; w < sizeof(wants) / sizeof(wants[0]); w++) {
			GrindPlan plan;
			PlanGrind(wants[w].grind, left, plan);
			CHECK(plan.row >= 0 && plan.row <= 8);
			if (plan.row < 0 || plan.row > 8) continue;
			const bool p = plan.parallel < 0 ? parallel : plan.parallel != 0;
			const int index = (left ? 1 : 0) | (p ? 2 : 0) | (backwards ? 4 : 0) | (flipped ? 8 : 0);
			const std::string& script = rows[plan.row][index];
			const bool ok = script.find(wants[w].word) == 0 || (wants[w].other && script.find(wants[w].other) == 0);
			CHECK(ok);
			if (!ok) printf("    %s, entry %d: the table has %s\n", GrindName(wants[w].grind), sub, script.c_str());
			// forward-facing: exactly the grind that was asked for
			if (!backwards && wants[w].grind != GRIND_BOARDLIP) CHECK(script.find(wants[w].word) == 0);
		}

		// nothing asked for: the 50-50 along the rail, a slide across it
		{
			GrindPlan plan;
			PlanGrind(GRIND_FIFTY, left, plan);
			CHECK_EQ(plan.row, 0);
			CHECK_EQ(plan.parallel, -1);
			const std::string& script = rows[0][sub];
			if (parallel) CHECK(script.find("trick_5050") == 0);
			else CHECK(script.find("trick_boardslide") == 0 || script.find("trick_lipslide") == 0);
		}

		// the three that are in no row: the scripts are there under the names the mod builds
		const int byName[] = { GRIND_BLUNTSLIDE, GRIND_NOSEBLUNT, GRIND_SALAD };
		for (size_t k = 0; k < 3; k++) {
			GrindPlan plan;
			PlanGrind(byName[k], left, plan);
			CHECK_EQ(plan.row, -1);
			CHECK(plan.prefix != 0);
			if (!plan.prefix) continue;
			const std::string name = lower(std::string(plan.prefix) + GrindSuffix(left, backwards, flipped));
			const bool found = lowerScripts.find("\nscript " + name + " ") != std::string::npos;
			CHECK(found);
			if (!found) printf("    no script %s\n", name.c_str());
			// and what is started instead if a game does not have them is in the table
			GrindPlan fb;
			PlanGrind(plan.fallback, left, fb);
			CHECK(fb.row >= 0);
		}
	}

	// toward and away: with the rail on the left the game's own buttons for a crooked are Up + Left
	GrindPlan plan;
	PlanGrind(GRIND_CROOKED, true, plan);
	CHECK_EQ(plan.row, 5);
	PlanGrind(GRIND_CROOKED, false, plan);
	CHECK_EQ(plan.row, 6);
}

// ---------------------------------------------------------------------------------------------
// The tilt tracker, through the recognizer
// ---------------------------------------------------------------------------------------------

// Ollie with a flick straight up, let the stick spring back, and be airborne
static void ollieIntoAir(Sim& s, float flickTo = U, bool springBack = true) {
	s.toward(D, 60);
	s.hold(200);
	s.toward(flickTo, 35);
	// the game takes a frame or two to leave the ground
	s.step();
	s.state = STATE_AIR;
	if (springBack) {
		s.spring();
		s.hold(120);
	}
}

static void testTiltTracking(double hz) {
	char name[128];

	snprintf(name, sizeof(name), "tilt: nothing held, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.hold(200);
		CHECK(!s.rec.GetTilt().valid);
		CHECK_EQ(s.land(), GRIND_FIFTY);
	}

	snprintf(name, sizeof(name), "tilt: straight pushes, %g Hz", hz);
	g_case = name;
	{
		struct Case { float angle; int grindLeft, grindRight; };
		const Case cases[] = {
			{ U, GRIND_NOSEGRIND, GRIND_NOSEGRIND },
			{ D, GRIND_FIVE_O, GRIND_FIVE_O },
			{ UL, GRIND_CROOKED, GRIND_OVERCROOK },
			{ UR, GRIND_OVERCROOK, GRIND_CROOKED },
			{ DL, GRIND_FIVE_O, GRIND_SALAD },
			{ DR, GRIND_SALAD, GRIND_FIVE_O },
			{ L, GRIND_BOARDLIP, GRIND_BOARDLIP },
			{ R, GRIND_BOARDLIP, GRIND_BOARDLIP },
		};
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			for (int side = 0; side < 2; side++) {
				// held for a while, and pushed only just before the board touches down
				for (int quick = 0; quick < 2; quick++) {
					Sim s(hz);
					ollieIntoAir(s);
					s.toward(cases[i].angle, 50);
					s.hold(quick ? 20 : 300);
					CHECK(s.rec.GetTilt().valid);
					CHECK_EQ(s.land(side == 0), side == 0 ? cases[i].grindLeft : cases[i].grindRight);
				}
			}
		}
	}

	snprintf(name, sizeof(name), "tilt: rolls, %g Hz", hz);
	g_case = name;
	{
		struct Case { float first, then; int grindLeft, grindRight; };
		const Case cases[] = {
			{ U, L, GRIND_NOSESLIDE, GRIND_NOSEBLUNT },
			{ U, R, GRIND_NOSEBLUNT, GRIND_NOSESLIDE },
			{ D, L, GRIND_TAILSLIDE, GRIND_BLUNTSLIDE },
			{ D, R, GRIND_BLUNTSLIDE, GRIND_TAILSLIDE },
			{ L, U, GRIND_FEEBLE, GRIND_SMITH },
			{ R, U, GRIND_SMITH, GRIND_FEEBLE },
			{ L, D, GRIND_CROOKED, GRIND_OVERCROOK },
			{ R, D, GRIND_OVERCROOK, GRIND_CROOKED },
			{ U, UL, GRIND_CROOKED, GRIND_OVERCROOK },      // a roll that stops at the diagonal
			{ D, DR, GRIND_SALAD, GRIND_FIVE_O },
		};
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			for (int side = 0; side < 2; side++) {
				Sim s(hz);
				ollieIntoAir(s);
				s.toward(cases[i].first, 50);
				s.hold(80);
				s.arcTo(cases[i].then, 120);
				s.hold(60);
				CHECK(s.rec.GetTilt().valid);
				CHECK_EQ(s.land(side == 0), side == 0 ? cases[i].grindLeft : cases[i].grindRight);
			}
		}
	}

	snprintf(name, sizeof(name), "tilt: the end of a flick is not a tilt, %g Hz", hz);
	g_case = name;
	{
		// Flick up-right (kickflip) and keep the stick there
		Sim s(hz);
		ollieIntoAir(s, UR, false);
		s.hold(60);
		CHECK(!s.rec.GetTilt().valid);          // too soon after take-off
		CHECK_EQ(GrindFromTilt(s.rec.GetTilt(), true), GRIND_FIFTY);
		s.hold(200);
		CHECK(s.rec.GetTilt().valid);           // held on purpose by now
		CHECK_EQ(s.rec.GetTilt().first, TILT_UP);
		CHECK_EQ(GrindFromTilt(s.rec.GetTilt(), true), GRIND_OVERCROOK);
	}
	{
		// The usual case: the stick springs back, then tilts
		Sim s(hz);
		ollieIntoAir(s, UR);
		CHECK(!s.rec.GetTilt().valid);
		s.toward(D, 50);
		s.hold(30);
		CHECK(s.rec.GetTilt().valid);
		CHECK_EQ(s.land(), GRIND_FIVE_O);
	}

	snprintf(name, sizeof(name), "tilt: back to the centre clears it, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.toward(U, 50);
		s.hold(100);
		CHECK(s.rec.GetTilt().valid);
		s.spring();
		s.hold(50);
		CHECK(!s.rec.GetTilt().valid);
		// and the next push starts over: left first, then up
		s.toward(L, 50);
		s.hold(80);
		s.arcTo(U, 100);
		s.hold(50);
		CHECK_EQ(s.rec.GetTilt().first, TILT_SIDE);
		CHECK(s.rec.GetTilt().firstLeft);
		CHECK_EQ(s.land(true), GRIND_FEEBLE);
	}

	snprintf(name, sizeof(name), "tilt: only in the air, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		s.toward(L, 50);
		s.hold(300);
		CHECK(!s.rec.GetTilt().valid);          // on the ground
		s.state = STATE_AIR;                    // rolled off a ledge with the stick held to the side
		s.hold(50);
		CHECK(!s.rec.GetTilt().valid);          // held since before take-off: has to stay a while
		s.hold(200);
		CHECK(s.rec.GetTilt().valid);
		s.state = STATE_RAIL;
		s.hold(30);
		CHECK(!s.rec.GetTilt().valid);
	}

	snprintf(name, sizeof(name), "tilt: a light touch is no tilt, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.toward(U, 50, 0.4f);
		s.hold(200);
		CHECK(!s.rec.GetTilt().valid);
		s.toward(U, 30, 0.6f);
		s.hold(30);
		CHECK(s.rec.GetTilt().valid);
		s.toward(U, 30, 0.4f);                  // easing off a little keeps it
		s.hold(100);
		CHECK(s.rec.GetTilt().valid);
		s.toward(U, 30, 0.3f);
		s.hold(30);
		CHECK(!s.rec.GetTilt().valid);
	}
}

// ---------------------------------------------------------------------------------------------
// Buttons in the air
// ---------------------------------------------------------------------------------------------

static void testAir(double hz) {
	char name[128];

	snprintf(name, sizeof(name), "air: tilting presses nothing, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		const size_t m = s.mark();
		s.toward(D, 50);
		s.hold(300);
		s.arcTo(L, 100);
		s.hold(200);
		s.spring();
		s.toward(U, 50);
		s.hold(300);
		CHECK_EQ(s.count(&Output::ollie, m), 0);
		CHECK_EQ(s.count(&Output::grind, m), 0);
		CHECK_EQ(s.count(&Output::grab, m), 0);
		CHECK_EQ(s.count(&Output::nollie, m), 0);
	}

	snprintf(name, sizeof(name), "air: no grind button when the game catches rails, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.hold(2500);
		CHECK_EQ(s.count(&Output::grind), 0);
	}
	{
		// ... and the timed hold as before when it does not
		Sim s(hz);
		s.railCatch = false;
		ollieIntoAir(s);
		s.hold(1000);
		CHECK(s.count(&Output::grind) > 0);
		// it ends on the rail
		const size_t m = s.mark();
		s.landOn(GRIND_FIFTY);
		s.hold(500);
		CHECK_EQ(s.count(&Output::grind, m), 0);
	}

	snprintf(name, sizeof(name), "air: a flick in the air is a flip, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		const size_t m = s.mark();
		s.toward(D, 50);
		s.hold(120);
		s.toward(UR, 35);
		s.spring();
		s.hold(300);
		CHECK_EQ(s.count(&Output::ollie, m), 0);
		std::vector<Press> flips = s.presses(&Output::flip, m);
		CHECK_EQ(flips.size(), 1);
		if (flips.size() == 1) {
			CHECK_EQ(flips[0].dir, DIR_L);      // kickflip slot
			CHECK(flips[0].frames >= 2);
		}
	}

	snprintf(name, sizeof(name), "air: a slow move across the stick is tilt, not a flip, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		const size_t m = s.mark();
		s.toward(D, 50);
		s.hold(150);
		s.toward(UR, 350);                      // from a tail tilt to the nose, taking its time
		s.hold(200);
		CHECK_EQ(s.count(&Output::flip, m), 0);
		CHECK_EQ(s.gestures(m).size(), 0);
		CHECK(s.rec.GetTilt().valid);
	}

	snprintf(name, sizeof(name), "air: rolling the stick to a side is tilt, not a shove-it, %g Hz", hz);
	g_case = name;
	{
		for (int side = 0; side < 2; side++) {
			Sim s(hz);
			ollieIntoAir(s);
			const size_t m = s.mark();
			s.toward(D, 50);
			s.hold(120);
			s.arcTo(side ? R : L, 90);
			s.hold(300);
			CHECK_EQ(s.count(&Output::flip, m), 0);
			CHECK_EQ(s.count(&Output::ollie, m), 0);
			CHECK_EQ(s.gestures(m).size(), 0);
			CHECK(s.rec.GetTilt().valid);
			CHECK_EQ(s.rec.GetTilt().first, TILT_DOWN);
			CHECK_EQ(s.land(true), side ? GRIND_BLUNTSLIDE : GRIND_TAILSLIDE);
		}
	}
	{
		// The same drag that started on the ground is the pop shove-it it always was
		Sim s(hz);
		s.toward(D, 60);
		s.hold(200);
		s.arcTo(R, 90);
		s.hold(150);
		s.state = STATE_AIR;
		s.spring();
		s.hold(300);
		std::vector<Press> flips = s.presses(&Output::flip);
		CHECK_EQ(flips.size(), 1);
		if (flips.size() == 1) CHECK_EQ(flips[0].dir, DIR_D);
	}

	snprintf(name, sizeof(name), "air: stick down through a landing is a crouch, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.toward(D, 50);
		s.hold(200);
		const size_t m = s.mark();
		CHECK_EQ(s.count(&Output::ollie, m - 1), 0);
		s.state = STATE_GROUND;                 // missed the rail
		s.hold(150);
		CHECK_EQ(s.count(&Output::ollie, m), (int)(s.mark() - m));   // held from the first frame on the ground
		s.spring();                             // let go: ollie
		s.hold(100);
		CHECK(!s.outs.back().ollie);
		CHECK_EQ(s.presses(&Output::ollie, m).size(), 1);
	}

	snprintf(name, sizeof(name), "air: a crouch carried off a ledge, %g Hz", hz);
	g_case = name;
	{
		// Flicked soon after the edge: still an ollie for the game, with the flip
		Sim s(hz);
		s.toward(D, 60);
		s.hold(200);
		s.state = STATE_AIR;
		s.hold(150);
		CHECK(s.outs.back().ollie);
		s.toward(UL, 35);
		s.spring();
		s.hold(300);
		CHECK_EQ(s.presses(&Output::ollie).size(), 1);
		std::vector<Press> flips = s.presses(&Output::flip);
		CHECK_EQ(flips.size(), 1);
		if (flips.size() == 1) CHECK_EQ(flips[0].dir, DIR_R);
	}
	{
		// Held all the way down: the button is let go in the air, late enough to do nothing,
		// and the rail is not ridden crouched
		Sim s(hz);
		s.toward(D, 60);
		s.hold(200);
		s.state = STATE_AIR;
		const size_t air = s.mark();
		s.hold(300);
		CHECK(s.outs.back().ollie);             // within the game's late-jump time: still down
		s.hold(250);
		CHECK(!s.outs.back().ollie);
		int release = -1;
		for (size_t i = air; i < s.outs.size(); i++) {
			if (s.outs[i - 1].ollie && !s.outs[i].ollie) release = (int)i;
		}
		CHECK(release >= 0);
		if (release >= 0) CHECK(s.msBetween((int)air, release) >= 380.0);
		CHECK_EQ(s.gestures().size(), 0);       // not a pop
		const size_t m = s.mark();
		s.land();
		s.hold(400);
		s.spring();
		s.hold(400);
		CHECK_EQ(s.count(&Output::ollie, m), 0);
	}
	{
		// A hop too short for that: the crouch stays a crouch, as in the game
		Sim s(hz);
		s.toward(D, 60);
		s.hold(200);
		s.state = STATE_AIR;
		s.hold(150);
		s.state = STATE_GROUND;
		s.hold(200);
		CHECK_EQ(s.presses(&Output::ollie).size(), 1);
		CHECK(s.outs.back().ollie);
	}
}

// ---------------------------------------------------------------------------------------------
// On a rail: only a flick pops
// ---------------------------------------------------------------------------------------------

// Land on a rail with the stick wherever it is and let the grind settle
static void settleOnRail(Sim& s, int grind) {
	s.landOn(grind);
	s.hold(100);
}

// The Ollie press of a pop out of a grind, and what follows it
static void expectRailPop(Sim& s, size_t from, int slot) {
	std::vector<Press> ollies = s.presses(&Output::ollie, from);
	CHECK_EQ(ollies.size(), 1);
	if (ollies.size() != 1) return;
	CHECK(ollies[0].frames >= 2);
	CHECK(ollies[0].frames * 1000.0 / s.hz >= s.cfg.railTap - 1000.0 / s.hz);
	CHECK(ollies[0].frames * 1000.0 / s.hz <= s.cfg.railTap + 3000.0 / s.hz + 1.0);
	const int release = ollies[0].first + ollies[0].frames;

	std::vector<Press> flips = s.presses(&Output::flip, from);
	if (slot == DIR_NONE) {
		CHECK_EQ(flips.size(), 0);
	}
	else {
		CHECK_EQ(flips.size(), 1);
		if (flips.size() == 1) {
			CHECK_EQ(flips[0].dir, slot);
			CHECK(flips[0].frames >= 2);
			// the direction goes down after Ollie is up, the button after the direction
			int dirFirst = flips[0].first;
			while (dirFirst > 0 && s.outs[dirFirst - 1].dir == slot) dirFirst--;
			CHECK(dirFirst > release);
			CHECK(flips[0].first > dirFirst);
			CHECK(s.msBetween(release, flips[0].first) <= 150.0);
		}
	}
	// while Ollie is down nothing else is
	for (int i = ollies[0].first; i <= release && i < (int)s.outs.size(); i++) {
		CHECK(!s.outs[i].flip);
		CHECK(!s.outs[i].grab);
		CHECK(!s.outs[i].grind);
		CHECK_EQ(s.outs[i].dir, DIR_NONE);
	}
}

static void testRailPop(double hz) {
	char name[128];

	snprintf(name, sizeof(name), "rail: holding and letting go never pops, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.toward(D, 50);
		s.hold(100);
		const size_t m = s.mark();
		CHECK_EQ(s.land(), GRIND_FIVE_O);
		s.hold(800);
		s.spring();
		s.hold(600);
		s.toward(U, 250);
		s.hold(500);
		s.spring(150);                          // slowly back
		s.hold(400);
		s.toward(D, 150);
		s.hold(300);
		s.spring(8);                            // and let it snap back
		s.hold(400);
		s.toward(DL, 60);
		s.hold(300);
		s.arcTo(DR, 200);
		s.spring();
		s.hold(400);
		CHECK_EQ(s.count(&Output::ollie, m), 0);
		CHECK_EQ(s.count(&Output::nollie, m), 0);
		CHECK_EQ(s.gestures(m).size(), 0);
		for (size_t i = m; i < s.outs.size(); i++) CHECK(!s.outs[i].ownsDirection);
	}

	snprintf(name, sizeof(name), "rail: a slow move from one end to the other is no flick, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		settleOnRail(s, GRIND_FIFTY);
		const size_t m = s.mark();
		s.toward(D, 60);
		s.hold(400);
		s.spring(200);
		s.hold(250);
		s.toward(U, 200);
		s.hold(400);
		// and without a rest in the middle, as long as it takes its time
		s.toward(D, 450);
		s.hold(400);
		s.arcTo(L, 300);                        // or round the rim
		s.arcTo(U, 300);
		s.hold(400);
		CHECK_EQ(s.count(&Output::ollie, m), 0);
		CHECK_EQ(s.gestures(m).size(), 0);
	}

	snprintf(name, sizeof(name), "rail: how fast is a flick, %g Hz", hz);
	g_case = name;
	{
		// Across the stick in a straight line, from the tail to the nose, in so many milliseconds
		const double times[] = { 30, 60, 100, 150, 320, 450, 700 };
		for (size_t i = 0; i < sizeof(times) / sizeof(times[0]); i++) {
			Sim s(hz);
			ollieIntoAir(s);
			settleOnRail(s, GRIND_FIFTY);
			const size_t m = s.mark();
			s.toward(D, 60);
			s.hold(200);
			s.toward(U, times[i]);
			s.hold(400);
			const bool popped = s.count(&Output::ollie, m) > 0;
			CHECK_EQ(popped, times[i] <= 150);
		}
		// Round the rim from the tail to the side, a quarter of the way round
		const double rolls[] = { 50, 90, 130, 260, 400, 800 };
		for (size_t i = 0; i < sizeof(rolls) / sizeof(rolls[0]); i++) {
			Sim s(hz);
			ollieIntoAir(s);
			settleOnRail(s, GRIND_FIFTY);
			const size_t m = s.mark();
			s.toward(D, 60);
			s.hold(200);
			s.arcTo(R, rolls[i]);
			s.hold(400);
			const bool popped = s.count(&Output::ollie, m) > 0;
			CHECK_EQ(popped, rolls[i] <= 130);
		}
	}

	snprintf(name, sizeof(name), "rail: flick up pops, %g Hz", hz);
	g_case = name;
	{
		struct Case { float from, to; int slot; };
		const Case cases[] = {
			{ D, U, DIR_NONE },
			{ D, UR, DIR_L },       // kickflip out
			{ D, UL, DIR_R },       // heelflip out
			{ DL, UR, DIR_DL },     // varial kickflip out
			{ DR, U, DIR_UL },      // hardflip out
		};
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			for (int held = 0; held < 2; held++) {
				Sim s(hz);
				ollieIntoAir(s);
				settleOnRail(s, GRIND_FIFTY);
				const size_t m = s.mark();
				s.toward(cases[i].from, 60);
				s.hold(150);
				s.toward(cases[i].to, 35);
				if (held) s.hold(150);
				s.spring();
				s.hold(250);
				CHECK_EQ(s.state, STATE_AIR);       // the jump took the board off the rail
				expectRailPop(s, m, cases[i].slot);
				std::vector<Output> g = s.gestures(m);
				CHECK_EQ(g.size(), 1);
				if (g.size() == 1) {
					CHECK_EQ(g[0].gesture, cases[i].slot == DIR_NONE ? GESTURE_POP : GESTURE_FLIP);
					CHECK_EQ(g[0].state, STATE_RAIL);
				}
			}
		}
	}

	snprintf(name, sizeof(name), "rail: flick down from the nose pops too, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		CHECK_EQ(s.cfg.nollie, 0);              // the nollie setting is for the ground
		ollieIntoAir(s);
		s.toward(U, 50);
		s.hold(100);
		const size_t m = s.mark();
		CHECK_EQ(s.land(), GRIND_NOSEGRIND);
		s.hold(500);
		s.toward(D, 35);
		s.spring();
		s.hold(250);
		expectRailPop(s, m, DIR_NONE);
		CHECK_EQ(s.count(&Output::nollie, m), 0);
	}
	{
		// ... with the mirrored flip: up, flick down-right is the kickflip slot
		Sim s(hz);
		ollieIntoAir(s);
		settleOnRail(s, GRIND_NOSEGRIND);
		const size_t m = s.mark();
		s.toward(U, 60);
		s.hold(300);
		s.toward(DR, 35);
		s.spring();
		s.hold(250);
		expectRailPop(s, m, DIR_L);
	}
	{
		// Off the rail it is not a wind-up
		Sim s(hz);
		ollieIntoAir(s);
		settleOnRail(s, GRIND_NOSEGRIND);
		s.toward(U, 60);
		s.hold(200);
		s.state = STATE_AIR;                    // the rail ended
		s.hold(50);
		s.state = STATE_GROUND;
		const size_t m = s.mark();
		s.hold(200);
		s.spring();                             // on a rail this would be watched for a flick
		s.hold(250);
		s.toward(U, 60);                        // stick up on the ground is nothing
		s.hold(200);
		s.toward(UL, 35, 0.3f);
		s.spring();
		s.hold(250);
		CHECK_EQ(s.count(&Output::ollie, m), 0);
		CHECK_EQ(s.count(&Output::nollie, m), 0);
		CHECK_EQ(s.gestures(m).size(), 0);
	}

	snprintf(name, sizeof(name), "rail: drag to the side is a shove-it out, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		settleOnRail(s, GRIND_FIFTY);
		const size_t m = s.mark();
		s.toward(D, 60);
		s.hold(150);
		s.arcTo(R, 90);
		s.hold(150);
		s.spring();
		s.hold(250);
		expectRailPop(s, m, DIR_D);
	}

	snprintf(name, sizeof(name), "rail: the stick held from the air needs no trip through the centre, %g Hz", hz);
	g_case = name;
	{
		// Ollie, keep the flick held up, land in a nosegrind, flick down
		Sim s(hz);
		ollieIntoAir(s, U, false);
		s.hold(300);
		const size_t m = s.mark();
		CHECK_EQ(s.land(), GRIND_NOSEGRIND);
		s.hold(400);
		s.toward(D, 35);
		s.spring();
		s.hold(250);
		expectRailPop(s, m, DIR_NONE);
	}

	snprintf(name, sizeof(name), "rail: two pops on two rails, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		for (int round = 0; round < 2; round++) {
			settleOnRail(s, GRIND_FIFTY);
			const size_t m = s.mark();
			s.toward(D, 60);
			s.hold(120);
			s.toward(UR, 35);
			s.spring();
			s.hold(60);
			s.state = STATE_AIR;
			s.hold(400);
			expectRailPop(s, m, DIR_L);
		}
	}

	snprintf(name, sizeof(name), "rail: a crouch that lands on a rail after a short hop, %g Hz", hz);
	g_case = name;
	{
		// Ollie already held when the board gets there: letting go pops, as in the game
		Sim s(hz);
		s.toward(D, 60);
		s.hold(200);
		s.state = STATE_AIR;
		s.hold(150);
		s.landOn(GRIND_FIVE_O);
		s.hold(300);
		CHECK(s.outs.back().ollie);
		s.spring();
		s.hold(100);
		CHECK(!s.outs.back().ollie);
		CHECK_EQ(s.presses(&Output::ollie).size(), 1);
	}
}

static void testRailPopPatterns(double hz) {
	char name[128];
	// Made-up patterns in the file format, not the ones of any game
	static const char text[] =
		"global_tolerance_dist 0.45\n"
		"pattern Ollie\n coord 0.0 0.9\n coord 0.0 -0.9\n"
		"pattern Kickflip\n coord 0.0 0.9\n coord 0.65 -0.65\n"
		"pattern PopShuvit\n coord 0.0 0.9\n coord 0.65 0.65\n coord 0.9 0.0\n"
		"pattern Nollie\n coord 0.0 -0.9\n coord 0.0 0.9\n"
		"pattern N_Kickflip\n coord 0.0 -0.9\n coord 0.65 0.65\n";
	PatternSet set;
	set.Parse(text, sizeof(text) - 1);

	snprintf(name, sizeof(name), "rail, patterns: flicks pop, %g Hz", hz);
	g_case = name;
	CHECK_EQ(set.Count(), 5);
	{
		struct Case { float from, to; int slot; const char* pattern; };
		const Case cases[] = {
			{ D, U, DIR_NONE, "Ollie" },
			{ D, UR, DIR_L, "Kickflip" },
			{ U, D, DIR_NONE, "Nollie" },       // from the top: followed on a rail even with Nollie=0
			{ U, DR, DIR_L, "N_Kickflip" },
		};
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			Sim s(hz);
			s.rec.UsePatterns(&set);
			ollieIntoAir(s);
			settleOnRail(s, GRIND_FIFTY);
			const size_t m = s.mark();
			s.toward(cases[i].from, 60);
			s.hold(150);
			s.toward(cases[i].to, 50);
			s.spring();
			s.hold(250);
			expectRailPop(s, m, cases[i].slot);
			std::vector<Output> g = s.gestures(m);
			CHECK_EQ(g.size(), 1);
			if (g.size() == 1) {
				CHECK(g[0].pattern != 0);
				if (g[0].pattern) CHECK(std::string(g[0].pattern) == cases[i].pattern);
			}
		}
	}

	snprintf(name, sizeof(name), "rail, patterns: letting go does not pop, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		s.rec.UsePatterns(&set);
		ollieIntoAir(s);
		settleOnRail(s, GRIND_FIFTY);
		const size_t m = s.mark();
		s.toward(D, 60);
		s.hold(400);
		s.spring();
		s.hold(500);
		s.toward(U, 60);
		s.hold(400);
		s.spring();
		s.hold(500);
		CHECK_EQ(s.count(&Output::ollie, m), 0);
		CHECK_EQ(s.gestures(m).size(), 0);
	}
	{
		// ... while on the ground it still does
		Sim s(hz);
		s.rec.UsePatterns(&set);
		s.toward(D, 60);
		s.hold(300);
		s.spring();
		s.hold(400);
		CHECK_EQ(s.presses(&Output::ollie).size(), 1);
		CHECK_EQ(s.gestures().size(), 1);
	}
}

// ---------------------------------------------------------------------------------------------
// On a rail: switching between 50-50, 5-0 and nosegrind
// ---------------------------------------------------------------------------------------------

static void expectSequence(Sim& s, size_t from, int target) {
	std::vector<Press> flip = s.presses(&Output::flip, from);
	std::vector<Press> grab = s.presses(&Output::grab, from);
	std::vector<Press> grind = s.presses(&Output::grind, from);
	const Press* a = 0;
	const Press* b = 0;
	if (target == GRIND_FIVE_O) {
		CHECK_EQ(flip.size(), 1); CHECK_EQ(grab.size(), 1); CHECK_EQ(grind.size(), 0);
		if (flip.size() == 1 && grab.size() == 1) { a = &flip[0]; b = &grab[0]; }
	}
	else if (target == GRIND_NOSEGRIND) {
		CHECK_EQ(flip.size(), 0); CHECK_EQ(grab.size(), 1); CHECK_EQ(grind.size(), 1);
		if (grind.size() == 1 && grab.size() == 1) { a = &grind[0]; b = &grab[0]; }
	}
	else {
		CHECK_EQ(flip.size(), 0); CHECK_EQ(grab.size(), 0); CHECK_EQ(grind.size(), 2);
		if (grind.size() == 2) { a = &grind[0]; b = &grind[1]; }
	}
	if (!a || !b) return;
	CHECK(a->frames >= 2);
	CHECK(b->frames >= 2);
	CHECK(b->first >= a->first + a->frames + 1);                  // the first is up before the second goes down
	CHECK(s.msBetween(a->first, b->first) <= 250.0);              // well inside the game's 300 ms
	for (int i = a->first; i < b->first + b->frames; i++) {
		CHECK(!s.outs[i].ollie);
		CHECK(!s.outs[i].ownsDirection);
	}
}

static void testRailSwitch(double hz) {
	char name[128];

	snprintf(name, sizeof(name), "switch: down, centre, up, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		// stick down right away: nothing is sent before the grind has settled
		s.toward(D, 60);
		size_t m = s.mark();
		s.hold(600);
		CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m), 0);
		s.hold(700);
		expectSequence(s, m, GRIND_FIVE_O);
		CHECK_EQ(s.game.kind, GRIND_FIVE_O);
		CHECK_EQ(s.game.switches, 1);

		// held: nothing more is sent, however long
		m = s.mark();
		s.hold(4000);
		CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m), 0);

		// let go: back to the 50-50
		m = s.mark();
		s.spring();
		s.hold(1500);
		expectSequence(s, m, GRIND_FIFTY);
		CHECK_EQ(s.game.kind, GRIND_FIFTY);

		// up: nosegrind
		m = s.mark();
		s.toward(U, 60);
		s.hold(2500);
		expectSequence(s, m, GRIND_NOSEGRIND);
		CHECK_EQ(s.game.kind, GRIND_NOSEGRIND);

		// and held there, the entry the game forgets to ignore is not hit again
		m = s.mark();
		s.hold(4000);
		CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m), 0);

		CHECK_EQ(s.game.switches, 3);
		CHECK_EQ(s.game.restarts, 0);
		CHECK_EQ(s.count(&Output::ollie, 0) > 0 ? 1 : 0, 1);       // only the ollie that got us here
		int reported = 0;
		for (size_t i = 0; i < s.outs.size(); i++) if (s.outs[i].switchTo != GRIND_UNKNOWN) reported++;
		CHECK_EQ(reported, 3);
	}

	snprintf(name, sizeof(name), "switch: straight from the tail to the nose, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.toward(D, 50);
		s.hold(100);
		CHECK_EQ(s.land(), GRIND_FIVE_O);
		s.hold(1500);
		CHECK_EQ(s.game.switches, 0);           // already what the stick asks for
		s.spring(250);                          // too slow to be a flick
		s.toward(U, 250);
		s.hold(3000);
		CHECK_EQ(s.game.kind, GRIND_NOSEGRIND);
		CHECK_EQ(s.game.restarts, 0);
		CHECK_EQ(s.count(&Output::ollie, 0) > 0 ? 1 : 0, 1);
		CHECK_EQ(s.presses(&Output::ollie).size(), 1);
	}

	snprintf(name, sizeof(name), "switch: a short dip is not a request, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.hold(1200);
		const size_t m = s.mark();
		s.toward(D, 60);
		s.hold(100);
		s.spring();
		s.hold(1500);
		CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m), 0);
		CHECK_EQ(s.game.switches, 0);
	}

	snprintf(name, sizeof(name), "switch: other grinds are left alone, %g Hz", hz);
	g_case = name;
	{
		const int kinds[] = { GRIND_CROOKED, GRIND_BOARDLIP, GRIND_SMITH, GRIND_TAILSLIDE };
		for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++) {
			Sim s(hz);
			ollieIntoAir(s);
			s.toward(UL, 50);
			s.hold(100);
			s.landOn(kinds[k]);
			const size_t m = s.mark();
			s.hold(1500);
			s.spring();
			s.hold(1500);
			s.toward(D, 60);
			s.hold(1500);
			s.toward(U, 150);
			s.hold(1500);
			CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m), 0);
			CHECK_EQ(s.game.kind, kinds[k]);
		}
	}

	snprintf(name, sizeof(name), "switch: a grind the stick had no part in stays, %g Hz", hz);
	g_case = name;
	{
		// e.g. a nosegrind picked with the classic buttons while the stick sat in the centre
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_NOSEGRIND);
		const size_t m = s.mark();
		s.hold(3000);
		CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m), 0);
		CHECK_EQ(s.game.kind, GRIND_NOSEGRIND);
		// once the stick is used it has a say, the centre included
		s.toward(D, 60);
		s.hold(1500);
		CHECK_EQ(s.game.kind, GRIND_FIVE_O);
		s.spring();
		s.hold(1500);
		CHECK_EQ(s.game.kind, GRIND_FIFTY);
	}

	snprintf(name, sizeof(name), "switch: nothing without word from the game, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.game.settleFrames = 1000000;          // never settles, never reports ready
		const size_t m = s.mark();
		s.toward(D, 60);
		s.hold(3000);
		CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m), 0);
	}
	{
		Sim s(hz);
		s.cfg.grindSwitch = 0;
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		const size_t m = s.mark();
		s.toward(D, 60);
		s.hold(3000);
		CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m), 0);
	}

	snprintf(name, sizeof(name), "switch: lost presses go out again, not sooner than the game needs, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.hold(1200);
		// Presses get lost for a while
		s.game.deaf = true;
		const size_t m = s.mark();
		s.toward(D, 60);
		s.hold(700);
		std::vector<Press> flips = s.presses(&Output::flip, m);
		CHECK_EQ(flips.size(), 1);
		s.hold(1500);
		flips = s.presses(&Output::flip, m);
		CHECK(flips.size() >= 2 && flips.size() <= 4);
		for (size_t i = 1; i < flips.size(); i++) {
			CHECK(s.msBetween(flips[i - 1].first, flips[i].first) >= 600.0);
		}
		s.game.deaf = false;
		s.hold(1500);
		CHECK_EQ(s.game.kind, GRIND_FIVE_O);
		CHECK_EQ(s.game.switches, 1);
	}

	snprintf(name, sizeof(name), "switch: a flick during the hold still pops, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.hold(1200);
		const size_t m = s.mark();
		s.toward(D, 60);
		s.hold(260);                            // long enough to ask for the 5-0 ...
		s.toward(UR, 35);                       // ... and then out with a kickflip
		s.spring();
		s.hold(60);
		s.state = STATE_AIR;
		s.game.leave();
		s.hold(400);
		std::vector<Press> ollies = s.presses(&Output::ollie, m);
		CHECK_EQ(ollies.size(), 1);
		if (ollies.size() == 1) {
			CHECK(ollies[0].frames >= 2);
			const int release = ollies[0].first + ollies[0].frames;
			// from the Ollie press on, the only button is the flip of the kickflip
			std::vector<Press> flips = s.presses(&Output::flip, (size_t)ollies[0].first);
			CHECK_EQ(flips.size(), 1);
			if (flips.size() == 1) {
				CHECK_EQ(flips[0].dir, DIR_L);
				CHECK(flips[0].first > release);
			}
			for (size_t i = (size_t)ollies[0].first; i < s.outs.size(); i++) {
				CHECK(!s.outs[i].grind);
				CHECK(!s.outs[i].grab);
			}
		}
	}

	snprintf(name, sizeof(name), "switch: off the rail the buttons are let go, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.hold(1200);
		s.toward(D, 60);
		// run until the first press of the switch is down, then the rail ends
		int guard = 0;
		while (!s.outs.back().flip && guard++ < 1000) s.step();
		CHECK(s.outs.back().flip);
		s.state = STATE_AIR;
		s.game.leave();
		const size_t m = s.mark();
		s.hold(500);
		CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m), 0);
	}
}

// ---------------------------------------------------------------------------------------------
// Grabs around a rail
// ---------------------------------------------------------------------------------------------

static void testGrabs(double hz) {
	char name[128];
	snprintf(name, sizeof(name), "grabs: none on a rail, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.lt = true;                            // grab in the air
		s.hold(300);
		CHECK(s.outs.back().grab);
		size_t m = s.mark();
		s.landOn(GRIND_FIFTY);
		s.hold(200);
		CHECK_EQ(s.count(&Output::grab, m), 0); // let go where the grind begins
		s.lt = false;
		s.hold(100);
		s.rt = true;                            // a trigger on the rail does nothing
		s.hold(300);
		s.rt = false;
		s.hold(100);
		CHECK_EQ(s.count(&Output::grab, m), 0);
		CHECK_EQ(s.gestures(m).size(), 0);

		// held while leaving the rail: the grab starts in the air
		s.lt = true;
		s.hold(200);
		CHECK_EQ(s.count(&Output::grab, m), 0);
		m = s.mark();
		s.state = STATE_AIR;
		s.hold(300);
		std::vector<Press> grabs = s.presses(&Output::grab, m);
		CHECK_EQ(grabs.size(), 1);
		if (grabs.size() == 1) CHECK_EQ(grabs[0].dir, DIR_L);
		CHECK(s.outs.back().grab);
	}
}

// ---------------------------------------------------------------------------------------------
// Without the game's help everything is as it was
// ---------------------------------------------------------------------------------------------

static void testUnknownState(double hz) {
	char name[128];
	snprintf(name, sizeof(name), "unknown state: the old behaviour, %g Hz", hz);
	g_case = name;
	Sim s(hz, STATE_UNKNOWN);
	s.railCatch = false;
	s.toward(D, 60);
	s.hold(250);
	CHECK(s.outs.back().ollie);
	s.toward(UR, 35);
	s.spring();
	s.hold(2500);
	CHECK_EQ(s.presses(&Output::ollie).size(), 1);
	CHECK_EQ(s.presses(&Output::flip).size(), 1);
	CHECK_EQ(s.presses(&Output::grind).size(), 1);
	CHECK(!s.rec.GetTilt().valid);
	// stick down again: crouch, let go: ollie
	const size_t m = s.mark();
	s.toward(D, 60);
	s.hold(200);
	s.spring();
	s.hold(300);
	CHECK_EQ(s.presses(&Output::ollie, m).size(), 1);
	CHECK_EQ(s.gestures(m).size(), 1);
}

// ---------------------------------------------------------------------------------------------
// Losing speed on a rail: Skate's numbers
// ---------------------------------------------------------------------------------------------

static bool near(double a, double b, double tolerance) {
	return fabs(a - b) <= tolerance;
}

static void testDragNumbers() {
	g_case = "drag: what the rail is made of";
	CHECK_EQ(SurfaceOfTerrain(45), SURFACE_RAIL);     // TERRAIN_GRINDMETAL
	CHECK_EQ(SurfaceOfTerrain(43), SURFACE_RAIL);     // TERRAIN_GRINDROUNDMETALPOLE
	CHECK_EQ(SurfaceOfTerrain(41), SURFACE_RAIL);     // TERRAIN_GRINDWIRE
	CHECK_EQ(SurfaceOfTerrain(57), SURFACE_RAIL);     // TERRAIN_GRINDBRASSRAIL
	CHECK_EQ(SurfaceOfTerrain(3), SURFACE_RAIL);      // TERRAIN_METALSMOOTH
	CHECK_EQ(SurfaceOfTerrain(49), SURFACE_RAIL);     // TERRAIN_GRINDPLASTIC
	CHECK_EQ(SurfaceOfTerrain(42), SURFACE_LEDGE);    // TERRAIN_GRINDCONC
	CHECK_EQ(SurfaceOfTerrain(48), SURFACE_LEDGE);    // TERRAIN_GRINDWOOD
	CHECK_EQ(SurfaceOfTerrain(46), SURFACE_LEDGE);    // TERRAIN_GRINDWOODRAILING
	CHECK_EQ(SurfaceOfTerrain(1), SURFACE_LEDGE);     // TERRAIN_CONCSMOOTH
	CHECK_EQ(SurfaceOfTerrain(0), SURFACE_LEDGE);     // TERRAIN_DEFAULT
	CHECK_EQ(SurfaceOfTerrain(47), SURFACE_ROUGH);    // TERRAIN_GRINDWOODLOG
	CHECK_EQ(SurfaceOfTerrain(14), SURFACE_ROUGH);    // TERRAIN_BRICK
	CHECK_EQ(SurfaceOfTerrain(40), SURFACE_ROUGH);    // TERRAIN_GRINDROPE
	// a number the game does not have is a plain ledge
	CHECK_EQ(SurfaceOfTerrain(-1), SURFACE_LEDGE);
	CHECK_EQ(SurfaceOfTerrain(60), SURFACE_LEDGE);
	CHECK_EQ(SurfaceOfTerrain(255), SURFACE_LEDGE);
	// the game has sixty terrains, 0 to 59
	int counts[3] = { 0, 0, 0 };
	for (int t = 0; t < 60; t++) {
		const int surface = SurfaceOfTerrain(t);
		CHECK(surface >= SURFACE_RAIL && surface <= SURFACE_ROUGH);
		if (surface >= SURFACE_RAIL && surface <= SURFACE_ROUGH) counts[surface]++;
	}
	CHECK_EQ(counts[SURFACE_RAIL], 26);
	CHECK_EQ(counts[SURFACE_ROUGH], 13);
	CHECK_EQ(counts[SURFACE_LEDGE], 21);
	CHECK(std::string(SurfaceName(SURFACE_RAIL)) == "rail");
	CHECK(std::string(SurfaceName(SURFACE_LEDGE)) == "ledge");

	g_case = "drag: which grind";
	CHECK_EQ(DragClassOf(GRIND_FIFTY, false), DRAG_TRUCKS);
	CHECK_EQ(DragClassOf(GRIND_FIVE_O, false), DRAG_TRUCKS);
	CHECK_EQ(DragClassOf(GRIND_NOSEGRIND, false), DRAG_TRUCKS);
	CHECK_EQ(DragClassOf(GRIND_UNKNOWN, false), DRAG_TRUCKS);
	CHECK_EQ(DragClassOf(GRIND_CROOKED, false), DRAG_ANGLED);
	CHECK_EQ(DragClassOf(GRIND_OVERCROOK, false), DRAG_ANGLED);
	CHECK_EQ(DragClassOf(GRIND_FEEBLE, false), DRAG_ANGLED);
	CHECK_EQ(DragClassOf(GRIND_SMITH, false), DRAG_ANGLED);
	CHECK_EQ(DragClassOf(GRIND_SALAD, false), DRAG_ANGLED);
	CHECK_EQ(DragClassOf(GRIND_OTHER, false), DRAG_ANGLED);
	CHECK_EQ(DragClassOf(GRIND_BOARDLIP, true), DRAG_BOARDSLIDE);
	CHECK_EQ(DragClassOf(GRIND_FIFTY, true), DRAG_BOARDSLIDE);      // no lean asked for, and the board landed across the rail
	CHECK_EQ(DragClassOf(GRIND_TAILSLIDE, true), DRAG_TIPSLIDE);
	CHECK_EQ(DragClassOf(GRIND_NOSESLIDE, true), DRAG_TIPSLIDE);
	CHECK_EQ(DragClassOf(GRIND_BLUNTSLIDE, true), DRAG_TIPSLIDE);
	CHECK_EQ(DragClassOf(GRIND_NOSEBLUNT, true), DRAG_TIPSLIDE);
	CHECK_EQ(DragClassOf(GRIND_UNKNOWN, true), DRAG_TIPSLIDE);
	CHECK_EQ(DragClassOf(GRIND_OTHER, true), DRAG_TIPSLIDE);

	g_case = "drag: the first four seconds";
	CHECK(near(DragRamp(0.0f), 0.63, 0.001));
	CHECK(near(DragRamp(-5.0f), 0.63, 0.001));
	CHECK(near(DragRamp(0.5f), 0.675, 0.001));
	CHECK(near(DragRamp(1.0f), 0.72, 0.001));
	CHECK(near(DragRamp(2.2f), 0.83, 0.001));
	CHECK(near(DragRamp(3.1f), 0.915, 0.001));
	CHECK(near(DragRamp(4.0f), 1.0, 0.001));
	CHECK(near(DragRamp(100.0f), 1.0, 0.001));
	float last = DragRamp(0.0f);
	for (int i = 1; i <= 500; i++) {
		const float share = DragRamp((float)i * 0.01f);
		CHECK(share >= last && share <= 1.0f);
		last = share;
	}

	g_case = "drag: Skate's numbers";
	// m/s^2 on a level rail when the board touches down and from four seconds on, as found in Skate 3
	static const struct { int dragClass; int surface; double start; double full; } want[] = {
		{ DRAG_TRUCKS, SURFACE_RAIL, 0.79, 1.25 },
		{ DRAG_TRUCKS, SURFACE_LEDGE, 1.37, 2.16 },
		{ DRAG_TRUCKS, SURFACE_ROUGH, 1.78, 2.82 },
		{ DRAG_BOARDSLIDE, SURFACE_RAIL, 0.87, 1.37 },
		{ DRAG_BOARDSLIDE, SURFACE_LEDGE, 1.88, 2.97 },
		{ DRAG_TIPSLIDE, SURFACE_RAIL, 0.94, 1.49 },
		{ DRAG_TIPSLIDE, SURFACE_LEDGE, 2.05, 3.24 },
		{ DRAG_ANGLED, SURFACE_RAIL, 1.10, 1.74 },
		{ DRAG_ANGLED, SURFACE_LEDGE, 2.73, 4.32 },
	};
	for (size_t i = 0; i < sizeof(want) / sizeof(want[0]); i++) {
		CHECK(near(SkateDrag(want[i].dragClass, want[i].surface, 0.0f), want[i].start, 0.02));
		CHECK(near(SkateDrag(want[i].dragClass, want[i].surface, 4.0f), want[i].full, 0.02));
		CHECK(near(SkateDrag(want[i].dragClass, want[i].surface, 60.0f), want[i].full, 0.02));
	}
	// least on the trucks, most with truck and deck on the rail; a rail slides furthest, a rough ledge least far
	for (int surface = SURFACE_RAIL; surface <= SURFACE_ROUGH; surface++) {
		CHECK(SkateDrag(DRAG_TRUCKS, surface, 2.0f) < SkateDrag(DRAG_BOARDSLIDE, surface, 2.0f));
		CHECK(SkateDrag(DRAG_BOARDSLIDE, surface, 2.0f) < SkateDrag(DRAG_TIPSLIDE, surface, 2.0f));
		CHECK(SkateDrag(DRAG_TIPSLIDE, surface, 2.0f) < SkateDrag(DRAG_ANGLED, surface, 2.0f));
	}
	for (int dragClass = DRAG_TRUCKS; dragClass <= DRAG_BOARDSLIDE; dragClass++) {
		CHECK(SkateDrag(dragClass, SURFACE_RAIL, 2.0f) < SkateDrag(dragClass, SURFACE_LEDGE, 2.0f));
		CHECK(SkateDrag(dragClass, SURFACE_LEDGE, 2.0f) < SkateDrag(dragClass, SURFACE_ROUGH, 2.0f));
	}
	// numbers from nowhere are taken for a 50-50 on a ledge
	CHECK(near(SkateDrag(99, 99, 1.0f), SkateDrag(DRAG_TRUCKS, SURFACE_LEDGE, 1.0f), 1e-6));
	CHECK(near(SkateDrag(-1, -1, 1.0f), SkateDrag(DRAG_TRUCKS, SURFACE_LEDGE, 1.0f), 1e-6));

	// the scale: the board is out of speed well above the creep the drag leaves it
	CHECK(kUnitsPerMetre > 39.37f);     // THUG's skater is quicker than a real one, whose metre is 39.37 inches
	CHECK(kCreepSpeed > 0.0f && kCreepSpeed < kStopSpeed);
	CHECK(kStopAfter >= 0.5f);

	// the settings
	Settings cfg;
	SetDefaults(cfg);
	CHECK_EQ(cfg.grindDrag, 100);
	CHECK_EQ(cfg.grindExit, 1);
	CHECK_EQ(cfg.grindBoost, 0);
	cfg.grindDrag = -5; cfg.grindExit = 7; cfg.grindBoost = -1;
	Sanitize(cfg);
	CHECK_EQ(cfg.grindDrag, 0);
	CHECK_EQ(cfg.grindExit, 1);
	CHECK_EQ(cfg.grindBoost, 1);
	cfg.grindDrag = 5000;
	Sanitize(cfg);
	CHECK_EQ(cfg.grindDrag, 1000);
}

struct Stay {
	double seconds;     // by the number of frames
	double counted;     // as the rail run counted them
	double metres;
	bool off;
};

// A board that comes onto a rail at so many m/s and is left alone until it wants off
static Stay ride(double hz, double start, int dragClass, int surface, int percent = 100, float level = 1.0f) {
	RailRun run;
	float speed = (float)start * kUnitsPerMetre;
	const float dt = (float)(1.0 / hz);
	double way = 0.0;
	const int limit = (int)(hz * 60.0);
	int frames = 0;
	while (frames < limit && !run.WantsOff()) {
		speed = run.Step(dt, speed, level, dragClass, surface, false, percent, true);
		way += (double)speed / hz;
		frames++;
	}
	Stay stay;
	stay.seconds = frames / hz;
	stay.counted = run.Seconds();
	stay.metres = way / kUnitsPerMetre;
	stay.off = run.WantsOff();
	return stay;
}

static void testRailRun(double hz) {
	char name[128];
	const float dt = (float)(1.0 / hz);
	const float stop = kStopSpeed * kUnitsPerMetre;
	const float creep = kCreepSpeed * kUnitsPerMetre;

	snprintf(name, sizeof(name), "rail run: how far a grind goes, %g Hz", hz);
	g_case = name;
	{
		// As in Skate 3: a 50-50 from 6 m/s goes about 18 m in 5.4 s on a rail, 11 m in 3.4 s on a ledge
		const Stay rail = ride(hz, 6.0, DRAG_TRUCKS, SURFACE_RAIL);
		CHECK(rail.off);
		CHECK(near(rail.seconds, 5.4, 0.2));
		CHECK(near(rail.metres, 18.0, 0.7));
		const Stay ledge = ride(hz, 6.0, DRAG_TRUCKS, SURFACE_LEDGE);
		CHECK(ledge.off);
		CHECK(near(ledge.seconds, 3.4, 0.2));
		CHECK(near(ledge.metres, 11.2, 0.6));
		// from 4 and from 8 m/s on a rail: 8.5 m in 3.8 s, 30.7 m in 7.0 s
		const Stay slow = ride(hz, 4.0, DRAG_TRUCKS, SURFACE_RAIL);
		CHECK(near(slow.seconds, 3.8, 0.2));
		CHECK(near(slow.metres, 8.5, 0.5));
		const Stay fast = ride(hz, 8.0, DRAG_TRUCKS, SURFACE_RAIL);
		CHECK(near(fast.seconds, 7.0, 0.2));
		CHECK(near(fast.metres, 30.7, 1.0));
		// a smith on a ledge from 6 m/s: 6 m in 1.9 s
		const Stay smith = ride(hz, 6.0, DRAG_ANGLED, SURFACE_LEDGE);
		CHECK(near(smith.seconds, 1.9, 0.15));
		CHECK(near(smith.metres, 6.0, 0.5));

		// GrindDrag=200: twice the drag
		const Stay twice = ride(hz, 6.0, DRAG_TRUCKS, SURFACE_RAIL, 200);
		CHECK(twice.off);
		CHECK(twice.seconds < rail.seconds * 0.62 && twice.seconds > rail.seconds * 0.45);
		// a rail at 60 degrees has half the drag of a level one (what gravity does there is the game's business)
		const Stay steep = ride(hz, 6.0, DRAG_TRUCKS, SURFACE_RAIL, 100, 0.5f);
		const Stay half = ride(hz, 6.0, DRAG_TRUCKS, SURFACE_RAIL, 50);
		CHECK(near(steep.seconds, half.seconds, 0.05));
		// GrindDrag=0: the board keeps its speed and never wants off
		const Stay none = ride(hz, 6.0, DRAG_TRUCKS, SURFACE_RAIL, 0);
		CHECK(!none.off);
		CHECK(near(none.seconds, 60.0, 0.01));
		CHECK(near(none.metres, 6.0 * 60.0, 0.5));
		// the seconds are counted frame by frame; over a minute that is good to a small part of one
		CHECK(near(none.counted, 60.0, 0.5));
		CHECK(near(rail.counted, rail.seconds, 0.01));
	}

	snprintf(name, sizeof(name), "rail run: the drag only slows, %g Hz", hz);
	g_case = name;
	{
		RailRun run;
		float speed = 500.0f;
		bool slower = true, aboveCreep = true;
		for (int i = 0; i < (int)(hz * 20.0); i++) {
			const float before = speed;
			speed = run.Step(dt, speed, 1.0f, DRAG_ANGLED, SURFACE_ROUGH, false, 1000, false);
			slower = slower && speed <= before;
			aboveCreep = aboveCreep && speed >= creep - 0.001f;
		}
		CHECK(slower);
		CHECK(aboveCreep);
		CHECK(near(speed, creep, 0.001));
		CHECK(!run.WantsOff());     // stepping off was switched off

		// a board slower than that is left as it is
		run.Begin();
		CHECK(near(run.Step(dt, creep * 0.5f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true), creep * 0.5f, 1e-6));
		CHECK(run.Step(dt, 0.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true) == 0.0f);

		// what it reports for the log
		run.Begin();
		const float after = run.Step(dt, 400.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true);
		CHECK(near(run.Drag(), SkateDrag(DRAG_TRUCKS, SURFACE_RAIL, 0.0f) * kUnitsPerMetre, 0.01));
		CHECK(near(after, 400.0f - run.Drag() * dt, 0.01));
		CHECK(near(run.Speed(), after, 1e-6));
		CHECK(near(run.Seconds(), dt, 1e-6));
		run.Begin();
		run.Step(dt, 400.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 250, true);
		CHECK(near(run.Drag(), SkateDrag(DRAG_TRUCKS, SURFACE_RAIL, 0.0f) * kUnitsPerMetre * 2.5f, 0.01));
	}

	snprintf(name, sizeof(name), "rail run: when the board wants off, %g Hz", hz);
	g_case = name;
	{
		// not in the first second, however slow it is
		RailRun run;
		bool early = false;
		for (int i = 0; i < (int)(hz * 0.95); i++) {
			run.Step(dt, 0.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true);
			early = early || run.WantsOff();
		}
		CHECK(!early);
		for (int i = 0; i < (int)(hz * 0.2) + 1; i++) run.Step(dt, 0.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true);
		CHECK(run.WantsOff());
		// the next rail starts over
		run.Begin();
		CHECK(!run.WantsOff());
		CHECK(run.Seconds() == 0.0f);

		// with speed left it stays on, however long
		for (int i = 0; i < (int)(hz * 30.0); i++) run.Step(dt, 300.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 0, true);
		CHECK(!run.WantsOff());
		// a slope turns the board around: off, even though it has a little speed again
		run.Step(dt, 30.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, true, 0, true);
		CHECK(run.WantsOff());
		run.Step(dt, stop * 3.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 0, true);
		CHECK(run.WantsOff());
		// it did not get off and is clearly moving again: it stays
		run.Step(dt, stop * 4.0f + 5.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 0, true);
		CHECK(!run.WantsOff());
		// under the stop speed, above it
		run.Step(dt, stop + 1.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 0, true);
		CHECK(!run.WantsOff());
		run.Step(dt, stop - 1.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 0, true);
		CHECK(run.WantsOff());
		// GrindExit=0
		run.Step(dt, 0.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 0, false);
		CHECK(!run.WantsOff());
	}

	snprintf(name, sizeof(name), "rail run: numbers the game should never hand over, %g Hz", hz);
	g_case = name;
	{
		RailRun run;
		// a frame of no or negative length
		CHECK(run.Step(-1.0f, 100.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true) == 100.0f);
		CHECK(run.Step(0.0f, 100.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true) == 100.0f);
		CHECK(run.Seconds() == 0.0f);
		// a negative speed, and not-a-numbers
		CHECK(run.Step(dt, -50.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true) == 0.0f);
		run.Begin();
		const float nan = (float)sqrt(-1.0);
		CHECK(run.Step(nan, nan, nan, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true) == 0.0f);
		CHECK(run.Seconds() == 0.0f);
		CHECK(!run.WantsOff());
		// a hitch of fifty seconds counts as a tenth of one
		run.Begin();
		const float hitch = run.Step(50.0f, 400.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true);
		CHECK(hitch > 390.0f && hitch < 400.0f);
		CHECK(near(run.Seconds(), 0.1, 1e-4));
		// how level the rail is: more than level is level, less than vertical is no drag
		run.Begin();
		const float level = run.Step(dt, 400.0f, 1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true);
		run.Begin();
		CHECK(near(run.Step(dt, 400.0f, 5.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true), level, 1e-6));
		run.Begin();
		CHECK(run.Step(dt, 400.0f, -1.0f, DRAG_TRUCKS, SURFACE_RAIL, false, 100, true) == 400.0f);
	}
}

// ---------------------------------------------------------------------------------------------
// On a rail: stepping off when the speed is gone
// ---------------------------------------------------------------------------------------------

// A press of the button that steps off the rail, the way the game needs it
static void expectStepPress(Sim& s, const Press& p, int side) {
	const int n = (int)s.outs.size();
	CHECK(p.frames >= 2);
	CHECK(p.frames * 1000.0 / s.hz >= 50.0 - 1000.0 / s.hz);
	CHECK(p.frames * 1000.0 / s.hz <= 50.0 + 3000.0 / s.hz + 1.0);
	for (int i = p.first; i < p.first + p.frames && i < n; i++) {
		CHECK_EQ(s.outs[i].stepSide, side);
		CHECK(s.outs[i].stepStarted == (i == p.first));
		CHECK(!s.outs[i].flip);
		CHECK(!s.outs[i].grab);
		CHECK(!s.outs[i].grind);
		CHECK(!s.outs[i].ollie);
		CHECK(!s.outs[i].ownsDirection);
	}
}

static int countStepStarts(const Sim& s, size_t from) {
	int n = 0;
	for (size_t i = from; i < s.outs.size(); i++) if (s.outs[i].stepStarted) n++;
	return n;
}

static void testStepOff(double hz) {
	char name[128];

	snprintf(name, sizeof(name), "step off: the speed is gone, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.hold(1500);
		CHECK_EQ(s.count(&Output::stepOff), 0);
		const size_t m = s.mark();
		s.offSide = -1;
		s.railOff = true;
		s.hold(300);
		// pressed on the very frame, with the side the board came from, and the skater is off the rail
		const std::vector<Press> off = s.presses(&Output::stepOff, m);
		CHECK_EQ(off.size(), 1);
		if (off.size() == 1) {
			CHECK_EQ(off[0].first, (int)m);
			CHECK(s.outs[off[0].first].stepStarted);
			CHECK_EQ(s.outs[off[0].first].stepSide, -1);
		}
		CHECK_EQ(countStepStarts(s, m), 1);
		CHECK_EQ(s.game.stepOffs, 1);
		CHECK_EQ(s.game.stepSide, -1);
		CHECK_EQ(s.state, STATE_GROUND);
		// nothing else was pressed for it
		CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m) + s.count(&Output::ollie, m), 0);
		// off the rail there is nothing to step off, whatever the game goes on saying
		s.hold(2000);
		CHECK_EQ(s.presses(&Output::stepOff, m).size(), 1);
		CHECK(!s.rec.Busy());
	}

	snprintf(name, sizeof(name), "step off: the side, %g Hz", hz);
	g_case = name;
	{
		static const int sides[][2] = { { -1, -1 }, { 1, 1 }, { 0, 0 }, { 7, 1 }, { -3, -1 } };
		for (size_t i = 0; i < sizeof(sides) / sizeof(sides[0]); i++) {
			Sim s(hz);
			ollieIntoAir(s);
			s.landOn(GRIND_NOSEGRIND);
			s.hold(1200);
			s.offSide = sides[i][0];
			s.railOff = true;
			s.hold(200);
			CHECK_EQ(s.game.stepOffs, 1);
			CHECK_EQ(s.game.stepSide, sides[i][1]);
		}
	}

	snprintf(name, sizeof(name), "step off: pressed again when nothing happens, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.game.noRelease = true;
		s.hold(1500);
		size_t m = s.mark();
		s.offSide = 1;
		s.railOff = true;
		s.hold(2600);
		const std::vector<Press> off = s.presses(&Output::stepOff, m);
		CHECK(off.size() >= 4 && off.size() <= 5);
		for (size_t k = 0; k < off.size(); k++) {
			// the last one may still be down
			if (off[k].first + off[k].frames < (int)s.outs.size()) expectStepPress(s, off[k], 1);
			if (k > 0) CHECK(s.msBetween(off[k - 1].first + off[k - 1].frames, off[k].first) >= 500.0);
		}
		CHECK_EQ(s.state, STATE_RAIL);
		CHECK_EQ(s.game.stepOffs, 0);
		CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m) + s.count(&Output::ollie, m), 0);

		// a slope has the board moving again and the game takes its word back: no more presses
		while (s.outs.back().stepOff) s.step();
		s.railOff = false;
		m = s.mark();
		s.hold(3000);
		CHECK_EQ(s.count(&Output::stepOff, m), 0);
		CHECK(!s.rec.Busy());
	}

	snprintf(name, sizeof(name), "step off: switched off, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		s.cfg.grindExit = 0;
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.hold(1500);
		size_t m = s.mark();
		s.railOff = true;
		s.hold(3000);
		CHECK_EQ(s.count(&Output::stepOff, m), 0);
		CHECK_EQ(countStepStarts(s, m), 0);
		CHECK_EQ(s.state, STATE_RAIL);
		// and the stick still switches grinds
		m = s.mark();
		s.toward(D, 60);
		s.hold(1200);
		expectSequence(s, m, GRIND_FIVE_O);
		CHECK_EQ(s.game.kind, GRIND_FIVE_O);
	}

	snprintf(name, sizeof(name), "step off: only on a rail, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		s.railOff = true;
		s.offSide = 1;
		s.hold(1000);                       // on the ground
		ollieIntoAir(s);
		s.hold(600);                        // in the air
		s.state = STATE_WALL;
		s.hold(300);
		s.state = STATE_AIR;
		s.hold(100);
		s.state = STATE_GROUND;
		s.hold(500);
		CHECK_EQ(s.count(&Output::stepOff), 0);
		CHECK_EQ(countStepStarts(s, 0), 0);
		// and with nothing known about the skater
		Sim blind(hz, STATE_UNKNOWN);
		blind.railOff = true;
		blind.hold(2000);
		CHECK_EQ(blind.count(&Output::stepOff), 0);
	}

	snprintf(name, sizeof(name), "step off: a stick that is held does not keep the board on the rail, %g Hz", hz);
	g_case = name;
	{
		// Stick down for the 5-0, and the speed runs out before the switch went: the board steps off,
		// and no switch is started on the way
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.game.noRelease = true;
		s.hold(1500);
		const size_t m = s.mark();
		s.toward(D, 60);
		s.railOff = true;
		s.hold(1500);
		const std::vector<Press> off = s.presses(&Output::stepOff, m);
		CHECK(off.size() >= 2);
		if (!off.empty()) CHECK(s.msBetween((int)m, off[0].first) <= 2000.0 / hz + 70.0);
		CHECK_EQ(s.count(&Output::flip, m) + s.count(&Output::grab, m) + s.count(&Output::grind, m), 0);
		CHECK_EQ(s.game.switches, 0);
		CHECK_EQ(s.count(&Output::ollie, m), 0);
	}

	snprintf(name, sizeof(name), "step off: a switch under way goes first, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.hold(1500);
		const size_t m = s.mark();
		s.toward(D, 60);
		// wait for the first press of the switch, then the speed is gone
		int guard = 0;
		while (!s.outs.back().flip && guard++ < (int)(hz * 2.0)) s.step();
		CHECK(s.outs.back().flip);
		s.offSide = -1;
		s.railOff = true;
		s.hold(2500);
		// the two presses of the switch came out whole, the step-off after them, never together
		int lastSwitch = -1, firstStep = -1;
		for (int i = (int)m; i < (int)s.outs.size(); i++) {
			const Output& o = s.outs[i];
			CHECK(!(o.stepOff && (o.flip || o.grab || o.grind)));
			if (o.flip || o.grab || o.grind) lastSwitch = i;
			if (o.stepOff && firstStep < 0) firstStep = i;
		}
		CHECK(firstStep > lastSwitch && lastSwitch >= 0);
		CHECK_EQ(s.game.switches, 1);
		CHECK_EQ(s.game.kind, GRIND_FIVE_O);
		// The new grind takes the button only after its first frames, as THUG's does, so the first
		// press may be lost. The second is not.
		const std::vector<Press> off = s.presses(&Output::stepOff, m);
		CHECK(off.size() >= 1 && off.size() <= 2);
		CHECK_EQ(s.game.stepOffs, 1);
		CHECK_EQ(s.game.stepSide, -1);
		CHECK_EQ(s.state, STATE_GROUND);
	}

	snprintf(name, sizeof(name), "step off: a flick under way goes first, %g Hz", hz);
	g_case = name;
	{
		// The stick is on its way out of the wind-up when the speed runs out: if that is a flick it pops
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.hold(100);
		s.toward(D, 40);
		s.hold(60);
		const size_t m = s.mark();
		s.moveTo(0.0f, 0.0f, 20);
		s.railOff = true;
		s.toward(U, 20);
		s.hold(40);
		s.spring();
		s.hold(400);
		CHECK_EQ(s.count(&Output::stepOff, m), 0);
		CHECK_EQ(s.presses(&Output::ollie, m).size(), 1);
		CHECK_EQ(s.state, STATE_AIR);
		CHECK_EQ(s.game.stepOffs, 0);
	}
	{
		// ... and if it was no flick, the board steps off once that is clear
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.hold(1500);
		s.toward(D, 300, 0.9f);             // slowly, and not held long enough to switch
		const size_t m = s.mark();
		s.moveTo(0.0f, -0.3f, 150);         // slowly out of the wind-up
		s.railOff = true;
		s.offSide = 1;
		s.moveTo(0.0f, 0.0f, 100);
		s.hold(600);
		CHECK_EQ(s.count(&Output::ollie, m), 0);
		CHECK_EQ(s.game.stepOffs, 1);
		CHECK_EQ(s.game.stepSide, 1);
		CHECK_EQ(s.state, STATE_GROUND);
		const std::vector<Press> off = s.presses(&Output::stepOff, m);
		CHECK_EQ(off.size(), 1);
		if (off.size() == 1) CHECK(s.msBetween((int)m, off[0].first) <= s.cfg.flickWindow + 150.0 + 3000.0 / hz);
	}

	snprintf(name, sizeof(name), "step off: the next rail starts fresh, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		ollieIntoAir(s);
		s.landOn(GRIND_FIFTY);
		s.game.noRelease = true;
		s.hold(1500);
		s.railOff = true;
		s.hold(120);                        // a press is out or under way, and the wait for the next one runs
		// the player pops off, lands on another rail, and that one is out of speed at once (the game
		// would not say so in the first second, but nothing here relies on that)
		s.railOff = false;
		s.state = STATE_AIR;
		s.game.leave();
		s.hold(200);
		s.game.noRelease = false;
		s.landOn(GRIND_FIFTY);
		s.hold(800);
		const size_t m = s.mark();
		s.railOff = true;
		s.hold(100);
		CHECK_EQ(s.game.stepOffs, 1);
		const std::vector<Press> off = s.presses(&Output::stepOff, m);
		CHECK_EQ(off.size(), 1);
		if (off.size() == 1) CHECK_EQ(off[0].first, (int)m);     // no leftover wait from the rail before
	}
}

static unsigned int g_seed = 12345;
static float frand() {
	g_seed = g_seed * 1664525u + 1013904223u;
	return (float)((g_seed >> 8) & 0xFFFF) / 65535.0f;
}

// ---------------------------------------------------------------------------------------------
// How far a rail reaches for the board
// ---------------------------------------------------------------------------------------------

static RailPass pass(float ax, float ay, float az, float bx, float by, float bz, const float* from, const float* to, int percent = 100) {
	const float a[3] = { ax, ay, az };
	const float b[3] = { bx, by, bz };
	return PassRail(a, b, from, to, percent);
}

// A board that falls through the height of a level rail along z, so far beside it
static RailPass beside(float distance, int percent = 100) {
	static const float from[3] = { 0.0f, 0.0f, -100.0f };
	static const float to[3] = { 0.0f, 0.0f, 100.0f };
	return pass(distance, 5.0f, -10.0f, distance, -5.0f, 0.0f, from, to, percent);
}

// How far apart two straight pieces are, found the slow way: many points along the first, and
// for each of them the nearest point of the second. A little too long at most, never too short.
static double slowDistance(const float* a, const float* b, const float* from, const float* to) {
	double least = 1.0e30;
	for (int i = 0; i <= 500; i++) {
		const double s = i / 500.0;
		double p[3], d[3];
		double length2 = 0.0, dot = 0.0;
		for (int k = 0; k < 3; k++) {
			p[k] = a[k] + s * ((double)b[k] - a[k]);
			d[k] = (double)to[k] - from[k];
			length2 += d[k] * d[k];
			dot += (p[k] - from[k]) * d[k];
		}
		double t = length2 > 0.0 ? dot / length2 : 0.0;
		if (t < 0.0) t = 0.0;
		if (t > 1.0) t = 1.0;
		double distance2 = 0.0;
		for (int k = 0; k < 3; k++) {
			const double r = from[k] + t * d[k] - p[k];
			distance2 += r * r;
		}
		const double distance = sqrt(distance2);
		if (distance < least) least = distance;
	}
	return least;
}

// What THUG's own search asks of a piece of rail: the distance, times 2 less the cosine of the
// level angle between the way the board goes and the rail, is 40 at most
static bool thugWouldReach(const float* a, const float* b, const float* from, const float* to) {
	const double mx = (double)b[0] - a[0], mz = (double)b[2] - a[2];
	const double rx = (double)to[0] - from[0], rz = (double)to[2] - from[2];
	const double m = sqrt(mx * mx + mz * mz), r = sqrt(rx * rx + rz * rz);
	double c = 1.0;
	if (m > 1.0e-9 && r > 1.0e-9) c = fabs(mx * rx + mz * rz) / (m * r);
	return slowDistance(a, b, from, to) * (2.0 - c) <= 40.0;
}

static void testReach() {
	static const float from[3] = { 0.0f, 0.0f, -100.0f };
	static const float to[3] = { 0.0f, 0.0f, 100.0f };

	g_case = "reach: Skate's distances in THUG's units";
	CHECK(near(kReachLevel, 0.243 * 80.0, 0.01));            // 19.4
	CHECK(near(kReachDown, 0.2 * 80.0, 0.01));               // 16
	CHECK(near(kReachUp, 8.0, 0.01));
	CHECK(near(kReachEnd, 16.0, 0.01));
	CHECK_EQ(kReachMostPercent, 400);
	// none of it as far as THUG reaches even across a rail (20 units), where it reaches least
	CHECK(kReachLevel < 20.0f && kReachDown < 20.0f && kReachUp < 20.0f && kReachEnd < 20.0f);
	CHECK(kReachBehind < kReachLevel && kReachBehind < kReachEnd);

	g_case = "reach: beside the rail";
	{
		// the board comes down right on the rail
		const RailPass on = beside(0.0f);
		CHECK(on.miss < 0.01f);
		CHECK(!on.away);
		CHECK(near(on.side, 0.0, 0.01));
		CHECK(near(on.point[0], 0.0, 0.01) && near(on.point[1], 0.0, 0.01));
		// 19.4 units
		CHECK(beside(10.0f).miss < 1.0f);
		CHECK(beside(19.0f).miss <= 1.0f);
		CHECK(beside(20.0f).miss > 1.0f);
		CHECK(beside(39.0f).miss > 1.9f);            // THUG would have taken this
		CHECK(near(beside(10.0f).miss, 10.0 / 19.44, 0.001));
		// and the same on the other side
		CHECK(near(beside(-10.0f).miss, beside(10.0f).miss, 1e-4));
		// what is reported: the level distance, signed by the side
		CHECK(near(fabs(beside(12.0f).side), 12.0, 0.01));
		CHECK(beside(12.0f).side * beside(-12.0f).side < 0.0f);
		CHECK(near(beside(12.0f).lift, 0.0, 0.5));
		CHECK(near(beside(12.0f).past, 0.0, 0.01));
		// which way the board goes makes no difference to how far the rail reaches: coming
		// across the rail it is the same 19.4 units as going along it. (THUG: 20 and 40.)
		CHECK(pass(-29, 0, 0, -19, 0, 0, from, to).miss <= 1.0f);
		CHECK(pass(-31, 0, 0, -21, 0, 0, from, to).miss > 1.0f);
		CHECK(near(pass(-22, 0, 0, -12, 0, 0, from, to).miss, 12.0 / 19.44, 0.001));
		CHECK(near(pass(12, 0, -40, 12, 0, -30, from, to).miss, 12.0 / 19.44, 0.001));
		CHECK(near(pass(-19, 0, -7, -12, 0, 0, from, to).miss, 12.0 / 19.44, 0.001));
	}

	g_case = "reach: above and below the rail";
	{
		// level flight along the rail, so high above it: up to 16 units
		CHECK(pass(0, 15, -10, 0, 15, 0, from, to).miss <= 1.0f);
		CHECK(pass(0, 17, -10, 0, 17, 0, from, to).miss > 1.0f);
		CHECK(near(pass(0, 15, -10, 0, 15, 0, from, to).lift, -15.0, 0.01));
		// and under it: up to 8
		CHECK(pass(0, -7, -10, 0, -7, 0, from, to).miss <= 1.0f);
		CHECK(pass(0, -9, -10, 0, -9, 0, from, to).miss > 1.0f);
		CHECK(near(pass(0, -7, -10, 0, -7, 0, from, to).lift, 7.0, 0.01));
		// THUG pulls a board down from 39 units above a rail; this does not
		CHECK(pass(0, 39, -10, 0, 39, 0, from, to).miss > 2.0f);
		// a board that falls through the rail's height between two frames is not missed: what
		// counts is the path
		CHECK(pass(5, 40, -10, 5, -40, -5, from, to).miss < 0.5f);
	}

	g_case = "reach: beside and above at once";
	{
		// The reaches are the half axes of one rounded shape. 10 beside and 12 above:
		// (10 / 19.44)^2 + (12 / 16)^2 = 0.827, the root of that
		CHECK(near(pass(10, 12, -10, 10, 12, 0, from, to).miss, 0.9095, 0.001));
		CHECK(near(pass(18, 4, -10, 18, 4, 0, from, to).miss, 0.9591, 0.001));
		// each within its own reach, the two together not
		CHECK(pass(15, 0, -10, 15, 0, 0, from, to).miss <= 1.0f);
		CHECK(pass(0, 12, -10, 0, 12, 0, from, to).miss <= 1.0f);
		CHECK(near(pass(15, 12, -10, 15, 12, 0, from, to).miss, 1.0760, 0.001));
		CHECK(pass(19, 15, -10, 19, 15, 0, from, to).miss > 1.3f);
		// and with the end of a rail: 12 short of it, 12 beside it
		static const float start[3] = { 0.0f, 0.0f, 0.0f };
		static const float end[3] = { 0.0f, 0.0f, 100.0f };
		CHECK(pass(0, 0, -22, 0, 0, -12, start, end).miss <= 1.0f);
		CHECK(pass(12, 0, -22, 12, 0, -12, start, end).miss <= 1.0f);
		CHECK(near(pass(12, 0, -22, 12, 0, -12, start, end).miss, 0.9714, 0.001));
		CHECK(pass(15, 0, -22, 15, 0, -12, start, end).miss > 1.0f);
	}

	g_case = "reach: the ends of a rail";
	{
		static const float start[3] = { 0.0f, 0.0f, 0.0f };
		static const float end[3] = { 0.0f, 0.0f, 100.0f };
		// short of the start by so much, flying toward it
		CHECK(pass(0, 2, -25, 0, 2, -15, start, end).miss <= 1.0f);
		CHECK(pass(0, 2, -27, 0, 2, -17, start, end).miss > 1.0f);
		CHECK(near(pass(0, 2, -25, 0, 2, -15, start, end).past, 15.0, 0.01));
		CHECK(!pass(0, 2, -25, 0, 2, -15, start, end).away);
		// the board goes to the end of the rail
		const RailPass p = pass(0, 2, -25, 0, 2, -15, start, end);
		CHECK(near(p.point[0], 0.0, 0.01) && near(p.point[1], 0.0, 0.01) && near(p.point[2], 0.0, 0.01));
		// off the far end and flying on: only just past it is still its rail
		CHECK(pass(0, 2, 105, 0, 2, 115, start, end).miss <= 1.0f);
		CHECK(!pass(0, 2, 105, 0, 2, 115, start, end).away);
		CHECK(pass(0, 2, 110, 0, 2, 120, start, end).miss > 1.0f);
		CHECK(pass(0, 2, 110, 0, 2, 120, start, end).away);
		// coming back toward that end from beyond it
		CHECK(pass(0, 2, 125, 0, 2, 112, start, end).miss <= 1.0f);
		CHECK(pass(0, 2, 130, 0, 2, 118, start, end).miss > 1.0f);
		CHECK(!pass(0, 2, 130, 0, 2, 118, start, end).away);
	}

	g_case = "reach: where the board goes";
	{
		// beside the rail and flying along it: the place on the rail next to where the board is
		// now, not where it was a frame ago
		const RailPass p = pass(8, 3, 20, 8, 3, 32, from, to);
		CHECK(near(p.point[0], 0.0, 0.01) && near(p.point[1], 0.0, 0.01) && near(p.point[2], 32.0, 0.01));
		// crossing it: where it crosses
		const RailPass q = pass(-6, 1, 40, 6, 1, 44, from, to);
		CHECK(near(q.point[0], 0.0, 0.01) && near(q.point[2], 42.0, 0.05));
		CHECK(near(q.side, 0.0, 0.05));
		CHECK(near(q.lift, -1.0, 0.05));
	}

	g_case = "reach: a rail the board has passed";
	{
		// across the rail, toward it: not away, however far
		CHECK(!pass(-39, 0, 0, -29, 0, 0, from, to).away);
		CHECK(!pass(-29, 0, 0, -19, 0, 0, from, to).away);
		// over it and gone: 10 units past it is not pulled back
		const RailPass gone = pass(10, 0, 0, 20, 0, 0, from, to);
		CHECK(gone.miss > 1.0f);
		CHECK(gone.away);
		CHECK(near(gone.miss, 1.5, 1e-4));          // by the distance alone it would be within reach
		// only just past it: still its rail
		CHECK(pass(5, 0, 0, 15, 0, 0, from, to).miss <= 1.0f);
		CHECK(pass(8, 0, 0, 18, 0, 0, from, to).miss <= 1.0f);
		CHECK(!pass(8, 0, 0, 18, 0, 0, from, to).away);
		CHECK(pass(9, 0, 0, 19, 0, 0, from, to).miss > 1.0f);
		// far past it: away, and as far as it is
		const RailPass far = pass(50, 0, 0, 60, 0, 0, from, to);
		CHECK(far.away);
		CHECK(near(far.miss, 50.0 / 19.44, 0.001));
		// along the rail with a little drift away from it: that is not leaving it
		CHECK(pass(15, 2, 0, 16, 2, 10, from, to).miss <= 1.0f);
		CHECK(!pass(15, 2, 0, 16, 2, 10, from, to).away);
		// too high over it, then down behind it: gone
		CHECK(pass(-5, 30, 0, 5, 26, 0, from, to).miss > 1.0f);
		CHECK(pass(15, 14, 0, 25, 10, 0, from, to).miss > 1.0f);
		// straight down next to it is not leaving it
		CHECK(!pass(12, 5, 0, 12, -5, 0, from, to).away);
		CHECK(pass(12, 5, 0, 12, -5, 0, from, to).miss <= 1.0f);
	}

	g_case = "reach: GrindReach";
	{
		CHECK(beside(9.0f, 50).miss <= 1.0f);
		CHECK(beside(10.0f, 50).miss > 1.0f);
		CHECK(beside(38.0f, 200).miss <= 1.0f);
		CHECK(beside(39.0f, 200).miss > 1.0f);
		CHECK(pass(0, 31, -10, 0, 31, 0, from, to, 200).miss <= 1.0f);
		CHECK(pass(0, 33, -10, 0, 33, 0, from, to, 200).miss > 1.0f);
		// twice the reach: half the miss
		CHECK(near(beside(12.0f, 200).miss * 2.0, beside(12.0f, 100).miss, 1e-3));
		CHECK(near(pass(10, 12, -10, 10, 12, 0, from, to, 200).miss * 2.0, pass(10, 12, -10, 10, 12, 0, from, to).miss, 1e-3));
		// and a rail the board has passed is let go later
		CHECK(pass(10, 0, 0, 20, 0, 0, from, to, 100).away);
		CHECK(!pass(10, 0, 0, 20, 0, 0, from, to, 200).away);
		CHECK(pass(10, 0, 0, 20, 0, 0, from, to, 200).miss <= 1.0f);
		// off: no rail (the game's own search is used then)
		CHECK(beside(0.0f, 0).miss >= kNoRail);
		CHECK(beside(0.0f, -5).miss >= kNoRail);
	}

	g_case = "reach: a rail on a slope";
	{
		// 45 degrees up along z
		static const float low[3] = { 0.0f, 0.0f, 0.0f };
		static const float high[3] = { 0.0f, 100.0f, 100.0f };
		// 10 units off it, square to it, on its upper side
		const RailPass above = pass(0, 57.07f, 42.93f, 0, 57.07f, 42.93f, low, high);
		CHECK(near(above.lift, -10.0, 0.05));
		CHECK(near(above.side, 0.0, 0.01));
		CHECK(near(above.past, 0.0, 0.05));
		CHECK(near(above.miss, 10.0 / 16.0, 0.01));
		CHECK(near(above.point[1], 50.0, 0.05) && near(above.point[2], 50.0, 0.05));
		// and under it
		const RailPass under = pass(0, 42.93f, 57.07f, 0, 42.93f, 57.07f, low, high);
		CHECK(near(under.lift, 10.0, 0.05));
		CHECK(under.miss > 1.0f);
		// beside it: level, whatever the slope
		const RailPass side = pass(12, 50, 50, 12, 50, 50, low, high);
		CHECK(near(fabs(side.side), 12.0, 0.01));
		CHECK(near(side.lift, 0.0, 0.05));
	}

	g_case = "reach: a rail that stands on end";
	{
		// It has no top side: only the level distance counts, at any height along it
		static const float foot[3] = { 0.0f, 0.0f, 0.0f };
		static const float top[3] = { 0.0f, 100.0f, 0.0f };
		CHECK(pass(10, 50, -5, 10, 50, 5, foot, top).miss <= 1.0f);
		CHECK(near(pass(10, 50, -5, 10, 50, 5, foot, top).miss, 10.0 / 19.44, 0.001));
		CHECK(near(pass(10, 90, -5, 10, 90, 5, foot, top).miss, 10.0 / 19.44, 0.001));
		CHECK(pass(25, 50, -5, 25, 50, 5, foot, top).miss > 1.0f);
		// above its top it ends like any rail: 16 units
		CHECK(pass(5, 110, -5, 5, 110, 5, foot, top).miss <= 1.0f);
		CHECK(near(pass(5, 110, -5, 5, 110, 5, foot, top).past, -10.0, 0.01));
		CHECK(pass(5, 118, -5, 5, 118, 5, foot, top).miss > 1.0f);
		CHECK(pass(0, 130, -5, 0, 130, 5, foot, top).miss > 1.5f);
		// and under its foot
		CHECK(pass(0, -10, -5, 0, -10, 5, foot, top).miss <= 1.0f);
		CHECK(pass(0, -20, -5, 0, -20, 5, foot, top).miss > 1.0f);
	}

	g_case = "reach: a rail that is one point";
	{
		static const float point[3] = { 0.0f, 0.0f, 0.0f };
		const RailPass p = pass(-5, 3, -10, -5, 3, 10, point, 0);
		CHECK(near(p.side, 5.0, 0.01));
		CHECK(near(p.lift, -3.0, 0.01));
		CHECK(near(p.miss, 0.3183, 0.001));         // (5 / 19.44)^2 + (3 / 16)^2, the root of that
		CHECK(!p.away);
		CHECK(near(p.point[0], 0.0, 1e-6) && near(p.point[1], 0.0, 1e-6) && near(p.point[2], 0.0, 1e-6));
		CHECK(pass(-19, 0, -10, -19, 0, 10, point, 0).miss <= 1.0f);
		CHECK(pass(-21, 0, -10, -21, 0, 10, point, 0).miss > 1.0f);
		CHECK(pass(0, 15, -10, 0, 15, 10, point, 0).miss <= 1.0f);
		CHECK(pass(0, 17, -10, 0, 17, 10, point, 0).miss > 1.0f);
		CHECK(pass(0, -9, -10, 0, -9, 10, point, 0).miss > 1.0f);
		// a piece of rail with no length is a point too
		CHECK(near(pass(-5, 3, -10, -5, 3, 10, point, point).miss, p.miss, 1e-5));
	}

	g_case = "reach: numbers the game should never hand over";
	{
		// a board that has not moved
		CHECK(pass(10, 2, 0, 10, 2, 0, from, to).miss <= 1.0f);
		CHECK(pass(25, 2, 0, 25, 2, 0, from, to).miss > 1.0f);
		CHECK(!pass(25, 2, 0, 25, 2, 0, from, to).away);
		const float nan = (float)sqrt(-1.0);
		const float bad[3] = { nan, 0.0f, 0.0f };
		const float a[3] = { 0.0f, 0.0f, 0.0f };
		CHECK(PassRail(bad, a, from, to, 100).miss >= kNoRail);
		CHECK(PassRail(a, bad, from, to, 100).miss >= kNoRail);
		CHECK(PassRail(a, a, bad, to, 100).miss >= kNoRail);
		CHECK(PassRail(a, a, from, bad, 100).miss >= kNoRail);
		CHECK(PassRail(0, a, from, to, 100).miss >= kNoRail);
		CHECK(PassRail(a, 0, from, to, 100).miss >= kNoRail);
		CHECK(PassRail(a, a, 0, to, 100).miss >= kNoRail);
		// numbers too big to reckon with: no rail, or a rail a long way off, never one within reach
		const float huge[3] = { 1.0e30f, 0.0f, 0.0f };
		CHECK(PassRail(huge, huge, from, to, 100).miss > 1.0f);
		CHECK(PassRail(a, a, huge, huge, 100).miss > 1.0f);
		const float inf = (float)(1.0 / sin(0.0));
		const float endless[3] = { inf, 0.0f, 0.0f };
		CHECK(PassRail(endless, a, from, to, 100).miss >= kNoRail);
		CHECK(PassRail(a, a, from, endless, 100).miss >= kNoRail);
	}

	g_case = "reach: against the slow way, and never further than THUG";
	{
		// Rails and flights of every kind. What is reported has to be the true distance between
		// the board's path and the rail, split three ways. And at 100 percent a rail within reach
		// is one THUG's own search would have reached too, with room to spare.
		int within = 0, sloped = 0;
		for (int round = 0; round < 40000; round++) {
			float r0[3], r1[3], a[3], b[3];
			for (int k = 0; k < 3; k++) {
				r0[k] = (frand() - 0.5f) * 200.0f;
				r1[k] = r0[k] + (frand() - 0.5f) * 240.0f;
			}
			if (round % 3 == 0) r1[1] = r0[1];                               // level rails, as most are
			if (round % 7 == 0) { r1[0] = r0[0]; r1[2] = r0[2] + 0.5f; }     // and some on end
			// a flight that starts somewhere near the rail
			const float t = frand() * 1.4f - 0.2f;
			for (int k = 0; k < 3; k++) {
				a[k] = r0[k] + t * (r1[k] - r0[k]) + (frand() - 0.5f) * 70.0f;
				b[k] = a[k] + (frand() - 0.5f) * (k == 1 ? 20.0f : 36.0f);
			}
			if (round % 11 == 0) { b[0] = a[0]; b[2] = a[2]; }               // straight down
			if (round % 13 == 0) { b[0] = a[0]; b[1] = a[1]; b[2] = a[2]; }  // not moving

			const RailPass p = PassRail(a, b, r0, r1, 100);
			const double d = slowDistance(a, b, r0, r1);
			const double reported = sqrt((double)p.side * p.side + (double)p.lift * p.lift + (double)p.past * p.past);
			CHECK(near(reported, d, 0.08 + 1e-4 * d));
			// the point on the rail is on the rail, and that far from the path
			{
				double along = 0.0, length2 = 0.0;
				for (int k = 0; k < 3; k++) {
					along += ((double)p.point[k] - r0[k]) * ((double)r1[k] - r0[k]);
					length2 += ((double)r1[k] - r0[k]) * ((double)r1[k] - r0[k]);
				}
				const double u = along / length2;
				CHECK(u > -1e-4 && u < 1.0 + 1e-4);
				for (int k = 0; k < 3; k++) CHECK(near(p.point[k], r0[k] + u * ((double)r1[k] - r0[k]), 0.02));
			}
			const float rise = fabsf(r1[1] - r0[1]);
			const float run = sqrtf((r1[0] - r0[0]) * (r1[0] - r0[0]) + (r1[2] - r0[2]) * (r1[2] - r0[2]));
			if (p.miss <= 1.0f) {
				within++;
				if (rise > 0.2f * run) sloped++;
				CHECK(d <= kReachLevel + 0.1);
				CHECK(thugWouldReach(a, b, r0, r1));
				CHECK(!p.away);
			}
			// by the distance alone: nothing as far as the longest reach is within reach
			if (d > kReachLevel + 0.1) CHECK(p.miss > 1.0f);
			// and nothing nearer than the shortest is out of it, unless the board is leaving
			if (d < kReachUp - 0.1 && !p.away) CHECK(p.miss <= 1.0f);
			if (p.away) CHECK(p.miss >= 1.5f - 1e-4f);
			// the reach scales with the setting
			if (!p.away && p.miss < 100.0f) {
				const RailPass half = PassRail(a, b, r0, r1, 50);
				if (!half.away) CHECK(near(half.miss, 2.0 * p.miss, 1e-3 * (1.0 + p.miss)));
				const RailPass twice = PassRail(a, b, r0, r1, 200);
				CHECK(!twice.away);
				CHECK(near(twice.miss * 2.0, p.miss, 1e-3 * (1.0 + p.miss)));
			}
		}
		CHECK(within > 2000);          // the test has to have had rails within reach to look at
		CHECK(sloped > 300);
	}
}

// Rail nodes laid out the way the search is told, with whatever size a pointer has here
struct FakeNode {
	float boxMin[4];
	float boxMax[4];
	float position[4];
	FakeNode* next;
	FakeNode* previous;
	uint32_t flags;
	uint32_t unused;
};

static const uint32_t kFakeActive = 1u << 2;
static const uint32_t kFakeClimbOnly = 1u << 4;

static RailNodes fakeNodes(const std::vector<FakeNode>& nodes) {
	RailNodes n;
	n.first = nodes.empty() ? 0 : (const unsigned char*)&nodes[0];
	n.count = (int)nodes.size();
	n.stride = (int)sizeof(FakeNode);
	n.boxMin = (int)offsetof(FakeNode, boxMin);
	n.boxMax = (int)offsetof(FakeNode, boxMax);
	n.position = (int)offsetof(FakeNode, position);
	n.next = (int)offsetof(FakeNode, next);
	n.previous = (int)offsetof(FakeNode, previous);
	n.flags = (int)offsetof(FakeNode, flags);
	n.needs = kFakeActive;
	n.skips = kFakeClimbOnly;
	return n;
}

// Rails given as runs of points; the nodes of one run are linked, and each node's box is the box
// around the piece that starts at it, the way the game stores it
static void addRail(std::vector<FakeNode>& nodes, const float (*points)[3], int count) {
	const size_t first = nodes.size();
	for (int i = 0; i < count; i++) {
		FakeNode n;
		memset(&n, 0, sizeof(n));
		for (int k = 0; k < 3; k++) n.position[k] = points[i][k];
		n.position[3] = 1.0f;
		n.flags = kFakeActive;
		nodes.push_back(n);
	}
	for (int i = 0; i < count; i++) {
		const float* p = points[i];
		const float* q = points[i + 1 < count ? i + 1 : i];
		for (int k = 0; k < 3; k++) {
			nodes[first + i].boxMin[k] = p[k] < q[k] ? p[k] : q[k];
			nodes[first + i].boxMax[k] = p[k] > q[k] ? p[k] : q[k];
		}
	}
}

static void linkRails(std::vector<FakeNode>& nodes, const std::vector<int>& lengths) {
	size_t at = 0;
	for (size_t r = 0; r < lengths.size(); r++) {
		for (int i = 0; i < lengths[r]; i++) {
			nodes[at + i].next = i + 1 < lengths[r] ? &nodes[at + i + 1] : 0;
			nodes[at + i].previous = i > 0 ? &nodes[at + i - 1] : 0;
		}
		at += lengths[r];
	}
}

static int g_railTests = 0;
static const float* g_refuse = 0;
static bool countingTest(const float* from, const float*) {
	g_railTests++;
	return from != g_refuse;
}

static void testFindRail() {
	g_case = "find rail: one rail";
	{
		static const float line[][3] = { { 0, 0, 0 }, { 0, 0, 100 }, { 0, 0, 200 } };
		std::vector<FakeNode> nodes;
		addRail(nodes, line, 3);
		std::vector<int> lengths(1, 3);
		linkRails(nodes, lengths);
		const RailNodes n = fakeNodes(nodes);
		RailPass p;
		// down onto the second piece, 10 beside it
		const float a[3] = { 10, 6, 140 }, b[3] = { 10, -4, 150 };
		const unsigned char* found = FindRail(n, a, b, 0, 1.0f, 100, 0, p);
		CHECK(found == (const unsigned char*)&nodes[1]);
		CHECK(p.miss <= 1.0f);
		CHECK(near(fabs(p.side), 10.0, 0.01));
		CHECK(near(p.point[0], 0.0, 0.01) && near(p.point[1], 0.0, 0.01) && p.point[2] > 139.0f && p.point[2] < 151.0f);
		// onto the first piece
		const float c[3] = { -5, 6, 40 }, d[3] = { -5, -4, 50 };
		CHECK(FindRail(n, c, d, 0, 1.0f, 100, 0, p) == (const unsigned char*)&nodes[0]);
		// too far beside it: nothing, and the rail that came closest is reported
		const float e[3] = { 30, 6, 140 }, f[3] = { 30, -4, 150 };
		CHECK(FindRail(n, e, f, 0, 1.0f, 100, 0, p) == 0);
		CHECK(p.miss > 1.0f && p.miss < 2.0f);
		CHECK(near(fabs(p.side), 30.0, 0.01));
		// twice the reach: taken
		CHECK(FindRail(n, e, f, 0, 1.0f, 200, 0, p) == (const unsigned char*)&nodes[1]);
		// nowhere near any rail
		const float g[3] = { 900, 6, 140 }, h[3] = { 900, -4, 150 };
		CHECK(FindRail(n, g, h, 0, 1.0f, 100, 0, p) == 0);
		CHECK(p.miss >= kNoRail);
		// the last node of a rail has no piece of its own
		const float i2[3] = { 0, 6, 205 }, j[3] = { 0, -4, 210 };
		CHECK(FindRail(n, i2, j, 0, 1.0f, 100, 0, p) == (const unsigned char*)&nodes[1]);
		CHECK(near(p.point[2], 200.0, 0.01));
		// the node the game says to leave alone
		CHECK(FindRail(n, a, b, (const unsigned char*)&nodes[1], 1.0f, 100, 0, p) == 0);
		// switched off
		CHECK(FindRail(n, a, b, 0, 1.0f, 0, 0, p) == 0);
		CHECK(p.miss >= kNoRail);
	}

	g_case = "find rail: the rail under the board, not the one it travels along";
	{
		// A box of ledges: one edge along z at x = 0, one along x at z = 100. The board flies
		// along z over the second, 15 beside the first: THUG takes the first (it runs the way the
		// board goes). The board is on the second.
		static const float first[][3] = { { 0, 0, 0 }, { 0, 0, 100 } };
		static const float second[][3] = { { 0, 0, 100 }, { 60, 0, 100 } };
		std::vector<FakeNode> nodes;
		addRail(nodes, first, 2);
		addRail(nodes, second, 2);
		std::vector<int> lengths(2, 2);
		linkRails(nodes, lengths);
		const RailNodes n = fakeNodes(nodes);
		RailPass p;
		const float a[3] = { 15, 5, 94 }, b[3] = { 15, -3, 104 };
		const unsigned char* found = FindRail(n, a, b, 0, 1.0f, 100, 0, p);
		CHECK(found == (const unsigned char*)&nodes[2]);
		CHECK(p.miss < 0.3f);
		CHECK(near(p.point[0], 15.0, 0.1) && near(p.point[2], 100.0, 0.01));
		// further down the first edge, away from the corner, the first edge it is
		const float c[3] = { 15, 5, 34 }, d[3] = { 15, -3, 44 };
		CHECK(FindRail(n, c, d, 0, 1.0f, 100, 0, p) == (const unsigned char*)&nodes[0]);
		// with the order of the rails the other way around the answer is the same
		std::vector<FakeNode> other;
		addRail(other, second, 2);
		addRail(other, first, 2);
		linkRails(other, lengths);
		const RailNodes m = fakeNodes(other);
		CHECK(FindRail(m, a, b, 0, 1.0f, 100, 0, p) == (const unsigned char*)&other[0]);
		CHECK(FindRail(m, c, d, 0, 1.0f, 100, 0, p) == (const unsigned char*)&other[2]);
	}

	g_case = "find rail: nodes that do not count";
	{
		static const float line[][3] = { { 0, 0, 0 }, { 0, 0, 100 } };
		std::vector<FakeNode> nodes;
		addRail(nodes, line, 2);
		std::vector<int> lengths(1, 2);
		linkRails(nodes, lengths);
		RailPass p;
		const float a[3] = { 5, 6, 40 }, b[3] = { 5, -4, 50 };
		CHECK(FindRail(fakeNodes(nodes), a, b, 0, 1.0f, 100, 0, p) != 0);
		// switched off by the level
		nodes[0].flags = 0;
		CHECK(FindRail(fakeNodes(nodes), a, b, 0, 1.0f, 100, 0, p) == 0);
		CHECK(p.miss >= kNoRail);
		// only for climbing
		nodes[0].flags = kFakeActive | kFakeClimbOnly;
		CHECK(FindRail(fakeNodes(nodes), a, b, 0, 1.0f, 100, 0, p) == 0);
		// other flags do not matter
		nodes[0].flags = kFakeActive | 0xFFFFFFEBu;
		CHECK(FindRail(fakeNodes(nodes), a, b, 0, 1.0f, 100, 0, p) != 0);
		// no nodes at all
		std::vector<FakeNode> none;
		CHECK(FindRail(fakeNodes(none), a, b, 0, 1.0f, 100, 0, p) == 0);
		RailNodes broken = fakeNodes(nodes);
		broken.count = -3;
		CHECK(FindRail(broken, a, b, 0, 1.0f, 100, 0, p) == 0);
		broken = fakeNodes(nodes);
		broken.stride = 0;
		CHECK(FindRail(broken, a, b, 0, 1.0f, 100, 0, p) == 0);
		// positions that are not numbers
		const float nan = (float)sqrt(-1.0);
		const float bad[3] = { nan, nan, nan };
		CHECK(FindRail(fakeNodes(nodes), bad, b, 0, 1.0f, 100, 0, p) == 0);
		CHECK(FindRail(fakeNodes(nodes), 0, b, 0, 1.0f, 100, 0, p) == 0);
	}

	g_case = "find rail: what the game's own search passes over";
	{
		static const float line[][3] = { { 0, 0, 0 }, { 0, 0, 100 } };
		std::vector<FakeNode> nodes;
		addRail(nodes, line, 2);
		std::vector<int> lengths(1, 2);
		linkRails(nodes, lengths);
		RailPass p;
		// a board that has not moved in this frame: no rail, though it sits right on one
		const float a[3] = { 2, 1, 50 };
		const float nearly[3] = { 2, 1, 50.000004f };
		const float moved[3] = { 2, 1, 50.1f };
		CHECK(FindRail(fakeNodes(nodes), a, a, 0, 1.0f, 100, 0, p) == 0);
		CHECK(p.miss >= kNoRail);
		CHECK(FindRail(fakeNodes(nodes), a, nearly, 0, 1.0f, 100, 0, p) == 0);
		CHECK(FindRail(fakeNodes(nodes), a, moved, 0, 1.0f, 100, 0, p) == (const unsigned char*)&nodes[0]);

		// a piece without length in the middle of a rail: never that one, its neighbours instead
		static const float knot[][3] = { { 0, 0, 0 }, { 0, 0, 100 }, { 0, 0, 100 }, { 0, 0, 200 } };
		std::vector<FakeNode> knotted;
		addRail(knotted, knot, 4);
		std::vector<int> four(1, 4);
		linkRails(knotted, four);
		for (int i = 0; i < 200; i++) {
			const float x = (frand() - 0.5f) * 30.0f, z = 80.0f + frand() * 40.0f;
			const float c[3] = { x, 6, z }, d[3] = { x + (frand() - 0.5f) * 10.0f, -4, z + (frand() - 0.5f) * 10.0f };
			const unsigned char* found = FindRail(fakeNodes(knotted), c, d, 0, 1.0f, 100, 0, p);
			CHECK(found != (const unsigned char*)&knotted[1]);
			if (fabsf(x) < 5.0f) CHECK(found == (const unsigned char*)&knotted[0] || found == (const unsigned char*)&knotted[2]);
		}
		// two nodes on one spot and nothing else: no rail at all
		static const float spot[][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
		std::vector<FakeNode> twin;
		addRail(twin, spot, 2);
		linkRails(twin, lengths);
		const float e[3] = { 1, 6, -3 }, f[3] = { 1, -4, 3 };
		CHECK(FindRail(fakeNodes(twin), e, f, 0, 1.0f, 100, 0, p) == 0);
	}

	g_case = "find rail: a point";
	{
		static const float post[][3] = { { 50, 20, 50 } };
		std::vector<FakeNode> nodes;
		addRail(nodes, post, 1);
		std::vector<int> lengths(1, 1);
		linkRails(nodes, lengths);
		RailPass p;
		const float a[3] = { 45, 26, 40 }, b[3] = { 45, 24, 52 };
		CHECK(FindRail(fakeNodes(nodes), a, b, 0, 1.0f, 100, 0, p) == (const unsigned char*)&nodes[0]);
		CHECK(near(p.point[0], 50.0, 1e-4) && near(p.point[1], 20.0, 1e-4) && near(p.point[2], 50.0, 1e-4));
		const float c[3] = { 20, 26, 40 }, d[3] = { 20, 24, 52 };
		CHECK(FindRail(fakeNodes(nodes), c, d, 0, 1.0f, 100, 0, p) == 0);
	}

	g_case = "find rail: the game's rules for parks from the editor";
	{
		static const float along[][3] = { { 0, 0, 0 }, { 0, 0, 100 } };
		static const float across[][3] = { { 200, 0, 50 }, { 300, 0, 50 } };
		std::vector<FakeNode> nodes;
		addRail(nodes, along, 2);
		addRail(nodes, across, 2);
		std::vector<int> lengths(2, 2);
		linkRails(nodes, lengths);
		const RailNodes n = fakeNodes(nodes);
		RailPass p;
		const float minCos = 0.6428f;    // 50 degrees
		// travelling along z: the rail along z is taken, the one across it is not
		const float a[3] = { 5, 6, 40 }, b[3] = { 5, -4, 50 };
		CHECK(FindRail(n, a, b, 0, minCos, 100, 0, p) == (const unsigned char*)&nodes[0]);
		const float c[3] = { 250, 6, 40 }, d[3] = { 250, -4, 52 };
		CHECK(FindRail(n, c, d, 0, 1.0f, 100, 0, p) == (const unsigned char*)&nodes[2]);
		CHECK(FindRail(n, c, d, 0, minCos, 100, 0, p) == 0);
		// straight down: no direction of travel, no rule
		const float e[3] = { 250, 6, 50 }, f[3] = { 250, -4, 50 };
		CHECK(FindRail(n, e, f, 0, minCos, 100, 0, p) == (const unsigned char*)&nodes[2]);
		// a rail that stands on end runs no way at all: with the rule it is left out
		{
			static const float pole[][3] = { { 500, 0, 0 }, { 500, 100, 0 } };
			std::vector<FakeNode> upright;
			addRail(upright, pole, 2);
			std::vector<int> two(1, 2);
			linkRails(upright, two);
			const float g[3] = { 505, 50, -5 }, h[3] = { 505, 50, 5 };
			CHECK(FindRail(fakeNodes(upright), g, h, 0, 1.0f, 100, 0, p) == (const unsigned char*)&upright[0]);
			CHECK(FindRail(fakeNodes(upright), g, h, 0, minCos, 100, 0, p) == 0);
		}

		// the game's own test of a piece of rail: asked only about rails within reach
		g_railTests = 0;
		g_refuse = 0;
		CHECK(FindRail(n, a, b, 0, 1.0f, 100, countingTest, p) == (const unsigned char*)&nodes[0]);
		CHECK_EQ(g_railTests, 1);
		g_refuse = nodes[0].position;
		CHECK(FindRail(n, a, b, 0, 1.0f, 100, countingTest, p) == 0);
		g_refuse = 0;
		g_railTests = 0;
		const float g[3] = { 30, 6, 40 }, h[3] = { 30, -4, 50 };
		CHECK(FindRail(n, g, h, 0, 1.0f, 100, countingTest, p) == 0);
		CHECK_EQ(g_railTests, 0);
	}

	g_case = "find rail: against looking at every piece";
	{
		// Rails strewn about, a board flying through them: the search with its boxes has to agree
		// with the plain way of trying every piece
		int takenAt100 = 0;
		for (int round = 0; round < 300; round++) {
			std::vector<FakeNode> nodes;
			std::vector<int> lengths;
			const int rails = 1 + (int)(frand() * 6.0f);
			for (int r = 0; r < rails; r++) {
				const int count = 1 + (int)(frand() * 4.0f);
				float points[4][3];
				float x = (frand() - 0.5f) * 200.0f, y = (frand() - 0.5f) * 60.0f, z = (frand() - 0.5f) * 200.0f;
				for (int i = 0; i < count; i++) {
					points[i][0] = x; points[i][1] = y; points[i][2] = z;
					x += (frand() - 0.5f) * 120.0f; y += (frand() - 0.5f) * 40.0f; z += (frand() - 0.5f) * 120.0f;
				}
				addRail(nodes, points, count);
				lengths.push_back(count);
			}
			linkRails(nodes, lengths);
			for (size_t i = 0; i < nodes.size(); i++) {
				if (frand() < 0.1f) nodes[i].flags = 0;
				else if (frand() < 0.1f) nodes[i].flags |= kFakeClimbOnly;
			}
			const RailNodes n = fakeNodes(nodes);
			const int percent = 25 + (int)(frand() * 300.0f);
			for (int flight = 0; flight < 40; flight++) {
				const float a[3] = { (frand() - 0.5f) * 260.0f, (frand() - 0.5f) * 80.0f, (frand() - 0.5f) * 260.0f };
				const float b[3] = { a[0] + (frand() - 0.5f) * 30.0f, a[1] + (frand() - 0.5f) * 24.0f, a[2] + (frand() - 0.5f) * 30.0f };
				RailPass p;
				const unsigned char* found = FindRail(n, a, b, 0, 1.0f, percent, 0, p);

				const FakeNode* want = 0;
				float least = kNoRail;
				for (size_t i = 0; i < nodes.size(); i++) {
					const FakeNode& node = nodes[i];
					if (!(node.flags & kFakeActive) || (node.flags & kFakeClimbOnly)) continue;
					if (!node.next && node.previous) continue;
					if (node.next) {
						const float dx = node.next->position[0] - node.position[0], dy = node.next->position[1] - node.position[1], dz = node.next->position[2] - node.position[2];
						if (dx * dx + dy * dy + dz * dz < 0.01f * 0.01f) continue;
					}
					const RailPass one = PassRail(a, b, node.position, node.next ? node.next->position : 0, percent);
					if (one.miss < least) { least = one.miss; want = &node; }
				}
				if (least <= 1.0f) {
					CHECK(found == (const unsigned char*)want);
					CHECK(near(p.miss, least, 1e-5));
				}
				else {
					CHECK(found == 0);
					// What is reported then is the closest rail among those near enough to be
					// looked at, so it cannot be closer than the closest of all
					CHECK(p.miss > 1.0f);
					CHECK(p.miss >= least - 1e-5f);
					// and a rail that only just missed is always the one reported
					if (least < 2.0f) CHECK(near(p.miss, least, 1e-5));
				}
				// a longer reach never loses a rail
				if (found) {
					RailPass q;
					CHECK(FindRail(n, a, b, 0, 1.0f, percent * 2, 0, q) != 0);
				}
				// and at 100 percent the rail that is taken is one THUG's own search reaches too
				{
					RailPass q;
					const unsigned char* taken = FindRail(n, a, b, 0, 1.0f, 100, 0, q);
					if (taken) {
						const FakeNode* node = (const FakeNode*)taken;
						takenAt100++;
						if (node->next) CHECK(thugWouldReach(a, b, node->position, node->next->position));
						else CHECK(slowDistance(a, b, node->position, node->position) <= 20.0);
					}
				}
			}
		}
		CHECK(takenAt100 > 300);       // the test has to have had rails to take
	}
}

// ---------------------------------------------------------------------------------------------
// Random play: things that must hold whatever the stick and the skater do
// ---------------------------------------------------------------------------------------------


static void testFuzz(double hz) {
	char name[128];
	snprintf(name, sizeof(name), "fuzz, %g Hz", hz);
	g_case = name;

	int railPops = 0, switches = 0, stepPresses = 0, stepOffs = 0;
	for (int run = 0; run < 150; run++) {
		Sim s(hz);
		s.cfg.nollie = run % 5 == 0;
		s.railCatch = run % 7 != 0;
		s.cfg.grindExit = run % 11 != 0;
		s.game.noRelease = run % 3 == 0;
		for (int move = 0; move < 60; move++) {
			// a stick move
			const float r = frand();
			if (r < 0.25f) s.spring(10 + frand() * 80);
			else if (r < 0.55f) s.toward((float)((int)(frand() * 8) * 45), 10 + frand() * 80, 0.5f + 0.5f * frand());
			else if (r < 0.75f) s.arcTo((float)((int)(frand() * 8) * 45), 30 + frand() * 150);
			else s.hold(20 + frand() * 400);
			s.lt = frand() < 0.15f;
			s.rt = frand() < 0.1f;

			// The game's word that the board is out of speed: on a rail, now and then, sometimes
			// taken back. Off a rail it says nothing (one run in four it talks nonsense there too).
			if (s.state == STATE_RAIL || run % 4 == 0) {
				const float w = frand();
				if (w < 0.25f) { s.railOff = true; s.offSide = (int)(frand() * 3.0f) - 1; }
				else if (w < 0.35f) s.railOff = false;
			}
			else {
				s.railOff = false;
			}

			// a change of state now and then, in an order the game could produce
			if (frand() < 0.3f) {
				if (s.state == STATE_GROUND) { s.state = STATE_AIR; }
				else if (s.state == STATE_AIR) {
					if (frand() < 0.6f) { s.land(frand() < 0.5f); }
					else { s.state = frand() < 0.8f ? STATE_GROUND : STATE_WALL; }
				}
				else if (s.state == STATE_RAIL) { s.state = STATE_AIR; s.game.leave(); }
				else { s.state = STATE_AIR; }
			}
		}
		s.spring();
		s.lt = s.rt = false;
		s.railOff = false;
		s.state = STATE_GROUND;
		s.game.leave();
		s.hold(3000);
		CHECK(!s.rec.Busy());
		stepOffs += s.game.stepOffs;

		const int n = (int)s.outs.size();
		for (int i = 0; i < n; i++) {
			const Output& o = s.outs[i];
			// The grind button is never held for a rail the game catches by itself: on a rail it is
			// only tapped for a switch
			if (s.railCatch && o.grind) CHECK_EQ(s.states[i], STATE_RAIL);
			// On a rail there is no grab that outlasts a switch press
			if (o.switchTo != GRIND_UNKNOWN) {
				switches++;
				CHECK_EQ(s.states[i], STATE_RAIL);
			}
			// A trick press owns exactly one direction
			if (o.flip && s.states[i] != STATE_RAIL) CHECK(o.ownsDirection);
			if (o.ownsDirection) CHECK(o.dir != DIR_NONE);
			// Tilt only in the air
			if (s.tilts[i].valid) CHECK_EQ(s.states[i], STATE_AIR);
			// The button that steps off a rail: on a rail only, only where that is switched on, and
			// with no switch or trick press beside it
			if (o.stepOff) {
				CHECK_EQ(s.states[i], STATE_RAIL);
				CHECK(s.cfg.grindExit != 0);
				CHECK(!o.flip);
				CHECK(!o.grab);
				CHECK(!o.grind);
				CHECK(!o.ownsDirection);
				CHECK(o.stepSide >= -1 && o.stepSide <= 1);
			}
			CHECK(o.stepStarted == (o.stepOff && (i == 0 || !s.outs[i - 1].stepOff)));
		}

		// Every press of it lasts at least two frames, unless the board left the rail under it or
		// the player popped off, and two presses on one rail are half a second apart
		{
			const std::vector<Press> v = s.presses(&Output::stepOff);
			stepPresses += (int)v.size();
			for (size_t k = 0; k < v.size(); k++) {
				const int last = v[k].first + v[k].frames - 1;
				if (last + 1 < n) {
					const bool cutShort = s.states[last + 1] != STATE_RAIL || s.outs[last + 1].ollie;
					if (!cutShort) CHECK(v[k].frames >= 2);
				}
				CHECK(v[k].frames * 1000.0 / hz <= 50.0 + 4000.0 / hz + 1.0);
				if (k > 0) {
					bool sameRail = true;
					for (int i = v[k - 1].first; i <= v[k].first; i++) sameRail = sameRail && s.states[i] == STATE_RAIL && !s.outs[i].ollie;
					if (sameRail) CHECK(s.msBetween(v[k - 1].first + v[k - 1].frames, v[k].first) >= 500.0);
				}
			}
		}

		// Every press of Ollie that starts on a rail is a tap of at least two frames, with nothing
		// else down, and it is over quickly
		std::vector<Press> ollies = s.presses(&Output::ollie);
		for (size_t k = 0; k < ollies.size(); k++) {
			const Press& p = ollies[k];
			if (s.states[p.first] != STATE_RAIL) continue;
			railPops++;
			CHECK(p.frames >= 2);
			CHECK(p.frames * 1000.0 / hz <= s.cfg.railTap + 4000.0 / hz + 1.0);
			for (int i = p.first; i < p.first + p.frames && i < n; i++) {
				// nothing new goes down while Ollie is: no switch press, no trick press
				CHECK(!s.outs[i].grind);
				if (i > p.first) CHECK(!s.outs[i].flip || s.outs[i - 1].flip);
			}
		}

		// Every button press lasts at least two frames, unless the board left the rail under it
		bool Output::* const buttons[] = { &Output::flip, &Output::grab, &Output::grind };
		for (size_t b = 0; b < 3; b++) {
			std::vector<Press> v = s.presses(buttons[b]);
			for (size_t k = 0; k < v.size(); k++) {
				const int last = v[k].first + v[k].frames - 1;
				if (last + 1 >= n) continue;
				const bool cutShort = s.states[last + 1] != s.states[v[k].first] || s.outs[last + 1].ollie;
				if (!cutShort) CHECK(v[k].frames >= 2);
			}
		}
		// The stand-in game never saw the same grind started twice by the buttons
		CHECK_EQ(s.game.restarts, 0);
	}
	printf("  %3g Hz: %d pops out of grinds, %d switches, %d presses to step off a rail (%d taken) in the random runs\n", hz, railPops, switches, stepPresses, stepOffs);
}

int main(int argc, char** argv) {
	// The folder with the game's skater scripts, for the check against its grind table
	const std::string scriptDir = argc > 1 ? argv[1] : "scripts/qb/source/scripts/game/skater";
	testNameChecksum();
	testGrindTable(scriptDir);
	testTiltTable();
	testDragNumbers();
	testReach();
	testFindRail();
	const double rates[] = { 30.0, 60.0, 120.0, 144.0, 240.0 };
	for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
		const double hz = rates[i];
		const int before = g_failures;
		testTiltTracking(hz);
		testAir(hz);
		testRailPop(hz);
		testRailPopPatterns(hz);
		testRailSwitch(hz);
		testRailRun(hz);
		testStepOff(hz);
		testGrabs(hz);
		testUnknownState(hz);
		testFuzz(hz);
		printf("  %3g Hz: %s\n", hz, g_failures == before ? "ok" : "FAILED");
	}
	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
