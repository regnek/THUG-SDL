#pragma once
// Flickit's car crashes: the parts without the game (crash_game.cpp puts them in the game).
//
// The game's drivable car has a rigid body of its own but goes through other cars, and the traffic follows
// its ways with no physics at all. So the crashes are worked out here: each car is a box along the ground
// (a rectangle turned with it, between two heights), two boxes that overlap touch, and the touch is an
// impulse that changes how both move and how they spin about the up axis, with the friction of the touch
// along it. A car that cannot move (a car of the traffic not yet knocked off its way) has no inverse mass.
// A hard hit lifts the car hit and rolls it over the way it was hit; the impulse each car took is its damage.
// Damage goes through smoke, heavy smoke and fire to an explosion, which throws the car up and pushes the
// cars around it away.
//
// Units are the game's: inches, seconds, and the masses as the game's own drivable car takes them (its
// setup's weight in pounds over 386.4, the gravity in inches a second squared). Up is +y. A spin is about
// +y: a point r from the middle (x, z along the ground) moves at (spin * r.z, -spin * r.x).
//
// Pure, tested by tests/crash_test.cpp.
#include <math.h>
#include <stdint.h>
#include <string.h>

namespace Crash {
	static const float kGravity = 386.4f;

	struct Body {
		float centre[2];        // the middle of its rectangle along the ground (x, z)
		float axisX[2];         // its sideways along the ground, a unit
		float axisZ[2];         // the way it faces along the ground, a unit
		float halfX, halfZ;     // half its width and half its length
		float low, high;        // the heights its box spans
		float pivot[2];         // what it turns about (its middle; the game's drivable car turns about its place)
		float vel[3];           // of its pivot
		float spin;             // radians a second about +y
		float invMass;          // 0: it does not move
		float invInertia;       // about +y; 0: it does not spin
	};

	// A car's box from where it is (its origin on the ground, as the game's cars have it), its matrix's rows
	// (row 0 sideways, row 2 the way it faces) and its body: halfWidth, rear (z of its back, below 0), front and
	// height. False when the matrix has no way along the ground (a car standing on its nose).
	inline bool MakeBox(const float pos[3], const float row0[3], const float row2[3], float halfWidth, float rear, float front,
		float height, Body* b) {
		float zx = row2[0], zz = row2[2];
		float len = sqrtf(zx * zx + zz * zz);
		if (len < 0.2f) {
			// facing up or down: the way along the ground is the sideways one turned
			zx = -row0[2];
			zz = row0[0];
			len = sqrtf(zx * zx + zz * zz);
			if (len < 0.2f) {
				return false;
			}
		}
		b->axisZ[0] = zx / len;
		b->axisZ[1] = zz / len;
		b->axisX[0] = b->axisZ[1];
		b->axisX[1] = -b->axisZ[0];
		const float mid = 0.5f * (front + rear);
		b->centre[0] = pos[0] + b->axisZ[0] * mid;
		b->centre[1] = pos[2] + b->axisZ[1] * mid;
		b->halfX = halfWidth;
		b->halfZ = 0.5f * (front - rear);
		b->low = pos[1] - 15.0f;
		b->high = pos[1] + height + 5.0f;
		b->pivot[0] = b->centre[0];
		b->pivot[1] = b->centre[1];
		return true;
	}

	struct Contact {
		float normal[3];        // from b towards a, a unit (along the ground, or straight up or down)
		float depth;            // how far they go into each other along it
		float point[2];         // where they touch, along the ground
	};

	inline void Corners(const Body& b, float out[4][2]) {
		static const float s[4][2] = { { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 } };
		for (int i = 0; i < 4; i++) {
			out[i][0] = b.centre[0] + b.axisX[0] * b.halfX * s[i][0] + b.axisZ[0] * b.halfZ * s[i][1];
			out[i][1] = b.centre[1] + b.axisX[1] * b.halfX * s[i][0] + b.axisZ[1] * b.halfZ * s[i][1];
		}
	}

	inline bool Inside(const Body& b, const float p[2], float margin) {
		const float dx = p[0] - b.centre[0], dz = p[1] - b.centre[1];
		return fabsf(dx * b.axisX[0] + dz * b.axisX[1]) <= b.halfX + margin && fabsf(dx * b.axisZ[0] + dz * b.axisZ[1]) <= b.halfZ + margin;
	}

	inline float Radius(const Body& b, const float axis[2]) {
		return b.halfX * fabsf(b.axisX[0] * axis[0] + b.axisX[1] * axis[1]) + b.halfZ * fabsf(b.axisZ[0] * axis[0] + b.axisZ[1] * axis[1]);
	}

	// Do two boxes touch, and how: the axis of least overlap of their rectangles (along the ground), or up or
	// down when one is mostly above the other (a car come down on another's roof)
	inline bool Overlap(const Body& a, const Body& b, Contact* c) {
		const float vertical = (a.high < b.high ? a.high : b.high) - (a.low > b.low ? a.low : b.low);
		if (vertical <= 0.0f) {
			return false;
		}
		const float d[2] = { a.centre[0] - b.centre[0], a.centre[1] - b.centre[1] };
		const float* axes[4] = { a.axisX, a.axisZ, b.axisX, b.axisZ };
		float best = 1.0e30f;
		int bestAxis = -1;
		for (int i = 0; i < 4; i++) {
			const float* l = axes[i];
			const float dist = fabsf(d[0] * l[0] + d[1] * l[1]);
			const float overlap = Radius(a, l) + Radius(b, l) - dist;
			if (overlap <= 0.0f) {
				return false;
			}
			if (overlap < best) {
				best = overlap;
				bestAxis = i;
			}
		}
		const float* l = axes[bestAxis];
		const float side = d[0] * l[0] + d[1] * l[1] >= 0.0f ? 1.0f : -1.0f;
		// where they touch: the corners of each inside the other, or between the two nearest corners
		float ca[4][2], cb[4][2], sum[2] = { 0.0f, 0.0f };
		Corners(a, ca);
		Corners(b, cb);
		int n = 0;
		for (int i = 0; i < 4; i++) {
			if (Inside(b, ca[i], 0.5f)) {
				sum[0] += ca[i][0];
				sum[1] += ca[i][1];
				n++;
			}
			if (Inside(a, cb[i], 0.5f)) {
				sum[0] += cb[i][0];
				sum[1] += cb[i][1];
				n++;
			}
		}
		if (n > 0) {
			c->point[0] = sum[0] / n;
			c->point[1] = sum[1] / n;
		}
		else {
			// the corner of each furthest towards the other
			int ia = 0, ib = 0;
			float fa = 1.0e30f, fb = -1.0e30f;
			for (int i = 0; i < 4; i++) {
				const float pa = (ca[i][0] * l[0] + ca[i][1] * l[1]) * side;
				const float pb = (cb[i][0] * l[0] + cb[i][1] * l[1]) * side;
				if (pa < fa) {
					fa = pa;
					ia = i;
				}
				if (pb > fb) {
					fb = pb;
					ib = i;
				}
			}
			c->point[0] = 0.5f * (ca[ia][0] + cb[ib][0]);
			c->point[1] = 0.5f * (ca[ia][1] + cb[ib][1]);
		}
		// one mostly above the other: they meet roof to wheels
		const float aMid = 0.5f * (a.low + a.high), bMid = 0.5f * (b.low + b.high);
		if (vertical < best && vertical < 0.35f * ((a.high - a.low) < (b.high - b.low) ? (a.high - a.low) : (b.high - b.low))) {
			c->normal[0] = 0.0f;
			c->normal[1] = aMid >= bMid ? 1.0f : -1.0f;
			c->normal[2] = 0.0f;
			c->depth = vertical;
			return true;
		}
		c->normal[0] = l[0] * side;
		c->normal[1] = 0.0f;
		c->normal[2] = l[1] * side;
		c->depth = best;
		return true;
	}

	struct Settings {
		float restitution;      // how much of the speed into each other comes back
		float friction;         // along the touch, at most this times the push
		float slop;             // they may be in each other this far before they are pushed apart
		float biasRate;         // pushed apart at this many units a second for each unit in each other
		float maxBias;          // at most this fast
	};

	inline Settings DefaultSettings() {
		Settings s;
		s.restitution = 0.25f;
		s.friction = 0.35f;
		s.slop = 2.0f;
		s.biasRate = 8.0f;
		s.maxBias = 240.0f;
		return s;
	}

	struct Result {
		float impulse[3];       // on a (b takes the opposite)
		float spinA, spinB;     // the change of their spins
		float approach;         // how fast they came into each other (0: they were not)
		float hitA, hitB;       // how much each one's speed changed from the hit itself (not the push apart)
	};

	// The impulse of a touch: how both move and spin after it is theirs plus this. False when nothing changes
	// (neither can move, or they were already apart and are not in each other past the slop).
	inline bool Resolve(const Body& a, const Body& b, const Contact& c, const Settings& s, Result* r) {
		memset(r, 0, sizeof(*r));
		const float* n = c.normal;
		const float ra[2] = { c.point[0] - a.pivot[0], c.point[1] - a.pivot[1] };
		const float rb[2] = { c.point[0] - b.pivot[0], c.point[1] - b.pivot[1] };
		const float va[3] = { a.vel[0] + a.spin * ra[1], a.vel[1], a.vel[2] - a.spin * ra[0] };
		const float vb[3] = { b.vel[0] + b.spin * rb[1], b.vel[1], b.vel[2] - b.spin * rb[0] };
		const float rel[3] = { va[0] - vb[0], va[1] - vb[1], va[2] - vb[2] };
		const float vn = rel[0] * n[0] + rel[1] * n[1] + rel[2] * n[2];
		const float ka = ra[1] * n[0] - ra[0] * n[2], kb = rb[1] * n[0] - rb[0] * n[2];
		const float denom = a.invMass + b.invMass + ka * ka * a.invInertia + kb * kb * b.invInertia;
		if (denom <= 0.0f) {
			return false;
		}
		float jn = 0.0f;
		if (vn < 0.0f) {
			jn = -(1.0f + s.restitution) * vn / denom;
			r->approach = -vn;
		}
		const float hit = jn;
		// pushed apart when in each other past the slop, at a speed for how far
		if (c.depth > s.slop) {
			float bias = (c.depth - s.slop) * s.biasRate;
			bias = bias > s.maxBias ? s.maxBias : bias;
			const float after = vn + jn * denom;
			if (after < bias) {
				jn += (bias - after) / denom;
			}
		}
		if (jn <= 0.0f) {
			return false;
		}
		// friction along the touch, against the way they slide on each other
		float t[3] = { rel[0] - vn * n[0], rel[1] - vn * n[1], rel[2] - vn * n[2] };
		const float tl = sqrtf(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
		float jt = 0.0f;
		if (tl > 1.0f) {
			for (int i = 0; i < 3; i++) {
				t[i] /= tl;
			}
			const float kat = ra[1] * t[0] - ra[0] * t[2], kbt = rb[1] * t[0] - rb[0] * t[2];
			const float denomT = a.invMass + b.invMass + kat * kat * a.invInertia + kbt * kbt * b.invInertia;
			if (denomT > 0.0f) {
				jt = -tl / denomT;
				const float most = s.friction * jn;
				jt = jt < -most ? -most : jt;
			}
		}
		for (int i = 0; i < 3; i++) {
			r->impulse[i] = jn * n[i] + jt * t[i];
		}
		r->spinA = a.invInertia * (ra[1] * r->impulse[0] - ra[0] * r->impulse[2]);
		r->spinB = -b.invInertia * (rb[1] * r->impulse[0] - rb[0] * r->impulse[2]);
		r->hitA = hit * a.invMass;
		r->hitB = hit * b.invMass;
		return true;
	}

	// A hard hit lifts the car hit and rolls it over: from launchFrom (units a second into each other) up, more
	// for more. push: the way along the ground the car is pushed (for b of a touch, the opposite of its
	// normal). up: units a second upwards; roll: radians a second about axis, a unit along the ground across
	// the push, so that its roof goes the way it is pushed (axis x up = push).
	struct Launch {
		float up;
		float roll;
		float axis[3];
	};

	inline Launch LaunchFor(float approach, const float push[3], float launchFrom, float scale) {
		Launch l;
		memset(&l, 0, sizeof(l));
		if (approach <= launchFrom || scale <= 0.0f || push[1] != 0.0f) {
			return l;
		}
		const float over = approach - launchFrom;
		l.up = over * 0.8f * scale;
		l.up = l.up > 700.0f ? 700.0f : l.up;
		l.roll = over * 0.008f * scale;
		l.roll = l.roll > 6.0f ? 6.0f : l.roll;
		l.axis[0] = push[2];
		l.axis[1] = 0.0f;
		l.axis[2] = -push[0];
		return l;
	}

	// The damage of a hit: the change of the car's speed past a floor, scaled
	inline float Damage(float speedChange, float floor, float scale) {
		return speedChange > floor ? (speedChange - floor) * scale : 0.0f;
	}

	// How a car stands: from its health and whether it has blown up
	enum Stage { kWhole, kSmoking, kSmokingHard, kBurning, kBurnt };
	static const float kHealth = 1000.0f;
	static const float kSmokeBelow = 600.0f;
	static const float kHardSmokeBelow = 300.0f;
	static const uint32_t kBurnMs = 5000;            // a car on fire blows up after this

	struct Health {
		float hp;
		bool burning;
		uint32_t burningSince;
		bool burnt;
	};

	inline Health Whole() {
		Health h;
		h.hp = kHealth;
		h.burning = false;
		h.burningSince = 0;
		h.burnt = false;
		return h;
	}

	inline Stage StageOf(const Health& h) {
		if (h.burnt) {
			return kBurnt;
		}
		if (h.burning) {
			return kBurning;
		}
		return h.hp < kHardSmokeBelow ? kSmokingHard : h.hp < kSmokeBelow ? kSmoking : kWhole;
	}

	enum Event { kNothing, kCaughtFire, kBlewUp };

	// Damage taken now: it may catch fire (at no health left)
	inline Event Hurt(Health* h, float damage, uint32_t now) {
		if (h->burnt || damage <= 0.0f) {
			return kNothing;
		}
		h->hp -= damage;
		if (h->hp <= 0.0f) {
			h->hp = 0.0f;
			if (!h->burning) {
				h->burning = true;
				h->burningSince = now;
				return kCaughtFire;
			}
		}
		return kNothing;
	}

	// As time goes by: a car on fire long enough blows up
	inline Event Tick(Health* h, uint32_t now) {
		if (h->burning && !h->burnt && now - h->burningSince >= kBurnMs) {
			h->burning = false;
			h->burnt = true;
			return kBlewUp;
		}
		return kNothing;
	}

	// The push of an explosion on what is this far from it: 1 at the middle, nothing from radius on
	inline float Blast(float distance, float radius) {
		if (radius <= 0.0f || distance >= radius) {
			return 0.0f;
		}
		const float k = 1.0f - (distance < 0.0f ? 0.0f : distance) / radius;
		return k * k;
	}

	/******************************************************************/
	/*                                                                */
	/*  A loose car: a box that tumbles                               */
	/*                                                                */
	/******************************************************************/

	// A car knocked off its way (or left, and then hit) is a box of its own that falls, bounces, slides, rolls
	// and comes to rest against the ground and the walls next to it, which the game gives as planes (the place
	// a line from its middle hit and the face's normal there). The box is the car's body: from under its wheels
	// to its roof, its width, its length. On its wheels it rolls along the way it faces and holds across it.
	//
	// rows are its axes in the world, as the game's matrix has them: its left, up, the way it faces. w is how it
	// spins (radians a second about each axis of the world); a point r from its middle moves at vel + w x r.
	struct Tumbler {
		float com[3];
		float vel[3];
		float w[3];
		float rows[3][3];
		float half[3];          // half its size along its rows
		float invMass;
		float invInertia[3];    // about its own rows
		float stillSeconds;     // how long it has been nearly still on the ground
		bool asleep;            // at rest: not moved until something hits it
	};

	struct Plane {
		float point[3];
		float normal[3];        // a unit, out of the face
	};

	struct TumbleSettings {
		float gravity;
		float restitution;      // of a corner that hits faster than bounceFrom
		float bounceFrom;
		float friction;         // its body against the ground or a wall
		float wheelRoll;        // on its wheels, along the way it faces (it rolls)
		float wheelSide;        // on its wheels, across (the tyres hold)
		float slop;
		float biasRate;
		float maxBias;
		float linearDamping;    // a second
		float angularDamping;
		float sleepSpeed;
		float sleepSpin;
		float sleepAfter;       // seconds that still before it is at rest
		float maxSpeed;
		float maxSpin;
		int iterations;
	};

	inline TumbleSettings DefaultTumble() {
		TumbleSettings s;
		s.gravity = kGravity;
		s.restitution = 0.2f;
		s.bounceFrom = 40.0f;
		s.friction = 0.55f;
		s.wheelRoll = 0.12f;
		s.wheelSide = 0.9f;
		s.slop = 0.5f;
		s.biasRate = 12.0f;
		s.maxBias = 300.0f;
		s.linearDamping = 0.05f;
		s.angularDamping = 0.3f;
		s.sleepSpeed = 8.0f;
		s.sleepSpin = 0.15f;
		s.sleepAfter = 0.5f;
		s.maxSpeed = 4000.0f;
		s.maxSpin = 15.0f;
		s.iterations = 8;
		return s;
	}

	inline float Dot3(const float a[3], const float b[3]) {
		return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	}
	inline void Cross3(const float a[3], const float b[3], float out[3]) {
		const float x = a[1] * b[2] - a[2] * b[1], y = a[2] * b[0] - a[0] * b[2], z = a[0] * b[1] - a[1] * b[0];
		out[0] = x;
		out[1] = y;
		out[2] = z;
	}
	inline float Length3(const float a[3]) {
		return sqrtf(Dot3(a, a));
	}
	inline bool Normalize3(float a[3]) {
		const float l = Length3(a);
		if (!(l > 1.0e-6f)) {
			return false;
		}
		a[0] /= l;
		a[1] /= l;
		a[2] /= l;
		return true;
	}

	// A car's box as a tumbler: its model's origin (on the ground, as a car of the traffic stands), its rows, its
	// body (halfWidth, rear, front, height), its inverse mass. At rest. The inertia is a solid box's.
	inline void MakeTumbler(const float origin[3], const float rows[3][3], float halfWidth, float rear, float front, float height,
		float invMass, Tumbler* t) {
		memset(t, 0, sizeof(*t));
		memcpy(t->rows, rows, sizeof(t->rows));
		t->half[0] = halfWidth;
		t->half[1] = 0.5f * height;
		t->half[2] = 0.5f * (front - rear);
		const float up = 0.5f * height, along = 0.5f * (front + rear);
		for (int i = 0; i < 3; i++) {
			t->com[i] = origin[i] + rows[1][i] * up + rows[2][i] * along;
		}
		t->invMass = invMass;
		const float a = t->half[0], b = t->half[1], c = t->half[2];
		const float k = invMass * 3.0f;
		t->invInertia[0] = k / (b * b + c * c);
		t->invInertia[1] = k / (a * a + c * c);
		t->invInertia[2] = k / (a * a + b * b);
		t->asleep = true;
	}

	// Its model's origin, for the game (the box's middle back down and back along)
	inline void OriginOf(const Tumbler& t, float rear, float front, float out[3]) {
		const float up = t.half[1], along = 0.5f * (front + rear);
		for (int i = 0; i < 3; i++) {
			out[i] = t.com[i] - t.rows[1][i] * up - t.rows[2][i] * along;
		}
	}

	// I^-1 v, in the world: v along its rows, each by its own, back
	inline void InvInertiaTimes(const Tumbler& t, const float v[3], float out[3]) {
		float b[3];
		for (int k = 0; k < 3; k++) {
			b[k] = Dot3(t.rows[k], v) * t.invInertia[k];
		}
		for (int i = 0; i < 3; i++) {
			out[i] = t.rows[0][i] * b[0] + t.rows[1][i] * b[1] + t.rows[2][i] * b[2];
		}
	}

	// An impulse p at r from its middle
	inline void Push(Tumbler* t, const float r[3], const float p[3]) {
		for (int i = 0; i < 3; i++) {
			t->vel[i] += p[i] * t->invMass;
		}
		float torque[3], dw[3];
		Cross3(r, p, torque);
		InvInertiaTimes(*t, torque, dw);
		for (int i = 0; i < 3; i++) {
			t->w[i] += dw[i];
		}
	}

	// How hard it is to change the speed of the point r along n: 1 over this is the impulse for a unit of it
	inline float Stiffness(const Tumbler& t, const float r[3], const float n[3]) {
		float rn[3], irn[3], back[3];
		Cross3(r, n, rn);
		InvInertiaTimes(t, rn, irn);
		Cross3(irn, r, back);
		return t.invMass + Dot3(back, n);
	}

	inline void PointVelocity(const Tumbler& t, const float r[3], float out[3]) {
		Cross3(t.w, r, out);
		for (int i = 0; i < 3; i++) {
			out[i] += t.vel[i];
		}
	}

	inline void CornerOf(const Tumbler& t, int i, float out[3]) {
		const float sx = (i & 1) ? 1.0f : -1.0f, sy = (i & 2) ? 1.0f : -1.0f, sz = (i & 4) ? 1.0f : -1.0f;
		for (int k = 0; k < 3; k++) {
			out[k] = t.com[k] + t.rows[0][k] * t.half[0] * sx + t.rows[1][k] * t.half[1] * sy + t.rows[2][k] * t.half[2] * sz;
		}
	}

	// Is it on its wheels: its up near the world's
	inline bool Upright(const Tumbler& t) {
		return t.rows[1][1] > 0.7f;
	}

	// Its rows square and of unit length again (they drift as they turn)
	inline void Square(Tumbler* t) {
		float* left = t->rows[0];
		float* up = t->rows[1];
		float* ahead = t->rows[2];
		if (!Normalize3(ahead)) {
			ahead[0] = 0.0f;
			ahead[1] = 0.0f;
			ahead[2] = 1.0f;
		}
		Cross3(up, ahead, left);
		if (!Normalize3(left)) {
			// up along the way it faces: any left square to it
			const float side[3] = { 1.0f, 0.0f, 0.0f }, other[3] = { 0.0f, 1.0f, 0.0f };
			Cross3(fabsf(ahead[0]) < 0.9f ? side : other, ahead, left);
			Normalize3(left);
		}
		Cross3(ahead, left, up);
	}

	struct TumbleResult {
		float hit;              // how much the touches changed its speed on this step (a landing, a wall)
		int touching;           // corners in the ground or a wall
		bool cameToRest;
	};

	// dt seconds of it against these planes. The game's frame in steps of no more than a 120th of a second.
	inline TumbleResult Tumble(Tumbler* t, float dt, const Plane* planes, int planeCount, const TumbleSettings& s) {
		TumbleResult res;
		memset(&res, 0, sizeof(res));
		if (t->asleep || !(dt > 0.0f)) {
			return res;
		}
		int steps = (int)ceilf(dt * 120.0f);
		steps = steps < 1 ? 1 : (steps > 8 ? 8 : steps);
		const float h = dt / (float)steps;
		for (int step = 0; step < steps; step++) {
			t->vel[1] -= s.gravity * h;
			const float before[3] = { t->vel[0], t->vel[1], t->vel[2] };
			struct Touch {
				float r[3], n[3], t1[3], t2[3];
				float k, k1, k2, mu1, mu2, target, jn, j1, j2;
			};
			Touch touches[32];
			int count = 0;
			for (int p = 0; p < planeCount && count < 32; p++) {
				const Plane& plane = planes[p];
				for (int i = 0; i < 8 && count < 32; i++) {
					float c[3];
					CornerOf(*t, i, c);
					const float d[3] = { plane.point[0] - c[0], plane.point[1] - c[1], plane.point[2] - c[2] };
					const float depth = Dot3(d, plane.normal);
					if (depth <= 0.0f) {
						continue;
					}
					Touch& x = touches[count++];
					memset(&x, 0, sizeof(x));
					for (int k = 0; k < 3; k++) {
						x.r[k] = c[k] - t->com[k];
						x.n[k] = plane.normal[k];
					}
					float u[3];
					PointVelocity(*t, x.r, u);
					const float vn = Dot3(u, x.n);
					x.target = vn < -s.bounceFrom ? -s.restitution * vn : 0.0f;
					float bias = (depth - s.slop) * s.biasRate;
					bias = bias < 0.0f ? 0.0f : (bias > s.maxBias ? s.maxBias : bias);
					x.target = x.target > bias ? x.target : bias;
					x.k = Stiffness(*t, x.r, x.n);
					// on its wheels on this face: along the way it faces it rolls, across it holds
					const bool wheel = (i & 2) == 0 && Dot3(t->rows[1], x.n) > 0.6f;
					const float* along = wheel ? t->rows[2] : 0;
					float tangent[3];
					if (along) {
						const float a = Dot3(along, x.n);
						for (int k = 0; k < 3; k++) {
							tangent[k] = along[k] - x.n[k] * a;
						}
					}
					else {
						for (int k = 0; k < 3; k++) {
							tangent[k] = u[k] - x.n[k] * vn;
						}
					}
					if (!Normalize3(tangent)) {
						const float side[3] = { 1.0f, 0.0f, 0.0f }, other[3] = { 0.0f, 0.0f, 1.0f };
						Cross3(x.n, fabsf(x.n[0]) < 0.9f ? side : other, tangent);
						Normalize3(tangent);
					}
					memcpy(x.t1, tangent, sizeof(x.t1));
					Cross3(x.n, x.t1, x.t2);
					x.mu1 = wheel ? s.wheelRoll : s.friction;
					x.mu2 = wheel ? s.wheelSide : s.friction;
					x.k1 = Stiffness(*t, x.r, x.t1);
					x.k2 = Stiffness(*t, x.r, x.t2);
				}
			}
			for (int it = 0; it < s.iterations && count > 0; it++) {
				for (int i = 0; i < count; i++) {
					Touch& x = touches[i];
					float u[3];
					PointVelocity(*t, x.r, u);
					if (x.k > 0.0f) {
						float dj = (x.target - Dot3(u, x.n)) / x.k;
						const float j = x.jn + dj > 0.0f ? x.jn + dj : 0.0f;
						dj = j - x.jn;
						x.jn = j;
						const float p[3] = { x.n[0] * dj, x.n[1] * dj, x.n[2] * dj };
						Push(t, x.r, p);
					}
					for (int f = 0; f < 2; f++) {
						const float* dir = f == 0 ? x.t1 : x.t2;
						const float kt = f == 0 ? x.k1 : x.k2;
						const float most = (f == 0 ? x.mu1 : x.mu2) * x.jn;
						float& acc = f == 0 ? x.j1 : x.j2;
						if (!(kt > 0.0f)) {
							continue;
						}
						PointVelocity(*t, x.r, u);
						float j = acc - Dot3(u, dir) / kt;
						j = j < -most ? -most : (j > most ? most : j);
						const float dj = j - acc;
						acc = j;
						const float p[3] = { dir[0] * dj, dir[1] * dj, dir[2] * dj };
						Push(t, x.r, p);
					}
				}
			}
			const float change[3] = { t->vel[0] - before[0], t->vel[1] - before[1], t->vel[2] - before[2] };
			const float hit = Length3(change);
			res.hit = hit > res.hit ? hit : res.hit;
			res.touching = count;
			// on: where it goes, how it turns
			for (int i = 0; i < 3; i++) {
				t->com[i] += t->vel[i] * h;
			}
			for (int k = 0; k < 3; k++) {
				float turn[3];
				Cross3(t->w, t->rows[k], turn);
				for (int i = 0; i < 3; i++) {
					t->rows[k][i] += turn[i] * h;
				}
			}
			Square(t);
			const float keep = 1.0f - s.linearDamping * h, keepSpin = 1.0f - s.angularDamping * h;
			for (int i = 0; i < 3; i++) {
				t->vel[i] *= keep > 0.0f ? keep : 0.0f;
				t->w[i] *= keepSpin > 0.0f ? keepSpin : 0.0f;
			}
			const float speed = Length3(t->vel), spin = Length3(t->w);
			if (speed > s.maxSpeed) {
				for (int i = 0; i < 3; i++) {
					t->vel[i] *= s.maxSpeed / speed;
				}
			}
			if (spin > s.maxSpin) {
				for (int i = 0; i < 3; i++) {
					t->w[i] *= s.maxSpin / spin;
				}
			}
			if (count >= 3 && speed < s.sleepSpeed && spin < s.sleepSpin) {
				t->stillSeconds += h;
			}
			else {
				t->stillSeconds = 0.0f;
			}
			if (t->stillSeconds >= s.sleepAfter) {
				t->asleep = true;
				memset(t->vel, 0, sizeof(t->vel));
				memset(t->w, 0, sizeof(t->w));
				res.cameToRest = true;
				break;
			}
		}
		return res;
	}

	// Woken by a hit
	inline void Wake(Tumbler* t) {
		t->asleep = false;
		t->stillSeconds = 0.0f;
	}

	// Its box along the ground, for the touches with other cars: the rectangle it covers (its way along the
	// ground from the way it faces, or from its left when it stands on its nose), between its lowest and
	// highest corner; its spin about up and how hard it is to turn about up
	inline bool FootprintOf(const Tumbler& t, Body* b) {
		float zx = t.rows[2][0], zz = t.rows[2][2];
		float len = sqrtf(zx * zx + zz * zz);
		if (len < 0.2f) {
			zx = -t.rows[0][2];
			zz = t.rows[0][0];
			len = sqrtf(zx * zx + zz * zz);
			if (len < 0.2f) {
				return false;
			}
		}
		b->axisZ[0] = zx / len;
		b->axisZ[1] = zz / len;
		b->axisX[0] = b->axisZ[1];
		b->axisX[1] = -b->axisZ[0];
		b->centre[0] = t.com[0];
		b->centre[1] = t.com[2];
		b->pivot[0] = t.com[0];
		b->pivot[1] = t.com[2];
		float hx = 0.0f, hz = 0.0f, hy = 0.0f;
		for (int k = 0; k < 3; k++) {
			hx += t.half[k] * fabsf(t.rows[k][0] * b->axisX[0] + t.rows[k][2] * b->axisX[1]);
			hz += t.half[k] * fabsf(t.rows[k][0] * b->axisZ[0] + t.rows[k][2] * b->axisZ[1]);
			hy += t.half[k] * fabsf(t.rows[k][1]);
		}
		b->halfX = hx;
		b->halfZ = hz;
		b->low = t.com[1] - hy;
		b->high = t.com[1] + hy;
		memcpy(b->vel, t.vel, sizeof(b->vel));
		b->spin = t.w[1];
		b->invMass = t.invMass;
		b->invInertia = t.invInertia[0] * t.rows[0][1] * t.rows[0][1] + t.invInertia[1] * t.rows[1][1] * t.rows[1][1] +
			t.invInertia[2] * t.rows[2][1] * t.rows[2][1];
		return true;
	}

	// A touch's impulse on it, at the touch along the ground at the height of its middle (b takes the opposite);
	// one from above or below at its middle (the touches along the ground turn it about up only, as Resolve
	// has them)
	inline void TakeHit(Tumbler* t, const Contact& c, const float impulse[3], float sign) {
		const bool vertical = fabsf(c.normal[1]) > 0.5f;
		const float r[3] = { vertical ? 0.0f : c.point[0] - t->com[0], 0.0f, vertical ? 0.0f : c.point[1] - t->com[2] };
		const float p[3] = { impulse[0] * sign, impulse[1] * sign, impulse[2] * sign };
		Push(t, r, p);
		Wake(t);
	}

	// Thrown up and rolled (a hard hit's launch, an explosion): up units a second upwards, roll radians a second
	// about axis
	inline void Throw(Tumbler* t, float up, float roll, const float axis[3]) {
		t->vel[1] += up;
		for (int i = 0; i < 3; i++) {
			t->w[i] += axis[i] * roll;
		}
		Wake(t);
	}
}
