#pragma once
#include <stddef.h>
#include <stdint.h>

// Room for a world made of many levels, past three of the game's fixed limits.
//
// [Memory] BigNodeNames=1 (the default) gives the game a bigger table from node names to node numbers
// (src/Flickit/nodenames.h). The game's has 16384 slots: more named nodes than that hang the load (New Jersey
// and Manhattan joined have 9725; one more story level fits, two do not), and nodes numbered past
// 32767 cannot be found by name.
//
// [Memory] BigLights=1 (the default) gives the game room for 2048 level lights instead of 256. When
// the game's 256 are taken, every further LevelLight node is left without one (New Jersey 85,
// Manhattan 141, Moscow 202, Slam City Jam 73).
//
// [Memory] BigFaceList=1 (the default) gives the game's collision room for 65536 faces in a box instead of
// 6000. Its list of the faces a box reaches is one static array that it fills without looking at its size;
// past 6000 it wrote face numbers over the component and object managers' pointers that follow it (the
// crashes while driving across the joined world). Past 65536 a box's faces are cut short, not written over
// anything.
//
// All three leave a level that fits the game's limits as the game has it: the same nodes are found by the
// same names, the same lights are made in the same order, the same faces are found in the same boxes.
namespace BigWorld {
	// While the DLL loads, before the game makes anything: put in what the ini does not switch off.
	void AtStartup(const char* configFile);
	// How many times the game has made the table of node names: once for each level load (and each test
	// of a park in the park editor). 0 while BigNodeNames is off.
	uint32_t NodeLoads();
	// News of the faces in a box, for the watchdog's file: a box past the game's 6000 (a new most), or faces
	// that did not fit. False when there is none.
	bool FaceListNews(char* text, size_t size);
}
