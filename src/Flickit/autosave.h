#pragma once

// Autosave: where the skater was last whole on the ground, kept on disk, and put back when a level loads.
//
// Every frame (FlickitGame, before the physics) the skater's spot is taken while he is skating on the
// ground: not walking, not in a bail, not in the air or on a rail or a lip, with the controls his (no
// cutscene) and his numbers whole. The spot goes to disk whenever it has moved (Throttle), at most once a
// second (the file is written on the game's own thread), so a crash loses a second at most. One spot per
// level (Slots), in a text file next to the exe.
//
// When a level has loaded and the skater has stood ready for half a second (Session), his spot in that
// level, if there is one, becomes the game's own custom restart (the one the pause menu's "Set Restart"
// keeps) and the game skips him to it, the way the menu's "Goto Cust Restart" does. Until then nothing
// is taken, so the start of a level never overwrites the spot it is about to go back to.
//
// Pure: no game memory and no files here, so that it can be tried in a test.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace Autosave {
	struct Spot {
		uint32_t level;         // the level's checksum (its load script's name: load_NJ ...)
		uint32_t saved;         // when, in seconds since 1970 (0: not known)
		float pos[4];
		float matrix[16];
	};

	// Spots for up to kMost levels; a new level takes the place of the one saved longest ago
	class Slots {
	public:
		static const int kMost = 16;
		Slots() : m_count(0) {}
		int Count() const { return m_count; }
		const Spot* Find(uint32_t level) const {
			for (int i = 0; i < m_count; i++) {
				if (m_spots[i].level == level) {
					return &m_spots[i];
				}
			}
			return 0;
		}
		void Put(const Spot& s) {
			for (int i = 0; i < m_count; i++) {
				if (m_spots[i].level == s.level) {
					m_spots[i] = s;
					return;
				}
			}
			if (m_count < kMost) {
				m_spots[m_count++] = s;
				return;
			}
			int oldest = 0;
			for (int i = 1; i < m_count; i++) {
				if (m_spots[i].saved < m_spots[oldest].saved) {
					oldest = i;
				}
			}
			m_spots[oldest] = s;
		}
		void Clear() { m_count = 0; }

		// One line per level, numbers written so that they read back exactly
		std::string Text() const {
			std::string out = "; Flickit autosave: where the skater was last on the ground in each level. Delete this file to start levels at their own start.\n";
			char line[1024];
			for (int i = 0; i < m_count; i++) {
				const Spot& s = m_spots[i];
				int n = snprintf(line, sizeof(line), "level=%08X saved=%u pos=%.9g,%.9g,%.9g,%.9g matrix=", s.level, s.saved,
					s.pos[0], s.pos[1], s.pos[2], s.pos[3]);
				for (int k = 0; k < 16 && n > 0 && n < (int)sizeof(line); k++) {
					n += snprintf(line + n, sizeof(line) - n, k ? ",%.9g" : "%.9g", s.matrix[k]);
				}
				out += line;
				out += "\n";
			}
			return out;
		}

		// Reads what Text wrote. Lines that do not read whole are left out. False if none did.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)      // sscanf, of numbers alone: there is nothing for it to overrun
#endif
		bool Read(const char* text) {
			Clear();
			const char* p = text;
			while (p && *p) {
				const char* end = strchr(p, '\n');
				std::string line = end ? std::string(p, end) : std::string(p);
				p = end ? end + 1 : 0;
				Spot s;
				memset(&s, 0, sizeof(s));
				unsigned level = 0, saved = 0;
				int used = 0;
				if (sscanf(line.c_str(), "level=%x saved=%u pos=%f,%f,%f,%f matrix=%n", &level, &saved,
						&s.pos[0], &s.pos[1], &s.pos[2], &s.pos[3], &used) != 6 || used <= 0) {
					continue;
				}
				const char* m = line.c_str() + used;
				bool ok = true;
				for (int k = 0; k < 16 && ok; k++) {
					char* stop = 0;
					s.matrix[k] = strtof(m, &stop);
					ok = stop != m && (k == 15 || *stop == ',');
					m = stop + (k == 15 ? 0 : 1);
				}
				if (!ok || !Whole(s)) {
					continue;
				}
				s.level = level;
				s.saved = saved;
				Put(s);
			}
			return m_count > 0;
		}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

		// Numbers, within reach, and a matrix whose first three rows are unit length
		static bool Whole(const Spot& s) {
			for (int i = 0; i < 4; i++) {
				if (!(s.pos[i] > -1.0e6f && s.pos[i] < 1.0e6f)) {
					return false;
				}
			}
			for (int i = 0; i < 16; i++) {
				if (!(s.matrix[i] > -1.0e6f && s.matrix[i] < 1.0e6f)) {
					return false;
				}
			}
			for (int r = 0; r < 3; r++) {
				const float* row = s.matrix + 4 * r;
				const float len2 = row[0] * row[0] + row[1] * row[1] + row[2] * row[2];
				if (!(len2 > 0.9f && len2 < 1.1f)) {
					return false;
				}
			}
			return true;
		}
	private:
		Spot m_spots[kMost];
		int m_count;
	};

	// When to write: the spot has moved a unit or turned a degree since the last one written, and a second
	// has gone by since then
	class Throttle {
	public:
		static const uint32_t kGapMs = 1000;
		Throttle() : m_have(false), m_at(0) {}
		bool Due(const Spot& s, uint32_t nowMs) const {
			if (!m_have) {
				return true;
			}
			if (nowMs - m_at < kGapMs) {
				return false;
			}
			if (s.level != m_last.level) {
				return true;
			}
			const float dx = s.pos[0] - m_last.pos[0], dy = s.pos[1] - m_last.pos[1], dz = s.pos[2] - m_last.pos[2];
			if (dx * dx + dy * dy + dz * dz > 1.0f) {
				return true;
			}
			// the way he faces: the third row
			const float dot = s.matrix[8] * m_last.matrix[8] + s.matrix[9] * m_last.matrix[9] + s.matrix[10] * m_last.matrix[10];
			return dot < 0.99985f;     // about a degree
		}
		void Wrote(const Spot& s, uint32_t nowMs) {
			m_have = true;
			m_last = s;
			m_at = nowMs;
		}
		void Forget() { m_have = false; }
	private:
		bool m_have;
		Spot m_last;
		uint32_t m_at;
	};

	// One level's stay, from its load: wait until the skater has been ready for kReadyFrames frames in a
	// row, then once: put him back (if there is a spot) and start taking spots. A new stay starts with a
	// new level, a new load (loads: a count of the level loads the game has made, which goes up on a load
	// of the same level again too) or a new skater object.
	enum Step {
		WAIT,           // not yet: take nothing
		RESTORE,        // now: put him back, then take spots
		TAKE            // take spots
	};

	class Session {
	public:
		static const int kReadyFrames = 30;
		Session() : m_level(0), m_loads(0), m_object(0), m_ready(0), m_done(false) {}
		Step Frame(uint32_t level, uint32_t loads, const void* skater, bool ready) {
			if (level != m_level || loads != m_loads || skater != m_object) {
				m_level = level;
				m_loads = loads;
				m_object = skater;
				m_ready = 0;
				m_done = false;
			}
			if (m_done) {
				return TAKE;
			}
			m_ready = ready ? m_ready + 1 : 0;
			if (m_ready < kReadyFrames) {
				return WAIT;
			}
			m_done = true;
			return RESTORE;
		}
		bool Done() const { return m_done; }
	private:
		uint32_t m_level;
		uint32_t m_loads;
		const void* m_object;
		int m_ready;
		bool m_done;
	};
}
