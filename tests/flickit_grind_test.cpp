// Offline tests for the grind side of src/flickit.cpp: the board tilt in the air, the flick-only pop
// on a rail and the switches between 50-50, 5-0 and nosegrind. No game, SDL or Windows needed:
//   g++ -std=c++11 -Wall -Wextra -I src tests/flickit_grind_test.cpp src/flickit.cpp -o flickit_grind_test && ./flickit_grind_test
// Run it from the top of the repository, or pass the folder scripts/qb/source/scripts/game/skater: the rows the mod
// hands the game are checked against the game's own grind table in grindlist.q.
//
// The skater's state, which the mod reads from the game, is scripted here. For the switches a
// small stand-in for the game watches the buttons the way THUG's grind script does: two presses in
// order within 300 ms, taken only once the grind has settled, the new grind reported 15 frames later.

#include "flickit.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

	// What the mod has been told (it hears of a new grind 15 frames after it started)
	int reportedKind;
	bool reportedReady;
	uint32_t serial;

	struct Event { int button; uint32_t time; };   // 0 flip, 1 grab, 2 grind
	std::vector<Event> events;
	bool prev[3];
	int switches;
	int restarts;          // the same grind started again (the bug above, if the mod ever triggers it)

	FakeGame() : on(false), kind(GRIND_UNKNOWN), extraTricks(false), settleFrames(45), sinceStart(0), buggyNoseEntry(true), deaf(false),
		reportedKind(GRIND_UNKNOWN), reportedReady(false), serial(0), switches(0), restarts(0) {
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
	FakeGame game;
	std::vector<Output> outs;
	std::vector<uint32_t> times;
	std::vector<int> states;
	std::vector<Tilt> tilts;

	explicit Sim(double rate = 60.0, int startState = STATE_GROUND) : hz(rate), clock(0.0), epoch(5000), x(0), y(0), lt(false), rt(false), state(startState), railCatch(true) {
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
// Random play: things that must hold whatever the stick and the skater do
// ---------------------------------------------------------------------------------------------

static unsigned int g_seed = 12345;
static float frand() {
	g_seed = g_seed * 1664525u + 1013904223u;
	return (float)((g_seed >> 8) & 0xFFFF) / 65535.0f;
}

static void testFuzz(double hz) {
	char name[128];
	snprintf(name, sizeof(name), "fuzz, %g Hz", hz);
	g_case = name;

	int railPops = 0, switches = 0;
	for (int run = 0; run < 150; run++) {
		Sim s(hz);
		s.cfg.nollie = run % 5 == 0;
		s.railCatch = run % 7 != 0;
		int phase = 0;                          // what the skater does next
		for (int move = 0; move < 60; move++) {
			// a stick move
			const float r = frand();
			if (r < 0.25f) s.spring(10 + frand() * 80);
			else if (r < 0.55f) s.toward((float)((int)(frand() * 8) * 45), 10 + frand() * 80, 0.5f + 0.5f * frand());
			else if (r < 0.75f) s.arcTo((float)((int)(frand() * 8) * 45), 30 + frand() * 150);
			else s.hold(20 + frand() * 400);
			s.lt = frand() < 0.15f;
			s.rt = frand() < 0.1f;

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
			phase++;
		}
		s.spring();
		s.lt = s.rt = false;
		s.state = STATE_GROUND;
		s.game.leave();
		s.hold(3000);
		CHECK(!s.rec.Busy());

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
	printf("  %3g Hz: %d pops out of grinds, %d switches in the random runs\n", hz, railPops, switches);
}

int main(int argc, char** argv) {
	// The folder with the game's skater scripts, for the check against its grind table
	const std::string scriptDir = argc > 1 ? argv[1] : "scripts/qb/source/scripts/game/skater";
	testNameChecksum();
	testGrindTable(scriptDir);
	testTiltTable();
	const double rates[] = { 30.0, 60.0, 120.0, 144.0, 240.0 };
	for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
		const double hz = rates[i];
		const int before = g_failures;
		testTiltTracking(hz);
		testAir(hz);
		testRailPop(hz);
		testRailPopPatterns(hz);
		testRailSwitch(hz);
		testGrabs(hz);
		testUnknownState(hz);
		testFuzz(hz);
		printf("  %3g Hz: %s\n", hz, g_failures == before ? "ok" : "FAILED");
	}
	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
