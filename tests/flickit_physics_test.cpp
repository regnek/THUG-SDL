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

static const PushTiming kTiming = { 0.3f, 0.25f, 0.9f };

// One run of the push: the button is down from each "down" to the "up" after it (seconds)
struct Press {
	double down, up;
};

// What stands in for the game's scripts in a run: nobody ever takes a push that asks for its
// animation, or a script takes it as soon as it is not busy and is then busy for so many seconds
// with the animation it has started (0: it is never busy)
static const double kNobody = -1.0;

struct PushLog {
	double speed;               // at the end
	double most;                // the fastest it ever was
	int pushes;                 // feet that left the ground
	double firstAdd, lastAdd;   // when speed was first and last added
	double added[16];           // what each push added
	double asked[16];
	double start[16];           // when each push started adding
	int taken;                  // pushes whose animation the script started
	double took[16];            // and when
};

static PushLog runPush(double rate, double seconds, double speed, const Press* presses, int count, int percent, const PushTiming& timing, int coast, double animation) {
	PushRun run;
	PushLog log;
	memset(&log, 0, sizeof(log));
	log.firstAdd = -1.0;
	log.lastAdd = -1.0;
	log.most = speed;
	const double dt = 1.0 / rate;
	uint32_t serial = run.Serial();
	bool adding = false;
	double busyUntil = -1.0;
	const int frames = (int)(seconds * rate + 0.5);
	for (int frame = 0; frame < frames; frame++) {
		const double t = frame * dt;
		bool held = false;
		for (int i = 0; i < count; i++) {
			if (t >= presses[i].down - 1e-9 && t < presses[i].up - 1e-9) held = true;
		}
		const float add = run.Step((float)dt, (float)speed, held, percent, timing);
		CHECK(add >= 0.0f);
		CHECK(add == add);
		if (add > 0.0f) {
			if (log.firstAdd < 0.0) log.firstAdd = t;
			log.lastAdd = t + dt;
			if (!adding && log.pushes < 16) log.start[log.pushes] = t;
			adding = true;
		}
		speed += add;
		if (coast > 0) speed = Coast((float)speed, (float)dt, coast);
		if (speed > log.most) log.most = speed;
		if (run.Serial() != serial) {
			serial = run.Serial();
			if (log.pushes < 16) {
				log.added[log.pushes] = run.Added();
				log.asked[log.pushes] = run.Asked();
			}
			log.pushes++;
			adding = false;
		}
		// the game's script, after the physics: it looks for a push only while its last
		// animation is not playing
		if (animation >= 0.0 && t + dt >= busyUntil - 1e-9 && run.Animating()) {
			const bool asking = run.Phase() == PUSH_ASKED;
			run.Taken();
			if (asking) {
				CHECK_EQ(run.Phase(), PUSH_INTO);
				busyUntil = t + dt + animation;
				if (log.taken < 16) log.took[log.taken] = t + dt;
				log.taken++;
			}
		}
	}
	log.speed = speed;
	return log;
}

static void testPushing() {
	const double unit = 80.0;
	const double frame60 = 1.0 / 60.0;

	g_case = "rolling: the settings and their defaults";
	{
		Settings s;
		SetDefaults(s);
		CHECK_EQ(s.push, 100);
		CHECK_EQ(s.pushOnOllie, 1);
		CHECK_EQ(s.pushOverTurn, 1);
		CHECK_EQ(s.pushDelay, 300);
		CHECK_EQ(s.pushContact, 250);
		CHECK_EQ(s.pushCycle, 900);
		CHECK_EQ(s.coast, 100);
		CHECK_EQ(s.steering, 100);
		Sanitize(s);
		CHECK_EQ(s.push, 100);
		CHECK_EQ(s.pushDelay, 300);
		CHECK_EQ(s.pushContact, 250);
		CHECK_EQ(s.pushCycle, 900);
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
		s.pushDelay = -5; s.pushContact = 1; s.pushCycle = 1;
		Sanitize(s);
		CHECK_EQ(s.pushOnOllie, 1);
		CHECK_EQ(s.pushOverTurn, 1);
		CHECK_EQ(s.pushDelay, 0);
		CHECK_EQ(s.pushContact, 50);
		CHECK_EQ(s.pushCycle, 200);
		s.pushOnOllie = 0;
		s.pushOverTurn = 0;
		s.pushDelay = 99999; s.pushContact = 99999; s.pushCycle = 99999;
		Sanitize(s);
		CHECK_EQ(s.pushOnOllie, 0);
		CHECK_EQ(s.pushOverTurn, 0);
		CHECK_EQ(s.pushDelay, 1000);
		CHECK_EQ(s.pushContact, 1000);
		CHECK_EQ(s.pushCycle, 3000);
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

	g_case = "rolling: one tap";
	{
		// A tap of 50 ms from a standstill, the game starts its animation at once: the speed
		// comes while the foot is on the ground, from 0.3 s to 0.55 s into the animation, and it
		// is what Skate's curve gives for 50 ms
		const Press tap[] = { { 0.0, 0.05 } };
		const PushLog log = runPush(60.0, 3.0, 0.0, tap, 1, 100, kTiming, 0, 0.0);
		CHECK_EQ(log.pushes, 1);
		CHECK_EQ(log.taken, 1);
		CHECK(near(log.took[0], frame60, 1e-6));
		const double want = at(kSkatePush, 0.05 / 0.226785704) * unit;
		CHECK(near(log.speed, want, 0.5));
		CHECK(near(log.speed, 81.5, 1.0));
		CHECK(near(log.added[0], log.speed, 1e-3));
		CHECK(near(log.asked[0], log.speed, 1e-3));
		CHECK(log.firstAdd >= log.took[0] + 0.3 - frame60 - 1e-6 && log.firstAdd <= log.took[0] + 0.3 + 1e-6);
		CHECK(log.lastAdd >= log.took[0] + 0.55 - frame60 && log.lastAdd <= log.took[0] + 0.55 + frame60 + 1e-6);
		// the shortest tap there is, one frame: Skate's tap
		const Press blip[] = { { 0.0, 1.0 / 60.0 } };
		const PushLog least = runPush(60.0, 3.0, 300.0, blip, 1, 100, kTiming, 0, 0.0);
		CHECK_EQ(least.pushes, 1);
		CHECK(least.speed - 300.0 >= 0.75 * unit - 0.01);
		CHECK(least.speed - 300.0 < 0.9 * unit);
	}

	g_case = "rolling: the button held";
	{
		// Held from a standstill: every push has all there is, 4 m/s, until the top speed is
		// reached. 320, 640, 680, and then no more.
		const Press hold[] = { { 0.0, 30.0 } };
		const PushLog log = runPush(60.0, 12.0, 0.0, hold, 1, 100, kTiming, 0, 0.0);
		CHECK_EQ(log.pushes, 3);
		CHECK_EQ(log.taken, 3);
		CHECK(near(log.added[0], 4.0 * unit, 0.5));
		CHECK(near(log.added[1], 4.0 * unit, 0.5));
		CHECK(near(log.added[2], 8.5 * unit - 8.0 * unit, 0.5));
		CHECK(near(log.asked[2], 4.0 * unit, 0.5));
		CHECK(near(log.speed, 8.5 * unit, 0.01));
		CHECK(log.most <= 8.5 * unit + 0.01);
		// the foot is down 0.3 s into each animation, and with a script that is never busy the
		// next push follows 0.9 s after the last
		for (int i = 0; i < 3; i++) {
			CHECK(near(log.start[i], log.took[i] + 0.3, frame60 + 1e-6));
		}
		CHECK(near(log.took[1] - log.took[0], 0.9, 2.0 * frame60));
		CHECK(near(log.took[2] - log.took[1], 0.9, 2.0 * frame60));
	}

	g_case = "rolling: the game's animation sets the pace";
	{
		const Press hold[] = { { 0.0, 30.0 } };
		// An animation longer than the cycle: the script looks for the next push when the
		// animation is over, and that is when the next push starts. Every 1.2 s here.
		const PushLog longer = runPush(60.0, 12.0, 0.0, hold, 1, 100, kTiming, 0, 1.2);
		CHECK_EQ(longer.pushes, 3);
		CHECK_EQ(longer.taken, 3);
		CHECK(near(longer.took[1] - longer.took[0], 1.2, 2.0 * frame60));
		CHECK(near(longer.took[2] - longer.took[1], 1.2, 2.0 * frame60));
		for (int i = 0; i < 3; i++) {
			CHECK(near(longer.start[i], longer.took[i] + 0.3, frame60 + 1e-6));
			CHECK(near(longer.added[i], i < 2 ? 4.0 * unit : 0.5 * unit, 0.5));
		}
		// An animation shorter than the cycle: the cycle is the least there is between pushes
		const PushLog shorter = runPush(60.0, 12.0, 0.0, hold, 1, 100, kTiming, 0, 0.5);
		CHECK_EQ(shorter.pushes, 3);
		CHECK(near(shorter.took[1] - shorter.took[0], 0.9, 2.0 * frame60));
		// Nobody takes the push (the game shows something else): it starts by itself after a
		// tenth of a second, and so does each one after it
		const PushLog alone = runPush(60.0, 12.0, 0.0, hold, 1, 100, kTiming, 0, kNobody);
		CHECK_EQ(alone.pushes, 3);
		CHECK_EQ(alone.taken, 0);
		CHECK(near(alone.start[0], 0.1 + 0.3, frame60 + 1e-6));
		CHECK(near(alone.start[1], 0.1 + 0.9 + 0.1 + 0.3, 2.0 * frame60));
		CHECK(near(alone.start[2], 0.1 + 0.9 + 0.1 + 0.9 + 0.1 + 0.3, 3.0 * frame60));
		CHECK(near(alone.added[0], 4.0 * unit, 0.5));
		// The script took the first push and then went on to something else for a long time:
		// the second push waits for it until a second and a half after the first was taken and
		// then goes by itself, and the third does not wait that long again
		const PushLog left = runPush(60.0, 12.0, 0.0, hold, 1, 100, kTiming, 0, 30.0);
		CHECK_EQ(left.pushes, 3);
		CHECK_EQ(left.taken, 1);
		CHECK(near(left.start[1], left.took[0] + 1.5 + 0.3, 2.0 * frame60));
		CHECK(near(left.start[2] - left.start[1], 0.9 + 0.1, 2.0 * frame60));
		// The same with a script whose animation is too long to wait for, 1.7 s: the second push
		// goes by itself, and the script comes back for the third
		const PushLog late = runPush(60.0, 12.0, 0.0, hold, 1, 100, kTiming, 0, 1.7);
		CHECK_EQ(late.pushes, 3);
		CHECK_EQ(late.taken, 2);
		CHECK(near(late.start[1], late.took[0] + 1.5 + 0.3, 2.0 * frame60));
		CHECK(near(late.took[1], late.took[0] + 1.5 + 0.9, 3.0 * frame60));
		// Taps. One a long time after a push the game took: the game is not taken to be busy
		// any more, so the push waits a tenth of a second.
		const Press taps[] = { { 0.0, 0.05 }, { 2.0, 2.05 } };
		const PushLog apart = runPush(60.0, 6.0, 0.0, taps, 2, 100, kTiming, 0, 30.0);
		CHECK_EQ(apart.pushes, 2);
		CHECK_EQ(apart.taken, 1);
		CHECK(near(apart.start[1], 2.0 + 0.1 + 0.3, 2.0 * frame60));
		// One just after the first push is over, while the game's animation of it is still
		// playing: it waits for the animation to end and is taken then, ...
		const Press soon[] = { { 0.0, 0.05 }, { 0.95, 1.0 } };
		const PushLog after = runPush(60.0, 6.0, 0.0, soon, 2, 100, kTiming, 0, 1.2);
		CHECK_EQ(after.pushes, 2);
		CHECK_EQ(after.taken, 2);
		CHECK(near(after.took[1], after.took[0] + 1.2, 2.0 * frame60));
		CHECK(near(after.start[1], after.took[1] + 0.3, frame60 + 1e-6));
		// ... and is as strong as the tap it came from, not as the wait was long
		CHECK(near(after.asked[1], at(kSkatePush, 0.05 / 0.25) * unit, 3.0));
		CHECK(after.asked[1] < 1.1 * unit);
		// and if the game never comes for it, it goes when the game can no longer be busy
		const PushLog never = runPush(60.0, 6.0, 0.0, soon, 2, 100, kTiming, 0, 30.0);
		CHECK_EQ(never.pushes, 2);
		CHECK_EQ(never.taken, 1);
		CHECK(near(never.start[1], never.took[0] + 1.5 + 0.3, 2.0 * frame60));
		// One a second and 0.45 after: under a tenth of a second is left of the game's time, so
		// it waits the tenth
		const Press edge[] = { { 0.0, 0.05 }, { 1.45, 1.5 } };
		const PushLog least = runPush(60.0, 6.0, 0.0, edge, 2, 100, kTiming, 0, 30.0);
		CHECK_EQ(least.pushes, 2);
		CHECK(near(least.start[1], 1.45 + 0.1 + 0.3, 2.0 * frame60));
		// What was under way is forgotten on a reset, and so is the game's being busy
		{
			PushRun run;
			float v = 0.0f;
			v += run.Step(1.0f / 60.0f, v, true, 100, kTiming);
			run.Taken();
			for (int frame = 0; frame < 10; frame++) v += run.Step(1.0f / 60.0f, v, false, 100, kTiming);
			CHECK_EQ(run.Phase(), PUSH_INTO);
			run.Reset();
			int waited = 0;
			for (int frame = 0; frame < 60 && run.Phase() != PUSH_INTO; frame++) {
				v += run.Step(1.0f / 60.0f, v, true, 100, kTiming);
				waited++;
			}
			CHECK_EQ(run.Phase(), PUSH_INTO);
			CHECK(waited >= 6 && waited <= 8);
		}
	}

	g_case = "rolling: the same at any frame rate";
	{
		const Press hold[] = { { 0.0, 30.0 } };
		const Press taps[] = { { 0.0, 0.05 }, { 1.5, 1.6 }, { 3.0, 3.2 }, { 4.5, 4.52 } };
		const double rates[] = { 30.0, 60.0, 75.0, 144.0, 240.0, 1000.0 };
		const double scripts[] = { kNobody, 0.0, 1.2 };
		for (size_t k = 0; k < sizeof(scripts) / sizeof(scripts[0]); k++) {
			const PushLog heldAt60 = runPush(60.0, 12.0, 0.0, hold, 1, 100, kTiming, 0, scripts[k]);
			const PushLog rolledAt60 = runPush(60.0, 12.0, 0.0, hold, 1, 100, kTiming, 100, scripts[k]);
			CHECK_EQ(heldAt60.pushes, 3);
			for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); r++) {
				const PushLog held = runPush(rates[r], 12.0, 0.0, hold, 1, 100, kTiming, 0, scripts[k]);
				CHECK_EQ(held.pushes, heldAt60.pushes);
				CHECK(near(held.speed, heldAt60.speed, 0.05));
				for (int i = 0; i < 3; i++) {
					CHECK(near(held.added[i], heldAt60.added[i], 0.5));
					// each push may be taken a frame late, so the times drift apart by a frame a push
					CHECK(near(held.start[i], heldAt60.start[i], (i + 2) * (1.0 / rates[r] + frame60)));
				}
				// With the board rolling out in between, when the next push starts hangs on the
				// moment the speed falls under the limit, so runs drift apart in time. What they
				// have in common: about as many pushes, and a speed near the top.
				const PushLog rolled = runPush(rates[r], 12.0, 0.0, hold, 1, 100, kTiming, 100, scripts[k]);
				CHECK(rolled.pushes >= rolledAt60.pushes - 1 && rolled.pushes <= rolledAt60.pushes + 1);
				CHECK(rolled.speed > 8.0 * unit && rolled.speed <= 8.5 * unit + 0.01);
				CHECK(rolled.most <= 8.5 * unit + 0.01);
				const PushLog tapped = runPush(rates[r], 12.0, 0.0, taps, 4, 100, kTiming, 0, scripts[k]);
				CHECK_EQ(tapped.pushes, 4);
				// a tap lasts a whole number of frames, so its length differs by up to a frame
				for (int i = 0; i < 4; i++) {
					const double length = taps[i].up - taps[i].down;
					const double low = at(kSkatePush, (length - 1.0 / rates[r]) / 0.25) * unit;
					const double high = at(kSkatePush, (length + 1.0 / rates[r]) / 0.226785704) * unit;
					CHECK(tapped.added[i] >= (low < 0.75 * unit ? 0.75 * unit : low) - 1.0);
					CHECK(tapped.added[i] <= (high > 4.0 * unit ? 4.0 * unit : high) + 1.0);
				}
			}
		}
	}

	g_case = "rolling: as fast as pushing gets";
	{
		// With the board rolling out between pushes: the button held keeps it near the top, and it
		// is never faster than Skate's 8.5 m/s
		const Press hold[] = { { 0.0, 100.0 } };
		const PushLog log = runPush(60.0, 60.0, 0.0, hold, 1, 100, kTiming, 100, 0.0);
		CHECK(log.most <= 8.5 * unit + 0.01);
		CHECK(log.most > 8.4 * unit);
		CHECK(log.speed > 8.0 * unit);
		CHECK(log.pushes > 10);
		// From 8.3 m/s on no push starts, and a press made then is not kept for later
		PushRun run;
		const float fast = 8.35f * (float)unit;
		for (int frame = 0; frame < 120; frame++) {
			CHECK(run.Step(1.0f / 60.0f, fast, frame < 10, 100, kTiming) == 0.0f);
			CHECK_EQ(run.Phase(), PUSH_NONE);
			CHECK(!run.Active());
			CHECK(!run.Animating());
		}
		for (int frame = 0; frame < 120; frame++) {
			CHECK(run.Step(1.0f / 60.0f, 300.0f, false, 100, kTiming) == 0.0f);
			CHECK_EQ(run.Phase(), PUSH_NONE);
		}
		// but a button that is still down when the board has slowed pushes
		PushRun again;
		for (int frame = 0; frame < 60; frame++) {
			CHECK(again.Step(1.0f / 60.0f, fast, true, 100, kTiming) == 0.0f);
		}
		CHECK_EQ(again.Phase(), PUSH_NONE);
		again.Step(1.0f / 60.0f, 8.2f * (float)unit, true, 100, kTiming);
		CHECK_EQ(again.Phase(), PUSH_ASKED);
		// and that push has all there is: the button has been down the whole time, however
		// soon the foot comes down
		{
			const PushTiming soon = { 0.05f, 0.25f, 0.9f };
			PushRun waited;
			for (int frame = 0; frame < 120; frame++) {
				CHECK(waited.Step(1.0f / 60.0f, fast, true, 100, soon) == 0.0f);
			}
			float v = 300.0f;
			for (int frame = 0; frame < 40; frame++) {
				v += waited.Step(1.0f / 60.0f, v, true, 100, soon);
				waited.Taken();
			}
			CHECK_EQ((int)waited.Serial(), 1);
			CHECK(near(waited.Asked(), 4.0 * unit, 0.5));
			CHECK(near(v, 300.0 + 4.0 * unit, 0.5));
			// whereas a press that has only just begun is what it is: one frame of asking and
			// 50 ms of the foot coming down
			PushRun fresh;
			float w = 300.0f;
			for (int frame = 0; frame < 40; frame++) {
				w += fresh.Step(1.0f / 60.0f, w, true, 100, soon);
				fresh.Taken();
			}
			CHECK_EQ((int)fresh.Serial(), 1);
			CHECK(near(fresh.HeldFor(), frame60 + 0.05, 1e-4));
			CHECK(near(fresh.Asked(), at(kSkatePush, (frame60 + 0.05) / 0.25) * unit, 2.0));
			CHECK(fresh.Asked() < 1.5 * unit);
		}
		// just under the limit a push starts, and adds only what is left to the top
		const Press tap[] = { { 0.0, 0.5 } };
		const PushLog close = runPush(60.0, 2.0, 8.25 * unit, tap, 1, 100, kTiming, 0, 0.0);
		CHECK_EQ(close.pushes, 1);
		CHECK(near(close.speed, 8.5 * unit, 0.01));
	}

	g_case = "rolling: the parts of a push";
	{
		// The push asks; the game takes it after the first frame. Then: into it for 0.3 s, the
		// foot down for 0.25 s (the board is being pushed until here), back for the rest of the
		// 0.9 s. The game is asked for its animation until a tenth of a second after it took it.
		PushRun run;
		const float dt = 1.0f / 200.0f;
		float speed = 100.0f;
		CHECK(!run.Active());
		CHECK(!run.Animating());
		for (int frame = 0; frame < 400; frame++) {
			const double t = frame * (double)dt;   // the time into the animation this frame ends at
			const float add = run.Step(dt, speed, frame < 4, 100, kTiming);
			speed += add;
			if (frame == 0) {
				CHECK_EQ(run.Phase(), PUSH_ASKED);
				CHECK(run.Animating());
				CHECK(run.Active());
				CHECK(add == 0.0f);
				run.Taken();
				CHECK_EQ(run.Phase(), PUSH_INTO);
			}
			else if (t < 0.295) {
				CHECK_EQ(run.Phase(), PUSH_INTO);
				CHECK(run.Active());
				CHECK(add == 0.0f);
				if (t < 0.095) CHECK(run.Animating());
				if (t > 0.105) CHECK(!run.Animating());
			}
			else if (t > 0.305 && t < 0.545) {
				CHECK_EQ(run.Phase(), PUSH_CONTACT);
				CHECK(!run.Animating());
				CHECK(run.Active());
				CHECK(add > 0.0f);
			}
			else if (t > 0.555 && t < 0.895) {
				CHECK_EQ(run.Phase(), PUSH_RECOVER);
				CHECK(!run.Animating());
				CHECK(!run.Active());
				CHECK(add == 0.0f);
			}
			else if (t > 0.905) {
				CHECK_EQ(run.Phase(), PUSH_NONE);
				CHECK(!run.Animating());
				CHECK(!run.Active());
				CHECK(add == 0.0f);
			}
			// the game saying "taken" again changes nothing
			if (frame == 20 || frame == 80 || frame == 150 || frame == 300) {
				const int phase = run.Phase();
				run.Taken();
				CHECK_EQ(run.Phase(), phase);
			}
		}
		CHECK_EQ((int)run.Serial(), 1);
		CHECK(near(run.From(), 100.0, 1e-3));
		CHECK(near(run.HeldFor(), 4 * dt, 1e-4));
		CHECK(near(run.Added(), speed - 100.0, 1e-2));
		CHECK(near(run.Asked(), run.Added(), 1e-2));
		// A push nobody takes: it asks for a tenth of a second, and goes on asking for a tenth
		// more while the foot is already on its way
		PushRun alone;
		float v = 0.0f, half = 0.0f;
		for (int frame = 0; frame < 400; frame++) {
			const double t = (frame + 1) * (double)dt;
			v += alone.Step(dt, v, true, 100, kTiming);
			if (t < 0.095) CHECK_EQ(alone.Phase(), PUSH_ASKED);
			if (t > 0.105 && t < 0.395) CHECK_EQ(alone.Phase(), PUSH_INTO);
			if (t < 0.195) CHECK(alone.Animating());
			if (t > 0.205 && t < 0.995) CHECK(!alone.Animating());
			if (t > 0.405 && t < 0.645) CHECK_EQ(alone.Phase(), PUSH_CONTACT);
			// the speed comes in evenly: half of it by the middle of the contact
			if (frame == 105 - 1) half = v;   // 0.525 s
		}
		CHECK(near(half, 0.5 * 4.0 * unit, 4.0));
		// with no delay the foot is down the moment the game takes the push, and the game is
		// still asked for a tenth of a second
		const PushTiming now = { 0.0f, 0.25f, 0.9f };
		PushRun direct;
		CHECK(direct.Step(dt, 0.0f, true, 100, now) == 0.0f);
		direct.Taken();
		CHECK(direct.Step(dt, 0.0f, true, 100, now) > 0.0f);
		CHECK_EQ(direct.Phase(), PUSH_CONTACT);
		CHECK(direct.Animating());
		for (int frame = 0; frame < 30; frame++) direct.Step(dt, 50.0f, true, 100, now);
		CHECK_EQ(direct.Phase(), PUSH_CONTACT);
		CHECK(!direct.Animating());
		// With the foot down for a twentieth of a second and no delay the push is over before the
		// game has had its tenth of a second: it is asked for the animation all the same
		const PushTiming brief = { 0.0f, 0.05f, 0.9f };
		PushRun shortest;
		for (int frame = 0; frame < 100; frame++) {
			const double t = (frame + 1) * (double)dt;   // 0.1 s of asking, then the push
			shortest.Step(dt, 0.0f, true, 100, brief);
			if (t > 0.155 && t < 0.195) {
				CHECK_EQ(shortest.Phase(), PUSH_RECOVER);
				CHECK(shortest.Animating());
				CHECK(!shortest.Active());
			}
			if (t > 0.205) {
				CHECK_EQ(shortest.Phase(), PUSH_RECOVER);
				CHECK(!shortest.Animating());
			}
		}
		// nothing asks, nothing is taken
		PushRun idle;
		idle.Taken();
		CHECK_EQ(idle.Phase(), PUSH_NONE);
		CHECK(idle.Step(dt, 100.0f, false, 100, kTiming) == 0.0f);
		CHECK_EQ(idle.Phase(), PUSH_NONE);
	}

	g_case = "rolling: a press while a push is under way";
	{
		// A second tap during the first push: the next push follows when the first is over, 0.9 s
		// after it began, and its strength is the second tap's
		const Press two[] = { { 0.0, 0.05 }, { 0.6, 0.7 } };
		const PushLog log = runPush(200.0, 4.0, 0.0, two, 2, 100, kTiming, 0, 0.0);
		CHECK_EQ(log.pushes, 2);
		CHECK(near(log.took[1] - log.took[0], 0.9, 0.011));
		CHECK(near(log.start[1], log.took[1] + 0.3, 0.006));
		CHECK(near(log.added[0], at(kSkatePush, 0.05 / 0.226785704) * unit, 1.5));
		CHECK(near(log.added[1], at(kSkatePush, 0.1 / 0.25) * unit, 3.0));
		// pressed during the first push and kept down: by the time the foot is down again the
		// button has been held long enough for all there is
		const Press kept[] = { { 0.0, 0.05 }, { 0.6, 1.3 } };
		const PushLog strong = runPush(200.0, 4.0, 0.0, kept, 2, 100, kTiming, 0, 0.0);
		CHECK_EQ(strong.pushes, 2);
		CHECK(near(strong.added[1], 4.0 * unit, 1.0));
		// the first push is not made stronger by a press that came after its own was let go
		CHECK(near(strong.added[0], log.added[0], 0.01));
		// three quick taps while one push runs are one more push, not three
		const Press mash[] = { { 0.0, 0.03 }, { 0.1, 0.13 }, { 0.2, 0.23 }, { 0.4, 0.43 } };
		const PushLog mashed = runPush(200.0, 4.0, 0.0, mash, 4, 100, kTiming, 0, 0.0);
		CHECK_EQ(mashed.pushes, 2);
		// A push is as strong as the press it came from, also when the button goes down again
		// before the foot does: a tap of 30 ms, then the button held from 0.1 s on
		const Press again[] = { { 0.0, 0.03 }, { 0.1, 0.5 } };
		const double scripts[] = { 0.0, kNobody };
		for (int k = 0; k < 2; k++) {
			const PushLog first = runPush(200.0, 4.0, 0.0, again, 2, 100, kTiming, 0, scripts[k]);
			CHECK_EQ(first.pushes, 2);
			CHECK(near(first.asked[0], at(kSkatePush, 0.03 / 0.226785704) * unit, 3.0));
			CHECK(first.asked[0] < 1.0 * unit);
			CHECK(near(first.asked[1], 4.0 * unit, 1.0));
			// the same for a push that was waiting its turn: a tap, a second tap while the first
			// push runs, and the button held once the second push has started
			const Press queue[] = { { 0.0, 0.05 }, { 0.6, 0.65 }, { 1.05, 1.5 } };
			const PushLog queued = runPush(200.0, 5.0, 0.0, queue, 3, 100, kTiming, 0, scripts[k]);
			CHECK_EQ(queued.pushes, 3);
			CHECK(near(queued.asked[1], at(kSkatePush, 0.05 / 0.25) * unit, 3.0));
			CHECK(queued.asked[1] < 1.1 * unit);
			CHECK(near(queued.asked[2], 4.0 * unit, 1.0));
		}
	}

	g_case = "rolling: the push as a share of Skate's";
	{
		const Press hold[] = { { 0.0, 30.0 } };
		const PushLog half = runPush(60.0, 12.0, 0.0, hold, 1, 50, kTiming, 0, 0.0);
		CHECK(near(half.added[0], 2.0 * unit, 0.5));
		CHECK(near(half.speed, 4.25 * unit, 0.01));
		const PushLog twice = runPush(60.0, 12.0, 0.0, hold, 1, 200, kTiming, 0, 0.0);
		CHECK(near(twice.added[0], 8.0 * unit, 1.0));
		CHECK(near(twice.speed, 17.0 * unit, 0.01));
		// off: nothing, whatever the button does
		PushRun off;
		for (int frame = 0; frame < 200; frame++) {
			CHECK(off.Step(1.0f / 60.0f, 100.0f, true, 0, kTiming) == 0.0f);
			CHECK_EQ(off.Phase(), PUSH_NONE);
		}
	}

	g_case = "rolling: no faster than Skate lets a push come in";
	{
		// With the foot on the ground for only 50 ms a full push would have to add 4 m/s in three
		// ticks. Skate allows 0.6 m/s a tick: what comes of it is under half of what was asked.
		const PushTiming quick = { 0.3f, 0.05f, 0.9f };
		const Press hold[] = { { 0.0, 0.4 } };
		const PushLog log = runPush(240.0, 3.0, 0.0, hold, 1, 100, quick, 0, 0.0);
		CHECK_EQ(log.pushes, 1);
		CHECK(near(log.asked[0], 4.0 * unit, 0.5));
		CHECK(log.added[0] < 0.5 * log.asked[0]);
		// three ticks at 0.6 to 0.58 m/s
		CHECK(near(log.added[0], 3.0 * 0.59 * unit, 6.0));
		// the usual quarter of a second is slow enough for the whole push
		const PushLog whole = runPush(240.0, 3.0, 0.0, hold, 1, 100, kTiming, 0, 0.0);
		CHECK(near(whole.added[0], 4.0 * unit, 0.5));
	}

	g_case = "rolling: odd frames and odd timings";
	{
		PushRun run;
		// no time, time backwards, not a number: nothing happens, nothing breaks
		CHECK(run.Step(0.0f, 100.0f, true, 100, kTiming) == 0.0f);
		CHECK(run.Step(-1.0f, 100.0f, true, 100, kTiming) == 0.0f);
		const float nan = (float)sqrt(-1.0);
		CHECK(run.Step(nan, 100.0f, true, 100, kTiming) == 0.0f);
		CHECK_EQ(run.Phase(), PUSH_NONE);
		// a frame that took seconds counts as a tenth of one: a push does not happen all at once
		PushRun hitch;
		float v = 0.0f;
		v += hitch.Step(5.0f, v, true, 100, kTiming);
		CHECK(v == 0.0f);
		CHECK_EQ(hitch.Phase(), PUSH_INTO);
		float most = 0.0f;
		for (int frame = 0; frame < 200; frame++) {
			const float add = hitch.Step(5.0f, v, true, 100, kTiming);
			if (add > most) most = add;
			v += add;
			CHECK(v <= 8.5f * (float)unit + 0.01f);
		}
		CHECK(most <= 4.0f * (float)unit * 0.1f / 0.25f + 0.5f);
		// A time that makes no sense counts as none. No delay: the foot is down at once, and the
		// next push still follows 0.9 s after this one.
		{
			const Press hold[] = { { 0.0, 30.0 } };
			const PushTiming early = { -1.0f, 0.25f, 0.9f };
			const PushLog log = runPush(200.0, 2.5, 0.0, hold, 1, 100, early, 0, 0.0);
			CHECK_EQ(log.pushes, 3);
			CHECK(near(log.start[0], log.took[0], 0.006));
			CHECK(near(log.took[1] - log.took[0], 0.9, 0.011));
			// the button had been down for a frame or two when the foot came down: a tap's push.
			// By the second push it has been down all the while.
			CHECK(log.added[0] >= 0.75 * 80.0 && log.added[0] < 0.85 * 80.0);
			CHECK(near(log.added[1], 4.0 * 80.0, 1.0));
			// No time on the ground: nothing is pushed, and the pushes still come 0.9 s apart
			const PushTiming none = { 0.3f, -1.0f, 0.9f };
			const PushLog air = runPush(200.0, 2.5, 0.0, hold, 1, 100, none, 0, 0.0);
			CHECK_EQ(air.pushes, 3);
			CHECK(air.speed == 0.0);
			CHECK(near(air.took[1] - air.took[0], 0.9, 0.011));
			const PushTiming zero = { 0.3f, 0.0f, 0.9f };
			const PushLog flat = runPush(200.0, 2.5, 0.0, hold, 1, 100, zero, 0, 0.0);
			CHECK_EQ(flat.pushes, 3);
			CHECK(flat.speed == 0.0);
		}
		// a speed that makes no sense with the push off: still nothing
		{
			PushRun off;
			for (int frame = 0; frame < 60; frame++) {
				CHECK(off.Step(1.0f / 60.0f, -1.0f, true, 0, kTiming) == 0.0f);
				CHECK(off.Step(1.0f / 60.0f, -1.0f, frame % 2 == 0, -100, kTiming) == 0.0f);
				CHECK_EQ(off.Phase(), PUSH_NONE);
			}
		}
		// A board that is faster than pushing gets by the time the foot comes down (a hill): the
		// push adds nothing, and takes nothing away
		{
			PushRun run;
			float v = 600.0f;
			float added = 0.0f;
			for (int frame = 0; frame < 60; frame++) {
				const float add = run.Step(1.0f / 60.0f, v, frame < 30, 100, kTiming);
				CHECK(add >= 0.0f);
				added += add;
				if (frame == 0) run.Taken();
				if (frame == 5) v = 800.0f;
			}
			CHECK_EQ((int)run.Serial(), 1);
			CHECK(added == 0.0f);
			CHECK(run.Added() == 0.0f);
			CHECK(near(run.From(), 800.0, 1e-3));
		}
		// a cycle shorter than the push itself: the next push follows straight on the last
		const PushTiming tight = { 0.1f, 0.2f, 0.05f };
		const Press hold[] = { { 0.0, 30.0 } };
		const PushLog log = runPush(200.0, 0.95, 0.0, hold, 1, 100, tight, 0, 0.0);
		CHECK_EQ(log.pushes, 3);
		// Reset: what was under way is gone
		PushRun gone;
		float w = 0.0f;
		for (int frame = 0; frame < 27; frame++) w += gone.Step(1.0f / 60.0f, w, frame < 3, 100, kTiming);
		CHECK_EQ(gone.Phase(), PUSH_CONTACT);
		gone.Reset();
		CHECK_EQ(gone.Phase(), PUSH_NONE);
		CHECK(!gone.Active());
		CHECK(!gone.Animating());
		for (int frame = 0; frame < 120; frame++) CHECK(gone.Step(1.0f / 60.0f, w, false, 100, kTiming) == 0.0f);
		// so is a press that was waiting for its turn
		{
			PushRun queued;
			float q = 0.0f;
			for (int frame = 0; frame < 20; frame++) {
				q += queued.Step(1.0f / 60.0f, q, frame < 3 || (frame >= 10 && frame < 13), 100, kTiming);
				if (frame == 0) queued.Taken();
			}
			CHECK_EQ(queued.Phase(), PUSH_CONTACT);
			queued.Reset();
			for (int frame = 0; frame < 240; frame++) {
				CHECK(queued.Step(1.0f / 60.0f, q, false, 100, kTiming) == 0.0f);
				CHECK_EQ(queued.Phase(), PUSH_NONE);
			}
			CHECK_EQ((int)queued.Serial(), 0);
		}
		// and a push that was only asking is gone as well
		PushRun asked;
		asked.Step(1.0f / 60.0f, 0.0f, true, 100, kTiming);
		CHECK_EQ(asked.Phase(), PUSH_ASKED);
		asked.Reset();
		CHECK_EQ(asked.Phase(), PUSH_NONE);
		asked.Taken();
		CHECK_EQ(asked.Phase(), PUSH_NONE);
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

int main(int argc, char** argv) {
	const std::string scriptDir = argc > 1 ? argv[1] : "scripts/qb/source/scripts/game/skater";
	testAgainstTheScripts(scriptDir);
	testJumps();
	testPushing();
	testCoasting();
	testSteering();
	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
