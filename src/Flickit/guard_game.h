#pragma once
// The guard in the game (what it decides is in guard.h).
//
// The game's pointer to its object manager (0x0069B6C0) gets a write breakpoint of the processor's, in every
// thread of the game, once the game has made the manager. A write to it from anywhere but the game's own two
// places goes to the log and to flickit_watchdog.txt next to the exe: the instruction that wrote it, the
// registers, the code up to it, and where the calls on the stack return to. Then what the pointer was goes
// back, before the game reads it again.
//
// Once a frame the pointer is also looked at: a change the breakpoint did not see (a thread it is not on, the
// system writing there) is written down and put back the same way. Threads that start later get the breakpoint
// within a frame or two, and every ten seconds all threads are looked at again.
//
// [Flickit] Guard=0 in thugsdl.ini switches it off.
namespace GuardGame {
	// While the DLL loads: the setting
	void AtStartup(const char* configFile);

	// Once a frame (the first player's pad poll): the breakpoint once there is a manager, the notes of what it
	// caught, and the look at the pointer
	void Frame();

	// From DllMain, when a thread starts (only a flag is set there)
	void ThreadStarted();
}
