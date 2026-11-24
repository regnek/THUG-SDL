// Tests for peds.h: whether a car's box meets someone, the throw, the flight, the landing, the slide, and when
// someone run over is back on their way.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src tests/peds_test.cpp -o peds_test

#include "peds.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static int s_checks = 0;
static int s_failed = 0;

static void Check(bool ok, const char* what) {
	s_checks++;
	if (!ok) {
		s_failed++;
		std::printf("FAILED: %s\n", what);
	}
}

static bool Near(float a, float b, float within = 0.01f) {
	return std::fabs(a - b) <= within;
}

// A car of the usual size at (x, 0, z) facing along +z, going at speed along it
static Crash::Body CarAt(float x, float z, float speed) {
	const float pos[3] = { x, 0.0f, z };
	const float row0[3] = { 1.0f, 0.0f, 0.0f };
	const float row2[3] = { 0.0f, 0.0f, 1.0f };
	Crash::Body b;
	std::memset(&b, 0, sizeof(b));
	Crash::MakeBox(pos, row0, row2, 36.0f, -70.0f, 130.0f, 55.0f, &b);
	b.pivot[0] = x;
	b.pivot[1] = z;
	b.vel[2] = speed;
	return b;
}

static Peds::Level Ground(float y) {
	Peds::Level l;
	l.ground = true;
	l.groundY = y;
	l.wall = false;
	return l;
}

int main() {
	const Peds::Settings set = Peds::DefaultSettings();

	// the box: in front of the car's middle, at its sides, behind, above and below
	Crash::Body car = CarAt(0.0f, 0.0f, 800.0f);
	Peds::Strike st;
	const float inFront[3] = { 0.0f, 0.0f, 135.0f };
	Check(Peds::Struck(car, inFront, &st), "struck: just in front of its nose");
	Check(Near(st.speed, 800.0f) && Near(st.vel[2], 800.0f), "struck: the car's speed there");
	const float past[3] = { 0.0f, 0.0f, 130.0f + Peds::kRadius + 1.0f };
	Check(!Peds::Struck(car, past, &st), "struck: not past its nose");
	const float atSide[3] = { 36.0f + Peds::kRadius - 1.0f, 0.0f, 30.0f };
	Check(Peds::Struck(car, atSide, &st) && st.side > 0.0f, "struck: at its side, that side");
	const float otherSide[3] = { -40.0f, 0.0f, 30.0f };
	Check(Peds::Struck(car, otherSide, &st) && st.side < 0.0f, "struck: at its other side, that side");
	const float wide[3] = { 36.0f + Peds::kRadius + 1.0f, 0.0f, 30.0f };
	Check(!Peds::Struck(car, wide, &st), "struck: not wide of it");
	const float behind[3] = { 0.0f, 0.0f, -75.0f };
	Check(Peds::Struck(car, behind, &st), "struck: backing into them");
	const float above[3] = { 0.0f, 200.0f, 30.0f };
	Check(!Peds::Struck(car, above, &st), "struck: not someone on a roof above it");
	const float below[3] = { 0.0f, -200.0f, 30.0f };
	Check(!Peds::Struck(car, below, &st), "struck: not someone on a street below it");

	// a car turned a quarter (facing +x): its box turns with it
	{
		const float pos[3] = { 0.0f, 0.0f, 0.0f };
		const float row0[3] = { 0.0f, 0.0f, -1.0f };
		const float row2[3] = { 1.0f, 0.0f, 0.0f };
		Crash::Body t;
		std::memset(&t, 0, sizeof(t));
		Crash::MakeBox(pos, row0, row2, 36.0f, -70.0f, 130.0f, 55.0f, &t);
		const float ahead[3] = { 135.0f, 0.0f, 0.0f };
		const float aside[3] = { 0.0f, 0.0f, 135.0f };
		Check(Peds::Struck(t, ahead, &st) && !Peds::Struck(t, aside, &st), "struck: a car turned, its box turned");
	}

	// spinning: the speed at its nose is its spin's too
	{
		Crash::Body s = CarAt(0.0f, 0.0f, 0.0f);
		s.spin = 2.0f;
		const float nose[3] = { 0.0f, 0.0f, 130.0f };
		Check(Peds::Struck(s, nose, &st) && Near(st.vel[0], 260.0f) && Near(st.speed, 260.0f), "struck: a spinning car's speed at its nose");
	}

	// the throw: on and away from the car, up the more the faster, but not past the most
	{
		Peds::Flight f;
		Peds::Struck(car, atSide, &st);
		Peds::Throw(car, atSide, st, set, &f);
		Check(f.vel[2] > 0.0f && Near(f.vel[2], 800.0f * set.along), "throw: on the way the car went, a share of its speed");
		Check(f.vel[0] > 0.0f, "throw: away from the car's middle");
		Check(Near(f.vel[1], set.upBase + set.upPerSpeed * 800.0f), "throw: up for the speed");
		Check(f.facing[1] < -0.99f, "throw: facing where the car came from");
		Check(!f.landed && !f.still && Near(f.pos[2], atSide[2]), "throw: from where they stood");
		Crash::Body fast = CarAt(0.0f, 0.0f, 5000.0f);
		Peds::Struck(fast, inFront, &st);
		Peds::Throw(fast, inFront, st, set, &f);
		Check(Near(f.vel[1], set.upMost), "throw: no higher than the most");
		Crash::Body slow = CarAt(0.0f, 0.0f, 70.0f);
		Peds::Struck(slow, inFront, &st);
		Peds::Throw(slow, inFront, st, set, &f);
		Check(f.vel[1] < 130.0f && f.vel[1] >= set.upBase, "throw: a slow car, a small hop");
		Crash::Body still = CarAt(0.0f, 0.0f, 0.0f);
		Peds::Struck(still, inFront, &st);
		Peds::Throw(still, inFront, st, set, &f);
		Check(Near(f.facing[0], 0.0f) && Near(f.facing[1], -1.0f), "throw: a car at rest, facing its nose");
	}

	// the flight: up, down to the ground, a slide, and still; never below the ground
	{
		Peds::Flight f;
		Peds::Struck(car, inFront, &st);
		Peds::Throw(car, inFront, st, set, &f);
		const float up = f.vel[1];
		float highest = 0.0f, lowest = 0.0f;
		int landings = 0, steps = 0;
		while (!f.still && steps < 2000) {
			const bool landed = Peds::Step(&f, 1.0f / 60.0f, Ground(0.0f), set);
			landings += landed ? 1 : 0;
			highest = f.pos[1] > highest ? f.pos[1] : highest;
			lowest = f.pos[1] < lowest ? f.pos[1] : lowest;
			steps++;
		}
		const float expected = up * up / (2.0f * Crash::kGravity);
		Check(Near(highest, expected, expected * 0.05f), "flight: as high as the throw takes them");
		Check(landings == 1, "flight: one landing");
		Check(f.still && Near(f.pos[1], 0.0f) && lowest >= -0.001f, "flight: still on the ground, never below it");
		Check(f.pos[2] > 300.0f && f.pos[2] < 2500.0f, "flight: thrown on a way, not to the end of the level");
		Check(steps < 600, "flight: still within ten seconds");
		const float stopped = f.pos[2];
		Peds::Step(&f, 1.0f / 60.0f, Ground(0.0f), set);
		Check(Near(f.pos[2], stopped), "flight: still stays still");
	}

	// landing on a lower street; a slide follows the ground
	{
		Peds::Flight f;
		Peds::Struck(car, inFront, &st);
		Peds::Throw(car, inFront, st, set, &f);
		bool landed = false;
		for (int i = 0; i < 2000 && !landed; i++) {
			landed = Peds::Step(&f, 1.0f / 60.0f, Ground(-100.0f), set);
		}
		Check(landed && Near(f.pos[1], -100.0f), "landing: on the street below");
		Peds::Step(&f, 1.0f / 60.0f, Ground(-110.0f), set);
		Check(Near(f.pos[1], -110.0f), "slide: down with the ground");
	}

	// a wall: no further along, but down still
	{
		Peds::Flight f;
		Peds::Struck(car, inFront, &st);
		Peds::Throw(car, inFront, st, set, &f);
		Peds::Level wall = Ground(0.0f);
		wall.wall = true;
		const float z = f.pos[2];
		Peds::Step(&f, 1.0f / 60.0f, wall, set);
		Check(Near(f.pos[2], z) && f.pos[1] > 0.0f, "wall: not through it, still in the air");
	}

	// nothing under them: they stop where they are, in time
	{
		Peds::Flight f;
		Peds::Struck(car, inFront, &st);
		Peds::Throw(car, inFront, st, set, &f);
		Peds::Level none;
		none.ground = false;
		none.groundY = 0.0f;
		none.wall = false;
		int steps = 0;
		while (!f.still && steps < 2000) {
			Peds::Step(&f, 1.0f / 60.0f, none, set);
			steps++;
		}
		Check(f.still && !f.landed && f.seconds > Peds::kLongest - 0.1f && f.seconds < Peds::kLongest + 0.1f, "no ground: still after the longest flight");
	}

	// a long gap or none moves nothing
	{
		Peds::Flight f;
		Peds::Struck(car, inFront, &st);
		Peds::Throw(car, inFront, st, set, &f);
		const float z = f.pos[2];
		Check(!Peds::Step(&f, 0.0f, Ground(0.0f), set) && Near(f.pos[2], z), "step: no time, no move");
	}

	// back on their way: lain long enough, and he is far from both places
	{
		const float lie[3] = { 0.0f, 0.0f, 0.0f };
		const float start[3] = { 5000.0f, 0.0f, 0.0f };
		const float near[3] = { 500.0f, 0.0f, 0.0f };
		const float between[3] = { 2500.0f, 0.0f, 0.0f };
		const float atStart[3] = { 5200.0f, 0.0f, 0.0f };
		const float away[3] = { 0.0f, 0.0f, 4000.0f };
		Check(!Peds::BackOnTheirWay(30.0f, 10.0f, lie, start, near, 2000.0f), "back: not with him near where they lie");
		Check(!Peds::BackOnTheirWay(5.0f, 10.0f, lie, start, away, 2000.0f), "back: not before they have lain long enough");
		Check(!Peds::BackOnTheirWay(30.0f, 10.0f, lie, start, atStart, 2000.0f), "back: not with him near where they would be again");
		Check(Peds::BackOnTheirWay(30.0f, 10.0f, lie, start, between, 2000.0f), "back: with him far from both");
		Check(Peds::BackOnTheirWay(30.0f, 10.0f, lie, start, away, 2000.0f), "back: with him away");
		Check(Peds::BackOnTheirWay(30.0f, 10.0f, lie, 0, between, 2000.0f), "back: no start to check");
	}

	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
