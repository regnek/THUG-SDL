// Where the time of a frame goes: what the watcher of the game's frames counts up, with nothing of the game
// or of Windows in it (frames_game.cpp does the watching).
//
// Two things are counted. The frames themselves: how long each took, how many ran long, and how the time of
// one splits between what comes before the level is drawn, the drawing, and what comes after it. And samples:
// many times a second another thread stops the game's thread for a moment and notes the address it runs at,
// and where in the game's or the mod's code that was called from (the first word on its stack that a call in
// that code comes back to). The places seen most often are where the time goes.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "str.h"

namespace Frames {
	/******************************************************************/
	/*  How often each of many keys was seen                          */
	/******************************************************************/

	struct Entry {
		uint32_t key;
		uint32_t count;
	};

	// A table of a fixed size (SLOTS, a power of two): no memory is taken once it is there, and adding to it
	// takes no lock, so one thread may add while it has another stopped. A key that finds no room is counted as
	// lost.
	template <int SLOTS>
	class Counts {
	public:
		Counts() {
			Clear();
		}

		void Clear() {
			memset(m_key, 0, sizeof(m_key));
			memset(m_count, 0, sizeof(m_count));
			m_total = 0;
			m_lost = 0;
			m_used = 0;
		}

		void Add(uint32_t key) {
			m_total++;
			uint32_t i = Hash(key) & (uint32_t)(SLOTS - 1);
			for (int probe = 0; probe < kProbes; probe++) {
				if (m_count[i] == 0) {
					if (m_used >= (uint32_t)(SLOTS / 4 * 3)) {
						break;      // three quarters full: no new keys
					}
					m_key[i] = key;
					m_count[i] = 1;
					m_used++;
					return;
				}
				if (m_key[i] == key) {
					m_count[i]++;
					return;
				}
				i = (i + 1) & (uint32_t)(SLOTS - 1);
			}
			m_lost++;
		}

		// Every Add, with those that found no room; those; the keys it holds
		uint32_t Total() const {
			return m_total;
		}
		uint32_t Lost() const {
			return m_lost;
		}
		uint32_t Keys() const {
			return m_used;
		}

		// The n keys seen most, the most seen first (of two seen as often, the lower key); answers how many
		// of them there are
		int Top(Entry* out, int n) const {
			int have = 0;
			for (int i = 0; i < SLOTS; i++) {
				if (m_count[i] == 0) {
					continue;
				}
				int at = have;
				while (at > 0 && (out[at - 1].count < m_count[i] || (out[at - 1].count == m_count[i] && out[at - 1].key > m_key[i]))) {
					at--;
				}
				if (at >= n) {
					continue;
				}
				const int last = have < n ? have : n - 1;
				for (int k = last; k > at; k--) {
					out[k] = out[k - 1];
				}
				out[at].key = m_key[i];
				out[at].count = m_count[i];
				if (have < n) {
					have++;
				}
			}
			return have;
		}

		template <class F>
		void ForEach(F f) const {
			for (int i = 0; i < SLOTS; i++) {
				if (m_count[i]) {
					f(m_key[i], m_count[i]);
				}
			}
		}

	private:
		enum { kProbes = 32 };

		static uint32_t Hash(uint32_t k) {
			k ^= k >> 16;
			k *= 0x7FEB352Du;
			k ^= k >> 15;
			k *= 0x846CA68Bu;
			k ^= k >> 16;
			return k;
		}

		uint32_t m_key[SLOTS];
		uint32_t m_count[SLOTS];
		uint32_t m_total;
		uint32_t m_lost;
		uint32_t m_used;
	};

	/******************************************************************/
	/*  Where a thread was called from, by its stack                  */
	/******************************************************************/

	// The length of a call through a register or through memory (FF /2) that starts at p, or 0 when what is
	// there is no such call. p[0] to p[2] are read.
	inline int IndirectCallLength(const uint8_t* p) {
		if (p[0] != 0xFF || (p[1] & 0x38) != 0x10) {
			return 0;
		}
		const int mod = p[1] >> 6, rm = p[1] & 7;
		if (mod == 3) {
			return 2;                               // call eax
		}
		int n = 2;
		if (rm == 4) {
			n++;                                    // a byte for base and index
			if (mod == 0 && (p[2] & 7) == 5) {
				n += 4;                             // no base: an address with it
			}
		}
		if (mod == 0 && rm == 5) {
			n += 4;                                 // call [address]
		}
		if (mod == 1) {
			n += 1;                                 // call [eax+8]
		}
		if (mod == 2) {
			n += 4;                                 // call [eax+1234h]
		}
		return n;
	}

	// Can an address be where a call comes back to: does a call instruction end just before it? window: the 7
	// bytes before the address and the one at it.
	inline bool AfterCall(const uint8_t window[8]) {
		if (window[2] == 0xE8) {
			return true;                            // call to a place (5 bytes)
		}
		for (int n = 2; n <= 7; n++) {
			if (IndirectCallLength(window + 7 - n) == n) {
				return true;
			}
		}
		return false;
	}

	// The first word of a stack that a call in the watched code comes back to, or 0. stack: its words from the
	// top; inCode(address): is that in the watched code; window(address, out): the 8 bytes AfterCall wants, false
	// if they cannot be read.
	template <class InCode, class Window>
	uint32_t Caller(const uint32_t* stack, size_t words, InCode inCode, Window window) {
		for (size_t i = 0; i < words; i++) {
			const uint32_t a = stack[i];
			uint8_t w[8];
			if (inCode(a) && window(a, w) && AfterCall(w)) {
				return a;
			}
		}
		return 0;
	}

	/******************************************************************/
	/*  The frames                                                    */
	/******************************************************************/

	static const double kGapMs = 5000.0;        // longer than this is no frame: a load, the window away

	// One frame, in ms. The parts are below 0 when they are not known (nothing was drawn, or the hooks that
	// mark them are not in).
	struct FrameTimes {
		double whole;
		double before;          // from the start of the frame (the pad is read) to the first mesh drawn
		double drawing;         // from there to the end of the drawing (just before the device's EndScene)
		double after;           // from there to the start of the next frame (the frame is shown in here)
		double inDraw;          // of the drawing: inside Direct3D's draw calls
		double ownDraw;         // of the drawing: the mod's own work around each of those calls
		uint32_t draws;         // meshes drawn
		uint32_t shifted;       // of them, drawn relative to their middles
	};

	// What a stretch of frames came to
	struct Period {
		uint32_t frames;        // those counted
		uint32_t gaps;          // those too long to be frames, left out of everything else
		double ms;              // the counted frames' time
		double longest;
		double longestBefore, longestDrawing, longestAfter;     // the longest frame's parts (below 0: not known)
		double median;          // the usual frame, to the ms
		uint32_t over33, over50, over100;
		uint32_t parted;        // frames whose parts are known; the sums of those parts:
		double before, drawing, after, inDraw, ownDraw;
		uint64_t draws;
		uint64_t shifted;
		uint32_t mostDraws;
	};

	class Pace {
	public:
		Pace() {
			Reset();
		}

		void Reset() {
			memset(&m_p, 0, sizeof(m_p));
			memset(m_ms, 0, sizeof(m_ms));
		}

		void Frame(const FrameTimes& f) {
			if (!(f.whole >= 0.0) || f.whole >= kGapMs) {
				m_p.gaps++;
				return;
			}
			m_p.frames++;
			m_p.ms += f.whole;
			if (f.whole > m_p.longest) {
				m_p.longest = f.whole;
				m_p.longestBefore = f.before;
				m_p.longestDrawing = f.drawing;
				m_p.longestAfter = f.after;
			}
			m_p.over33 += f.whole > 33.4 ? 1 : 0;
			m_p.over50 += f.whole > 50.0 ? 1 : 0;
			m_p.over100 += f.whole > 100.0 ? 1 : 0;
			const int whole = (int)f.whole;
			m_ms[whole < kMs - 1 ? whole : kMs - 1]++;
			m_p.draws += f.draws;
			m_p.shifted += f.shifted;
			if (f.draws > m_p.mostDraws) {
				m_p.mostDraws = f.draws;
			}
			if (f.before >= 0.0 && f.drawing >= 0.0 && f.after >= 0.0) {
				m_p.parted++;
				m_p.before += f.before;
				m_p.drawing += f.drawing;
				m_p.after += f.after;
				m_p.inDraw += f.inDraw > 0.0 ? f.inDraw : 0.0;
				m_p.ownDraw += f.ownDraw > 0.0 ? f.ownDraw : 0.0;
			}
		}

		Period Sum() const {
			Period p = m_p;
			uint32_t seen = 0;
			for (int i = 0; i < kMs; i++) {
				seen += m_ms[i];
				if (seen * 2 >= p.frames && p.frames) {
					p.median = (double)i + 0.5;
					break;
				}
			}
			return p;
		}

	private:
		enum { kMs = 256 };

		Period m_p;
		uint32_t m_ms[kMs];     // how many frames took each whole number of ms (the last: that and longer)
	};

	// Worth a report: 25 ms a frame and more, a twentieth of the frames (and three at least) over 33 ms, or one
	// of 100 ms
	inline bool Slow(const Period& p) {
		if (p.frames == 0) {
			return false;
		}
		return p.ms / p.frames >= 25.0 || (p.over33 >= 3 && p.over33 * 20 >= p.frames) || p.longest >= 100.0;
	}

	// Which stretches are told. The first with frames enough to say anything, and after it one in every thirty
	// that were not slow: those are to compare with. Of the slow ones the first three in a row, then every
	// sixth (once a minute), and any that is clearly worse than the last one told: frames a third longer, or
	// its longest frame twice as long. Eighty slow ones in all, so that a game that is always slow by these
	// numbers does not use them up before the bad place comes. And the stretch after something was switched
	// (Next) is told whatever it is like.
	class Teller {
	public:
		enum { kMinFrames = 30, kMost = 80, kQuietEvery = 30 };
		enum { NO = 0, SLOW = 1, COMPARE = 2 };

		Teller() : m_first(true), m_next(false), m_quiet(0), m_run(0), m_told(0), m_each(0.0), m_longest(0.0) {
		}

		// The next stretch with frames enough is told, slow or not
		void Next() {
			m_next = true;
			m_run = 0;
		}

		int Tell(const Period& p) {
			if (p.frames < (uint32_t)kMinFrames) {
				return NO;
			}
			const bool next = m_next;
			m_next = false;
			if (!Slow(p)) {
				m_run = 0;
				if (next || m_first || ++m_quiet >= kQuietEvery) {
					m_first = false;
					m_quiet = 0;
					return COMPARE;
				}
				return NO;
			}
			m_run++;
			const double each = p.ms / p.frames;
			const bool worse = each > m_each * 4.0 / 3.0 || p.longest > m_longest * 2.0;
			if (!next && (m_told >= kMost || !(m_run <= 3 || m_run % 6 == 0 || worse))) {
				return NO;
			}
			m_first = false;
			m_told++;
			m_each = each;
			m_longest = p.longest;
			return SLOW;
		}

		// The slow one just told was the last that will be
		bool Spent() const {
			return m_told >= kMost;
		}

	private:
		bool m_first;
		bool m_next;
		int m_quiet;            // stretches that were not slow since one was told
		int m_run;              // slow ones in a row
		int m_told;             // slow ones told
		double m_each, m_longest;   // of the last slow one told
	};

	// How far into a frame a sample is one of a frame that has run long: twice the usual frame, 25 ms at least
	// and 100 at most
	inline double LongAfter(const Period& p) {
		const double twice = 2.0 * p.median;
		return twice < 25.0 ? 25.0 : (twice > 100.0 ? 100.0 : twice);
	}

	// The stretch in words: "412 frames in 10.0 s: 24.3 ms each (41 a second), ..."
	inline size_t FormatPeriod(char* out, size_t size, const Period& p, double seconds) {
		if (p.frames == 0) {
			return Str::Format(out, size, "no frames in %.1f s (%u stretches of five seconds and more: a load, the window away)", seconds, p.gaps);
		}
		const double each = p.ms / p.frames;
		size_t n = Str::Format(out, size, "%u frames in %.1f s: %.1f ms each (%.0f a second), the usual one %.0f ms, the longest %.0f ms",
			p.frames, seconds, each, each > 0.0 ? 1000.0 / each : 0.0, p.median, p.longest);
		if (p.longestBefore >= 0.0 && p.longestDrawing >= 0.0 && p.longestAfter >= 0.0) {
			n += Str::Format(out + n, size - n, " (%.0f of it before the level is drawn, %.0f drawing, %.0f after)", p.longestBefore, p.longestDrawing,
				p.longestAfter);
		}
		n += Str::Format(out + n, size - n, "; %u over 33 ms, %u over 50, %u over 100", p.over33, p.over50, p.over100);
		if (p.gaps) {
			n += Str::Format(out + n, size - n, "; %u stretches of five seconds and more left out (a load, the window away)", p.gaps);
		}
		if (p.draws) {
			n += Str::Format(out + n, size - n, "; %.0f meshes drawn a frame (%u at most), %.0f of them relative to their middles",
				(double)p.draws / p.frames, p.mostDraws, (double)p.shifted / p.frames);
		}
		if (p.parted) {
			const double k = 1.0 / p.parted;
			n += Str::Format(out + n, size - n, "; of a frame: %.1f ms before the level is drawn, %.1f ms drawing (%.1f in Direct3D's draw calls, %.2f the mod's own work for them), %.1f ms after",
				p.before * k, p.drawing * k, p.inDraw * k, p.ownDraw * k, p.after * k);
		}
		return n;
	}

	// The keys seen most as a list: "31.2% name; 12.0% name; ..." with the names from name(key, out, size)
	template <class Name>
	size_t FormatTop(char* out, size_t size, const Entry* top, int n, uint32_t total, Name name) {
		size_t used = 0;
		if (size) {
			out[0] = 0;
		}
		for (int i = 0; i < n && total; i++) {
			char what[64];
			name(top[i].key, what, sizeof(what));
			used += Str::Format(out + used, size - used, "%s%.1f%% %s", i ? "; " : "", 100.0 * top[i].count / total, what);
		}
		return used;
	}
}
