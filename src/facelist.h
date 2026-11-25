#pragma once
// The faces of a collision object that a box reaches, kept in a list with an end (bigworld.cpp puts it in the
// game).
//
// The game's collision finds those faces by walking the object's BSP tree, and appends the face numbers of each
// leaf it reaches to one list (16 bits a number), which it empties at the start of each box. The game's own list
// holds 6000 numbers and the game appended without looking at how many it held: a box that reached more wrote
// over what follows the list, the pointers to the game's managers (the crashes while driving across the joined
// world). Append never writes past the end of the list it is given: a leaf that does not fit in what is left is
// cut short, and what did not fit is counted.
//
// Pure, tested by tests/facelist_test.cpp.
#include <stdint.h>
#include <string.h>

namespace FaceList {
	struct Tally {
		uint32_t most;          // the most numbers a box has reached
		uint32_t past;          // boxes that went past the mark (the game's 6000)
		uint32_t dropped;       // numbers that did not fit, all boxes together
	};

	// Appends n numbers from `from` to `list` (room for cap of them) after the *count already there, and moves
	// *count on. A count past cap (which this never leaves) is taken as cap. The tally keeps the most, the boxes
	// that went past `mark` (one each: a box's count only grows) and what did not fit.
	inline void Append(uint16_t* list, uint32_t cap, volatile uint32_t* count, const uint16_t* from, uint32_t n,
		uint32_t mark, Tally* tally) {
		uint32_t at = *count;
		if (at > cap) {
			at = cap;
		}
		const uint32_t room = cap - at;
		if (n > room) {
			tally->dropped += n - room;
			n = room;
		}
		if (n > 0) {
			memcpy(list + at, from, n * sizeof(uint16_t));
		}
		if (at <= mark && at + n > mark) {
			tally->past++;
		}
		at += n;
		*count = at;
		if (at > tally->most) {
			tally->most = at;
		}
	}

	// What is worth a note since the last one: numbers that did not fit (first, every time there are more), or
	// a box past the most told (the first past the mark it starts at, then each a quarter beyond the one
	// before, so a few notes on the way to the end of the list and not one a box).
	enum News { kNoNews, kDropped, kMost };

	struct Told {
		uint32_t most;          // the next note on the most is for a box past this
		uint32_t dropped;
	};

	inline Told FirstTold(uint32_t mark) {
		Told t;
		t.most = mark;
		t.dropped = 0;
		return t;
	}

	inline News NewsOf(const Tally& tally, Told* told) {
		if (tally.dropped != told->dropped) {
			told->dropped = tally.dropped;
			return kDropped;
		}
		if (tally.most > told->most) {
			told->most = tally.most + tally.most / 4;
			return kMost;
		}
		return kNoNews;
	}
}
