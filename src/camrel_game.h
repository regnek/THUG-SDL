#pragma once

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
}
