// Tests for crash.h: the boxes, their touches, the impulses, a hard hit's launch, damage and its stages.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src/Flickit tests/crash_test.cpp -o crash_test

#include "crash.h"

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

// A car of the usual size facing heading (radians from +z towards +x), standing at (x, y, z)
static Crash::Body Car(float x, float y, float z, float heading, float invMass = 0.13f, float invInertia = 7.7e-5f) {
	const float row2[3] = { std::sin(heading), 0.0f, std::cos(heading) };
	const float row0[3] = { std::cos(heading), 0.0f, -std::sin(heading) };
	const float pos[3] = { x, y, z };
	Crash::Body b;
	std::memset(&b, 0, sizeof(b));
	Crash::MakeBox(pos, row0, row2, 36.0f, -70.0f, 140.0f, 56.0f, &b);
	b.invMass = invMass;
	b.invInertia = invInertia;
	return b;
}

static void TestBoxes() {
	const Crash::Body c = Car(100.0f, 0.0f, 200.0f, 0.0f);
	Check(Near(c.centre[0], 100.0f) && Near(c.centre[1], 235.0f), "box: the middle is between its back and front");
	Check(Near(c.halfX, 36.0f) && Near(c.halfZ, 105.0f), "box: half its width and length");
	Check(c.low < 0.0f && Near(c.high, 61.0f), "box: from a little under its wheels to over its roof");
	const Crash::Body turned = Car(0.0f, 0.0f, 0.0f, 1.5707963f);
	Check(Near(turned.axisZ[0], 1.0f) && Near(turned.axisZ[1], 0.0f) && Near(turned.centre[0], 35.0f), "box: turned a quarter, along x");
	// a car on its nose: the way along the ground comes from its side
	const float pos[3] = { 0, 0, 0 }, up[3] = { 0, 1, 0 }, side[3] = { 1, 0, 0 };
	Crash::Body nose;
	Check(Crash::MakeBox(pos, side, up, 36.0f, -70.0f, 140.0f, 56.0f, &nose) && Near(std::fabs(nose.axisZ[1]), 1.0f), "box: on its nose");
	const float flat[3] = { 0, 1, 0 };
	Check(!Crash::MakeBox(pos, flat, up, 36.0f, -70.0f, 140.0f, 56.0f, &nose), "box: no way along the ground at all");
}

static void TestOverlap() {
	Crash::Contact c;
	// side by side, apart
	Check(!Crash::Overlap(Car(0, 0, 0, 0), Car(80, 0, 0, 0), &c), "overlap: side by side with a gap");
	// side by side, 10 into each other: pushed apart along x, a from b
	Check(Crash::Overlap(Car(0, 0, 0, 0), Car(62, 0, 0, 0), &c), "overlap: side by side, touching");
	Check(Near(c.depth, 10.0f, 0.05f) && Near(c.normal[0], -1.0f) && Near(c.normal[2], 0.0f), "overlap: the normal from b to a, along x");
	Check(Near(c.point[0], 31.0f, 0.5f), "overlap: they touch between them");
	// nose to tail
	Check(Crash::Overlap(Car(0, 0, 0, 0), Car(0, 0, 200, 0), &c) && Near(c.normal[2], -1.0f) && Near(c.depth, 10.0f, 0.05f), "overlap: nose to tail");
	// one above the other: no touch when it is clear of the roof
	Check(!Crash::Overlap(Car(0, 200, 0, 0), Car(0, 0, 0, 0), &c), "overlap: one well above the other");
	// come down on its roof: up
	Check(Crash::Overlap(Car(10, 55, 20, 0.3f), Car(0, 0, 0, 0), &c) && Near(c.normal[1], 1.0f), "overlap: on the roof, pushed up");
	// across each other at an angle (a T-bone)
	Check(Crash::Overlap(Car(0, 0, 0, 0), Car(100, 0, 40, 1.5707963f), &c), "overlap: a T-bone");
	Check(c.normal[1] == 0.0f && Near(c.normal[0] * c.normal[0] + c.normal[2] * c.normal[2], 1.0f), "overlap: the normal is a unit along the ground");
}

static void TestResolve() {
	const Crash::Settings s = Crash::DefaultSettings();
	Crash::Contact c;
	// head on, same mass (b faces the other way: its front 140 back from where it stands): both bounce back
	// with a quarter of the speed
	Crash::Body a = Car(0, 0, 0, 0), b = Car(0, 0, 270, 3.1415927f);
	a.vel[2] = 500.0f;
	b.vel[2] = -500.0f;
	Check(Crash::Overlap(a, b, &c), "resolve: head on, touching");
	Crash::Result r;
	Check(Crash::Resolve(a, b, c, s, &r), "resolve: head on, an impulse");
	const float va = a.vel[2] + r.impulse[2] * a.invMass, vb = b.vel[2] - r.impulse[2] * b.invMass;
	Check(Near(r.approach, 1000.0f, 0.5f), "resolve: how fast they came together");
	Check(va < 0.0f && vb > 0.0f && Near(vb - va, 250.0f, 2.0f), "resolve: apart at a quarter of the speed");
	Check(Near(r.hitA, 625.0f, 3.0f) && Near(r.hitB, 625.0f, 3.0f), "resolve: each one's speed changed by as much");
	Check(Near(r.spinA, 0.0f, 1e-3f) && Near(r.spinB, 0.0f, 1e-3f), "resolve: square on, no spin");
	// a car that cannot move: the other takes it all
	b = Car(0, 0, 270, 3.1415927f, 0.0f, 0.0f);
	b.vel[2] = 0.0f;
	a.vel[2] = 400.0f;
	Crash::Overlap(a, b, &c);
	Crash::Resolve(a, b, c, s, &r);
	Check(Near(a.vel[2] + r.impulse[2] * a.invMass, -100.0f, 2.0f) && r.hitB == 0.0f, "resolve: against what does not move, a quarter back");
	// apart already and barely in each other: nothing
	a.vel[2] = -50.0f;
	Check(!Crash::Resolve(a, b, c, s, &r) || r.approach == 0.0f, "resolve: moving apart, no hit");
	// deep in each other and still: pushed apart, but no hit to speak of
	Crash::Body d = Car(0, 0, 0, 0), e = Car(0, 0, 150, 0);
	Check(Crash::Overlap(d, e, &c) && c.depth > 30.0f, "resolve: deep in each other");
	Check(Crash::Resolve(d, e, c, s, &r) && r.approach == 0.0f && r.hitA == 0.0f, "resolve: still, pushed apart only");
	Check(r.impulse[2] < 0.0f && -r.impulse[2] * d.invMass * 2.0f <= s.maxBias + 1.0f, "resolve: apart at no more than the most");
	// hit off its middle: it spins
	Crash::Body f = Car(0, 0, 0, 0), g = Car(60, 0, 150, 0);
	f.vel[2] = 600.0f;
	Check(Crash::Overlap(f, g, &c), "resolve: hit by its corner");
	Check(Crash::Resolve(f, g, c, s, &r) && std::fabs(r.spinB) > 0.01f, "resolve: hit off its middle, it spins");
	// sliding along each other: friction slows the slide
	Crash::Body h = Car(0, 0, 0, 0), k = Car(70, 0, 30, 0);
	h.vel[0] = 100.0f;
	h.vel[2] = 600.0f;
	Check(Crash::Overlap(h, k, &c) && Crash::Resolve(h, k, c, s, &r), "resolve: a scrape");
	Check(r.impulse[2] < 0.0f && std::fabs(r.impulse[2]) <= s.friction * std::fabs(r.impulse[0]) + 0.01f, "resolve: the friction no more than its share");
	// the game's drivable car turns about its place (its back axle), not its box's middle: a hit square on its
	// nose turns it unless the hit is through that place
	Crash::Body m = Car(0, 0, 0, 0), n = Car(30, 0, 0, 0, 0.0f, 0.0f);
	m.pivot[0] = 0.0f;
	m.pivot[1] = 0.0f;
	m.vel[0] = 300.0f;
	Check(Crash::Overlap(m, n, &c) && Crash::Resolve(m, n, c, s, &r) && std::fabs(r.spinA) > 0.01f, "resolve: turned about its pivot");
	// from above: straight up, no turn
	Crash::Body top = Car(10, 55, 20, 0.3f), under = Car(0, 0, 0, 0);
	top.vel[1] = -300.0f;
	Check(Crash::Overlap(top, under, &c) && Crash::Resolve(top, under, c, s, &r) && r.impulse[1] > 0.0f, "resolve: come down on a roof, pushed up");
}

static void TestLaunch() {
	const float push[3] = { 1.0f, 0.0f, 0.0f };
	const Crash::Launch none = Crash::LaunchFor(500.0f, push, 650.0f, 1.0f);
	Check(none.up == 0.0f && none.roll == 0.0f, "launch: not below the speed");
	const Crash::Launch l = Crash::LaunchFor(1150.0f, push, 650.0f, 1.0f);
	Check(Near(l.up, 400.0f) && Near(l.roll, 4.0f), "launch: lifted and rolled for the speed past it");
	// the roof goes the way it is pushed: axis x up = push
	const float up[3] = { 0, 1, 0 };
	const float x = l.axis[1] * up[2] - l.axis[2] * up[1], z = l.axis[0] * up[1] - l.axis[1] * up[0];
	Check(Near(x, 1.0f) && Near(z, 0.0f), "launch: it rolls the way it is pushed");
	const Crash::Launch huge = Crash::LaunchFor(9000.0f, push, 650.0f, 1.0f);
	Check(huge.up <= 700.0f && huge.roll <= 6.0f, "launch: no more than the most");
	const float down[3] = { 0, -1, 0 };
	Check(Crash::LaunchFor(2000.0f, down, 650.0f, 1.0f).up == 0.0f, "launch: not for a car come down on another");
	Check(Crash::LaunchFor(2000.0f, push, 650.0f, 0.0f).up == 0.0f, "launch: switched off");
}

static void TestDamage() {
	Check(Crash::Damage(80.0f, 100.0f, 1.0f) == 0.0f && Near(Crash::Damage(400.0f, 100.0f, 1.0f), 300.0f), "damage: past the floor");
	Crash::Health h = Crash::Whole();
	Check(Crash::StageOf(h) == Crash::kWhole, "stage: whole");
	Check(Crash::Hurt(&h, 450.0f, 1000) == Crash::kNothing && Crash::StageOf(h) == Crash::kSmoking, "stage: smoking under 600");
	Crash::Hurt(&h, 300.0f, 1500);
	Check(Crash::StageOf(h) == Crash::kSmokingHard, "stage: smoking hard under 300");
	Check(Crash::Hurt(&h, 400.0f, 2000) == Crash::kCaughtFire && Crash::StageOf(h) == Crash::kBurning && h.hp == 0.0f, "stage: on fire at nothing left");
	Check(Crash::Hurt(&h, 400.0f, 2500) == Crash::kNothing, "stage: catches fire once");
	Check(Crash::Tick(&h, 2000 + Crash::kBurnMs - 1) == Crash::kNothing, "stage: burns a while");
	Check(Crash::Tick(&h, 2000 + Crash::kBurnMs) == Crash::kBlewUp && Crash::StageOf(h) == Crash::kBurnt, "stage: then blows up");
	Check(Crash::Tick(&h, 99999) == Crash::kNothing && Crash::Hurt(&h, 500.0f, 99999) == Crash::kNothing, "stage: burnt is the end");
	// the clock going round past zero
	Crash::Health w = Crash::Whole();
	Crash::Hurt(&w, 2000.0f, 0xFFFFF000u);
	Check(Crash::Tick(&w, 0xFFFFF000u + Crash::kBurnMs) == Crash::kBlewUp, "stage: across the clock's wrap");
	Check(Near(Crash::Blast(0.0f, 500.0f), 1.0f) && Near(Crash::Blast(250.0f, 500.0f), 0.25f) && Crash::Blast(600.0f, 500.0f) == 0.0f, "blast: less farther away");
}

// A loose car of the usual size with its model's origin at (x, y, z), facing +z, at rest
static Crash::Tumbler Loose(float x, float y, float z) {
	const float origin[3] = { x, y, z };
	const float rows[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	Crash::Tumbler t;
	Crash::MakeTumbler(origin, rows, 36.0f, -70.0f, 140.0f, 56.0f, 0.13f, &t);
	return t;
}

static Crash::Plane Ground(float y) {
	Crash::Plane p = { { 0.0f, y, 0.0f }, { 0.0f, 1.0f, 0.0f } };
	return p;
}

static float Speed(const Crash::Tumbler& t) {
	return std::sqrt(t.vel[0] * t.vel[0] + t.vel[1] * t.vel[1] + t.vel[2] * t.vel[2]);
}

// seconds of it at the game's 60 frames a second, against these planes; whether it came to rest
static bool Run(Crash::Tumbler* t, float seconds, const Crash::Plane* planes, int n, float* hardest = 0) {
	const Crash::TumbleSettings s = Crash::DefaultTumble();
	bool rest = false;
	for (int f = 0; f < (int)(seconds * 60.0f); f++) {
		const Crash::TumbleResult r = Crash::Tumble(t, 1.0f / 60.0f, planes, n, s);
		rest = rest || r.cameToRest;
		if (hardest && r.hit > *hardest) {
			*hardest = r.hit;
		}
	}
	return rest;
}

static void TestTumbler() {
	// made from where it stands, and back
	Crash::Tumbler t = Loose(100.0f, 5.0f, -40.0f);
	Check(Near(t.com[0], 100.0f) && Near(t.com[1], 33.0f) && Near(t.com[2], -5.0f), "tumbler: its middle above and ahead of its origin");
	float origin[3];
	Crash::OriginOf(t, -70.0f, 140.0f, origin);
	Check(Near(origin[0], 100.0f) && Near(origin[1], 5.0f) && Near(origin[2], -40.0f), "tumbler: its origin from its middle");
	Check(t.asleep && Crash::Upright(t), "tumbler: made at rest, on its wheels");
	Check(t.invInertia[2] > t.invInertia[0] && t.invInertia[2] > t.invInertia[1], "tumbler: it rolls over more easily than it turns");

	// asleep: a step does nothing
	const Crash::Plane ground = Ground(0.0f);
	Crash::Tumbler still = Loose(0, 0, 0);
	Crash::Tumble(&still, 1.0f / 60.0f, &ground, 1, Crash::DefaultTumble());
	Check(Near(still.com[1], 28.0f), "tumbler: at rest it stays");

	// dropped from a metre and a half: it lands, settles on its wheels and comes to rest there
	Crash::Tumbler drop = Loose(0, 60, 0);
	Crash::Wake(&drop);
	float hardest = 0.0f;
	const bool rested = Run(&drop, 4.0f, &ground, 1, &hardest);
	Check(rested && drop.asleep, "tumbler: dropped, it comes to rest");
	Check(Near(drop.com[1], 28.0f, 1.5f) && Crash::Upright(drop), "tumbler: on the ground, on its wheels");
	Check(hardest > 200.0f, "tumbler: the landing is a hit");

	// rolling along the way it faces it keeps going a while; across it, the tyres stop it
	Crash::Tumbler along = Loose(0, 0, 0);
	Crash::Wake(&along);
	along.vel[2] = 300.0f;
	Run(&along, 1.0f, &ground, 1);
	Check(along.vel[2] > 200.0f && along.vel[2] < 290.0f, "tumbler: on its wheels it rolls on");
	Crash::Tumbler across = Loose(0, 0, 0);
	Crash::Wake(&across);
	across.vel[0] = 300.0f;
	Run(&across, 1.5f, &ground, 1);
	Check(std::fabs(across.vel[0]) < 20.0f && across.com[0] > 50.0f && across.com[0] < 250.0f, "tumbler: pushed sideways it slides and stops");

	// thrown up and rolled over: it comes down on some side of it and rests there, on the ground
	Crash::Tumbler thrown = Loose(0, 0, 0);
	const float axis[3] = { 0.0f, 0.0f, 1.0f };
	Crash::Throw(&thrown, 450.0f, 5.0f, axis);
	thrown.vel[0] = 250.0f;
	Check(Run(&thrown, 8.0f, &ground, 1), "tumbler: rolled over, it comes to rest");
	bool onASide = false;
	for (int k = 0; k < 3; k++) {
		onASide = onASide || Near(thrown.com[1], thrown.half[k], 2.0f);
	}
	Check(onASide && thrown.com[1] > 0.0f, "tumbler: rests on one of its sides, on the ground");
	const float* r0 = thrown.rows[0];
	const float* r2 = thrown.rows[2];
	Check(Near(Crash::Dot3(r0, r2), 0.0f, 1e-3f) && Near(Crash::Length3(thrown.rows[1]), 1.0f, 1e-3f), "tumbler: its axes still square");

	// into a wall: it does not go through
	Crash::Tumbler wallward = Loose(0, 0, 0);
	Crash::Wake(&wallward);
	wallward.vel[0] = 600.0f;
	const Crash::Plane planes[2] = { ground, { { 200.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } } };
	hardest = 0.0f;
	Run(&wallward, 2.0f, planes, 2, &hardest);
	Check(wallward.com[0] + wallward.half[0] < 203.0f && hardest > 300.0f, "tumbler: stopped by the wall, hard");

	// hit off its middle along the ground: it turns
	Crash::Tumbler spun = Loose(0, 0, 0);
	Crash::Contact c;
	std::memset(&c, 0, sizeof(c));
	c.point[0] = 36.0f;
	c.point[1] = 100.0f;
	const float impulse[3] = { 0.0f, 0.0f, -800.0f };
	Crash::TakeHit(&spun, c, impulse, 1.0f);
	Check(!spun.asleep && spun.vel[2] < 0.0f && std::fabs(spun.w[1]) > 0.05f, "tumbler: hit off its middle, it moves and turns");

	// its box along the ground: on its wheels the car's own; on its side, its height across
	Crash::Body b;
	const Crash::Tumbler flat = Loose(0, 0, 0);
	Check(Crash::FootprintOf(flat, &b) && Near(b.halfX, 36.0f) && Near(b.halfZ, 105.0f) && Near(b.low, 0.0f) && Near(b.high, 56.0f), "footprint: on its wheels");
	Crash::Tumbler side = Loose(0, 0, 0);
	const float rows[3][3] = { { 0, 1, 0 }, { -1, 0, 0 }, { 0, 0, 1 } };
	std::memcpy(side.rows, rows, sizeof(rows));
	Check(Crash::FootprintOf(side, &b) && Near(b.halfX, 28.0f) && Near(b.halfZ, 105.0f) && Near(b.high - b.low, 72.0f), "footprint: on its side");
	Check(Near(b.invInertia, side.invInertia[0], 1e-6f), "footprint: turning about up is turning about its left");
	Check(Speed(flat) == 0.0f, "footprint: still");
}

int main() {
	TestBoxes();
	TestOverlap();
	TestResolve();
	TestLaunch();
	TestDamage();
	TestTumbler();
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
