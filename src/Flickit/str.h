#pragma once
// Text into a buffer of a known size, never past its end, and never anything but text when it does not fit.
//
// The C library's sprintf_s and strcpy_s do not cut a text short: when it does not fit they call the
// library's handler for a wrong parameter, and that ends the game. A note about numbers that have gone wrong
// is exactly where a text gets longer than anyone thought (a position of 1e30 is 31 digits), so everything
// in the mod that writes text into a buffer goes through Format or Copy: the text is cut short to fit, and
// the buffer always ends in a 0.
//
// Pure, tested by tests/str_test.cpp.
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define STR_PRINTF(text, first) __attribute__((format(printf, text, first)))
#else
#define STR_PRINTF(text, first)
#endif

namespace Str {
	// As vsnprintf, with the 0 at the end always there. Answers how many characters are in out (without the 0).
	inline size_t FormatV(char* out, size_t size, const char* format, va_list args) {
		if (!out || size == 0) {
			return 0;
		}
		const int n = vsnprintf(out, size, format, args);
		out[size - 1] = 0;
		if (n < 0) {
			// (an error in the format; older C libraries also answer this for a text that did not fit)
			return strlen(out);
		}
		return (size_t)n < size ? (size_t)n : size - 1;
	}

	inline size_t Format(char* out, size_t size, const char* format, ...) STR_PRINTF(3, 4);
	inline size_t Format(char* out, size_t size, const char* format, ...) {
		va_list args;
		va_start(args, format);
		const size_t n = FormatV(out, size, format, args);
		va_end(args);
		return n;
	}

	// from (nothing, for a null) into to, cut short to fit; what is left of to is zeroes
	inline void Copy(char* to, size_t size, const char* from) {
		if (!to || size == 0) {
			return;
		}
		size_t n = from ? strlen(from) : 0;
		if (n > size - 1) {
			n = size - 1;
		}
		if (n) {
			memcpy(to, from, n);
		}
		memset(to + n, 0, size - n);
	}
}
