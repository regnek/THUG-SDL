// Tests for autosave.h: the text round trip, the slots, when to write, and when to put the skater back.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src tests/autosave_test.cpp -o autosave_test

#include "autosave.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static int s_checks = 0;
static int s_failed = 0;

static void Check(bool ok, const char* what) {
	s_checks++;
	if (!ok) {
		s_failed++;
		std::printf("FAILED: %s\n", what);
	}
}

static Autosave::Spot Make(uint32_t level, float x, float y, float z, float heading) {
	Autosave::Spot s;
	std::memset(&s, 0, sizeof(s));
	s.level = level;
	s.saved = 1700000000u + level % 1000;
	s.pos[0] = x;
	s.pos[1] = y;
	s.pos[2] = z;
	s.pos[3] = 1.0f;
	const float c = std::cos(heading), n = std::sin(heading);
	// left, up, forward rows, as the game keeps them, and a fourth row
	const float m[16] = { c, 0, -n, 0, 0, 1, 0, 0, n, 0, c, 0, 0, 0, 0, 1 };
	std::memcpy(s.matrix, m, sizeof(m));
	return s;
}

static bool Same(const Autosave::Spot& a, const Autosave::Spot& b) {
	return a.level == b.level && a.saved == b.saved && std::memcmp(a.pos, b.pos, sizeof(a.pos)) == 0
		&& std::memcmp(a.matrix, b.matrix, sizeof(a.matrix)) == 0;
}

static void TestText() {
	Autosave::Slots slots;
	const Autosave::Spot a = Make(0x8E3E4C1Au, 22856.123f, 51.6f, -7861.5f, 1.234567f);
	const Autosave::Spot b = Make(0x1234ABCDu, -0.000123f, 1.0e-7f, 39104.99f, -2.5f);
	slots.Put(a);
	slots.Put(b);
	const std::string text = slots.Text();
	Autosave::Slots back;
	Check(back.Read(text.c_str()), "reads its own text");
	Check(back.Count() == 2, "both levels");
	Check(back.Find(a.level) && Same(*back.Find(a.level), a), "the first spot exactly as it was");
	Check(back.Find(b.level) && Same(*back.Find(b.level), b), "the second spot exactly as it was");
	Check(back.Find(0x55555555u) == 0, "no spot for a level never saved");

	// a line cut short (a crash while writing would leave the old file, but a hand edit might not)
	std::string cut = text.substr(0, text.size() - 40);
	Autosave::Slots part;
	part.Read(cut.c_str());
	Check(part.Find(a.level) != 0 && part.Find(b.level) == 0, "a line cut short is left out, the whole one kept");

	Check(!back.Read(""), "nothing to read: false");
	Check(back.Count() == 0, "and empty");
	Check(!back.Read("level=zz saved=1 pos=1,2,3\n; junk\n"), "junk: false");

	// spots that are not whole are not read
	Autosave::Spot nanSpot = Make(7, 1, 2, 3, 0);
	nanSpot.pos[1] = std::numeric_limits<float>::quiet_NaN();
	Check(!Autosave::Slots::Whole(nanSpot), "a NaN position is not whole");
	Autosave::Spot far = Make(7, 2.0e6f, 2, 3, 0);
	Check(!Autosave::Slots::Whole(far), "nor one past reach");
	Autosave::Spot skew = Make(7, 1, 2, 3, 0);
	skew.matrix[0] = 3.0f;
	Check(!Autosave::Slots::Whole(skew), "nor a matrix with a long row");
	Autosave::Slots bad;
	bad.Put(nanSpot);
	Autosave::Slots badBack;
	Check(!badBack.Read(bad.Text().c_str()), "a written NaN does not read back as a spot");
	// Windows line ends
	std::string crlf;
	for (char ch : text) {
		if (ch == '\n') {
			crlf += '\r';
		}
		crlf += ch;
	}
	Autosave::Slots win;
	Check(win.Read(crlf.c_str()) && win.Count() == 2 && Same(*win.Find(a.level), a), "reads a file with Windows line ends");
}

static void TestSlots() {
	Autosave::Slots slots;
	for (uint32_t i = 0; i < (uint32_t)Autosave::Slots::kMost; i++) {
		Autosave::Spot s = Make(100 + i, (float)i, 0, 0, 0);
		s.saved = 1000 + i;
		slots.Put(s);
	}
	Check(slots.Count() == Autosave::Slots::kMost, "full");
	Autosave::Spot again = Make(105, 55.0f, 0, 0, 0);
	again.saved = 5000;
	slots.Put(again);
	Check(slots.Count() == Autosave::Slots::kMost && slots.Find(105)->pos[0] == 55.0f, "a level saved again takes its own place");
	Autosave::Spot fresh = Make(999, 9.0f, 0, 0, 0);
	fresh.saved = 6000;
	slots.Put(fresh);
	Check(slots.Find(999) != 0 && slots.Find(100) == 0, "a new level takes the place of the one saved longest ago");
	Check(slots.Find(105) != 0 && slots.Find(101) != 0, "the others stay");
}

static void TestThrottle() {
	Autosave::Throttle t;
	Autosave::Spot s = Make(1, 0, 0, 0, 0);
	Check(t.Due(s, 1000), "the first spot is written at once");
	t.Wrote(s, 1000);
	Autosave::Spot moved = Make(1, 5, 0, 0, 0);
	Check(!t.Due(moved, 1250), "not within a second");
	Check(!t.Due(moved, 1999), "nor just short of one");
	Check(t.Due(moved, 2000), "moved, a second later: written");
	Autosave::Spot still = Make(1, 0.5f, 0, 0, 0);
	Check(!t.Due(still, 5000), "half a unit and no turn: not written, however long");
	Autosave::Spot turned = Make(1, 0, 0, 0, 0.05f);
	Check(t.Due(turned, 5000), "turned three degrees: written");
	Autosave::Spot hair = Make(1, 0, 0, 0, 0.005f);
	Check(!t.Due(hair, 5000), "turned a third of a degree: not");
	Autosave::Spot other = Make(2, 0, 0, 0, 0);
	Check(t.Due(other, 2300), "another level: written");
	// the clock wraps
	Autosave::Throttle w;
	w.Wrote(s, 0xFFFFFF00u);
	Check(!w.Due(moved, 0xFFFFFF80u), "near the wrap, not yet");
	Check(!w.Due(moved, 0x00000100u), "across the wrap, half a second on: not yet");
	Check(w.Due(moved, 0x00000300u), "across the wrap, a second on: due");
	w.Forget();
	Check(w.Due(s, 0x00000101u), "forgotten: due at once");
}

static void TestSession() {
	Autosave::Session s;
	int a = 0, b = 0;
	void* skater = &a;
	Check(s.Frame(0x10, 1, skater, true) == Autosave::WAIT, "the first frame of a level: wait");
	for (int i = 1; i < Autosave::Session::kReadyFrames - 1; i++) {
		s.Frame(0x10, 1, skater, true);
	}
	Check(s.Frame(0x10, 1, skater, true) == Autosave::RESTORE, "ready for half a second: put him back");
	Check(s.Frame(0x10, 1, skater, true) == Autosave::TAKE, "then take spots");
	Check(s.Frame(0x10, 1, skater, false) == Autosave::TAKE, "not ready later: still taking (the frame's own test says whether to keep it)");

	// a new level
	Check(s.Frame(0x20, 1, skater, true) == Autosave::WAIT, "a new level: wait again");
	for (int i = 0; i < 10; i++) {
		s.Frame(0x20, 1, skater, true);
	}
	Check(s.Frame(0x20, 1, skater, false) == Autosave::WAIT, "not ready: the count starts over");
	int restores = 0;
	for (int i = 0; i < 100; i++) {
		restores += s.Frame(0x20, 1, skater, true) == Autosave::RESTORE;
	}
	Check(restores == 1, "put back once only");

	// a new skater object in the same level
	Check(s.Frame(0x20, 1, &b, true) == Autosave::WAIT, "a new skater object: wait again");
	Check(!s.Done(), "not done yet");

	// the same level loaded again: a new stay
	Autosave::Session r;
	for (int i = 0; i < Autosave::Session::kReadyFrames; i++) {
		r.Frame(0x30, 5, skater, true);
	}
	Check(r.Done(), "the first load put back");
	Check(r.Frame(0x30, 6, skater, true) == Autosave::WAIT, "the same level loaded again: wait again");
	int again = 0;
	for (int i = 0; i < 100; i++) {
		again += r.Frame(0x30, 6, skater, true) == Autosave::RESTORE;
	}
	Check(again == 1, "and put back once on that load");
}

int main() {
	TestText();
	TestSlots();
	TestThrottle();
	TestSession();
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
