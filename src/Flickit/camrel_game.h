#pragma once
#include <stdint.h>

// Camera-relative drawing of the level (camrel.h): the hooks in the game's mesh code.
namespace CamRelGame {
	// Reads [Render] CameraRelative (default 1) and CameraRelativeKey (default 64, F7; 0: no key) and
	// puts the hooks in when it is on and the game's code is what they expect.
	void AtStartup(const char* configFile);

	// The SDL scancode of the key that switches it on and off while the game runs (0: none).
	int ToggleKey();

	// On to off or back: every registered mesh goes to the card again as the new state wants it.
	void Toggle();

	// Where it stands, into the log.
	void LogStatus();

	// Whether the mesh draw is hooked (on or switched off), and how many meshes it has drawn since the start:
	// every draw of a mesh of the game's, registered or not. The watchdog's sign that the level is drawn at all.
	bool Counting();
	uint32_t DrawCount();

	// The level is drawn relative to its meshes' middles now (not off in the ini, not switched off by the key)
	bool On();
}
