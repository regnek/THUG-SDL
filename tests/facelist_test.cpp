// Tests for facelist.h: the faces a box reaches, appended to a list that is never written past its end, and the
// notes on them.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src tests/facelist_test.cpp -o facelist_test

#include "facelist.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int s_checks = 0;
static int s_failed = 0;

static void Check(bool ok, const char* what) {
	s_checks++;
	if (!ok) {
		s_failed++;
		std::printf("FAILED: %s\n", what);
	}
}

// A leaf: n numbers from `first` on
static std::vector<uint16_t> Leaf(uint32_t n, uint16_t first) {
	std::vector<uint16_t> v(n);
	for (uint32_t i = 0; i < n; i++) {
		v[i] = (uint16_t)(first + i);
	}
	return v;
}

static FaceList::Tally NoTally() {
	FaceList::Tally t;
	t.most = 0;
	t.past = 0;
	t.dropped = 0;
	return t;
}

int main() {
	// The list is on the heap at its exact size, so a write past its end is AddressSanitizer's to see
	const uint32_t cap = 16;
	const uint32_t mark = 6;
	uint16_t* list = new uint16_t[cap];

	{
		FaceList::Tally t = NoTally();
		uint32_t count = 0;
		std::vector<uint16_t> a = Leaf(4, 100);
		std::vector<uint16_t> b = Leaf(3, 200);
		FaceList::Append(list, cap, &count, a.data(), 4, mark, &t);
		Check(count == 4, "a leaf of four: four in the list");
		Check(list[0] == 100 && list[3] == 103, "the leaf's numbers, in order");
		Check(t.most == 4 && t.past == 0 && t.dropped == 0, "the tally after one leaf");
		FaceList::Append(list, cap, &count, b.data(), 3, mark, &t);
		Check(count == 7, "a second leaf goes after the first");
		Check(list[3] == 103 && list[4] == 200 && list[6] == 202, "the second leaf's numbers follow the first's");
		Check(t.most == 7, "the most is the count");
		Check(t.past == 1, "a box that goes past the mark is counted once");
		FaceList::Append(list, cap, &count, a.data(), 4, mark, &t);
		Check(count == 11 && t.past == 1, "the same box, further past the mark: still once");
		Check(t.dropped == 0, "all of it fitted");
	}

	{
		// A leaf that does not fit in what is left: cut short at the end of the list
		FaceList::Tally t = NoTally();
		uint32_t count = 12;
		std::vector<uint16_t> a = Leaf(10, 300);
		list[11] = 7;
		FaceList::Append(list, cap, &count, a.data(), 10, mark, &t);
		Check(count == cap, "a leaf too big for what is left fills the list to its end");
		Check(list[11] == 7, "what was there before stays");
		Check(list[12] == 300 && list[15] == 303, "the part that fits is the leaf's first numbers");
		Check(t.dropped == 6, "what did not fit is counted");
		Check(t.most == cap, "the most is the list's end");
		Check(t.past == 0, "a box that started past the mark is not counted again");

		// More leaves once it is full: nothing written, all of them counted
		FaceList::Append(list, cap, &count, a.data(), 10, mark, &t);
		Check(count == cap && t.dropped == 16, "a leaf with the list full: none of it in, all of it counted");
		FaceList::Append(list, cap, &count, a.data(), 0, mark, &t);
		Check(count == cap && t.dropped == 16, "an empty leaf changes nothing");
	}

	{
		// A count past the end (which Append never leaves, but the game's own number could be): taken as the end
		FaceList::Tally t = NoTally();
		uint32_t count = 40000;
		std::vector<uint16_t> a = Leaf(3, 1);
		FaceList::Append(list, cap, &count, a.data(), 3, mark, &t);
		Check(count == cap, "a count past the end is put back to the end");
		Check(t.dropped == 3, "and the leaf did not fit");
	}

	{
		// The mark: a leaf that ends on it is not past it; the next one is
		FaceList::Tally t = NoTally();
		uint32_t count = 0;
		std::vector<uint16_t> a = Leaf(6, 1);
		FaceList::Append(list, cap, &count, a.data(), 6, mark, &t);
		Check(count == mark && t.past == 0, "a box that reaches the mark exactly is not past it");
		FaceList::Append(list, cap, &count, a.data(), 1, mark, &t);
		Check(t.past == 1, "one more number and it is");
		// The next box: the game empties the list at its start
		count = 0;
		FaceList::Append(list, cap, &count, a.data(), 6, mark, &t);
		FaceList::Append(list, cap, &count, a.data(), 6, mark, &t);
		Check(t.past == 2, "a second box past the mark");
		Check(t.most == 12, "the most of all boxes");
		count = 0;
		FaceList::Append(list, cap, &count, a.data(), 2, mark, &t);
		Check(t.most == 12 && count == 2, "a smaller box after it leaves the most as it was");
	}

	{
		// The sizes in the game: 6000 in the game's list, 65536 in the DLL's. A box of 7000 fits whole.
		const uint32_t big = 65536;
		uint16_t* faces = new uint16_t[big];
		FaceList::Tally t = NoTally();
		uint32_t count = 0;
		std::vector<uint16_t> leaf = Leaf(700, 0);
		for (int i = 0; i < 10; i++) {
			FaceList::Append(faces, big, &count, leaf.data(), 700, 6000, &t);
		}
		Check(count == 7000 && t.dropped == 0 && t.past == 1, "a box of 7000 faces: all in, once past 6000");
		Check(faces[6999] == 699, "its last number is where it goes");
		// And one of 70000 stops at the end, with the rest counted
		count = 0;
		std::vector<uint16_t> huge = Leaf(65535, 0);
		FaceList::Append(faces, big, &count, huge.data(), 65535, 6000, &t);
		FaceList::Append(faces, big, &count, huge.data(), 4465, 6000, &t);
		Check(count == big, "a box of 70000: the list full");
		Check(t.dropped == 70000 - big, "and the 4464 that did not fit counted");
		Check(t.past == 2, "a second box past 6000");
		Check(faces[big - 1] == 0, "the last number in the list is the second leaf's first");
		delete[] faces;
	}

	{
		// The notes
		FaceList::Tally t = NoTally();
		FaceList::Told told = FaceList::FirstTold(6000);
		Check(FaceList::NewsOf(t, &told) == FaceList::kNoNews, "nothing yet: no note");
		t.most = 6000;
		Check(FaceList::NewsOf(t, &told) == FaceList::kNoNews, "a box of 6000 fitted the game's list: no note");
		t.most = 6001;
		t.past = 1;
		Check(FaceList::NewsOf(t, &told) == FaceList::kMost, "the first box past 6000: a note");
		Check(told.most == 6001 + 1500, "the next note on the most is a quarter beyond it");
		Check(FaceList::NewsOf(t, &told) == FaceList::kNoNews, "the same most again: no note");
		t.most = 7000;
		Check(FaceList::NewsOf(t, &told) == FaceList::kNoNews, "a little more: no note");
		t.most = 8000;
		Check(FaceList::NewsOf(t, &told) == FaceList::kMost, "a quarter more: a note");
		t.dropped = 5;
		t.most = 65536;
		Check(FaceList::NewsOf(t, &told) == FaceList::kDropped, "numbers that did not fit come first");
		Check(FaceList::NewsOf(t, &told) == FaceList::kMost, "then the most");
		Check(FaceList::NewsOf(t, &told) == FaceList::kNoNews, "then nothing");
		t.dropped = 9;
		Check(FaceList::NewsOf(t, &told) == FaceList::kDropped, "more that did not fit: a note each time");
		Check(FaceList::NewsOf(t, &told) == FaceList::kNoNews, "and nothing after it");

		// How many notes on the most a session has at most, on the way from 6000 to the end of the list
		FaceList::Tally u = NoTally();
		FaceList::Told toldU = FaceList::FirstTold(6000);
		int notes = 0;
		for (uint32_t most = 6000; most <= 65536; most++) {
			u.most = most;
			if (FaceList::NewsOf(u, &toldU) == FaceList::kMost) {
				notes++;
			}
		}
		Check(notes > 0 && notes <= 12, "from 6000 to the end, a dozen notes on the most at most");
	}

	delete[] list;
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
