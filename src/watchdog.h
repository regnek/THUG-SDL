#pragma once

// The watchdog: the frame the skater's numbers (or the skater camera's) stop being numbers.
//
// A position, speed or matrix that has become NaN or infinite stays that way: every sum it goes into
// comes out the same. On the skater, the rail code goes on scoring the grind (none of its tests are
// true for a NaN, so none of them end it), while the camera that follows him sees nothing: the screen is
// the colour the game clears it to (0x506070, set at 0x004B75E3), with the score and the trick on top.
//
// FlickitGame keeps the last frames in a History, looks at every frame's numbers (Sane), and when they
// go, writes the frames before it to the log and puts the skater back where he last was whole: that
// frame's position, matrices and speed (Restore). If that does not hold, the game's own reset of his
// physics there (Reset), twice; then it gives up until he has been whole for two seconds (Policy).
// The camera is only looked at once it has shown, for a few seconds, that what is taken for it is it:
// near the skater, with a matrix of unit rows (CameraCheck).
//
// Pure: no game memory here, so that it can be tried in a test.

#include <cstdint>

namespace Watchdog {
	// Further out than any level reaches (the joined world is under 60000 across): a number past
	// this is as lost as one that is not a number
	static const float kFarthest = 1.0e6f;

	// Every one of the n numbers is a number, and within kFarthest of nothing
	inline bool Sane(const float* v, int n, float limit = kFarthest) {
		for (int i = 0; i < n; i++) {
			if (!(v[i] > -limit && v[i] < limit)) {        // false for a NaN as well
				return false;
			}
		}
		return true;
	}

	// One frame of the skater, as it was
	struct Sample {
		uint32_t frame;         // the watchdog's own count
		uint32_t tick;          // GetTickCount
		int state;              // CSkaterStateComponent: 0 ground, 1 air, 2 wall, 3 lip, 4 rail, 5 wallplant
		uint32_t script;        // the script he runs, and the one that called it
		uint32_t caller;
		float pos[3];
		float vel[3];
		float matrix[16];       // the object's
		float display[16];      // the one he is drawn with
		bool onRail;
		bool hasNext;
		bool hasPrevious;
		float rail[3];          // the rail node his piece of rail starts at, and its neighbours
		float next[3];
		float previous[3];
		bool camKnown;
		float cam[3];
		float camMatrix[16];
	};

	// The last N samples, newest last
	template <int N>
	class History {
	public:
		History() : m_count(0), m_head(0) {}
		void Push(const Sample& s) {
			m_items[m_head] = s;
			m_head = (m_head + 1) % N;
			if (m_count < N) {
				m_count++;
			}
		}
		int Count() const { return m_count; }
		// back 0: the newest
		const Sample* Back(int back) const {
			if (back < 0 || back >= m_count) {
				return 0;
			}
			return &m_items[(m_head - 1 - back + N) % N];
		}
		void Clear() {
			m_count = 0;
			m_head = 0;
		}
	private:
		Sample m_items[N];
		int m_count;
		int m_head;
	};

	// What to do about the skater on a frame
	enum Action {
		KEEP,           // his numbers are fine: keep the frame
		RESTORE,        // put back the last whole frame
		RESET,          // that, and the game's reset of his physics
		GIVE_UP,        // neither held: say so once and leave him be
		NOTHING         // given up already
	};

	class Policy {
	public:
		// Two seconds whole before the count of tries starts again
		static const int kForgive = 120;
		Policy() : m_tries(0), m_good(0) {}
		Action Frame(bool sane) {
			if (sane) {
				if (++m_good >= kForgive) {
					m_tries = 0;
				}
				return KEEP;
			}
			m_good = 0;
			m_tries++;
			if (m_tries == 1) {
				return RESTORE;
			}
			if (m_tries <= 3) {
				return RESET;
			}
			return m_tries == 4 ? GIVE_UP : NOTHING;
		}
		int Tries() const { return m_tries; }
	private:
		int m_tries;
		int m_good;
	};

	// Whether the object taken for the skater camera is it: over its first kFrames whole frames it keeps
	// within kNear of the skater, and its matrix rows are unit length
	class CameraCheck {
	public:
		static const int kFrames = 180;
		static constexpr float kNear = 3000.0f;
		CameraCheck() : m_seen(0), m_fit(0) {}
		// Only frames where the skater and the camera are both whole
		void Feed(const float skater[3], const float cam[3], const float matrix[16]) {
			if (m_seen >= kFrames) {
				return;
			}
			m_seen++;
			const float dx = cam[0] - skater[0], dy = cam[1] - skater[1], dz = cam[2] - skater[2];
			bool fit = dx * dx + dy * dy + dz * dz < kNear * kNear;
			for (int r = 0; r < 3 && fit; r++) {
				const float* row = matrix + 4 * r;
				const float len2 = row[0] * row[0] + row[1] * row[1] + row[2] * row[2];
				fit = len2 > 0.9f && len2 < 1.1f;
			}
			if (fit) {
				m_fit++;
			}
		}
		bool Decided() const { return m_seen >= kFrames; }
		// Nine in ten frames fit
		bool Trusted() const { return Decided() && m_fit * 10 >= m_seen * 9; }
		void Reset() {
			m_seen = 0;
			m_fit = 0;
		}
	private:
		int m_seen;
		int m_fit;
	};
}
