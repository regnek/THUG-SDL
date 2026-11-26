#pragma once
// Flickit's cars in the game (the parts without the game are in drive.h).
//
// Any car of the level can be taken, a moving one too: walk or skate up to it and press Circle (the button
// the game gets into its own drivable cars with: "Press Circle to drive", car_scripts.q). The car becomes the
// game's own drivable car (PlayerVehicle) with the model of the car taken, put where that car was, facing its
// way, and given its speed. Holding Triangle stops it and gets him out (the game's own); he stands beside the
// driver's door on his board, and the car stays there, on the ground under it, to be taken again. While a car is
// in reach the game's own "Press Circle to drive." box shows, unless another player is nearer (players_game.h):
// then Circle is his.
// A car of the level's traffic comes back at the start of its way once he is far from there.
//
// Crashes (crash.h): his car hits the cars of the traffic and the cars standing on their own. A car of the
// traffic hit hard enough is knocked off its way: it becomes a car standing on its own (a box of Flickit's own
// that slides, turns, rolls over and comes to rest against the level), to be taken like a car he left; ten such
// cars at most, his and the knocked ones together. A hard crash throws the car hit up and over. Each hit is
// damage: smoke, heavy smoke, fire, and a few seconds after it caught fire the car blows up, which throws it,
// burns it out and pushes and damages what is near. His own car too: he is thrown out of it if he is still in.
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
		uint8_t* take;             // the game's Circle button: one byte, down or not (cleared for a press that takes a car)
		bool free;                 // on the ground (or walking), not in a bail, the controls his
		bool driving;
		float otherAway;           // how far the other thing Circle is for is (another player: players_game.h), a large
		                           // number with none: a car only gets Circle when it is nearer
	};

	// [Flickit] Cars (1 on), CarReach (how close to a car's body, in game units: 45 by default), CarParkedTime
	// (seconds a car he left stands before it goes back: 120 by default, 0 for always), CarCrashes (1 on),
	// CarDamage and CarLaunch (percent, 100 by default: how much a hit hurts, how high a hard crash throws)
	void Configure(const char* configFile);

	// The script commands the car scripts use. Before the game makes its table of them (InitPatch).
	void AddCFuncs();

	// Once the game has read its own scripts (where THUG-SDL changes them, initScriptPatch): the car
	// scripts go to the game. Should that not have happened, they go in the first level.
	void AtScriptsLoaded();

	// Once a frame, from the pad data of the first player: Flickit put the game's Circle down for a grab of its
	// own (a trigger, with TriggerGrabs) and his own buttons did not. Such a press takes no car.
	void GrabMade(bool made);

	// Once a frame, after the game has read the local skater's pad
	void PadFrame(const Skater& s);
	// How far the nearest car in reach was on this frame (from its body), a large number with none
	float CarAway();
	// Whether the game's Circle on this frame's pad data is Flickit's own (a trigger grab), not his
	bool FlickitGrab();
}
