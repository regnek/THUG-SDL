#pragma once
// Flickit's cars in the game (the parts without the game are in drive.h).
//
// Any car of the level can be taken, a moving one too: walk or skate up to it and press Triangle (the
// game's Grind button, which is also the button it gets out of a car with). The car becomes the game's own
// drivable car (PlayerVehicle) with the model of the car taken, put where that car was, facing its way, and
// given its speed. Holding Triangle stops it and gets him out (the game's own); he stands beside the
// driver's door on his board, and the car stays there to be taken again (the last four he left are kept).
// A car of the level's traffic comes back at the start of its way once he is far from there.
//
// The steps are scripts of Flickit's own (scripts/flickit/cars.q, compiled into flickit_cars_qb.h), which
// the game reads in the first level; they ask the commands below for what only the code can do. It needs
// Flickit's hooks in the game (FlickitGame::Installed): the skater's pad update is where it looks.
#include <stdint.h>

namespace DriveGame {
	// The local skater on this frame of his pad, as FlickitGame reads him
	struct Skater {
		uint8_t* object;
		const float* pos;          // where he is (four floats)
		uint8_t* triangle;         // the game's Triangle button: one byte, down or not (cleared for a press that takes a car)
		bool free;                 // on the ground (or walking), not in a bail, the controls his
		bool driving;
	};

	// [Flickit] Cars (1 on), CarReach (how close to a car's body, in game units: 45 by default)
	void Configure(const char* configFile);

	// The script commands the car scripts use. Before the game makes its table of them (InitPatch).
	void AddCFuncs();

	// Once the game has read its own scripts (where THUG-SDL changes them, initScriptPatch): the car
	// scripts go to the game. Should that not have happened, they go in the first level.
	void AtScriptsLoaded();

	// Once a frame, after the game has read the local skater's pad
	void PadFrame(const Skater& s);
}
