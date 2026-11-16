#pragma once

// Flickit, the part that reaches into the running game.
//
// flickit.cpp only sees the controller. For grinds the way Skate does them that is not enough: the
// mod has to know whether the skater is in the air or on a rail, and five things in the game's rail
// code have to change:
//
//   - a rail is caught without the Grind button being down
//   - a rail that catches that way reaches for the board only as far as Skate's would
//   - the grind is picked from the lean of the board (the right stick) instead of the direction buttons
//   - the board does not drift on a rail (optional)
//   - the board loses speed on a rail, and the speed the game adds when it lands on one is left out
//
// For jumps the way Skate has them two more things change, outside the rail code:
//
//   - the gravity in the air
//   - the speed of a jump
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

	// Puts the changes into the game's code. They do nothing until SetFrame switches them on,
	// frame by frame. Returns false and changes nothing if Available() is false.
	bool Install();
	bool Installed();

	struct Skater {
		int state;              // Flickit::SkaterState
		bool skating;           // on the board: not on foot, not driving
		int grind;              // Flickit::Grind the grind script reported, GRIND_UNKNOWN if it has not yet
		bool grindReady;        // that script takes a switch to another grind now
		uint32_t grindSerial;   // goes up whenever a grind starts

		// On a rail, as of the last frame the game's rail physics ran with the drag or the step-off on
		bool railOff;           // the board is out of speed and should step off (Flickit::RailRun)
		int offSide;            // the side the board came onto the rail from, seen from the skater now: -1 left, 1 right, 0 not known
		float railSeconds;      // how long it has been on the rail
		float railSpeed;        // its speed, in the game's units per second
		float railDrag;         // the drag on that frame, in the game's units per second squared
		int railTerrain;        // the game's terrain number of the rail
		int railSurface;        // Flickit::RailSurface, what that number means for the drag
		int dragClass;          // Flickit::DragClass of the grind

		// Rails that caught the board by themselves, and rails that did not, for the log
		uint32_t catchSerial;   // goes up with every rail taken within Flickit's reach
		Flickit::RailPass caught;   // how the board passed that rail on the frame it was taken
		uint32_t missSerial;    // goes up when a flight ends on the ground with a rail just out of reach
		Flickit::RailPass missed;   // the rail that came closest in that flight
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
		int drag;               // percent of Skate's drag on a rail, 0 for none
		bool stepOff;           // a board that is out of speed on a rail steps off
		bool boost;             // with the drag on, the game still adds speed when the board lands on a rail
		int reach;              // percent of Skate's reach for rails that catch by themselves, 0: the game's own
		int gravity;            // gravity in the air in percent of Skate's, 0: the game's own
		int pop;                // the speed of a jump in percent of Skate's, 0: the game's own
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
