// Offline tests for the physics numbers in src/flickit.cpp: what Flickit asks of the game's gravity
// and of the speed of a jump, and how it pushes, rolls and steers the board, against Skate's numbers
// and against THUG's own.
// No game, SDL or Windows needed:
//   g++ -std=c++11 -Wall -Wextra -I src tests/flickit_physics_test.cpp src/flickit.cpp -o flickit_physics_test && ./flickit_physics_test
// Run it from the top of the repository, or pass the folder scripts/qb/source/scripts/game/skater: THUG's numbers
// in flickit.cpp are checked against the game's own physics.q.

#include "flickit.h"

#include <math.h>
#include <stdio.h>
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

static bool near(double a, double b, double tolerance) {
	return fabs(a - b) <= tolerance;
}

// ---------------------------------------------------------------------------------------------
// The game's own numbers, read from its script source
// ---------------------------------------------------------------------------------------------

static std::string readFile(const std::string& path) {
	std::string out;
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return out;
	char buffer[4096];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) out.append(buffer, n);
	fclose(f);
	return out;
}

// "name = value" at the start of a line (after blanks): the value. found says whether it is there.
static double scriptNumber(const std::string& text, const char* name, bool& found) {
	found = false;
	const std::string key(name);
	size_t at = 0;
	while ((at = text.find(key, at)) != std::string::npos) {
		size_t start = at;
		while (start > 0 && (text[start - 1] == ' ' || text[start - 1] == '\t')) start--;
		const bool lineStart = start == 0 || text[start - 1] == '\n';
		size_t p = at + key.size();
		while (p < text.size() && (text[p] == ' ' || text[p] == '\t')) p++;
		if (lineStart && p < text.size() && text[p] == '=') {
			found = true;
			return atof(text.c_str() + p + 1);
		}
		at += key.size();
	}
	return 0.0;
}

// "name = { PAIR(low, high) ..." : the two numbers
static bool scriptPair(const std::string& text, const char* name, double& low, double& high) {
	const std::string key = std::string(name) + " = { PAIR(";
	const size_t at = text.find(key);
	if (at == std::string::npos) return false;
	const char* p = text.c_str() + at + key.size();
	char* end = 0;
	low = strtod(p, &end);
	if (end == p || *end != ',') return false;
	p = end + 1;
	high = strtod(p, &end);
	return end != p;
}

static void testAgainstTheScripts(const std::string& dir) {
	g_case = "THUG's numbers: physics.q";
	const std::string text = readFile(dir + "/physics.q");
	CHECK(!text.empty());
	if (text.empty()) {
		printf("  (no physics.q in %s)\n", dir.c_str());
		return;
	}
	bool found = false;
	const double gravity = scriptNumber(text, "Physics_Air_Gravity", found);
	CHECK(found);
	CHECK(near(-gravity, kThugAirGravity, 1e-3));
	double low = 0.0, high = 0.0;
	CHECK(scriptPair(text, "Physics_Jump_Speed_Stat", low, high));
	CHECK(near(low, kThugPopWorst, 1e-3));
	CHECK(near(high, kThugPopBest, 1e-3));
	CHECK(scriptPair(text, "Physics_Jump_Speed_min_Stat", low, high));
	CHECK(near(low, kThugPopTap, 1e-3));
	CHECK(near(high, kThugPopTap, 1e-3));
	// the hang stats that divide the gravity: 1 on the flat, so the number above is what acts there
	const double hang = scriptNumber(text, "Physics_Air_hang_Stat", found);
	CHECK(found);
	CHECK(near(hang, 1.0, 1e-6));

	// Rolling. The turn: 1.8 radians a second
	const double turn = scriptNumber(text, "Physics_Ground_Rotation", found);
	CHECK(found);
	CHECK(near(turn, kThugGroundTurn, 1e-3));
	// The kick: up to 394..496 units/s standing and 532..675 crouched. Skate's top speed for a
	// push, at the scale used here, is the best of that.
	CHECK(scriptPair(text, "Skater_Max_Standing_Kick_Speed_Stat", low, high));
	CHECK(near(low, 394.0, 1e-3) && near(high, 496.0, 1e-3));
	CHECK(scriptPair(text, "Skater_Max_Crouched_Kick_Speed_Stat", low, high));
	CHECK(near(low, 532.0, 1e-3) && near(high, 675.0, 1e-3));
	CHECK(near(kPushTopSpeed * kUnitsPerMetre, high, 10.0));
	CHECK(scriptPair(text, "Physics_Standing_Acceleration_Stat", low, high));
	CHECK(near(low, 629.0, 1e-3) && near(high, 700.0, 1e-3));
	CHECK(scriptPair(text, "Physics_Crouching_Acceleration_stat", low, high));
	CHECK(near(low, 1057.0, 1e-3) && near(high, 1200.0, 1e-3));
	// The drag that holds the skater back: the game takes this number times 60 times the square
	// of the speed off the speed each second. Standing at 700 units/s that is 294 units/s2.
	const double drag = scriptNumber(text, "Physics_Standing_Air_Friction", found);
	CHECK(found);
	CHECK(near(drag * 60.0 * 700.0 * 700.0, 294.0, 0.5));
	// Next to nothing for the wheels
	const double wheels = scriptNumber(text, "Physics_Rolling_Friction", found);
	CHECK(found);
	CHECK(wheels * 60.0 < 0.001);

	// Spinning. 6.85 to 7.75 radians a second for Left or Right in the air, once the button has
	// been down for a tenth of a second. (The game would bring the turning in between that
	// time and a second one, but the second one is the shorter: it is all there at once.)
	CHECK(scriptPair(text, "Physics_Air_Rotation_stat", low, high));
	CHECK(near(low, kThugAirSpinWorst, 1e-3) && near(high, kThugAirSpinBest, 1e-3));
	const double wait = scriptNumber(text, "Physics_Air_No_Rotate_Time", found);
	CHECK(found);
	CHECK(near(wait * 0.001, kThugAirSpinWait, 1e-6));
	const double ramp = scriptNumber(text, "Physics_Air_Ramp_Rotate_Time", found);
	CHECK(found);
	CHECK(ramp < wait);
}

// ---------------------------------------------------------------------------------------------
// Jumps
// ---------------------------------------------------------------------------------------------

static void testJumps() {
	// Skate 3, as measured there: gravity 9.8 m/s2; the pop lifts the rider 0.38 m (quick) to
	// 0.76 m (full), which takes 0.56 to 0.79 s. One of Skate's metres is 80 of THUG's units.
	const double g = 9.8, unit = 80.0;
	const double full = 0.76, quick = 0.38;

	g_case = "jumps: the settings and their defaults";
	{
		Settings s;
		SetDefaults(s);
		CHECK_EQ(s.airGravity, 100);
		CHECK_EQ(s.jumpPop, 100);
		// 0 is THUG's own; anything else stays within reason
		s.airGravity = 0; s.jumpPop = 0;
		Sanitize(s);
		CHECK_EQ(s.airGravity, 0);
		CHECK_EQ(s.jumpPop, 0);
		s.airGravity = -7; s.jumpPop = -1;
		Sanitize(s);
		CHECK_EQ(s.airGravity, 0);
		CHECK_EQ(s.jumpPop, 0);
		s.airGravity = 3; s.jumpPop = 1;
		Sanitize(s);
		CHECK_EQ(s.airGravity, 25);
		CHECK_EQ(s.jumpPop, 25);
		s.airGravity = 100000; s.jumpPop = 100000;
		Sanitize(s);
		CHECK_EQ(s.airGravity, 400);
		CHECK_EQ(s.jumpPop, 400);
		CHECK_EQ(kJumpMostPercent, 400);
		s.airGravity = 172; s.jumpPop = 146;
		Sanitize(s);
		CHECK_EQ(s.airGravity, 172);
		CHECK_EQ(s.jumpPop, 146);
	}

	g_case = "jumps: the factors";
	{
		// off: the game's numbers as they are
		CHECK(AirGravityFactor(0) == 1.0f);
		CHECK(AirGravityFactor(-5) == 1.0f);
		CHECK(JumpPopFactor(0) == 1.0f);
		CHECK(JumpPopFactor(-5) == 1.0f);
		// 100: Skate's gravity, 784 units/s2, 58 percent of THUG's
		CHECK(near(AirGravityFactor(100) * kThugAirGravity, g * unit, 0.01));
		CHECK(near(AirGravityFactor(100), 0.5807, 0.0005));
		// and the pop that lifts the rider by Skate's full pop when THUG's is at its best: 69 percent
		CHECK(near(JumpPopFactor(100) * kThugPopBest, sqrt(2.0 * g * full) * unit, 0.01));
		CHECK(near(JumpPopFactor(100), 0.686, 0.0005));
		// the settings are percent
		CHECK(near(AirGravityFactor(50), 0.5 * AirGravityFactor(100), 1e-6));
		CHECK(near(AirGravityFactor(200), 2.0 * AirGravityFactor(100), 1e-6));
		CHECK(near(JumpPopFactor(50), 0.5 * JumpPopFactor(100), 1e-6));
		// what gives THUG's own numbers back, should anyone ask: 172 and 146
		CHECK(near(AirGravityFactor(172), 1.0, 0.005));
		CHECK(near(JumpPopFactor(146), 1.0, 0.005));
		// never nothing, never upside down
		for (int percent = -10; percent <= kJumpMostPercent; percent++) {
			CHECK(AirGravityFactor(percent) > 0.0f);
			CHECK(JumpPopFactor(percent) > 0.0f);
		}
	}

	g_case = "jumps: THUG's own, off flat ground";
	{
		// 45 to 75 units in 0.52 to 0.67 s
		const FlatJump tap = JumpOnFlat(kThugPopTap, 0, 0);
		const FlatJump best = JumpOnFlat(kThugPopBest, 0, 0);
		const FlatJump worst = JumpOnFlat(kThugPopWorst, 0, 0);
		CHECK(near(tap.height, 45.4, 0.1));
		CHECK(near(tap.seconds, 0.519, 0.001));
		CHECK(near(best.height, 75.0, 0.1));
		CHECK(near(best.seconds, 0.667, 0.001));
		CHECK(near(worst.height, 63.5, 0.1));
		CHECK(near(worst.seconds, 0.613, 0.001));
		// for a skater 77 units tall: 0.6 to 1 times his height
		CHECK(tap.height / 77.0 > 0.55 && best.height / 77.0 < 1.0);
		// nothing sensible to say about no pop
		CHECK(JumpOnFlat(0.0f, 100, 100).height == 0.0f);
		CHECK(JumpOnFlat(-5.0f, 100, 100).seconds == 0.0f);
	}

	g_case = "jumps: Skate's, in THUG";
	{
		const FlatJump best = JumpOnFlat(kThugPopBest, 100, 100);
		const FlatJump worst = JumpOnFlat(kThugPopWorst, 100, 100);
		const FlatJump tap = JumpOnFlat(kThugPopTap, 100, 100);
		// THUG's best pop: Skate's full pop, 0.76 m up in 0.79 s
		CHECK(near(best.height / unit, full, 0.002));
		CHECK(near(best.seconds, 2.0 * sqrt(2.0 * full / g), 0.002));
		CHECK(near(best.seconds, 0.788, 0.002));
		// every other pop THUG has lands inside what Skate's pops do: 0.38 to 0.76 m, 0.56 to 0.79 s
		CHECK(tap.height / unit > quick && tap.height / unit < full);
		CHECK(worst.height / unit > quick && worst.height / unit < full);
		CHECK(tap.seconds > 2.0 * sqrt(2.0 * quick / g) && tap.seconds < best.seconds);
		CHECK(worst.seconds > tap.seconds && worst.seconds < best.seconds);
		CHECK(near(tap.height, 36.8, 0.1));
		CHECK(near(tap.seconds, 0.613, 0.002));
		// against THUG's own: four fifths as high, a fifth longer, whatever the pop
		const float pops[] = { kThugPopTap, kThugPopWorst, kThugPopBest, 200.0f, 525.0f };
		for (size_t i = 0; i < sizeof(pops) / sizeof(pops[0]); i++) {
			const FlatJump theirs = JumpOnFlat(pops[i], 0, 0);
			const FlatJump ours = JumpOnFlat(pops[i], 100, 100);
			CHECK(near(ours.height / theirs.height, 0.81, 0.005));
			CHECK(near(ours.seconds / theirs.seconds, 1.18, 0.005));
		}
		// gravity alone, as off a ramp, where the speed is not the pop's: 1.7 times as high and long
		const FlatJump ramp = JumpOnFlat(700.0f, 100, 0);
		const FlatJump rampTheirs = JumpOnFlat(700.0f, 0, 0);
		CHECK(near(ramp.height / rampTheirs.height, 1.72, 0.01));
		CHECK(near(ramp.seconds / rampTheirs.seconds, 1.72, 0.01));
		// the pop alone leaves the time in the air to the game's gravity
		const FlatJump low = JumpOnFlat(kThugPopBest, 0, 100);
		CHECK(near(low.seconds / JumpOnFlat(kThugPopBest, 0, 0).seconds, JumpPopFactor(100), 1e-4));
	}

	g_case = "jumps: a jump stepped the way the game steps it";
	{
		// The game adds the pop to the upward speed once and takes gravity off it every frame.
		// Stepped at any frame rate the jump has to come out as JumpOnFlat says.
		const double rates[] = { 30.0, 60.0, 144.0, 240.0 };
		for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); r++) {
			const double dt = 1.0 / rates[r];
			double speed = kThugPopBest * JumpPopFactor(100);
			const double gravity = kThugAirGravity * AirGravityFactor(100);
			double height = 0.0, top = 0.0, seconds = 0.0;
			for (int frame = 0; frame < 100000; frame++) {
				height += speed * dt - 0.5 * gravity * dt * dt;
				speed -= gravity * dt;
				seconds += dt;
				if (height > top) top = height;
				if (height <= 0.0) break;
			}
			const FlatJump want = JumpOnFlat(kThugPopBest, 100, 100);
			CHECK(near(top, want.height, 1.0));
			CHECK(near(seconds, want.seconds, dt + 1e-6));
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Rolling
// ---------------------------------------------------------------------------------------------

// Skate 3's curves as read from its tuning, typed in here a second time: eight points, straight
// lines between them, flat before the first and after the last
struct Table {
	double x[8];
	double y[8];
};

static double at(const Table& t, double x) {
	if (x <= t.x[0]) return t.y[0];
	for (int i = 1; i < 8; i++) {
		if (x < t.x[i]) return t.y[i - 1] + (t.y[i] - t.y[i - 1]) * (x - t.x[i - 1]) / (t.x[i] - t.x[i - 1]);
	}
	return t.y[7];
}

// the speed a push is asked to add (m/s) over the share of the full hold
static const Table kSkatePush = { { 0, 0.241699904, 0.342629492, 0.419654697, 0.515272319, 0.767596304, 0.884462178, 1 },
	{ 0.75, 1.04464304, 1.43035698, 2.04107094, 2.5232141, 2.97321391, 3.50357103, 4.5 } };
// what the wheels take (m/s2) over the speed (m/s)
static const Table kSkateWheels = { { 0, 1.14514506, 2.28228211, 3.33133101, 4.19619608, 5.15715694, 6.52652597, 8 },
	{ 0.128571406, 0.151785702, 0.207142904, 0.292857111, 0.385714293, 0.441071391, 0.480357111, 0.5 } };
// what the air takes (m/s2) over the speed (m/s)
static const Table kSkateAir = { { 0, 8.12052155, 10.42134, 12.0454397, 12.8574896, 14.3322496, 18.4039097, 26.9781799 },
	{ 0, 0, 0.0299999993, 0.162857205, 0.600000024, 0.904053986, 1.04999995, 1.20000005 } };
// the trucks' angle as a share of 0.6 radians over the stick
static const Table kSkateStick = { { 0, 0.05859375, 0.25, 0.375, 0.508789122, 0.690429688, 0.857715428, 1 },
	{ 0, 0, 0.0337837897, 0.0777027011, 0.172297299, 0.334459513, 0.564189196, 0.834459424 } };
// and over the speed as a share of 50 m/s
static const Table kSkateTruckSpeed = { { 0.0569550917, 0.123767801, 0.173055902, 0.219058096, 0.297918886, 0.400000006, 0.5, 1 },
	{ 1, 0.969594598, 0.847972989, 0.608108103, 0.395270288, 0.300000012, 0.280000001, 0.270000011 } };

// One run of the push: the button is down from each "down" to the "up" after it (seconds)
struct Press {
	double down, up;
};

// What stands in for the game in a run: its scripts, and the animation they play
enum {
	SCRIPT_FLAT = 0,    // the loop of a skater on flat ground: plays the push while the Flip button counts as held,
	                    // waits for it to end, plays the idle, looks again on the next frame
	SCRIPT_NOLLIE = 1,  // the loop of a skater riding nollie: starts the push on a yes to CanKick if it is
	                    // not his animation already; on a no, and with his animation at its end, the idle
	SCRIPT_CROUCH = 2   // the crouch: starts the push while the button counts as held if it is not his
	                    // animation already; then, with his animation at its end, the idle
};
struct Game {
	double length;      // of the push animation (s)
	double freeFrom;    // the scripts look for a push from this time on (s); less than 0: never
	int script;
	bool loops;         // the animation starts over at its end by itself
	double cutAt;       // the first play is replaced by another animation when it is this far (a share); less than 0: never
	double speed;       // how fast the animation runs: 1 is its own speed
};
static const Game kGame = { 0.8, 0.0, SCRIPT_FLAT, false, -1.0, 1.0 };

struct PushLog {
	double speed;               // at the end
	double most;                // the fastest it ever was
	int pushes;                 // feet that have pushed
	int plays;                  // plays of the push animation
	double took[16];            // when each play started
	double added[16];           // what was added during each play
	double asked[16];           // what the push that went with it was asked to add
	double from[16];            // and the speed it started at
	double held[16];            // and how long the button had been down
	double said[16];            // and what it says it added
	double firstAt[16];         // when speed was first added in each play, ...
	double lastAt[16];          // ... and last (the end of that frame)
	double firstShare[16];      // how far into the animation that was
	double lastShare[16];
	double biggest[16];         // the most one frame added in each play
	double firstAdd[16];        // what the first frame added, and the last
	double lastAdd[16];
	double worst;               // the furthest the speed ever was from where the foot said, in units/s
	double asking;              // for how long the game was asked to play a push
	double hiding;              // for how long the scripts were kept from seeing Left and Right
	int phase;                  // at the end
};

static PushLog runPush(double rate, double seconds, double speed, const Press* presses, int count, int percent, const PushProfile& profile, int coast, const Game& game) {
	PushRun run;
	PushLog log;
	memset(&log, 0, sizeof(log));
	for (int i = 0; i < 16; i++) {
		log.firstAt[i] = -1.0;
		log.lastAt[i] = -1.0;
	}
	log.most = speed;
	const double dt = 1.0 / rate;
	PushAnim anim = { false, false, 0, 0.0f, (float)game.length };
	double clock = 0.0;         // into the animation
	bool waiting = false;       // the flat loop is waiting for the animation to end
	bool cut = false;
	uint32_t serial = run.Serial();
	const int frames = (int)(seconds * rate + 0.5);
	for (int frame = 0; frame < frames; frame++) {
		const double t = frame * dt;
		// the game's scripts, before the physics
		bool start = false;
		if (game.cutAt >= 0.0 && !cut && anim.current && anim.running && clock / game.length >= game.cutAt) {
			// something else takes the skater's animation
			cut = true;
			anim.current = false;
			anim.running = false;
			anim.play++;
			waiting = false;
		}
		else if (game.freeFrom >= 0.0 && t >= game.freeFrom - 1e-9) {
			if (game.script == SCRIPT_FLAT) {
				if (waiting) {
					if (!anim.running) {
						waiting = false;
						anim.current = false;   // the idle
						anim.play++;
					}
				}
				else if (run.Animating()) {
					start = true;
					waiting = true;
				}
			}
			else if (game.script == SCRIPT_NOLLIE) {
				if (run.Animating() && !run.AnimCurrent()) {
					start = !anim.current;
				}
				else if (anim.current && !anim.running) {
					anim.current = false;
					anim.play++;
				}
			}
			else {
				if (run.Animating() && !anim.current) {
					start = true;
				}
				else if (anim.current && !anim.running) {
					anim.current = false;
					anim.play++;
				}
			}
		}
		if (start) {
			anim.current = true;
			anim.running = true;
			anim.play++;
			clock = 0.0;
			if (log.plays < 16) log.took[log.plays] = t;
			log.plays++;
		}
		anim.time = (float)clock;
		const int play = log.plays > 0 && log.plays <= 16 ? log.plays - 1 : -1;
		const double share = clock / game.length;

		// the physics
		bool held = false;
		for (int i = 0; i < count; i++) {
			if (t >= presses[i].down - 1e-9 && t < presses[i].up - 1e-9) held = true;
		}
		const float add = run.Step((float)dt, (float)speed, held, percent, anim, profile);
		CHECK(add >= 0.0f);
		CHECK(add == add);
		speed += add;
		if (add > 0.0f) {
			CHECK(play >= 0);   // no speed before the game has played a push
			if (play >= 0) {
				if (log.firstAt[play] < 0.0) {
					log.firstAt[play] = t;
					log.firstShare[play] = share;
					log.firstAdd[play] = add;
				}
				log.lastAt[play] = t + dt;
				log.lastShare[play] = share;
				log.lastAdd[play] = add;
				log.added[play] += add;
				if (add > log.biggest[play]) log.biggest[play] = add;
			}
		}
		if (run.Phase() == PUSH_CONTACT && anim.current) {
			const double off = fabs((speed - run.From()) - run.Asked() * PushDone(profile, (float)share));
			if (off > log.worst) log.worst = off;
		}
		if (coast > 0) speed = Coast((float)speed, (float)dt, coast);
		if (speed > log.most) log.most = speed;
		if (run.Serial() != serial) {
			CHECK_EQ((int)(run.Serial() - serial), 1);
			serial = run.Serial();
			if (play >= 0) {
				log.asked[play] = run.Asked();
				log.from[play] = run.From();
				log.held[play] = run.HeldFor();
				log.said[play] = run.Added();
			}
			log.pushes++;
		}
		if (run.Animating()) log.asking += dt;
		if (run.HideTurn()) {
			CHECK(run.Animating());
			log.hiding += dt;
		}
		// a push that has an animation is that animation's
		if (run.Phase() >= PUSH_INTO) {
			CHECK(anim.current);
			CHECK_EQ((int)run.Play(), (int)anim.play);
			CHECK(!run.Animating());
		}
		CHECK_EQ(run.AnimCurrent() ? 1 : 0, anim.current ? 1 : 0);

		// the game's animation, after the physics
		if (anim.current && anim.running) {
			clock += dt * game.speed;
			if (clock >= game.length - 1e-9) {
				if (game.loops) {
					clock -= game.length;
					anim.play++;
					if (log.plays < 16) log.took[log.plays] = t + dt;
					log.plays++;
				}
				else {
					clock = game.length;
					anim.running = false;
				}
			}
		}
	}
	log.speed = speed;
	log.phase = run.Phase();
	return log;
}

// The speed a hold of so many seconds asks for (units/s), as Skate's curve has it: from a
// standstill the full hold is 0.2268 s, from walking pace on a quarter of a second
static double askedFor(double hold, bool standing) {
	double v = at(kSkatePush, hold / (standing ? 0.226785704 : 0.25));
	if (v > 4.0) v = 4.0;
	return v * 80.0;
}

static double smooth(double t) {
	if (t <= 0.0) return 0.0;
	if (t >= 1.0) return 1.0;
	return t * t * (3.0 - 2.0 * t);
}

// A push animation to watch the feet in: where its two feet are (the ball of each) at a share
// of it. The board is 4.5 high and the ball of a foot 1.2 above its sole.
struct Clip {
	double length;      // s
	double off;         // the share at which the foot leaves the board, ...
	double down;        // ... is on the ground,
	double go;          // ... starts back along it,
	double stop;        // ... has travelled,
	double up;          // ... leaves the ground,
	double on;          // ... and is on the board again
	double ahead;       // how far ahead of its place on the board it comes down (units)
	double behind;      // how far behind that place it stops
	double start;       // where on that way it comes down: 0 at the point ahead, more: further back (it reaches ahead on the ground first)
	bool leftPushes;
	bool sideways;      // the board points along the first axis instead of the third
	double blend;       // s over which the game blends into the animation from the skater standing on his board
	double noise;       // units of jitter on every number
};
static const Clip kClip = { 0.8, 0.12, 0.30, 0.30, 0.60, 0.66, 0.88, 14.0, 14.0, 0.0, false, false, 0.0, 0.0 };

static double jitter(int& seed) {
	seed = seed * 1103515245 + 12345;
	return ((seed >> 16) & 0x7FFF) / 16383.5 - 1.0;
}

static void clipFeet(const Clip& c, double share, float* left, float* right, int* seed) {
	const double deck = 5.7, ground = 1.2;
	const double rest[3] = { 3.0, deck, -9.0 };
	const double land[3] = { 9.0, ground, -9.0 + c.ahead - c.start };
	const double front[3] = { 9.0, ground, -9.0 + c.ahead };
	const double end[3] = { 9.0, ground, -9.0 - c.behind };
	double p[3];
	if (share <= c.off) {
		for (int i = 0; i < 3; i++) p[i] = rest[i];
	}
	else if (share < c.down) {
		const double u = smooth((share - c.off) / (c.down - c.off));
		for (int i = 0; i < 3; i++) p[i] = rest[i] + (land[i] - rest[i]) * u;
	}
	else if (share < c.go) {
		// on the ground, before the stroke: it stays, or reaches ahead
		const double u = smooth((share - c.down) / (c.go - c.down));
		for (int i = 0; i < 3; i++) p[i] = land[i] + (front[i] - land[i]) * u;
	}
	else if (share < c.stop) {
		const double u = smooth((share - c.go) / (c.stop - c.go));
		for (int i = 0; i < 3; i++) p[i] = front[i] + (end[i] - front[i]) * u;
	}
	else if (share < c.up) {
		for (int i = 0; i < 3; i++) p[i] = end[i];
	}
	else if (share < c.on) {
		const double u = (share - c.up) / (c.on - c.up);
		for (int i = 0; i < 3; i++) p[i] = end[i] + (rest[i] - end[i]) * smooth(u);
		p[1] += 3.0 * sin(3.14159265358979 * u);
	}
	else {
		for (int i = 0; i < 3; i++) p[i] = rest[i];
	}
	// the game blends into the animation from the skater on his board
	if (c.blend > 0.0) {
		const double w = share * c.length / c.blend;
		if (w < 1.0) {
			for (int i = 0; i < 3; i++) p[i] = rest[i] + (p[i] - rest[i]) * w;
		}
	}
	// the other foot stays where it is and shifts a little with the weight
	double q[3] = { -3.0, deck + 0.15 * sin(6.2831853 * share), 9.0 };
	if (seed && c.noise > 0.0) {
		for (int i = 0; i < 3; i++) {
			p[i] += c.noise * jitter(*seed);
			q[i] += c.noise * jitter(*seed);
		}
	}
	if (c.sideways) {
		double s = p[0]; p[0] = p[2]; p[2] = s;
		s = q[0]; q[0] = q[2]; q[2] = s;
	}
	float* pushing = c.leftPushes ? left : right;
	float* standing = c.leftPushes ? right : left;
	const double mirror = c.leftPushes ? -1.0 : 1.0;
	for (int i = 0; i < 3; i++) {
		pushing[i] = (float)(i == 0 && !c.sideways ? mirror * p[i] : p[i]);
		standing[i] = (float)(i == 0 && !c.sideways ? mirror * q[i] : q[i]);
	}
}

// The feet of one play of a clip, looked at so many times a second from the first share to the last
static FootResult watchClip(const Clip& c, double fps, double firstShare, double lastShare, int* looks) {
	FootWatch watch;
	int seed = 7;
	int n = 0;
	for (int k = 0;; k++) {
		const double share = k / fps / c.length;
		if (share > lastShare + 1e-9 || share > 1.0 - 1e-9) break;     // the game never shows the push at its very end
		if (share < firstShare - 1e-9) continue;
		float left[3], right[3];
		clipFeet(c, share, left, right, &seed);
		watch.Sample((float)share, left, right);
		n++;
	}
	if (looks) *looks = n;
	FootResult result;
	watch.End((float)c.length, result);
	return result;
}

static void testPushing() {
	const double unit = 80.0;
	const double frame60 = 1.0 / 60.0;
	const float f60 = (float)frame60;
	const float nan = nanf("");
	// the foot pushes from 30% to 62% of the game's push animation, which is 0.8 s long
	PushProfile foot;
	SmoothPushProfile(foot, 0.30f, 0.62f);

	g_case = "rolling: the settings and their defaults";
	{
		Settings s;
		SetDefaults(s);
		CHECK_EQ(s.push, 100);
		CHECK_EQ(s.pushOnOllie, 1);
		CHECK_EQ(s.pushOverTurn, 1);
		CHECK_EQ(s.pushMeasure, 1);
		CHECK_EQ(s.pushFootDown, 30);
		CHECK_EQ(s.pushFootUp, 62);
		CHECK_EQ(s.coast, 100);
		CHECK_EQ(s.steering, 100);
		Sanitize(s);
		CHECK_EQ(s.push, 100);
		CHECK_EQ(s.pushMeasure, 1);
		CHECK_EQ(s.pushFootDown, 30);
		CHECK_EQ(s.pushFootUp, 62);
		CHECK_EQ(s.coast, 100);
		CHECK_EQ(s.steering, 100);
		// 0 is THUG's own; anything else stays within reason
		s.push = 0; s.steering = 0; s.coast = 0;
		Sanitize(s);
		CHECK_EQ(s.push, 0);
		CHECK_EQ(s.steering, 0);
		CHECK_EQ(s.coast, 0);
		s.push = -3; s.steering = -3; s.coast = -3;
		Sanitize(s);
		CHECK_EQ(s.push, 0);
		CHECK_EQ(s.steering, 0);
		CHECK_EQ(s.coast, 0);
		s.push = 1; s.steering = 1; s.coast = 1;
		Sanitize(s);
		CHECK_EQ(s.push, 25);
		CHECK_EQ(s.steering, 25);
		CHECK_EQ(s.coast, 1);
		s.push = 100000; s.steering = 100000; s.coast = 100000;
		Sanitize(s);
		CHECK_EQ(s.push, 400);
		CHECK_EQ(s.steering, 400);
		CHECK_EQ(kRollMostPercent, 400);
		CHECK_EQ(s.coast, 1000);
		s.pushOnOllie = 7;
		s.pushOverTurn = -2;
		s.pushMeasure = 5;
		s.pushFootDown = -5; s.pushFootUp = 1;
		Sanitize(s);
		CHECK_EQ(s.pushOnOllie, 1);
		CHECK_EQ(s.pushOverTurn, 1);
		CHECK_EQ(s.pushMeasure, 1);
		CHECK_EQ(s.pushFootDown, 0);
		CHECK_EQ(s.pushFootUp, 5);
		s.pushOnOllie = 0;
		s.pushOverTurn = 0;
		s.pushMeasure = 0;
		s.pushFootDown = 99999; s.pushFootUp = 99999;
		Sanitize(s);
		CHECK_EQ(s.pushOnOllie, 0);
		CHECK_EQ(s.pushOverTurn, 0);
		CHECK_EQ(s.pushMeasure, 0);
		CHECK_EQ(s.pushFootDown, 90);
		CHECK_EQ(s.pushFootUp, 100);
		// the foot has pushed after it has started to, by a twentieth of the animation at least
		s.pushFootDown = 40; s.pushFootUp = 20;
		Sanitize(s);
		CHECK_EQ(s.pushFootDown, 40);
		CHECK_EQ(s.pushFootUp, 45);
		s.pushFootDown = 90; s.pushFootUp = 0;
		Sanitize(s);
		CHECK_EQ(s.pushFootDown, 90);
		CHECK_EQ(s.pushFootUp, 95);
		s.pushFootDown = 10; s.pushFootUp = 80;
		Sanitize(s);
		CHECK_EQ(s.pushFootDown, 10);
		CHECK_EQ(s.pushFootUp, 80);
	}

	g_case = "rolling: what a push is asked to add";
	{
		// Skate: 0.75 m/s for a tap, 4 m/s from a quarter of a second on, never more
		CHECK(near(kPushTap, 0.75, 1e-6));
		CHECK(near(kPushMost, 4.0, 1e-6));
		CHECK(near(kPushFullHold, 0.25, 1e-6));
		CHECK(near(kPushTopSpeed, 8.5, 1e-6));
		CHECK(near(kPushLastSpeed, 8.3, 1e-6));
		CHECK(near(PushStrength(0.0f, 3.0f), 0.75, 1e-4));
		CHECK(near(PushStrength(-1.0f, 3.0f), 0.75, 1e-4));
		CHECK(near(PushStrength(0.25f, 3.0f), 4.0, 1e-4));
		CHECK(near(PushStrength(5.0f, 3.0f), 4.0, 1e-4));
		// in between as Skate's curve has it, over the share of the quarter second
		for (int i = 0; i <= 250; i++) {
			const double hold = i * 0.001;
			double want = at(kSkatePush, hold / 0.25);
			if (want > 4.0) want = 4.0;
			CHECK(near(PushStrength((float)hold, 3.0f), want, 0.002));
			CHECK(near(PushStrength((float)hold, 8.0f), want, 0.002));
		}
		// a held button never asks for less than a shorter hold did
		float before = 0.0f;
		for (int i = 0; i <= 400; i++) {
			const float asked = PushStrength(i * 0.001f, 2.0f);
			CHECK(asked >= before);
			CHECK(asked >= 0.75f && asked <= 4.0f);
			before = asked;
		}
		// some of the way: a tenth of a second is 1.9 m/s, 0.15 s is 2.7, 0.2 s is 3.1
		CHECK(near(PushStrength(0.1f, 3.0f), 1.885, 0.01));
		CHECK(near(PushStrength(0.15f, 3.0f), 2.674, 0.01));
		CHECK(near(PushStrength(0.2f, 3.0f), 3.120, 0.01));
		// from a standstill the full push takes a little less of a hold: 0.227 s
		CHECK(near(PushStrength(0.2268f, 0.0f), 4.0, 0.01));
		CHECK(PushStrength(0.2268f, 3.0f) < 3.9f);
		CHECK(PushStrength(0.1f, 0.0f) > PushStrength(0.1f, 3.0f));
		// the most one tick adds: 0.6 m/s at a standstill, 0.5 at the top speed
		CHECK(near(PushStep(0.0f), 0.6, 1e-6));
		CHECK(near(PushStep(8.5f), 0.5, 1e-6));
		CHECK(near(PushStep(4.25f), 0.55, 1e-6));
		CHECK(near(PushStep(-3.0f), 0.6, 1e-6));
		CHECK(near(PushStep(30.0f), 0.5, 1e-6));
	}

	g_case = "rolling: where the foot pushes";
	{
		PushProfile p;
		SmoothPushProfile(p, 0.30f, 0.62f);
		CHECK(near(p.down, 0.30, 1e-6));
		CHECK(near(p.up, 0.62, 1e-6));
		CHECK_EQ((int)PUSH_KNOTS, 17);
		CHECK(p.done[0] == 0.0f);
		CHECK(p.done[PUSH_KNOTS - 1] == 1.0f);
		CHECK(near(p.done[8], 0.5, 1e-6));
		for (int k = 0; k < PUSH_KNOTS; k++) {
			if (k + 1 < PUSH_KNOTS) CHECK(p.done[k + 1] > p.done[k]);
			CHECK(near(p.done[k] + p.done[PUSH_KNOTS - 1 - k], 1.0, 1e-6));
			CHECK(near(p.done[k], smooth(k / 16.0), 1e-6));
		}
		// gentle at both ends: the first sixteenth of the time is a hundredth of the way, a middle one a tenth
		CHECK(p.done[1] - p.done[0] < 0.02);
		CHECK(p.done[9] - p.done[8] > 0.09);
		// nothing before it pushes, all of it after, and between as the points say
		CHECK(PushDone(p, -1.0f) == 0.0f);
		CHECK(PushDone(p, 0.0f) == 0.0f);
		CHECK(PushDone(p, 0.29f) == 0.0f);
		CHECK(PushDone(p, 0.30f) == 0.0f);
		CHECK(PushDone(p, 0.62f) == 1.0f);
		CHECK(PushDone(p, 0.63f) == 1.0f);
		CHECK(PushDone(p, 1.0f) == 1.0f);
		CHECK(PushDone(p, 5.0f) == 1.0f);
		CHECK(PushDone(p, nan) == 0.0f);
		CHECK(near(PushDone(p, 0.46f), 0.5, 1e-4));
		CHECK(near(PushDone(p, 0.38f), smooth(0.25), 1e-4));
		CHECK(near(PushDone(p, 0.54f), smooth(0.75), 1e-4));
		float before = 0.0f;
		for (int i = 0; i <= 1000; i++) {
			const double share = i * 0.001;
			const float d = PushDone(p, (float)share);
			CHECK(d >= before);
			CHECK(d >= 0.0f && d <= 1.0f);
			CHECK(near(d, smooth((share - 0.30) / 0.32), 0.004));
			before = d;
		}
		// what is asked for stays within reason: it starts in the first 95 hundredths and takes 2 at least
		SmoothPushProfile(p, -3.0f, 0.5f);
		CHECK(p.down == 0.0f);
		CHECK(near(p.up, 0.5, 1e-6));
		SmoothPushProfile(p, 2.0f, 3.0f);
		CHECK(near(p.down, 0.95, 1e-6));
		CHECK(p.up == 1.0f);
		SmoothPushProfile(p, 0.99f, 0.5f);
		CHECK(near(p.down, 0.95, 1e-6));
		CHECK(near(p.up, 0.97, 1e-6));
		SmoothPushProfile(p, 0.5f, 0.2f);
		CHECK(near(p.down, 0.5, 1e-6));
		CHECK(near(p.up, 0.52, 1e-6));
		SmoothPushProfile(p, 0.5f, 0.51f);
		CHECK(near(p.up, 0.52, 1e-6));
		SmoothPushProfile(p, 0.5f, 9.0f);
		CHECK(p.up == 1.0f);
		SmoothPushProfile(p, nan, nan);
		CHECK(p.down == 0.0f);
		CHECK(near(p.up, 0.02, 1e-6));
		CHECK(near(p.done[8], 0.5, 1e-6));
		// a foot that pushes in no time at all: nothing before, everything after
		PushProfile none;
		SmoothPushProfile(none, 0.4f, 0.5f);
		none.up = 0.4f;
		CHECK(PushDone(none, 0.39f) == 0.0f);
		CHECK(PushDone(none, 0.4f) == 0.0f);
		CHECK(PushDone(none, 0.401f) == 1.0f);
		// a foot of any other kind is read between its points just the same: this one waits for half the time
		PushProfile bent;
		SmoothPushProfile(bent, 0.2f, 0.6f);
		for (int k = 0; k < PUSH_KNOTS; k++) bent.done[k] = k < 8 ? 0.0f : (float)(k - 8) / 8.0f;
		CHECK(PushDone(bent, 0.3f) == 0.0f);
		CHECK(PushDone(bent, 0.4f) == 0.0f);
		CHECK(near(PushDone(bent, 0.45f), 0.25, 1e-5));
		CHECK(near(PushDone(bent, 0.5f), 0.5, 1e-5));
		CHECK(near(PushDone(bent, 0.55f), 0.75, 1e-5));
		CHECK(near(PushDone(bent, 0.5999f), 1.0, 0.002));
	}

	g_case = "rolling: one tap";
	{
		// A tap of 50 ms from a standstill. The game plays its push animation, and the speed
		// comes while its foot pushes, from 30% to 62% of it: nothing before, nothing after.
		// How much: what Skate's curve gives for 50 ms.
		const Press tap[] = { { 0.0, 0.05 } };
		const PushLog log = runPush(60.0, 3.0, 0.0, tap, 1, 100, foot, 0, kGame);
		CHECK_EQ(log.plays, 1);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.took[0], frame60, 1e-6));    // the script starts it on the frame after the press
		CHECK(near(log.speed, askedFor(0.05, true), 0.5));
		CHECK(near(log.speed, 81.5, 1.0));
		CHECK(near(log.added[0], log.speed, 1e-3));
		CHECK(near(log.asked[0], log.speed, 1e-3));
		CHECK(near(log.held[0], 0.05, 1e-4));
		CHECK(near(log.from[0], 0.0, 1e-6));
		CHECK(log.firstShare[0] > 0.30 && log.firstShare[0] <= 0.30 + frame60 / 0.8 + 1e-6);
		CHECK(log.lastShare[0] >= 0.62 - 1e-6 && log.lastShare[0] < 0.62 + frame60 / 0.8);
		CHECK(near(log.firstAt[0], log.took[0] + 0.30 * 0.8, frame60 + 1e-6));
		CHECK(near(log.lastAt[0], log.took[0] + 0.62 * 0.8, 2.0 * frame60));
		CHECK_EQ(log.phase, PUSH_NONE);
		CHECK(near(log.asking, frame60, 1e-6));     // the game was asked for one frame
		// the shortest tap there is, one frame: Skate's tap
		const Press blip[] = { { 0.0, 1.0 / 60.0 } };
		const PushLog least = runPush(60.0, 3.0, 300.0, blip, 1, 100, foot, 0, kGame);
		CHECK_EQ(least.pushes, 1);
		CHECK(least.speed - 300.0 >= 0.75 * unit - 0.01);
		CHECK(least.speed - 300.0 < 0.9 * unit);
	}

	g_case = "rolling: the speed comes as the foot pushes";
	{
		const Press hold[] = { { 0.0, 0.5 } };
		const PushLog log = runPush(60.0, 3.0, 0.0, hold, 1, 100, foot, 0, kGame);
		CHECK_EQ(log.pushes, 1);
		CHECK_EQ(log.plays, 1);
		CHECK(near(log.added[0], 4.0 * unit, 0.01));
		// on every frame the foot pushes the board has as much of the push as the foot has of its stroke
		CHECK(log.worst < 0.01);
		// so it comes gently: the first frame and the last add little, the middle ones the most,
		// and that is one and a half times what an even share would be: 320 units/s over 0.256 s
		CHECK(log.firstAdd[0] < 0.25 * log.biggest[0]);
		CHECK(log.lastAdd[0] < 0.25 * log.biggest[0]);
		CHECK(log.biggest[0] <= 1.5 * 320.0 * frame60 / 0.256 + 0.01);
		CHECK(log.biggest[0] > 1.4 * 320.0 * frame60 / 0.256);
		// frame by frame, with the button down and the animation running
		PushRun run;
		PushAnim anim = { true, true, 1, 0.0f, 0.8f };
		double speed = 100.0;
		for (int k = 0; k <= 48; k++) {
			anim.time = (float)(k * frame60);
			speed += run.Step(f60, (float)speed, true, 100, anim, foot);
			const double share = k * frame60 / 0.8;
			CHECK(near(speed, 100.0 + 320.0 * smooth((share - 0.30) / 0.32), 320.0 * 0.004 + 0.01));
			CHECK_EQ(run.Phase(), share < 0.30 ? PUSH_INTO : (share < 0.62 ? PUSH_CONTACT : PUSH_RECOVER));
			CHECK_EQ(run.Active() ? 1 : 0, share < 0.62 ? 1 : 0);
			CHECK_EQ((int)run.Serial(), share < 0.62 ? 0 : 1);
		}
		CHECK(near(speed, 420.0, 0.01));
		CHECK(near(run.Added(), 320.0, 0.01));
		CHECK(near(run.Asked(), 320.0, 0.01));
		CHECK(near(run.From(), 100.0, 1e-6));
	}

	g_case = "rolling: at any frame rate";
	{
		const Press hold[] = { { 0.0, 0.5 } };
		const double rates[] = { 30.0, 50.0, 60.0, 75.0, 120.0, 144.0, 240.0 };
		for (int i = 0; i < 7; i++) {
			const PushLog log = runPush(rates[i], 3.0, 100.0, hold, 1, 100, foot, 0, kGame);
			const double step = 1.0 / rates[i] / 0.8;
			CHECK_EQ(log.plays, 1);
			CHECK_EQ(log.pushes, 1);
			CHECK(near(log.added[0], 320.0, 0.05));
			CHECK(near(log.speed, 420.0, 0.05));
			CHECK(log.worst < 0.05);
			CHECK(log.firstShare[0] > 0.30 && log.firstShare[0] <= 0.30 + step + 1e-6);
			CHECK(log.lastShare[0] >= 0.62 - 1e-6 && log.lastShare[0] < 0.62 + step + 1e-6);
		}
	}

	g_case = "rolling: the animation is the clock";
	{
		// Whatever the length of the game's animation: the speed comes from 30% to 62% of it
		const Press hold[] = { { 0.0, 0.6 } };
		const double lengths[] = { 0.6, 0.8, 1.2, 1.6 };
		for (int i = 0; i < 4; i++) {
			Game g = kGame;
			g.length = lengths[i];
			const PushLog log = runPush(60.0, 4.0, 100.0, hold, 1, 100, foot, 0, g);
			CHECK_EQ(log.plays, 1);
			CHECK_EQ(log.pushes, 1);
			CHECK(near(log.firstAt[0], log.took[0] + 0.30 * lengths[i], frame60 + 1e-6));
			CHECK(near(log.lastAt[0], log.took[0] + 0.62 * lengths[i], 2.0 * frame60 + 1e-6));
			CHECK(near(log.added[0], 320.0, 0.05));
		}
		// And whatever speed the game plays it at: at twice its own the foot pushes from 0.12 s
		// to 0.248 s after the start, at half from 0.48 s to 0.992 s
		Game fast = kGame;
		fast.speed = 2.0;
		const Press shorter[] = { { 0.0, 0.3 } };
		PushLog log = runPush(60.0, 4.0, 100.0, shorter, 1, 100, foot, 0, fast);
		CHECK_EQ(log.plays, 1);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.firstAt[0], log.took[0] + 0.12, frame60 + 1e-6));
		CHECK(near(log.lastAt[0], log.took[0] + 0.248, 2.0 * frame60 + 1e-6));
		CHECK(near(log.asked[0], 320.0, 0.01));
		// (a stroke of an eighth of a second is too short for all of 4 m/s: Skate lets a push
		// come in only so fast. See below.)
		CHECK(log.added[0] > 0.85 * 320.0 && log.added[0] <= 320.01);
		CHECK(log.biggest[0] <= 0.6 * unit + 0.01);
		Game slow = kGame;
		slow.speed = 0.5;
		log = runPush(60.0, 4.0, 100.0, hold, 1, 100, foot, 0, slow);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.firstAt[0], log.took[0] + 0.48, frame60 + 1e-6));
		CHECK(near(log.lastAt[0], log.took[0] + 0.992, 2.0 * frame60 + 1e-6));
		CHECK(near(log.added[0], 320.0, 0.05));
	}

	g_case = "rolling: the button held";
	{
		// Held from a standstill: every push has all there is, 4 m/s, until the top speed is
		// reached. 320, 640, 680, and then no more: from 8.3 m/s on no push starts, and the
		// game is not asked for an animation that would push nothing.
		const Press hold[] = { { 0.0, 30.0 } };
		const PushLog log = runPush(60.0, 12.0, 0.0, hold, 1, 100, foot, 0, kGame);
		CHECK_EQ(log.pushes, 3);
		CHECK_EQ(log.plays, 3);
		CHECK(near(log.added[0], 4.0 * unit, 0.05));
		CHECK(near(log.added[1], 4.0 * unit, 0.05));
		CHECK(near(log.added[2], 8.5 * unit - 8.0 * unit, 0.05));
		CHECK(near(log.asked[2], 4.0 * unit, 0.5));
		CHECK(near(log.speed, 8.5 * unit, 0.01));
		CHECK(log.most <= 8.5 * unit + 0.01);
		// one push for every play of the animation, at the same point of each
		for (int i = 0; i < 3; i++) {
			CHECK(near(log.firstAt[i], log.took[i] + 0.30 * 0.8, frame60 + 1e-6));
			CHECK(near(log.lastAt[i], log.took[i] + 0.62 * 0.8, 2.0 * frame60));
			CHECK(near(log.said[i], log.added[i], 1e-3));
		}
		// the animation ends, the game shows the idle for a frame, and starts the next push
		CHECK(near(log.took[1] - log.took[0], 0.8 + frame60, 1e-6));
		CHECK(near(log.took[2] - log.took[1], 0.8 + frame60, 1e-6));
		CHECK_EQ(log.phase, PUSH_NONE);
		CHECK(near(log.asking, 3.0 * frame60, 1e-6));   // asked three times, for a frame each
	}

	g_case = "rolling: no animation, no push";
	{
		// The game's scripts do not get to play the push: they are busy with a landing, say. A
		// button that stays down keeps asking, and nothing is added.
		Game never = kGame;
		never.freeFrom = -1.0;
		const Press hold[] = { { 0.0, 30.0 } };
		PushLog log = runPush(60.0, 5.0, 200.0, hold, 1, 100, foot, 0, never);
		CHECK_EQ(log.plays, 0);
		CHECK_EQ(log.pushes, 0);
		CHECK(near(log.speed, 200.0, 1e-6));
		CHECK(near(log.asking, 5.0, 1e-6));
		CHECK_EQ(log.phase, PUSH_ASKED);
		// a tap gives up after three quarters of a second
		const Press tap[] = { { 0.0, 0.05 } };
		log = runPush(60.0, 5.0, 200.0, tap, 1, 100, foot, 0, never);
		CHECK_EQ(log.pushes, 0);
		CHECK(near(log.speed, 200.0, 1e-6));
		CHECK(near(log.asking, 0.75, frame60 + 1e-6));
		CHECK_EQ(log.phase, PUSH_NONE);
		// When the scripts get to it in time the push is there, late but whole: the animation and the speed together
		Game late = kGame;
		late.freeFrom = 0.5;
		log = runPush(60.0, 5.0, 200.0, tap, 1, 100, foot, 0, late);
		CHECK_EQ(log.plays, 1);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.took[0], 0.5, 1e-6));
		CHECK(near(log.firstAt[0], log.took[0] + 0.24, frame60 + 1e-6));
		CHECK(near(log.added[0], askedFor(0.05, false), 0.5));
		late.freeFrom = 0.7;
		log = runPush(60.0, 5.0, 200.0, tap, 1, 100, foot, 0, late);
		CHECK_EQ(log.pushes, 1);
		// and when they do not, it is not: nothing happens a second after a tap
		late.freeFrom = 0.8;
		log = runPush(60.0, 5.0, 200.0, tap, 1, 100, foot, 0, late);
		CHECK_EQ(log.plays, 0);
		CHECK_EQ(log.pushes, 0);
		CHECK(near(log.speed, 200.0, 1e-6));
		// a button that is still down gets its push whenever the scripts get to it
		late.freeFrom = 3.0;
		log = runPush(60.0, 4.2, 200.0, hold, 1, 100, foot, 0, late);
		CHECK_EQ(log.plays, 2);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.took[0], 3.0, 1e-6));
		CHECK(near(log.added[0], 320.0, 0.05));
		CHECK(near(log.asking, 3.0 + frame60, 1e-6));
	}

	g_case = "rolling: a tap while the foot is busy";
	{
		// A second tap during the animation is kept: the next push follows the first, with an animation of its own
		const Press two[] = { { 0.0, 0.05 }, { 0.4, 0.45 } };
		PushLog log = runPush(60.0, 4.0, 0.0, two, 2, 100, foot, 0, kGame);
		CHECK_EQ(log.plays, 2);
		CHECK_EQ(log.pushes, 2);
		CHECK(near(log.took[1] - log.took[0], 0.8 + frame60, 1e-6));
		CHECK(near(log.added[0], askedFor(0.05, true), 0.5));
		CHECK(near(log.added[1], askedFor(0.05, false), 0.5));
		CHECK(near(log.speed, log.added[0] + log.added[1], 1e-3));
		// one is kept, not three
		const Press four[] = { { 0.0, 0.05 }, { 0.2, 0.25 }, { 0.4, 0.45 }, { 0.6, 0.65 } };
		log = runPush(60.0, 5.0, 0.0, four, 4, 100, foot, 0, kGame);
		CHECK_EQ(log.plays, 2);
		CHECK_EQ(log.pushes, 2);
		// The push that was kept is as strong as its own tap was long, whatever the button
		// does while it waits and after its animation has started
		const Press three[] = { { 0.0, 0.05 }, { 0.4, 0.5 }, { 0.9, 1.9 } };
		log = runPush(60.0, 1.75, 0.0, three, 3, 100, foot, 0, kGame);
		CHECK_EQ(log.pushes, 2);
		CHECK(near(log.held[1], 0.1, 1e-4));
		CHECK(near(log.added[1], askedFor(0.1, false), 0.5));
		// A press that comes before the foot of the first push is down does not make that push
		// stronger. It is the next push, and that one has all a held button gives.
		const Press quick[] = { { 0.0, 0.05 }, { 0.1, 2.0 } };
		log = runPush(60.0, 1.9, 0.0, quick, 2, 100, foot, 0, kGame);
		CHECK_EQ(log.pushes, 2);
		CHECK(near(log.added[0], askedFor(0.05, true), 0.5));
		CHECK(near(log.held[0], 0.05, 1e-4));
		CHECK(near(log.added[1], 320.0, 0.05));
	}

	g_case = "rolling: how long the button was down";
	{
		// held for a tenth of a second and let go before the foot is down: a tenth of a second's push
		const Press brief[] = { { 0.0, 0.1 } };
		PushLog log = runPush(60.0, 3.0, 0.0, brief, 1, 100, foot, 0, kGame);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.held[0], 0.1, 1e-4));
		CHECK(near(log.added[0], askedFor(0.1, true), 0.5));
		const Press medium[] = { { 0.0, 0.15 } };
		log = runPush(60.0, 3.0, 300.0, medium, 1, 100, foot, 0, kGame);
		CHECK(near(log.held[0], 0.15, 1e-4));
		CHECK(near(log.added[0], askedFor(0.15, false), 0.5));
		CHECK(log.added[0] > 200.0 && log.added[0] < 230.0);
		// Let go while the foot pushes: what it had been down for by then. The foot starts
		// 0.27 s after the press here, so that is the whole push.
		const Press into[] = { { 0.0, 0.35 } };
		log = runPush(60.0, 3.0, 300.0, into, 1, 100, foot, 0, kGame);
		CHECK_EQ(log.pushes, 1);
		CHECK_EQ(log.plays, 1);
		CHECK(near(log.held[0], 0.35, 1e-4));
		CHECK(near(log.added[0], 320.0, 0.05));
		// Still down when the foot has pushed: what comes after does not count for this push
		const Press longer[] = { { 0.0, 0.7 } };
		log = runPush(60.0, 3.0, 300.0, longer, 1, 100, foot, 0, kGame);
		CHECK_EQ(log.pushes, 1);
		CHECK_EQ(log.plays, 1);
		CHECK(near(log.held[0], 32.0 * frame60, 1e-4));     // the press, a frame for the script, and 0.5 s of animation
		CHECK(near(log.added[0], 320.0, 0.05));
		// A foot that starts to push before the button has been down for a quarter of a second
		// (the animation at twice its speed: 0.15 s after the press): a button that stays down
		// counts for more as the foot pushes on, and by the end of the stroke it is the whole push
		Game fast = kGame;
		fast.speed = 2.0;
		const Press held[] = { { 0.0, 0.3 } };
		log = runPush(60.0, 3.0, 300.0, held, 1, 100, foot, 0, fast);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.from[0], 300.0, 1e-6));
		CHECK(near(log.asked[0], 320.0, 0.01));
		// (as much of it as fits into a stroke that short: Skate lets a push come in only so fast)
		CHECK(log.added[0] > 0.85 * 320.0 && log.added[0] <= 320.01);
		// let go in the middle of that stroke, 0.2 s after the press: a fifth of a second's push
		const Press partly[] = { { 0.0, 0.2 } };
		log = runPush(60.0, 3.0, 300.0, partly, 1, 100, foot, 0, fast);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.held[0], 0.2, 1e-4));
		CHECK(near(log.asked[0], askedFor(0.2, false), 0.5));
		CHECK(log.added[0] > 0.9 * log.asked[0] && log.added[0] <= log.asked[0] + 0.01);
		// and pressed again after that: the first push stays what it was
		const Press again[] = { { 0.0, 0.2 }, { 0.22, 0.4 } };
		log = runPush(60.0, 0.45, 300.0, again, 2, 100, foot, 0, fast);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.held[0], 0.2, 1e-4));
		CHECK(near(log.asked[0], askedFor(0.2, false), 0.5));
		// the push never gets weaker while the foot pushes, and the board never loses what it has
		PushRun run;
		PushAnim anim = { true, true, 1, 0.0f, 0.4f };
		double speed = 300.0;
		float asked = 0.0f;
		for (int k = 0; k <= 24; k++) {
			anim.time = (float)(k * frame60);
			const float add = run.Step(f60, (float)speed, k < 12, 100, anim, foot);
			CHECK(add >= 0.0f);
			speed += add;
			if (run.Phase() == PUSH_CONTACT) {
				CHECK(run.Asked() >= asked);
				asked = run.Asked();
			}
		}
		CHECK(near(asked, askedFor(0.2, false), 0.5));
		CHECK(speed > 300.0 + 0.9 * asked && speed <= 300.0 + asked + 0.01);
	}

	g_case = "rolling: the animation is cut short";
	{
		// Something else takes the skater's animation before the foot is down: no push. The
		// button is still down, so the game is asked again, and the next animation pushes.
		Game early = kGame;
		early.cutAt = 0.2;
		const Press hold[] = { { 0.0, 30.0 } };
		PushLog log = runPush(60.0, 1.2, 100.0, hold, 1, 100, foot, 0, early);
		CHECK_EQ(log.plays, 3);
		CHECK_EQ(log.pushes, 1);
		CHECK(log.added[0] == 0.0);
		CHECK(log.firstAt[0] < 0.0);
		CHECK(near(log.added[1], 320.0, 0.05));
		CHECK(log.added[2] == 0.0);
		// In the middle of the stroke: the board has what the foot had done by then, and that is all
		Game middle = kGame;
		middle.cutAt = 0.46;
		const Press once[] = { { 0.0, 0.35 } };
		log = runPush(60.0, 3.0, 100.0, once, 1, 100, foot, 0, middle);
		CHECK_EQ(log.plays, 1);
		CHECK_EQ(log.pushes, 1);
		CHECK(log.lastShare[0] < 0.46);
		CHECK(near(log.added[0], 320.0 * smooth((log.lastShare[0] - 0.30) / 0.32), 320.0 * 0.004 + 0.01));
		CHECK(log.added[0] > 0.4 * 320.0 && log.added[0] < 0.5 * 320.0);
		CHECK(near(log.speed, 100.0 + log.added[0], 1e-3));
		CHECK_EQ(log.phase, PUSH_NONE);
		// after the stroke it makes no difference
		Game after = kGame;
		after.cutAt = 0.7;
		log = runPush(60.0, 3.0, 100.0, once, 1, 100, foot, 0, after);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.added[0], 320.0, 0.05));
	}

	g_case = "rolling: the animation ends between two frames";
	{
		// A foot that pushes to the very end of its animation: the game has put the next
		// animation in its place before the last of the stroke was seen. The stroke is
		// finished all the same.
		PushProfile late;
		SmoothPushProfile(late, 0.60f, 1.0f);
		const Press once[] = { { 0.0, 0.5 } };
		const double rates[] = { 30.0, 60.0, 144.0 };
		for (int i = 0; i < 3; i++) {
			const PushLog log = runPush(rates[i], 3.0, 100.0, once, 1, 100, late, 0, kGame);
			CHECK_EQ(log.pushes, 1);
			CHECK(near(log.added[0], 320.0, 0.05));
		}
		// frame by frame: the last the push sees of the animation is 98% of it, where the foot
		// has a little of its way left. On the next frame the skater is in his idle, and the
		// board gets the rest.
		PushRun run;
		PushAnim anim = { true, true, 1, 0.0f, 0.8f };
		double speed = 100.0;
		for (int k = 0; k <= 47; k++) {
			anim.time = (float)(k * frame60);
			speed += run.Step(f60, (float)speed, k < 30, 100, anim, late);
		}
		CHECK_EQ(run.Phase(), PUSH_CONTACT);
		CHECK(speed < 419.0 && speed > 415.0);
		CHECK_EQ((int)run.Serial(), 0);
		PushAnim idle = { false, false, 2, 0.0f, 0.0f };
		const float rest = run.Step(f60, (float)speed, false, 100, idle, late);
		CHECK(rest > 1.0f);
		CHECK(near(speed + rest, 420.0, 0.01));
		CHECK_EQ(run.Phase(), PUSH_NONE);
		CHECK_EQ((int)run.Serial(), 1);
		CHECK(run.Step(f60, (float)speed + rest, false, 100, idle, late) == 0.0f);
		// "It ran to its end" is said of this animation only: how the last one went by is forgotten
		PushRun stale;
		PushAnim hitch = { true, true, 1, 0.0f, 0.8f };
		stale.Step(f60, 100.0f, true, 100, hitch, late);
		hitch.time = 0.24f;         // 30% of it gone on the next frame
		stale.Step(f60, 100.0f, true, 100, hitch, late);
		CHECK_EQ(stale.Phase(), PUSH_INTO);
		idle.play = 2;
		stale.Step(f60, 100.0f, true, 100, idle, late);     // and then it is cut: no push, the game is asked again
		CHECK_EQ(stale.Phase(), PUSH_ASKED);
		hitch.play = 3;
		hitch.time = 0.56f;         // the next one is first seen 70% in, in its stroke
		const float some = stale.Step(f60, 100.0f, true, 100, hitch, late);
		CHECK(some > 0.0f);
		CHECK_EQ(stale.Phase(), PUSH_CONTACT);
		idle.play = 4;              // cut again, nowhere near its end: nothing more
		CHECK(stale.Step(f60, 100.0f + some, true, 100, idle, late) == 0.0f);
		// the same with the animation started over (a loop) instead of replaced
		PushRun round;
		anim.play = 5;
		speed = 100.0;
		for (int k = 0; k <= 47; k++) {
			anim.time = (float)(k * frame60);
			speed += round.Step(f60, (float)speed, k < 30, 100, anim, late);
		}
		anim.play = 6;
		anim.time = 0.0f;
		speed += round.Step(f60, (float)speed, false, 100, anim, late);
		CHECK(near(speed, 420.0, 0.01));
		CHECK_EQ(round.Phase(), PUSH_NONE);
		// but one that was cut off well before its end is not: at 80% this foot has done a
		// little under half of its stroke
		Game cut = kGame;
		cut.cutAt = 0.8;
		const PushLog log = runPush(60.0, 3.0, 100.0, once, 1, 100, late, 0, cut);
		CHECK_EQ(log.pushes, 1);
		CHECK(log.added[0] > 0.3 * 320.0 && log.added[0] < 0.5 * 320.0);
	}

	g_case = "rolling: as fast as pushing gets";
	{
		const Press hold[] = { { 0.0, 30.0 } };
		// from 8.3 m/s on no push starts, and the game is not asked for one
		PushLog log = runPush(60.0, 3.0, 8.3 * unit, hold, 1, 100, foot, 0, kGame);
		CHECK_EQ(log.plays, 0);
		CHECK_EQ(log.pushes, 0);
		CHECK(log.asking == 0.0);
		CHECK(near(log.speed, 664.0, 1e-3));
		CHECK_EQ(log.phase, PUSH_NONE);
		// just below it one does, and it ends at 8.5 m/s
		log = runPush(60.0, 3.0, 8.29 * unit, hold, 1, 100, foot, 0, kGame);
		CHECK_EQ(log.plays, 1);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.asked[0], 320.0, 0.5));
		CHECK(near(log.speed, 680.0, 0.01));
		CHECK(log.most <= 680.01);
		// a tap at that speed is not kept for when the board has slowed
		const Press tap[] = { { 0.0, 1.0 / 60.0 } };
		log = runPush(60.0, 6.0, 8.3 * unit, tap, 1, 100, foot, 100, kGame);
		CHECK_EQ(log.plays, 0);
		CHECK(log.asking == 0.0);
		CHECK(log.speed < 664.0);
		// a button that stays down pushes as soon as it has
		log = runPush(60.0, 6.0, 8.3 * unit, hold, 1, 100, foot, 100, kGame);
		CHECK(log.plays >= 1);
		CHECK(log.took[0] < 0.1);
		CHECK(log.most <= 680.01);
		// percent: half of Skate's push, and twice
		log = runPush(60.0, 12.0, 0.0, hold, 1, 50, foot, 0, kGame);
		CHECK(near(log.added[0], 160.0, 0.05));
		CHECK(near(log.speed, 340.0, 0.01));
		CHECK_EQ(log.pushes, 3);
		log = runPush(60.0, 12.0, 0.0, hold, 1, 200, foot, 0, kGame);
		CHECK(near(log.added[0], 640.0, 0.05));
		CHECK(near(log.speed, 1360.0, 0.01));
		CHECK_EQ(log.pushes, 3);
		// A board that got that fast by itself, downhill, while the push was waiting for its
		// animation: the animation plays, and pushes nothing
		PushRun run;
		PushAnim anim = { false, false, 1, 0.0f, 0.8f };
		CHECK(run.Step(f60, 600.0f, true, 100, anim, foot) == 0.0f);
		CHECK_EQ(run.Phase(), PUSH_ASKED);
		anim.current = true;
		anim.running = true;
		anim.play = 2;
		for (int k = 0; k <= 48; k++) {
			anim.time = (float)(k * frame60);
			CHECK(run.Step(f60, 670.0f, true, 100, anim, foot) == 0.0f);
			CHECK_EQ(run.Phase(), PUSH_NONE);
			CHECK(!run.Animating());
		}
		CHECK_EQ((int)run.Serial(), 0);
	}

	g_case = "rolling: no faster than Skate lets a push come in";
	{
		// A foot that would push within a 60th of a second: the board still gets no more than
		// 0.6 m/s in a 60th at a standstill and 0.5 at full speed
		PushProfile snap;
		SmoothPushProfile(snap, 0.30f, 0.32f);
		const Press once[] = { { 0.0, 0.5 } };
		PushLog log = runPush(60.0, 3.0, 0.0, once, 1, 100, snap, 0, kGame);
		CHECK_EQ(log.pushes, 1);
		CHECK(log.biggest[0] <= 0.6 * unit + 0.01);
		CHECK(log.biggest[0] > 0.59 * unit);
		CHECK(log.added[0] <= 2.0 * 0.6 * unit + 0.01);
		CHECK(log.added[0] > 0.6 * unit);
		CHECK(near(log.asked[0], 320.0, 0.01));
		// at 240 frames a second a frame is a quarter of that, at 30 twice
		log = runPush(240.0, 3.0, 0.0, once, 1, 100, snap, 0, kGame);
		CHECK(log.biggest[0] <= 0.6 * unit / 4.0 + 0.01);
		CHECK(log.biggest[0] > 0.59 * unit / 4.0);
		log = runPush(30.0, 3.0, 0.0, once, 1, 100, snap, 0, kGame);
		CHECK(log.biggest[0] <= 2.0 * 0.6 * unit + 0.01);
		CHECK(log.biggest[0] > 2.0 * 0.59 * unit);
		// near the top speed it is less
		log = runPush(60.0, 3.0, 600.0, once, 1, 100, snap, 0, kGame);
		CHECK(log.biggest[0] <= (0.6 - 0.1 * 600.0 / 680.0) * unit + 0.01);
		CHECK(log.biggest[0] > 0.5 * unit);
		// and with half of Skate's push half as much
		log = runPush(60.0, 3.0, 0.0, once, 1, 50, snap, 0, kGame);
		CHECK(log.biggest[0] <= 0.6 * unit / 2.0 + 0.01);
		CHECK(log.biggest[0] > 0.59 * unit / 2.0);
		// (300 units/s is then as far up the board's range as 600 is with all of it)
		log = runPush(60.0, 3.0, 300.0, once, 1, 50, snap, 0, kGame);
		CHECK(log.biggest[0] <= (0.6 - 0.1 * 300.0 / 340.0) * unit / 2.0 + 0.01);
		CHECK(log.biggest[0] > 0.5 * unit / 2.0);
	}

	g_case = "rolling: what else pulls at the board is made up for";
	{
		// The board rolls out while the foot is on its way down. From where it is when the
		// foot starts, the push takes it up by what was asked: what the wheels take meanwhile
		// is added on top.
		const Press once[] = { { 0.0, 0.5 } };
		const PushLog log = runPush(60.0, 3.0, 300.0, once, 1, 100, foot, 100, kGame);
		CHECK_EQ(log.pushes, 1);
		CHECK(log.from[0] < 300.0 - 3.0 && log.from[0] > 300.0 - 12.0);
		CHECK(near(log.asked[0], 320.0, 0.01));
		CHECK(log.added[0] > 320.0 + 5.0 && log.added[0] < 320.0 + 12.0);
		CHECK(log.most > log.from[0] + 320.0 - 1.0 && log.most <= log.from[0] + 320.0 + 0.01);
	}

	g_case = "rolling: a board that is ahead of the foot";
	{
		// Downhill the board picks up speed by itself. Where it is already faster than the
		// push would have it by now, the push adds nothing. It never takes speed away.
		PushRun run;
		PushAnim anim = { true, true, 1, 0.0f, 0.8f };
		double speed = 100.0;
		for (int k = 0; k <= 48; k++) {
			anim.time = (float)(k * frame60);
			if (k == 18) speed += 200.0;
			const float add = run.Step(f60, (float)speed, true, 100, anim, foot);
			CHECK(add >= 0.0f);
			const double share = k * frame60 / 0.8;
			const double wanted = 100.0 + 320.0 * smooth((share - 0.30) / 0.32);
			if (k >= 18 && wanted < speed - 1.0) CHECK(add == 0.0f);
			speed += add;
		}
		// When the foot has pushed the board is where the push would have had it, no further:
		// the push gave it 24 units/s before the slope gave it 200, and the last 96 after
		CHECK(near(speed, 420.0, 0.01));
		CHECK(near(run.Added(), 120.0, 1.0));
		// faster than the whole push would have made it: nothing at all
		PushRun none;
		speed = 100.0;
		for (int k = 0; k <= 48; k++) {
			anim.time = (float)(k * frame60);
			if (k == 16) speed = 500.0;
			const float add = none.Step(f60, (float)speed, true, 100, anim, foot);
			if (k >= 16) CHECK(add == 0.0f);
			speed += add;
		}
		CHECK_EQ((int)none.Serial(), 1);
		CHECK(none.Added() < 3.0f);
		CHECK(near(speed, 500.0, 1e-6));
	}

	g_case = "rolling: the end of the animation";
	{
		// The animation stops at its end and stays the skater's (the game has nothing else to
		// play yet): a foot that was still pushing has finished its stroke
		PushProfile late;
		SmoothPushProfile(late, 0.60f, 1.0f);
		PushRun stop;
		PushAnim anim = { true, true, 1, 0.0f, 0.8f };
		double speed = 100.0;
		for (int k = 0; k <= 47; k++) {
			anim.time = (float)(k * frame60);
			speed += stop.Step(f60, (float)speed, k < 30, 100, anim, late);
		}
		CHECK_EQ(stop.Phase(), PUSH_CONTACT);
		CHECK(speed < 419.9);
		anim.running = false;
		anim.time = 0.8f;
		speed += stop.Step(f60, (float)speed, false, 100, anim, late);
		CHECK_EQ(stop.Phase(), PUSH_RECOVER);
		CHECK_EQ((int)stop.Serial(), 1);
		CHECK(near(speed, 420.0, 0.01));
		CHECK(stop.Step(f60, (float)speed, false, 100, anim, late) == 0.0f);
		// An animation that is no push animation any more, whatever else is said of it: the
		// push is over where it was
		PushRun gone;
		anim.running = true;
		speed = 100.0;
		for (int k = 0; k <= 22; k++) {
			anim.time = (float)(k * frame60);
			speed += gone.Step(f60, (float)speed, k < 20, 100, anim, foot);
		}
		CHECK_EQ(gone.Phase(), PUSH_CONTACT);
		PushAnim other = { false, false, 1, 0.5f, 0.8f };
		CHECK(gone.Step(f60, (float)speed, false, 100, other, foot) == 0.0f);
		CHECK_EQ(gone.Phase(), PUSH_NONE);
		CHECK_EQ((int)gone.Serial(), 1);
		// What the button does after the foot has pushed is nothing to that push
		PushRun after;
		speed = 100.0;
		for (int k = 0; k <= 32; k++) {
			anim.time = (float)(k * frame60);
			speed += after.Step(f60, (float)speed, true, 100, anim, foot);
		}
		CHECK_EQ(after.Phase(), PUSH_RECOVER);
		const float heldFor = after.HeldFor();
		CHECK(near(heldFor, 31.0 * frame60, 1e-4));
		for (int k = 33; k <= 47; k++) {
			anim.time = (float)(k * frame60);
			after.Step(f60, (float)speed, true, 100, anim, foot);
			CHECK(after.HeldFor() == heldFor);
		}
		// nor after its animation was cut off in mid-stroke (the board is too fast for another push by then)
		PushRun cut;
		speed = 650.0;
		for (int k = 0; k <= 24; k++) {
			anim.time = (float)(k * frame60);
			speed += cut.Step(f60, (float)speed, true, 100, anim, foot);
		}
		CHECK_EQ(cut.Phase(), PUSH_CONTACT);
		CHECK(speed > 664.0);
		PushAnim idle = { false, false, 2, 0.0f, 0.0f };
		cut.Step(f60, (float)speed, true, 100, idle, foot);
		CHECK_EQ(cut.Phase(), PUSH_NONE);
		const float was = cut.HeldFor();
		for (int k = 0; k < 10; k++) {
			cut.Step(f60, (float)speed, true, 100, idle, foot);
			CHECK_EQ(cut.Phase(), PUSH_NONE);
			CHECK(cut.HeldFor() == was);
		}
	}

	g_case = "rolling: asking the game for the animation";
	{
		PushRun run;
		PushAnim idle = { false, false, 5, 0.0f, 0.0f };
		CHECK(!run.Animating());
		CHECK(!run.HideTurn());
		CHECK(!run.Active());
		CHECK(!run.AnimCurrent());
		CHECK_EQ(run.Phase(), PUSH_NONE);
		// The button goes down: the game is asked. Its scripts do not see Left and Right for a
		// fifth of a second, then they do for three fifths, and so on for as long as it asks.
		int hidden = 0;
		for (int frame = 0; frame < 150; frame++) {
			CHECK(run.Step(f60, 200.0f, true, 100, idle, foot) == 0.0f);
			CHECK_EQ(run.Phase(), PUSH_ASKED);
			CHECK(run.Animating());
			CHECK(run.Active());
			const double into = fmod((frame + 1) * frame60, 0.8);
			if (into > 0.001 && into < 0.199) CHECK(run.HideTurn());
			if (into > 0.201 && into < 0.799) CHECK(!run.HideTurn());
			if (run.HideTurn()) hidden++;
		}
		CHECK(hidden >= 40 && hidden <= 44);    // 2.5 s: three times a fifth of a second, and the first tenth of the fourth
		// A push animation that was running before the button went down, and has its foot
		// down already: the game is busy with it and is not asked. The push waits for the next one.
		PushRun waits;
		PushAnim busy = { true, true, 6, 0.38f, 0.8f };
		CHECK(waits.Step(f60, 200.0f, false, 100, busy, foot) == 0.0f);
		CHECK_EQ(waits.Phase(), PUSH_NONE);
		CHECK(waits.AnimCurrent());
		busy.time = 0.4f;
		CHECK(waits.Step(f60, 200.0f, true, 100, busy, foot) == 0.0f);
		CHECK_EQ(waits.Phase(), PUSH_ASKED);
		CHECK(!waits.Animating());
		CHECK(!waits.HideTurn());
		CHECK(waits.AnimCurrent());
		CHECK(waits.Active());
		// One that has stopped at its end is still the skater's animation. The game is asked
		// again. (The loop of a skater who rides nollie has to hear a no first; it gets that
		// from the hook, which asks AnimCurrent.)
		PushAnim ended = { true, false, 6, 0.8f, 0.8f };
		CHECK(waits.Step(f60, 200.0f, true, 100, ended, foot) == 0.0f);
		CHECK_EQ(waits.Phase(), PUSH_ASKED);
		CHECK(waits.Animating());
		CHECK(waits.HideTurn());
		CHECK(waits.AnimCurrent());
		// The game starts a push: the push takes up with it and asks no more
		PushAnim fresh = { true, true, 7, 0.0f, 0.8f };
		CHECK(run.Step(f60, 200.0f, true, 100, fresh, foot) == 0.0f);
		CHECK_EQ(run.Phase(), PUSH_INTO);
		CHECK(!run.Animating());
		CHECK(!run.HideTurn());
		CHECK(run.Active());
		CHECK(run.AnimCurrent());
		CHECK_EQ((int)run.Play(), 7);
		CHECK(near(run.Down(), 0.30, 1e-6));
		CHECK(near(run.Up(), 0.62, 1e-6));
		// through the foot's stroke the board is being pushed; after it, not
		fresh.time = 0.4f;
		CHECK(run.Step(f60, 200.0f, true, 100, fresh, foot) > 0.0f);
		CHECK_EQ(run.Phase(), PUSH_CONTACT);
		CHECK(run.Active());
		CHECK(!run.Animating());
		fresh.time = 0.6f;
		run.Step(f60, 200.0f, true, 100, fresh, foot);
		CHECK_EQ(run.Phase(), PUSH_RECOVER);
		CHECK(!run.Active());
		CHECK(!run.Animating());
		// the animation over, the button still down: the next one is asked for, with a new fifth of a second
		idle.play = 8;
		CHECK(run.Step(f60, 200.0f, true, 100, idle, foot) == 0.0f);
		CHECK_EQ(run.Phase(), PUSH_ASKED);
		CHECK(run.Animating());
		CHECK(run.HideTurn());
		CHECK(!run.AnimCurrent());
		// an animation of no length is no animation
		PushRun other;
		PushAnim empty = { true, true, 9, 0.0f, 0.0f };
		CHECK(other.Step(f60, 200.0f, true, 100, empty, foot) == 0.0f);
		CHECK_EQ(other.Phase(), PUSH_ASKED);
		CHECK(other.Animating());
		// and "running" says nothing of an animation that is not a push animation
		PushRun odd;
		PushAnim walk = { false, true, 9, 0.1f, 0.8f };
		CHECK(odd.Step(f60, 200.0f, true, 100, walk, foot) == 0.0f);
		CHECK_EQ(odd.Phase(), PUSH_ASKED);
		CHECK(odd.Animating());
		CHECK(!odd.AnimCurrent());
		// Frames that are no frames still see what the animation does: a push animation that
		// ran through one was running before the button went down
		PushRun blink;
		PushAnim going = { true, true, 4, 0.4f, 0.8f };
		blink.Step(0.0f, 100.0f, false, 100, going, foot);
		going.time = 0.42f;
		CHECK(blink.Step(f60, 100.0f, true, 100, going, foot) == 0.0f);
		CHECK_EQ(blink.Phase(), PUSH_ASKED);
		// Every push asks anew. This one has to ask for 0.6 s before the game plays it; the
		// tap after it still has its three quarters of a second.
		PushRun twice;
		PushAnim nothing = { false, false, 1, 0.0f, 0.0f };
		for (int k = 0; k < 36; k++) twice.Step(f60, 100.0f, true, 100, nothing, foot);
		CHECK(twice.Animating());
		PushAnim played = { true, true, 2, 0.0f, 0.8f };
		for (int k = 0; k <= 47; k++) {
			played.time = (float)(k * frame60);
			twice.Step(f60, 100.0f, false, 100, played, foot);
		}
		CHECK_EQ(twice.Phase(), PUSH_RECOVER);
		nothing.play = 3;
		int asking = 0;
		for (int k = 0; k < 120; k++) {
			twice.Step(f60, 100.0f, k < 2, 100, nothing, foot);
			if (twice.Animating()) asking++;
		}
		CHECK(asking >= 44 && asking <= 46);
		// A tap during a push animation that is not its own waits for that to end, however
		// long it takes: the three quarters of a second are for a game that has nothing to show
		PushRun patient;
		PushAnim slow = { true, true, 1, 0.7f, 2.0f };
		patient.Step(f60, 100.0f, false, 100, slow, foot);
		for (int k = 1; k <= 76; k++) {
			slow.time = (float)(0.7 + k * frame60);
			CHECK(patient.Step(f60, 100.0f, k < 3, 100, slow, foot) == 0.0f);
			CHECK_EQ(patient.Phase(), PUSH_ASKED);
			CHECK(!patient.Animating());
		}
		nothing.play = 2;
		patient.Step(f60, 100.0f, false, 100, nothing, foot);
		CHECK_EQ(patient.Phase(), PUSH_ASKED);
		CHECK(patient.Animating());
	}

	g_case = "rolling: a push for an animation that is already running";
	{
		// The game is in a push animation nobody asked for, and its foot is not down yet: a
		// press takes it. Two frames of a press: a tap's push.
		PushRun run;
		PushAnim anim = { true, true, 3, 0.1f, 0.8f };
		double speed = 100.0;
		speed += run.Step(f60, (float)speed, false, 100, anim, foot);
		CHECK_EQ(run.Phase(), PUSH_NONE);
		CHECK(!run.Animating());
		for (int k = 12; k <= 48; k++) {
			anim.time = (float)(k * frame60);
			speed += run.Step(f60, (float)speed, k < 14, 100, anim, foot);
			if (k == 12) CHECK_EQ(run.Phase(), PUSH_INTO);
		}
		CHECK_EQ(run.Phase(), PUSH_RECOVER);
		CHECK_EQ((int)run.Serial(), 1);
		CHECK(near(run.HeldFor(), 2.0 * frame60, 1e-4));
		CHECK(near(run.Added(), askedFor(2.0 * frame60, false), 0.5));
		CHECK(near(speed, 100.0 + run.Added(), 1e-3));
		// With its foot down already it is too late: the push waits, and nothing is added for
		// the rest of that animation. The next one is its own.
		PushRun late;
		speed = 100.0;
		anim.time = (float)(18 * frame60);
		CHECK(late.Step(f60, (float)speed, false, 100, anim, foot) == 0.0f);
		for (int k = 19; k <= 48; k++) {
			anim.time = (float)(k * frame60);
			CHECK(late.Step(f60, (float)speed, true, 100, anim, foot) == 0.0f);
			CHECK_EQ(late.Phase(), PUSH_ASKED);
			CHECK(!late.Animating());
		}
		PushAnim idle = { false, false, 4, 0.0f, 0.0f };
		CHECK(late.Step(f60, (float)speed, true, 100, idle, foot) == 0.0f);
		CHECK(late.Animating());
		anim.play = 5;
		for (int k = 0; k <= 48; k++) {
			anim.time = (float)(k * frame60);
			speed += late.Step(f60, (float)speed, true, 100, anim, foot);
		}
		CHECK_EQ((int)late.Serial(), 1);
		CHECK(near(speed, 420.0, 0.05));
		// a hair after the foot starts is too late, a hair before is not
		PushRun edge;
		anim.play = 6;
		anim.time = 0.2f;
		edge.Step(f60, 100.0f, false, 100, anim, foot);
		anim.time = 0.2404f;
		CHECK(edge.Step(f60, 100.0f, true, 100, anim, foot) == 0.0f);
		CHECK_EQ(edge.Phase(), PUSH_ASKED);
		PushRun before;
		anim.time = 0.2f;
		before.Step(f60, 100.0f, false, 100, anim, foot);
		anim.time = 0.2396f;
		CHECK(before.Step(f60, 100.0f, true, 100, anim, foot) == 0.0f);
		CHECK_EQ(before.Phase(), PUSH_INTO);
	}

	g_case = "rolling: an animation the game started for the push is the push's";
	{
		// The push was wanted, the game has started its animation since, and the first frame
		// that sees it is late: a hitch. The foot is in its stroke already. It is this push's
		// foot all the same, and the board catches up with it as fast as Skate lets it.
		PushAnim idle = { false, false, 1, 0.0f, 0.0f };
		PushRun run;
		double speed = 100.0;
		for (int k = 0; k < 30; k++) {
			CHECK(run.Step(f60, (float)speed, true, 100, idle, foot) == 0.0f);
		}
		CHECK_EQ(run.Phase(), PUSH_ASKED);
		PushAnim anim = { true, true, 2, 0.0f, 0.8f };
		double biggest = 0.0;
		for (int k = 24; k <= 48; k++) {
			anim.time = (float)(k * frame60);
			const float add = run.Step(f60, (float)speed, true, 100, anim, foot);
			if (k == 24) {
				CHECK_EQ(run.Phase(), PUSH_CONTACT);
				CHECK(add > 0.58f * 80.0f);
			}
			if (add > biggest) biggest = add;
			speed += add;
		}
		CHECK_EQ(run.Phase(), PUSH_RECOVER);
		CHECK_EQ((int)run.Serial(), 1);
		CHECK(biggest <= 0.6 * 80.0 + 0.01);
		CHECK(speed > 100.0 + 300.0 && speed <= 420.01);
		// seen for the first time after the stroke: one frame's worth, and that is all
		PushRun missed;
		missed.Step(f60, 100.0f, true, 100, idle, foot);
		anim.play = 3;
		anim.time = 0.7f;
		const float once = missed.Step(f60, 100.0f, true, 100, anim, foot);
		CHECK(once > 0.0f && once <= 0.6f * 80.0f + 0.01f);
		CHECK_EQ(missed.Phase(), PUSH_RECOVER);
		anim.time = 0.72f;
		CHECK(missed.Step(f60, 100.0f + once, true, 100, anim, foot) == 0.0f);
		// An animation that was running before the push was wanted is another matter: see above.
		// And one that starts over by itself is a new animation every round.
		PushRun round;
		anim.play = 4;
		anim.time = 0.7f;
		round.Step(f60, 100.0f, false, 100, anim, foot);
		anim.play = 5;
		anim.time = 0.4f;       // a round later, the look is late again
		CHECK(round.Step(f60, 100.0f, true, 100, anim, foot) > 0.0f);
		CHECK_EQ(round.Phase(), PUSH_CONTACT);
		// A foot that pushes from the very start of its animation: the push can only take up
		// with it because the game has just started it
		PushProfile first;
		SmoothPushProfile(first, 0.0f, 0.4f);
		CHECK(first.down == 0.0f);
		const Press hold[] = { { 0.0, 0.5 } };
		const PushLog log = runPush(60.0, 3.0, 100.0, hold, 1, 100, first, 0, kGame);
		CHECK_EQ(log.plays, 1);
		CHECK_EQ(log.pushes, 1);
		CHECK(log.firstShare[0] > 0.0 && log.firstShare[0] < 0.05);
		CHECK(near(log.asked[0], 320.0, 0.01));
		CHECK(log.added[0] > 0.9 * 320.0 && log.added[0] <= 320.01);
	}

	g_case = "rolling: an animation that starts over by itself";
	{
		// The push with the front foot runs as a loop. With the button down every round of it
		// is a push; let go, the rounds go on and push nothing.
		Game loop = kGame;
		loop.loops = true;
		const Press hold[] = { { 0.0, 2.2 } };
		const PushLog log = runPush(60.0, 6.0, 0.0, hold, 1, 100, foot, 0, loop);
		CHECK_EQ(log.pushes, 3);
		CHECK(log.plays >= 7);
		CHECK(near(log.added[0], 320.0, 0.05));
		CHECK(near(log.added[1], 320.0, 0.05));
		CHECK(near(log.added[2], 40.0, 0.05));
		for (int i = 3; i < log.plays && i < 16; i++) CHECK(log.added[i] == 0.0);
		for (int i = 0; i < 3; i++) CHECK(near(log.firstAt[i], log.took[i] + 0.24, frame60 + 1e-6));
		CHECK(near(log.took[1] - log.took[0], 0.8, 1e-6));
		CHECK(near(log.speed, 680.0, 0.01));
		// A foot that pushes through the whole of every round: where one round's push ends
		// the next one's starts, on the same frame, and nothing is counted twice or lost
		{
			PushProfile whole;
			SmoothPushProfile(whole, 0.0f, 1.0f);
			PushRun both;
			PushAnim round = { true, true, 1, 0.0f, 0.8f };
			double fast = 0.0;
			for (int lap = 0; lap < 2; lap++) {
				for (int k = 0; k <= 47; k++) {
					round.time = (float)(k * frame60);
					fast += both.Step(f60, (float)fast, true, 100, round, whole);
					if (lap == 1 && k == 0) CHECK(near(fast, 320.0, 0.01));
				}
				CHECK(fast < (lap + 1) * 320.0 - 0.2);
				round.play++;
			}
			round.time = 0.0f;
			fast += both.Step(f60, (float)fast, false, 100, round, whole);
			CHECK(near(fast, 640.0, 0.01));
			CHECK_EQ((int)both.Serial(), 2);
		}
		// a tap in one round is a push in the next
		const Press tap[] = { { 0.5, 0.55 } };
		PushRun run;
		PushAnim anim = { true, true, 1, 0.0f, 0.8f };
		double speed = 100.0;
		int rounds = 0;
		double clock = 0.0;
		double addedIn[4] = { 0.0, 0.0, 0.0, 0.0 };
		for (int frame = 0; frame < 180; frame++) {
			const double t = frame * frame60;
			anim.time = (float)clock;
			const float add = run.Step(f60, (float)speed, t >= tap[0].down - 1e-9 && t < tap[0].up - 1e-9, 100, anim, foot);
			speed += add;
			if (rounds < 4) addedIn[rounds] += add;
			clock += frame60;
			if (clock >= 0.8 - 1e-9) {
				clock -= 0.8;
				anim.play++;
				rounds++;
			}
		}
		CHECK(addedIn[0] == 0.0);
		CHECK(near(addedIn[1], askedFor(0.05, false), 0.5));
		CHECK(addedIn[2] == 0.0);
	}

	g_case = "rolling: the push keeps the foot it started with";
	{
		// Where the foot pushes is settled when the push takes up with the animation. What is
		// said of the animation's foot afterwards (it has been watched meanwhile, say) is for
		// the next push.
		PushProfile other;
		SmoothPushProfile(other, 0.05f, 0.15f);
		PushRun run;
		PushAnim anim = { true, true, 1, 0.0f, 0.8f };
		double speed = 100.0;
		for (int k = 0; k <= 48; k++) {
			anim.time = (float)(k * frame60);
			const float add = run.Step(f60, (float)speed, true, 100, anim, k == 0 ? foot : other);
			speed += add;
			const double share = k * frame60 / 0.8;
			if (share < 0.30) CHECK(add == 0.0f);
			CHECK(near(speed, 100.0 + 320.0 * smooth((share - 0.30) / 0.32), 320.0 * 0.004 + 0.01));
			CHECK(near(run.Down(), 0.30, 1e-6));
			CHECK(near(run.Up(), 0.62, 1e-6));
		}
		// the next animation: the other foot
		PushAnim idle = { false, false, 2, 0.0f, 0.0f };
		run.Step(f60, (float)speed, true, 100, idle, other);
		anim.play = 3;
		anim.time = 0.0f;
		run.Step(f60, (float)speed, true, 100, anim, other);
		CHECK_EQ(run.Phase(), PUSH_INTO);
		CHECK(near(run.Down(), 0.05, 1e-6));
		CHECK(near(run.Up(), 0.15, 1e-6));
	}

	g_case = "rolling: a push that is called off";
	{
		// Reset (a bail, Down, the board off the ground) at any point of a push: nothing more
		// comes of it, whatever the animation goes on to do
		const int at[] = { 6, 20, 40 };     // on the way down, in the stroke, after it
		for (int i = 0; i < 3; i++) {
			PushRun run;
			PushAnim anim = { true, true, 1, 0.0f, 0.8f };
			double speed = 100.0;
			for (int k = 0; k <= at[i]; k++) {
				anim.time = (float)(k * frame60);
				speed += run.Step(f60, (float)speed, k < 30, 100, anim, foot);
			}
			CHECK_EQ(run.Phase(), i == 0 ? PUSH_INTO : (i == 1 ? PUSH_CONTACT : PUSH_RECOVER));
			const uint32_t serial = run.Serial();
			const double was = speed;
			run.Reset();
			CHECK_EQ(run.Phase(), PUSH_NONE);
			CHECK(!run.Active());
			CHECK(!run.Animating());
			CHECK(!run.HideTurn());
			for (int k = at[i] + 1; k <= 48; k++) {
				anim.time = (float)(k * frame60);
				CHECK(run.Step(f60, (float)speed, false, 100, anim, foot) == 0.0f);
				CHECK_EQ(run.Phase(), PUSH_NONE);
			}
			CHECK_EQ((int)run.Serial(), (int)serial);
			CHECK(speed == was);
		}
		// a press that was being kept is forgotten as well
		PushRun run;
		PushAnim anim = { true, true, 1, 0.0f, 0.8f };
		for (int k = 0; k <= 20; k++) {
			anim.time = (float)(k * frame60);
			run.Step(f60, 100.0f, k < 2 || (k >= 10 && k < 12), 100, anim, foot);
		}
		run.Reset();
		PushAnim idle = { false, false, 2, 0.0f, 0.0f };
		for (int k = 0; k < 60; k++) {
			CHECK(run.Step(f60, 100.0f, false, 100, idle, foot) == 0.0f);
			CHECK_EQ(run.Phase(), PUSH_NONE);
			CHECK(!run.Animating());
		}
		// and a push that was only asking
		PushRun asked;
		asked.Step(f60, 0.0f, true, 100, idle, foot);
		CHECK_EQ(asked.Phase(), PUSH_ASKED);
		CHECK(asked.HideTurn());
		asked.Reset();
		CHECK_EQ(asked.Phase(), PUSH_NONE);
		CHECK(!asked.Animating());
		CHECK(!asked.HideTurn());
		// With the button still down after it (Down let go again): the game is asked anew, and
		// a foot that is in its stroke already is not this push's foot
		PushRun again;
		double speed = 100.0;
		for (int k = 0; k <= 48; k++) {
			anim.time = (float)(k * frame60);
			if (k == 20) again.Reset();
			const float add = again.Step(f60, (float)speed, true, 100, anim, foot);
			if (k >= 20) {
				CHECK(add == 0.0f);
				CHECK_EQ(again.Phase(), PUSH_ASKED);
			}
			speed += add;
		}
	}

	g_case = "rolling: frames that are no frames";
	{
		PushRun run;
		PushAnim idle = { false, false, 1, 0.0f, 0.0f };
		CHECK(run.Step(0.0f, 100.0f, true, 100, idle, foot) == 0.0f);
		CHECK_EQ(run.Phase(), PUSH_NONE);
		CHECK(run.Step(-1.0f, 100.0f, true, 100, idle, foot) == 0.0f);
		CHECK_EQ(run.Phase(), PUSH_NONE);
		CHECK(run.Step(nan, 100.0f, true, 100, idle, foot) == 0.0f);
		CHECK_EQ(run.Phase(), PUSH_NONE);
		// with the push the game's own (0) nothing is asked either
		CHECK(run.Step(f60, 100.0f, true, 0, idle, foot) == 0.0f);
		CHECK_EQ(run.Phase(), PUSH_NONE);
		CHECK(run.Step(f60, 100.0f, true, -5, idle, foot) == 0.0f);
		CHECK_EQ(run.Phase(), PUSH_NONE);
		// the press is not lost over it, and what the animation does is seen
		PushAnim ended = { true, false, 1, 0.8f, 0.8f };
		run.Step(0.0f, 100.0f, true, 100, ended, foot);
		CHECK(run.AnimCurrent());
		CHECK(run.Step(f60, 100.0f, true, 100, idle, foot) == 0.0f);
		CHECK_EQ(run.Phase(), PUSH_ASKED);
		CHECK(!run.AnimCurrent());
		// a frame that took a second counts as a tenth: a tap gives up after eight of them, not after one
		PushRun slow;
		slow.Step(f60, 100.0f, true, 100, idle, foot);
		for (int k = 0; k < 7; k++) {
			slow.Step(1.0f, 100.0f, false, 100, idle, foot);
			CHECK_EQ(slow.Phase(), PUSH_ASKED);
		}
		slow.Step(1.0f, 100.0f, false, 100, idle, foot);
		CHECK_EQ(slow.Phase(), PUSH_NONE);
		// and in the stroke it adds what six of Skate's ticks may, no more
		PushRun hitch;
		PushAnim anim = { true, true, 2, 0.0f, 0.8f };
		for (int k = 0; k < 14; k++) {
			anim.time = (float)(k * frame60);
			CHECK(hitch.Step(f60, 0.0f, true, 100, anim, foot) == 0.0f);
		}
		anim.time = 0.7f;
		const float add = hitch.Step(1.0f, 0.0f, true, 100, anim, foot);
		CHECK(add > 0.0f);
		CHECK(add <= 6.0f * 0.6f * 80.0f + 0.01f);
		CHECK(add > 5.9f * 0.6f * 80.0f);
		CHECK_EQ(hitch.Phase(), PUSH_RECOVER);
	}

	g_case = "rolling: the game's other scripts that play the push";
	{
		// The loop of a skater who rides nollie, and the crouch: neither waits for the
		// animation, and both leave it standing at its end until they have something else to
		// play. With the button down the pushes follow each other all the same.
		const Press hold[] = { { 0.0, 30.0 } };
		const int scripts[] = { SCRIPT_NOLLIE, SCRIPT_CROUCH };
		for (int i = 0; i < 2; i++) {
			Game g = kGame;
			g.script = scripts[i];
			const PushLog log = runPush(60.0, 12.0, 0.0, hold, 1, 100, foot, 0, g);
			CHECK_EQ(log.pushes, 3);
			CHECK_EQ(log.plays, 3);
			CHECK(near(log.speed, 680.0, 0.01));
			CHECK(near(log.took[1] - log.took[0], 0.8 + frame60, 1e-6));
			// and taps
			const Press taps[] = { { 0.0, 0.05 }, { 0.4, 0.45 }, { 2.5, 2.55 } };
			const PushLog tapped = runPush(60.0, 5.0, 0.0, taps, 3, 100, foot, 0, g);
			CHECK_EQ(tapped.pushes, 3);
			CHECK_EQ(tapped.plays, 3);
			CHECK(near(tapped.took[2], 2.5 + frame60, 1e-6));
		}
	}
}

// The pushes of a game whose push animation the mod does not get to see: the game's scripts are
// told to play the push, and what comes back is "he is in no push animation". From some frame on
// it is seen after all (never, if that is less than 0).
struct BlindLog {
	double speed;
	int pushes;
	int took;               // the times the clock took over
	double tookAt;          // when it first did
	double firstAdd;        // when speed first came
	double starts[8];       // when each push by the clock started adding
	double told[8];         // and when the game had been told to play it
	int clockPushes;
	int seenPushes;
	bool blindAtEnd;
};

static BlindLog runBlind(double seconds, double seenFrom, bool held, const PushProfile& profile) {
	const double dt = 1.0 / 60.0;
	PushRun run;
	PushClock clock;
	BlindLog log;
	memset(&log, 0, sizeof(log));
	log.tookAt = -1.0;
	log.firstAdd = -1.0;
	double speed = 0.0;
	double game = -1.0;         // how far the game's own animation is; less than 0: it plays none
	uint32_t play = 0;
	uint32_t serial = run.Serial();
	uint32_t clockSerial = clock.Serial();
	double lastTold = -1.0;
	bool adding = false;
	const int frames = (int)(seconds * 60.0 + 0.5);
	for (int frame = 0; frame < frames; frame++) {
		const double t = frame * dt;
		// the game's scripts: told to play the push, they play it, and wait for it to end
		if (game < 0.0 && run.Animating()) {
			clock.Told();
			lastTold = t;
			game = 0.0;
			play++;
		}
		else if (game >= 0.8) {
			game = -1.0;
			play++;
		}
		// the look at the skater's animation
		const bool seen = seenFrom >= 0.0 && t >= seenFrom && game >= 0.0;
		PushAnim anim = { seen, seen, play, (float)(game > 0.0 ? game : 0.0), 0.8f };
		const bool byClock = clock.Step(seen, (float)dt, anim);
		CHECK(!(seen && byClock));
		const float add = run.Step((float)dt, (float)speed, held || t < 0.05, 100, anim, profile);
		CHECK(add >= 0.0f);
		if (add > 0.0f) {
			CHECK(seen || byClock);
			if (log.firstAdd < 0.0) log.firstAdd = t;
			if (!adding && byClock && log.clockPushes < 8) {
				log.starts[log.clockPushes] = t;
				log.told[log.clockPushes] = lastTold;
			}
			adding = true;
		}
		speed += add;
		if (run.Serial() != serial) {
			serial = run.Serial();
			log.pushes++;
			if (byClock) log.clockPushes++; else log.seenPushes++;
			adding = false;
		}
		if (clock.Serial() != clockSerial) {
			clockSerial = clock.Serial();
			log.took++;
			if (log.tookAt < 0.0) log.tookAt = t;
		}
		if (game >= 0.0) game += dt;
	}
	log.speed = speed;
	log.blindAtEnd = clock.Blind();
	return log;
}

static void testPushClock() {
	const double frame60 = 1.0 / 60.0;
	PushProfile foot;
	SmoothPushProfile(foot, 0.30f, 0.62f);

	g_case = "rolling: a game whose push animation is seen";
	{
		// As it should be: the clock never takes over
		const BlindLog log = runBlind(12.0, 0.0, true, foot);
		CHECK_EQ(log.took, 0);
		CHECK_EQ(log.pushes, 3);
		CHECK_EQ(log.seenPushes, 3);
		CHECK_EQ(log.clockPushes, 0);
		CHECK(near(log.speed, 680.0, 0.01));
		CHECK(!log.blindAtEnd);
		CHECK_EQ(kPushClockAfter, 3);
		CHECK(near(kPushClockLength, 0.8, 1e-6));
	}

	g_case = "rolling: a game whose push animation is not seen";
	{
		// The game is told to play the push, plays it, is told again when it is over, and the
		// look at the skater never shows a push animation. After the third time the pushes go
		// by the clock: each one from the frame the game is told, and the speed comes where
		// the settings say the foot pushes. Two animations' worth of time is lost, no more.
		const BlindLog log = runBlind(12.0, -1.0, true, foot);
		CHECK_EQ(log.took, 1);
		CHECK(log.blindAtEnd);
		CHECK(log.tookAt > 1.6 && log.tookAt < 1.8);
		CHECK(log.firstAdd > 2.4 && log.firstAdd < 2.9);
		CHECK_EQ(log.pushes, 3);
		CHECK_EQ(log.clockPushes, 3);
		CHECK(near(log.speed, 680.0, 0.01));
		for (int i = 0; i < 3; i++) {
			CHECK(near(log.starts[i] - log.told[i], 0.30 * 0.8, 2.0 * frame60));
		}
		// one push for every 0.8 s by the clock and the frames around it, as with the animation
		CHECK(near(log.starts[1] - log.starts[0], 0.8 + 3.0 * frame60, 2.0 * frame60));
		// one tap is not enough to tell: it is lost, and the clock does not take over for it
		const BlindLog tap = runBlind(5.0, -1.0, false, foot);
		CHECK_EQ(tap.took, 0);
		CHECK_EQ(tap.pushes, 0);
		CHECK(tap.speed == 0.0);
	}

	g_case = "rolling: a push animation that is seen after all";
	{
		// From the fourth second on the look at the skater shows the animation: the clock
		// steps back at once and the pushes are the animation's again
		const BlindLog log = runBlind(12.0, 4.0, true, foot);
		CHECK_EQ(log.took, 1);
		CHECK(!log.blindAtEnd);
		CHECK(log.clockPushes >= 1);
		CHECK(log.seenPushes >= 1);
		CHECK(near(log.speed, 680.0, 0.01));
	}

	g_case = "rolling: the clock by itself";
	{
		PushClock clock;
		PushAnim anim = { false, false, 7, 0.0f, 0.0f };
		CHECK(!clock.Blind());
		CHECK_EQ((int)clock.Serial(), 0);
		// told twice and nothing seen: not yet
		for (int k = 0; k < 2; k++) {
			clock.Told();
			for (int f = 0; f < 50; f++) {
				CHECK(!clock.Step(false, (float)frame60, anim));
				CHECK(!clock.Blind());
			}
		}
		CHECK(!anim.current);
		CHECK_EQ((int)anim.play, 7);
		// the third time: it takes over, and the next time the game is told a push starts
		clock.Told();
		CHECK(!clock.Step(false, (float)frame60, anim));
		CHECK(clock.Blind());
		CHECK_EQ((int)clock.Serial(), 1);
		CHECK(!clock.Step(false, (float)frame60, anim));
		clock.Told();
		uint32_t play = 0;
		int ran = 0;
		for (int k = 0; k < 60; k++) {
			PushAnim now = { false, false, 7, 0.0f, 0.0f };
			CHECK(clock.Step(false, (float)frame60, now));
			CHECK(now.current);
			CHECK(near(now.length, 0.8, 1e-6));
			if (k == 0) play = now.play;
			CHECK_EQ((int)now.play, (int)play);
			CHECK(now.play > 0x80000000u);      // far from the numbers of the game's animations
			if (!now.running) {
				CHECK(near(now.time, 0.8, 1e-6));
				break;
			}
			CHECK(near(now.time, k * frame60, 1e-4));
			ran++;
			if (k == 20) clock.Told();      // told again while one is under way: it is the same push
		}
		CHECK(ran == 48 || ran == 49);
		// it has stood at its end for a frame: now there is none, until the game is told again
		CHECK(!clock.Step(false, (float)frame60, anim));
		CHECK(!anim.current);
		clock.Told();
		PushAnim next = { false, false, 7, 0.0f, 0.0f };
		CHECK(clock.Step(false, (float)frame60, next));
		CHECK(next.play != play);
		CHECK(next.time == 0.0f);
		// a frame that is no frame does not move it, a long one moves it a tenth of a second
		CHECK(clock.Step(false, 0.0f, next));
		CHECK(near(next.time, frame60, 1e-5));
		CHECK(clock.Step(false, -1.0f, next));
		CHECK(near(next.time, frame60, 1e-5));
		CHECK(clock.Step(false, nanf(""), next));
		CHECK(near(next.time, frame60, 1e-5));
		CHECK(clock.Step(false, 5.0f, next));
		CHECK(near(next.time, frame60, 1e-5));
		CHECK(clock.Step(false, (float)frame60, next));
		CHECK(near(next.time, frame60 + 0.1, 1e-5));
		// a push animation is seen: the clock is not needed, and what is handed in stays as it is
		PushAnim real = { true, true, 9, 0.3f, 0.7f };
		CHECK(!clock.Step(true, (float)frame60, real));
		CHECK(!clock.Blind());
		CHECK_EQ((int)real.play, 9);
		CHECK(near(real.time, 0.3, 1e-6));
		CHECK_EQ((int)clock.Serial(), 1);
		// and it starts counting from nothing again
		for (int k = 0; k < 2; k++) clock.Told();
		CHECK(!clock.Step(false, (float)frame60, anim));
		CHECK(!clock.Blind());
		// seeing one in between starts the count over
		CHECK(!clock.Step(true, (float)frame60, real));
		for (int k = 0; k < 2; k++) clock.Told();
		CHECK(!clock.Step(false, (float)frame60, anim));
		CHECK(!clock.Blind());
		clock.Told();
		// it takes over again, and the push it was in the middle of last time is not taken up again
		PushAnim none = { false, false, 7, 0.0f, 0.0f };
		CHECK(!clock.Step(false, (float)frame60, none));
		CHECK(clock.Blind());
		CHECK_EQ((int)clock.Serial(), 2);
		CHECK(!clock.Step(false, (float)frame60, none));
		CHECK(!none.current);
		// Reset: back to seeing
		clock.Reset();
		CHECK(!clock.Blind());
		clock.Told();
		CHECK(!clock.Step(false, (float)frame60, anim));
		CHECK_EQ((int)clock.Serial(), 2);
	}
}

static void testFeet() {
	const float nan = nanf("");

	g_case = "feet: a push animation watched";
	{
		// The foot leaves the board at 12% of the animation, is on the ground at 30%, travels
		// 28 units back along it until 60%, leaves it at 66% and is on the board again at 88%
		int looks = 0;
		const FootResult r = watchClip(kClip, 60.0, 0.0, 1.0, &looks);
		CHECK_EQ(r.why, FOOT_OK);
		CHECK_EQ(looks, 48);
		CHECK_EQ(r.samples, 48);
		CHECK(!r.left);
		CHECK(near(r.drop, 4.5, 0.1));
		CHECK(near(r.sweep, 28.0, 0.2));
		// it is down from a little before it has landed until a little after it has lifted
		CHECK(r.lowFrom > 0.20 && r.lowFrom < 0.26);
		CHECK(r.lowTo > 0.66 && r.lowTo < 0.71);
		// its stroke is the time it travels, less the first and the last fiftieth of the way
		CHECK(near(r.profile.down, 0.3125, 1e-4));
		CHECK(near(r.profile.up, 0.5833, 1e-4));
		// and how it travels: gently off, gently in, half way at half time
		CHECK(r.profile.done[0] == 0.0f);
		CHECK(r.profile.done[PUSH_KNOTS - 1] == 1.0f);
		for (int k = 0; k + 1 < PUSH_KNOTS; k++) CHECK(r.profile.done[k + 1] >= r.profile.done[k]);
		CHECK(near(PushDone(r.profile, 0.45f), 0.5, 0.02));
		for (int i = 30; i <= 60; i++) {
			CHECK(near(PushDone(r.profile, i * 0.01f), smooth((i * 0.01 - 0.30) / 0.30), 0.03));
		}
		// A push that goes by it: on every frame the board has as much of the push as the
		// foot has of its way
		PushRun run;
		PushAnim anim = { true, true, 1, 0.0f, 0.8f };
		double speed = 100.0;
		for (int k = 0; k <= 48; k++) {
			anim.time = (float)(k / 60.0);
			speed += run.Step(1.0f / 60.0f, (float)speed, true, 100, anim, r.profile);
			const double share = k / 60.0 / 0.8;
			CHECK(near((speed - 100.0) / 320.0, smooth((share - 0.30) / 0.30), 0.03));
		}
		CHECK(near(speed, 420.0, 0.01));
	}

	g_case = "feet: at any frame rate";
	{
		const double rates[] = { 25.0, 30.0, 50.0, 60.0, 90.0, 144.0, 240.0, 500.0 };
		for (int i = 0; i < 8; i++) {
			int looks = 0;
			const FootResult r = watchClip(kClip, rates[i], 0.0, 1.0, &looks);
			const double step = 1.0 / rates[i] / 0.8;
			CHECK_EQ(r.why, FOOT_OK);
			// one look is kept for every 128th of the animation
			CHECK(r.samples <= 128);
			if (looks <= 100) CHECK_EQ(r.samples, looks);
			if (looks > 200) CHECK(r.samples > 120);
			CHECK(!r.left);
			CHECK(near(r.drop, 4.5, 0.1));
			CHECK(near(r.sweep, 28.0, 0.3));
			CHECK(near(r.profile.down, 0.325, 0.006 + step));
			CHECK(near(r.profile.up, 0.575, 0.006 + step));
			CHECK(near(PushDone(r.profile, 0.45f), 0.5, 0.03));
		}
	}

	g_case = "feet: whichever foot, whichever way";
	{
		const FootResult right = watchClip(kClip, 60.0, 0.0, 1.0, 0);
		Clip c = kClip;
		c.leftPushes = true;
		FootResult r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_OK);
		CHECK(r.left);
		CHECK(near(r.drop, right.drop, 1e-4));
		CHECK(near(r.sweep, right.sweep, 1e-4));
		CHECK(near(r.profile.down, right.profile.down, 1e-6));
		CHECK(near(r.profile.up, right.profile.up, 1e-6));
		for (int k = 0; k < PUSH_KNOTS; k++) CHECK(near(r.profile.done[k], right.profile.done[k], 1e-5));
		// the board along the other axis
		c = kClip;
		c.sideways = true;
		r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_OK);
		CHECK(!r.left);
		CHECK(near(r.sweep, right.sweep, 1e-4));
		CHECK(near(r.profile.down, right.profile.down, 1e-6));
		CHECK(near(r.profile.up, right.profile.up, 1e-6));
		for (int k = 0; k < PUSH_KNOTS; k++) CHECK(near(r.profile.done[k], right.profile.done[k], 1e-5));
		// a stroke that goes forward instead of back
		c = kClip;
		c.ahead = -14.0;
		c.behind = -14.0;
		r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_OK);
		CHECK(near(r.sweep, right.sweep, 1e-3));
		CHECK(near(r.profile.down, right.profile.down, 1e-6));
		CHECK(near(r.profile.up, right.profile.up, 1e-6));
		// numbers that jitter by a twentieth of a unit
		c = kClip;
		c.noise = 0.05;
		r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_OK);
		CHECK(near(r.drop, right.drop, 0.15));
		CHECK(near(r.sweep, right.sweep, 0.3));
		CHECK(near(r.profile.down, right.profile.down, 0.025));
		CHECK(near(r.profile.up, right.profile.up, 0.025));
		for (int k = 0; k + 1 < PUSH_KNOTS; k++) CHECK(r.profile.done[k + 1] >= r.profile.done[k]);
		CHECK(near(PushDone(r.profile, 0.45f), 0.5, 0.04));
	}

	g_case = "feet: the stroke is where the foot travels";
	{
		// On the ground from 25% to 66%, travelling from 35% to 55%: before and after it stands
		Clip c = kClip;
		c.down = 0.25;
		c.go = 0.35;
		c.stop = 0.55;
		FootResult r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_OK);
		CHECK(r.lowFrom < 0.23);
		CHECK(r.lowTo > 0.66);
		CHECK(near(r.profile.down, 0.35 + 0.017, 0.022));
		CHECK(near(r.profile.up, 0.55 - 0.017, 0.022));
		CHECK(r.profile.down >= 0.35 - 1e-6);
		CHECK(r.profile.up <= 0.55 + 1e-6);
		CHECK(near(r.sweep, 28.0, 0.2));
		// It comes down 8 units behind where its stroke starts and reaches ahead along the
		// ground first, until 35%: the stroke is the way back, all 28 units of it
		c = kClip;
		c.down = 0.25;
		c.go = 0.35;
		c.start = 8.0;
		r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_OK);
		CHECK(near(r.sweep, 28.0, 0.2));
		CHECK(near(r.profile.down, 0.35 + 0.021, 0.022));
		CHECK(r.profile.down >= 0.35 - 1e-6);
		CHECK(near(r.profile.up, 0.60 - 0.021, 0.022));
		CHECK(r.profile.done[0] == 0.0f);
		CHECK(near(PushDone(r.profile, 0.475f), 0.5, 0.03));
	}

	g_case = "feet: a foot that goes back on itself";
	{
		// Half way through its stroke the foot comes 2 units back before it goes on. What it
		// has behind it does not get less for that: it stands at a half until the foot has
		// made up the 2 units.
		FootWatch w;
		for (int k = 0; k < 48; k++) {
			const double share = k / 48.0;
			float left[3], right[3];
			clipFeet(kClip, share, left, right, 0);
			const double u = (share - 0.30) / 0.30;
			if (u > 0.0 && u < 1.0) {
				const double gone = u < 0.4 ? 14.0 * u / 0.4 : (u < 0.55 ? 14.0 - 2.0 * (u - 0.4) / 0.15 : 12.0 + 16.0 * (u - 0.55) / 0.45);
				right[2] = (float)(5.0 - gone);
			}
			w.Sample((float)share, left, right);
		}
		FootResult r;
		w.End(0.8f, r);
		CHECK_EQ(r.why, FOOT_OK);
		CHECK(near(r.sweep, 28.0, 0.2));
		for (int k = 0; k + 1 < PUSH_KNOTS; k++) CHECK(r.profile.done[k + 1] >= r.profile.done[k]);
		const float standing = PushDone(r.profile, 0.30f + 0.30f * 0.50f);
		CHECK(near(standing, 0.5, 0.03));
		CHECK(near(PushDone(r.profile, 0.30f + 0.30f * 0.47f), standing, 0.005));
		CHECK(near(PushDone(r.profile, 0.30f + 0.30f * 0.55f), standing, 0.005));
		CHECK(PushDone(r.profile, 0.30f + 0.30f * 0.30f) < standing - 0.1f);
		CHECK(PushDone(r.profile, 0.30f + 0.30f * 0.75f) > standing + 0.1f);
	}

	g_case = "feet: what the game shows is what is watched";
	{
		// The game blends into the animation over 0.3 s. For that long the skater's foot is
		// not where the animation has it: it comes down a little later and not as far ahead.
		// That is the foot the player sees, and the one that pushes.
		const FootResult plain = watchClip(kClip, 60.0, 0.0, 1.0, 0);
		Clip c = kClip;
		c.blend = 0.3;
		const FootResult r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_OK);
		CHECK(r.profile.down > plain.profile.down);
		CHECK(r.profile.down < 0.375);
		CHECK(near(r.profile.up, plain.profile.up, 1e-6));
		CHECK(r.sweep < plain.sweep - 1.0);
		CHECK(r.sweep > 24.0);
		CHECK(near(r.drop, 4.5, 0.1));
	}

	g_case = "feet: animations that say nothing";
	{
		// both feet stay on the board
		Clip c = kClip;
		c.off = 2.0;
		FootResult r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_NO_DROP);
		CHECK(r.drop < 0.5);
		// a foot that only dips: a unit below the other is no foot on the ground
		// (a board is 4 to 5 units high)
		FootWatch dip;
		for (int k = 0; k < 48; k++) {
			const float y = 5.7f - (k >= 15 && k < 30 ? 1.0f : 0.0f);
			const float left[3] = { -3.0f, 5.7f, 9.0f };
			const float right[3] = { 3.0f, y, -9.0f + (k >= 15 && k < 30 ? (float)(k - 15) : 0.0f) };
			dip.Sample((float)(k / 48.0), left, right);
		}
		dip.End(0.8f, r);
		CHECK_EQ(r.why, FOOT_NO_DROP);
		CHECK(near(r.drop, 1.0, 0.01));
		// a foot that is put down and lifted again where it was
		c = kClip;
		c.ahead = 0.0;
		c.behind = 0.0;
		r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_NO_SWEEP);
		CHECK(near(r.drop, 4.5, 0.1));
		CHECK(r.sweep < 4.0);
		// one that travels 3 units down there, and one that travels 5
		c.ahead = 1.5;
		c.behind = 1.5;
		r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_NO_SWEEP);
		c.ahead = 2.5;
		c.behind = 2.5;
		r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_OK);
		CHECK(near(r.sweep, 5.0, 0.3));
		// too few looks
		int looks = 0;
		r = watchClip(kClip, 10.0, 0.0, 1.0, &looks);
		CHECK_EQ(looks, 8);
		CHECK_EQ(r.why, FOOT_FEW);
		CHECK_EQ(r.samples, 8);
		r = watchClip(kClip, 12.5, 0.0, 1.0, &looks);
		CHECK_EQ(looks, 10);
		CHECK_EQ(r.why, FOOT_OK);
		// not seen from the start, not seen to the end, a hole in the middle
		r = watchClip(kClip, 60.0, 0.22, 1.0, 0);
		CHECK_EQ(r.why, FOOT_PART);
		r = watchClip(kClip, 60.0, 0.18, 1.0, 0);
		CHECK_EQ(r.why, FOOT_OK);
		r = watchClip(kClip, 60.0, 0.0, 0.84, 0);
		CHECK_EQ(r.why, FOOT_PART);
		r = watchClip(kClip, 60.0, 0.0, 0.86, 0);
		CHECK_EQ(r.why, FOOT_OK);
		for (int hole = 0; hole < 2; hole++) {
			FootWatch w;
			for (int k = 0; k < 48; k++) {
				const double share = k / 48.0;
				if (share > 0.4 && share < (hole ? 0.63 : 0.57)) continue;
				float left[3], right[3];
				clipFeet(kClip, share, left, right, 0);
				w.Sample((float)share, left, right);
			}
			w.End(0.8f, r);
			CHECK_EQ(r.why, hole ? FOOT_PART : FOOT_OK);
		}
		// a foot that is at one end of its way on one look and at the other on the next
		c = kClip;
		c.go = 0.45;
		c.stop = 0.4501;
		r = watchClip(c, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_SHORT);
		CHECK(near(r.sweep, 28.0, 0.2));
		// and one that travels for 6% of an animation of half a second: three hundredths of a second
		c = kClip;
		c.length = 0.5;
		c.go = 0.40;
		c.stop = 0.46;
		r = watchClip(c, 240.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_SHORT);
		c.length = 1.2;
		r = watchClip(c, 240.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_OK);
		// nothing that was found wanting leaves a foot behind that pushes at once
		CHECK(near(r.profile.up - r.profile.down, 0.05, 0.012));
		// Nor is a fortieth of an animation a stroke, however long the animation is said to be
		c = kClip;
		c.length = 4.0;
		c.go = 0.45;
		c.stop = 0.48;
		r = watchClip(c, 240.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_SHORT);
		c.stop = 0.52;
		r = watchClip(c, 240.0, 0.0, 1.0, 0);
		CHECK_EQ(r.why, FOOT_OK);
		CHECK(r.profile.up - r.profile.down >= 0.04);
	}

	g_case = "feet: looks that are no looks";
	{
		FootWatch w;
		CHECK_EQ(w.Samples(), 0);
		const float l[3] = { -3.0f, 5.7f, 9.0f };
		const float r[3] = { 3.0f, 5.7f, -9.0f };
		w.Sample(0.5f, l, r);
		CHECK_EQ(w.Samples(), 1);
		w.Sample(0.5f, l, r);
		w.Sample(0.501f, l, r);
		CHECK_EQ(w.Samples(), 1);       // the same 128th of the animation: one look
		w.Sample(-0.1f, l, r);
		w.Sample(1.1f, l, r);
		w.Sample(nan, l, r);
		w.Sample(0.6f, 0, r);
		w.Sample(0.6f, l, 0);
		const float bad[3] = { 1.0f, nan, 2.0f };
		const float worse[3] = { 1.0f, 2.0f, HUGE_VALF };
		w.Sample(0.6f, bad, r);
		w.Sample(0.6f, l, bad);
		w.Sample(0.6f, worse, r);
		w.Sample(0.6f, l, worse);
		CHECK_EQ(w.Samples(), 1);
		w.Sample(1.0f, l, r);           // the very end is a look, and so is the very start
		w.Sample(0.0f, l, r);
		CHECK_EQ(w.Samples(), 3);
		FootResult out;
		w.End(0.8f, out);
		CHECK_EQ(out.why, FOOT_FEW);
		CHECK_EQ(out.samples, 3);
		w.Begin();
		CHECK_EQ(w.Samples(), 0);
		w.End(0.8f, out);
		CHECK_EQ(out.why, FOOT_FEW);
		CHECK_EQ(out.samples, 0);
		// Of two looks in the same 128th the later one counts: an animation whose feet never
		// leave the board, and then the real one over it
		Clip still = kClip;
		still.off = 2.0;
		FootWatch twice;
		for (int pass = 0; pass < 2; pass++) {
			for (int k = 0; k < 48; k++) {
				float left[3], right[3];
				clipFeet(pass ? kClip : still, k / 48.0, left, right, 0);
				twice.Sample((float)(k / 48.0), left, right);
			}
		}
		twice.End(0.8f, out);
		const FootResult plain = watchClip(kClip, 60.0, 0.0, 1.0, 0);
		CHECK_EQ(out.why, FOOT_OK);
		CHECK(near(out.profile.down, plain.profile.down, 1e-6));
		CHECK(near(out.sweep, plain.sweep, 1e-5));
		// and after Begin nothing of the last animation is left
		twice.Begin();
		for (int k = 0; k < 48; k += 4) {
			float left[3], right[3];
			clipFeet(kClip, k / 48.0, left, right, 0);
			twice.Sample((float)(k / 48.0), left, right);
		}
		CHECK_EQ(twice.Samples(), 12);
	}

	g_case = "feet: watched, then pushed with";
	{
		// The whole of it: the game blends into its animation, the feet are watched through
		// one play, and the next push goes by what they did. The speed comes while the foot
		// the player sees travels.
		Clip c = kClip;
		c.blend = 0.3;
		const FootResult r = watchClip(c, 60.0, 0.0, 1.0, 0);
		const Press once[] = { { 0.0, 0.5 } };
		const PushLog log = runPush(60.0, 3.0, 100.0, once, 1, 100, r.profile, 0, kGame);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.added[0], 320.0, 0.05));
		CHECK(log.firstShare[0] > r.profile.down && log.firstShare[0] <= r.profile.down + 1.0 / 48.0 + 1e-6);
		CHECK(log.lastShare[0] >= r.profile.up - 1e-6 && log.lastShare[0] < r.profile.up + 1.0 / 48.0);
		CHECK(log.worst < 0.01);
	}
}

static void testCoasting() {
	const double unit = 80.0;

	g_case = "rolling: what the board loses";
	{
		// Skate: the wheels take 0.13 m/s2 at a crawl and 0.5 from 8 m/s on; above 8.1 m/s the
		// air takes more
		for (int i = 0; i <= 3000; i++) {
			const double v = i * 0.01;
			CHECK(near(CoastLoss((float)v), at(kSkateWheels, v) + at(kSkateAir, v), 0.001));
		}
		CHECK(near(CoastLoss(0.0f), 0.1286, 0.001));
		CHECK(near(CoastLoss(8.0f), 0.5, 0.001));
		CHECK(near(CoastLoss(12.8575f), 1.1, 0.002));
		CHECK(near(CoastLoss(40.0f), 1.7, 0.001));
		CHECK(CoastLoss(-5.0f) == CoastLoss(5.0f));
		// the faster, the more
		float before = 0.0f;
		for (int i = 0; i <= 3000; i++) {
			const float loss = CoastLoss(i * 0.01f);
			CHECK(loss >= before);
			before = loss;
		}
		// in the game's units: 40 units/s2 at 700, where THUG's own drag takes 294 standing
		CHECK(near(CoastLoss(700.0f / (float)unit) * unit, 40.7, 0.2));
	}

	g_case = "rolling: rolling out";
	{
		// From 8 m/s the board rolls for half a minute and 87 m; from 4 m/s for 21 s and 35 m
		const double rates[] = { 30.0, 60.0, 144.0, 240.0 };
		for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); r++) {
			const double dt = 1.0 / rates[r];
			const double from[] = { 8.0, 4.0 };
			const double wantTime[] = { 29.7, 20.9 };
			const double wantWay[] = { 86.7, 34.5 };
			for (int k = 0; k < 2; k++) {
				double speed = from[k] * unit, seconds = 0.0, way = 0.0;
				for (int frame = 0; frame < 1000000 && speed > 0.0; frame++) {
					way += speed * dt;
					const float next = Coast((float)speed, (float)dt, 100);
					CHECK(next < speed);
					CHECK(next >= 0.0f);
					speed = next;
					seconds += dt;
				}
				CHECK(near(seconds, wantTime[k], 0.3));
				CHECK(near(way / unit, wantWay[k], 1.0));
			}
		}
		// half of Skate's is half, twice is twice, none is none
		CHECK(near(640.0 - Coast(640.0f, 0.01f, 50), 0.5 * (640.0 - Coast(640.0f, 0.01f, 100)), 1e-3));
		CHECK(near(640.0 - Coast(640.0f, 0.01f, 200), 2.0 * (640.0 - Coast(640.0f, 0.01f, 100)), 1e-3));
		CHECK(Coast(640.0f, 0.01f, 0) == 640.0f);
		CHECK(Coast(640.0f, 0.01f, -4) == 640.0f);
		// in one frame at 60 Hz, from 640 units/s: half a metre per second squared, 0.67 units/s
		CHECK(near(640.0 - Coast(640.0f, 1.0f / 60.0f, 100), 0.5 * unit / 60.0, 0.01));
		// a board that stands, stands; no time, no loss
		CHECK(Coast(0.0f, 0.01f, 100) == 0.0f);
		CHECK(Coast(640.0f, 0.0f, 100) == 640.0f);
		CHECK(Coast(640.0f, -1.0f, 100) == 640.0f);
		// the last of the speed goes in one frame, not below nothing
		CHECK(Coast(0.01f, 1.0f, 100) == 0.0f);
	}
}

static void testSteering() {
	const double unit = 80.0;
	const double g = 9.8;

	g_case = "rolling: how fast the board turns";
	{
		// no stick, no turn
		for (int speed = 0; speed <= 2000; speed += 10) {
			CHECK(TurnRate(0.0f, (float)speed, 100) == 0.0f);
			CHECK(TurnRate(0.05f, (float)speed, 100) == 0.0f);
		}
		// standing still: THUG's own turn on the spot
		CHECK(near(TurnRate(1.0f, 0.0f, 100), kThugGroundTurn, 1e-3));
		// It fades out as the board gets going: at 20 units/s four fifths of it are left, and by
		// 50 the trucks have taken over (0.625 m/s on a circle of 0.44 m)
		CHECK(near(TurnRate(1.0f, 20.0f, 100), 0.8 * kThugGroundTurn, 1e-3));
		CHECK(near(TurnRate(1.0f, 50.0f, 100), 0.625 * tan(0.6 * 0.834459424) / 0.243, 0.005));
		CHECK(near(TurnRate(1.0f, 99.0f, 100), 1.2375 * tan(0.6 * 0.834459424) / 0.243, 0.005));
		// at a walk the board goes where its trucks point: full stick is 0.5 radians at the
		// trucks, which sit 0.243 m from the middle
		const double full = 0.6 * 0.834459424;
		CHECK(near(TurnRate(1.0f, 1.0f * (float)unit, 100), 1.0 * tan(full) / 0.243, 0.01));
		CHECK(near(TurnRate(1.0f, 1.0f * (float)unit, 100), 2.25, 0.01));
		// half the stick is a fifth of that angle
		CHECK(near(at(kSkateStick, 0.5) / 0.834459424, 0.2, 0.01));
		CHECK(near(TurnRate(0.5f, 1.0f * (float)unit, 100), 1.0 * tan(0.6 * at(kSkateStick, 0.5)) / 0.243, 0.01));
		// at speed the wheels' grip is the limit: 0.8 g sideways, so 0.92 rad/s at 8.5 m/s, where
		// THUG turns at 1.8
		CHECK(near(kTurnGrip, 0.8, 1e-6));
		CHECK(near(TurnRate(1.0f, 680.0f, 100), 0.8 * g / 8.5, 0.002));
		CHECK(near(TurnRate(1.0f, 680.0f, 100), 0.922, 0.002));
		CHECK(near(TurnRate(1.0f, 900.0f, 100), 0.697, 0.002));
		// the two are level at about 350 units/s; below it Skate's board turns the faster one
		CHECK(TurnRate(1.0f, 340.0f, 100) > kThugGroundTurn);
		CHECK(TurnRate(1.0f, 360.0f, 100) < kThugGroundTurn);
		// what the stick asks for, as Skate has it: trucks first, grip last
		for (int speed = 100; speed <= 2000; speed += 7) {
			for (int i = 0; i <= 20; i++) {
				const double stick = i / 20.0;
				const double v = speed / unit;
				double share = v / 50.0;
				if (share > 1.0) share = 1.0;
				const double angle = 0.6 * at(kSkateStick, stick) * at(kSkateTruckSpeed, share);
				double want = v * tan(angle) / 0.243;
				if (want > 0.8 * g / v) want = 0.8 * g / v;
				CHECK(near(TurnRate((float)stick, (float)speed, 100), want, 0.004));
				// to the side the board never pulls more than the wheels hold
				CHECK(TurnRate((float)stick, (float)speed, 100) * speed <= 0.8 * g * unit * 1.001);
			}
		}
		// more stick never turns less; left and right are the same
		for (int speed = 0; speed <= 1500; speed += 25) {
			float before = 0.0f;
			for (int i = 0; i <= 100; i++) {
				const float rate = TurnRate(i * 0.01f, (float)speed, 100);
				CHECK(rate >= before - 1e-5f);
				CHECK(rate == TurnRate(-i * 0.01f, (float)speed, 100));
				before = rate;
			}
		}
		// no jump anywhere along the speed, the turn on the spot included
		for (int i = 0; i <= 4; i++) {
			const float stick = 0.2f + 0.2f * i;
			float before = TurnRate(stick, 0.0f, 100);
			for (int speed = 1; speed <= 1500; speed++) {
				const float rate = TurnRate(stick, (float)speed, 100);
				CHECK(fabs(rate - before) < 0.06f);
				before = rate;
			}
		}
		// the setting is the grip: twice the grip turns twice as fast at speed, and changes
		// nothing where the trucks are the limit
		CHECK(near(TurnRate(1.0f, 680.0f, 200), 2.0 * TurnRate(1.0f, 680.0f, 100), 1e-3));
		CHECK(near(TurnRate(1.0f, 680.0f, 50), 0.5 * TurnRate(1.0f, 680.0f, 100), 1e-3));
		CHECK(near(TurnRate(1.0f, 80.0f, 200), TurnRate(1.0f, 80.0f, 100), 1e-4));
		CHECK(TurnRate(1.0f, 680.0f, 0) == 0.0f);
		CHECK(TurnRate(1.0f, 50.0f, 0) == 0.0f);
		CHECK(TurnRate(1.0f, 0.0f, 0) == 0.0f);
		CHECK(TurnRate(1.0f, 0.0f, -100) == 0.0f);
		// with twice the grip the board turns at speed about as THUG's does
		CHECK(near(TurnRate(1.0f, 680.0f, 200), kThugGroundTurn, 0.1));
	}

	g_case = "rolling: the trucks follow the stick";
	{
		// Full stick to the right at 3 m/s: the trucks get there in about a fifth of a second
		const double rates[] = { 30.0, 60.0, 144.0, 240.0 };
		double at100[4];
		for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); r++) {
			Steer steer;
			const float dt = (float)(1.0 / rates[r]);
			const float speed = 240.0f;
			const double settled = 0.6 * 0.834459424 * at(kSkateTruckSpeed, 3.0 / 50.0);
			double t = 0.0;
			at100[r] = 0.0;
			float rate = 0.0f;
			while (t < 1.0 - 1e-9) {
				rate = steer.Step(dt, 1.0f, speed, false, 100);
				t += dt;
				CHECK(rate >= 0.0f);
				CHECK(steer.Angle() >= 0.0f && steer.Angle() <= settled + 1e-4);
				if (at100[r] == 0.0 && t >= 0.1 - 1e-9) at100[r] = steer.Angle();
			}
			CHECK(near(steer.Angle(), settled, 0.002));
			CHECK(near(rate, TurnRate(1.0f, speed, 100), 0.01));
			// a tenth of a second in: two thirds to three quarters of the way
			CHECK(at100[r] > 0.6 * settled && at100[r] < 0.78 * settled);
		}
		// and the same way at every frame rate
		for (int r = 1; r < 4; r++) {
			CHECK(near(at100[r], at100[0], 0.03 * at100[0]));
		}
		// left is the other way
		Steer left;
		float rate = 0.0f;
		for (int frame = 0; frame < 120; frame++) rate = left.Step(1.0f / 60.0f, -1.0f, 240.0f, false, 100);
		CHECK(near(rate, -TurnRate(1.0f, 240.0f, 100), 0.01));
		CHECK(left.Angle() < 0.0f);
		// let go: the board straightens out
		for (int frame = 0; frame < 120; frame++) rate = left.Step(1.0f / 60.0f, 0.0f, 240.0f, false, 100);
		CHECK(fabs(rate) < 0.01f);
		// Reset: straight at once
		Steer reset;
		for (int frame = 0; frame < 60; frame++) reset.Step(1.0f / 60.0f, 1.0f, 240.0f, false, 100);
		reset.Reset();
		CHECK(reset.Angle() == 0.0f);
		CHECK(reset.Step(1.0f / 60.0f, 0.0f, 240.0f, false, 100) == 0.0f);
	}

	g_case = "rolling: a pushing foot takes turning away";
	{
		// Skate turns the trucks half as far while a foot is on the ground. It gets there by
		// 0.015 a tick, and comes back by 0.05 a tick.
		Steer steer;
		const float dt = 1.0f / 60.0f;
		const float speed = 240.0f;
		for (int frame = 0; frame < 120; frame++) steer.Step(dt, 1.0f, speed, false, 100);
		const float freeAngle = steer.Angle();
		for (int frame = 0; frame < 15; frame++) steer.Step(dt, 1.0f, speed, true, 100);   // a foot down for a quarter second
		CHECK(steer.Angle() < freeAngle);
		CHECK(steer.Angle() > 0.75f * freeAngle);
		for (int frame = 0; frame < 120; frame++) steer.Step(dt, 1.0f, speed, true, 100);
		CHECK(near(steer.Angle(), 0.5 * freeAngle, 0.002));
		for (int frame = 0; frame < 60; frame++) steer.Step(dt, 1.0f, speed, false, 100);
		CHECK(near(steer.Angle(), freeAngle, 0.002));
	}

	g_case = "rolling: odd frames and odd sticks";
	{
		Steer steer;
		const float nan = (float)sqrt(-1.0);
		CHECK(steer.Step(0.0f, 1.0f, 240.0f, false, 100) == 0.0f);
		CHECK(steer.Step(-1.0f, 1.0f, 240.0f, false, 100) == 0.0f);
		CHECK(steer.Step(nan, 1.0f, 240.0f, false, 100) == 0.0f);
		CHECK(steer.Angle() == 0.0f);
		CHECK(steer.Step(1.0f / 60.0f, nan, 240.0f, false, 100) == 0.0f);
		CHECK(steer.Step(1.0f / 60.0f, 1.0f, 240.0f, false, 0) == 0.0f);
		CHECK(steer.Angle() == 0.0f);
		// and after a stick that was not a number the board still steers
		float after = 0.0f;
		for (int frame = 0; frame < 120; frame++) after = steer.Step(1.0f / 60.0f, 1.0f, 240.0f, false, 100);
		CHECK(near(after, TurnRate(1.0f, 240.0f, 100), 0.01));
		// a stick beyond its end is its end; backwards speed is speed
		Steer far, end;
		float a = 0.0f, b = 0.0f;
		for (int frame = 0; frame < 120; frame++) {
			a = far.Step(1.0f / 60.0f, 7.0f, -240.0f, false, 100);
			b = end.Step(1.0f / 60.0f, 1.0f, 240.0f, false, 100);
		}
		CHECK(a == b);
		// a frame that took seconds counts as a tenth of one, and gives a number
		Steer hitch;
		const float rate = hitch.Step(30.0f, 1.0f, 240.0f, false, 100);
		CHECK(rate == rate);
		CHECK(rate >= 0.0f && rate <= TurnRate(1.0f, 240.0f, 100) + 1e-4f);
		// six ticks' worth: the trucks are most of the way, not all of it
		const double settled = 0.6 * 0.834459424 * at(kSkateTruckSpeed, 3.0 / 50.0);
		CHECK(hitch.Angle() > 0.6 * settled && hitch.Angle() < 0.8 * settled);
	}
}

// ---------------------------------------------------------------------------------------------
// Spinning in the air
// ---------------------------------------------------------------------------------------------

// Skate 3's curves (physics_bodyspin, the set its normal difficulty uses), to the last digit
static const Table kSkateSpin = { { 0, 0.0521172583, 0.127035797, 0.244299695, 0.368078202, 0.563517928, 0.905537486, 2 },
	{ 3.1500001, 6.36428499, 7.77857113, 7.93928623, 7.7142849, 6.9749999, 5.6892848, 3.6642859 } };
static const Table kSkateSpinStep = { { 0, 0.115635201, 0.25570029, 0.394136786, 0.532573283, 0.677524388, 0.877850175, 1 },
	{ 0.992856979, 0.760714471, 0.507143021, 0.349999994, 0.224999994, 0.171428606, 0.157142907, 0.153571397 } };
static const Table kSkateFlick = { { -0.496742696, -0.374592811, -0.254071712, 0, 0.135179207, 0.228012994, 0.337133497, 0.5 },
	{ 0, 0.0500000007, 1, 1, 1, 0.62857151, 0.385714293, 0.285714298 } };

// The stick over time, the board leaves the ground at 0. -1 (left) to 1 (right).
typedef double (*StickAt)(double seconds);

// Skate's rules, written down a second time and as plainly as they read in its code: one pass a
// tick, the ticks before the board leaves the ground only watch the stick. Returns the rate of
// every tick in the air (rad/s, to the right when positive), and the flick that counted.
static std::vector<double> skateSpin(StickAt stickAt, double lead, double air, double* flickOut) {
	const double tick = 1.0 / 60.0;
	double history[30];
	for (int i = 0; i < 30; i++) history[i] = 0.0;
	int index = 0;
	double last = stickAt(-lead - tick);
	double smooth = last;
	double rate = 0.0, peak = 0.0, time = 0.0;
	std::vector<double> rates;
	const int before = (int)(lead / tick + 0.5), during = (int)(air / tick + 0.5);
	for (int k = -before; k < during; k++) {
		const double stick = stickAt(k * tick);
		const double moved = (stick - last) * 1.5;
		last = stick;
		double flick = 0.0;
		if (moved * stick > 0.1) flick = moved < -1.0 ? -1.0 : (moved > 1.0 ? 1.0 : moved);
		history[index] = flick;
		index = (index + 1) % 30;
		if (k < 0) {
			time = 0.0;
			rate = 0.0;
			smooth = 0.8 * smooth + 0.2 * stick;
			continue;
		}
		if (time == 0.0) {
			peak = 0.0;
			double when = 0.0;
			int i = index;
			for (int n = 0; n < 29; n++) {
				i = (i + 29) % 30;
				when -= tick;
				const double counts = at(kSkateFlick, when) * history[i];
				if (fabs(counts) > fabs(peak)) peak = counts;
			}
		}
		const double counts = at(kSkateFlick, time) * flick;
		if (fabs(counts) > fabs(peak)) peak = counts;
		const double was = time;
		time += tick;
		double input = stick;
		if (fabs(stick) < 1.52588e-05) {
			smooth *= 0.96;
			input = smooth;
			last = smooth;
		}
		else {
			smooth = 0.8 * smooth + 0.2 * stick;
		}
		const double target = at(kSkateSpin, time) * input;
		if (target * peak < 0.0) peak = 0.0;
		const double wanted = -(0.4 + 0.6 * fabs(peak)) * target;
		const double step = at(kSkateSpinStep, was);
		const double slow = step < 0.2 ? step : 0.2;
		const double up = rate > 0.0 ? step : slow;
		const double down = rate > 0.0 ? -slow : -step;
		double change = wanted - rate;
		if (change > up) change = up;
		if (change < down) change = down;
		rate += change;
		rates.push_back(-rate);
	}
	if (flickOut) *flickOut = fabs(peak);
	return rates;
}

// The same air through the code under test, at any frame rate: the frames before the board
// leaves the ground are fed, the ones after are stepped. Returns the degrees turned.
struct SpinLog {
	double degrees;     // turned, to the right when positive
	double flick;
	double most;        // the fastest it turned, rad/s
	std::vector<double> rates;   // of every frame in the air
};

static SpinLog runSpin(double fps, StickAt stickAt, double lead, double air, int percent) {
	Spin spin;
	SpinLog log;
	log.degrees = 0.0;
	log.most = 0.0;
	const double dt = 1.0 / fps;
	const int before = (int)(lead * fps + 0.5), during = (int)(air * fps + 0.5);
	// the stick has been where it starts for a long time
	for (int k = 0; k < 120; k++) spin.Feed((float)dt, (float)stickAt(-lead - dt));
	for (int k = -before; k < during; k++) {
		const double stick = stickAt(k * dt);
		if (k < 0) {
			spin.Feed((float)dt, (float)stick);
			CHECK(!spin.InAir());
			continue;
		}
		const float rate = spin.Step((float)dt, (float)stick, percent);
		CHECK(rate == rate);
		log.rates.push_back(rate);
		log.degrees += rate * dt * 57.29577951;
		if (fabs(rate) > log.most) log.most = fabs(rate);
	}
	log.flick = spin.Flick();
	CHECK(near(spin.Turned() * 57.29577951, log.degrees, 0.05 + 0.001 * fabs(log.degrees)));
	return log;
}

static double spinDegrees(const std::vector<double>& rates) {
	double sum = 0.0;
	for (size_t i = 0; i < rates.size(); i++) sum += rates[i] / 60.0 * 57.29577951;
	return sum;
}

static double stickHeld(double) { return 1.0; }
static double stickThrown(double t) { return t >= 0.0 ? 1.0 : 0.0; }
static double stickThrownLeft(double t) { return t >= 0.0 ? -1.0 : 0.0; }
static double stickHeldLeft(double) { return -1.0; }
static double stickThrownAt100Before(double t) { return t >= -0.1 ? 1.0 : 0.0; }
static double stickThrownAt300Before(double t) { return t >= -0.3 ? 1.0 : 0.0; }
static double stickThrownAt450Before(double t) { return t >= -0.45 ? 1.0 : 0.0; }
static double stickThrownAt100After(double t) { return t >= 0.1 ? 1.0 : 0.0; }
static double stickThrownAt300After(double t) { return t >= 0.3 ? 1.0 : 0.0; }
static double stickOverIn50(double t) { return t >= 0.0 ? 1.0 : (t <= -0.05 ? 0.0 : (t + 0.05) / 0.05); }
static double stickOverIn100(double t) { return t >= 0.0 ? 1.0 : (t <= -0.1 ? 0.0 : (t + 0.1) / 0.1); }
static double stickOverIn200(double t) { return t >= 0.0 ? 1.0 : (t <= -0.2 ? 0.0 : (t + 0.2) / 0.2); }
static double stickHalf(double) { return 0.5; }
static double stickHalfThrown(double t) { return t >= 0.0 ? 0.5 : 0.0; }
static double stickLetGoAt200(double t) { return t >= 0.0 && t < 0.2 ? 1.0 : 0.0; }
static double stickLetGoAt400(double t) { return t >= 0.0 && t < 0.4 ? 1.0 : 0.0; }
static double stickLeftThenRight(double t) { return t < -0.05 ? 0.0 : (t < 0.0 ? -1.0 : 1.0); }
static double stickNone(double) { return 0.0; }
// held on the ground, let go as the board leaves it
static double stickLetGoAtTakeoff(double t) { return t < 0.0 ? 1.0 : 0.0; }
// held, let go a tenth of a second into the air, thrown over again a tenth later
static double stickHeldLetGoThrown(double t) { return t < 0.1 ? 1.0 : (t < 0.2 ? 0.0 : 1.0); }
// the same, the second throw between two ticks
static double stickHeldLetGoThrownBetween(double t) { return t < 0.1 ? 1.0 : (t < 0.205 ? 0.0 : 1.0); }
// held on the ground, let go for three ticks, thrown over again as the board leaves it
static double stickReleasedThenThrown(double t) { return t < -0.05 ? 1.0 : (t < 0.0 ? 0.0 : 1.0); }
// thrown to the right as the board leaves the ground, to the left 0.3 s later
static double stickRightThenLeftInAir(double t) { return t < 0.0 ? 0.0 : (t < 0.3 ? 1.0 : -1.0); }
static double stickLeftThenRightInAir(double t) { return -stickRightThenLeftInAir(t); }
// held to the left, pulled most of the way in at once 0.2 s before the board leaves the ground,
// then pushed slowly across to the right
static double stickInThenAcross(double t) { return t < -0.2 ? -1.0 : (t >= 0.0 ? 1.0 : -0.1 + 1.1 * (t + 0.2) / 0.2); }
// thrown to the left 0.3 s before, then pushed slowly across to the right
static double stickLeftFlickThenSlowRight(double t) { return t < -0.3 ? 0.0 : (t >= 0.0 ? 1.0 : -1.0 + 2.0 * (t + 0.3) / 0.3); }
// a small slow move near the centre, then held there
static double stickSmallSlow(double t) { return t < -0.05 ? 0.0 : (t >= 0.0 ? 0.3 : 0.3 * (t + 0.05) / 0.05); }
// thrown over 28 and 29 ticks before the board leaves the ground
static double stickThrown28Before(double t) { return t >= -28.0 / 60.0 - 1e-6 ? 1.0 : 0.0; }
static double stickThrown29Before(double t) { return t >= -29.0 / 60.0 - 1e-6 ? 1.0 : 0.0; }

static void testSpinning() {
	g_case = "spinning: the setting";
	{
		Settings s;
		SetDefaults(s);
		CHECK_EQ(s.spin, 100);
		Sanitize(s);
		CHECK_EQ(s.spin, 100);
		s.spin = 0;
		Sanitize(s);
		CHECK_EQ(s.spin, 0);
		s.spin = -7;
		Sanitize(s);
		CHECK_EQ(s.spin, 0);
		s.spin = 3;
		Sanitize(s);
		CHECK_EQ(s.spin, 25);
		s.spin = 99999;
		Sanitize(s);
		CHECK_EQ(s.spin, 400);
		CHECK_EQ(kSpinMostPercent, 400);
	}

	g_case = "spinning: Skate's rate";
	{
		// With the stick at its end after a full flick: 3.15 rad/s as the board leaves the ground,
		// 7.9 a quarter of a second later, 5.7 at 0.9 s, 3.7 from 2 s on
		CHECK(near(SpinRate(0.0f), 3.15, 1e-3));
		CHECK(near(SpinRate(-1.0f), 3.15, 1e-3));
		CHECK(near(SpinRate(0.2443f), 7.939, 2e-3));
		CHECK(near(SpinRate(0.9055f), 5.689, 2e-3));
		CHECK(near(SpinRate(2.0f), 3.664, 1e-3));
		CHECK(near(SpinRate(30.0f), 3.664, 1e-3));
		for (int i = 0; i <= 250; i++) {
			const double t = i * 0.01;
			CHECK(near(SpinRate((float)t), at(kSkateSpin, t), 0.01));
		}
		// a stick that is only held gets two fifths of it
		CHECK(near(kSpinLeast, 0.4, 1e-6));
		// THUG turns faster than Skate's best from a quarter of a second on at the latest,
		// whatever the stats
		CHECK(kThugAirSpinWorst > SpinRate(1.0f));
		CHECK(kThugAirSpinBest < SpinRate(0.2443f));
	}

	g_case = "spinning: tick for tick against Skate's rules";
	{
		// (to the right: to the left Skate itself is a tick late, see "left and right" below)
		const StickAt sticks[] = { stickHeld, stickThrown, stickThrownAt100Before, stickThrownAt300Before,
			stickThrownAt450Before, stickThrownAt100After, stickThrownAt300After, stickOverIn50, stickOverIn100, stickOverIn200,
			stickHalf, stickHalfThrown, stickLetGoAt200, stickLetGoAt400, stickLeftThenRight, stickNone,
			stickLetGoAtTakeoff, stickHeldLetGoThrown, stickRightThenLeftInAir, stickInThenAcross, stickLeftFlickThenSlowRight,
			stickSmallSlow, stickThrown28Before, stickThrown29Before, stickReleasedThenThrown };
		for (size_t n = 0; n < sizeof(sticks) / sizeof(sticks[0]); n++) {
			double flick = 0.0;
			const std::vector<double> want = skateSpin(sticks[n], 1.0, 3.0, &flick);
			const SpinLog got = runSpin(60.0, sticks[n], 1.0, 3.0, 100);
			CHECK_EQ(got.rates.size(), want.size());
			double worst = 0.0;
			for (size_t i = 0; i < want.size() && i < got.rates.size(); i++) {
				const double off = fabs(got.rates[i] - want[i]);
				if (off > worst) worst = off;
			}
			// the curves in the code are rounded to four places
			CHECK(worst < 0.004);
			CHECK(near(got.flick, flick, 0.002));
			CHECK(near(got.degrees, spinDegrees(want), 0.5));
		}
	}

	g_case = "spinning: how far a skater turns";
	{
		// Degrees in an air of 0.56 s (Skate's quick pop), 0.79 s (its full one), 1 s and 2 s
		const double airs[] = { 0.56, 0.79, 1.0, 2.0 };
		struct Row { StickAt stick; double flick; double degrees[4]; };
		const Row rows[] = {
			{ stickHeld, 0.0, { 94, 127, 155, 260 } },                 // the stick held all along
			{ stickThrown, 1.0, { 220, 301, 372, 635 } },              // thrown over as the board leaves the ground
			{ stickThrownAt100Before, 1.0, { 220, 301, 372, 635 } },   // a tenth of a second before: the same
			{ stickThrownAt300Before, 0.51, { 162, 219, 269, 453 } },  // 0.3 s before: half a flick
			{ stickThrownAt450Before, 0.01, { 96, 129, 158, 265 } },   // 0.45 s before: none to speak of
			{ stickOverIn50, 0.5, { 161, 217, 267, 451 } },            // pushed over in a twentieth of a second
			{ stickOverIn100, 0.25, { 128, 173, 212, 356 } },
			{ stickOverIn200, 0.125, { 112, 150, 184, 308 } },
			{ stickThrownAt100After, 1.0, { 164, 245, 316, 579 } },    // thrown over in the air: later, but as fast
			{ stickThrownAt300After, 0.47, { 47, 102, 151, 329 } },
			{ stickHalf, 0.0, { 48, 64, 78, 131 } },                   // half the stick is half the turning
			{ stickHalfThrown, 0.75, { 100, 135, 165, 276 } },
			{ stickLetGoAt200, 1.0, { 179, 206, 218, 232 } },          // let go: the turning runs out
			{ stickLetGoAt400, 1.0, { 213, 266, 292, 316 } },
		};
		for (size_t r = 0; r < sizeof(rows) / sizeof(rows[0]); r++) {
			for (int a = 0; a < 4; a++) {
				const SpinLog log = runSpin(60.0, rows[r].stick, 1.0, airs[a], 100);
				CHECK(near(log.degrees, rows[r].degrees[a], 1.5));
				CHECK(near(log.flick, rows[r].flick, 0.01));
				CHECK(log.most <= 7.94 + 0.01);
			}
		}
		// What THUG does with the same airs, with Left or Right held from the start: nothing for
		// 0.1 s, then 6.85 to 7.75 rad/s. In the short air Skate's best is THUG's, in a long one
		// it is not.
		const double thugShort = kThugAirSpinWorst * (0.56 - kThugAirSpinWait) * 57.29577951;
		const double thugLong = kThugAirSpinWorst * (2.0 - kThugAirSpinWait) * 57.29577951;
		CHECK(near(thugShort, 180.5, 1.0));
		CHECK(near(thugLong, 745.7, 1.0));
		CHECK(runSpin(60.0, stickThrown, 1.0, 0.56, 100).degrees > thugShort);
		CHECK(runSpin(60.0, stickThrown, 1.0, 2.0, 100).degrees < thugLong);
		CHECK(runSpin(60.0, stickHeld, 1.0, 0.56, 100).degrees < 0.6 * thugShort);
		// no stick, no turning
		const SpinLog none = runSpin(60.0, stickNone, 1.0, 2.0, 100);
		CHECK(none.degrees == 0.0);
		CHECK(none.most == 0.0);
	}

	g_case = "spinning: what counts as a flick";
	{
		// A stick that was held on the ground and is let go as the board leaves it: the turning
		// runs on from where the stick was, a little less each tick. A sixth of a turn, no more.
		const SpinLog letGo = runSpin(60.0, stickLetGoAtTakeoff, 1.0, 2.0, 100);
		CHECK(near(letGo.flick, 0.0, 1e-3));
		CHECK(letGo.degrees > 40.0 && letGo.degrees < 75.0);
		CHECK(near(letGo.degrees, spinDegrees(skateSpin(stickLetGoAtTakeoff, 1.0, 2.0, 0)), 0.5));
		// Let go in the air and thrown over again a tenth of a second later: the stick counts
		// as having been where the turning still was, so the throw is a small flick
		const SpinLog again = runSpin(60.0, stickHeldLetGoThrown, 1.0, 1.0, 100);
		double want = 0.0;
		skateSpin(stickHeldLetGoThrown, 1.0, 1.0, &want);
		CHECK(near(again.flick, want, 0.005));
		CHECK(again.flick > 0.1 && again.flick < 0.45);
		// On the ground a stick that is let go is simply at rest: thrown over again from there it
		// is a full flick, whatever it did before
		const SpinLog rethrown = runSpin(60.0, stickReleasedThenThrown, 1.0, 0.79, 100);
		CHECK(near(rethrown.flick, 1.0, 1e-3));
		CHECK(near(runSpin(1000.0, stickReleasedThenThrown, 1.0, 0.79, 100).flick, 1.0, 0.01));
		// the same when the throw comes between two ticks, at a frame rate that has frames there
		const SpinLog between = runSpin(1000.0, stickHeldLetGoThrownBetween, 1.0, 1.0, 100);
		CHECK(near(between.flick, again.flick, 0.05));
		const SpinLog between240 = runSpin(240.0, stickHeldLetGoThrownBetween, 1.0, 1.0, 100);
		CHECK(near(between240.flick, again.flick, 0.05));
		// A small slow move near the centre is none: the way the stick went times where it got
		// to has to come to something
		const SpinLog small = runSpin(60.0, stickSmallSlow, 1.0, 0.79, 100);
		CHECK(small.flick == 0.0);
		CHECK(near(small.degrees, 0.3 * runSpin(60.0, stickHeld, 1.0, 0.79, 100).degrees, 6.0));
		// A stick that is pulled in is not flicked, however fast. Pulled in from the left and
		// then pushed slowly over to the right: the slow push is all there is.
		const SpinLog across = runSpin(60.0, stickInThenAcross, 1.0, 0.79, 100);
		CHECK(across.degrees > 0.0);
		CHECK(across.flick < 0.3);
		// Thrown to the left and then pushed slowly over to the right: the throw went the other
		// way and does not count
		const SpinLog crossed = runSpin(60.0, stickLeftFlickThenSlowRight, 1.0, 0.79, 100);
		CHECK(crossed.degrees > 0.0);
		CHECK(crossed.flick < 0.3);
		CHECK(crossed.degrees < 1.3 * runSpin(60.0, stickHeld, 1.0, 0.79, 100).degrees);
		// How far back a flick counts: the tick the board leaves the ground on and the 28
		// before it, the last of them for next to nothing
		const SpinLog far = runSpin(60.0, stickThrown28Before, 1.0, 0.3, 100);
		CHECK(far.flick > 0.003 && far.flick < 0.008);
		const SpinLog further = runSpin(60.0, stickThrown29Before, 1.0, 0.3, 100);
		CHECK(further.flick == 0.0);
		// To the left as to the right, also with the stick thrown the other way in the air: the
		// turning comes down by a fifth of a rad/s a tick and then goes the other way
		const SpinLog there = runSpin(60.0, stickRightThenLeftInAir, 1.0, 1.5, 100);
		const SpinLog back = runSpin(60.0, stickLeftThenRightInAir, 1.0, 1.5, 100);
		CHECK_EQ(there.rates.size(), back.rates.size());
		for (size_t i = 0; i < there.rates.size() && i < back.rates.size(); i++) {
			CHECK(near(there.rates[i], -back.rates[i], 1e-5));
			if (i > 0) {
				// from the throw at 0.3 s until the turning has stopped
				const bool slowing = there.rates[i] >= 0.0 && there.rates[i] < there.rates[i - 1];
				if (slowing && i > 20) CHECK(there.rates[i - 1] - there.rates[i] <= 0.2 + 1e-4);
			}
		}
		CHECK(there.rates[17] > 6.0);                         // just before the throw: still at speed
		CHECK(there.rates[17 + 20] > there.rates[17] - 4.1);  // a third of a second later: 4 rad/s less at most
		CHECK(there.rates.back() < -1.0);                     // and in the end it does turn the other way
	}

	g_case = "spinning: left and right";
	{
		// To the right when positive, to the left when negative, and exactly as far either way
		const SpinLog right = runSpin(60.0, stickThrown, 1.0, 0.79, 100);
		const SpinLog left = runSpin(60.0, stickThrownLeft, 1.0, 0.79, 100);
		CHECK(right.degrees > 0.0);
		CHECK(left.degrees < 0.0);
		CHECK(near(right.degrees, -left.degrees, 1e-3));
		const SpinLog heldRight = runSpin(60.0, stickHeld, 1.0, 0.79, 100);
		const SpinLog heldLeft = runSpin(60.0, stickHeldLeft, 1.0, 0.79, 100);
		CHECK(near(heldRight.degrees, -heldLeft.degrees, 1e-3));
		CHECK_EQ(right.rates.size(), left.rates.size());
		for (size_t i = 0; i < right.rates.size() && i < left.rates.size(); i++) {
			CHECK(right.rates[i] >= 0.0);
			CHECK(left.rates[i] <= 0.0);
			CHECK(near(right.rates[i], -left.rates[i], 1e-5));
		}
		// Skate's own rules are not quite even: a rate of nothing counts as turning one way, so
		// a turn to the other side gets going a tick later and stays a few degrees behind. The
		// code under test turns to both sides like Skate's quicker one.
		{
			const std::vector<double> skateRight = skateSpin(stickThrown, 1.0, 0.79, 0);
			const std::vector<double> skateLeft = skateSpin(stickThrownLeft, 1.0, 0.79, 0);
			const double quick = spinDegrees(skateRight), late = -spinDegrees(skateLeft);
			CHECK(quick > late + 4.0 && quick < late + 9.0);
			CHECK(near(right.degrees, quick, 0.5));
			CHECK(near(-left.degrees, quick, 0.5));
			CHECK(near(skateLeft[0], -0.2 * 0.0 - 0.2, 1e-9));     // its first tick to the late side: the slow step
			CHECK(skateRight[0] > 0.9);                            // and to the quick side
		}
		// Flicked to the left, then turned to the right: the flick does not count
		const SpinLog crossed = runSpin(60.0, stickLeftThenRight, 1.0, 0.79, 100);
		CHECK(crossed.degrees > 0.0);
		// (the throw from the left end to the right end is a flick to the right, and a full one)
		CHECK(near(crossed.flick, 1.0, 0.01));
	}

	g_case = "spinning: the same at any frame rate";
	{
		const StickAt sticks[] = { stickHeld, stickThrown, stickOverIn50, stickOverIn100, stickThrownAt100After, stickLetGoAt400, stickHalfThrown,
			stickHeldLetGoThrown };
		const double rates[] = { 60.0, 75.0, 90.0, 120.0, 144.0, 165.0, 240.0, 1000.0 };
		for (size_t n = 0; n < sizeof(sticks) / sizeof(sticks[0]); n++) {
			const SpinLog at60 = runSpin(60.0, sticks[n], 1.0, 0.8, 100);
			for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); r++) {
				const SpinLog log = runSpin(rates[r], sticks[n], 1.0, 0.8, 100);
				// a frame is not a tick: the stick is seen a little sooner or later
				CHECK(near(log.degrees, at60.degrees, 2.0 + 0.02 * fabs(at60.degrees)));
				CHECK(near(log.flick, at60.flick, 0.02));
				CHECK(log.most <= 7.94 + 0.01);
			}
		}
		// a throw of the stick is a full flick whether it takes one frame of a 60th or one of a 1000th
		CHECK(near(runSpin(60.0, stickThrown, 1.0, 0.5, 100).flick, 1.0, 0.01));
		CHECK(near(runSpin(1000.0, stickThrown, 1.0, 0.5, 100).flick, 1.0, 0.01));
		// half the way in one frame is three quarters of one, as it is for Skate at its 60 ticks
		CHECK(near(runSpin(60.0, stickHalfThrown, 1.0, 0.5, 100).flick, 0.75, 0.01));
		CHECK(near(runSpin(144.0, stickHalfThrown, 1.0, 0.5, 100).flick, 0.75, 0.01));
		CHECK(near(runSpin(1000.0, stickHalfThrown, 1.0, 0.5, 100).flick, 0.75, 0.01));
		// and a stick pushed over in a tenth of a second is a quarter of one at any rate
		CHECK(near(runSpin(30.0, stickOverIn100, 1.0, 0.5, 100).flick, 0.25, 0.03));
		CHECK(near(runSpin(50.0, stickOverIn100, 1.0, 0.5, 100).flick, 0.25, 0.03));
		CHECK(near(runSpin(240.0, stickOverIn100, 1.0, 0.5, 100).flick, 0.25, 0.03));
		// Under 60 frames a second a frame is longer than a tick, and what the stick did within
		// one is not known: it is taken to have moved evenly. So a throw that fills one frame of
		// a 30th counts as if it had taken both ticks of it, three quarters of a flick, and half
		// a throw in a frame of a 50th as five eighths. Everything slower than that is as above.
		CHECK(near(runSpin(30.0, stickThrown, 1.0, 0.5, 100).flick, 0.75, 0.01));
		CHECK(near(runSpin(50.0, stickThrown, 1.0, 0.5, 100).flick, 1.0, 0.01));
		CHECK(near(runSpin(50.0, stickHalfThrown, 1.0, 0.5, 100).flick, 0.625, 0.01));
		CHECK(near(runSpin(50.0, stickHeld, 1.0, 0.8, 100).degrees, runSpin(60.0, stickHeld, 1.0, 0.8, 100).degrees, 3.0));
		CHECK(near(runSpin(50.0, stickOverIn50, 1.0, 0.8, 100).degrees, runSpin(60.0, stickOverIn50, 1.0, 0.8, 100).degrees, 5.0));
		const SpinLog slow = runSpin(30.0, stickThrown, 1.0, 0.8, 100);
		const SpinLog full = runSpin(60.0, stickThrown, 1.0, 0.8, 100);
		CHECK(slow.degrees > 0.8 * full.degrees && slow.degrees < 0.95 * full.degrees);
		CHECK(near(runSpin(30.0, stickHeld, 1.0, 0.8, 100).degrees, runSpin(60.0, stickHeld, 1.0, 0.8, 100).degrees, 6.0));
		CHECK(near(runSpin(30.0, stickOverIn100, 1.0, 0.8, 100).degrees, runSpin(60.0, stickOverIn100, 1.0, 0.8, 100).degrees, 8.0));
	}

	g_case = "spinning: as a share of Skate's";
	{
		const SpinLog whole = runSpin(60.0, stickThrown, 1.0, 0.79, 100);
		const SpinLog half = runSpin(60.0, stickThrown, 1.0, 0.79, 50);
		const SpinLog twice = runSpin(60.0, stickThrown, 1.0, 0.79, 200);
		CHECK(near(half.degrees, 0.5 * whole.degrees, 0.1));
		CHECK(near(twice.degrees, 2.0 * whole.degrees, 0.2));
		CHECK(near(half.flick, whole.flick, 1e-6));
		// off: nothing
		Spin off;
		for (int i = 0; i < 120; i++) {
			CHECK(off.Step(1.0f / 60.0f, 1.0f, 0) == 0.0f);
			CHECK(off.Step(1.0f / 60.0f, 1.0f, -50) == 0.0f);
			CHECK(!off.InAir());
		}
	}

	g_case = "spinning: one air after another";
	{
		Spin spin;
		const float dt = 1.0f / 60.0f;
		// on the ground, the stick at rest
		for (int i = 0; i < 60; i++) spin.Feed(dt, 0.0f);
		CHECK(!spin.InAir());
		CHECK(spin.Seconds() == 0.0f);
		CHECK(spin.Turned() == 0.0f);
		// an air with the stick thrown over
		double first = 0.0;
		for (int i = 0; i < 36; i++) first += spin.Step(dt, 1.0f, 100) * dt;
		CHECK(spin.InAir());
		CHECK(near(spin.Seconds(), 0.6, 1e-3));
		CHECK(near(spin.Flick(), 1.0, 1e-3));
		CHECK(near(spin.Turned(), first, 1e-3));
		CHECK(first > 3.5);
		// it lands: the turning is over, what the air came to is still there to be read
		spin.Land();
		CHECK(!spin.InAir());
		CHECK(near(spin.Turned(), first, 1e-3));
		CHECK(near(spin.Seconds(), 0.6, 1e-3));
		CHECK(near(spin.Flick(), 1.0, 1e-3));
		// on the ground with the stick still over, for a second
		for (int i = 0; i < 60; i++) spin.Feed(dt, 1.0f);
		CHECK(!spin.InAir());
		// the next air starts from nothing, and the old flick is long gone: the stick is only held now
		double second = 0.0;
		float firstRate = spin.Step(dt, 1.0f, 100);
		second += firstRate * dt;
		CHECK(spin.InAir());
		CHECK(near(spin.Seconds(), dt, 1e-4));
		CHECK(firstRate > 0.0f && firstRate < 1.5f);
		for (int i = 1; i < 36; i++) second += spin.Step(dt, 1.0f, 100) * dt;
		CHECK(near(spin.Flick(), 0.0, 1e-3));
		CHECK(near(second, 0.4 * first, 0.25));
		CHECK(near(spin.Turned(), second, 1e-3));
		// Straight from one air into the next without a frame on the ground (a bounce): it lands
		// in between all the same when it is told so
		spin.Land();
		const float again = spin.Step(dt, 1.0f, 100);
		CHECK(spin.InAir());
		CHECK(near(spin.Seconds(), dt, 1e-4));
		CHECK(again > 0.0f && again < 1.5f);
		// Two airs a third of a second apart, the stick thrown over at the start of the first and
		// held since: by the second the throw is 0.6 s old and counts for nothing
		{
			Spin two;
			for (int i = 0; i < 60; i++) two.Feed(dt, 0.0f);
			for (int i = 0; i < 18; i++) two.Step(dt, 1.0f, 100);
			CHECK(near(two.Flick(), 1.0, 1e-3));
			two.Land();
			for (int i = 0; i < 18; i++) two.Feed(dt, 1.0f);
			for (int i = 0; i < 18; i++) two.Step(dt, 1.0f, 100);
			CHECK(near(two.Flick(), 0.0, 1e-3));
		}
		// The ground ends an air by itself as well, without being told
		{
			Spin untold;
			for (int i = 0; i < 20; i++) untold.Step(dt, 1.0f, 100);
			CHECK(untold.InAir());
			untold.Feed(dt, 1.0f);
			CHECK(!untold.InAir());
			for (int i = 0; i < 59; i++) untold.Feed(dt, 1.0f);
			const float start = untold.Step(dt, 1.0f, 100);
			CHECK(near(untold.Seconds(), dt, 1e-4));
			CHECK(start > 0.0f && start < 1.5f);
		}
		// What was left of a tick when the board landed is not carried into the next air
		{
			Spin carry;
			carry.Step(0.01f, 0.0f, 100);
			carry.Step(0.01f, 0.0f, 100);
			CHECK(carry.InAir());
			carry.Land();
			carry.Step(0.014f, 0.0f, 100);
			CHECK(!carry.InAir());
			carry.Step(0.014f, 0.0f, 100);
			CHECK(carry.InAir());
		}
		// Reset: nothing is left
		CHECK(near(spin.Seconds(), dt, 1e-4));
		for (int i = 0; i < 30; i++) spin.Feed(dt, 0.0f);
		for (int i = 0; i < 12; i++) spin.Step(dt, 1.0f, 100);
		CHECK(near(spin.Flick(), 1.0, 1e-3));
		spin.Reset();
		CHECK(!spin.InAir());
		CHECK(spin.Turned() == 0.0f);
		CHECK(spin.Flick() == 0.0f);
		CHECK(spin.Seconds() == 0.0f);
		// not the stick as it was either: an air with the stick at rest does not turn
		for (int i = 0; i < 30; i++) CHECK(spin.Step(dt, 0.0f, 100) == 0.0f);
		// nor a throw that no tick had taken up yet
		{
			Spin pending;
			for (int i = 0; i < 100; i++) pending.Feed(0.001f, 0.0f);
			pending.Feed(0.001f, 1.0f);
			pending.Reset();
			for (int i = 0; i < 300; i++) pending.Step(0.001f, 1.0f, 100);
			CHECK(pending.InAir());
			CHECK(near(pending.Flick(), 0.0, 1e-3));
		}
		// A throw in the first moments after a reset, before the trail of the stick is a tick
		// long: it is measured from as far back as the trail goes
		{
			Spin early;
			for (int i = 0; i < 4; i++) early.Feed(0.001f, 0.0f);
			for (int i = 0; i < 3; i++) early.Feed(0.001f, 0.5f);
			for (int i = 0; i < 200; i++) early.Step(0.001f, 1.0f, 100);
			CHECK(near(early.Flick(), 1.0, 0.01));
		}
		// a flick made before the reset does not count after it
		Spin fresh;
		for (int i = 0; i < 30; i++) fresh.Feed(dt, 0.0f);
		fresh.Feed(dt, 1.0f);
		fresh.Reset();
		for (int i = 0; i < 30; i++) fresh.Step(dt, 1.0f, 100);
		CHECK(near(fresh.Flick(), 0.0, 1e-3));
	}

	g_case = "spinning: odd frames";
	{
		Spin spin;
		const float nan = (float)sqrt(-1.0);
		// no time, time backwards, not a number: nothing happens
		CHECK(spin.Step(0.0f, 1.0f, 100) == 0.0f);
		CHECK(spin.Step(-1.0f, 1.0f, 100) == 0.0f);
		CHECK(spin.Step(nan, 1.0f, 100) == 0.0f);
		CHECK(!spin.InAir());
		spin.Feed(0.0f, 1.0f);
		spin.Feed(-1.0f, 1.0f);
		spin.Feed(nan, 1.0f);
		CHECK(!spin.InAir());
		// and nothing is spoilt: after them a throw is a throw
		{
			Spin after;
			for (int i = 0; i < 30; i++) after.Feed(1.0f / 60.0f, 0.0f);
			after.Feed(nan, 0.0f);
			after.Feed(-1.0f, 0.0f);
			after.Feed(0.0f, 0.0f);
			for (int i = 0; i < 12; i++) after.Step(1.0f / 60.0f, 1.0f, 100);
			CHECK(near(after.Flick(), 1.0, 1e-3));
			// and a stick that is held is not: a second on the ground with it over, then an air
			after.Land();
			after.Feed(nan, 1.0f);
			after.Feed(-1.0f, 1.0f);
			after.Feed(0.0f, 1.0f);
			for (int i = 0; i < 60; i++) after.Feed(1.0f / 60.0f, 1.0f);
			for (int i = 0; i < 12; i++) after.Step(1.0f / 60.0f, 1.0f, 100);
			CHECK(near(after.Flick(), 0.0, 1e-3));
			// a stick that is not a number on the ground is a stick at rest too
			Spin bad;
			for (int i = 0; i < 30; i++) bad.Feed(1.0f / 60.0f, nan);
			double turned = 0.0;
			for (int i = 0; i < 30; i++) {
				const float rate = bad.Step(1.0f / 60.0f, 0.0f, 100);
				CHECK(rate == 0.0f);
				turned += rate;
			}
			CHECK(turned == 0.0);
			for (int i = 0; i < 30; i++) CHECK(bad.Step(1.0f / 60.0f, 1.0f, 100) >= 0.0f);
			CHECK(bad.Turned() > 0.5f);
		}
		// a stick that is not a number is a stick at rest
		for (int i = 0; i < 60; i++) {
			const float rate = spin.Step(1.0f / 60.0f, nan, 100);
			CHECK(rate == 0.0f);
		}
		// a stick past its end is at its end
		Spin far, end;
		double a = 0.0, b = 0.0;
		for (int i = 0; i < 40; i++) {
			a += far.Step(1.0f / 60.0f, 5.0f, 100);
			b += end.Step(1.0f / 60.0f, 1.0f, 100);
		}
		CHECK(near(a, b, 1e-4));
		// a frame that took seconds counts as a tenth of one: six ticks, not hundreds
		Spin hitch;
		const float rate = hitch.Step(5.0f, 1.0f, 100);
		CHECK(near(hitch.Seconds(), 0.1, 1e-3));
		CHECK(rate > 0.0f && rate <= 7.94f);
		CHECK(near(hitch.Turned(), rate * 0.1, 1e-4));
		// frames far shorter than a tick: the turning waits for the tick, the flick is still seen
		Spin quick;
		for (int i = 0; i < 100; i++) quick.Feed(0.0001f, 0.0f);
		float seen = 0.0f;
		for (int i = 0; i < 400; i++) seen = quick.Step(0.0001f, 1.0f, 100);
		CHECK(seen > 0.0f);
		CHECK(near(quick.Flick(), 1.0, 0.01));
	}
}

int main(int argc, char** argv) {
	const std::string scriptDir = argc > 1 ? argv[1] : "scripts/qb/source/scripts/game/skater";
	testAgainstTheScripts(scriptDir);
	testJumps();
	testPushing();
	testPushClock();
	testFeet();
	testCoasting();
	testSteering();
	testSpinning();
	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
