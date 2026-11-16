// Offline tests for the physics numbers in src/flickit.cpp: what Flickit asks of the game's gravity
// and of the speed of a jump, against Skate's numbers and against THUG's own.
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

int main(int argc, char** argv) {
	const std::string scriptDir = argc > 1 ? argv[1] : "scripts/qb/source/scripts/game/skater";
	testAgainstTheScripts(scriptDir);
	testJumps();
	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
