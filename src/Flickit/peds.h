#pragma once
// Flickit's people run over by a car, the parts without the game (tested in tests/peds_test.cpp): whether a car's
// box meets someone, how they are thrown, their flight, the landing and the slide to a stop, and when someone who
// lies there can be back on their way. src/Flickit/drive_game.cpp does the rest with the game.
//
// The game's units: inches and seconds, up is +y. Someone is a post kRadius about where they stand, kHeight tall.
#include <math.h>
#include <string.h>

#include "crash.h"

namespace Peds {
	static const float kRadius = 12.0f;
	static const float kHeight = 72.0f;
	static const float kLongest = 6.0f;      // seconds: a flight with no ground under it ends where it is

	struct Settings {
		float from;          // the car's speed at them that knocks them over (slower: it goes through them)
		float upBase;        // they go up at this speed at the least
		float upPerSpeed;    // and at this much more for each inch a second of the car's
		float upMost;
		float along;         // the share of the car's speed they go on with
		float side;          // and the speed away from the car's middle, sideways
		float drag;          // in the air their speed along the ground falls off by this share a second
		float slide;         // on the ground by this (faster)
		float stopBelow;     // and stops below this speed
	};

	inline Settings DefaultSettings() {
		Settings s;
		s.from = 60.0f;
		s.upBase = 100.0f;
		s.upPerSpeed = 0.25f;
		s.upMost = 450.0f;
		s.along = 0.7f;
		s.side = 150.0f;
		s.drag = 0.8f;
		s.slide = 4.0f;
		s.stopBelow = 25.0f;
		return s;
	}

	// The car at someone standing at feet: its speed there (its spin's too), along the ground, and which side of
	// its middle they are on (+1 along its sideways, -1 the other way)
	struct Strike {
		float vel[3];
		float speed;
		float side;
	};

	// Whether the car's box meets someone standing at feet
	inline bool Struck(const Crash::Body& car, const float feet[3], Strike* out) {
		if (feet[1] + kHeight < car.low || feet[1] > car.high) {
			return false;
		}
		const float dx = feet[0] - car.centre[0], dz = feet[2] - car.centre[1];
		const float lx = dx * car.axisX[0] + dz * car.axisX[1];
		const float lz = dx * car.axisZ[0] + dz * car.axisZ[1];
		if (fabsf(lx) > car.halfX + kRadius || fabsf(lz) > car.halfZ + kRadius) {
			return false;
		}
		const float rx = feet[0] - car.pivot[0], rz = feet[2] - car.pivot[1];
		out->vel[0] = car.vel[0] + car.spin * rz;
		out->vel[1] = car.vel[1];
		out->vel[2] = car.vel[2] - car.spin * rx;
		out->speed = sqrtf(out->vel[0] * out->vel[0] + out->vel[2] * out->vel[2]);
		out->side = lx >= 0.0f ? 1.0f : -1.0f;
		return true;
	}

	// Someone in the air after a hit, or sliding along the ground after the landing
	struct Flight {
		float pos[3];        // their feet
		float vel[3];
		float facing[2];     // the way they face along the ground: where the car came from, so they fall away from it
		float seconds;       // since the hit
		bool landed;
		bool still;
	};

	// Thrown by the car: on with a share of its speed, away from its middle, and up, the more the faster it went
	inline void Throw(const Crash::Body& car, const float feet[3], const Strike& st, const Settings& s, Flight* f) {
		memset(f, 0, sizeof(*f));
		memcpy(f->pos, feet, sizeof(f->pos));
		f->vel[0] = st.vel[0] * s.along + car.axisX[0] * st.side * s.side;
		f->vel[2] = st.vel[2] * s.along + car.axisX[1] * st.side * s.side;
		const float up = s.upBase + s.upPerSpeed * st.speed;
		f->vel[1] = up > s.upMost ? s.upMost : up;
		float fx = -st.vel[0], fz = -st.vel[2];
		const float length = sqrtf(fx * fx + fz * fz);
		if (length > 1.0f) {
			fx /= length;
			fz /= length;
		}
		else {
			fx = -car.axisZ[0];
			fz = -car.axisZ[1];
		}
		f->facing[0] = fx;
		f->facing[1] = fz;
	}

	// The level's say in a step: the ground under them (its height, or none), and a wall between where they were
	// and where they would be
	struct Level {
		bool ground;
		float groundY;
		bool wall;
	};

	// dt seconds of the flight or the slide. True on the step they land.
	inline bool Step(Flight* f, float dt, const Level& level, const Settings& s) {
		if (f->still || !(dt > 0.0f)) {
			return false;
		}
		f->seconds += dt;
		if (level.wall) {
			// up against something: no further that way
			f->vel[0] = 0.0f;
			f->vel[2] = 0.0f;
		}
		if (!f->landed) {
			f->vel[1] -= Crash::kGravity * dt;
			const float k = expf(-s.drag * dt);
			f->vel[0] *= k;
			f->vel[2] *= k;
			for (int i = 0; i < 3; i++) {
				f->pos[i] += f->vel[i] * dt;
			}
			if (level.ground && f->vel[1] <= 0.0f && f->pos[1] <= level.groundY) {
				f->pos[1] = level.groundY;
				f->vel[1] = 0.0f;
				f->landed = true;
				return true;
			}
			if (f->seconds > kLongest) {
				// nothing under them (out over the edge of the level): they stop where they are
				memset(f->vel, 0, sizeof(f->vel));
				f->still = true;
			}
			return false;
		}
		const float k = expf(-s.slide * dt);
		f->vel[0] *= k;
		f->vel[2] *= k;
		f->pos[0] += f->vel[0] * dt;
		f->pos[2] += f->vel[2] * dt;
		if (level.ground) {
			f->pos[1] = level.groundY;
		}
		if (sqrtf(f->vel[0] * f->vel[0] + f->vel[2] * f->vel[2]) < s.stopBelow) {
			memset(f->vel, 0, sizeof(f->vel));
			f->still = true;
		}
		return false;
	}

	inline bool FarAlongTheGround(const float a[3], const float b[3], float away) {
		const float dx = a[0] - b[0], dz = a[2] - b[2];
		return dx * dx + dz * dz > away * away;
	}

	// Someone who was run over is back on their way (made again at the start of it) once they have lain there
	// long enough and he is far from both where they lie and where they would be again (start: none, no check)
	inline bool BackOnTheirWay(float lainSeconds, float leastSeconds, const float lie[3], const float* start, const float him[3], float away) {
		return lainSeconds >= leastSeconds && FarAlongTheGround(lie, him, away) && (!start || FarAlongTheGround(start, him, away));
	}
}
