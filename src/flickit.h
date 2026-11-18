#pragma once

// Flickit: Skate-style right stick controls for THUG.
//
// This module has no dependency on the game, on SDL or on Windows. It takes the
// right stick position and the triggers once per frame and answers with the
// classic Tony Hawk buttons that should be down on that frame. input.cpp reads
// the controller, calls Update() and merges the answer into the pad data the
// game sees.
//
//   pull the stick down          -> Ollie is held (crouch); how far is how high the pop goes
//   flick it away                -> Ollie is released (pop)
//   let it come back to the centre without a flick -> Ollie is released and he stands up again
//   where the flick ends         -> direction + Flip is tapped (flip trick slot)
//   triggers                     -> direction + Grab (left / right hand)
//   after a pop                  -> Grind is held for a while (no grind button)
//
// When input.cpp can tell it what the skater is doing (flickit_game.cpp), grinds work the way
// Skate does them, and the timed Grind hold is not used:
//   in the air the stick leans the board, and the lean at touchdown is the grind (see Tilt)
//   on a rail the stick at rest switches between 50-50, 5-0 and nosegrind
//   on a rail only a flick pops; letting the stick go does not
//   in the air a flick across the stick is a flip, rolling it to a side is not a shove-it
//   on a rail the board loses speed, and steps off when the speed is gone (see RailRun)

#include <stdint.h>
#include <stddef.h>

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

	// The stance the stick asks for (Output::stance). Skate has no stance to switch to: the stick
	// pushed up puts the skater's weight on the nose, and back at the centre he rides as before.
	// THUG has nollie as a stance of its own, with its own animations (see StanceKeeper).
	enum Stance {
		STANCE_ANY = 0,       // the stick asks for nothing: whatever the game has
		STANCE_REGULAR = 1,   // out of nollie
		STANCE_NOLLIE = 2     // into nollie
	};

	struct Settings {
		int enabled;          // master switch
		int toggleButton;     // SDL_GameControllerButton that switches Flickit on/off in a level, -1 for none
		int reloadKey;        // SDL_Scancode that reloads this section while the game runs, 0 for none
		int debug;            // 1: log every recognized gesture

		int kickflipSide;     // 0: kickflip = flick up-right, heelflip = up-left (with flicksByFeet: with the right foot forward). 1: mirrored
		int flicksByFeet;     // 1: the flicks go by the foot he has forward (Input::leftFootForward), as in Skate
		int nollie;           // 1: stick up = nollie stance, a flick down from there = nollie (and the nollie flips)
		int nollieTaps;       // without the game hooks: presses of the Nollie button that get from regular into nollie stance
		int triggerGrabs;     // 1: left / right trigger = grab
		int autoGrind;        // 1: hold Grind after every pop (where the game does not catch rails by itself)

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

		// Stick patterns (see PatternSet)
		int patterns;         // 1 = decide tricks from the pattern file when one is loaded
		int missTicks;        // 60 Hz ticks the stick may spend between two points of a pattern

		// Grinding the Skate way. These need the game hooks (flickit_game.cpp); without them the
		// timed Grind hold above is used.
		int gameHooks;        // 1: the mod may read the skater's state and hook the game's rail code
		int grindCatch;       // 1: rails catch without the Grind button
		int grindPick;        // 1: the stick position at touchdown picks the grind
		int grindSwitch;      // 1: on a rail, stick down / up / centre = 5-0 / nosegrind / 50-50
		int grindBalance;     // 1: keep the game's balance on rails, 0: the board does not drift
		int grindMirror;      // 1: swap "toward the rail" and "away from it" when picking the grind
		int pauseOffBoard;    // 1: Flickit stands down while walking or driving
		int tilt;             // percent of deflection that counts as tilting the board
		int tiltSettle;       // ms after take-off before a stick that never re-centred counts as a tilt
		int switchHold;       // ms the stick has to stay put on a rail before the grind switches
		int railTap;          // ms Ollie is pressed for the pop out of a grind
		int flickSpeed;       // on a rail and in the air, where a slow move of the stick is something else:
		                      // how fast it has to travel to be a flick, percent of full deflection per 100 ms
		int grindDrag;        // percent of Skate's drag on a rail, 0: none, as THUG has it
		int grindExit;        // 1: the board steps off a rail when it has run out of speed
		int grindBoost;       // 1: keep the game's speed boost for landing on a rail while the drag is on
		int grindReach;       // percent of Skate's reach for a rail that catches by itself, 0: THUG's own
		int airGravity;       // gravity in the air, in percent of Skate's, 0: THUG's own
		int jumpPop;          // the speed of a jump, in percent of Skate's, 0: THUG's own
		int crouchPose;       // 1: the crouch the skater shows goes as deep as the stick is pulled, 0: THUG's one crouch

		// Rolling the Skate way (see Rolling below). These need the game hooks too.
		int push;             // percent of Skate's push: what one push adds and how fast pushing gets. 0: THUG's own kick
		int pushOnOllie;      // 1: the Ollie button pushes as well while the board rolls on the ground
		int pushOverTurn;     // 1: the game shows a push in a turn too; 0: its turn animation comes first, as in THUG
		int pushMeasure;      // 1: the feet are watched in the game's push animations, and a push goes by what its foot does
		int pushFootDown;     // where in a push animation the foot starts to push, in percent of its length, ...
		int pushFootUp;       // ... and where it has pushed: for an animation the feet have not been watched in
		                      // (as watched in the game's: 7 to 12, and 31 to 34)
		int coast;            // percent of Skate's rolling resistance, 0: THUG's own friction
		int steering;         // percent of Skate's grip in a turn, 0: THUG's own turning

		// Spinning in the air the Skate way (see Spinning in the air below). Needs the game hooks.
		int spin;             // percent of Skate's turning in the air, 0: THUG's own
	};

	void SetDefaults(Settings& s);
	void Sanitize(Settings& s);

	// What the skater is doing, when the mod can see it. STATE_UNKNOWN keeps the behaviour of the
	// versions that only saw the controller.
	enum SkaterState {
		STATE_UNKNOWN = -1,
		STATE_GROUND = 0,
		STATE_AIR = 1,
		STATE_WALL = 2,
		STATE_LIP = 3,
		STATE_RAIL = 4,
		STATE_OTHER = 5
	};

	// Grinds the board tilt can ask for, and the ones the stick switches between on a rail
	enum Grind {
		GRIND_UNKNOWN = -1,
		GRIND_FIFTY = 0,      // nothing asked for: 50-50, or a board / lipslide when the board lands across the rail
		GRIND_FIVE_O,
		GRIND_NOSEGRIND,
		GRIND_CROOKED,
		GRIND_OVERCROOK,
		GRIND_FEEBLE,
		GRIND_SMITH,
		GRIND_SALAD,
		GRIND_BOARDLIP,       // boardslide or lipslide, whichever fits the way the skater faces
		GRIND_TAILSLIDE,
		GRIND_NOSESLIDE,
		GRIND_BLUNTSLIDE,
		GRIND_NOSEBLUNT,
		GRIND_OTHER           // reported by the game: a grind the stick does not switch away from
	};

	struct Input {
		uint32_t now;         // milliseconds, any epoch
		float rx;             // right stick, -1..1, + = right
		float ry;             // right stick, -1..1, + = up
		bool leftTrigger;
		bool rightTrigger;

		// From the game, when it can be seen. The defaults are "nothing known".
		int state;            // SkaterState
		bool railCatch;       // the game catches rails by itself: no Grind hold from here
		int grind;            // Grind the skater is in (GRIND_FIFTY, GRIND_FIVE_O, GRIND_NOSEGRIND, GRIND_OTHER or GRIND_UNKNOWN)
		bool grindReady;      // the game takes a switch to another grind now
		uint32_t grindSerial; // changes whenever the game starts a grind
		bool railOff;         // the board has run out of speed on this rail: time to step off
		int offSide;          // the side to step off to: -1 left, 1 right, 0 wherever the game finds room
		// The flip trick the skater is in can be taken round once more now: the game has one more
		// rotation of it on the trick button, there is time to press for it, and air enough to land
		// it. flipLevel tells one such chance from the next (it changes with every trick the game starts).
		bool flipMore;
		uint32_t flipLevel;
		// The game takes Output::stance and keeps the skater in it (StanceKeeper). Without that the
		// Nollie button is tapped (Settings::nollieTaps) as a crouch up begins.
		bool keepsStance;
		// His left foot is forward (the game's Flipped): a regular skater in his own stance, or a goofy
		// one riding switch. Skate's flicks go by the feet, the kickflip toward the rider's heels: with
		// Settings::flicksByFeet they are mirrored left / right then. False where it is not known,
		// and then KickflipSide alone decides.
		bool leftFootForward;

		Input() : now(0), rx(0.0f), ry(0.0f), leftTrigger(false), rightTrigger(false),
			state(STATE_UNKNOWN), railCatch(false), grind(GRIND_UNKNOWN), grindReady(false), grindSerial(0),
			railOff(false), offSide(0), flipMore(false), flipLevel(0), keepsStance(false), leftFootForward(false) {}
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

		// Set when a stick pattern decided the trick
		const char* pattern;  // its name, 0 otherwise
		float speed;          // 0..1: how fast the pattern was drawn
		int ticks;            // 60 Hz ticks it took

		int state;            // SkaterState the frame was made in
		int switchTo;         // Grind a switch on the rail was started for on this frame, GRIND_UNKNOWN otherwise

		// Stepping off a rail that the board has stopped on. THUG has this on a button (R2 on its
		// PlayStation pad: the skater steps down next to the rail, or hops off it where there is no
		// ground to step on). The direction held with it picks the side.
		bool stepOff;         // hold that button
		int stepSide;         // with Left (-1) or Right (1), 0 for neither
		bool stepStarted;     // set on the frame the press starts, for logging

		// The crouch for an ollie (on the ground)
		float popStrength;    // on the frame of a pop out of a crouch: how far the stick was pulled, 0 (a quick pop) to 1; -1 otherwise
		bool standUp;         // on the frame a crouch ends without a pop: the skater stands back up
		float crouchPose;     // while the stick crouches on the ground: how deep the crouch is to show, 0 (only
		                      // just) to 1 (all the way), as the pop would go; -1: the stick has no crouch
		// The stick held at the end of a flip trick's flick
		bool holding;         // it is held there
		bool flipAgain;       // on the frame the trick button is pressed once more for another rotation
		// With Settings::nollie: the stance the stick asks for on this frame (Stance). Nollie while it
		// crouches up, regular while it crouches down and on the frame a crouch up ends without a
		// pop; STANCE_ANY otherwise, a pop included (the game's jump from nollie is a nollie).
		int stance;
		// The flicks are read mirrored left / right on this frame (KickflipSide, and the left foot
		// forward): the kickflip is the flick up-left. A gesture keeps the side it began on: it
		// changes between gestures only.
		bool mirrored;
	};

	// ------------------------------------------------------------------------------------------
	// Board tilt
	//
	// In the air the stick does not press anything: it leans the board, and the lean the board has
	// when it touches a rail is the grind. As in Skate, two things count: where the stick was pushed
	// first (up = nose, down = tail, or to a side) and where it is at touchdown.
	//
	//   centre                      50-50
	//   up / down                   nosegrind / 5-0
	//   up + toward the rail        crooked          up + away          overcrook
	//   down + away                 salad            (down + toward stays a 5-0)
	//   to a side                   boardslide or lipslide
	//   up, rolled toward           noseslide        up, rolled away    nosebluntslide
	//   down, rolled toward         tailslide        down, rolled away  bluntslide
	//   toward, rolled up           feeble           away, rolled up    smith
	//   toward, rolled down         crooked          away, rolled down  overcrook   (Skate: willy / overwilly)
	// ------------------------------------------------------------------------------------------

	enum TiltClass { TILT_NONE = 0, TILT_UP = 1, TILT_DOWN = 2, TILT_SIDE = 3 };

	struct Tilt {
		bool valid;           // the stick is held away from the centre, and not as the tail end of a flick
		int first;            // TiltClass of the first push
		bool firstLeft;       // the first push went to the left
		int cur;              // Dir the stick is in now
	};

	// The grind a tilt asks for. railOnLeft: the rail is to the left of the direction of travel.
	int GrindFromTilt(const Tilt& t, bool railOnLeft);
	const char* GrindName(int grind);
	const char* StateName(int state);

	// How to start a grind in THUG. The game picks its grinds from a table (GrindTrickList in
	// grindlist.q): one row for each direction the player could hold with the Grind button, and in
	// each row sixteen entries for the side the rail is on, whether the board lands along the rail,
	// whether the skater rides backwards and his stance. A grind the tilt asks for is started by
	// handing the game the row it is in. Three grinds are in no row and are started as scripts by name.
	struct GrindPlan {
		int row;              // row of GrindTrickList, -1: start the script named by prefix instead
		int parallel;         // 1 / 0: tell the game the board lands along / across the rail, -1: as it is
		const char* prefix;   // script name without its ending, for row == -1
		int fallback;         // the Grind to start instead if the game has no such script
	};
	void PlanGrind(int grind, bool railOnLeft, GrindPlan& plan);
	// The ending the game gives a grind script's name (Template1 in grindlist.q)
	const char* GrindSuffix(bool railOnLeft, bool backwards, bool flipped);
	// The game's checksum of a name: CRC-32 of the lower-cased text, not inverted at the end
	uint32_t NameChecksum(const char* name);

	class TiltTracker {
	public:
		TiltTracker();
		void Reset();
		// Once per frame, in every state. A tilt is reported only while airborne.
		void Update(uint32_t now, float x, float y, bool airborne, const Settings& cfg);
		// A flick is under way. markNow: where the stick is right now belongs to the flick, it is
		// no tilt unless it is held. expectEnd: the stick is still on its way to the end of the flick.
		void Flick(uint32_t now, bool markNow, bool expectEnd);
		const Tilt& Get() const { return m_tilt; }

	private:
		Tilt m_tilt;
		bool m_airborne;
		bool m_deflected;
		bool m_clean;         // pushed out from the centre in the air, not left over from a flick or from the ground
		bool m_latched;       // the first push has been read
		uint32_t m_since;
		bool m_expectEnd;
		uint32_t m_flickAt;
	};

	// ------------------------------------------------------------------------------------------
	// Losing speed on a rail
	//
	// THUG has no friction on a rail: a grind on a level rail never ends. In Skate it does. There a
	// drag works against the slide for as long as the board is on the rail:
	//
	//   - it is a force, the same at any speed (it takes off so much speed per second)
	//   - it depends on the grind: least on the trucks (50-50, 5-0, nosegrind), more in slides, most
	//     in the grinds that put a truck and the deck on the rail (crooked, smith and their kin)
	//   - it depends on what is ground: a metal rail slides far, a ledge takes about twice the speed,
	//     a rough one more
	//   - it starts at about two thirds and grows to its full strength over the first four seconds
	//   - on a slope it shrinks with the slope (the pull of gravity is the game's own business)
	//
	// When the speed is gone the board does not sit there: after at least a second on the rail, with
	// almost no speed left (or once the slope has turned it around), it steps off.
	//
	// The numbers are Skate 3's, measured there in metres and seconds. THUG's skater is quicker than
	// Skate's by the factor kUnitsPerMetre (Skate's best push is 8 m/s, THUG's about 600 to 675 of
	// its units a second), so that is the scale used: the drag takes the same share of the speed per
	// second in both games.
	// ------------------------------------------------------------------------------------------

	// What a rail is made of, as far as the drag cares
	enum RailSurface {
		SURFACE_RAIL = 0,     // metal, wire, plastic, glass: slides far
		SURFACE_LEDGE = 1,    // concrete, wood and whatever else
		SURFACE_ROUGH = 2     // rock, brick, logs, rope
	};

	enum DragClass {
		DRAG_TRUCKS = 0,      // 50-50, 5-0, nosegrind
		DRAG_ANGLED = 1,      // crooked, overcrook, smith, feeble, salad and the specials
		DRAG_TIPSLIDE = 2,    // nose-, tail-, blunt- and nosebluntslide
		DRAG_BOARDSLIDE = 3   // boardslide, lipslide
	};

	// THUG gives every rail a terrain number (TERRAIN_GRINDMETAL is 45, TERRAIN_GRINDCONC 42, ...),
	// which it uses for the sound and the sparks.
	int SurfaceOfTerrain(int terrain);
	const char* SurfaceName(int surface);
	// grind: what is known of the grind (a Grind, GRIND_OTHER or GRIND_UNKNOWN). slide: the game
	// balances it as a slide, not as a grind.
	int DragClassOf(int grind, bool slide);
	const char* DragClassName(int dragClass);
	// The share of the full drag after so many seconds on the rail
	float DragRamp(float seconds);
	// Skate's drag on a level rail, in m/s^2
	float SkateDrag(int dragClass, int surface, float seconds);

	extern const float kUnitsPerMetre;   // THUG's speed units for one of Skate's metres
	extern const float kStopSpeed;       // m/s: below this the board is out of speed
	extern const float kStopAfter;       // s: a board that has not been on the rail this long stays on it
	extern const float kCreepSpeed;      // m/s: the drag never takes the speed below this

	// One stay on a rail
	class RailRun {
	public:
		RailRun();
		// The board has just touched a rail
		void Begin();
		// One frame of the game's rail physics. dt: its length in seconds. speed: the speed along
		// the rail, in the game's units per second. level: how level the rail is where the board is,
		// 1 for level, 0 for vertical. turned: the board moves the other way than on the frame
		// before. dragPercent: Settings::grindDrag. mayStepOff: Settings::grindExit.
		// Returns the speed the board goes on with.
		float Step(float dt, float speed, float level, int dragClass, int surface, bool turned, int dragPercent, bool mayStepOff);
		// The board is out of speed: it should step off now
		bool WantsOff() const { return m_wantsOff; }
		float Seconds() const { return m_seconds; }
		// For the log: the drag of the last frame in the game's units per second squared, and the speed after it
		float Drag() const { return m_drag; }
		float Speed() const { return m_speed; }

	private:
		float m_seconds;
		float m_drag;
		float m_speed;
		bool m_wantsOff;
	};

	// Off the rail when the speed is gone. Skate pushes a board that has come to rest on a rail
	// sideways off it: toward the open side when there is a surface next to the rail on one side only,
	// as on a ledge, so that it drops off the ledge and not onto it; otherwise toward the side the
	// board sits on. The push is a force on the board that lasts until the board moves sideways at a
	// set speed, and it depends on the grind: 100 N up to 1 m/s on the trucks (120 N up to 0.9 m/s in
	// a 5-0, and only 10 N with a surface right next to the rail on both sides), 100 N up to 0.75 m/s
	// with truck and deck on the rail, 100 N up to 1.5 m/s in a nose or tail slide, 90 N up to
	// 0.75 m/s across the rail. THUG's own way off a rail lifts the skater kExitStep units and puts
	// him as far to the side, which is about as far as Skate's push takes the board before it is off.
	// So he leaves with the speed that push gives the board over that distance, or its set speed if
	// that comes first: 80 units/s on the trucks.
	enum RailExitWhy {
		EXIT_OPEN_SIDE = 0,   // a surface on the other side (a ledge): away from it
		EXIT_CAME_FROM = 1,   // no surface on either side: the side the board came from
		EXIT_BOTH_SIDES = 2,  // a surface on both: the side the board came from, and a gentle push
		EXIT_NO_SIDE = 3      // nothing to go by: the side THUG takes by itself
	};
	struct RailExitPlan {
		int side;             // +1 or -1: which of the two sides of the rail
		int why;              // RailExitWhy
		float speed;          // units/s sideways as he leaves the rail
	};
	extern const float kExitStep;  // units: how far THUG's way off a rail lifts the skater and puts him to the side
	// surfaceA / surfaceB: there is a surface next to the rail, at its height, on side +1 / -1.
	// cameFrom: the side the board came onto the rail from (+1, -1, 0 when not known). fallback: the
	// side to take when nothing says (+1 or -1). fiveO: the grind is a 5-0.
	void PlanRailExit(bool surfaceA, bool surfaceB, int cameFrom, int fallback, int dragClass, bool fiveO, RailExitPlan& plan);
	const char* RailExitWhyText(int why);

	// ------------------------------------------------------------------------------------------
	// How far a rail reaches for the board
	//
	// THUG takes a rail when the path of the skater's one point comes within 40 units of it while
	// he travels along it (20 when he crosses it), measured in any direction, from above, below or
	// beside, and it puts the board on the rail in that same frame. That is a metre for a skater
	// 77 units tall. With a Grind button to time that is a leap of faith the player asks for. With
	// rails that catch by themselves it is a magnet.
	//
	// Skate has no such thing. In the air its board follows the animation, and a helper looks eight
	// ticks ahead along the board's flight for a part of the board (a truck, the nose or tail, the
	// middle) that is about to pass a rail at less than 0.17 to 0.24 m. Then it slides the board
	// toward the rail by at most 3 cm a tick and 20 cm in all. So the board has to be coming down
	// on the rail already, and the help is a hand's width.
	//
	// THUG's skater has no nose and no trucks, only that one point in the middle of his board. So
	// the test here is the one Skate has for the middle of the board, which holds whichever way
	// the board is turned, at the scale the drag uses (kUnitsPerMetre: both skaters get the same
	// time to hit a rail):
	//
	//   - level, around the rail: 0.243 m, about 19 units
	//   - how far the board may still be above the rail: 0.2 m, 16 units (all the helper ever
	//     moves a board), and below it: half that
	//   - beyond the end of a rail: 0.2 m, 16 units
	//   - a rail the board has passed and is moving away from is let go at 8 units
	//
	// The three distances are the half axes of one rounded shape, not of a box: a board that is
	// both beside a rail and above it has to be nearer on both counts. With that no point of the
	// shape is as far as 20 units from the rail, so at 100 percent the reach of a rail lies wholly
	// inside the reach THUG gives that rail, whichever way the board comes.
	//
	// All of it is measured from the path the board took in this frame, so a board that falls
	// through the height of a rail between two frames is not missed. Among several rails the one
	// that lies best under the board is taken, not the one that runs most nearly the way the board
	// is going, which is what THUG prefers.
	// ------------------------------------------------------------------------------------------

	extern const int kReachMostPercent;   // the most Settings::grindReach can be
	extern const float kReachLevel;    // units, at 100 percent
	extern const float kReachDown;     // the rail is this far under the board
	extern const float kReachUp;       // the rail is this far over it
	extern const float kReachEnd;
	extern const float kReachBehind;

	// How the board passes one piece of rail in one frame
	struct RailPass {
		float miss;       // 1 and less: within reach. The smaller, the better the rail lies under the board
		float side;       // units: how far beside the rail the board is, level and across the rail
		float lift;       // units: more than 0: the rail is above the board, less: below it
		float past;       // units: how far the board is beyond an end of the rail
		bool away;        // the board has passed the rail and is leaving it: out of reach, however near
		float point[3];   // where on the rail the board would go
	};

	// a, b: where the board was a frame ago and where it is now (x, y, z; y is up). from, to: the
	// two ends of the piece of rail; to == 0: a rail that is one point. percent:
	// Settings::grindReach, more than 0.
	RailPass PassRail(const float* a, const float* b, const float* from, const float* to, int percent);

	// The game's rail nodes, as far as the search needs them: one after the other in memory,
	// "stride" bytes apart, with these things at these offsets.
	struct RailNodes {
		const unsigned char* first;
		int count;
		int stride;
		int boxMin, boxMax;        // three floats each: the box around the piece of rail that starts here
		int position;              // three floats
		int next, previous;        // pointers to nodes; a node without a next one ends a rail
		int flags;                 // 32 bits
		unsigned int needs;        // flags a node must have to be looked at
		unsigned int skips;        // flags that rule it out
	};
	// The game's own extra test of a piece of rail (it has one while a park from the editor runs)
	typedef bool (*RailTest)(const float* from, const float* to);

	// Looks for the piece of rail that lies best under the board. Returns its first node and
	// fills "pass", or returns 0 when no rail is within reach; then "pass" has the rail that came
	// closest (its miss is above 1), or a miss of kNoRail when there was none anywhere near.
	// ignore: a node not to take. minCos: 1, or the least cosine between the level directions of
	// travel and rail (the game asks for that in parks from the editor). Like the game's own
	// search it finds nothing for a board that has not moved, and passes over pieces of rail
	// that have no length.
	const unsigned char* FindRail(const RailNodes& nodes, const float* a, const float* b,
		const unsigned char* ignore, float minCos, int percent, RailTest test, RailPass& pass);
	extern const float kNoRail;

	// ------------------------------------------------------------------------------------------
	// Jumps
	//
	// THUG: a jump adds 350 units/s upward for a tap of the Ollie button and up to 414 to 450 (by
	// the skater's ollie stat) when the button was held for 0.2 s. Gravity in the air is 1350
	// units/s2. On flat ground that is 45 to 75 units up, 0.6 to 1 times the skater's height, in
	// 0.52 to 0.67 s. For his size he jumps two and a half times as high as Skate's rider and is
	// back down sooner: gravity is about three times real.
	//
	// Skate: real gravity, 9.8 m/s2. The pop is the speed that lifts the rider 0.38 m (a quick
	// pop) to 0.76 m (a full one): 2.7 to 3.9 m/s, 0.56 to 0.79 s in the air.
	//
	// At the scale the drag and the reach use (kUnitsPerMetre) Skate's gravity is 784 units/s2,
	// 58 percent of THUG's, and its full pop is 309 units/s, 69 percent of THUG's best. With both,
	// every jump is four fifths as high as it was and lasts a fifth longer. What leaves a ramp
	// flies 1.7 times as high and as long, because the speed off a ramp is not the pop's.
	// ------------------------------------------------------------------------------------------

	extern const float kThugAirGravity;   // units/s2
	extern const float kThugPopTap;       // units/s: the Ollie button tapped
	extern const float kThugPopWorst;     // held for 0.2 s, with the worst ollie stat
	extern const float kThugPopBest;      // and with the best
	extern const int kJumpMostPercent;    // the most Settings::airGravity and jumpPop can be

	// What THUG's gravity in the air is multiplied by. percent: Settings::airGravity, of Skate's
	// gravity; 0 and less: 1, THUG's own.
	float AirGravityFactor(int percent);
	// What the speed of a jump is multiplied by. percent: Settings::jumpPop, of Skate's full pop
	// against THUG's best; 0 and less: 1.
	float JumpPopFactor(int percent);

	// A jump off flat ground with one of THUG's pops (kThugPopTap ...): how high it goes, in
	// units, and how long it lasts, in seconds
	// The share of Skate's full pop that a pop out of a crouch this deep gets (0: a quick pop, 1: a
	// full one). Skate lifts the rider half as high for a quick pop as for a full one.
	float PopDepthShare(float strength);

	struct FlatJump {
		float height;
		float seconds;
	};
	FlatJump JumpOnFlat(float thugPop, int gravityPercent, int popPercent);

	// ------------------------------------------------------------------------------------------
	// Rolling: pushing, coasting, steering
	//
	// THUG's skater kicks by himself whenever he is slower than his kick speed (394 to 496 units/s
	// standing, 532 to 675 crouched, by his speed stat), at 629 to 1200 units/s2. What stops him
	// going faster is a drag that grows with the square of the speed (294 units/s2 at 700, standing).
	// He turns at 1.8 radians a second whenever left or right is held: all or nothing, at any speed.
	//
	// Skate:
	//
	//   - The board is pushed on a button. How long the button was down when the foot reaches the
	//     ground says how much speed the push is asked to add: 0.75 m/s for a tap, 4 m/s from a
	//     quarter of a second on. While the foot is on the ground the board is taken up to that
	//     speed, by no more than 0.6 m/s in one tick near a standstill and 0.5 at full speed, and
	//     never past 8.5 m/s. From 8.3 m/s on no push starts. A button that stays down pushes again
	//     and again, each time with all it has. There is no clock in it and no easing curve: the
	//     speed the board is taken up to is the speed the push animation itself moves at, and the
	//     animation says when the foot is on the ground. So the board speeds up as the foot pushes.
	//     Here the same is done with the game's own push animation: one push is one play of it,
	//     its foot is watched, and the speed comes in as the foot travels back along the ground.
	//   - On its four wheels the board loses 0.13 m/s2 at a crawl and 0.5 m/s2 from 8 m/s on, and
	//     more above 8.1 m/s (1.1 m/s2 at 13 m/s). From 8 m/s it rolls for half a minute.
	//   - The stick turns the trucks, by up to 0.5 radians at a walk and less the faster the board
	//     is, and gently near its centre: half the stick is a fifth of the angle. The board follows
	//     the trucks as far as the wheels grip, 0.8 g sideways. So at a walk it turns on the spot,
	//     and at full speed the circle it can hold is nine metres in radius. While a foot is on
	//     the ground the trucks turn less and less, down to half.
	//
	// All of it is put into THUG's units at kUnitsPerMetre, as the rails and the jumps are: Skate's
	// 8.5 m/s is 680 units/s, about the best THUG's own kick does.
	// ------------------------------------------------------------------------------------------

	extern const float kPushTopSpeed;     // m/s: pushing never takes the board past this
	extern const float kPushLastSpeed;    // m/s: from this speed on no push starts
	extern const float kPushTap;          // m/s a tap of the button adds
	extern const float kPushMost;         // m/s the strongest push adds
	extern const float kPushFullHold;     // s the button is down for the strongest push
	extern const float kThugGroundTurn;   // rad/s: THUG's own turning (Physics_Ground_Rotation)
	extern const float kTurnGrip;         // what the wheels hold sideways, in g
	extern const int kRollMostPercent;    // the most Settings::push and steering can be

	// The speed a push is asked to add, in m/s. heldSeconds: how long the button has been down
	// when the foot reaches the ground. speed: the board's, in m/s.
	float PushStrength(float heldSeconds, float speed);
	// The most a push adds in one of Skate's ticks (a 60th of a second), in m/s, at a speed in m/s
	float PushStep(float speed);
	// What the board loses while it rolls on its four wheels, in m/s2, at a speed in m/s
	float CoastLoss(float speed);
	// One frame of rolling: the speed after it. speed in the game's units per second, dt in
	// seconds, percent: Settings::coast.
	float Coast(float speed, float dt, int percent);

	// Where in a push animation the foot pushes, and how
	enum { PUSH_KNOTS = 17 };
	struct PushProfile {
		float down;                 // the share of the animation (0 to 1) at which the foot starts to push
		float up;                   // the share at which it has pushed
		float done[PUSH_KNOTS];     // how much of its stroke the foot has behind it, at even steps from down
		                            // to up: from 0 to 1, never falling
	};
	// A foot that pushes from one share of the animation to another, gently at both ends
	void SmoothPushProfile(PushProfile& out, float down, float up);
	// How much of its stroke the foot has behind it at a share of the animation: 0 before it pushes, 1 after
	float PushDone(const PushProfile& profile, float share);

	// What the game's animation of the skater is doing on one frame, as far as a push cares
	struct PushAnim {
		bool current;     // a push animation is the skater's animation, ...
		bool running;     // ... and it has not reached its end
		uint32_t play;    // changes whenever the game starts an animation
		float time;       // seconds into it
		float length;     // its length in seconds
	};

	enum PushPhase {
		PUSH_NONE = 0,
		PUSH_ASKED = 1,     // a push is wanted: the game is asked to play its push animation
		PUSH_INTO = 2,      // the animation runs, the foot is on its way down
		PUSH_CONTACT = 3,   // the foot pushes
		PUSH_RECOVER = 4    // it has pushed: the rest of the animation
	};

	// What a push animation the game shows gives way to. Skate's order on the ground puts the
	// wind-up for an ollie or a nollie (the crouch) before the push, and lets it break into a
	// push once the push is past its start, where the foot goes down and pushes; the push comes
	// before the brake and the turns. Down (THUG's brake: THUG never kicks with Down held, to one
	// side or not) has already taken the push's speed away (the physics see to that), so the
	// animation goes at once.
	enum GiveWay {
		GIVE_NONE = 0,
		GIVE_CROUCH = 1,
		GIVE_DOWN = 2
	};
	// stroking: PushRun::Stroking. crouched: the skater crouches. down: Down is held.
	int PushGivesWay(bool stroking, bool crouched, bool down);

	// The pushes of one skater. Step is called once on every frame of the game's ground physics.
	//
	// One push is one play of the game's push animation, and the animation is its clock. The game
	// plays it, and its scripts decide when: a push that is wanted asks them (Animating), and
	// becomes a push when a push animation is seen to run, one the game has started since or one
	// that was running already with its foot still in the air. While the foot pushes the board is
	// taken up to the speed the push is for, by as much as the foot has travelled: its stroke
	// half done, half the speed is there. How strong the push is, is how long the button was
	// down: until it was let go, or until the foot has pushed. A push the game shows no animation
	// for does not happen: a button that stays down keeps asking, a tap gives up after three
	// quarters of a second.
	class PushRun {
	public:
		PushRun();
		// Nothing is under way, and no press is remembered
		void Reset();
		// dt: seconds. speed: the board's, in the game's units per second. held: a push button
		// is down. percent: Settings::push, more than 0. anim: the skater's animation on this
		// frame. profile: where the foot pushes in that animation, if it is a push animation.
		// Returns the speed to add on this frame.
		float Step(float dt, float speed, bool held, int percent, const PushAnim& anim, const PushProfile& profile);
		int Phase() const { return m_phase; }
		// From the moment a push is wanted until its foot has pushed: the board is being pushed
		bool Active() const;
		// The push the game shows has its foot on the way down or pushing
		bool Stroking() const { return m_phase == PUSH_INTO || m_phase == PUSH_CONTACT; }
		// The game should start its push animation now
		bool Animating() const;
		// A push animation was the skater's animation on the last frame, running or at its end
		bool AnimCurrent() const { return m_animCurrent; }
		// The play of the animation the push goes with (PushAnim::play), from PUSH_INTO on
		uint32_t Play() const { return m_play; }
		// Once a frame, before the game's scripts run. free: they are in the loop that plays
		// the push (not busy with a landing, say). Whether they should not see Left and Right
		// on this frame, so that they get as far as the push: for a fifth of a second of
		// their being free, then not for the next three fifths, and so on.
		bool HideTurn(bool free, float dt);
		// For the log. The serial goes up when a foot has pushed.
		uint32_t Serial() const { return m_serial; }
		float HeldFor() const { return m_pushHold; }   // seconds the button was down for it
		float Asked() const { return m_asked; }        // units/s the push was asked to add
		float Added() const { return m_added; }        // units/s it did add
		float From() const { return m_from; }          // the speed it started at
		float To() const { return m_to; }              // and the speed when its foot had pushed
		float Down() const { return m_profile.down; }  // the shares of the animation between which its foot pushed
		float Up() const { return m_profile.up; }

	private:
		float Pull(float speed, float dt, float units, float done);

		int m_phase;
		bool m_animCurrent; // the skater's animation on the last frame was a push animation, ...
		bool m_animRunning; // ... and it had not reached its end
		uint32_t m_play;    // the play of the animation this push goes with
		uint32_t m_seenPlay;    // the play the last frame was in
		uint32_t m_askPlay;     // the play the frame before the push was wanted was in
		PushProfile m_profile;  // where its foot pushes
		float m_share;      // how far into that play the last frame was (0 to 1), ...
		float m_shareStep;  // ... and how far the frame before it was behind
		float m_askTime;    // how long the push has been asking with no push animation running
		float m_hideTime;   // and how much of that the game's scripts were free to play it
		bool m_held;        // the button on the frame before
		float m_hold;       // how long the press that is down, or the last one, has lasted
		bool m_asking;      // a press that no push has taken up yet
		bool m_tracking;    // the press this push came from is still down
		float m_pushHold;   // how long that press has lasted, as far as it counts for this push
		float m_target;     // the speed the board is taken up to while the foot pushes
		float m_asked;
		float m_added;
		float m_from;
		float m_to;
		uint32_t m_serial;
	};

	// A clock that stands in for the game's push animation. The pushes go with an animation
	// that is read off the skater. Should the game be told to play a push three times in a row
	// with no push animation seen on him after any of them (other scripts than the ones this
	// was written against, say), the pushes are not lost: from then on each goes by the clock,
	// from the moment the game is told, until a push animation is seen after all. (The game's
	// ground script waits for the push it plays, so three times is two pushes' worth of time.)
	extern const int kPushClockAfter;       // told this many times in a row
	extern const float kPushClockLength;    // seconds one push takes by the clock
	class PushClock {
	public:
		PushClock();
		void Reset();
		// The game's scripts have just been told to play a push
		void Told();
		// Once on every frame of the ground physics, after the look at the skater's
		// animation. seen: he is in a push animation. dt: seconds. Returns true while the
		// clock stands in for an animation; anim is then what one of its length would be.
		bool Step(bool seen, float dt, PushAnim& anim);
		// The pushes go by the clock
		bool Blind() const { return m_blind; }
		// For the log: goes up when the clock takes over
		uint32_t Serial() const { return m_serial; }

	private:
		int m_told;         // times in a row the game was told and nothing was seen
		bool m_blind;
		bool m_running;     // a push by the clock is under way
		float m_time;       // and has been for so long
		uint32_t m_play;    // its number, apart from the numbers of the game's animations
		uint32_t m_serial;
	};

	// What one play of a push animation showed of the feet
	enum FootWhy {
		FOOT_OK = 0,
		FOOT_FEW = 1,       // too few looks at the feet
		FOOT_PART = 2,      // not the whole animation was seen
		FOOT_NO_DROP = 3,   // neither foot gets below the other: both stay on the board
		FOOT_SHORT = 4      // the foot is on the ground for no time at all
	};
	struct FootResult {
		int why;                // FOOT_OK, or why nothing came of it
		int samples;            // looks at the feet
		bool left;              // the foot that pushes is the first of the two
		bool swept;             // it goes back along the ground, and the speed goes by that; else it stands where it was put
		float board;            // where the other foot stands: the middle of its heights, in the skater's own frame
		float floor;            // the lowest the foot that pushes gets
		float drop;             // the one less the other, in the game's units
		float sweep;            // how far it goes back
		float lowFrom, lowTo;   // the shares of the animation between which it is on the ground
		PushProfile profile;    // where the speed of a push comes in, and how
	};

	// Watches the two feet through one play of a push animation and works out where the foot
	// pushes. The foot that pushes is the one that gets lower; the other stands on the board.
	// It is on the ground while it is within a fifth of its drop of its lowest point. Its
	// stroke goes the way it travels while on the ground: from the farthest forward it is
	// between coming down below the board and leaving the ground, to the farthest back it gets
	// after that. In THUG's own push animations that is the foot coming down beside the board,
	// sliding back along the ground and going on back as it rises behind the skater; the speed
	// of a push comes in as it goes, most of it while the foot is low and the rest as the leg
	// follows through. A foot that stands where it was put has the speed come in over the time
	// it is on the ground, gently at both ends.
	class FootWatch {
	public:
		FootWatch();
		void Begin();
		// share: how far into the animation (0 to 1). left, right: where the two feet are in
		// the skater's own frame, three numbers each, the second of them the height.
		void Sample(float share, const float* left, const float* right);
		// The play is over. seconds: the length of the animation.
		void End(float seconds, FootResult& out) const;
		int Samples() const;
		// The looks in order, seven numbers each: the share, then the left foot, then the
		// right. At most "most" of them. Returns how many.
		int Looks(float* out, int most) const;

	private:
		enum { BINS = 128 };
		bool m_have[BINS];
		float m_share[BINS];
		float m_left[BINS][3];
		float m_right[BINS][3];
	};

	// The turning of one skater. Step is called once on every frame of the game's ground physics.
	class Steer {
	public:
		Steer();
		void Reset();
		// dt: seconds. stick: -1 (left) to 1 (right). speed: the game's units per second.
		// pushing: a foot is on the ground. percent: Settings::steering, more than 0.
		// Returns how fast the board turns, in radians per second, to the right when positive.
		float Step(float dt, float stick, float speed, bool pushing, int percent);
		// For the log and the tests: the angle of the trucks, in radians
		float Angle() const { return m_angle; }

	private:
		float m_stick;      // the stick, eased
		float m_angle;      // the trucks, eased
		float m_ramp;       // 0.5 to 1: a pushing foot takes turning away
		float m_carry;      // time that has not made a whole tick yet
	};

	// How fast a board turns once its trucks have settled: stick 0 to 1, speed in the game's
	// units per second, the answer in radians per second
	float TurnRate(float stick, float speed, int percent);

	// ------------------------------------------------------------------------------------------
	// Spinning in the air
	//
	// THUG turns the skater in the air at 6.85 to 7.75 radians a second (by his spin stat) for as
	// long as left or right is held: nothing for the first tenth of a second the button is
	// down, then all of it, and the same from the first moment of an air to the last.
	//
	// Skate (the tuning its normal difficulty uses):
	//
	//   - The stick says how fast, by how far it is pushed. With the stick at its end the rate
	//     goes with the time since the board left the ground: 3.2 rad/s at once, 7.9 after a
	//     quarter of a second, 5.7 after 0.9 s, 3.7 from 2 s on.
	//   - Of that rate a stick that is simply held gets 0.4. The rest is for a flick: how fast
	//     the stick moved outward in one tick (times 1.5, at most 1), the best of the last
	//     quarter second before the board left the ground and the first 0.14 s after it (earlier
	//     and later flicks count for less). The stick thrown to its end in one tick is the full
	//     rate; over a twentieth of a second it is 0.7 of it.
	//   - The rate follows by at most 1 rad/s a tick at first and 0.15 after a second in the
	//     air, and comes down by at most 0.2 a tick.
	//   - A stick that is let go does not stop the turning: it goes on from where the stick was,
	//     4 percent less each tick.
	//
	// What Skate adds by itself on a ramp (it turns the skater back toward the way down) is
	// not here: THUG has its own for that.
	// ------------------------------------------------------------------------------------------

	extern const float kThugAirSpinWorst;   // rad/s: THUG's own turning in the air, with the worst spin stat
	extern const float kThugAirSpinBest;    // and with the best (Physics_Air_Rotation_stat)
	extern const float kThugAirSpinWait;    // s before THUG's turning starts (Physics_Air_No_Rotate_Time)
	extern const float kSpinLeast;          // the share of Skate's rate a stick that was not flicked gets
	extern const int kSpinMostPercent;      // the most Settings::spin can be

	// Skate's rate with the stick at its end after a full flick, in radians per second, at a
	// time in seconds since the board left the ground
	float SpinRate(float seconds);

	// The turning of one skater in the air.
	class Spin {
	public:
		Spin();
		void Reset();
		// Once on every frame the skater is not in the air: the stick is watched there too,
		// since a flick just before the board leaves the ground counts. dt: seconds.
		// stick: -1 (left) to 1 (right).
		void Feed(float dt, float stick);
		// Once on every frame of the game's physics in the air. percent: Settings::spin, more
		// than 0. Returns how fast the skater turns, in radians per second, to the right when
		// positive.
		float Step(float dt, float stick, int percent);
		// The air is over. What it came to (Flick, Seconds, Turned) stays until the next one.
		void Land();
		// For the log and the tests
		bool InAir() const { return m_air; }
		float Flick() const { return m_peak < 0.0f ? -m_peak : m_peak; }   // 0 to 1: the flick that counts
		float Seconds() const { return m_time; }                          // in this air
		float Turned() const { return m_turned; }                         // radians asked for in this air, to the right when positive

	private:
		enum { HISTORY = 30, TRAIL = 12 };
		void Watch(float dt, float stick);
		void Tick(float stick, bool air);

		float m_history[HISTORY];   // how fast the stick moved outward, tick by tick
		int m_index;                // where the next one goes
		float m_trailAge[TRAIL];    // where the stick was of late: how long ago (s), the latest look first, ...
		float m_trailStick[TRAIL];  // ... and where
		int m_trailCount;
		float m_flick;              // how fast it moved outward at best since the last tick
		float m_smooth;             // the stick, eased
		bool m_air;
		float m_time;               // seconds in this air
		float m_peak;               // the flick that counts, with the side it went to
		float m_rate;               // rad/s, the way Skate counts it: to the LEFT when positive
		float m_carry;              // time in the air that has not made a whole tick yet
		float m_feed;               // the same on the ground
		float m_turned;
	};

	// ------------------------------------------------------------------------------------------
	// The stance the stick asks for
	//
	// THUG rides nollie as a stance of its own: a loop of the ground of its own (OnGroundNollieAI)
	// with the nollie animations (its crouch is NollieCrouchIdle), and the game's Nollie as the
	// jump from there. Its Nollie button goes round three stances: regular, pressure, nollie.
	//
	// Here the stick decides, as in Skate: crouched up the skater rides nollie, crouched down he
	// rides regular (Output::stance). A crouch up that ends without a pop takes him back to regular.
	// After a pop the game has it: its landing takes him out of nollie as it always did. The game's
	// side looks between frames of a loop of the ground, where no script waits for anything, and
	// moves his script to the other loop, which sets the stance as it starts (OnGroundNollieAI:
	// NollieOn, OnGroundAi: NollieOff). Anywhere else (a landing, a manual, a brake, a grind, where
	// the game's scripts set nollie for their own reasons) it waits. Pressure, the third stance, is
	// left as it is in a crouch down; the stick's nollie is never pressure as well.
	// ------------------------------------------------------------------------------------------

	enum StanceMove {
		STANCE_STAY = 0,        // nothing to do
		STANCE_TO_NOLLIE = 1,   // his script goes to the nollie loop
		STANCE_TO_REGULAR = 2   // and to the regular one
	};

	// What the skater is doing as the game's side looks
	struct StanceLook {
		bool ground;          // on his board on the ground, not in a bail
		bool regularLoop;     // between frames of the loop of the ground (OnGroundAi)
		bool nollieLoop;      // between frames of the nollie loop
		bool pressure;        // in the game's pressure stance
	};

	struct StanceAnswer {
		int move;             // StanceMove, to be made at once
		bool endPressure;     // and out of pressure (before the move, if there is one)
		bool late;            // a move back to regular because nothing came of a crouch up: he is
		                      // still in the nollie loop on the ground a while after it (a pop the
		                      // game did not jump for)
	};

	class StanceKeeper {
	public:
		StanceKeeper();
		void Reset();
		// On every frame the stick drives the skater: what it asks for (Stance)
		void Ask(int stance);
		// After that, once on the frame: where the skater is. The answer is what to do there.
		StanceAnswer Look(const StanceLook& look);
		// He rides nollie because the stick put him in it, and has not left the nollie loop since
		bool Ours() const { return m_ours; }

	private:
		int m_asked;          // what the stick asks for on this frame
		bool m_back;          // regular was asked for, and he is not there yet
		bool m_ours;
		int m_quiet;          // looks in the nollie loop since the stick last asked for nollie
	};

	// Looks in the nollie loop, with nothing asked for, after which a nollie of the stick's that
	// went nowhere goes back to regular (half a second at 60 frames a second)
	static const int kStanceQuietLooks = 30;

	// ------------------------------------------------------------------------------------------
	// Stick patterns
	//
	// A pattern is a short list of stick positions that have to be passed in order, each with a
	// tolerance radius. The text format is the one Skate's own pattern files use, so a player who
	// owns that game can drop its skater.pat next to thugsdl.ini. No pattern data ships with this mod.
	//
	//   global_tolerance_dist 0.4      default tolerance for the patterns that follow
	//   pattern Kickflip               starts a pattern; the name says which trick it is
	//       tolerance_dist 0.4         tolerance of this pattern
	//       coord 0.0 0.7              a point: x (right is positive), y (DOWN is positive), rim = 1.0
	//       clock 2 1.0                a point by clock face: hour (12 = up, 3 = right, 0 = center), radius
	//
	// Other words followed by a number (min_time, global_tolerance_time, ...) are skipped.
	// ------------------------------------------------------------------------------------------

	enum { MAX_PATTERNS = 128, MAX_PATTERN_POINTS = 8, MAX_PATTERN_NAME = 32 };

	struct Pattern {
		char name[MAX_PATTERN_NAME];
		float tolerance;
		int count;
		float x[MAX_PATTERN_POINTS];
		float y[MAX_PATTERN_POINTS];   // down is positive, as in the files

		// What the name asks for
		int known;    // 0: a name this mod has no trick for (the pattern is not tracked)
		int slot;     // direction that goes with Flip, DIR_NONE for a plain ollie
		int taps;     // presses of Flip: 2 for the 360 versions, which THUG does with a second tap
		int nollie;   // 1 for patterns that start with the stick up (Nollie and the N_ names)
	};

	class PatternSet {
	public:
		PatternSet();
		void Clear();
		// Reads pattern text. Returns the number of patterns now in the set.
		int Parse(const char* text, size_t length);
		int Count() const { return m_count; }
		const Pattern& At(int i) const { return m_patterns[i]; }

	private:
		Pattern m_patterns[MAX_PATTERNS];
		int m_count;
	};

	// Fills known / slot / taps / nollie from the pattern's name. Returns p.known.
	int NamePattern(Pattern& p);

	struct PatternMatch {
		int index;     // which pattern of the set
		float speed;   // 1 = drawn fast, 0 = drawn slowly
		int ticks;
		float error;   // sum of squared distances to the points that were hit
	};

	class PatternMatcher {
	public:
		PatternMatcher();
		void Reset();
		// One 60 Hz tick with the stick at (x, yDown). Returns true when a pattern was completed on
		// this tick; if several were, the longest wins, then the most accurate and fastest.
		bool Tick(const PatternSet& set, float x, float yDown, int missTicks, bool nollie, PatternMatch& out);

	private:
		struct State {
			unsigned char active;
			unsigned char next;     // index of the point expected next
			unsigned char misses;   // ticks in a row away from both the last point and the next one
			unsigned short ticks;
			float error;
		};
		State m_state[MAX_PATTERNS];

		// A completed pattern waits a tick or two when a longer one has only its last point to go,
		// so a shuvit drawn along the rim is not cut short by the kickflip that ends in the same place.
		bool m_pending;
		PatternMatch m_pendingMatch;
		float m_pendingRating;
		int m_pendingPoints;
		int m_grace;
	};

	class Recognizer {
	public:
		Recognizer();
		void Reset();
		void Update(const Input& in, const Settings& cfg, Output& out);

		// True while something is still pending (a held wind-up, a queued press, a grind hold)
		bool Busy() const;

		// Decide tricks from stick patterns instead of the built-in directions. The set has to outlive
		// the recognizer; pass 0 to go back to the built-in directions. Survives Reset().
		void UsePatterns(const PatternSet* set);

		// How the board is leaning right now (airborne only), for the grind pick at touchdown
		const Tilt& GetTilt() const { return m_tilt.Get(); }

	private:
		// PH_LEAVE: the stick has left a crouch on the ground, and Ollie is still held. A flick pops;
		// the stick coming to rest at the centre, or no flick in time, stands him back up.
		enum Phase { PH_IDLE, PH_WINDUP, PH_TRACK, PH_LEAVE };
		enum Button { BTN_FLIP, BTN_GRAB };
		enum { MAX_QUEUE = 4 };
		// What leaving the wind-up meant: on the ground Ollie was let go (a pop). On a rail and in
		// the air nothing was pressed yet, and what follows decides.
		enum Track { TRACK_POP, TRACK_RAIL, TRACK_AIR };
		enum Zone { ZONE_NONE, ZONE_CENTER, ZONE_DOWN, ZONE_UP };
		enum SwitchStep { SEQ_IDLE, SEQ_FIRST, SEQ_GAP, SEQ_SECOND };

		struct Pulse {
			int button;
			int dir;
			uint32_t notBefore;
			int stage;            // 0 waiting, 1 direction only, 2 direction + button
			uint32_t stageStart;
			int frames;
		};

		void BeginWindup(int wind, int sector, const Input& in, const Settings& cfg);
		void StartNollieTaps(const Input& in, const Settings& cfg);
		void BeginTrack(uint32_t now, int track);
		void LeaveWindup(const Input& in, const Settings& cfg, bool nearCentre);
		void Popped(uint32_t now, const Settings& cfg);
		void RailPop(const Input& in, const Settings& cfg);
		void ArmGrindHold(const Input& in, const Settings& cfg);
		void Finalize(int end, const Input& in, const Settings& cfg, Output& out);
		void FinalizePattern(const Pattern& p, const PatternMatch& m, const Input& in, const Settings& cfg, Output& out);
		void Enqueue(int button, int dir, uint32_t notBefore);
		uint32_t AfterPop(const Settings& cfg) const;
		void RunTaps(uint32_t now, Output& out);
		void RunQueue(const Input& in, const Settings& cfg, Output& out);
		void RunRailSwitch(const Input& in, const Settings& cfg, float mag, int sector, Output& out);
		void RunStepOff(const Input& in, const Settings& cfg, Output& out);
		void NoteSpeed(const Input& in, const Settings& cfg);
		void NoteCrouch(uint32_t now, float target);
		float CrouchStrength(uint32_t now, const Settings& cfg) const;
		void ResumeCrouch(uint32_t now, const Settings& cfg);
		void StandUp(const Input& in, Output& out);
		void HoldFlip(int slot, int sector, const Input& in);
		bool WasFast(const Settings& cfg) const;

		int m_phase;
		int m_wind;
		bool m_armed;
		int m_start;
		int m_cur;
		uint32_t m_curSince;
		uint32_t m_popTime;       // when Ollie was let go
		uint32_t m_trackStart;    // when the stick left the wind-up
		bool m_hasPopped;
		int m_cand;
		uint32_t m_candSince;

		// Wind-ups in the air and on a rail do not hold Ollie
		bool m_holdOllie;
		int m_track;
		// The pop out of a grind: a short press of Ollie
		bool m_railTap;
		uint32_t m_railTapStart;
		int m_railTapFrames;

		int m_lastState;
		uint32_t m_airSince;

		// The last moments of the stick, to tell a flick from a slow move
		enum { HISTORY = 64 };
		uint32_t m_histTime[HISTORY];
		float m_histX[HISTORY];
		float m_histY[HISTORY];
		int m_histCount;
		int m_histNext;
		bool m_hasFast;
		uint32_t m_lastFast;      // the last frame the stick was moving at flick speed

		// How deep the crouch is (0 to 1): it follows the stick, deeper at most so fast, and the last
		// moments of it
		float m_crouch;
		uint32_t m_crouchAt;
		enum { CROUCH_HISTORY = 32 };
		uint32_t m_crouchTime[CROUCH_HISTORY];
		float m_crouchDepth[CROUCH_HISTORY];
		int m_crouchCount;
		int m_crouchNext;
		// Leaving the crouch (PH_LEAVE): since when, how strong the pop would be, since when the
		// stick has rested at the centre (0: it has not)
		uint32_t m_leaveStart;
		float m_leaveStrength;
		uint32_t m_restSince;
		bool m_resting;
		float m_popOut;           // the strength of a pop made on this frame, for Output::popStrength
		// The stick held at the end of a flip trick's flick: the trick's slot and where the flick
		// ended, and the game's chance that was last pressed for
		bool m_hold;
		int m_holdSlot;
		int m_holdSector;
		uint32_t m_holdLevel;

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

		// The flicks are read mirrored left / right (Output::mirrored)
		bool m_mirror;

		// Stick patterns
		const PatternSet* m_patterns;
		PatternMatcher m_matcher;
		uint32_t m_tickLast;
		int m_tickAccum;      // thirds of a millisecond since the last 60 Hz tick
		bool m_tickInit;

		// Board tilt in the air
		TiltTracker m_tilt;

		// Switching between 50-50, 5-0 and nosegrind on a rail
		int m_zone;
		uint32_t m_zoneSince;
		bool m_railStick;         // the stick has had a say in this grind
		int m_seqStep;
		int m_seqTarget;
		uint32_t m_seqStepStart;
		int m_seqFrames;
		bool m_switchWait;        // a switch was sent: wait for the game to start the new grind
		uint32_t m_switchSerial;
		uint32_t m_switchSent;
		int m_switchWaitFrames;

		// Stepping off a rail the board has stopped on
		bool m_offDown;           // the button is down
		int m_offSide;
		uint32_t m_offStart;
		int m_offFrames;
		bool m_offWait;           // it was pressed: the game gets a moment before it is pressed again
		uint32_t m_offSent;
		int m_offWaitFrames;
	};

	// Helpers, exposed for the tests and for logging
	int Sector(float x, float y);
	float AngleDeg(float x, float y);
	bool InWindupHalf(float angleDeg, int wind, float marginDeg);
	int MapFlip(int wind, int start, int end, int kickflipSide);
	const char* DirName(int dir);
	const char* SlotName(int slot);
}
