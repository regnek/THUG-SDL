#pragma once

// Flickit, the part that reaches into the running game.
//
// flickit.cpp only sees the controller. For grinds the way Skate does them that is not enough: the
// mod has to know whether the skater is in the air or on a rail, and three things in the game's rail
// code have to change:
//
//   - a rail is caught without the Grind button being down
//   - the grind is picked from the lean of the board (the right stick) instead of the direction buttons
//   - the board does not drift on a rail (optional)
//
// Everything in here is written against the one PC executable of THUG (THUG.exe / THUG-SDL.exe, the
// same code). Before anything is read or changed the code at every address used is compared with
// what is expected there. If one byte differs nothing is touched and Flickit works as it did
// without this file.

#include <stdint.h>
#include "flickit.h"

namespace FlickitGame {

	// Compares the game's code with what this file was written against. The result is remembered.
	bool Available();

	// Puts the three changes into the game's rail code. They do nothing until SetFrame switches
	// them on, frame by frame. Returns false and changes nothing if Available() is false.
	bool Install();
	bool Installed();

	struct Skater {
		int state;              // Flickit::SkaterState
		bool skating;           // on the board: not on foot, not driving
		int grind;              // Flickit::Grind the grind script reported, GRIND_UNKNOWN if it has not yet
		bool grindReady;        // that script takes a switch to another grind now
		uint32_t grindSerial;   // goes up whenever a grind starts
	};

	// What the first local skater is doing. False while there is none or the game is not Available().
	bool ReadSkater(Skater& out);

	struct Frame {
		bool catchRails;        // rails catch without the Grind button
		bool pickGrind;         // the tilt picks the grind
		bool noBalance;         // the board does not drift on a rail
		bool mirror;            // swap "toward the rail" and "away from it"
		bool classicGrind;      // the player holds the real Grind button: the game picks as it always did
		Flickit::Tilt tilt;     // the lean of the board right now
	};

	// Once per frame, before player one's controller is read: nothing is on for this frame
	void BeginFrame();
	// Flickit drives the first local skater on this frame
	void SetFrame(const Frame& frame);

	// The last grind the pick started, for the log. Returns false if there was none since the last call.
	struct Pick {
		int grind;              // what the tilt asked for
		int started;            // what was started (differs if the game has no script for it)
		bool railOnLeft;
		bool byName;            // started as a script by name instead of through the game's table
		Flickit::Tilt tilt;
	};
	bool TakePick(Pick& out);

	// Called by the mod's IsPS2 script function when the game's Grind script asks. That script asks
	// twice in the life of a grind: 15 frames in, and at the moment it starts taking switches.
	void OnGrindScript(void* script);
}
