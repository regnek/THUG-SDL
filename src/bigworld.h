#pragma once

// Room for a world made of many levels, past two of the game's fixed limits.
//
// [Memory] BigNodeNames=1 (the default) gives the game a bigger table from node names to node numbers
// (src/nodenames.h). The game's has 16384 slots: more named nodes than that hang the load (New Jersey
// and Manhattan joined have 9725; one more story level fits, two do not), and nodes numbered past
// 32767 cannot be found by name.
//
// [Memory] BigLights=1 (the default) gives the game room for 2048 level lights instead of 256. When
// the game's 256 are taken, every further LevelLight node is left without one (New Jersey 85,
// Manhattan 141, Moscow 202, Slam City Jam 73).
//
// Both leave a level that fits the game's limits as the game has it: the same nodes are found by the
// same names, the same lights are made in the same order.
namespace BigWorld {
	// While the DLL loads, before the game makes anything: put in what the ini does not switch off.
	void AtStartup(const char* configFile);
}
