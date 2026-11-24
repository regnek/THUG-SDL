// Tests for guard.h: what is done about a write to the pointer, the call test for the words of a stack, and
// the debug control bits.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src tests/guard_test.cpp -o guard_test

#include "guard.h"

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

static Guard::Write W(bool seen, bool own, uint32_t now, uint32_t before, bool nowIs, bool beforeIs) {
	Guard::Write w;
	w.seen = seen;
	w.gamesOwn = own;
	w.now = now;
	w.before = before;
	w.nowIsManager = nowIs;
	w.beforeIsManager = beforeIs;
	return w;
}

static void TestDecide() {
	const uint32_t manager = 0x0A3C0040;
	const uint32_t other = 0x0B000080;
	// The crash: two 16-bit numbers over the pointer
	Check(Guard::Decide(W(true, false, 0x003B003A, manager, false, true)) == Guard::kPutBack, "the crash's write is put back");
	Check(Guard::Decide(W(false, false, 0x003B003A, manager, false, true)) == Guard::kPutBack, "also when only the look once a frame finds it");
	// Half of it written (a 16-bit write over the low half)
	Check(Guard::Decide(W(true, false, (manager & 0xFFFF0000) | 0x003A, manager, false, true)) == Guard::kPutBack, "half a pointer is put back");
	// The game's own
	Check(Guard::Decide(W(true, true, manager, 0, false, false)) == Guard::kAccept, "the game making it is taken");
	Check(Guard::Decide(W(true, true, 0, manager, false, false)) == Guard::kAccept, "the game ending it is taken");
	// Nothing written that changes it
	Check(Guard::Decide(W(true, false, manager, manager, true, true)) == Guard::kLeave, "the same value written: left");
	// A manager from somewhere unknown
	Check(Guard::Decide(W(true, false, other, manager, true, true)) == Guard::kAccept, "another manager is taken");
	// A zero
	Check(Guard::Decide(W(true, false, 0, manager, false, true)) == Guard::kPutBack, "a zero the breakpoint saw written from elsewhere is put back");
	Check(Guard::Decide(W(false, false, 0, manager, false, true)) == Guard::kAccept, "a zero only found at the look is the game's end of it");
	// Something over a pointer that was not there yet
	Check(Guard::Decide(W(true, false, 0x003B003A, 0, false, false)) == Guard::kPutBack, "garbage over no pointer: back to none");
	// Before was not a manager (should not happen): nothing to go back to
	Check(Guard::Decide(W(true, false, 0x003B003A, 0x12345678, false, false)) == Guard::kLeave, "nothing good to put back: left");
}

// The 7 bytes before an address that has the instruction given (of n bytes) just before it, the rest filler
static bool After(const uint8_t* insn, int n, uint8_t filler = 0x90) {
	uint8_t b[7];
	std::memset(b, filler, sizeof(b));
	std::memcpy(b + 7 - n, insn, (size_t)n);
	return Guard::AfterCall(b);
}

static void TestAfterCall() {
	// Where Kevin's crash's calls return to, as the game's code has them (0x0049029B, 0x004F95B4, 0x004F6A31)
	const uint8_t r1[7] = { 0x24, 0x10, 0xE8, 0x15, 0xF9, 0xF8, 0xFF };
	const uint8_t r2[7] = { 0x8B, 0xCF, 0xE8, 0x6C, 0x66, 0x03, 0x00 };
	const uint8_t r3[7] = { 0x8B, 0xCF, 0xE8, 0x2F, 0x98, 0xF9, 0xFF };
	Check(Guard::AfterCall(r1) && Guard::AfterCall(r2) && Guard::AfterCall(r3), "the crash's return addresses are after calls");
	const uint8_t notCall[7] = { 0xCC, 0xCC, 0xCC, 0xCC, 0x56, 0x8B, 0xF1 };     // 0x0041FBB3, the fault itself
	Check(!Guard::AfterCall(notCall), "the start of a function is not");

	const uint8_t callRel[] = { 0xE8, 0x00, 0x10, 0x00, 0x00 };
	Check(After(callRel, 5), "call rel32");
	const uint8_t callEax[] = { 0xFF, 0xD0 };
	Check(After(callEax, 2), "call eax");
	const uint8_t callEdi[] = { 0xFF, 0xD7 };
	Check(After(callEdi, 2), "call edi");
	const uint8_t callMemEdx[] = { 0xFF, 0x12 };
	Check(After(callMemEdx, 2), "call [edx]");
	const uint8_t callAbs[] = { 0xFF, 0x15, 0x00, 0x30, 0x62, 0x00 };
	Check(After(callAbs, 6), "call [abs32]");
	const uint8_t callDisp8[] = { 0xFF, 0x50, 0x08 };
	Check(After(callDisp8, 3), "call [eax+8]");
	const uint8_t callSib[] = { 0xFF, 0x14, 0x24 };
	Check(After(callSib, 3), "call [esp]");
	const uint8_t callSibDisp8[] = { 0xFF, 0x54, 0x24, 0x08 };
	Check(After(callSibDisp8, 4), "call [esp+8]");
	const uint8_t callDisp32[] = { 0xFF, 0x90, 0x00, 0x01, 0x00, 0x00 };
	Check(After(callDisp32, 6), "call [eax+0x100]");
	const uint8_t callSibDisp32[] = { 0xFF, 0x94, 0x24, 0x00, 0x01, 0x00, 0x00 };
	Check(After(callSibDisp32, 7), "call [esp+0x100]");
	const uint8_t callTable[] = { 0xFF, 0x14, 0x85, 0x00, 0x30, 0x62, 0x00 };
	Check(After(callTable, 7), "call [eax*4+table]");

	const uint8_t pushMem[] = { 0xFF, 0x30 };
	Check(!After(pushMem, 2), "push [eax] is not a call");
	const uint8_t jmpEax[] = { 0xFF, 0xE0 };
	Check(!After(jmpEax, 2), "nor jmp eax");
	const uint8_t incMem[] = { 0xFF, 0x00 };
	Check(!After(incMem, 2), "nor inc [eax]");
	const uint8_t mov[] = { 0x8B, 0x44, 0x24, 0x08 };
	Check(!After(mov, 4), "nor a mov");
	// FF 15 with its 4 bytes is 6 long: the same two bytes nearer the end are not a whole call
	const uint8_t abs2[] = { 0xFF, 0x15 };
	Check(!After(abs2, 2), "call [abs32] without its address is not one");
	const uint8_t sib2[] = { 0xFF, 0x14 };
	Check(!After(sib2, 2), "call [sib] without its SIB byte is not one");
}

static void TestDebugBits() {
	const uint32_t a = 0x0069B6C0;
	Check(Guard::WithBreakpoint(0) == 0x000D0001, "breakpoint 0 on a 4-byte write");
	Check(Guard::HasBreakpoint(a, Guard::WithBreakpoint(0), a), "and that is it");
	// Breakpoint 1, on execute, of someone else's: kept
	const uint32_t other = 0x00000004;                  // L1, RW1 = 00, LEN1 = 00
	Check(Guard::WithBreakpoint(other) == (other | 0x000D0001), "another breakpoint's bits are kept");
	// Breakpoint 0 there, but on reads and writes: made a write one
	const uint32_t rw = 0x000F0001;
	Check(!Guard::HasBreakpoint(a, rw, a), "a read-write one is not ours");
	Check(Guard::WithBreakpoint(rw) == 0x000D0001, "and becomes ours");
	Check(!Guard::HasBreakpoint(a + 4, Guard::WithBreakpoint(0), a), "nor one at another address");
	Check(!Guard::HasBreakpoint(a, 0x000D0000, a), "nor one that is not on");
	// Breakpoint 1 (DR1): bit 2, and bits 20-23
	Check(Guard::WithBreakpoint(0, 1) == 0x00D00004, "breakpoint 1 on a 4-byte write");
	Check(Guard::WithBreakpoint(Guard::WithBreakpoint(0, 0), 1) == 0x00DD0005, "breakpoints 0 and 1 together");
	Check(Guard::HasBreakpoint(a, Guard::WithBreakpoint(Guard::WithBreakpoint(0, 0), 1), a, 1)
		&& Guard::HasBreakpoint(a, Guard::WithBreakpoint(Guard::WithBreakpoint(0, 0), 1), a, 0), "each is there with the other");
	Check(Guard::WithBreakpoint(Guard::WithBreakpoint(0, 1), 0) == Guard::WithBreakpoint(Guard::WithBreakpoint(0, 0), 1), "in either order");
	Check(!Guard::HasBreakpoint(a, Guard::WithBreakpoint(0, 0), a, 1), "breakpoint 0 is not breakpoint 1");
	Check(Guard::Dr7Mask(0) == Guard::kDr7Mask && Guard::Dr7Bits(0) == Guard::kDr7Bits, "breakpoint 0's bits as before");
}

int main() {
	TestDecide();
	TestAfterCall();
	TestDebugBits();
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
