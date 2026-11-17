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
// And for rolling the way Skate's board does, eight more: four in the physics of the ground, three
// commands of the game's scripts, and the place where the game takes in the controller.
//
//   - the skater does not kick by himself: a push comes from Flickit, on a button (the game's own
//     kick test; its switch for kicking unasked is held at "off" meanwhile). The push goes with
//     the game's push animation: which animation the skater is in and how far into it is read
//     from his animation component, and where his feet are from his skeleton. Both are only read.
//   - below walking pace he is not stopped dead, the board rolls out (the game's brake test)
//   - the board loses speed the way Skate's does (the game's friction on the ground)
//   - it turns by the stick the way Skate's does (the game's turning on the ground)
//   - the game's scripts are told when to play the push (the commands CanKick, where the loop
//     of a skater riding nollie asks it, and Held for the Flip button, which is the game's own
//     button for a kick)
//   - they play it in a turn as well: while a push asks for its animation they do not see
//     Left and Right (the game's picture of the pad, between the look at the controller and the
//     physics, which get both back)
//   - they play it after a landing, a skate-off from a rail and a bump into a wall without
//     waiting for that animation to end (the command AnimFinished, for those animations only)
//
// For spinning the way Skate's skater does, two more, in the physics of the air:
//
//   - which way the skater turns in the air and whether he does (the game's own turning, which
//     for that one call sees Left or Right the way Flickit's turning goes)
//   - how fast (the number the game asks its stats for there)
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

		// Rolling
		bool rolling;           // on the board, on the ground, nothing balanced, not bailing: a push button pushes
		float speed;            // the game's units per second
		uint32_t pushSerial;    // goes up when the foot of a push has pushed
		float pushHeld;         // that push: seconds the button had been down
		float pushAsked;        // the speed it was asked to add
		float pushAdded;        // the speed it added
		float pushFrom;         // the speed it started at
		float pushTo;           // and the speed when its foot had pushed
		const char* pushAnim;   // the game's animation it went with ("the clock" if none was seen)
		float pushLength;       // how long that is, in seconds
		float pushDown;         // the shares of it between which the foot pushed
		float pushUp;
		int pushSource;         // where those two came from: PushSource

		// The feet in a push animation, for the log
		uint32_t footSerial;    // goes up when a play of a push animation has told something new
		const char* footAnim;   // the animation
		float footLength;       // its length in seconds
		Flickit::FootResult foot;   // what its feet did
		const float* footTrace; // the first time an animation was watched whole: every look at its feet,
		int footTraceLooks;     // seven numbers each (Flickit::FootWatch::Looks), and how many. Else 0
		uint32_t blindSerial;   // goes up when the game was told to play a push again and again and no push animation was seen
		uint32_t blindAnim;     // the animation the skater was in instead (the checksum of its name)
		uint32_t cutSerial;     // goes up when a push has cut short an animation the game's scripts were waiting for,
		const char* cutAnim;    // which,
		float cutShare;         // and how far into it the skater was

		// Spinning
		uint32_t spinSerial;    // goes up when an air in which Flickit turned the skater is over
		float spinTurned;       // that air: radians Flickit asked for, to the right when positive
		float spinFlick;        // the flick of the stick that counted, 0 to 1
		float spinSeconds;      // how long the air lasted
	};

	// Where a push had it from at which point of its animation the foot pushes
	enum PushSource {
		PUSH_AS_SET = 0,        // the settings
		PUSH_MEASURED = 1,      // the feet were watched in that animation
		PUSH_BORROWED = 2       // they were watched in another push animation of the same kind
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
		int push;               // the push in percent of Skate's, 0: the game's own kick
		bool pushHeld;          // a push button is down, and the press is a push
		bool pushOverTurn;      // the game's scripts play a push in a turn as well
		bool pushMeasure;       // the feet are watched in the game's push animations
		float pushFootDown;     // where the foot pushes in an animation that has not been measured:
		float pushFootUp;       // from this share of it to this
		int coast;              // rolling resistance in percent of Skate's, 0: the game's own friction
		int steering;           // grip in a turn in percent of Skate's, 0: the game's own turning
		float steer;            // the movement stick, -1 (left) to 1 (right)
		int spin;               // turning in the air in percent of Skate's, 0: the game's own
		float seconds;          // since the look at the controller before this one
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
