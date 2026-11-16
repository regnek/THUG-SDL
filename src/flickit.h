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

	struct Settings {
		int enabled;          // master switch
		int toggleButton;     // SDL_GameControllerButton that switches Flickit on/off in a level, -1 for none
		int reloadKey;        // SDL_Scancode that reloads this section while the game runs, 0 for none
		int debug;            // 1: log every recognized gesture

		int kickflipSide;     // 0: kickflip = flick up-right, heelflip = up-left. 1: mirrored
		int nollie;           // 1: stick up, then flick down = nollie
		int nollieTaps;       // presses of the Nollie button needed to get from regular into nollie stance
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

		Input() : now(0), rx(0.0f), ry(0.0f), leftTrigger(false), rightTrigger(false),
			state(STATE_UNKNOWN), railCatch(false), grind(GRIND_UNKNOWN), grindReady(false), grindSerial(0),
			railOff(false), offSide(0) {}
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
	struct FlatJump {
		float height;
		float seconds;
	};
	FlatJump JumpOnFlat(float thugPop, int gravityPercent, int popPercent);

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
		enum Phase { PH_IDLE, PH_WINDUP, PH_TRACK };
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
		void StartNollieTaps(uint32_t now, const Settings& cfg);
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
