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

	// The three orientation rows of a matrix are about unit length and about square to each other. A matrix
	// can be all numbers and still have collapsed (two rows the same, a row of nothing); a view made from it
	// is not a number. For the reports only: nothing is put back for it alone
	inline bool Square(const float* m, float slack = 0.1f) {
		for (int r = 0; r < 3; r++) {
			const float* a = m + 4 * r;
			const float len2 = a[0] * a[0] + a[1] * a[1] + a[2] * a[2];
			if (!(len2 > (1.0f - slack) * (1.0f - slack) && len2 < (1.0f + slack) * (1.0f + slack))) {
				return false;
			}
			for (int k = r + 1; k < 3; k++) {
				const float* b = m + 4 * k;
				const float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
				if (!(d > -slack && d < slack)) {
					return false;
				}
			}
		}
		return true;
	}

	// The view and projection the renderer last drew the level with: all numbers (a view of the joined
	// world has positions in it up to about 60000; a projection only small numbers)
	inline bool ViewSane(const float view[16], const float proj[16]) {
		return Sane(view, 16, 1.0e7f) && Sane(proj, 16, 1.0e7f);
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
		bool targetKnown;       // what the camera follows of him (the restart moves it with him)
		float target[3];
		float targetUp[3];
		bool viewKnown;         // the renderer's last view of the level: the eye, its two directions,
		float eye[3];           // and the matrices made of them
		float at[3];
		float up[3];
		float view[16];
		float proj[16];
		uint32_t draws;         // meshes drawn so far (counted by the camera-relative drawing), 0 if not counted
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

	// What to do about the view on a frame. The view goes on being made from the camera every frame, so
	// putting the camera back is tried first; if the view does not come back, the game's own restart where
	// the skater stands (it moves what the camera follows along with him), twice, a second apart.
	enum ViewAction {
		VIEW_KEEP,              // whole
		VIEW_CAMERA,            // the first bad frame: put the camera back
		VIEW_RESTART,           // restart him where he is
		VIEW_GIVE_UP,           // neither brought it back: say so once
		VIEW_WAIT               // bad, nothing to do on this frame
	};

	class ViewPolicy {
	public:
		static const int kFirst = 10;       // bad frames before the first restart
		static const int kBetween = 60;     // and from one restart to the next
		static const int kRestarts = 2;
		static const int kForgive = 120;    // whole frames before the restarts count from nothing again
		ViewPolicy() : m_bad(0), m_good(0), m_restarts(0), m_next(kFirst), m_gaveUp(false) {}
		// restartable: the skater is whole and his controls are his, so a restart has something to start from
		ViewAction Frame(bool viewOk, bool restartable) {
			if (viewOk) {
				m_bad = 0;
				m_next = kFirst;
				m_gaveUp = false;
				if (++m_good >= kForgive) {
					m_restarts = 0;
				}
				return VIEW_KEEP;
			}
			m_good = 0;
			m_bad++;
			if (m_bad == 1) {
				return VIEW_CAMERA;
			}
			if (m_bad >= m_next) {
				if (m_restarts < kRestarts) {
					if (!restartable) {
						return VIEW_WAIT;
					}
					m_restarts++;
					m_next = m_bad + kBetween;
					return VIEW_RESTART;
				}
				if (!m_gaveUp) {
					m_gaveUp = true;
					return VIEW_GIVE_UP;
				}
			}
			return VIEW_WAIT;
		}
		int Bad() const { return m_bad; }
		int Restarts() const { return m_restarts; }
	private:
		int m_bad;
		int m_good;
		int m_restarts;
		int m_next;
		bool m_gaveUp;
	};

	// Frames in a row in which the level was not drawn at all: the count of mesh draws did not move
	class Blackout {
	public:
		static const int kFrames = 30;      // half a second
		Blackout() : m_last(0), m_known(false), m_frames(0) {}
		// 1: the level has just gone kFrames frames undrawn; -n: it is drawn again after n frames (n >= kFrames)
		int Frame(uint32_t draws) {
			if (!m_known) {
				m_known = true;
				m_last = draws;
				return 0;
			}
			const bool drawn = draws != m_last;
			m_last = draws;
			if (!drawn) {
				return ++m_frames == kFrames ? 1 : 0;
			}
			const int was = m_frames;
			m_frames = 0;
			return was >= kFrames ? -was : 0;
		}
		int Frames() const { return m_frames; }
		void Reset() {
			m_known = false;
			m_frames = 0;
		}
	private:
		uint32_t m_last;
		bool m_known;
		int m_frames;
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
