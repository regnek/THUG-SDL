#pragma once

// Flickit: Skate-style right stick controls for THUG.
//
// This module has no dependency on the game, on SDL or on Windows. It takes the
// right stick position and the triggers once per frame and answers with the
// classic Tony Hawk buttons that should be down on that frame. input.cpp reads
// the controller, calls Update() and merges the answer into the pad data the
// game sees.
//
//   pull the stick down          -> Ollie is held (crouch)
//   let it go / flick it away    -> Ollie is released (pop)
//   where the flick ends         -> direction + Flip is tapped (flip trick slot)
//   triggers                     -> direction + Grab (left / right hand)
//   after a pop                  -> Grind is held for a while (no grind button)

#include <stdint.h>

namespace Flickit {

	// Eight-way sectors, counter-clockwise starting at "right". +y is up.
	enum Dir {
		DIR_NONE = -1,
		DIR_R = 0,
		DIR_UR = 1,
		DIR_U = 2,
		DIR_UL = 3,
		DIR_L = 4,
		DIR_DL = 5,
		DIR_D = 6,
		DIR_DR = 7
	};

	enum Windup {
		WIND_OLLIE = 0,   // stick pulled down, flicked up
		WIND_NOLLIE = 1   // stick pushed up, flicked down
	};

	enum Gesture {
		GESTURE_NONE = 0,
		GESTURE_POP = 1,    // pop with no flip trick
		GESTURE_FLIP = 2,   // pop followed by a flip trick slot
		GESTURE_GRAB = 3    // trigger grab
	};

	struct Settings {
		int enabled;          // master switch
		int toggleButton;     // SDL_GameControllerButton that switches Flickit on/off in a level, -1 for none
		int reloadKey;        // SDL_Scancode that reloads this section while the game runs, 0 for none
		int debug;            // 1: log every recognized gesture

		int kickflipSide;     // 0: kickflip = flick up-right, heelflip = up-left. 1: mirrored
		int nollie;           // 1: stick up, then flick down = nollie
		int nollieTaps;       // presses of the Nollie button needed to get from regular into nollie stance
		int triggerGrabs;     // 1: left / right trigger = grab
		int autoGrind;        // 1: hold Grind after every pop

		// Stick thresholds, percent of full deflection
		int deadzone;         // below this the stick counts as centered
		int engage;           // a wind-up starts above this
		int release;          // a wind-up ends below this
		int flick;            // the end of a flick has to reach this
		int snap;             // a flick that reaches this in the opposite half is accepted at once

		// Timings, milliseconds
		int flickWindow;      // after the pop, the flick has this long to arrive
		int settle;           // a drag to the side has to rest this long to count
		int dwell;            // the stick has to rest this long in a wind-up sector to make it the start
		int afterPop;         // pause between releasing Ollie and the first trick input
		int dirLead;          // the direction goes down this long before the trick button
		int pulse;            // how long direction + trick button stay down
		int autoGrindDelay;   // Grind goes down this long after the pop
		int autoGrindHold;    // and stays down this long
	};

	void SetDefaults(Settings& s);
	void Sanitize(Settings& s);

	struct Input {
		uint32_t now;         // milliseconds, any epoch
		float rx;             // right stick, -1..1, + = right
		float ry;             // right stick, -1..1, + = up
		bool leftTrigger;
		bool rightTrigger;
	};

	struct Output {
		bool ollie;           // hold Ollie
		bool flip;            // hold Flip
		bool grab;            // hold Grab
		bool grind;           // hold Grind
		bool nollie;          // hold Nollie
		int dir;              // direction to hold on the d-pad, DIR_NONE for none
		bool ownsDirection;   // true: the player's own direction input must be dropped this frame

		// Filled on the frame a gesture is recognized, for logging
		int gesture;          // Gesture
		int windup;           // Windup
		int start;            // sector the stick was held in
		int end;              // sector the flick ended in
		int slot;             // direction of the trick slot that will be pressed
		uint32_t elapsed;     // ms between the pop and the decision
	};

	class Recognizer {
	public:
		Recognizer();
		void Reset();
		void Update(const Input& in, const Settings& cfg, Output& out);

		// True while something is still pending (a held wind-up, a queued press, a grind hold)
		bool Busy() const;

	private:
		enum Phase { PH_IDLE, PH_WINDUP, PH_TRACK };
		enum Button { BTN_FLIP, BTN_GRAB };
		enum { MAX_QUEUE = 4 };

		struct Pulse {
			int button;
			int dir;
			uint32_t notBefore;
			int stage;            // 0 waiting, 1 direction only, 2 direction + button
			uint32_t stageStart;
			int frames;
		};

		void BeginWindup(int wind, int sector, uint32_t now, const Settings& cfg);
		void Pop(uint32_t now, const Settings& cfg);
		void Finalize(int end, uint32_t now, const Settings& cfg, Output& out);
		void Enqueue(int button, int dir, uint32_t notBefore);
		uint32_t AfterPop(const Settings& cfg) const;
		void RunTaps(uint32_t now, Output& out);
		void RunQueue(const Input& in, const Settings& cfg, Output& out);

		int m_phase;
		int m_wind;
		bool m_armed;
		int m_start;
		int m_cur;
		uint32_t m_curSince;
		uint32_t m_popTime;
		bool m_hasPopped;
		int m_cand;
		uint32_t m_candSince;

		int m_tapsLeft;
		bool m_tapDown;
		uint32_t m_tapStageStart;
		int m_tapFrames;

		Pulse m_queue[MAX_QUEUE];
		int m_queued;

		bool m_grabHeld;
		bool m_prevLeftTrigger;
		bool m_prevRightTrigger;

		bool m_grindPending;
		uint32_t m_grindFrom;
		uint32_t m_grindUntil;

		// A trick press is being entered on this frame (direction down, button about to go down)
		bool m_entering;
		int m_enteringButton;

		// What was asked for on the previous frame
		bool m_lastOllie;
		bool m_lastGrind;
		bool m_lastNollie;
		bool m_lastGrab;
	};

	// Helpers, exposed for the tests and for logging
	int Sector(float x, float y);
	float AngleDeg(float x, float y);
	bool InWindupHalf(float angleDeg, int wind, float marginDeg);
	int MapFlip(int wind, int start, int end, int kickflipSide);
	const char* DirName(int dir);
	const char* SlotName(int slot);
}
