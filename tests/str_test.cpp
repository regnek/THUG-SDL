// Tests for str.h: text into a buffer, cut short to fit, never past its end.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src/Flickit tests/str_test.cpp -o str_test

#include "str.h"

#include <cstdio>
#include <cstring>

static int s_checks = 0;
static int s_failed = 0;

static void Check(bool ok, const char* what) {
	s_checks++;
	if (!ok) {
		s_failed++;
		std::printf("FAILED: %s\n", what);
	}
}

int main() {
	// The buffers are on the heap at their exact size, so a write past the end is AddressSanitizer's to see
	{
		char* b = new char[16];
		std::memset(b, 'x', 16);
		size_t n = Str::Format(b, 16, "%s %d", "spot", 7);
		Check(n == 6 && std::strcmp(b, "spot 7") == 0, "a text that fits is the text");
		n = Str::Format(b, 16, "%s", "exactly fifteen.");
		Check(n == 15 && std::strcmp(b, "exactly fifteen") == 0, "one too long loses its last character");
		n = Str::Format(b, 16, "at (%.1f %.1f %.1f)", 1.0e30, -1.0e30, 1.0e30);
		Check(n == 15 && std::strlen(b) == 15 && std::strncmp(b, "at (10000000000", 15) == 0, "numbers gone wild are cut short, not written past the end");
		n = Str::Format(b, 16, "%s", "");
		Check(n == 0 && b[0] == 0, "nothing is an empty text");
		delete[] b;
	}
	{
		char* one = new char[1];
		one[0] = 'x';
		const size_t n = Str::Format(one, 1, "%d", 12345);
		Check(n == 0 && one[0] == 0, "a buffer of one holds the end alone");
		delete[] one;
		Check(Str::Format(0, 16, "%d", 1) == 0, "no buffer: nothing written");
		char keep[4] = { 'a', 'b', 'c', 0 };
		Check(Str::Format(keep, 0, "%d", 1) == 0 && keep[0] == 'a', "no room: nothing written");
	}
	{
		// What follows a formatted text in the same buffer goes where it ends
		char* b = new char[24];
		size_t used = Str::Format(b, 24, "%s", "one");
		used += Str::Format(b + used, 24 - used, " %s", "two");
		used += Str::Format(b + used, 24 - used, " %s", "three and more than fits");
		Check(used == 23 && std::strcmp(b, "one two three and more ") == 0, "appended piece by piece, the last cut short");
		used += Str::Format(b + used, 24 - used, "%s", "more");
		Check(used == 23 && std::strlen(b) == 23, "a full buffer takes no more");
		delete[] b;
	}
	{
		char* b = new char[8];
		std::memset(b, 'x', 8);
		Str::Copy(b, 8, "car");
		Check(std::strcmp(b, "car") == 0 && b[4] == 0 && b[7] == 0, "a copy that fits, the rest zeroes");
		Str::Copy(b, 8, "a longer name");
		Check(std::strcmp(b, "a longe") == 0, "a copy cut short ends in a 0");
		Str::Copy(b, 8, 0);
		Check(b[0] == 0 && b[7] == 0, "a null is an empty text");
		Str::Copy(b, 8, "seven77");
		Check(std::strcmp(b, "seven77") == 0, "one that just fits");
		delete[] b;
		char keep[2] = { 'a', 'b' };
		Str::Copy(keep, 0, "zz");
		Check(keep[0] == 'a', "no room: nothing copied");
	}

	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
