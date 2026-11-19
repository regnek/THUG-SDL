#pragma once

// The game's address space, for big levels: whether THUG-SDL.exe carries the large-address flag
// (4 GB instead of 2 on a 64-bit Windows), a test that puts the game's memory above 2 GB from the
// start, and how much of the address space is in use.
//
// [Memory] HighMemoryTest=1 in thugsdl.ini, with the flag set, takes every free stretch of address
// space below 2 GB while the DLL loads, before the game reserves anything. It is reserved only, no
// memory behind it. Everything the game asks for after that lands above 2 GB, where code that takes
// an address for a signed number (or keeps a flag in its top bit) goes wrong at once, rather than
// only on the day a level is big enough to reach that far.
namespace Memory {
	// While the DLL loads: log the address space, and run the test if the ini asks for it.
	void AtStartup(const char* configFile);
	// Log how much of the address space is in use, and where the game's main block is once it has one.
	void LogStatus();
	// LogStatus once, the first time the game has its main block (call every frame).
	void LogOnceReady();
}
