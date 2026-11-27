// Tests for frames.h: the counts of where a thread was seen, the look for a caller on a stack, and the sums
// over a stretch of frames.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src/Flickit tests/frames_test.cpp -o frames_test

#include "frames.h"

#include <cstdio>
#include <cstring>
#include <map>
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

static void TestCounts() {
	Frames::Counts<64>* c = new Frames::Counts<64>();
	Frames::Entry top[8];
	Check(c->Total() == 0 && c->Keys() == 0 && c->Top(top, 8) == 0, "counts: empty at first");
	for (int i = 0; i < 5; i++) {
		c->Add(100);
	}
	c->Add(7);
	c->Add(7);
	c->Add(0);
	c->Add(900000);
	Check(c->Total() == 9 && c->Keys() == 4 && c->Lost() == 0, "counts: four keys, nine seen");
	int n = c->Top(top, 8);
	Check(n == 4 && top[0].key == 100 && top[0].count == 5 && top[1].key == 7 && top[1].count == 2, "counts: the most seen first");
	Check(top[2].key == 0 && top[3].key == 900000 && top[2].count == 1 && top[3].count == 1, "counts: of two seen as often, the lower key first");
	n = c->Top(top, 2);
	Check(n == 2 && top[0].key == 100 && top[1].key == 7, "counts: only as many as asked for");
	uint32_t sum = 0;
	c->ForEach([&sum](uint32_t, uint32_t count) { sum += count; });
	Check(sum == 9, "counts: each key once in a walk");
	// more keys than there is room for: three quarters of the table are filled, the rest is counted as lost
	c->Clear();
	for (uint32_t k = 1; k <= 200; k++) {
		c->Add(k * 977u);
	}
	Check(c->Total() == 200 && c->Keys() == 48 && c->Lost() == 152, "counts: full at three quarters, the rest lost");
	c->Add(977u);
	Check(c->Keys() == 48 && c->Lost() == 152, "counts: a key it holds is still counted when it is full");
	c->Clear();
	Check(c->Total() == 0 && c->Lost() == 0 && c->Top(top, 8) == 0, "counts: cleared");
	delete c;

	// against a map, with many keys and a top that has to drop some
	Frames::Counts<1024>* big = new Frames::Counts<1024>();
	std::map<uint32_t, uint32_t> truth;
	uint32_t seed = 12345;
	for (int i = 0; i < 20000; i++) {
		seed = seed * 1664525u + 1013904223u;
		const uint32_t key = ((seed >> 8) % 300) * ((seed >> 20) % 3 == 0 ? 1 : 7);
		big->Add(key);
		truth[key]++;
	}
	Check(big->Lost() == 0 && big->Keys() == truth.size(), "counts: as many keys as a map has");
	Frames::Entry best[10];
	n = big->Top(best, 10);
	bool same = n == 10;
	std::vector<std::pair<uint32_t, uint32_t> > sorted;      // (count, key)
	for (std::map<uint32_t, uint32_t>::iterator it = truth.begin(); it != truth.end(); ++it) {
		sorted.push_back(std::make_pair(it->second, it->first));
	}
	for (int i = 0; i < 10 && same; i++) {
		// the largest left: the highest count, then the lowest key
		size_t pick = 0;
		for (size_t k = 1; k < sorted.size(); k++) {
			if (sorted[k].first > sorted[pick].first || (sorted[k].first == sorted[pick].first && sorted[k].second < sorted[pick].second)) {
				pick = k;
			}
		}
		same = best[i].count == sorted[pick].first && best[i].key == sorted[pick].second;
		sorted.erase(sorted.begin() + (long)pick);
	}
	Check(same, "counts: the ten most seen are the map's ten");
	delete big;
}

// Eight bytes: seven before an address and the one at it, the last of the seven given (seven at most)
static bool After(std::vector<uint8_t> tail) {
	uint8_t w[8];
	std::memset(w, 0x90, sizeof(w));
	std::memcpy(w + 7 - tail.size(), tail.data(), tail.size());
	return Frames::AfterCall(w);
}

static void TestCalls() {
	const uint8_t reg[3] = { 0xFF, 0xD0, 0x90 };              // call eax
	const uint8_t mem[3] = { 0xFF, 0x10, 0x90 };              // call [eax]
	const uint8_t disp8[3] = { 0xFF, 0x50, 0x08 };            // call [eax+8]
	const uint8_t sib[3] = { 0xFF, 0x14, 0x88 };              // call [eax+ecx*4]
	const uint8_t sib8[3] = { 0xFF, 0x54, 0x24 };             // call [esp+8]
	const uint8_t abs32[3] = { 0xFF, 0x15, 0x00 };            // call [address]
	const uint8_t disp32[3] = { 0xFF, 0x90, 0x48 };           // call [eax+148h]
	const uint8_t sib32[3] = { 0xFF, 0x94, 0x88 };            // call [eax+ecx*4+address]
	const uint8_t table[3] = { 0xFF, 0x14, 0x85 };            // call [eax*4+address]
	const uint8_t jump[3] = { 0xFF, 0xE0, 0x90 };             // jmp eax
	const uint8_t push[3] = { 0xFF, 0x30, 0x90 };             // push [eax]
	Check(Frames::IndirectCallLength(reg) == 2 && Frames::IndirectCallLength(mem) == 2, "calls: through a register, 2 bytes");
	Check(Frames::IndirectCallLength(disp8) == 3 && Frames::IndirectCallLength(sib) == 3, "calls: 3 bytes");
	Check(Frames::IndirectCallLength(sib8) == 4, "calls: call [esp+8] is 4");
	Check(Frames::IndirectCallLength(abs32) == 6 && Frames::IndirectCallLength(disp32) == 6, "calls: with an address, 6");
	Check(Frames::IndirectCallLength(sib32) == 7 && Frames::IndirectCallLength(table) == 7, "calls: 7 bytes");
	Check(Frames::IndirectCallLength(jump) == 0 && Frames::IndirectCallLength(push) == 0, "calls: a jump and a push are no calls");

	Check(After({ 0xE8, 0x11, 0x22, 0x33, 0x44 }), "after: a call to a place");
	Check(After({ 0xFF, 0xD0 }), "after: call eax");
	Check(After({ 0xFF, 0x50, 0x08 }), "after: call [eax+8]");
	Check(After({ 0xFF, 0x54, 0x24, 0x08 }), "after: call [esp+8]");
	Check(After({ 0xFF, 0x15, 0x78, 0x56, 0x34, 0x12 }), "after: call [address]");
	Check(After({ 0x81, 0x48, 0x01, 0x00, 0x00, 0xFF, 0xD0 }), "after: the mesh draw's mov eax, [ecx+148h] / call eax");
	Check(After({ 0xFF, 0x94, 0x88, 0x78, 0x56, 0x34, 0x12 }), "after: call [eax+ecx*4+address]");
	Check(!After({ 0x8B, 0xC1, 0x89, 0x45, 0xFC }), "after: plain code is no call");
	Check(!After({ 0xFF, 0xE0 }), "after: a jump through a register is no call");
	Check(!After({ 0xFF, 0xD0, 0x90 }), "after: a call that ended a byte earlier");
	Check(!After({ 0xE8, 0x11, 0x22, 0x33 }), "after: four bytes after an E8 is inside the call");

	// a stack: numbers, an address in other code, one in the code that does not follow a call, then the caller
	std::vector<uint8_t> code(64, 0x90);
	const uint32_t base = 0x00401000;
	code[20] = 0xE8;                    // a call at base + 20: comes back to base + 25
	code[40] = 0xFF;
	code[41] = 0xD0;                    // call eax at base + 40: comes back to base + 42
	auto inCode = [&](uint32_t a) { return a >= base && a < base + code.size(); };
	auto window = [&](uint32_t a, uint8_t* out) {
		if (a < base + 7 || a >= base + code.size()) {
			return false;
		}
		std::memcpy(out, &code[a - base - 7], 8);
		return true;
	};
	const uint32_t stack[] = { 0, 12345, 0x7FFE0304, base + 30, base + 3, base + 25, base + 42, 7 };
	Check(Frames::Caller(stack, 8, inCode, window) == base + 25, "caller: the first word a call in the code comes back to");
	Check(Frames::Caller(stack, 5, inCode, window) == 0, "caller: none in the words looked at");
	Check(Frames::Caller(stack + 6, 2, inCode, window) == base + 42, "caller: after a call through a register");
	Check(Frames::Caller(stack, 0, inCode, window) == 0, "caller: an empty stack");
}

static Frames::FrameTimes Plain(double ms) {
	Frames::FrameTimes f;
	f.whole = ms;
	f.before = f.drawing = f.after = f.inDraw = f.ownDraw = -1.0;
	f.draws = 0;
	f.shifted = 0;
	return f;
}

static void TestPace() {
	Frames::Pace pace;
	Frames::Period p = pace.Sum();
	char text[600];
	Check(p.frames == 0 && !Frames::Slow(p), "pace: nothing yet");
	Frames::FormatPeriod(text, sizeof(text), p, 10.0);
	Check(std::strcmp(text, "no frames in 10.0 s (0 stretches of five seconds and more: a load, the window away)") == 0, "pace: no frames, in words");
	// 600 frames of 16.7 ms: smooth
	for (int i = 0; i < 600; i++) {
		pace.Frame(Plain(16.7));
	}
	p = pace.Sum();
	Check(p.frames == 600 && p.over33 == 0 && p.median == 16.5 && !Frames::Slow(p), "pace: sixty a second is not slow");
	Check(Frames::LongAfter(p) == 33.0, "pace: long is twice the usual frame");
	// one hitch of 180 ms makes the stretch worth a report
	pace.Frame(Plain(180.0));
	p = pace.Sum();
	Check(p.frames == 601 && p.over33 == 1 && p.over50 == 1 && p.over100 == 1 && p.longest == 180.0 && Frames::Slow(p), "pace: one frame of 180 ms");
	// a load is no frame
	pace.Frame(Plain(14200.0));
	pace.Frame(Plain(-1.0));
	p = pace.Sum();
	Check(p.frames == 601 && p.gaps == 2 && p.longest == 180.0, "pace: a stretch of many seconds is left out");
	Frames::FormatPeriod(text, sizeof(text), p, 10.2);
	Check(std::strstr(text, "601 frames in 10.2 s: 17.0 ms each (59 a second), the usual one 16 ms, the longest 180 ms; 1 over 33 ms, 1 over 50, 1 over 100; 2 stretches of five") == text,
		"pace: in words");
	// a freeze of two seconds is a frame, and a long one
	pace.Frame(Plain(2300.0));
	p = pace.Sum();
	Check(p.frames == 602 && p.longest == 2300.0 && p.gaps == 2, "pace: two seconds of nothing is a frame");

	// a twentieth over 33 ms
	pace.Reset();
	for (int i = 0; i < 100; i++) {
		pace.Frame(Plain(i % 20 == 0 ? 40.0 : 16.0));
	}
	p = pace.Sum();
	Check(p.over33 == 5 && Frames::Slow(p), "pace: a twentieth of the frames over 33 ms is slow");
	pace.Reset();
	for (int i = 0; i < 100; i++) {
		pace.Frame(Plain(i < 2 ? 40.0 : 16.0));
	}
	Check(!Frames::Slow(pace.Sum()), "pace: two long frames in a hundred are not");
	// thirty a second all the time
	pace.Reset();
	for (int i = 0; i < 300; i++) {
		pace.Frame(Plain(33.0));
	}
	p = pace.Sum();
	Check(Frames::Slow(p) && p.over33 == 0 && Frames::LongAfter(p) == 67.0, "pace: 33 ms a frame is slow, and long is past 67");
	pace.Reset();
	for (int i = 0; i < 50; i++) {
		pace.Frame(Plain(5.0));
	}
	Check(Frames::LongAfter(pace.Sum()) == 25.0, "pace: long is 25 ms at least");
	pace.Reset();
	for (int i = 0; i < 50; i++) {
		pace.Frame(Plain(400.0));
	}
	p = pace.Sum();
	Check(p.median == 255.5 && Frames::LongAfter(p) == 100.0, "pace: and 100 at most");

	// the parts and the draws
	pace.Reset();
	for (int i = 0; i < 10; i++) {
		Frames::FrameTimes f = Plain(20.0);
		f.before = 6.0;
		f.drawing = 10.0;
		f.after = 4.0;
		f.inDraw = 7.0;
		f.ownDraw = 0.5;
		f.draws = 3000 + 100 * (uint32_t)i;
		f.shifted = 2000;
		pace.Frame(f);
	}
	pace.Frame(Plain(20.0));        // one without its parts (nothing drawn)
	p = pace.Sum();
	Check(p.frames == 11 && p.parted == 10 && p.mostDraws == 3900 && p.draws == 34500 && p.shifted == 20000, "pace: draws and parts summed");
	Check(p.longest == 20.0 && p.longestBefore == 6.0 && p.longestDrawing == 10.0 && p.longestAfter == 4.0, "pace: the longest frame's parts");
	Frames::FormatPeriod(text, sizeof(text), p, 0.22);
	Check(std::strstr(text, "the longest 20 ms (6 of it before the level is drawn, 10 drawing, 4 after); 0 over 33 ms") != 0, "pace: the longest frame's parts in words");
	{
		Frames::FrameTimes hitch = Plain(250.0);
		hitch.before = 230.0;
		hitch.drawing = 15.0;
		hitch.after = 5.0;
		hitch.inDraw = 9.0;
		hitch.ownDraw = 1.0;
		Frames::Pace other;
		other.Frame(Plain(16.0));
		other.Frame(hitch);
		other.Frame(Plain(17.0));
		const Frames::Period q = other.Sum();
		Check(q.longest == 250.0 && q.longestBefore == 230.0 && q.longestAfter == 5.0, "pace: a hitch keeps its own parts");
	}
	Frames::FormatPeriod(text, sizeof(text), p, 0.22);
	Check(std::strstr(text, "; 3136 meshes drawn a frame (3900 at most), 1818 of them relative to their middles; of a frame: 6.0 ms before the level is drawn, 10.0 ms drawing (7.0 in Direct3D's draw calls, 0.50 the mod's own work for them), 4.0 ms after") != 0,
		"pace: the parts in words");
	// cut short to a small buffer, not past its end
	char* small = new char[40];
	const size_t n = Frames::FormatPeriod(small, 40, p, 0.22);
	Check(n == 39 && std::strlen(small) == 39, "pace: words cut short to fit");
	delete[] small;
}

static Frames::Period Stretch(uint32_t frames, double each, double longest) {
	Frames::Period p;
	std::memset(&p, 0, sizeof(p));
	p.frames = frames;
	p.ms = each * frames;
	p.longest = longest;
	p.median = each;
	return p;
}

static void TestTeller() {
	const Frames::Period good = Stretch(600, 16.7, 20.0), slow = Stretch(250, 40.0, 60.0);
	{
		Frames::Teller t;
		Check(t.Tell(Stretch(20, 500.0, 900.0)) == Frames::Teller::NO, "teller: too few frames say nothing");
		Check(t.Tell(good) == Frames::Teller::COMPARE, "teller: the first stretch is one to compare with");
		bool quiet = true;
		for (int i = 0; i < 29; i++) {
			quiet = quiet && t.Tell(good) == Frames::Teller::NO;
		}
		Check(quiet && t.Tell(good) == Frames::Teller::COMPARE, "teller: then one in thirty that were not slow");
	}
	{
		Frames::Teller t;
		int told[13];
		for (int i = 1; i <= 12; i++) {
			told[i] = t.Tell(slow);
		}
		Check(told[1] == Frames::Teller::SLOW && told[2] == Frames::Teller::SLOW && told[3] == Frames::Teller::SLOW, "teller: the first three slow ones in a row");
		Check(told[4] == Frames::Teller::NO && told[5] == Frames::Teller::NO && told[6] == Frames::Teller::SLOW, "teller: then the sixth");
		Check(told[7] == Frames::Teller::NO && told[11] == Frames::Teller::NO && told[12] == Frames::Teller::SLOW, "teller: and the twelfth");
		Check(t.Tell(Stretch(250, 44.0, 70.0)) == Frames::Teller::NO, "teller: a little worse is more of the same");
		Check(t.Tell(Stretch(180, 56.0, 70.0)) == Frames::Teller::SLOW, "teller: frames a third longer are told");
		Check(t.Tell(Stretch(180, 56.0, 110.0)) == Frames::Teller::NO, "teller: the same again is not");
		Check(t.Tell(Stretch(180, 56.0, 150.0)) == Frames::Teller::SLOW, "teller: a longest frame twice as long is");
		// a stretch that was not slow ends the row: the next three slow ones are told again
		Check(t.Tell(good) == Frames::Teller::NO, "teller: a good stretch after one to compare with was told... is not told");
		Check(t.Tell(slow) == Frames::Teller::SLOW && t.Tell(slow) == Frames::Teller::SLOW && t.Tell(slow) == Frames::Teller::SLOW && t.Tell(slow) == Frames::Teller::NO,
			"teller: a new row of slow ones starts with three");
	}
	{
		// something was switched: the stretch after it is told whatever it is like, and a row starts anew
		Frames::Teller t;
		t.Tell(good);
		t.Next();
		Check(t.Tell(Stretch(10, 16.0, 20.0)) == Frames::Teller::NO, "teller: a stretch with too few frames is still nothing");
		Check(t.Tell(good) == Frames::Teller::COMPARE && t.Tell(good) == Frames::Teller::NO, "teller: the stretch after a switch is told though it is not slow");
		for (int i = 0; i < 5; i++) {
			t.Tell(slow);
		}
		t.Next();
		Check(t.Tell(slow) == Frames::Teller::SLOW && t.Tell(slow) == Frames::Teller::SLOW && t.Tell(slow) == Frames::Teller::SLOW && t.Tell(slow) == Frames::Teller::NO,
			"teller: a switch in a row of slow ones starts the row anew");
	}
	{
		// always slow: every sixth after the first three, eighty in all
		Frames::Teller t;
		int told = 0, asked = 0;
		while (asked < 2000) {
			asked++;
			if (t.Tell(slow) == Frames::Teller::SLOW) {
				told++;
				if (t.Spent()) {
					break;
				}
			}
		}
		Check(told == 80 && asked == 77 * 6, "teller: eighty slow ones last for 77 minutes of nothing but slow");
		bool none = true;
		for (int i = 0; i < 50; i++) {
			none = none && t.Tell(Stretch(100, 90.0, 400.0)) == Frames::Teller::NO;
		}
		Check(none, "teller: then no more slow ones");
		t.Next();
		Check(t.Tell(slow) == Frames::Teller::SLOW && t.Tell(slow) == Frames::Teller::NO, "teller: but the one after a switch");
		Check(t.Tell(good) == Frames::Teller::NO, "teller: (one to compare with was told by the first slow one)");
	}
}

static void TestTop() {
	const Frames::Entry top[3] = { { 0x12D44, 312 }, { 5, 120 }, { 9, 68 } };
	char text[200];
	auto name = [](uint32_t key, char* out, size_t size) { Str::Format(out, size, "place %X", key); };
	size_t n = Frames::FormatTop(text, sizeof(text), top, 3, 1000, name);
	Check(std::strcmp(text, "31.2% place 12D44; 12.0% place 5; 6.8% place 9") == 0 && n == std::strlen(text), "top: a list with shares");
	n = Frames::FormatTop(text, sizeof(text), top, 0, 1000, name);
	Check(n == 0 && text[0] == 0, "top: nothing is an empty text");
	n = Frames::FormatTop(text, sizeof(text), top, 3, 0, name);
	Check(n == 0 && text[0] == 0, "top: nothing seen is an empty text");
	char* small = new char[20];
	n = Frames::FormatTop(small, 20, top, 3, 1000, name);
	Check(n == 19 && std::strlen(small) == 19, "top: cut short to fit");
	delete[] small;
}

int main() {
	TestCounts();
	TestCalls();
	TestPace();
	TestTeller();
	TestTop();
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
