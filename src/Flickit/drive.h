#pragma once
// Flickit's cars: the parts that need neither Windows nor the game, tested by tests/drive_test.cpp.
//
// Any car of the level can be taken, a moving one too: walk or skate up to it and press Circle. The car is
// the game's own drivable one with the model of the car taken, put where that car was and given its speed.
// Getting out (Triangle held, the game's own) leaves the car standing there, to be taken again.
//
// An object's place as the game keeps it: the position, and a matrix whose rows are the way its left side
// points, the way up and the way it faces (four floats each). 0 radians faces +z, pi/2 faces +x.
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "str.h"

namespace Drive {
	// The box of a car's body, along its own left (x), up (y) and the way it faces (z)
	struct Box {
		float halfWidth;
		float rear;         // z of the back (below 0)
		float front;        // z of the front
		float height;
	};

	// A car of the usual size, for a model with no box of its own
	inline Box UsualBox() {
		Box b;
		b.halfWidth = 38.0f;
		b.rear = -65.0f;
		b.front = 150.0f;
		b.height = 62.0f;
		return b;
	}

	// The camera of the game's drivable car for a car of this box: the game's own (55 up, 240 behind) for a car
	// of the usual size, higher and farther back for a taller one (a box truck: the road ahead shows over its
	// roof) and a little farther back for a longer one
	struct CameraPlace {
		float height;
		float distance;
	};
	inline CameraPlace CameraFor(const Box& b) {
		const Box usual = UsualBox();
		const float taller = b.height > usual.height ? b.height - usual.height : 0.0f;
		const float longer = (b.front - b.rear) > (usual.front - usual.rear) ? (b.front - b.rear) - (usual.front - usual.rear) : 0.0f;
		CameraPlace c;
		c.height = 55.0f + 1.3f * taller;
		c.distance = 240.0f + 1.5f * taller + 0.5f * longer;
		return c;
	}

	// The box from the base of the game's colliders for a car (the first of them: three corners of the body's
	// base, two at the front and one at the back on the left), and the height of the highest corner of any
	inline bool BoxFromColliders(const float front[3], const float frontOther[3], const float back[3], float top, Box* out) {
		const float halfWidth = fabsf(front[0] - frontOther[0]) * 0.5f;
		const float f = front[2] > back[2] ? front[2] : back[2];
		const float r = front[2] > back[2] ? back[2] : front[2];
		if (!(halfWidth > 5.0f && halfWidth < 200.0f) || !(f - r > 20.0f && f - r < 800.0f) || !(top > 0.0f && top < 400.0f)) {
			return false;
		}
		out->halfWidth = halfWidth;
		out->front = f;
		out->rear = r;
		out->height = top;
		return true;
	}

	// Where p is from an object at pos with that matrix: along its left, up and the way it faces
	inline void Local(const float pos[3], const float m[12], const float p[3], float out[3]) {
		const float d[3] = { p[0] - pos[0], p[1] - pos[1], p[2] - pos[2] };
		out[0] = d[0] * m[0] + d[1] * m[1] + d[2] * m[2];
		out[1] = d[0] * m[4] + d[1] * m[5] + d[2] * m[6];
		out[2] = d[0] * m[8] + d[1] * m[9] + d[2] * m[10];
	}

	// Is a place (from the car, as Local gives it) within reach of the car's body? And how far from it,
	// for picking the nearest car (0 against the body).
	inline bool InReach(const Box& b, float reach, const float local[3], float* away) {
		const float below = 60.0f;   // he may stand lower than the car's wheels (a kerb, a slope)
		if (local[1] < -below || local[1] > b.height + reach) {
			return false;
		}
		float dx = fabsf(local[0]) - b.halfWidth;
		float dz = local[2] > b.front ? local[2] - b.front : (local[2] < b.rear ? b.rear - local[2] : 0.0f);
		dx = dx > 0.0f ? dx : 0.0f;
		const float d = sqrtf(dx * dx + dz * dz);
		if (d > reach) {
			return false;
		}
		if (away) {
			*away = d;
		}
		return true;
	}

	// The way a matrix faces, in radians, from its third row
	inline float Heading(const float m[12]) {
		return atan2f(m[8], m[10]);
	}

	// The speed of a car from where it was on the frames before
	struct Track {
		float last[3];
		float vel[3];
		bool have;
		uint32_t frames;       // frames in a row it has been seen

		void Reset() {
			memset(this, 0, sizeof(*this));
		}
		// Where it is now, seconds after the last time. A jump (a car put back at the start of its way)
		// starts again.
		void Update(const float p[3], float seconds) {
			if (have && seconds > 0.0005f && seconds < 0.5f) {
				float v[3];
				for (int i = 0; i < 3; i++) {
					v[i] = (p[i] - last[i]) / seconds;
				}
				const float speed2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
				if (speed2 > 6000.0f * 6000.0f) {
					frames = 0;
					memset(vel, 0, sizeof(vel));
				}
				else {
					const float keep = frames > 0 ? 0.5f : 0.0f;
					for (int i = 0; i < 3; i++) {
						vel[i] = vel[i] * keep + v[i] * (1.0f - keep);
					}
					frames++;
				}
			}
			else {
				frames = 0;
				memset(vel, 0, sizeof(vel));
			}
			memcpy(last, p, sizeof(last));
			have = true;
		}
		// Its speed the way it faces (a car of the traffic does not go backwards)
		float Forward(const float m[12]) const {
			const float s = vel[0] * m[8] + vel[1] * m[9] + vel[2] * m[10];
			return s > 0.0f ? s : 0.0f;
		}
	};

	// The places beside a car to get out at, in the order they are tried: the driver's side (the car's left),
	// the other side, behind it, in front of it. At the height of the car, gap from the body. Its matrix
	// level, the way it faces: heading.
	inline void Doors(const float pos[3], float heading, const Box& b, float gap, float out[4][3]) {
		const float fx = sinf(heading), fz = cosf(heading);    // the way it faces
		const float lx = fz, lz = -fx;                          // its left
		const float along = b.rear + (b.front - b.rear) * 0.45f;   // a little behind the middle: the front seats
		const float side = b.halfWidth + gap;
		const float local[4][2] = { { side, along }, { -side, along }, { 0.0f, b.rear - gap }, { 0.0f, b.front + gap } };
		for (int i = 0; i < 4; i++) {
			out[i][0] = pos[0] + lx * local[i][0] + fx * local[i][1];
			out[i][1] = pos[1];
			out[i][2] = pos[2] + lz * local[i][0] + fz * local[i][1];
		}
	}

	// Where a car is left standing: its model's origin on the ground, the ground's slope under it. The game's car
	// drives with its body up on its springs, so the place it is at is above the place its model stands at (a
	// car of the traffic stands with its model's origin on the ground, at its node); a parked car put at the
	// driven car's place floats.
	//
	// probes: n places under the car, each its x along the car's left and z along the way it faces (from the
	// car's place, level), and the height of the ground found there (found[i] false: none). The plane through
	// them by least squares (three or more), else level at their average; a plane steeper than steepest (rise
	// over run) or more than within off any probe is not trusted and the car stands level at the probe nearest
	// its middle. pos: the car's place (x and z kept); m: its matrix's rows (left, up, the way it faces), at
	// heading. False when no probe found ground (then nothing changes).
	inline bool GroundUnder(float pos[3], float m[12], float heading, const float probes[][3], const bool found[], int n,
			float steepest = 0.5f, float within = 6.0f) {
		int have = 0, nearest = -1;
		float nearestAway = 1.0e9f, mean = 0.0f;
		for (int i = 0; i < n; i++) {
			if (!found[i]) {
				continue;
			}
			have++;
			mean += probes[i][1];
			const float away = probes[i][0] * probes[i][0] + probes[i][2] * probes[i][2];
			if (away < nearestAway) {
				nearestAway = away;
				nearest = i;
			}
		}
		if (have == 0) {
			return false;
		}
		mean /= (float)have;
		float y = mean, slopeLeft = 0.0f, slopeAhead = 0.0f;
		if (have >= 3) {
			// y = a + b x + c z: the normal equations, solved by Cramer's rule
			double s[3][3] = { { 0 } }, t[3] = { 0 };
			for (int i = 0; i < n; i++) {
				if (!found[i]) {
					continue;
				}
				const double row[3] = { 1.0, probes[i][0], probes[i][2] };
				for (int r = 0; r < 3; r++) {
					for (int c = 0; c < 3; c++) {
						s[r][c] += row[r] * row[c];
					}
					t[r] += row[r] * probes[i][1];
				}
			}
			const double det = s[0][0] * (s[1][1] * s[2][2] - s[1][2] * s[2][1]) - s[0][1] * (s[1][0] * s[2][2] - s[1][2] * s[2][0]) +
				s[0][2] * (s[1][0] * s[2][1] - s[1][1] * s[2][0]);
			if (fabs(det) > 1.0e-6) {
				double sol[3];
				for (int k = 0; k < 3; k++) {
					double mm[3][3];
					for (int r = 0; r < 3; r++) {
						for (int c = 0; c < 3; c++) {
							mm[r][c] = c == k ? t[r] : s[r][c];
						}
					}
					sol[k] = (mm[0][0] * (mm[1][1] * mm[2][2] - mm[1][2] * mm[2][1]) - mm[0][1] * (mm[1][0] * mm[2][2] - mm[1][2] * mm[2][0]) +
						mm[0][2] * (mm[1][0] * mm[2][1] - mm[1][1] * mm[2][0])) / det;
				}
				bool trusted = fabs(sol[1]) <= steepest && fabs(sol[2]) <= steepest;
				for (int i = 0; i < n && trusted; i++) {
					if (found[i] && fabs(sol[0] + sol[1] * probes[i][0] + sol[2] * probes[i][2] - probes[i][1]) > within) {
						trusted = false;
					}
				}
				if (trusted) {
					y = (float)sol[0];
					slopeLeft = (float)sol[1];
					slopeAhead = (float)sol[2];
				}
				else {
					y = probes[nearest][1];
				}
			}
		}
		// the rows: the way it faces up the slope ahead, its left up the slope to the left, up square to both
		const float fx = sinf(heading), fz = cosf(heading);
		const float lx = fz, lz = -fx;
		float ahead[3] = { fx, slopeAhead, fz };
		float left[3] = { lx, slopeLeft, lz };
		float up[3] = { ahead[1] * left[2] - ahead[2] * left[1], ahead[2] * left[0] - ahead[0] * left[2], ahead[0] * left[1] - ahead[1] * left[0] };
		const float upLen = sqrtf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
		const float aheadLen = sqrtf(ahead[0] * ahead[0] + ahead[1] * ahead[1] + ahead[2] * ahead[2]);
		for (int k = 0; k < 3; k++) {
			up[k] /= upLen;
			ahead[k] /= aheadLen;
		}
		left[0] = up[1] * ahead[2] - up[2] * ahead[1];
		left[1] = up[2] * ahead[0] - up[0] * ahead[2];
		left[2] = up[0] * ahead[1] - up[1] * ahead[0];
		const float rows[12] = { left[0], left[1], left[2], 0.0f, up[0], up[1], up[2], 0.0f, ahead[0], ahead[1], ahead[2], 0.0f };
		memcpy(m, rows, sizeof(rows));
		pos[1] = y;
		return true;
	}

	// The name of the structure in cars.q for a model: Flickit_Car_ and the model's file name without its
	// folders and its ending, lower case (veh\veh_suv_drg\veh_suv_drg.mdl: Flickit_Car_veh_suv_drg).
	inline bool TuneName(const char* model, char* out, size_t size) {
		if (!model || !out || size < 16) {
			return false;
		}
		const char* base = model;
		for (const char* p = model; *p; p++) {
			if (*p == '\\' || *p == '/') {
				base = p + 1;
			}
		}
		size_t n = 0;
		while (base[n] && base[n] != '.') {
			n++;
		}
		const char prefix[] = "Flickit_Car_";
		const size_t pre = sizeof(prefix) - 1;
		if (n == 0 || pre + n + 1 > size) {
			return false;
		}
		memcpy(out, prefix, pre);
		for (size_t i = 0; i < n; i++) {
			const char c = base[i];
			out[pre + i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
		}
		out[pre + n] = 0;
		return true;
	}

	// Cars of the level that went (a car he left that had to make room for another, a car he drove that went
	// with a restart), to be put back at the start of their way: not before a while has gone by (if there is
	// a wait), and only when he is far from there, so that no car appears before his eyes.
	class Comeback {
	public:
		enum { kMax = 32 };
		Comeback() : m_count(0) {}

		void Clear() {
			m_count = 0;
		}
		int Count() const {
			return m_count;
		}
		// A car taken, starting at start. False when the list is full (the car then stays away).
		bool Add(uint32_t name, const float start[3], uint32_t now) {
			for (int i = 0; i < m_count; i++) {
				if (m_items[i].name == name) {
					m_items[i].taken = now;
					return true;
				}
			}
			if (m_count >= kMax) {
				return false;
			}
			Item& it = m_items[m_count++];
			it.name = name;
			memcpy(it.start, start, sizeof(it.start));
			it.taken = now;
			return true;
		}
		// The car whose turn it is to come back now (taken long enough ago, its start far from where he is),
		// which leaves the list; 0 for none.
		uint32_t Due(uint32_t now, const float at[3], uint32_t waitMs, float farAway) {
			for (int i = 0; i < m_count; i++) {
				const Item& it = m_items[i];
				if (now - it.taken < waitMs) {
					continue;
				}
				const float dx = it.start[0] - at[0], dy = it.start[1] - at[1], dz = it.start[2] - at[2];
				if (dx * dx + dy * dy + dz * dz < farAway * farAway) {
					continue;
				}
				const uint32_t name = it.name;
				RemoveAt(i);
				return name;
			}
			return 0;
		}
		// A car that is there again (made by the level itself) leaves the list
		void Remove(uint32_t name) {
			for (int i = 0; i < m_count; i++) {
				if (m_items[i].name == name) {
					RemoveAt(i);
					return;
				}
			}
		}
		uint32_t NameAt(int i) const {
			return i >= 0 && i < m_count ? m_items[i].name : 0;
		}

	private:
		struct Item {
			uint32_t name;
			float start[3];
			uint32_t taken;
		};
		void RemoveAt(int i) {
			for (int k = i; k + 1 < m_count; k++) {
				m_items[k] = m_items[k + 1];
			}
			m_count--;
		}
		Item m_items[kMax];
		int m_count;
	};

	// Two places at least farAway apart
	inline bool FarApart(const float a[3], const float b[3], float farAway) {
		const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
		return dx * dx + dy * dy + dz * dz >= farAway * farAway;
	}

	// Whether a car he left goes back now: it has stood standMs since he got out (0: it stays), he is farAway
	// from it, and from start too when a car is to appear there (the car it was, at the start of its way; 0
	// for none), so that no car goes or comes before his eyes. The level then has the cars it had.
	inline bool GoesBack(uint32_t now, uint32_t parkedAt, uint32_t standMs, const float parked[3], const float at[3],
		const float* start, float farAway) {
		if (standMs == 0 || now - parkedAt < standMs) {
			return false;
		}
		return FarApart(parked, at, farAway) && (!start || FarApart(start, at, farAway));
	}

	// The cars standing on their own, under names of their own (Flickit_ParkedCar0 ...): the cars he got out
	// of, and the cars knocked off their way in a crash. A free name if there is one, otherwise the one left
	// longest ago (that car goes), one far from him first. Each keeps the car of the level it was (its origin:
	// taken again and left again, still that one) and when it was left.
	class Parking {
	public:
		enum { kSlots = 10, kModel = 128 };
		Parking() {
			Clear();
		}
		void Clear() {
			memset(m_slots, 0, sizeof(m_slots));
			m_stamp = 0;
		}
		// A slot for another car without taking one from under his eyes: a free one (inUse: whether a slot's
		// car is still there), else the one left longest ago of those away from him (away, if given); -1 for
		// none
		int Room(const bool inUse[kSlots], const bool* away) const {
			int oldest = -1;
			for (int i = 0; i < kSlots; i++) {
				if (!inUse[i]) {
					return i;
				}
				if (away && away[i] && (oldest < 0 || m_slots[i].stamp < m_slots[oldest].stamp)) {
					oldest = i;
				}
			}
			return oldest;
		}
		// The slot for the next car he leaves: room if there is any, else the one left longest ago
		int Pick(const bool inUse[kSlots], const bool* away = 0) const {
			const int room = Room(inUse, away);
			if (room >= 0) {
				return room;
			}
			int oldest = 0;
			for (int i = 1; i < kSlots; i++) {
				if (m_slots[i].stamp < m_slots[oldest].stamp) {
					oldest = i;
				}
			}
			return oldest;
		}
		void Park(int slot, const char* model, const Box& box, uint32_t origin = 0, uint32_t now = 0) {
			if (slot < 0 || slot >= kSlots) {
				return;
			}
			Slot& s = m_slots[slot];
			s.used = true;
			s.stamp = ++m_stamp;
			s.box = box;
			s.origin = origin;
			s.parkedAt = now;
			Str::Copy(s.model, kModel, model);
		}
		void Leave(int slot) {
			if (slot >= 0 && slot < kSlots) {
				m_slots[slot].used = false;
			}
		}
		bool Used(int slot) const {
			return slot >= 0 && slot < kSlots && m_slots[slot].used;
		}
		const char* Model(int slot) const {
			return Used(slot) ? m_slots[slot].model : 0;
		}
		const Box* BoxOf(int slot) const {
			return Used(slot) ? &m_slots[slot].box : 0;
		}
		uint32_t Origin(int slot) const {
			return Used(slot) ? m_slots[slot].origin : 0;
		}
		uint32_t ParkedAt(int slot) const {
			return Used(slot) ? m_slots[slot].parkedAt : 0;
		}

	private:
		struct Slot {
			bool used;
			uint32_t stamp;
			Box box;
			uint32_t origin;
			uint32_t parkedAt;
			char model[kModel];
		};
		Slot m_slots[kSlots];
		uint32_t m_stamp;
	};
}
