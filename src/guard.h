#pragma once
// The guard: the parts that need neither Windows nor the game, tested by tests/guard_test.cpp.
//
// The game keeps a pointer to its object manager at 0x0069B6C0 (and, 8 bytes before it, one to its component
// manager, the second word the guard watches: see guard_game.cpp). Every look-up of an object by its name goes
// through it: the scripts' ObjectExists, Obj::ResolveToObject, the score's. It is made once, when the game
// starts (0x00420E80, from 0x004BFA20), and only goes when the game ends (0x0041F930). Those are the game's
// only writes to it: its code reads it in 51 places and writes it in 3.
//
// A crash while driving (0x0041FBB3, a read of 0x003B0042) had it at 0x003B003A: something else had written
// over it, with what look like two 16-bit numbers (58 and 59), and the next look-up through it failed. What
// wrote it is not in that crash. The guard puts a write breakpoint of the processor's on that word (guard_game.h):
// the write that comes from anywhere else is caught as it happens, written down with where it came from, and
// the pointer goes back to what it was.
#include <stdint.h>

namespace Guard {
	// What to do about a write to the pointer
	enum Action {
		kAccept,        // it is the pointer now
		kPutBack,       // something wrote over it: what it was goes back
		kLeave,         // nothing to put back
	};

	struct Write {
		bool seen;              // the breakpoint caught it (false: the pointer was found changed, once a frame)
		bool gamesOwn;          // by one of the game's own writes to it
		uint32_t now;           // what it is after the write
		uint32_t before;        // what it was
		bool nowIsManager;      // what it is now points to an object manager (the game's, by its table of functions)
		bool beforeIsManager;
	};

	inline Action Decide(const Write& w) {
		if (w.gamesOwn) {
			return kAccept;
		}
		if (w.now == w.before) {
			return kLeave;
		}
		if (w.nowIsManager) {
			return kAccept;     // another one, as the game would make it
		}
		if (!w.seen && w.now == 0) {
			return kAccept;     // the game's own end of it, in a thread the breakpoint was not on
		}
		return w.beforeIsManager || w.before == 0 ? kPutBack : kLeave;
	}

	// A ModRM byte (the one after FF): its middle three bits are 2 for a call; the others say what follows it
	inline bool CallReg(uint8_t m) { return ((m >> 3) & 7) == 2; }
	inline int Mod(uint8_t m) { return m >> 6; }
	inline int Rm(uint8_t m) { return m & 7; }

	// Does a call instruction end where these 7 bytes do? For telling, of the words on a stack, those that are
	// where a call returns to. A call to a place in the code (E8 and 4 bytes), or through a register or memory
	// (FF with a reg field of 2, in all its lengths: 2 to 7 bytes).
	inline bool AfterCall(const uint8_t b[7]) {
		if (b[2] == 0xE8) {
			return true;
		}
		// 2 bytes: call reg, call [reg]
		if (b[5] == 0xFF && CallReg(b[6]) && (Mod(b[6]) == 3 || (Mod(b[6]) == 0 && Rm(b[6]) != 4 && Rm(b[6]) != 5))) {
			return true;
		}
		// 3 bytes: call [reg + 8 bits], call [a scaled index and a base]
		if (b[4] == 0xFF && CallReg(b[5]) && ((Mod(b[5]) == 1 && Rm(b[5]) != 4) || (Mod(b[5]) == 0 && Rm(b[5]) == 4 && (b[6] & 7) != 5))) {
			return true;
		}
		// 4 bytes: call [a scaled index and a base + 8 bits]
		if (b[3] == 0xFF && CallReg(b[4]) && Mod(b[4]) == 1 && Rm(b[4]) == 4) {
			return true;
		}
		// 6 bytes: call [reg + 32 bits], call [an address]
		if (b[1] == 0xFF && CallReg(b[2]) && ((Mod(b[2]) == 2 && Rm(b[2]) != 4) || (Mod(b[2]) == 0 && Rm(b[2]) == 5))) {
			return true;
		}
		// 7 bytes: call [a scaled index and a base + 32 bits], call [a scaled index + 32 bits]
		if (b[0] == 0xFF && CallReg(b[1]) && Rm(b[1]) == 4 && (Mod(b[1]) == 2 || (Mod(b[1]) == 0 && (b[2] & 7) == 5))) {
			return true;
		}
		return false;
	}

	// The processor's debug control (DR7) for breakpoint i (0 to 3; its address in DR0 to DR3) on a write of the
	// 4 bytes at its address: on in this thread (bit 2i), on a write (bits 16+4i, 17+4i: 01), 4 bytes long (bits
	// 18+4i, 19+4i: 11). The mask is all of breakpoint i's bits, so that the others' are left as they are.
	static const uint32_t kDr7Mask = 0x000F0003;     // breakpoint 0's
	static const uint32_t kDr7Bits = 0x000D0001;

	inline uint32_t Dr7Mask(int i) {
		return (3u << (2 * i)) | (0xFu << (16 + 4 * i));
	}

	inline uint32_t Dr7Bits(int i) {
		return (1u << (2 * i)) | (0xDu << (16 + 4 * i));
	}

	inline uint32_t WithBreakpoint(uint32_t dr7, int i = 0) {
		return (dr7 & ~Dr7Mask(i)) | Dr7Bits(i);
	}

	inline bool HasBreakpoint(uint32_t drAddress, uint32_t dr7, uint32_t address, int i = 0) {
		return drAddress == address && (dr7 & Dr7Mask(i)) == Dr7Bits(i);
	}
}
