#include "flickit_game.h"
#include "flickit_pose.h"
#include "watchdog.h"
#include "autosave.h"
#include "bigworld.h"
#include "drive_game.h"
#include "players_game.h"
#include "daynight_game.h"

#include <windows.h>
#include <string.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <ctime>

#include "patch.h"
#include "camrel_game.h"
#include "Logger/Logger.h"

namespace FlickitGame {

	/******************************************************************/
	/*                                                                */
	/*  The game, as far as this file knows it                        */
	/******************************************************************/

	// Skate::Instance(), the module that owns the skaters. 0 until the game has made it.
	static void** const kSkate = (void**)0x0076A788;

	// The first skater that is played on this computer, or 0
	typedef void* (__thiscall* GetLocalSkater_t)(void* skate);
	static const GetLocalSkater_t GetLocalSkater = (GetLocalSkater_t)0x004F94F0;

	// A component of an object by the checksum of its type name, or 0
	typedef void* (__thiscall* GetComponent_t)(void* object, uint32_t type);
	static const GetComponent_t GetComponent = (GetComponent_t)0x0041BDC0;

	// CSkaterCorePhysicsComponent::do_grind_trick: starts the grind script out of GrindTrickList.
	// Direction is the row (0 none, 1 Up, 2 Down, 3 Left, 4 Right, 5 Up+Left, 6 Up+Right, 7 Down+Left,
	// 8 Down+Right). The four flags pick the entry in the row; of each only the low byte counts.
	typedef void(__thiscall* DoGrindTrick_t)(void* core, uint32_t direction, uint32_t railOnLeft, uint32_t parallel, uint32_t backwards, uint32_t flipped);
	static const DoGrindTrick_t DoGrindTrick = (DoGrindTrick_t)0x005608C0;

	// The balance of a grind or slide, once per frame. this = the balance component + 0x7C
	typedef void(__thiscall* BalanceUpdate_t)(void* record);
	static const BalanceUpdate_t BalanceUpdate = (BalanceUpdate_t)0x0050B9C0;

	// Mdl::Score::SetBalanceMeter: shows (on) or hides the balance meter of a skater's panel, its
	// arrow at value. this = the score. The meter's scripts (show_balance_meter, hide_balance_meter)
	// run only when on is not what the meter already is; of on only the low byte counts.
	typedef void(__thiscall* SetBalanceMeter_t)(void* score, uint32_t on, float value);
	static const SetBalanceMeter_t SetBalanceMeter = (SetBalanceMeter_t)0x004F6700;

	// Script::CScript
	typedef void(__thiscall* SetScript_t)(void* script, uint32_t name, void* params, void* object);
	static const SetScript_t SetScript = (SetScript_t)0x0040FBC0;
	typedef void(__thiscall* ScriptUpdate_t)(void* script);
	static const ScriptUpdate_t ScriptUpdate = (ScriptUpdate_t)0x0040F0A0;

	// CSkaterFlipAndRotateComponent: carries out a flip or rotation a script left for "after"
	typedef void(__thiscall* FlipRotateAfters_t)(void* component);
	static const FlipRotateAfters_t FlipRotateAfters = (FlipRotateAfters_t)0x00570560;

	// The entry of a global script symbol, or 0. Byte 1 of the entry is its type, 7 for a script.
	typedef void* (__cdecl* LookUpSymbol_t)(uint32_t name);
	static const LookUpSymbol_t LookUpSymbol = (LookUpSymbol_t)0x004133A0;

	// Script::CStruct::GetChecksum
	typedef bool(__thiscall* StructGetChecksum_t)(void* structure, uint32_t name, uint32_t* out, bool assert);
	static const StructGetChecksum_t StructGetChecksum = (StructGetChecksum_t)0x004110B0;

	// Script::CStruct is no more than the head of a list of its items (0x00430620 makes one empty).
	// AddChecksum puts in a checksum under a name, or with no name as a flag (what ContainsFlag
	// looks for); Clear gives every item back and leaves the list empty.
	struct GameStruct {
		void* first;
	};
	typedef void(__thiscall* StructAddChecksum_t)(GameStruct* structure, uint32_t name, uint32_t value);
	static const StructAddChecksum_t StructAddChecksum = (StructAddChecksum_t)0x004123F0;
	// The same for a whole number
	typedef void(__thiscall* StructAddInteger_t)(GameStruct* structure, uint32_t name, int32_t value);
	static const StructAddInteger_t StructAddInteger = (StructAddInteger_t)0x00412230;
	typedef void(__thiscall* StructClear_t)(GameStruct* structure);
	static const StructClear_t StructClear = (StructClear_t)0x00411790;

	// CSkaterBalanceTrickComponent's script commands (DoBalanceTrick, StopBalanceTrick, ...): its
	// slot in the component's table of virtual functions, the one a script's command goes through.
	// The command's name, its parameters, the script. 2 for a name it does not have.
	typedef int(__thiscall* BalanceCommand_t)(void* balance, uint32_t name, GameStruct* params, void* script);
	static const BalanceCommand_t BalanceCommand = (BalanceCommand_t)0x0055C370;

	// CSkaterCorePhysicsComponent: does the line from CORE_LINE_START to CORE_LINE_END hit the world?
	// The test SkateInAble and the game's test for a ledge at the start of a grind go through. The two
	// numbers are what the line ignores (0x10, 0 there: what does not collide).
	typedef bool(__thiscall* LineHit_t)(void* core, uint32_t ignore, uint32_t ignore2);
	static const LineHit_t LineHit = (LineHit_t)0x005602F0;

	// One of the skater's physics values (physics.q) by the checksum of its name
	typedef float(__cdecl* GetPhysicsFloat_t)(uint32_t name, int flag);
	static const GetPhysicsFloat_t GetPhysicsFloat = (GetPhysicsFloat_t)0x00516110;

	// CRailManager::StickToRail: looks for a rail near the way the skater moved in this frame, from
	// "from" to "to". Fills in the point on the rail and the node its piece starts at.
	typedef bool(__thiscall* RailSearch_t)(void* manager, const float* from, const float* to, float* point, void** node, const void* ignore, float minCos, int side);
	static const RailSearch_t RailSearch = (RailSearch_t)0x00514630;

	// The game's own extra test of a piece of rail. It says yes at once unless a park from the
	// editor is running. Two vectors, handed over whole.
	struct GameVector {
		float x, y, z, w;
	};
	typedef bool(__cdecl* RailValid_t)(GameVector from, GameVector to);
	static const RailValid_t RailValid = (RailValid_t)0x00512760;

	// CSkaterCorePhysicsComponent::get_air_gravity: Physics_Air_Gravity over the hang stat (and
	// times Moon_gravity with that cheat on). Less than 0.
	typedef float(__thiscall* AirGravity_t)(void* core);
	static const AirGravity_t AirGravity = (AirGravity_t)0x0055E670;

	// CSkaterCorePhysicsComponent::do_jump, what the script command Jump does: works out the speed
	// of the jump from the stats and from how long the Ollie button was held, and adds it
	typedef void(__thiscall* DoJump_t)(void* core, void* params);
	static const DoJump_t DoJump = (DoJump_t)0x00560410;

	// Script::CStruct::GetFloat: true and the number if the structure has one of that name
	typedef bool(__thiscall* StructGetFloat_t)(void* structure, uint32_t name, float* out, int assert);
	static const StructGetFloat_t StructGetFloat = (StructGetFloat_t)0x00410E20;

	// CSkaterCorePhysicsComponent, in the physics of the ground: should the skater brake on this
	// frame, and may he kick? Both only look.
	typedef bool(__thiscall* ShouldBrake_t)(void* core);
	static const ShouldBrake_t ShouldBrake = (ShouldBrake_t)0x0055D250;
	typedef bool(__thiscall* ShouldKick_t)(void* core);
	static const ShouldKick_t ShouldKick = (ShouldKick_t)0x0055D410;
	// The friction of one frame on the ground: the air's, and the wheels'
	typedef void(__thiscall* GroundFriction_t)(void* core);
	static const GroundFriction_t GroundFriction = (GroundFriction_t)0x00563B80;
	// The turning of one frame on the ground, by Left and Right
	typedef void(__thiscall* GroundTurn_t)(void* core);
	static const GroundTurn_t GroundTurn = (GroundTurn_t)0x0055E790;
	// What that turning is done with: a vector turned about the upright axis, and a matrix
	// turned about its own
	typedef void(__thiscall* VectorTurn_t)(void* vector, float angle);
	static const VectorTurn_t VectorTurn = (VectorTurn_t)0x0042F6D0;
	typedef void* (__thiscall* MatrixTurn_t)(void* matrix, float angle);
	static const MatrixTurn_t MatrixTurn = (MatrixTurn_t)0x00405230;
	// The trick component's answer to "is this button down", for the script command Held
	typedef bool(__thiscall* ButtonHeld_t)(void* trick, uint32_t button);
	static const ButtonHeld_t ButtonHeld = (ButtonHeld_t)0x00451A70;
	// The game's own picture of a pad, brought up to date from what one look at the controller
	// gave: which buttons are down, since when, and where the sticks are
	typedef void(__thiscall* PadUpdate_t)(void* pad, void* data);
	static const PadUpdate_t PadUpdate = (PadUpdate_t)0x0051AEB0;
	// CSkaterCorePhysicsComponent, in the physics of the air: the turning of one frame, by Left
	// and Right or by the two spin buttons
	typedef void(__thiscall* AirTurn_t)(void* core);
	static const AirTurn_t AirTurn = (AirTurn_t)0x0055F050;
	// One of the skater's numbers that go by his stats (the pairs in physics.q), by the checksum
	// of its name. this = the skater.
	typedef float(__thiscall* ScriptedStat_t)(void* skater, uint32_t name);
	static const ScriptedStat_t ScriptedStat = (ScriptedStat_t)0x00516AD0;
	// Gfx::CSkeleton: where a bone is, in the frame of the object the skeleton belongs to (left,
	// up, forward). The bone goes in by the checksum of its name, the answer comes out as four
	// numbers. False if the skeleton has no such bone. The script command Obj_GetBonePosition
	// goes through it. It only reads.
	typedef bool(__thiscall* BonePosition_t)(void* skeleton, uint32_t bone, float* out);
	static const BonePosition_t BonePosition = (BonePosition_t)0x00487100;
	// CAnimationComponent: is the object's animation over (its first blend channel at its
	// end, and not one that loops). The script command AnimFinished answers with it.
	typedef bool(__thiscall* AnimOver_t)(void* anim);
	static const AnimOver_t AnimOver = (AnimOver_t)0x004310D0;
	// A blend channel of an animation: is it at its end (never one that loops). The wait of a
	// script for the end of an object's animation asks it about the first channel.
	typedef bool(__thiscall* ChannelOver_t)(void* channel);
	static const ChannelOver_t ChannelOver = (ChannelOver_t)0x004703B0;
	// CAnimationComponent: the start of a blend, as PlayAnim starts a new animation with one. The
	// channels after the second are dropped, the first begins to blend out over "seconds", and a
	// new channel goes in front (see flickit_pose.h).
	typedef void(__thiscall* BlendStart_t)(void* anim, float seconds);
	static const BlendStart_t BlendStart = (BlendStart_t)0x00430F20;
	// CAnimationComponent: the pose of the object, its channels blended by their weights, put on
	// its skeleton. Once a frame, in the component's update.
	typedef void(__thiscall* PoseBlend_t)(void* anim);
	static const PoseBlend_t PoseBlend = (PoseBlend_t)0x00431B60;
	// CAnimationComponent: start an animation, what PlayAnim does once it has read its parameters:
	// the name, whether to tell the other players of a net game, where it starts and ends
	// (seconds), how it loops (0 once, 1 Cycle, 2 PingPong, 3 wobble), the blend's time, and its
	// speed. The blend starts in it (BlendStartHook).
	typedef void(__thiscall* PlayAnimCore_t)(void* anim, uint32_t name, int net, float from, float to, int loop, float blend, float speed);
	static const PlayAnimCore_t PlayAnimCore = (PlayAnimCore_t)0x004320E0;
	// The game's clock, in milliseconds (its frames, at 60 a second)
	typedef int(__cdecl* GameMs_t)();
	static const GameMs_t GameMs = (GameMs_t)0x00582C20;
	// CSkaterCorePhysicsComponent: how long until a skater at height y, going up at vy, is down at
	// "floor", under the gravity of the air (the call inside it to that gravity is Flickit's). The
	// script command GetAirTimeLeft asks it with the floor below the skater.
	typedef float(__thiscall* TimeToHeight_t)(void* core, float floor, float y, float vy);
	static const TimeToHeight_t TimeToHeight = (TimeToHeight_t)0x0055E730;
	// CAnimationComponent: whether the object has an animation of that name, and how long it is
	// (seconds, at its own speed). The script command GetAnimLength asks these two.
	typedef bool(__thiscall* AnimExists_t)(void* anim, uint32_t name);
	static const AnimExists_t AnimExists = (AnimExists_t)0x00430650;
	typedef float(__thiscall* AnimLength_t)(void* anim, uint32_t name);
	static const AnimLength_t AnimLength = (AnimLength_t)0x00430630;

	enum {
		// any component
		COMPONENT_OBJECT = 0x0C,        // the object it belongs to

		// the skater object
		OBJECT_SCRIPT = 0x10,           // its script
		OBJECT_LEFT = 0x60,             // the way its left side points (the first row of its matrix; "Move x = 1" in a script goes there)
		OBJECT_MATRIX = 0x60,           // that matrix
		OBJECT_FORWARD = 0x80,          // the way it faces (the third row)
		OBJECT_POSITION = 0x40,         // where it is
		OBJECT_VELOCITY = 0xA0,
		OBJECT_PLAYER = 0x2BC,          // which player's skater it is
		OBJECT_GOOFY = 0x134,           // one byte: his profile's stance is goofy

		// CAnimationComponent
		ANIM_FIRST = 0x24,              // the first of its blend channels: the animation the object is in.
		                                // With no animation it is the head of that list itself.
		// a blend channel
		CHANNEL_SELF = 0x04,            // itself. The head of the list has -1 here.
		CHANNEL_START = 0x14,           // seconds: where its animation starts,
		CHANNEL_TIME = 0x18,            // where it is,
		CHANNEL_END = 0x1C,             // and where it ends
		CHANNEL_SPEED = 0x20,           // how fast it runs, per 60th of a second
		CHANNEL_BACKWARDS = 0x24,       // not 0: it runs from the end to the start
		CHANNEL_NAME = 0x30,            // the checksum of the animation's name

		// CSkeletonComponent
		SKELETON_SKELETON = 0x18,       // its Gfx::CSkeleton

		// Script::CScript
		SCRIPT_PARAMS = 0x14,           // the parameters of the script that is running
		SCRIPT_DEPTH = 0x30,            // how many calls deep it is
		SCRIPT_CALLS = 0x8C,            // where the calls are kept, one after the other: ...
		SCRIPT_CALL_SIZE = 0x2C,        // ... this long each, the name of the caller first
		SCRIPT_CALLS_MOST = 0x10,       // there is room for this many
		SCRIPT_NAME = 0xBC,             // and the checksum of its name

		// CSkaterCorePhysicsComponent
		CORE_DISPLAY_MATRIX = 0x18,     // the matrix the skater is drawn with
		CORE_FRAME_LENGTH = 0x8C,       // seconds, the frame its physics are working on
		CORE_RAIL_NODE = 0x22C,         // the rail it is on: the node the segment starts at
		CORE_AUTO_KICK = 0x2D0,         // one byte: he kicks without being asked (the game's option)
		CORE_NO_KICK = 0x2D8,           // one byte: a script has forbidden kicking (CanKickOff, in a revert)
		CORE_ROLLING_FRICTION = 0x2DC,  // what the wheels lose; scripts raise it to brake
		CORE_TENSE_TIME = 0x33C,        // ms the jump button was down for the jump that comes (the game's clock)
		CORE_VELOCITY_LOCKED = 0x34C,   // one byte: turning does not turn the direction of travel
		CORE_TURNING_LEFT = 0x365,      // one byte: the last turn on the ground went to the left
		CORE_GRIND_TWEAK = 0x368,
		CORE_LINE_START = 0x208,        // the two ends of the line for LineHit, four floats each
		CORE_LINE_END = 0x218,
		CORE_INPUT = 0x4B4,             // CInputComponent
		CORE_BALANCE = 0x4CC,           // CSkaterBalanceTrickComponent
		CORE_STATE = 0x4D0,             // CSkaterStateComponent

		// CSkaterBalanceTrickComponent
		BALANCE_TYPE = 0x18,            // checksum of what is being balanced: grind, slide, lip, manual...

		// CRailManager
		MANAGER_NODES = 0x40,           // its rail nodes, one after the other
		MANAGER_COUNT = 0x4C,           // how many

		// a rail node
		RAIL_NODE_SIZE = 0x50,
		RAIL_BOX_MIN = 0x00,            // the box around the piece of rail that starts at this node
		RAIL_BOX_MAX = 0x10,
		RAIL_POSITION = 0x20,           // four floats
		RAIL_NEXT = 0x40,               // the node the piece ends at, 0 at the end of a rail
		RAIL_PREVIOUS = 0x44,
		RAIL_FLAGS = 0x48,
		RAIL_TERRAIN = 0x4E,            // one byte: what the rail is made of, for the sound and the sparks
		RAIL_FLAG_ACTIVE = 1 << 2,      // the level has it switched on
		RAIL_FLAG_CLIMB_ONLY = 1 << 4,  // for climbing, not for the board

		// CInputComponent
		INPUT_PAD = 0x20,               // the game's picture of the pad; each button of it starts with one byte "is down"
		INPUT_DOWN_PRESSED = 0x40,      // Down is held
		INPUT_LEFT_PRESSED = 0x60,      // Left is, by the d-pad or by the stick
		INPUT_RIGHT_PRESSED = 0x80,
		BUTTON_PRESSED_AT = 0x04,       // in a button of the pad: the game's clock (ms) when it went down
		INPUT_CIRCLE_PRESSED = 0x160,   // Circle is down (the pad's buttons by the names it gives them: 0x0051AA60)
		INPUT_GRIND_PRESSED = 0x1A0,    // the Grind button (Triangle) is down
		INPUT_JUMP_PRESSED = 0x1C0,     // the jump button (X) is
		INPUT_DISABLED = 0x498,         // one byte: a script has taken the controls away (DisablePlayerInput)
		INPUT_NET_DISABLED = 0x499,     // one byte: the same for a network game

		// Skate::Instance()
		SKATE_AUTO_KICK = 0x3B1,        // one byte for each player, eight bytes apart: the option "auto kick"
		SKATE_PLAYER_SIZE = 8,

		// the SkaterStancePanel component (0x24 bytes, made at 0x00578CE0): the stances that the
		// script commands NollieOn / NollieOff / InNollie and PressureOn / PressureOff / InPressure
		// (0x00578BE0) set and read
		PANEL_NOLLIE = 0x1D,            // one byte: he rides nollie
		PANEL_PRESSURE = 0x1E,          // one byte: he rides pressure

		// CSkaterStateComponent
		STATE_STATE = 0x18,             // 0 ground, 1 air, 2 wall, 3 lip, 4 rail, 5 wallplant
		STATE_PHYSICS = 0x1C,           // 0 skating, 1 walking
		STATE_CROUCHED = 0x20,          // one byte: he crouches, the jump button down (what the script command Crouched answers)
		STATE_FLIPPED = 0x28,           // one byte: his animations are flipped (Flipped); not flipped is goofy's way round
		STATE_VERT_AIR = 0x30,
		STATE_BAILING = 0x70,           // one byte: in a bail
		STATE_DOING_TRICK = 0xD8,
		STATE_DRIVING = 0x114,
		STATE_HEIGHT = 0x118,           // in the air: how far above the ground below him he is

		// CTrickComponent: the tricks the trick in progress can turn into on a press of a button
		// (SetExtraTricks), for a while
		TRICK_EXTRA_ON = 0x12C8,        // one byte: there are some
		TRICK_EXTRA_FOREVER = 0x12C9,   // one byte: for as long as the trick lasts (no Duration)
		TRICK_EXTRA_LENGTH = 0x12CC,    // else for this many ms (the game's clock) ...
		TRICK_EXTRA_START = 0x12D0,     // ... from this moment
		TRICK_EXTRA_COUNT = 0x12D4,     // how many lists of them,
		TRICK_EXTRA_NAMES = 0x12D8,     // the name of each list, one after the other
		TRICK_EXTRA_MOST = 0x0A,        // room for this many

		// the balance record of a rail (CSkaterBalanceTrickComponent + 0x7C)
		BALANCE_RECORD = 0x7C,
		BALANCE_LEAN = 0x30,
		BALANCE_LEAN_SPEED = 0x34,
	};

	// Name checksums
	static const uint32_t kSkaterState = 0x43686585;          // SkaterState
	static const uint32_t kSkaterCorePhysics = 0x5BD63B29;    // SkaterCorePhysics
	static const uint32_t kSlideBalance = 0x8D10119D;         // slide: the balance of a slide. A grind's is "grind"
	static const uint32_t kGrindBalance = 0x255ED86F;         // grind: the balance of a grind, and the name of the Grind script
	// DoBalanceTrick ButtonA = Right ButtonB = Left Type = <Type> DoFlipCheck: the balance a grind
	// script starts for the rail
	static const uint32_t kDoBalanceTrick = 0x7E92670C;       // DoBalanceTrick
	static const uint32_t kButtonA = 0x4F1A0B01;              // ButtonA
	static const uint32_t kButtonB = 0xD6135ABB;              // ButtonB
	static const uint32_t kRight = 0x4B358AEB;                // Right
	static const uint32_t kLeft = 0x85981897;                 // Left
	static const uint32_t kType = 0x7321A8D6;                 // Type
	static const uint32_t kDoFlipCheck = 0x1304E677;          // DoFlipCheck
	// SkateInOrBail { FallingLeft or Fallingright GrindBail = Airborne moveright = 5 movey = 5 }: THUG's way
	// off a rail, to one side, without a bail
	static const uint32_t kSkateInOrBail = 0x90E528CA;        // SkateInOrBail
	static const uint32_t kFallingLeft = 0x73CE4E57;          // FallingLeft
	static const uint32_t kFallingRight = 0xD0A71E0D;         // Fallingright
	static const uint32_t kGrindBail = 0xA7E24442;            // GrindBail
	static const uint32_t kAirborne = 0xCF3C89F7;             // Airborne
	static const uint32_t kMoveRight = 0x1878C644;            // moveright
	static const uint32_t kMoveY = 0x5A151ED3;                // movey
	// The game's test for a ledge next to a rail
	static const uint32_t kGroundSnapUp = 0xE4D79235;         // Physics_Ground_Snap_Up
	static const uint32_t kBadLedgeDown = 0x9CD7ED5C;         // Rail_Bad_Ledge_Drop_Down_Dist
	static const uint32_t kBadLedgeSide = 0x31669752;         // Rail_Bad_Ledge_Side_Dist
	static const uint32_t kSkaterFlipAndRotate = 0x488AF07A;  // SkaterFlipAndRotate
	static const uint32_t kAnim = 0x98549BA4;                 // Anim
	static const uint32_t kTailGrindRange = 0x7896F05F;       // TailGrind_Range: the 5-0
	static const uint32_t kNoseGrindRange = 0x8391BDEB;       // NoseGrind_Range: the nosegrind
	static const uint32_t kFiftyFiftyRange = 0xB914FDA6;      // FiftyFifty_Range, FiftyFifty2_range, FiftyFifty3_range: the 50-50
	static const uint32_t kFiftyFifty2Range = 0x11431833;
	static const uint32_t kFiftyFifty3Range = 0xB7341387;
	static const uint32_t kTrick = 0x270F56E1;                // Trick: the component that answers Held
	static const uint32_t kSquare = 0x321C9756;               // Square: the Flip button, the game's own button for a kick
	static const uint32_t kRollingFriction = 0x78F80EC4;      // Physics_Rolling_Friction
	static const uint32_t kNollieLoop = 0xF8D9EFB4;           // OnGroundNollieAI
	static const uint32_t kGroundLoop = 0x1DDE4804;           // OnGroundAi
	static const uint32_t kWaitOneFrame = 0x18B3B291;         // WaitOneGameFrame: where the loops of the ground wait for the next frame
	static const uint32_t kStancePanel = 0x21FC3301;          // SkaterStancePanel: the component that keeps nollie and pressure
	// The animations started under a crouch for it to blend with: the one the skater rolls in, and
	// the one he rolls in in nollie stance
	static const uint32_t kStandIdle = 0x23DB4AEA;          // Idle
	static const uint32_t kStandNollieIdle = 0xFC69157A;    // NollieSkatingIdle
	// the blend they start with (the game's own blend time; the Keeper sets the shares)
	static const float kStandBlend = 0.3f;
	static const uint32_t kWaitAnimChecking = 0x64DF5E4C;     // WaitAnimWhilstChecking
	static const uint32_t kLand2 = 0x6D8A1BC1;                // Land2: the skater lands
	static const uint32_t kSkateInLand = 0xE08E71D3;          // SkateInLand: he skates off a rail onto the ground
	static const uint32_t kFlailLeft = 0xB4101D70;            // FlailLeft: he bumps into a wall
	static const uint32_t kFlailRight = 0x756A7535;           // FlailRight
	static const uint32_t kMongoStep = 0x16751E0B;            // MongoPush: the step off the board before a push with the front foot
	static const uint32_t kAirRotation = 0x4957DB43;          // Physics_Air_Rotation_stat
	static const uint32_t kAnimation = 0x72AD7B23;            // Animation: the component that plays an object's animations
	static const uint32_t kSkeleton = 0x222756D5;             // Skeleton: the component that has its bones
	static const uint32_t kToeLeft = 0xF3A6A221;              // Bone_Toe_L
	static const uint32_t kToeRight = 0x09A99F42;             // Bone_Toe_R

	// The game's push animations. The six of a skater who stands on his board differ in what
	// the arms do; the other two are the push with the front foot and the push out of a crouch.
	// What the feet did in one animation stands in for another of its kind until that one has
	// been watched itself.
	struct PushClip {
		uint32_t name;
		int kind;
		const char* text;
	};
	static const PushClip kPushClips[] = {
		{ 0x4266D67F, 0, "PushCycle" },
		{ 0x860F4FD8, 0, "PushCycleArms1" },
		{ 0x1F061E62, 0, "PushCycleArms2" },
		{ 0x68012EF4, 0, "PushCycleArms3" },
		{ 0xF665BB57, 0, "PushCycleArms4" },
		{ 0x81628BC1, 0, "PushCycleArms5" },
		{ 0xFF10285F, 1, "MongoPushCycle" },
		{ 0x991F66BC, 2, "CrouchPush1" },
	};
	enum { PUSH_CLIPS = sizeof(kPushClips) / sizeof(kPushClips[0]) };

	// The animations the game's scripts play when the skater lands (Land2, PlayLandAnim),
	// skates off a rail onto the ground (SkateInLand) or bumps into a wall (FlailLeft,
	// FlailRight), and then wait for to the end (WaitAnimWhilstChecking) before they go back
	// to the loop that plays the push. A push of Flickit's cuts them short.
	struct CutClip {
		uint32_t name;
		const char* text;
	};
	static const CutClip kPushCuts[] = {
		{ 0x50159F24, "CrouchBumpDown" },
		{ 0x47273509, "IdleBump" },
		{ 0xBC825547, "CrouchedLand" },
		{ 0xB9DA565C, "CrouchedLand1" },
		{ 0x20D307E6, "CrouchedLand2" },
		{ 0x57D43770, "CrouchedLand3" },
		{ 0x01C6C9A9, "CrouchedLand1_small" },
		{ 0x302ED334, "CrouchedLand2_small" },
		{ 0x9659D880, "CrouchedLand3_small" },
		{ 0xA6B243EA, "CrouchedLandTurn" },
		{ 0x11A7B91F, "CrouchedLandTurn1" },
		{ 0x88AEE8A5, "CrouchedLandTurn2" },
		{ 0xFFA9D833, "CrouchedLandTurn3" },
		{ 0x490A6675, "StretchLegsLand" },
		{ 0xC72E8DD2, "SketchyLand" },
		{ 0x39C49D4F, "SketchyLand1" },
		{ 0x05AE68AE, "SketchyCrouchLand" },
		{ 0x57FF2A27, "Land" },
		{ 0xB4101D70, "FlailLeft" },
		{ 0x986630A1, "FlailLeft2" },
		{ 0x718F0623, "FlailLeftCrouched_small" },
		{ 0xFB4885E9, "FlailLeftCrouched2" },
		{ 0x8C4FB57F, "FlailLeftCrouched3" },
		{ 0x756A7535, "FlailRight" },
		{ 0x9E11FFD6, "FlailRight2" },
		{ 0x4E3F0CA9, "FlailRightCrouched_small" },
		{ 0xFEEF3ADB, "FlailRightCrouched2" },
		{ 0x89E80A4D, "FlailRightCrouched3" },
	};
	enum { PUSH_CUTS = sizeof(kPushCuts) / sizeof(kPushCuts[0]) };

	// The flip tricks the game takes round once more on another press of the trick button, and
	// which are that same trick with one rotation more: what the trick in progress is given to
	// turn into (its ExtraTricks), and the animation of that trick, riding regular and nollie
	// (airtricks.q). A 360 flip out of a varial kickflip, say, is another trick, and not here.
	struct HeldFlip {
		uint32_t extra;
		const char* text;
		uint32_t anim;
		const char* animText;
		uint32_t nollie;
		const char* nollieText;
		const char* title;      // what the game calls the trick
	};
	static const HeldFlip kHeldFlips[] = {
		{ 0xB35FC553, "DoubleKickflip", 0xB35FC553, "DoubleKickflip", 0xB6834A5A, "DoubleNollieKickflip", "Double Kickflip" },
		{ 0xD94EE2EC, "TripleKickflip", 0xD94EE2EC, "TripleKickflip", 0xD94EE2EC, "TripleKickflip", "Triple Kickflip" },
		{ 0x9E0997C9, "DoubleHeelflip", 0x9E0997C9, "DoubleHeelflip", 0x9BD518C0, "DoubleNollieHeelflip", "Double Heelflip" },
		{ 0xF418B076, "TripleHeelflip", 0xF418B076, "TripleHeelflip", 0xF418B076, "TripleHeelflip", "Triple Heelflip" },
		{ 0x7F634D04, "DoubleImpossible", 0x7F634D04, "DoubleImpossible", 0x7F634D04, "DoubleImpossible", "Double Impossible" },
		{ 0xB9342009, "TripleImpossible", 0xB9342009, "TripleImpossible", 0xB9342009, "TripleImpossible", "Triple Impossible" },
		{ 0x39BD0DC1, "Extra360ShoveIt", 0xE54680B2, "_360ShoveIt", 0x424B04FA, "NollieBS360ShoveIt", "360 Shove-It" },
		{ 0x1A555500, "Extra540ShoveIt", 0xC6AED873, "_540ShoveIt", 0xC6AED873, "_540ShoveIt", "540 Shove-It" },
		{ 0x73BD4833, "ExtraFS360ShoveIt", 0xBEFD23C9, "FS360ShoveIt", 0x2050BFF9, "Nollie360ShoveIt", "360 FS Shove-It" },
		{ 0x505510F2, "ExtraFS540ShoveIt", 0x9D157B08, "FS540ShoveIt", 0x9D157B08, "FS540ShoveIt", "540 FS Shove-It" },
		{ 0x3F05C386, "Extra_DBackfootKickflip", 0xB6834A5A, "DoubleNollieKickflip", 0xB6834A5A, "DoubleNollieKickflip", "Double Back Foot Flip" },
		{ 0x1253911C, "Extra_DBackfootHeelflip", 0x9BD518C0, "DoubleNollieHeelflip", 0x9BD518C0, "DoubleNollieHeelflip", "Double Back Foot Heelflip" },
		{ 0x53F2E1B9, "Extra_360BackfootShoveIt", 0x424B04FA, "NollieBS360ShoveIt", 0x424B04FA, "NollieBS360ShoveIt", "360 Back Foot Shove-It" },
	};
	enum { HELD_FLIPS = sizeof(kHeldFlips) / sizeof(kHeldFlips[0]) };

	// Where the game's code is changed
	static uint8_t* const kCatchTest1 = (uint8_t*)0x0056CB60;   // cmp byte ptr [eax+1A0h], 0   in maybe_stick_to_rail
	static uint8_t* const kCatchTest2 = (uint8_t*)0x0056FA36;   // cmp byte ptr [edx+1A0h], 0   the same test in the skater's update
	static uint8_t* const kPickCall = (uint8_t*)0x0056AFDC;     // call do_grind_trick          at the end of stick_to_rail
	static uint8_t* const kBalanceCall = (uint8_t*)0x0056D065;  // call the balance update      in the rail physics
	// call SetBalanceMeter in the balance update: the meter of a balance on Left and Right, shown on
	// every frame of it (one on Up and Down, a manual's, has a call of its own right after this one)
	static uint8_t* const kMeterCall = (uint8_t*)0x0050BF0F;
	static uint8_t* const kBoostCall = (uint8_t*)0x0056A9D9;    // call GetPhysicsFloat         for Rail_Speed_Boost, in stick_to_rail
	// call CRailManager::StickToRail: after each of the two catch tests, first for the rails of the
	// level, then for those of each thing that moves
	static uint8_t* const kSearchCall1 = (uint8_t*)0x0056CC54;
	static uint8_t* const kSearchMovingCall1 = (uint8_t*)0x0056CD56;
	static uint8_t* const kSearchCall2 = (uint8_t*)0x0056FB28;
	static uint8_t* const kSearchMovingCall2 = (uint8_t*)0x0056FC26;
	// call get_air_gravity: every place the game asks for the gravity in the air. The first is in
	// the function that works out how long a flight to a given height takes, the next two in the
	// function at 0x00561B80, which uses that, and the last at the top of the physics of the air
	// (0x0056BAD0), where the gravity goes into the skater's speed frame by frame.
	static uint8_t* const kGravityCall1 = (uint8_t*)0x0055E731;
	static uint8_t* const kGravityCall2 = (uint8_t*)0x0056246B;
	static uint8_t* const kGravityCall3 = (uint8_t*)0x0056272E;
	static uint8_t* const kGravityCall4 = (uint8_t*)0x0056BAE6;
	// call do_jump, in the function that carries out the script commands of the physics component
	static uint8_t* const kJumpCall = (uint8_t*)0x00567FEF;
	// call CStruct::GetFloat in do_jump: a script may hand the speed of the jump over as "speed".
	// The speed worked out so far is where the answer goes; after this call it is final.
	static uint8_t* const kJumpSpeedCall = (uint8_t*)0x005606EA;
	// In the physics of the ground (0x0056DF80), one after the other: call should_brake, call
	// should_kick (when not braking), call the friction, and at the end call the turning
	static uint8_t* const kBrakeTestCall = (uint8_t*)0x0056E1B1;
	static uint8_t* const kKickTestCall = (uint8_t*)0x0056E1CD;
	static uint8_t* const kFrictionCall = (uint8_t*)0x0056E2BF;
	static uint8_t* const kTurnCall = (uint8_t*)0x0056E81D;
	// call should_kick, for the script command CanKick
	static uint8_t* const kCanKickCall = (uint8_t*)0x0056747C;
	// call "is this button down", for the script command Held with one button
	static uint8_t* const kHeldCall = (uint8_t*)0x004566CC;
	// call the pad update, where an input component is handed what one look at the controller gave
	static uint8_t* const kPadUpdateCall = (uint8_t*)0x00437E9E;
	// call the turning in the air, in the physics of the air (0x0056BAD0), and inside that turning
	// the two calls that ask how fast the skater turns: one for Left, one for Right
	static uint8_t* const kAirTurnCall = (uint8_t*)0x0056BB46;
	// in the script command AnimFinished, the call that asks whether the animation is over
	static uint8_t* const kAnimFinishedCall = (uint8_t*)0x00432EF3;
	// in the wait of a script for the end of an object's animation (the animation component's
	// wait), the call that asks whether its first channel is over
	static uint8_t* const kAnimWaitCall = (uint8_t*)0x00431DC7;
	static uint8_t* const kSpinLeftCall = (uint8_t*)0x0055F2FD;
	static uint8_t* const kSpinRightCall = (uint8_t*)0x0055F361;
	// in PlayAnim's start of an animation (0x004320E0), the call that starts the blend; and in the
	// update of an animation component, the call that makes its pose
	static uint8_t* const kBlendStartCall = (uint8_t*)0x004322AC;
	static uint8_t* const kPoseCall = (uint8_t*)0x004320A1;

	/******************************************************************/
	/*                                                                */
	/*  Is this the game the above was read from?                     */
	/******************************************************************/

	struct Expect {
		uint32_t address;
		int size;
		uint8_t bytes[18];
		const char* what;
	};

	static const Expect kExpect[] = {
		// the places that get changed, with the instruction before each
		{ 0x0056CB5A, 15, { 0x8B, 0x86, 0xB4, 0x04, 0x00, 0x00, 0x80, 0xB8, 0xA0, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x84 }, "rail catch test" },
		{ 0x0056FA30, 15, { 0x8B, 0x96, 0xB4, 0x04, 0x00, 0x00, 0x80, 0xBA, 0xA0, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x84 }, "rail catch test in the update" },
		{ 0x0056AFDA, 7, { 0x8B, 0xCE, 0xE8, 0xDF, 0x58, 0xFF, 0xFF }, "grind pick" },
		{ 0x0056D062, 8, { 0x8D, 0x48, 0x7C, 0xE8, 0x56, 0xE9, 0xF9, 0xFF }, "rail balance" },
		{ 0x0050BF0D, 7, { 0x6A, 0x01, 0xE8, 0xEC, 0xA7, 0xFE, 0xFF }, "balance meter" },
		{ 0x0056A9A7, 5, { 0x68, 0x95, 0xB3, 0x7B, 0x45 }, "rail boost name" },
		{ 0x0056A9D9, 5, { 0xE8, 0x32, 0xB7, 0xFA, 0xFF }, "rail boost" },
		{ 0x0056CC52, 7, { 0x8B, 0xCF, 0xE8, 0xD7, 0x79, 0xFA, 0xFF }, "rail search" },
		{ 0x0056CD54, 7, { 0x8B, 0xCB, 0xE8, 0xD5, 0x78, 0xFA, 0xFF }, "rail search on a moving object" },
		{ 0x0056FB26, 7, { 0x8B, 0xCF, 0xE8, 0x03, 0x4B, 0xFA, 0xFF }, "rail search in the update" },
		{ 0x0056FC24, 7, { 0x8B, 0xCD, 0xE8, 0x05, 0x4A, 0xFA, 0xFF }, "rail search on a moving object in the update" },
		{ 0x0055E730, 6, { 0x51, 0xE8, 0x3A, 0xFF, 0xFF, 0xFF }, "air gravity for the length of a flight" },
		{ 0x00562469, 7, { 0x8B, 0xCE, 0xE8, 0x00, 0xC2, 0xFF, 0xFF }, "air gravity for a flight" },
		{ 0x0056272E, 5, { 0xE8, 0x3D, 0xBF, 0xFF, 0xFF }, "air gravity for a flight, again" },
		{ 0x0056BAE6, 5, { 0xE8, 0x85, 0x2B, 0xFF, 0xFF }, "air gravity in the physics of the air" },
		{ 0x00567FEE, 6, { 0x52, 0xE8, 0x1C, 0x84, 0xFF, 0xFF }, "jump" },
		{ 0x005606E6, 9, { 0xD9, 0x5C, 0x24, 0x1C, 0xE8, 0x31, 0x07, 0xEB, 0xFF }, "jump speed" },
		{ 0x0056E1AF, 7, { 0x8B, 0xCE, 0xE8, 0x9A, 0xF0, 0xFE, 0xFF }, "brake test" },
		{ 0x0056E1C6, 12, { 0xC6, 0x86, 0xEC, 0x02, 0x00, 0x00, 0x00, 0xE8, 0x3E, 0xF2, 0xFE, 0xFF }, "kick test" },
		{ 0x0056E2BD, 7, { 0x8B, 0xCE, 0xE8, 0xBC, 0x58, 0xFF, 0xFF }, "friction on the ground" },
		{ 0x0056E81B, 7, { 0x8B, 0xCE, 0xE8, 0x6E, 0xFF, 0xFE, 0xFF }, "turning on the ground" },
		{ 0x0056747C, 5, { 0xE8, 0x8F, 0x5F, 0xFF, 0xFF }, "CanKick" },
		{ 0x004566C9, 8, { 0x50, 0x8B, 0xCE, 0xE8, 0x9F, 0xB3, 0xFF, 0xFF }, "Held" },
		{ 0x00437E9A, 9, { 0x57, 0x8D, 0x4E, 0x20, 0xE8, 0x0D, 0x30, 0x0E, 0x00 }, "pad update" },
		{ 0x0056BB44, 7, { 0x8B, 0xCE, 0xE8, 0x05, 0x35, 0xFF, 0xFF }, "turning in the air" },
		{ 0x00432EC1, 7, { 0x3D, 0xD5, 0x99, 0xCC, 0x76, 0x74, 0x2B }, "AnimFinished" },
		{ 0x00432EF3, 11, { 0xE8, 0xD8, 0xE1, 0xFF, 0xFF, 0xF6, 0xD8, 0x1B, 0xC0, 0xF7, 0xD8 }, "AnimFinished: is the animation over" },
		{ 0x00431DA0, 14, { 0x56, 0x8B, 0x74, 0x24, 0x08, 0x83, 0xBE, 0x9C, 0x00, 0x00, 0x00, 0x05, 0x75, 0x29 }, "animation wait: for the end" },
		{ 0x00431DAE, 13, { 0x8B, 0x41, 0x24, 0x83, 0x78, 0x04, 0xFF, 0x74, 0x20, 0x85, 0xC0, 0x74, 0x1C }, "animation wait: the first channel" },
		{ 0x00431DC7, 16, { 0xE8, 0xE4, 0xE5, 0x03, 0x00, 0x84, 0xC0, 0x74, 0x07, 0x8B, 0xCE, 0xE8, 0xC9, 0xB9, 0xFD, 0xFF }, "animation wait: is it over, then the wait is" },
		{ 0x0055F2F5, 13, { 0x8B, 0x4E, 0x0C, 0x68, 0x43, 0xDB, 0x57, 0x49, 0xE8, 0xCE, 0x77, 0xFB, 0xFF }, "spin rate for Left" },
		{ 0x0055F359, 13, { 0x8B, 0x4E, 0x0C, 0x68, 0x43, 0xDB, 0x57, 0x49, 0xE8, 0x6A, 0x77, 0xFB, 0xFF }, "spin rate for Right" },
		{ 0x004322A7, 10, { 0x8B, 0xCE, 0xD9, 0x1C, 0x24, 0xE8, 0x6F, 0xEC, 0xFF, 0xFF }, "PlayAnim: the start of the blend" },
		{ 0x00432297, 8, { 0x8D, 0x4E, 0x18, 0xE8, 0x91, 0xEB, 0xFF, 0xFF }, "PlayAnim without a blend: every channel goes" },
		{ 0x00432281, 16, { 0xD9, 0xEE, 0xD8, 0x9C, 0x24, 0x6C, 0x05, 0x00, 0x00, 0xDF, 0xE0, 0xF6, 0xC4, 0x44, 0x7B, 0x06 }, "PlayAnim: a blend of no time drops every channel" },
		{ 0x00432291, 6, { 0x80, 0x7E, 0x34, 0x00, 0x75, 0x08 }, "PlayAnim: and so does blending switched off" },
		{ 0x00432912, 6, { 0x89, 0x5E, 0x40, 0x88, 0x5E, 0x34 }, "the animation component: blending starts off" },
		{ 0x004330CD, 6, { 0x0F, 0x95, 0xC2, 0x88, 0x56, 0x34 }, "Obj_EnableAnimBlending: the switch" },
		{ 0x0043209F, 7, { 0x8B, 0xCF, 0xE8, 0xBA, 0xFA, 0xFF, 0xFF }, "the animation's update: the pose" },
		// what the crouch pose relies on (flickit_pose.h)
		{ 0x00430F4B, 6, { 0x8B, 0x41, 0x24, 0x8D, 0x69, 0x18 }, "blend start: the list of channels" },
		{ 0x00430F76, 5, { 0x83, 0xFF, 0x03, 0x7C, 0x1D }, "blend start: two channels are kept" },
		{ 0x0043100B, 18, { 0xD9, 0x46, 0x50, 0xD9, 0x5C, 0x24, 0x18, 0xD9, 0x46, 0x54, 0xD9, 0x5C, 0x24, 0x14, 0xD9, 0x44, 0x24, 0x14 }, "blend start: what the others weigh" },
		{ 0x00431035, 18, { 0xD9, 0x44, 0x24, 0x30, 0xD9, 0xE8, 0xDE, 0xE1, 0xD9, 0x5C, 0x24, 0x30, 0xD9, 0x47, 0x50, 0xD9, 0x5C, 0x24 }, "blend start: the first weighs the rest" },
		{ 0x0043107E, 10, { 0x33, 0xC0, 0x89, 0x68, 0x10, 0x8B, 0x55, 0x0C, 0x89, 0x50 }, "blend start: a new channel in front" },
		{ 0x00431088, 10, { 0x0C, 0x8B, 0x4D, 0x0C, 0x89, 0x41, 0x10, 0x89, 0x45, 0x0C }, "blend start: a new channel in front, the links" },
		{ 0x00470B50, 18, { 0x51, 0x8B, 0x41, 0x34, 0x83, 0xE8, 0x00, 0x74, 0x1C, 0x83, 0xE8, 0x01, 0x74, 0x09, 0x83, 0xE8, 0x01, 0x75 }, "channel weight" },
		{ 0x00470B67, 6, { 0xD9, 0x41, 0x54, 0xD8, 0x49, 0x50 }, "channel weight while it blends out" },
		{ 0x00470A87, 9, { 0x83, 0x7E, 0x34, 0x01, 0x88, 0x5E, 0x2C, 0x75, 0x48 }, "channel update: blending out" },
		{ 0x00470A9F, 12, { 0xD9, 0x46, 0x50, 0xD9, 0x46, 0x4C, 0xD8, 0x4C, 0x24, 0x08, 0xDE, 0xE9 }, "channel update: what is left of the blend" },
		{ 0x00470AC5, 18, { 0xD8, 0x4E, 0x54, 0xDC, 0x1D, 0xC8, 0xED, 0x62, 0x00, 0xDF, 0xE0, 0xF6, 0xC4, 0x05, 0x7A, 0x03, 0x89, 0x5E }, "channel update: over when it weighs less than nothing" },
		{ 0x00431B00, 18, { 0x8B, 0x44, 0x24, 0x04, 0x56, 0x57, 0x8B, 0xF1, 0x50, 0x8D, 0x4E, 0x18, 0xE8, 0x0F, 0xE1, 0x0F, 0x00, 0x8B }, "pose: a channel's weight" },
		{ 0x00431B11, 9, { 0x8B, 0xF8, 0x8B, 0xCF, 0xE8, 0x36, 0xF0, 0x03, 0x00 }, "pose: a channel's weight, asked" },
		{ 0x004307A2, 16, { 0x85, 0xED, 0xD9, 0x44, 0x24, 0x10, 0xD8, 0x64, 0x24, 0x54, 0x89, 0x6C, 0x24, 0x18, 0xD9, 0x1A }, "pose: the playing channel weighs the rest" },
		{ 0x00431B60, 16, { 0x83, 0xEC, 0x08, 0x80, 0x3D, 0x60, 0xC8, 0x7C, 0x00, 0x00, 0x53, 0x8B, 0xD9, 0x0F, 0x85, 0xB8 }, "pose" },
		{ 0x00430F20, 12, { 0x6A, 0xFF, 0x68, 0xEB, 0x2B, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "blend start" },
		{ 0x004320E0, 12, { 0x81, 0xEC, 0x4C, 0x05, 0x00, 0x00, 0xA1, 0xB0, 0x4A, 0x69, 0x00, 0x33 }, "PlayAnim's start of an animation" },
		{ 0x0043210D, 8, { 0x80, 0xBC, 0x24, 0x5C, 0x05, 0x00, 0x00, 0x00 }, "PlayAnim's start: the other players are told when asked" },
		{ 0x00432345, 3, { 0xC2, 0x1C, 0x00 }, "PlayAnim's start pops seven" },
		{ 0x0047045F, 15, { 0xD9, 0x44, 0x24, 0x08, 0xD9, 0x59, 0x14, 0xD9, 0x51, 0x18, 0xD8, 0xD1, 0xDF, 0xE0, 0xF6 }, "a channel's Set: start and time" },
		{ 0x00470480, 18, { 0xD9, 0x44, 0x24, 0x08, 0xD9, 0x59, 0x1C, 0xD9, 0x44, 0x24, 0x18, 0xDC, 0x35, 0xA8, 0xEE, 0x62, 0x00, 0xD9 }, "a channel's Set: end and speed" },
		{ 0x004704A4, 18, { 0x8B, 0x54, 0x24, 0x04, 0x89, 0x41, 0x24, 0x8B, 0x44, 0x24, 0x10, 0x89, 0x41, 0x28, 0xC6, 0x41, 0x2C, 0x00 }, "a channel's Set: backwards and the loop" },
		// the functions that get called
		{ 0x004F94F0, 12, { 0x53, 0x56, 0x57, 0x8B, 0xF9, 0x33, 0xF6, 0xE8, 0x84, 0xC0, 0xF3, 0xFF }, "GetLocalSkater" },
		{ 0x0041BDC0, 12, { 0x8B, 0x81, 0xF0, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x12, 0x8B, 0x4C }, "GetComponent" },
		{ 0x005608C0, 12, { 0x6A, 0xFF, 0x68, 0x0B, 0xB4, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "do_grind_trick" },
		{ 0x0050B9C0, 12, { 0x6A, 0xFF, 0x68, 0x5B, 0x8D, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "balance update" },
		{ 0x004F6700, 14, { 0xD9, 0x44, 0x24, 0x08, 0x8B, 0x44, 0x24, 0x04, 0x6A, 0x00, 0x51, 0xD9, 0x1C, 0x24 }, "SetBalanceMeter" },
		{ 0x0040FBC0, 12, { 0x8B, 0x44, 0x24, 0x0C, 0x8B, 0x54, 0x24, 0x08, 0x50, 0x8B, 0x44, 0x24 }, "CScript::SetScript" },
		{ 0x0040F0A0, 12, { 0x53, 0x56, 0x8B, 0xF1, 0x33, 0xDB, 0x39, 0x5E, 0x0C, 0x88, 0x9E, 0xA9 }, "CScript::Update" },
		{ 0x00570560, 12, { 0x53, 0x56, 0x8B, 0xF1, 0x32, 0xDB, 0x38, 0x5E, 0x19, 0x74, 0x08, 0xE8 }, "flip and rotate afters" },
		{ 0x004133A0, 12, { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0xC1, 0x25, 0xFF, 0x0F, 0x00, 0x00, 0x56 }, "LookUpSymbol" },
		{ 0x004110B0, 12, { 0x51, 0x53, 0x56, 0x8B, 0x31, 0x32, 0xDB, 0x85, 0xF6, 0x89, 0x4C, 0x24 }, "CStruct::GetChecksum" },
		{ 0x00516110, 14, { 0x51, 0x56, 0x6A, 0x00, 0x68, 0xDD, 0x89, 0xBC, 0x8C, 0xE8, 0xE2, 0xD4, 0xEF, 0xFF }, "GetPhysicsFloat" },
		{ 0x00514630, 12, { 0x81, 0xEC, 0x60, 0x01, 0x00, 0x00, 0x53, 0x55, 0x56, 0x8B, 0xF1, 0x57 }, "CRailManager::StickToRail" },
		{ 0x00514CB6, 3, { 0xC2, 0x1C, 0x00 }, "StickToRail return" },
		{ 0x00512760, 12, { 0x6A, 0xFF, 0x68, 0xAB, 0xB3, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "rail test" },
		{ 0x0055E670, 12, { 0x8B, 0x81, 0xD0, 0x04, 0x00, 0x00, 0x83, 0xEC, 0x08, 0x80, 0x78, 0x30 }, "get_air_gravity" },
		{ 0x0055E724, 7, { 0xD9, 0x04, 0x24, 0x83, 0xC4, 0x08, 0xC3 }, "get_air_gravity return" },
		{ 0x00560410, 12, { 0x83, 0xEC, 0x14, 0x53, 0x55, 0x56, 0x57, 0x8B, 0xF1, 0x8B, 0x4C, 0x24 }, "do_jump" },
		{ 0x005608B4, 6, { 0x83, 0xC4, 0x14, 0xC2, 0x04, 0x00 }, "do_jump return" },
		{ 0x00410E20, 12, { 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x8D, 0x44, 0x24, 0x0C, 0x50, 0x6A }, "CStruct::GetFloat" },
		{ 0x0055D250, 12, { 0x51, 0x56, 0x8B, 0xF1, 0x57, 0x8B, 0xBE, 0xB4, 0x04, 0x00, 0x00, 0x83 }, "should_brake" },
		{ 0x0055D410, 12, { 0x83, 0xEC, 0x08, 0x56, 0x8B, 0xF1, 0x8B, 0x86, 0xB4, 0x04, 0x00, 0x00 }, "should_kick" },
		{ 0x00563B80, 12, { 0x83, 0xEC, 0x08, 0x56, 0x8B, 0xF1, 0x80, 0xBE, 0xD0, 0x02, 0x00, 0x00 }, "friction on the ground" },
		{ 0x0055E790, 12, { 0x83, 0xEC, 0x08, 0x53, 0x56, 0x57, 0x8B, 0xF9, 0x8B, 0x47, 0x0C, 0xD9 }, "turning on the ground" },
		{ 0x0042F6D0, 12, { 0x51, 0xD9, 0x44, 0x24, 0x08, 0x56, 0x8B, 0xF1, 0xE8, 0x93, 0x8F, 0x1C }, "turn a vector" },
		{ 0x0042F731, 3, { 0xC2, 0x04, 0x00 }, "turn a vector, return" },
		{ 0x00405230, 12, { 0xD9, 0x44, 0x24, 0x04, 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00, 0x56, 0x51 }, "turn a matrix" },
		{ 0x0040527B, 3, { 0xC2, 0x04, 0x00 }, "turn a matrix, return" },
		{ 0x00451A70, 12, { 0x8B, 0x44, 0x24, 0x04, 0x56, 0x50, 0x8B, 0xF1, 0xE8, 0x03, 0xF6, 0xFC }, "button held" },
		{ 0x00451A88, 3, { 0xC2, 0x04, 0x00 }, "button held, return" },
		{ 0x0051AEB0, 12, { 0x83, 0xEC, 0x0C, 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x18, 0x0F, 0xB6, 0x47 }, "pad update function" },
		{ 0x0051B3AA, 3, { 0xC2, 0x04, 0x00 }, "pad update function, return" },
		{ 0x0055F050, 12, { 0x83, 0xEC, 0x18, 0x56, 0x8B, 0xF1, 0x8B, 0x86, 0xB4, 0x04, 0x00, 0x00 }, "turning in the air, function" },
		{ 0x00516AD0, 12, { 0x51, 0x56, 0x57, 0x6A, 0x00, 0x68, 0xDD, 0x89, 0xBC, 0x8C, 0x8B, 0xF9 }, "scripted stat" },
		{ 0x00516B1E, 6, { 0x5F, 0x5E, 0x59, 0xC2, 0x04, 0x00 }, "scripted stat, return" },
		// do_jump: the name it asks GetFloat for, where the speed is taken up again after the
		// call, and where it goes into the skater's speed
		{ 0x005606C5, 5, { 0x68, 0x09, 0x01, 0xD9, 0xF0 }, "jump speed name" },
		{ 0x005607D4, 16, { 0xD9, 0x80, 0xA4, 0x00, 0x00, 0x00, 0xD8, 0x44, 0x24, 0x10, 0xD9, 0x98, 0xA4, 0x00, 0x00, 0x00 }, "jump speed added" },
		// the game's own walk through the rail nodes, where it uses what the search here reads: the
		// list and its length, a node's size, flags, box, position, next and previous node
		{ 0x00514786, 10, { 0x8B, 0x76, 0x40, 0x03, 0x74, 0x24, 0x24, 0x8B, 0x46, 0x48 }, "rail nodes" },
		{ 0x0051478D, 11, { 0x8B, 0x46, 0x48, 0x8B, 0xC8, 0xC1, 0xE9, 0x04, 0xF6, 0xC1, 0x01 }, "rail node flags" },
		{ 0x005147AB, 15, { 0xC1, 0xE8, 0x02, 0xA8, 0x01, 0x0F, 0x84, 0x94, 0x04, 0x00, 0x00, 0x83, 0x7E, 0x40, 0x00 }, "rail node active, next" },
		{ 0x005147C0, 3, { 0xD9, 0x46, 0x10 }, "rail box max" },
		{ 0x005147D2, 2, { 0xD9, 0x06 }, "rail box min" },
		{ 0x00514833, 16, { 0xD9, 0x46, 0x20, 0x8B, 0x46, 0x40, 0xD9, 0x9C, 0x24, 0xA8, 0x00, 0x00, 0x00, 0xD9, 0x46, 0x24 }, "rail node position" },
		{ 0x00514AF1, 4, { 0x83, 0x7E, 0x44, 0x00 }, "rail node previous" },
		{ 0x00514C5E, 11, { 0x83, 0x44, 0x24, 0x24, 0x50, 0x83, 0xC0, 0x01, 0x3B, 0x46, 0x4C }, "rail node size, count" },
		// the game's rail physics, where it uses what the drag reads and changes: the balance component and
		// what it balances, the rail node, the length of the frame, the speed, a rail node's terrain
		{ 0x0056D049, 15, { 0x8B, 0x86, 0xCC, 0x04, 0x00, 0x00, 0x8B, 0x48, 0x18, 0x81, 0xF9, 0x6F, 0xD8, 0x5E, 0x25 }, "balance kind" },
		{ 0x0056D0C1, 6, { 0x8B, 0x9E, 0x2C, 0x02, 0x00, 0x00 }, "rail node" },
		{ 0x0056D234, 6, { 0xD9, 0x86, 0x8C, 0x00, 0x00, 0x00 }, "frame length" },
		{ 0x0056D23E, 15, { 0x8B, 0x46, 0x0C, 0xD9, 0x84, 0x24, 0x94, 0x00, 0x00, 0x00, 0x05, 0xA0, 0x00, 0x00, 0x00 }, "speed" },
		{ 0x0056D6F8, 10, { 0x89, 0x86, 0x2C, 0x02, 0x00, 0x00, 0x0F, 0xB6, 0x40, 0x4E }, "rail terrain" },
		// the game's own test for stepping off a rail to the left or right: the skater's left
		{ 0x00567E7F, 3, { 0x8D, 0x4F, 0x60 }, "the skater's left" },
		// rolling: what the game's own brake test, friction, turning and kick read, where they
		// read it: the switch for kicking unasked, the Flip button and Down, the flag of a bail,
		// what the wheels lose and the name of the usual number, Left and Right, whether the
		// direction of travel turns with the board, the two matrices, and which way is forward
		{ 0x0055D25E, 16, { 0x80, 0xBE, 0xD0, 0x02, 0x00, 0x00, 0x00, 0x75, 0x54, 0x80, 0xBF, 0x60, 0x01, 0x00, 0x00, 0x00 }, "brake test: auto kick, Flip" },
		{ 0x0055D2BB, 4, { 0x80, 0x7F, 0x20, 0x00 }, "brake test: Down" },
		{ 0x00563B86, 17, { 0x80, 0xBE, 0xD0, 0x02, 0x00, 0x00, 0x00, 0x75, 0x30, 0x8B, 0x86, 0xD0, 0x04, 0x00, 0x00, 0x80, 0x78 }, "friction: auto kick, the state" },
		{ 0x00563B97, 2, { 0x70, 0x00 }, "friction: in a bail" },
		{ 0x00563B9B, 13, { 0xD9, 0x86, 0xDC, 0x02, 0x00, 0x00, 0x6A, 0x00, 0x68, 0xC4, 0x0E, 0xF8, 0x78 }, "friction: the wheels" },
		{ 0x0055E7D5, 4, { 0x80, 0x7E, 0x40, 0x00 }, "turning: Left" },
		{ 0x0055E7E6, 4, { 0x80, 0x7E, 0x20, 0x00 }, "turning: Down" },
		{ 0x0055E809, 4, { 0x80, 0x7E, 0x60, 0x00 }, "turning: Right" },
		{ 0x0055E8AC, 13, { 0x80, 0xBF, 0x4C, 0x03, 0x00, 0x00, 0x00, 0x88, 0x87, 0x65, 0x03, 0x00, 0x00 }, "turning: locked, which way" },
		{ 0x0055E8BB, 18, { 0x51, 0x8B, 0x4F, 0x0C, 0xD9, 0x1C, 0x24, 0x81, 0xC1, 0xA0, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x0E, 0xED, 0xFF }, "turning: the speed" },
		{ 0x0055E8D1, 15, { 0x51, 0x8B, 0x4F, 0x0C, 0xD9, 0x1C, 0x24, 0x83, 0xC1, 0x60, 0xE8, 0x50, 0x69, 0xEA, 0xFF }, "turning: the skater" },
		{ 0x0055E8E4, 12, { 0x51, 0x8D, 0x4F, 0x18, 0xD9, 0x1C, 0x24, 0xE8, 0x40, 0x69, 0xEA, 0xFF }, "turning: as he is drawn" },
		{ 0x0055D41F, 7, { 0x80, 0xBE, 0xD8, 0x02, 0x00, 0x00, 0x00 }, "kick test: forbidden" },
		{ 0x0055D441, 4, { 0x80, 0x78, 0x20, 0x00 }, "kick test: Down" },
		{ 0x0055D4F7, 10, { 0x80, 0xBF, 0x4C, 0x03, 0x00, 0x00, 0x00, 0x8B, 0x77, 0x0C }, "kick: locked" },
		{ 0x0055D574, 6, { 0xD9, 0x86, 0x80, 0x00, 0x00, 0x00 }, "kick: forward" },
		{ 0x0056E183, 9, { 0x8B, 0x86, 0xCC, 0x04, 0x00, 0x00, 0x8B, 0x48, 0x18 }, "ground physics: what is balanced" },
		// Left and Right as the game's scripts read them (LeftPressed, RightPressed), and the
		// order of an object's frame: its script first, then its components, the physics among them
		{ 0x00437BD4, 9, { 0x33, 0xC0, 0x38, 0x47, 0x60, 0x5F, 0x0F, 0x95, 0xC0 }, "LeftPressed" },
		{ 0x00437A12, 12, { 0x33, 0xC0, 0x38, 0x87, 0x80, 0x00, 0x00, 0x00, 0x5F, 0x0F, 0x95, 0xC0 }, "RightPressed" },
		{ 0x0041BBBA, 12, { 0x8B, 0x4F, 0x10, 0x85, 0xC9, 0x74, 0x05, 0xE8, 0xDA, 0x34, 0xFF, 0xFF }, "object update: the script" },
		{ 0x0041BBD4, 9, { 0x56, 0x8B, 0xB7, 0xF0, 0x00, 0x00, 0x00, 0x85, 0xF6 }, "object update: the components" },
		{ 0x0041BBE0, 14, { 0x84, 0x5E, 0x08, 0x75, 0x0E, 0x8B, 0x16, 0x8B, 0x42, 0x04, 0x8B, 0xCE, 0xFF, 0xD0 }, "object update: one component" },
		// the turning in the air: where it reads Left and Right (the pad is on the stack, the
		// button 0x40 / 0x60 into it), and the game's "how long has this button been down",
		// which takes the time it went down from the button's second word
		{ 0x0055F2E9, 12, { 0x8B, 0x7C, 0x24, 0x10, 0x83, 0xC7, 0x40, 0x80, 0x3F, 0x00, 0x74, 0x58 }, "air turning: Left" },
		{ 0x0055F34D, 12, { 0x8B, 0x7C, 0x24, 0x10, 0x83, 0xC7, 0x60, 0x80, 0x3F, 0x00, 0x74, 0x5B }, "air turning: Right" },
		{ 0x005199E0, 12, { 0x56, 0x8B, 0x71, 0x04, 0xE8, 0x37, 0x92, 0x06, 0x00, 0x2B, 0xC6, 0x5E }, "how long a button has been down" },
		// the name of the script a CScript is running: kept and replaced where it calls another
		// script, and set where it is given a script to run
		{ 0x0040E7E9, 6, { 0x8B, 0x95, 0xBC, 0x00, 0x00, 0x00 }, "script name, kept for a call" },
		{ 0x0040E861, 6, { 0x89, 0x95, 0xBC, 0x00, 0x00, 0x00 }, "script name, the called script's" },
		{ 0x0040F949, 6, { 0x89, 0xAE, 0xBC, 0x00, 0x00, 0x00 }, "script name, set" },
		// the game's own "have the controls been taken away" test
		{ 0x004374F0, 18, { 0x80, 0xB9, 0x98, 0x04, 0x00, 0x00, 0x00, 0x75, 0x0C, 0x80, 0xB9, 0x99, 0x04, 0x00, 0x00, 0x00, 0x75, 0x03 }, "input disabled" },
		// the script command RestoreAutoKick: where the option is kept, and where it goes
		{ 0x0056871A, 18, { 0x8B, 0x56, 0x0C, 0x8B, 0x82, 0xBC, 0x02, 0x00, 0x00, 0x8B, 0x0D, 0x88, 0xA7, 0x76, 0x00, 0x8A, 0x94, 0xC1 }, "auto kick: the player" },
		{ 0x00568729, 13, { 0x8A, 0x94, 0xC1, 0xB1, 0x03, 0x00, 0x00, 0x88, 0x96, 0xD0, 0x02, 0x00, 0x00 }, "auto kick: the option" },
		// the animation an object is in: its animation component keeps a list of blend channels
		// (the head of the list is in the component and says -1 where a channel has itself),
		// the newest goes first, and that one is "the" animation for the script commands
		{ 0x004328DE, 3, { 0x8D, 0x4E, 0x18 }, "animation: the list of channels" },
		{ 0x004328ED, 7, { 0xC7, 0x41, 0x04, 0xFF, 0xFF, 0xFF, 0xFF }, "animation: the head of the list" },
		{ 0x00432908, 7, { 0xC7, 0x46, 0x04, 0x23, 0x7B, 0xAD, 0x72 }, "animation: the component's name" },
		{ 0x004706DC, 3, { 0x89, 0x40, 0x04 }, "animation: a channel has itself" },
		{ 0x00431080, 18, { 0x89, 0x68, 0x10, 0x8B, 0x55, 0x0C, 0x89, 0x50, 0x0C, 0x8B, 0x4D, 0x0C, 0x89, 0x41, 0x10, 0x89, 0x45, 0x0C }, "animation: a new channel goes first" },
		{ 0x00431DE0, 7, { 0x8B, 0x41, 0x24, 0x83, 0x78, 0x04, 0xFF }, "animation: the one AnimEquals asks about" },
		// what a channel keeps of its animation: the name, where it starts, is and ends
		// (seconds), which way it runs, and when it counts as over
		{ 0x00470350, 4, { 0x8B, 0x41, 0x30, 0xC3 }, "animation: its name" },
		{ 0x00470360, 4, { 0xD9, 0x41, 0x18, 0xC3 }, "animation: its time" },
		{ 0x0047045F, 18, { 0xD9, 0x44, 0x24, 0x08, 0xD9, 0x59, 0x14, 0xD9, 0x51, 0x18, 0xD8, 0xD1, 0xDF, 0xE0, 0xF6, 0xC4, 0x05, 0x7A }, "animation: start and time are set" },
		{ 0x00470484, 16, { 0xD9, 0x59, 0x1C, 0xD9, 0x44, 0x24, 0x18, 0xDC, 0x35, 0xA8, 0xEE, 0x62, 0x00, 0xD9, 0x59, 0x20 }, "animation: end and speed are set" },
		{ 0x004704A4, 18, { 0x8B, 0x54, 0x24, 0x04, 0x89, 0x41, 0x24, 0x8B, 0x44, 0x24, 0x10, 0x89, 0x41, 0x28, 0xC6, 0x41, 0x2C, 0x00 }, "animation: direction and loop are set" },
		{ 0x004704B6, 6, { 0x89, 0x51, 0x30, 0xC2, 0x18, 0x00 }, "animation: the name is set" },
		{ 0x004703B0, 18, { 0x83, 0x79, 0x28, 0x00, 0x75, 0x23, 0x83, 0x79, 0x24, 0x00, 0xD9, 0x41, 0x18, 0x75, 0x15, 0xD9, 0x41, 0x1C }, "animation: is it over" },
		// the bones: the skeleton component, its skeleton, and the game's "where is this bone"
		// the way the script command Obj_GetBonePosition calls it
		{ 0x0044D81B, 14, { 0xC7, 0x46, 0x04, 0xD5, 0x56, 0x27, 0x22, 0xC7, 0x46, 0x18, 0x00, 0x00, 0x00, 0x00 }, "skeleton: the component's name" },
		{ 0x0044D41F, 10, { 0x8B, 0x4F, 0x18, 0x56, 0x50, 0xE8, 0xD7, 0x9C, 0x03, 0x00 }, "skeleton: the command's way to a bone" },
		{ 0x00487100, 16, { 0x81, 0xEC, 0xC0, 0x00, 0x00, 0x00, 0x53, 0x8B, 0x9C, 0x24, 0xC8, 0x00, 0x00, 0x00, 0x56, 0x57 }, "bone position" },
		{ 0x004871F6, 10, { 0xB0, 0x01, 0x81, 0xC4, 0xC0, 0x00, 0x00, 0x00, 0xC2, 0x08 }, "bone position: its answer" },
		// is the animation over: the first channel, unless the list is empty, and the
		// channel's own test (above)
		{ 0x004310D0, 16, { 0x8B, 0x41, 0x24, 0x83, 0x78, 0x04, 0xFF, 0x74, 0x04, 0x85, 0xC0, 0x75, 0x03, 0x32, 0xC0, 0xC3 }, "animation over" },
		{ 0x004310E0, 17, { 0x8B, 0x48, 0x04, 0x83, 0xE9, 0xFF, 0xF7, 0xD9, 0x1B, 0xC9, 0x23, 0xC8, 0xE9, 0xBF, 0xF2, 0x03, 0x00 }, "animation over: the channel's test" },
		{ 0x004703B0, 6, { 0x83, 0x79, 0x28, 0x00, 0x75, 0x23 }, "channel over: never for a loop" },
		// a script waits for the end of an animation: Obj_WaitAnimFinished makes it wait (5) on
		// the animation component, the script asks the component's wait every frame
		{ 0x0043310F, 12, { 0x8B, 0x4C, 0x24, 0x34, 0x56, 0x6A, 0x05, 0xE8, 0x65, 0xA6, 0xFD, 0xFF }, "Obj_WaitAnimFinished" },
		{ 0x0040D8CD, 14, { 0x8B, 0x8E, 0xA0, 0x00, 0x00, 0x00, 0x8B, 0x11, 0x8B, 0x42, 0x14, 0x56, 0xFF, 0xD0 }, "script: a wait on a component" },
		{ 0x0062F340, 4, { 0xA0, 0x1D, 0x43, 0x00 }, "animation component: its wait" },
		// what the script command Crouched answers
		{ 0x00567B91, 14, { 0x8B, 0x8E, 0xD0, 0x04, 0x00, 0x00, 0x33, 0xC0, 0x38, 0x41, 0x20, 0x0F, 0x95, 0xC0 }, "Crouched" },
		// the jump is the higher the longer the jump button was down, up to Skater_max_tense_time
		{ 0x005605FE, 5, { 0x68, 0x97, 0xC0, 0x33, 0xF7 }, "do_jump: Skater_max_tense_time" },
		{ 0x00560608, 6, { 0x8B, 0x8E, 0x3C, 0x03, 0x00, 0x00 }, "do_jump: how long the jump button was down" },
		{ 0x005606B0, 6, { 0xDB, 0x86, 0x3C, 0x03, 0x00, 0x00 }, "do_jump: the share of the longest" },
		// SetExtraTricks, and the check that ends them when their time is up
		{ 0x004551C1, 14, { 0xC6, 0x86, 0xC8, 0x12, 0x00, 0x00, 0x01, 0xC6, 0x86, 0xC9, 0x12, 0x00, 0x00, 0x01 }, "SetExtraTricks: on, for as long as the trick lasts" },
		{ 0x004551D8, 6, { 0x88, 0x9E, 0xC9, 0x12, 0x00, 0x00 }, "SetExtraTricks: for a Duration" },
		{ 0x004551ED, 6, { 0x89, 0x86, 0xD0, 0x12, 0x00, 0x00 }, "SetExtraTricks: from now" },
		{ 0x00455217, 6, { 0x89, 0x8E, 0xCC, 0x12, 0x00, 0x00 }, "SetExtraTricks: this long" },
		{ 0x00455245, 16, { 0xC7, 0x86, 0xD4, 0x12, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x89, 0x86, 0xD8, 0x12, 0x00, 0x00 }, "SetExtraTricks: one list, its name" },
		{ 0x0062EA00, 8, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x59, 0x40 }, "SetExtraTricks: 100 ..." },
		{ 0x0062FA28, 8, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x40 }, "SetExtraTricks: ... over 6 (frames to ms)" },
		{ 0x004550EB, 11, { 0x8B, 0xBE, 0xD0, 0x12, 0x00, 0x00, 0xE8, 0x2A, 0xDB, 0x12, 0x00 }, "extra tricks: how long they have been on" },
		{ 0x004550F6, 10, { 0x2B, 0xC7, 0x3B, 0x86, 0xCC, 0x12, 0x00, 0x00, 0x76, 0x0A }, "extra tricks: the time is up" },
		{ 0x00582C20, 14, { 0x83, 0xEC, 0x0C, 0xA1, 0x68, 0xC8, 0x7C, 0x00, 0xD9, 0x05, 0x74, 0xA2, 0x68, 0x00 }, "the game clock" },
		// the time until the skater is down, as GetAirTimeLeft asks it
		{ 0x0055E736, 13, { 0xD9, 0xE0, 0xD9, 0x1C, 0x24, 0xD9, 0x44, 0x24, 0x10, 0xD9, 0x44, 0x24, 0x0C }, "time to a height" },
		{ 0x0055E780, 4, { 0x59, 0xC2, 0x0C, 0x00 }, "time to a height: its answer" },
		{ 0x0056772C, 6, { 0xD8, 0xA2, 0x18, 0x01, 0x00, 0x00 }, "GetAirTimeLeft: the height above the ground" },
		{ 0x0056773D, 5, { 0xE8, 0xEE, 0x6F, 0xFF, 0xFF }, "GetAirTimeLeft: the time to it" },
		// an animation of the object's, and how long it is
		{ 0x00430630, 17, { 0x8B, 0x41, 0x38, 0x03, 0x44, 0x24, 0x04, 0x6A, 0x00, 0x50, 0xE8, 0x31, 0x7F, 0x04, 0x00, 0xD9, 0x00 }, "animation length" },
		{ 0x00430650, 15, { 0x8B, 0x41, 0x38, 0x03, 0x44, 0x24, 0x04, 0x6A, 0x00, 0x50, 0xE8, 0x11, 0x7F, 0x04, 0x00 }, "animation exists" },
		{ 0x0043065F, 9, { 0x83, 0xC4, 0x08, 0x85, 0xC0, 0x0F, 0x95, 0xC0, 0xC2 }, "animation exists: its answer" },
		{ 0x00470410, 10, { 0x51, 0xD9, 0x41, 0x20, 0xDC, 0x0D, 0xA8, 0xEE, 0x62, 0x00 }, "channel speed" },
		{ 0x0062EEA8, 8, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4E, 0x40 }, "channel speed: per 60th of a second" },
		// a script calls another: the name of the one running goes into the next place for a
		// call, and the return takes it back from the last one
		{ 0x0040E7E6, 14, { 0x8B, 0x4D, 0x30, 0x8B, 0x95, 0xBC, 0x00, 0x00, 0x00, 0x8B, 0xC1, 0x6B, 0xC0, 0x2C }, "script call: the depth, the name" },
		{ 0x0040E7F4, 14, { 0x03, 0x85, 0x8C, 0x00, 0x00, 0x00, 0x83, 0xC1, 0x01, 0x89, 0x4D, 0x30, 0x89, 0x10 }, "script call: the place for it" },
		{ 0x0040EFF9, 15, { 0x8B, 0x7E, 0x30, 0x6B, 0xFF, 0x2C, 0x03, 0xBE, 0x8C, 0x00, 0x00, 0x00, 0x53, 0x8B, 0x0F }, "script return: the last call" },
		{ 0x0040F008, 6, { 0x89, 0x8E, 0xBC, 0x00, 0x00, 0x00 }, "script return: the name" },
		{ 0x0040E7A8, 5, { 0x68, 0xC0, 0x02, 0x00, 0x00 }, "script call: room for 16 calls" },
		// the game's own "is he skating" test: the two offsets ReadSkater uses for it
		{ 0x0044F8A0, 18, { 0x8B, 0x41, 0x2C, 0x80, 0xB8, 0x14, 0x01, 0x00, 0x00, 0x00, 0x75, 0x0C, 0x83, 0x78, 0x1C, 0x00, 0x75, 0x06 }, "skating test" },
		// the stance commands of SkaterStancePanel: which name goes where, the byte that InPressure
		// reads and PressureOff clears, and the type the component is made with
		{ 0x00578BE0, 18, { 0x8B, 0x44, 0x24, 0x04, 0x3D, 0xD6, 0x7D, 0xB5, 0x71, 0x53, 0xBB, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x87, 0x97 }, "stance commands" },
		{ 0x00578BF5, 16, { 0x74, 0x71, 0x3D, 0x10, 0xD7, 0x23, 0x0A, 0x74, 0x46, 0x3D, 0xCE, 0x1D, 0xB6, 0x1E, 0x74, 0x33 }, "stance commands: pressure, nollie" },
		{ 0x00578C68, 15, { 0x38, 0x59, 0x1E, 0x75, 0x54, 0xD9, 0xEE, 0x51, 0xD9, 0x1C, 0x24, 0xC6, 0x41, 0x1E, 0x00 }, "PressureOff" },
		{ 0x00578C8C, 7, { 0x3D, 0x0B, 0x9D, 0xAB, 0x9F, 0x74, 0x34 }, "stance commands: InPressure" },
		{ 0x00578CC7, 12, { 0x33, 0xC0, 0x38, 0x41, 0x1E, 0x5B, 0x0F, 0x95, 0xC0, 0xC2, 0x0C, 0x00 }, "InPressure" },
		{ 0x00578D25, 13, { 0xC7, 0x06, 0x40, 0x72, 0x64, 0x00, 0xC7, 0x46, 0x04, 0x01, 0x33, 0xFC, 0x21 }, "stance component: its type" },
		// the foot forward: a skater starts flipped when his profile is regular, not when it is goofy;
		// riding switch is the game's own test, which the script command Switched answers with
		{ 0x00517122, 18, { 0x80, 0xBE, 0x34, 0x01, 0x00, 0x00, 0x00, 0x8B, 0x8E, 0x7C, 0x02, 0x00, 0x00, 0x8B, 0xB9, 0xD0, 0x04, 0x00 }, "a skater's start: goofy" },
		{ 0x00517135, 12, { 0x0F, 0x94, 0xC0, 0x83, 0xC7, 0x28, 0x38, 0x07, 0x74, 0x0A, 0x88, 0x07 }, "a skater's start: flipped unless goofy" },
		{ 0x00451FE0, 18, { 0x8B, 0x41, 0x0C, 0x80, 0xB8, 0x34, 0x01, 0x00, 0x00, 0x00, 0x74, 0x0B, 0x8B, 0x89, 0xD0, 0x04, 0x00, 0x00 }, "IsSwitched: goofy" },
		{ 0x00451FF2, 18, { 0x0F, 0xB6, 0x41, 0x28, 0xC3, 0x8B, 0x91, 0xD0, 0x04, 0x00, 0x00, 0x33, 0xC0, 0x38, 0x42, 0x28, 0x0F, 0x94 }, "IsSwitched: flipped" },
		{ 0x00568248, 17, { 0x81, 0xFF, 0x0B, 0xB8, 0x66, 0x8F, 0x0F, 0x85, 0xA4, 0x09, 0x00, 0x00, 0xE8, 0x87, 0x9D, 0xEE, 0xFF }, "Switched" },
		// the balance of a rail, started the way a grind script starts it: the balance component's
		// script commands in the slot of its table that a script's command goes through,
		// DoBalanceTrick among them, the names it reads (the two buttons, the type, the flag
		// DoFlipCheck), that it does nothing for the balance that is on already, the records of a
		// slide and a grind it starts, and the three it pops
		{ 0x00646D18, 4, { 0x70, 0xC3, 0x55, 0x00 }, "balance component: its script commands" },
		{ 0x0055C370, 17, { 0x8B, 0x44, 0x24, 0x04, 0x83, 0xEC, 0x14, 0x3D, 0x4B, 0xCD, 0x0D, 0x7A, 0x53, 0x56, 0x57, 0x8B, 0xF1 }, "balance commands" },
		{ 0x0055C5DF, 11, { 0x3D, 0x0C, 0x67, 0x92, 0x7E, 0x0F, 0x84, 0xAD, 0x00, 0x00, 0x00 }, "balance commands: DoBalanceTrick" },
		{ 0x0055C6F3, 12, { 0x6A, 0x01, 0x8D, 0x54, 0x24, 0x14, 0x52, 0x68, 0x01, 0x0B, 0x1A, 0x4F }, "DoBalanceTrick: ButtonA" },
		{ 0x0055C70E, 12, { 0x6A, 0x01, 0x8D, 0x44, 0x24, 0x10, 0x50, 0x68, 0xBB, 0x5A, 0x13, 0xD6 }, "DoBalanceTrick: ButtonB" },
		{ 0x0055C721, 11, { 0x53, 0x8D, 0x4C, 0x24, 0x1C, 0x51, 0x68, 0xD6, 0xA8, 0x21, 0x73 }, "DoBalanceTrick: Type" },
		{ 0x0055C737, 13, { 0x8B, 0x44, 0x24, 0x18, 0x39, 0x46, 0x18, 0x0F, 0x84, 0x20, 0x01, 0x00, 0x00 }, "DoBalanceTrick: nothing for the balance that is on" },
		{ 0x0055C744, 10, { 0x68, 0x77, 0xE6, 0x04, 0x13, 0x8B, 0xCF, 0x89, 0x46, 0x18 }, "DoBalanceTrick: DoFlipCheck, the balance that is on now" },
		{ 0x0055C773, 9, { 0x8B, 0x4E, 0x18, 0x81, 0xF9, 0x9D, 0x11, 0x10, 0x8D }, "DoBalanceTrick: a slide" },
		{ 0x0055C788, 6, { 0x81, 0xF9, 0x6F, 0xD8, 0x5E, 0x25 }, "DoBalanceTrick: a grind" },
		{ 0x0055C864, 14, { 0x5F, 0x5E, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x5B, 0x83, 0xC4, 0x14, 0xC2, 0x0C, 0x00 }, "balance commands pop three" },
		// a structure of parameters: one word, the head of its list. An item: a type (0x0D a
		// checksum), its name, its value; a flag is a checksum with no name. Adding a checksum, and
		// giving every item back.
		{ 0x00430620, 9, { 0x8B, 0xC1, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3 }, "CStruct: made empty" },
		{ 0x004112B0, 16, { 0x83, 0x7E, 0x04, 0x00, 0x75, 0x2C, 0x80, 0x3E, 0x0D, 0x75, 0x27, 0x8B, 0x46, 0x08, 0x3B, 0xC7 }, "CStruct: a flag" },
		{ 0x004123F0, 12, { 0x6A, 0xFF, 0x68, 0x9B, 0x19, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "CStruct::AddChecksum" },
		{ 0x0041246D, 3, { 0x89, 0x48, 0x04 }, "CStruct::AddChecksum: the name" },
		{ 0x0041247B, 6, { 0xC6, 0x00, 0x0D, 0x89, 0x50, 0x08 }, "CStruct::AddChecksum: a checksum, its value" },
		{ 0x00412493, 6, { 0x83, 0xC4, 0x10, 0xC2, 0x08, 0x00 }, "CStruct::AddChecksum pops two" },
		{ 0x00411790, 12, { 0x55, 0x8B, 0xE9, 0x56, 0x8B, 0x75, 0x00, 0x85, 0xF6, 0x74, 0x74, 0x53 }, "CStruct::Clear" },
		{ 0x00411810, 9, { 0xC7, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D, 0xC3 }, "CStruct::Clear: empty, pops none" },
		// where a script returns from a call: the parameters of the script it goes back to; and the
		// rail state, which stick_to_rail sets before it has the grind picked
		{ 0x0040F00E, 6, { 0x8B, 0x57, 0x04, 0x89, 0x56, 0x14 }, "script return: the parameters" },
		{ 0x0056A7D5, 9, { 0x6A, 0x04, 0x8B, 0xCE, 0xE8, 0x52, 0x26, 0xFF, 0xFF }, "stick_to_rail: on the rail" },
		{ 0x0055CE38, 9, { 0x8B, 0x86, 0xD0, 0x04, 0x00, 0x00, 0x83, 0xC0, 0x18 }, "set_state: the state" },
		// the way off a rail (StartRailExit): the physics component's line test, from +0x208 to
		// +0x218 through its own feeler, true when it hits, two to pop; the game's test for a ledge
		// next to a rail at the start of a grind, which goes through it with the three names; a whole
		// number into a structure; and how the trick component starts a button's trick: the flips and
		// turns left for after, the script, at once, and he is doing a trick
		{ 0x005602F0, 18, { 0x83, 0xEC, 0x10, 0x53, 0x56, 0x8B, 0xF1, 0x57, 0x8D, 0x86, 0x18, 0x02, 0x00, 0x00, 0x50, 0x8D, 0x8E, 0x08 }, "line test" },
		{ 0x00560302, 10, { 0x02, 0x00, 0x00, 0x8D, 0xBE, 0xA8, 0x00, 0x00, 0x00, 0x51 }, "line test: the physics' own feeler" },
		{ 0x00560336, 13, { 0x75, 0x0B, 0x5F, 0x5E, 0x32, 0xC0, 0x5B, 0x83, 0xC4, 0x10, 0xC2, 0x08, 0x00 }, "line test: no hit, pops two" },
		{ 0x0056AAFB, 12, { 0x6A, 0x00, 0x68, 0x35, 0x92, 0xD7, 0xE4, 0xE8, 0x09, 0xB6, 0xFA, 0xFF }, "ledge test: Physics_Ground_Snap_Up" },
		{ 0x0056AB10, 13, { 0x6A, 0x00, 0xD9, 0x96, 0x08, 0x02, 0x00, 0x00, 0x68, 0x5C, 0xED, 0xD7, 0x9C }, "ledge test: Rail_Bad_Ledge_Drop_Down_Dist, the line" },
		{ 0x0056AB3D, 14, { 0x6A, 0x00, 0xD9, 0x9C, 0x24, 0x9C, 0x00, 0x00, 0x00, 0x68, 0x52, 0x97, 0x66, 0x31 }, "ledge test: Rail_Bad_Ledge_Side_Dist" },
		{ 0x0056ADD7, 9, { 0x6A, 0x10, 0x8B, 0xCE, 0xE8, 0x10, 0x55, 0xFF, 0xFF }, "ledge test: the line test" },
		{ 0x00412230, 12, { 0x6A, 0xFF, 0x68, 0x9B, 0x19, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "CStruct::AddInteger" },
		{ 0x004122A5, 11, { 0x8B, 0x4C, 0x24, 0x1C, 0x8B, 0x54, 0x24, 0x20, 0x89, 0x48, 0x04 }, "CStruct::AddInteger: the name" },
		{ 0x004122BB, 6, { 0xC6, 0x00, 0x01, 0x89, 0x50, 0x08 }, "CStruct::AddInteger: a whole number, its value" },
		{ 0x004122D3, 6, { 0x83, 0xC4, 0x10, 0xC2, 0x08, 0x00 }, "CStruct::AddInteger pops two" },
		{ 0x00455546, 8, { 0x8B, 0x4E, 0x2C, 0xE8, 0x12, 0xB0, 0x11, 0x00 }, "a button's trick: the afters" },
		{ 0x0045554E, 17, { 0x8B, 0x54, 0x24, 0x24, 0x8B, 0x44, 0x24, 0x10, 0x52, 0x50, 0x8B, 0xCF, 0xE8, 0x51, 0x95, 0xFC, 0xFF }, "a button's trick: its script" },
		{ 0x0045555F, 8, { 0x8B, 0x4F, 0x10, 0xE8, 0x39, 0x9B, 0xFB, 0xFF }, "a button's trick: run at once" },
		{ 0x00455567, 10, { 0x8B, 0x4E, 0x30, 0xC6, 0x81, 0xD8, 0x00, 0x00, 0x00, 0x01 }, "a button's trick: doing a trick" },
	};

	static bool s_checked = false;
	static bool s_available = false;
	static bool s_installed = false;

	bool Available() {
		if (s_checked) {
			return s_available;
		}
		s_checked = true;
		s_available = true;
		for (size_t i = 0; i < sizeof(kExpect) / sizeof(kExpect[0]); i++) {
			if (memcmp((const void*)kExpect[i].address, kExpect[i].bytes, (size_t)kExpect[i].size) != 0) {
				Log::TypedLog(CHN_SDL, "Flickit: the game's code at %08X (%s) is not what was expected\n", kExpect[i].address, kExpect[i].what);
				s_available = false;
			}
		}
		// The checksums in this file are the game's (and the code that computes them is right)
		if (Flickit::NameChecksum("SkaterState") != kSkaterState || Flickit::NameChecksum("SkaterFlipAndRotate") != kSkaterFlipAndRotate
			|| Flickit::NameChecksum("SkaterCorePhysics") != kSkaterCorePhysics || Flickit::NameChecksum("slide") != kSlideBalance
			|| Flickit::NameChecksum("Trick") != kTrick || Flickit::NameChecksum("Square") != kSquare
			|| Flickit::NameChecksum("Physics_Rolling_Friction") != kRollingFriction
			|| Flickit::NameChecksum("OnGroundNollieAI") != kNollieLoop || Flickit::NameChecksum("Physics_Air_Rotation_stat") != kAirRotation
			|| Flickit::NameChecksum("Animation") != kAnimation || Flickit::NameChecksum("Skeleton") != kSkeleton
			|| Flickit::NameChecksum("Bone_Toe_L") != kToeLeft || Flickit::NameChecksum("Bone_Toe_R") != kToeRight
			|| Flickit::NameChecksum("SkaterStancePanel") != kStancePanel) {
			s_available = false;
		}
		if (Flickit::NameChecksum("grind") != kGrindBalance || Flickit::NameChecksum("Grind") != kGrindBalance
			|| Flickit::NameChecksum("DoBalanceTrick") != kDoBalanceTrick || Flickit::NameChecksum("ButtonA") != kButtonA
			|| Flickit::NameChecksum("ButtonB") != kButtonB || Flickit::NameChecksum("Right") != kRight
			|| Flickit::NameChecksum("Left") != kLeft || Flickit::NameChecksum("Type") != kType
			|| Flickit::NameChecksum("DoFlipCheck") != kDoFlipCheck) {
			s_available = false;
		}
		if (Flickit::NameChecksum("SkateInOrBail") != kSkateInOrBail || Flickit::NameChecksum("FallingLeft") != kFallingLeft
			|| Flickit::NameChecksum("Fallingright") != kFallingRight || Flickit::NameChecksum("GrindBail") != kGrindBail
			|| Flickit::NameChecksum("Airborne") != kAirborne || Flickit::NameChecksum("moveright") != kMoveRight
			|| Flickit::NameChecksum("movey") != kMoveY || Flickit::NameChecksum("Physics_Ground_Snap_Up") != kGroundSnapUp
			|| Flickit::NameChecksum("Rail_Bad_Ledge_Drop_Down_Dist") != kBadLedgeDown
			|| Flickit::NameChecksum("Rail_Bad_Ledge_Side_Dist") != kBadLedgeSide) {
			s_available = false;
		}
		for (int i = 0; i < PUSH_CLIPS; i++) {
			if (Flickit::NameChecksum(kPushClips[i].text) != kPushClips[i].name) {
				s_available = false;
			}
		}
		for (int i = 0; i < PUSH_CUTS; i++) {
			if (Flickit::NameChecksum(kPushCuts[i].text) != kPushCuts[i].name) {
				s_available = false;
			}
		}
		// the crouch pose's names (flickit_pose.h) and the ones started under a crouch
		for (int i = 0; i < FlickitPose::CROUCH_ANIMS; i++) {
			if (Flickit::NameChecksum(FlickitPose::kCrouchAnims[i].name) != FlickitPose::kCrouchAnims[i].crc) {
				s_available = false;
			}
		}
		for (int i = 0; i < FlickitPose::STAND_ANIMS; i++) {
			if (Flickit::NameChecksum(FlickitPose::kStandAnims[i].name) != FlickitPose::kStandAnims[i].crc) {
				s_available = false;
			}
		}
		if (Flickit::NameChecksum("Idle") != kStandIdle || Flickit::NameChecksum("NollieSkatingIdle") != kStandNollieIdle) {
			s_available = false;
		}
		for (int i = 0; i < HELD_FLIPS; i++) {
			if (Flickit::NameChecksum(kHeldFlips[i].text) != kHeldFlips[i].extra || Flickit::NameChecksum(kHeldFlips[i].animText) != kHeldFlips[i].anim
				|| Flickit::NameChecksum(kHeldFlips[i].nollieText) != kHeldFlips[i].nollie) {
				s_available = false;
			}
		}
		static const struct { uint32_t name; const char* text; } scripts[] = {
			{ kGroundLoop, "OnGroundAi" }, { kNollieLoop, "OnGroundNollieAI" }, { kWaitOneFrame, "WaitOneGameFrame" },
			{ kWaitAnimChecking, "WaitAnimWhilstChecking" }, { kLand2, "Land2" }, { kSkateInLand, "SkateInLand" },
			{ kFlailLeft, "FlailLeft" }, { kFlailRight, "FlailRight" },
		};
		for (size_t i = 0; i < sizeof(scripts) / sizeof(scripts[0]); i++) {
			if (Flickit::NameChecksum(scripts[i].text) != scripts[i].name) {
				s_available = false;
			}
		}
		if (!s_available) {
			Log::TypedLog(CHN_SDL, "Flickit: this is not the game the grind hooks were made for. They stay off, grinds work with the timed Grind hold\n");
		}
		return s_available;
	}

	bool Installed() {
		return s_installed;
	}

	/******************************************************************/
	/*                                                                */
	/*  What Flickit wants on this frame                              */
	/******************************************************************/

	static bool s_active = false;
	static Frame s_frame;
	// Frames handed over so far, for how fresh something said on one of them is
	static uint32_t s_frames = 0;
	// A crouch that ended without a pop: the game is to see no jump in it (PadUpdateHook)
	static bool s_standUp = false;
	// The board is out of speed on a rail: the recognizer asks for the way off on this frame (RailExitFrame)
	static bool s_exitAsked = false;
	// A pop out of a crouch: how deep the crouch was (Flickit::Output::popStrength), and on which frame
	static float s_popStrength = -1.0f;
	static uint32_t s_popFrame = 0;
	// A jump from such a pop gets this share of a full one (JumpSpeedHook)
	static float s_popShare = 1.0f;
	static const uint32_t kPopFresh = 30;

	static bool s_hasPick = false;
	static Pick s_pick;
	// The stance the stick asks for (Flickit::StanceKeeper), and for the log the last change it made:
	// to which stance, whether it took him out of pressure, whether it was a nollie that went nowhere
	static Flickit::StanceKeeper s_stance;
	static uint32_t s_stanceSerial = 0;
	static int s_stanceTo = Flickit::STANCE_ANY;
	static bool s_stancePressure = false;
	static bool s_stanceLate = false;

	static void HoldAutoKick(bool off);
	static void WatchStick(const Frame& frame);

	void SetFrame(const Frame& frame) {
		s_frame = frame;
		s_active = true;
		s_frames++;
		if (frame.standUp) {
			s_standUp = true;
		}
		if (frame.railExit) {
			s_exitAsked = true;
		}
		s_stance.Ask(frame.stance);
		if (frame.popStrength >= 0.0f) {
			s_popStrength = frame.popStrength;
			s_popFrame = s_frames;
		}
		// With Flickit's push the skater does not kick by himself
		HoldAutoKick(frame.push > 0);
		// The stick is watched on the ground too: a flick just before a jump makes the spin
		WatchStick(frame);
	}

	bool TakePick(Pick& out) {
		if (!s_hasPick) {
			return false;
		}
		out = s_pick;
		s_hasPick = false;
		return true;
	}

	static inline uint8_t* LocalSkater() {
		void* skate = *kSkate;
		return skate ? (uint8_t*)GetLocalSkater(skate) : 0;
	}

	// Is this physics component the one of the skater Flickit drives?
	static inline bool IsLocal(const uint8_t* component) {
		const uint8_t* object = *(uint8_t* const*)(component + COMPONENT_OBJECT);
		return object != 0 && object == LocalSkater();
	}

	/******************************************************************/
	/*                                                                */
	/*  The stay on a rail                                            */
	/******************************************************************/

	struct RailStay {
		Flickit::RailRun run;   // how long, how fast, and whether the speed is gone
		bool hasDirection;      // the direction of travel on the frame before
		float direction[3];
		bool hasFrom;           // which way the board came onto the rail from: from the rail to that side, level
		float from[3];
		bool hasPick;           // the grind Flickit's pick started on this rail
		int pick;
		int side;               // that side seen from the skater: -1 left, 1 right, 0 not known
		int terrain;
		int surface;
		int dragClass;
	};
	static RailStay s_rail;

	// Rails that catch by themselves, within Skate's reach
	struct Reach {
		bool byItself;              // the catch test just before said yes without the Grind button being down
		bool found;                 // the search that followed took a rail; the game may still turn it down
		Flickit::RailPass pass;     // how the board passed it
		uint32_t catches;           // rails the board went onto that way, for the log
		Flickit::RailPass caught;
		bool tookRail;              // in this flight
		bool turnedDown;            // in this flight the game would not have a rail that was within reach (the one just left, mostly)
		Flickit::RailPass nearest;  // the rail that came closest in this flight without being within reach
		uint32_t misses;            // flights that ended on the ground with such a rail close by
		Flickit::RailPass missed;
	};
	static Reach s_reach;
	// A rail that missed by more than this is not worth a line in the log
	static const float kWorthTelling = 2.5f;

	// A board that stepped off a rail because it had stopped there stands right next to that rail.
	// Until it has been on the ground, or a few seconds have passed, rails do not catch by themselves.
	static bool s_noCatch = false;
	static int s_noCatchPolls = 0;
	static const int kNoCatchPolls = 240;

	// Before a catch test, and at the start of a frame: what the last search found has been dealt
	// with. A rail that is still marked as found was not taken by the game.
	static void ReachForget() {
		if (s_reach.found) {
			s_reach.turnedDown = true;
		}
		s_reach.byItself = false;
		s_reach.found = false;
	}

	// Rolling: the pushes and the turning of the skater Flickit drives
	static Flickit::PushRun s_push;
	static Flickit::Steer s_steer;
	static bool s_pushAsked = false;     // since the last look the game's ground physics have asked about a kick
	static int s_pushIdle = 0;           // looks in a row without that
	static const int kPushIdleMost = 3;
	static bool s_groundSeen = false;    // since the last look the game's ground physics have run for him at all
	static int s_groundIdle = kPushIdleMost + 1;   // looks in a row without that
	// The animation the skater was in when the ground physics last looked, for telling when the
	// game starts one. The channel is only ever compared, never followed.
	static const uint8_t* s_animChannel = 0;
	static uint32_t s_animName = 0;
	static float s_animTime = 0.0f;
	static uint32_t s_animPlay = 0;      // goes up with every animation the game is seen to start
	// The push that is under way, or the last one: the play of the animation it goes with,
	// which animation that is, how long, and where the word on its foot came from
	static uint32_t s_pushPlay = 0;
	static int s_pushClip = 0;
	static float s_pushLength = 0.0f;
	static int s_pushSource = PUSH_AS_SET;
	// What the feet did in each push animation, once a play of it has been watched to its end
	struct FootKnown {
		bool have;
		Flickit::PushProfile profile;
		uint32_t stamp;                  // the later it was watched, the bigger
		int told;                        // what the log was last told about it: a Flickit::FootWhy plus one, 0 nothing yet
		bool traced;                     // the log has had every look at its feet once
	};
	static FootKnown s_footKnown[PUSH_CLIPS];
	static uint32_t s_footStamp = 0;
	// The play that is being watched
	static Flickit::FootWatch s_foot;
	static bool s_footWatching = false;
	static uint32_t s_footPlay = 0;
	static int s_footClip = 0;
	static float s_footLength = 0.0f;
	static float s_footFirst = 0.0f;     // how far into the animation the first frame of the watch was, and the last
	static float s_footLast = 0.0f;
	// For the log: goes up when a play has told something new, and what
	static uint32_t s_footSerial = 0;
	static int s_footToldClip = 0;
	static float s_footToldLength = 0.0f;
	static Flickit::FootResult s_footTold;
	// and with it, the first time an animation was watched whole, every look at its feet
	enum { FOOT_TRACE_LOOKS = 128 };
	static float s_footTrace[FOOT_TRACE_LOOKS * 7];
	static int s_footTraceLooks = 0;
	// A foot that pushes this much earlier or later than last time (as a share of the animation) is news
	static const float kFootNews = 0.03f;
	// For the log: goes up when a push has cut one of the game's animations short, which, and how far it had got
	static uint32_t s_cutSerial = 0;
	static int s_cutClip = 0;
	static float s_cutShare = 0.0f;
	// and when a push animation has given way to a crouch or to Down: which, to what, how far it had got
	static uint32_t s_waySerial = 0;
	static const char* s_wayAnim = "";
	static int s_wayTo = 0;
	static float s_wayShare = 0.0f;
	// The crouch shown as deep as the stick's (flickit_pose.h); the game's clock on the frame; for
	// the log, when a crouch began to show it and from what, and when the game dropped it
	static FlickitPose::Keeper s_keeper;
	static uint32_t s_poseNow = 0;
	static uint32_t s_poseSerial = 0;
	static uint32_t s_poseFrom = 0;
	static uint32_t s_poseLostSerial = 0;
	static uint32_t s_poseMissSerial = 0;
	static uint32_t s_poseMissAnim = 0;
	static int s_poseMissChannels = 0;
	static uint32_t s_poseMissLast = 0;
	static int s_poseMissBlending = 0;
	// and when a standing pose was started under a crouch that had none, under what
	static uint32_t s_poseUnderSerial = 0;
	static uint32_t s_poseUnderCrouch = 0;
	static uint32_t s_poseUnderStand = 0;
	// The clock that stands in where the game is told to play a push and no push animation is
	// seen on the skater (Flickit::PushClock), and for the log the animation he was in instead
	// when it took over
	static Flickit::PushClock s_clock;
	static uint32_t s_clockSerial = 0;
	static uint32_t s_clockAnim = 0;
	// Spinning: the turning in the air of the skater Flickit drives
	static Flickit::Spin s_spin;
	static bool s_airSeen = false;       // since the last look the game's physics of the air have turned him
	static int s_airIdle = 0;            // looks in a row without that, while Flickit's turning is under way
	static bool s_spinning = false;      // the game's turning in the air is running for him right now, ...
	static float s_spinRate = 0.0f;      // ... and this is how fast Flickit has him turn (rad/s)
	static uint32_t s_spinSerial = 0;    // for the log: goes up when an air with Flickit's turning is over
	static float s_spinTurned = 0.0f;    // what that air came to: radians asked for, to the right when positive,
	static float s_spinFlick = 0.0f;     // the flick that counted (0 to 1),
	static float s_spinSeconds = 0.0f;   // and how long it lasted
	// The skater whose "auto kick" is held at "off", so that he does not kick by himself
	static uint8_t* s_autoKickHeld = 0;
	// Left and Right, kept from the game's scripts for the frame in which a push asks for its
	// animation: the input component they were taken out of (0: nothing is kept), and what they
	// were. The pointer is only ever compared, never followed: the skater may be gone by the time
	// it is looked at again.
	static const uint8_t* s_turnKept = 0;
	static uint8_t s_turnLeft = 0;
	static uint8_t s_turnRight = 0;

	// The physics of this skater are about to run: Left and Right are what the player holds again
	static inline void GiveBackTurn(const uint8_t* core) {
		if (!s_turnKept) {
			return;
		}
		uint8_t* input = *(uint8_t* const*)(core + CORE_INPUT);
		if (input && input == s_turnKept) {
			input[INPUT_LEFT_PRESSED] = s_turnLeft;
			input[INPUT_RIGHT_PRESSED] = s_turnRight;
			s_turnKept = 0;
		}
	}

	// The same before the game takes its next look at the controller, for a frame on which no
	// physics came by to give them back. The game tells a new press by this very byte: left at
	// "not down" it would take a held Left for one that has just been pressed.
	static void GiveBackTurnNow() {
		if (!s_turnKept) {
			return;     // nothing is kept (and the game is not asked for anything)
		}
		uint8_t* skater = LocalSkater();
		const uint8_t* core = skater ? (const uint8_t*)GetComponent(skater, kSkaterCorePhysics) : 0;
		if (core) {
			GiveBackTurn(core);
		}
		s_turnKept = 0;
	}

	// Flickit's push needs the game's "auto kick" off: the skater then kicks only when asked, and
	// the game's scripts play the push only when asked. The game's own option is put back the way
	// its script command RestoreAutoKick does it, as soon as the push is not Flickit's any more.
	static void HoldAutoKick(bool off) {
		if (!off && !s_autoKickHeld) {
			return;     // nothing to put back (and the game is not asked for anything)
		}
		uint8_t* skater = LocalSkater();
		uint8_t* core = skater ? (uint8_t*)GetComponent(skater, kSkaterCorePhysics) : 0;
		if (off) {
			if (core) {
				core[CORE_AUTO_KICK] = 0;
				s_autoKickHeld = skater;
			}
			return;
		}
		const uint8_t* skate = (const uint8_t*)*kSkate;
		if (core && skate && skater == s_autoKickHeld) {
			const int32_t player = *(const int32_t*)(skater + OBJECT_PLAYER);
			if (player >= 0 && player < 8) {
				core[CORE_AUTO_KICK] = skate[SKATE_AUTO_KICK + player * SKATE_PLAYER_SIZE];
			}
		}
		s_autoKickHeld = 0;
	}

	// Once per look at the controller, while Flickit drives. In the air the game's physics step
	// the turning (AirTurnHook); everywhere else the stick is only watched.
	static void WatchStick(const Frame& frame) {
		if (frame.spin <= 0) {
			s_spin.Reset();
			s_airSeen = false;
			s_airIdle = 0;
			return;
		}
		if (s_airSeen) {
			s_airSeen = false;
			s_airIdle = 0;
			return;
		}
		if (s_spin.InAir()) {
			if (++s_airIdle <= kPushIdleMost) {
				return;     // one look without the physics of the air is not a landing yet
			}
			// The air is over: what it came to, for the log
			s_spinTurned = s_spin.Turned();
			s_spinFlick = s_spin.Flick();
			s_spinSeconds = s_spin.Seconds();
			s_spinSerial++;
			s_spin.Land();
		}
		s_airIdle = 0;
		s_spin.Feed(frame.seconds, frame.steer);
	}

	void BeginFrame() {
		if (!s_active) {
			// Flickit did not drive the skater on the last frame: the kick is the game's again
			HoldAutoKick(false);
			s_push.Reset();
			s_footWatching = false;
			s_clock.Reset();
			s_steer.Reset();
			s_spin.Reset();
			s_airSeen = false;
			s_airIdle = 0;
			s_stance.Reset();
		}
		GiveBackTurnNow();
		s_active = false;
		s_exitAsked = false;
		ReachForget();
	}

	static void RailForget() {
		s_rail.run.Begin();
		s_rail.hasDirection = false;
		s_rail.hasFrom = false;
		s_rail.hasPick = false;
		s_rail.side = 0;
	}

	/******************************************************************/
	/*                                                                */
	/*  The grind in progress, as its script reports it               */
	/******************************************************************/

	static int s_grind = Flickit::GRIND_UNKNOWN;
	static bool s_grindReady = false;
	static uint32_t s_grindSerial = 1;
	static int s_grindCalls = 0;

	static uint8_t* s_lastSkater = 0;
	static int s_lastState = Flickit::STATE_UNKNOWN;

	static void NewGrind() {
		s_grind = Flickit::GRIND_UNKNOWN;
		s_grindReady = false;
		s_grindCalls = 0;
		s_grindSerial++;
	}

	// The game's Grind script runs "IF IsPs2" at two points: 15 frames after the grind started, and
	// when its first animation is over, right before it sets the button combinations that switch to
	// another grind (SetExtraTricks). A grind that was started by such a switch runs the same script
	// from the top. So the calls come in pairs, and the second of a pair means: switches are taken.
	void OnGrindScript(void* script) {
		if (!s_available || !script) {
			return;
		}
		const uint8_t* skater = LocalSkater();
		if (!skater || *(void* const*)(skater + OBJECT_SCRIPT) != script) {
			return;
		}

		if (s_grindCalls >= 2) {
			// A third call: the grind was switched and this is the new one
			NewGrind();
			s_rail.hasPick = false;
		}
		s_grindCalls++;
		s_grindReady = (s_grindCalls >= 2);

		// Which grind it is shows in the animation the script was given
		uint32_t anim = 0;
		void* params = *(void* const*)((const uint8_t*)script + SCRIPT_PARAMS);
		if (params) {
			StructGetChecksum(params, kAnim, &anim, false);
		}
		if (anim == kTailGrindRange) {
			s_grind = Flickit::GRIND_FIVE_O;
		}
		else if (anim == kNoseGrindRange) {
			s_grind = Flickit::GRIND_NOSEGRIND;
		}
		else if (anim == kFiftyFiftyRange || anim == kFiftyFifty2Range || anim == kFiftyFifty3Range) {
			s_grind = Flickit::GRIND_FIFTY;
		}
		else {
			s_grind = Flickit::GRIND_OTHER;
		}
	}

	// Where an object's script is: the name of the script that is running, and of the one that
	// called it (0 when none did)
	struct ScriptAt {
		uint32_t running;
		uint32_t caller;
	};
	static ScriptAt ScriptOf(const uint8_t* object) {
		ScriptAt at = { 0, 0 };
		const uint8_t* script = *(const uint8_t* const*)(object + OBJECT_SCRIPT);
		if (!script) {
			return at;
		}
		at.running = *(const uint32_t*)(script + SCRIPT_NAME);
		const int32_t depth = *(const int32_t*)(script + SCRIPT_DEPTH);
		const uint8_t* calls = *(const uint8_t* const*)(script + SCRIPT_CALLS);
		if (calls && depth > 0 && depth <= SCRIPT_CALLS_MOST) {
			at.caller = *(const uint32_t*)(calls + (depth - 1) * SCRIPT_CALL_SIZE);
		}
		return at;
	}

	// A push button pushes: the skater is on his board on the ground with nothing balanced (the
	// game does not kick in a manual either), he is not in a bail, no script has forbidden
	// kicking and none has taken the controls away
	static bool CanPush(const uint8_t* skater) {
		const uint8_t* state = (const uint8_t*)GetComponent((void*)skater, kSkaterState);
		const uint8_t* core = (const uint8_t*)GetComponent((void*)skater, kSkaterCorePhysics);
		const uint8_t* balance = core ? *(const uint8_t* const*)(core + CORE_BALANCE) : 0;
		const uint8_t* input = core ? *(const uint8_t* const*)(core + CORE_INPUT) : 0;
		return state && *(const int32_t*)(state + STATE_STATE) == 0 && state[STATE_DRIVING] == 0
			&& *(const int32_t*)(state + STATE_PHYSICS) == 0 && state[STATE_BAILING] == 0
			&& balance != 0 && *(const uint32_t*)(balance + BALANCE_TYPE) == 0 && core[CORE_NO_KICK] == 0
			&& input != 0 && input[INPUT_DISABLED] == 0 && input[INPUT_NET_DISABLED] == 0;
	}

	// He crouches: the jump button is down on the ground (Flickit holds it while the stick is
	// pulled back for an ollie)
	static bool Crouching(const uint8_t* skater) {
		const uint8_t* state = (const uint8_t*)GetComponent((void*)skater, kSkaterState);
		return state && state[STATE_CROUCHED] != 0;
	}

	static const HeldFlip* HeldFlipOf(uint32_t extra) {
		for (int i = 0; i < HELD_FLIPS; i++) {
			if (kHeldFlips[i].extra == extra) {
				return &kHeldFlips[i];
			}
		}
		return 0;
	}

	// Pressing for another rotation of a flip trick: not before this long into the trick (so that
	// the stick has been held), and not later than this before the game stops taking the press
	static const int32_t kHeldDecideMs = 120;
	static const int32_t kHeldLeaveMs = 70;
	// A flip trick bails a landing until its animation is this close to its end (FlipTrick:
	// grindslack, 25 frames of the animation), and a rotation more is only pressed for with this
	// much air to spare beyond that
	static const float kFlipSlack = 25.0f / 60.0f;
	static const float kFlipSpare = 2.0f / 60.0f;

	// In the air: can the flip trick the skater is in be taken round once more now? It can when
	// the game has given it a trick to turn into on another press of the trick button, that trick
	// is the same one with one rotation more (kHeldFlips), the game still takes the press, and
	// the air left (as the script command GetAirTimeLeft has it) lasts until that trick's
	// animation no longer bails a landing. Skate keeps a held kickflip going on the same terms:
	// another rotation while there is air enough to land it.
	static void ReadHeldFlip(uint8_t* skater, Skater& out) {
		const uint8_t* trick = (const uint8_t*)GetComponent(skater, kTrick);
		if (!trick) {
			return;
		}
		out.flipLevel = *(const uint32_t*)(trick + TRICK_EXTRA_START);
		if (trick[TRICK_EXTRA_ON] == 0 || trick[TRICK_EXTRA_FOREVER] != 0) {
			return;
		}
		const int32_t count = *(const int32_t*)(trick + TRICK_EXTRA_COUNT);
		const HeldFlip* next = 0;
		for (int i = 0; i < count && i < TRICK_EXTRA_MOST && !next; i++) {
			next = HeldFlipOf(*(const uint32_t*)(trick + TRICK_EXTRA_NAMES + 4 * i));
		}
		if (!next) {
			return;
		}
		const int32_t into = GameMs() - *(const int32_t*)(trick + TRICK_EXTRA_START);
		if (into < kHeldDecideMs || into > *(const int32_t*)(trick + TRICK_EXTRA_LENGTH) - kHeldLeaveMs) {
			return;
		}
		out.flipDecide = true;
		out.flipNext = next->title;
		// Where the animation of the trick is, how fast it runs: the next one goes on from there
		uint8_t* anim = (uint8_t*)GetComponent(skater, kAnimation);
		const uint8_t* channel = anim ? *(const uint8_t* const*)(anim + ANIM_FIRST) : 0;
		if (!channel || *(const int32_t*)(channel + CHANNEL_SELF) == -1 || *(const int32_t*)(channel + CHANNEL_BACKWARDS) != 0) {
			return;
		}
		const float speed = *(const float*)(channel + CHANNEL_SPEED) * 60.0f;
		const float at = *(const float*)(channel + CHANNEL_TIME) - *(const float*)(channel + CHANNEL_START);
		float length = 0.0f;
		const uint32_t anims[2] = { next->anim, next->nollie };
		for (int i = 0; i < 2; i++) {
			if (AnimExists(anim, anims[i])) {
				const float l = std::fabs(AnimLength(anim, anims[i]));
				if (l > length) {
					length = l;
				}
			}
		}
		if (!(speed > 0.01f) || !(length > 0.0f) || !(at >= 0.0f)) {
			return;
		}
		out.flipNeed = (length - kFlipSlack - at) / speed + kFlipSpare;
		uint8_t* core = (uint8_t*)GetComponent(skater, kSkaterCorePhysics);
		const uint8_t* state = (const uint8_t*)GetComponent(skater, kSkaterState);
		if (!core || !state) {
			return;
		}
		const float y = ((const float*)(skater + OBJECT_POSITION))[1];
		const float vy = ((const float*)(skater + OBJECT_VELOCITY))[1];
		out.flipAirLeft = TimeToHeight(core, y - *(const float*)(state + STATE_HEIGHT), y, vy);
		out.flipMore = out.flipAirLeft >= out.flipNeed;
	}

	bool ReadSkater(Skater& out) {
		if (!Available()) {
			return false;
		}
		uint8_t* skater = LocalSkater();
		if (!skater) {
			s_lastSkater = 0;
			s_lastState = Flickit::STATE_UNKNOWN;
			return false;
		}
		const uint8_t* state = (const uint8_t*)GetComponent(skater, kSkaterState);
		if (!state) {
			return false;
		}

		switch (*(const int32_t*)(state + STATE_STATE)) {
		case 0: out.state = Flickit::STATE_GROUND; break;
		case 1: out.state = Flickit::STATE_AIR; break;
		case 2: out.state = Flickit::STATE_WALL; break;
		case 3: out.state = Flickit::STATE_LIP; break;
		case 4: out.state = Flickit::STATE_RAIL; break;
		default: out.state = Flickit::STATE_OTHER; break;
		}
		// The game's own test (Skating): not driving, and the physics are the board's
		out.skating = state[STATE_DRIVING] == 0 && *(const int32_t*)(state + STATE_PHYSICS) == 0;
		// Which foot is forward: the game's Flipped. A skater starts flipped when his profile is
		// regular and not when it is goofy (0x00517122): flipped, his left foot is forward. Riding
		// switch is the other way round from his profile: the game's own test (IsSwitched, which the
		// script command Switched answers), here for the log.
		out.leftFootForward = state[STATE_FLIPPED] != 0;
		out.goofy = skater[OBJECT_GOOFY] != 0;
		out.switched = out.goofy ? out.leftFootForward : !out.leftFootForward;

		out.flipMore = false;
		out.flipDecide = false;
		out.flipLevel = 0;
		out.flipNext = "";
		out.flipAirLeft = 0.0f;
		out.flipNeed = 0.0f;
		if (s_installed && out.state == Flickit::STATE_AIR && out.skating && state[STATE_BAILING] == 0) {
			ReadHeldFlip(skater, out);
		}

		// The board has just touched a rail: what the script of the last grind said is history
		if (skater != s_lastSkater || (out.state == Flickit::STATE_RAIL && s_lastState != Flickit::STATE_RAIL)) {
			NewGrind();
		}
		// A flight starts with no rail seen. One that ends on the ground with a rail that was just
		// out of reach is told to the log.
		if (out.state == Flickit::STATE_AIR && s_lastState != Flickit::STATE_AIR) {
			s_reach.nearest.miss = Flickit::kNoRail;
			s_reach.tookRail = false;
			s_reach.turnedDown = false;
		}
		else if (out.state == Flickit::STATE_GROUND && s_lastState == Flickit::STATE_AIR) {
			if (!s_reach.tookRail && !s_reach.turnedDown && s_reach.nearest.miss < kWorthTelling) {
				s_reach.missed = s_reach.nearest;
				s_reach.misses++;
			}
			s_reach.nearest.miss = Flickit::kNoRail;
		}
		s_lastSkater = skater;
		s_lastState = out.state;

		out.grind = s_grind;
		out.grindReady = s_grindReady;
		out.grindSerial = s_grindSerial;

		if (out.state != Flickit::STATE_RAIL) {
			// Off the rail: what was known of the stay on it is history
			RailForget();
			if (s_noCatch && (out.state == Flickit::STATE_GROUND || ++s_noCatchPolls > kNoCatchPolls)) {
				s_noCatch = false;
			}
		}
		out.railOff = out.state == Flickit::STATE_RAIL && s_rail.run.WantsOff();
		out.offSide = s_rail.side;
		out.railSeconds = s_rail.run.Seconds();
		out.railSpeed = s_rail.run.Speed();
		out.railDrag = s_rail.run.Drag();
		out.railTerrain = s_rail.terrain;
		out.railSurface = s_rail.surface;
		out.dragClass = s_rail.dragClass;
		out.catchSerial = s_reach.catches;
		out.caught = s_reach.caught;
		out.missSerial = s_reach.misses;
		out.missed = s_reach.missed;

		// Rolling. A push button pushes while the board rolls on the ground with nothing balanced
		// (the game does not kick in a manual either), the skater is not in a bail, no script
		// has forbidden kicking and none has taken the controls away. And while the game is
		// running his physics at all: a skater the game holds still, for a talk or a film, is
		// not being pushed, and the buttons are the game's to read.
		if (s_groundSeen) {
			s_groundIdle = 0;
		}
		else if (s_groundIdle <= kPushIdleMost) {
			s_groundIdle++;
		}
		s_groundSeen = false;
		out.rolling = CanPush(skater) && s_groundIdle <= kPushIdleMost;
		const float* velocity = (const float*)(skater + OBJECT_VELOCITY);
		out.speed = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]);
		if (!out.rolling) {
			s_push.Reset();
			s_pushIdle = 0;
		}
		else if (s_pushAsked) {
			s_pushIdle = 0;
		}
		else if (++s_pushIdle > kPushIdleMost) {
			// On the ground, but the game's physics have not come by for a kick: no push is left half done
			s_push.Reset();
		}
		s_pushAsked = false;
		if (out.state != Flickit::STATE_GROUND || !out.skating) {
			// What the trucks did before the board left the ground is history
			s_steer.Reset();
		}
		out.pushSerial = s_push.Serial();
		out.pushHeld = s_push.HeldFor();
		out.pushAsked = s_push.Asked();
		out.pushAdded = s_push.Added();
		out.pushFrom = s_push.From();
		out.pushTo = s_push.To();
		out.pushAnim = s_pushClip >= 0 ? kPushClips[s_pushClip].text : "the clock";
		out.pushLength = s_pushLength;
		out.pushDown = s_push.Down();
		out.pushUp = s_push.Up();
		out.pushSource = s_pushSource;
		out.footSerial = s_footSerial;
		out.footAnim = kPushClips[s_footToldClip].text;
		out.footLength = s_footToldLength;
		out.foot = s_footTold;
		out.footTrace = s_footTrace;
		out.footTraceLooks = s_footTraceLooks;
		out.blindSerial = s_clock.Serial();
		out.blindAnim = s_clockAnim;
		out.cutSerial = s_cutSerial;
		out.cutAnim = kPushCuts[s_cutClip].text;
		out.cutShare = s_cutShare;
		out.waySerial = s_waySerial;
		out.wayAnim = s_wayAnim;
		out.wayTo = s_wayTo;
		out.wayShare = s_wayShare;
		out.poseSerial = s_poseSerial;
		out.poseFrom = s_poseFrom;
		out.poseLostSerial = s_poseLostSerial;
		out.poseMissSerial = s_poseMissSerial;
		out.poseMissAnim = s_poseMissAnim;
		out.poseMissChannels = s_poseMissChannels;
		out.poseMissLast = s_poseMissLast;
		out.poseMissBlending = s_poseMissBlending;
		out.poseUnderSerial = s_poseUnderSerial;
		out.poseUnderCrouch = s_poseUnderCrouch;
		out.poseUnderStand = s_poseUnderStand;
		out.stanceSerial = s_stanceSerial;
		out.stanceTo = s_stanceTo;
		out.stancePressure = s_stancePressure;
		out.stanceLate = s_stanceLate;
		out.spinSerial = s_spinSerial;
		out.spinTurned = s_spinTurned;
		out.spinFlick = s_spinFlick;
		out.spinSeconds = s_spinSeconds;
		return true;
	}

	/******************************************************************/
	/*                                                                */
	/*  Rails catch without the Grind button                          */
	/******************************************************************/

	// The game asks "is the Grind button down?" before it looks for a rail under a skater in the air.
	// Both places that ask call this instead. core: the skater's CSkaterCorePhysicsComponent.
	static bool __cdecl RailCatchAllowed(const uint8_t* core) {
		// The search that follows a "yes" is the game's own, at its own reach, unless the yes
		// comes from further down
		ReachForget();
		const uint8_t* input = *(const uint8_t* const*)(core + CORE_INPUT);
		if (input && input[INPUT_GRIND_PRESSED] != 0) {
			return true;    // the button, as it always was
		}
		if (!s_active || !s_frame.catchRails || !IsLocal(core)) {
			return false;
		}
		if (s_noCatch) {
			return false;   // it has just stepped off a rail it had stopped on
		}
		// Out of a ramp the coping is left alone: lip tricks and grinding the coping stay on the button
		const uint8_t* state = *(const uint8_t* const*)(core + CORE_STATE);
		if (!state || state[STATE_VERT_AIR] != 0) {
			return false;
		}
		s_reach.byItself = true;
		return true;
	}

	// Stands where "cmp byte ptr [reg+1A0h], 0" was (7 bytes: this call, then "test al, al"). The
	// "je" that follows is the game's. esi is the physics component at both places. Every register
	// but eax is handed back as it came.
	static __declspec(naked) void RailCatchHook() {
		__asm {
			push ecx
			push edx
			push esi
			call RailCatchAllowed
			add esp, 4
			pop edx
			pop ecx
			ret
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  A rail that catches by itself reaches as far as Skate's       */
	/******************************************************************/

	static bool RailTestOfTheGame(const float* from, const float* to) {
		const GameVector a = { from[0], from[1], from[2], from[3] };
		const GameVector b = { to[0], to[1], to[2], to[3] };
		return RailValid(a, b);
	}

	// Stands in for the game's search when the catch test before it said yes without the Grind
	// button. from, to: where the skater was a frame ago and where he is now, in the frame of the
	// thing the rails belong to (the level, or something that moves).
	static bool FindRailInReach(const uint8_t* manager, const float* from, const float* to, float* point, void** node, const void* ignore, float minCos) {
		if (!manager) {
			return false;
		}
		Flickit::RailNodes nodes;
		nodes.first = *(const uint8_t* const*)(manager + MANAGER_NODES);
		nodes.count = *(const int32_t*)(manager + MANAGER_COUNT);
		nodes.stride = RAIL_NODE_SIZE;
		nodes.boxMin = RAIL_BOX_MIN;
		nodes.boxMax = RAIL_BOX_MAX;
		nodes.position = RAIL_POSITION;
		nodes.next = RAIL_NEXT;
		nodes.previous = RAIL_PREVIOUS;
		nodes.flags = RAIL_FLAGS;
		nodes.needs = RAIL_FLAG_ACTIVE;
		nodes.skips = RAIL_FLAG_CLIMB_ONLY;

		Flickit::RailPass pass;
		const uint8_t* found = Flickit::FindRail(nodes, from, to, (const uint8_t*)ignore, minCos, s_frame.reach, &RailTestOfTheGame, pass);
		if (!found) {
			if (pass.miss < s_reach.nearest.miss) {
				s_reach.nearest = pass;
			}
			return false;
		}
		point[0] = pass.point[0];
		point[1] = pass.point[1];
		point[2] = pass.point[2];
		point[3] = *(const float*)(found + RAIL_POSITION + 12);
		*node = (void*)found;
		s_reach.found = true;
		s_reach.pass = pass;
		return true;
	}

	// Stands where the game calls CRailManager::StickToRail after a catch test, for the rails of
	// the level and for those of each thing that moves. Same arguments, same answer in al, and the
	// seven arguments are taken off the stack as the game's function does it.
	static bool __fastcall RailSearchHook(uint8_t* manager, void* /*edx*/, const float* from, const float* to, float* point, void** node, const void* ignore, float minCos, int side) {
		if (s_reach.byItself && s_active && s_frame.reach > 0) {
			return FindRailInReach(manager, from, to, point, node, ignore, minCos);
		}
		return RailSearch(manager, from, to, point, node, ignore, minCos, side);
	}

	/******************************************************************/
	/*                                                                */
	/*  Jumps: Skate's gravity in the air, Skate's pop                */
	/******************************************************************/

	// Stands where the game calls get_air_gravity. Same convention: this in ecx, no arguments, the
	// answer on the floating point stack.
	static float __fastcall AirGravityHook(uint8_t* core) {
		GiveBackTurn(core);     // a jump out of a push: the air turns him by Left and Right too
		float gravity = AirGravity(core);
		if (s_active && s_frame.gravity > 0 && IsLocal(core)) {
			gravity *= Flickit::AirGravityFactor(s_frame.gravity);
		}
		return gravity;
	}

	// The skater whose jump the game is working out right now, if it is Flickit's and the pop is
	// not the game's own
	static const uint8_t* s_jumping = 0;

	// Stands where the game calls do_jump. Same convention: this in ecx, one argument, which the
	// callee takes off the stack.
	//
	// The game makes a jump the higher the longer the jump button was down, up to a fifth of a
	// second (Skater_max_tense_time). Out of a crouch of Flickit's it is how deep the crouch was,
	// the way Skate's pop is: the game is given the whole fifth of a second, so that it works out
	// the skater's best jump, and that is scaled for the crouch below.
	static void __fastcall JumpHook(uint8_t* core, void* /*edx*/, void* params) {
		s_jumping = (s_active && s_frame.pop > 0 && IsLocal(core)) ? core : 0;
		s_popShare = 1.0f;
		if (s_jumping && s_popStrength >= 0.0f && s_frames - s_popFrame <= kPopFresh) {
			*(int32_t*)(core + CORE_TENSE_TIME) = 0x7FFFFFFF;   // the game takes the lesser of this and its most
			s_popShare = Flickit::PopDepthShare(s_popStrength);
		}
		s_popStrength = -1.0f;
		DoJump(core, params);
		s_jumping = 0;
	}

	// Stands where do_jump asks the script for a speed of its own. By then the speed from the
	// stats is in "speed" already, and what is there after this call is what the jump gets. Same
	// convention as CStruct::GetFloat: this in ecx, three arguments, the callee takes them off.
	static bool __fastcall JumpSpeedHook(void* params, void* /*edx*/, uint32_t name, float* speed, int assert) {
		const bool given = StructGetFloat(params, name, speed, assert);
		if (s_jumping && speed) {
			*speed *= Flickit::JumpPopFactor(s_frame.pop) * s_popShare;
		}
		return given;
	}

	/******************************************************************/
	/*                                                                */
	/*  The board touches a rail                                      */
	/******************************************************************/

	// At the end of the game's stick_to_rail, for the skater Flickit drives. railOnLeft is the
	// game's own flag for the side the rail was on.
	static void RailTouched(const uint8_t* core, bool railOnLeft) {
		RailForget();
		s_noCatch = false;
		if (s_reach.found) {
			// It is the rail the search within Skate's reach took a moment ago
			s_reach.found = false;
			s_reach.caught = s_reach.pass;
			s_reach.catches++;
		}
		s_reach.tookRail = true;

		// By now the speed points along the rail. With (x, z) the level direction of travel, the
		// game's flag says the rail lay toward (z, -x) from the board. The board came from the
		// other side.
		const uint8_t* skater = *(const uint8_t* const*)(core + COMPONENT_OBJECT);
		if (!skater) {
			return;
		}
		const float* velocity = (const float*)(skater + OBJECT_VELOCITY);
		const float length = std::sqrt(velocity[0] * velocity[0] + velocity[2] * velocity[2]);
		if (length > 1.0f) {
			const float sign = railOnLeft ? -1.0f : 1.0f;
			s_rail.from[0] = sign * velocity[2] / length;
			s_rail.from[1] = 0.0f;
			s_rail.from[2] = -sign * velocity[0] / length;
			s_rail.hasFrom = true;
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  The lean of the board picks the grind                         */
	/******************************************************************/

	// The salads are written as grinds a double tap switches to from another grind (IsATap), and
	// such a grind does not start the balance of the rail: the Grind script leaves it to the grind
	// before. In THUG-SDL's scripts it asks "IF NOT DoingBalanceTrick" first, a name that nothing
	// defines, and the script VM takes a name it does not know for true (0x0040EC7D), so it never
	// starts one; with the original double taps (OriginalDoubleTapGrinds) it does, with the cheese
	// of the grinds before cleared. When Flickit starts a salad as the board lands on the rail there
	// is no grind before it. With no balance on, the game's rail physics never update one (0x0056D049),
	// so there is no drag, no meter and no fall, and the board keeps its speed to the end of the rail.
	// So once the script has run up to its first wait, Flickit starts the balance the way the Grind
	// script starts it for a grind that is no tap: DoBalanceTrick ButtonA = Right ButtonB = Left
	// Type = <Type> DoFlipCheck. Only while he is on the rail and the script is in the Grind (it
	// bails out of it at once with BailIsOn), and only when no balance of a rail is on. True when
	// it started one.
	static bool StartRailBalance(uint8_t* core, const uint8_t* state, void* script) {
		uint8_t* balance = *(uint8_t**)(core + CORE_BALANCE);
		if (!balance || !script || *(const int32_t*)(state + STATE_STATE) != 4
			|| *(const uint32_t*)((const uint8_t*)script + SCRIPT_NAME) != kGrindBalance) {
			return false;
		}
		const uint32_t on = *(const uint32_t*)(balance + BALANCE_TYPE);
		if (on == kGrindBalance || on == kSlideBalance) {
			return false;   // the script started it
		}
		// <Type>: what the grind script was told it is
		void* params = *(void* const*)((const uint8_t*)script + SCRIPT_PARAMS);
		uint32_t type = 0;
		if (!params || !StructGetChecksum(params, kType, &type, false) || (type != kGrindBalance && type != kSlideBalance)) {
			return false;
		}
		GameStruct command = { 0 };
		StructAddChecksum(&command, kButtonA, kRight);
		StructAddChecksum(&command, kButtonB, kLeft);
		StructAddChecksum(&command, kType, type);
		StructAddChecksum(&command, 0, kDoFlipCheck);
		BalanceCommand(balance, kDoBalanceTrick, &command, script);
		StructClear(&command);
		return *(const uint32_t*)(balance + BALANCE_TYPE) == type;
	}

	// Starts a grind script by name, the way do_grind_trick starts the one it found in its table.
	// balanceStarted: Flickit had to start the balance of the rail itself (StartRailBalance).
	static bool StartGrindScript(uint8_t* core, const char* prefix, bool railOnLeft, bool backwards, bool flipped, bool& balanceStarted) {
		balanceStarted = false;
		// The name the game itself would put together: the grind and the ending for this approach
		char name[64];
		size_t length = 0;
		const char* parts[2] = { prefix, Flickit::GrindSuffix(railOnLeft, backwards, flipped) };
		for (int i = 0; i < 2; i++) {
			for (const char* c = parts[i]; *c; c++) {
				if (length + 1 >= sizeof(name)) {
					return false;
				}
				name[length++] = *c;
			}
		}
		name[length] = '\0';
		const uint32_t checksum = Flickit::NameChecksum(name);

		const uint8_t* symbol = (const uint8_t*)LookUpSymbol(checksum);
		if (!symbol || symbol[1] != 7) {
			return false;   // this game's scripts have no such grind
		}

		uint8_t* skater = *(uint8_t**)(core + COMPONENT_OBJECT);
		uint8_t* state = *(uint8_t**)(core + CORE_STATE);
		if (!skater || !state || !*(void**)(skater + OBJECT_SCRIPT)) {
			return false;
		}
		void* flipRotate = GetComponent(skater, kSkaterFlipAndRotate);
		if (!flipRotate) {
			return false;
		}

		*(uint32_t*)(core + CORE_GRIND_TWEAK) = 0;
		FlipRotateAfters(flipRotate);
		void* script = *(void**)(skater + OBJECT_SCRIPT);
		SetScript(script, checksum, 0, skater);
		ScriptUpdate(script);
		state[STATE_DOING_TRICK] = 1;
		balanceStarted = StartRailBalance(core, state, script);
		return true;
	}

	// Stands in for do_grind_trick at the one place it is called from, when the board has just
	// touched a rail. Same calling convention: this in ecx, five arguments, the callee pops them.
	static void __fastcall GrindPickHook(uint8_t* core, void* /*edx*/, uint32_t direction, uint32_t railOnLeft, uint32_t parallel, uint32_t backwards, uint32_t flipped) {
		const bool left = (railOnLeft & 0xFF) != 0;
		const bool local = s_active && IsLocal(core);
		if (local) {
			RailTouched(core, left);
		}
		if (!local || !s_frame.pickGrind || s_frame.classicGrind) {
			// Not Flickit's skater, or the player holds the Grind button: the direction buttons pick
			DoGrindTrick(core, direction, railOnLeft, parallel, backwards, flipped);
			return;
		}

		const int asked = Flickit::GrindFromTilt(s_frame.tilt, left != s_frame.mirror);

		// The board as it reaches the rail. The skater still faces the way he did in the air (the
		// game has just read that for its along / across test, the Parallel argument), his speed
		// points along the rail by now, and RailTouched has the side he came from.
		Flickit::RailContact contact;
		contact.known = false;
		contact.angle = 0.0f;
		contact.noseLeads = true;
		contact.across = (parallel & 0xFF) == 0;
		contact.backwards = (backwards & 0xFF) != 0;
		const uint8_t* skater = *(const uint8_t* const*)(core + COMPONENT_OBJECT);
		if (skater && s_rail.hasFrom) {
			Flickit::ContactAtRail((const float*)(skater + OBJECT_FORWARD), (const float*)(skater + OBJECT_VELOCITY), s_rail.from,
				contact.across, contact.backwards, contact);
		}
		int grind = s_frame.byBoard ? Flickit::GrindFromBoard(asked, contact, s_frame.alongAngle, s_frame.mirror) : asked;

		s_pick.asked = asked;
		s_pick.contact = contact;
		s_pick.byBoard = s_frame.byBoard;
		s_pick.grind = grind;
		s_pick.railOnLeft = left;
		s_pick.byName = false;
		s_pick.balanceStarted = false;
		s_pick.tilt = s_frame.tilt;
		s_hasPick = true;

		Flickit::GrindPlan plan;
		Flickit::PlanGrind(grind, left, plan);
		if (plan.row < 0) {
			bool balanceStarted = false;
			if (StartGrindScript(core, plan.prefix, left, (backwards & 0xFF) != 0, (flipped & 0xFF) != 0, balanceStarted)) {
				s_pick.started = grind;
				s_pick.byName = true;
				s_pick.balanceStarted = balanceStarted;
				s_rail.pick = grind;
				s_rail.hasPick = true;
				return;
			}
			grind = plan.fallback;
			Flickit::PlanGrind(grind, left, plan);
		}
		s_pick.started = grind;
		s_rail.pick = grind;
		s_rail.hasPick = true;
		DoGrindTrick(core, (uint32_t)plan.row, railOnLeft, plan.parallel < 0 ? parallel : (uint32_t)plan.parallel, backwards, flipped);
	}

	/******************************************************************/
	/*                                                                */
	/*  The board loses speed on a rail                               */
	/******************************************************************/

	// One frame of the game's rail physics for the skater Flickit drives, before they look at the
	// speed. balance: his CSkaterBalanceTrickComponent.
	static void RailFrame(uint8_t* balance) {
		uint8_t* skater = *(uint8_t**)(balance + COMPONENT_OBJECT);
		uint8_t* core = skater ? (uint8_t*)GetComponent(skater, kSkaterCorePhysics) : 0;
		if (!core || *(uint8_t**)(core + CORE_BALANCE) != balance) {
			return;
		}

		// On a rail the speed points along the rail
		float* velocity = (float*)(skater + OBJECT_VELOCITY);
		const float speed = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]);
		const bool moving = speed > 0.001f;
		const float level = moving ? std::sqrt(velocity[0] * velocity[0] + velocity[2] * velocity[2]) / speed : 1.0f;
		const bool turned = moving && s_rail.hasDirection
			&& velocity[0] * s_rail.direction[0] + velocity[1] * s_rail.direction[1] + velocity[2] * s_rail.direction[2] < 0.0f;

		const uint8_t* node = *(const uint8_t* const*)(core + CORE_RAIL_NODE);
		s_rail.terrain = node ? node[RAIL_TERRAIN] : 0;
		s_rail.surface = Flickit::SurfaceOfTerrain(s_rail.terrain);

		// What kind of grind: the game balances it as a grind or as a slide, its script has said
		// whether it is one of the three on the trucks, and the pick knows what it started
		const bool slide = *(const uint32_t*)(balance + BALANCE_TYPE) == kSlideBalance;
		int grind = slide ? Flickit::GRIND_UNKNOWN : s_grind;
		if (s_rail.hasPick && (slide || grind == Flickit::GRIND_UNKNOWN)) {
			grind = s_rail.pick;
		}
		s_rail.dragClass = Flickit::DragClassOf(grind, slide);

		const float after = s_rail.run.Step(*(const float*)(core + CORE_FRAME_LENGTH), speed, level,
			s_rail.dragClass, s_rail.surface, turned, s_frame.drag, s_frame.stepOff);
		if (moving) {
			s_rail.direction[0] = velocity[0] / speed;
			s_rail.direction[1] = velocity[1] / speed;
			s_rail.direction[2] = velocity[2] / speed;
			s_rail.hasDirection = true;
			if (after < speed) {
				const float scale = after / speed;
				velocity[0] *= scale;
				velocity[1] *= scale;
				velocity[2] *= scale;
			}
		}

		// The side the board came from, as the skater stands now
		s_rail.side = 0;
		if (s_rail.hasFrom) {
			const float* left = (const float*)(skater + OBJECT_LEFT);
			const float toLeft = left[0] * s_rail.from[0] + left[1] * s_rail.from[1] + left[2] * s_rail.from[2];
			s_rail.side = toLeft > 0.0f ? -1 : 1;
		}

		s_noCatch = s_rail.run.WantsOff();
		s_noCatchPolls = 0;
	}

	/******************************************************************/
	/*                                                                */
	/*  The board does not drift on a rail                            */
	/******************************************************************/

	// While RailBalanceHook has the balance update of a rail running for a board that does not
	// drift: its meter has nothing to show
	static bool s_meterOff = false;

	// Stands where the balance update shows the balance meter. Same convention as the game's: this
	// (the score) in ecx, two arguments, which the callee takes off the stack. The update is the one
	// place that shows the meter of a grind or slide: everything else only ever hides it. So while
	// the board cannot lose its balance the meter is told "off" instead, and stays hidden; one that
	// was up already (Flickit switched on in the middle of a grind, say) goes as it would at the end
	// of a balance.
	static void __fastcall MeterHook(void* score, void* /*edx*/, uint32_t on, float value) {
		SetBalanceMeter(score, s_meterOff ? 0 : on, value);
	}

	// Stands in for the balance update of a grind or slide. With the game's own "perfect rail" cheat
	// the lean and its speed are set to zero right after this call; the same is done here, without
	// the cheat being on. record: the balance component + 0x7C.
	static void __fastcall RailBalanceHook(uint8_t* record, void* /*edx*/) {
		uint8_t* balance = record - BALANCE_RECORD;
		const bool local = s_active && IsLocal(balance);
		s_meterOff = local && s_frame.noBalance;
		BalanceUpdate(record);
		s_meterOff = false;
		if (!local || (!s_frame.noBalance && s_frame.drag <= 0 && !s_frame.stepOff)) {
			return;
		}
		if (s_frame.noBalance) {
			*(float*)(record + BALANCE_LEAN) = 0.0f;
			*(float*)(record + BALANCE_LEAN_SPEED) = 0.0f;
		}
		// This call is made once on every frame of a grind or slide, before the rail physics look at
		// the speed: the place for the drag
		if (s_frame.drag > 0 || s_frame.stepOff) {
			RailFrame(balance);
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  No speed for landing on a rail                                */
	/******************************************************************/

	// THUG adds speed whenever the board lands on a rail (Rail_Speed_Boost in physics.q, 150). Skate
	// does not, and with a drag on the rail it would be a way to get the speed back by hopping. So
	// for the skater Flickit drives, while the drag is on, the game is told the boost is next to
	// nothing. Not nothing at all: a board that lands on a rail with no speed along it gets its only
	// speed from this boost, and the game's rail code reads the direction of travel off the speed.
	static float __cdecl SmallRailBoost(uint32_t /*name*/, int /*flag*/) {
		return 4.0f;
	}

	// The function that is to answer the game's question for Rail_Speed_Boost
	static void* __cdecl RailBoostAnswer(const uint8_t* core) {
		if (s_active && s_frame.drag > 0 && !s_frame.boost && IsLocal(core)) {
			return (void*)&SmallRailBoost;
		}
		return (void*)GetPhysicsFloat;
	}

	// Stands where stick_to_rail calls GetPhysicsFloat for Rail_Speed_Boost. esi is the physics
	// component there. The two arguments of that call are still on the stack, and stay there for the
	// function that is jumped to: it returns straight to the game, which takes them off.
	static __declspec(naked) void RailBoostHook() {
		__asm {
			push esi
			call RailBoostAnswer
			add esp, 4
			jmp eax
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  Off the rail when the speed is gone                           */
	/******************************************************************/

	// When the board has run out of speed (RailRun::WantsOff) Skate pushes it sideways off the rail:
	// off a ledge away from its top, off a rail to the side the board sits on (Flickit::PlanRailExit).
	// THUG's way off a rail is its drop-down, SkateInOrBail: next to a vert ramp the skater skates in,
	// anywhere else he is lifted 5 units, put 5 units to the side and let fall. Its button press goes
	// to the right whichever side is asked for, unless THUG-SDL's directional drop-down is on (the
	// scripts move by "moveright" for both sides), so that half the time he came down on top of the
	// ledge he had just left. Flickit starts that script itself instead, the way the trick component
	// starts it for the button, with the side and the step it picked, and gives the skater Skate's
	// speed sideways once he is in the air.
	struct ExitPush {
		bool pending;                       // the script runs; the push is due once he is in the air
		int frames;                         // frames it has been due
		float side[3];                      // the way off, level
		float speed;                        // units/s
	};
	static ExitPush s_exitPush;
	static const int kExitPushFrames = 30;   // a skater still on the rail after this long did not get off
	static bool s_hasExit = false;
	static RailExit s_exit;

	// Is there a surface right next to the rail on this side, at its height: the game's own test for
	// a ledge at the start of a grind, at the place the board is now. A line from Physics_Ground_Snap_Up
	// above the rail to Rail_Bad_Ledge_Drop_Down_Dist below it, Rail_Bad_Ledge_Side_Dist to the side,
	// through the physics component's own line test.
	static bool SurfaceBeside(uint8_t* core, const float* at, const float* across, float sign) {
		const float up = GetPhysicsFloat(kGroundSnapUp, 0);
		const float down = GetPhysicsFloat(kBadLedgeDown, 0);
		const float aside = GetPhysicsFloat(kBadLedgeSide, 0) * sign;
		float* start = (float*)(core + CORE_LINE_START);
		float* end = (float*)(core + CORE_LINE_END);
		start[0] = at[0] + across[0] * aside;
		start[1] = at[1] + up;
		start[2] = at[2] + across[2] * aside;
		start[3] = at[3];
		end[0] = start[0];
		end[1] = at[1] - down;
		end[2] = start[2];
		end[3] = at[3];
		return LineHit(core, 0x10, 0);
	}

	// The board has been out of speed on the rail long enough: take him off it. On a frame of the
	// skater's pad, between his script's turn and his physics'.
	static void StartRailExit(uint8_t* skater, uint8_t* core, uint8_t* state) {
		if (*(const int32_t*)(state + STATE_STATE) != 4 || s_exitPush.pending) {
			return;
		}
		// Only from the grind's own script, as the game's button does: not in a bail, a switch, a pop
		void* script = *(void**)(skater + OBJECT_SCRIPT);
		const ScriptAt at = ScriptOf(skater);
		if (!script || (at.running != kGrindBalance && at.caller != kGrindBalance)) {
			return;
		}
		const uint8_t* symbol = (const uint8_t*)LookUpSymbol(kSkateInOrBail);
		if (!symbol || symbol[1] != 7) {
			return;
		}

		// Across the rail, level: from the way the board went on it, else from the piece of rail
		float along[3] = { 0.0f, 0.0f, 0.0f };
		if (s_rail.hasDirection) {
			along[0] = s_rail.direction[0];
			along[2] = s_rail.direction[2];
		}
		else {
			const uint8_t* node = *(const uint8_t* const*)(core + CORE_RAIL_NODE);
			const uint8_t* next = node ? *(const uint8_t* const*)(node + RAIL_NEXT) : 0;
			const uint8_t* previous = node ? *(const uint8_t* const*)(node + RAIL_PREVIOUS) : 0;
			const uint8_t* from = next ? node : previous;
			const uint8_t* to = next ? next : node;
			if (from && to) {
				along[0] = ((const float*)(to + RAIL_POSITION))[0] - ((const float*)(from + RAIL_POSITION))[0];
				along[2] = ((const float*)(to + RAIL_POSITION))[2] - ((const float*)(from + RAIL_POSITION))[2];
			}
		}
		const float length = std::sqrt(along[0] * along[0] + along[2] * along[2]);
		if (!(length > 0.001f)) {
			return;
		}
		const float across[3] = { along[2] / length, 0.0f, -along[0] / length };

		const float* position = (const float*)(skater + OBJECT_POSITION);
		const bool surfaceA = SurfaceBeside(core, position, across, 1.0f);
		const bool surfaceB = SurfaceBeside(core, position, across, -1.0f);
		int cameFrom = 0;
		if (s_rail.hasFrom) {
			cameFrom = s_rail.from[0] * across[0] + s_rail.from[2] * across[2] > 0.0f ? 1 : -1;
		}
		// Where THUG goes when nothing says: the skater's right
		const float* left = (const float*)(skater + OBJECT_LEFT);
		const float leftAcross = left[0] * across[0] + left[2] * across[2];
		const bool fiveO = s_grind == Flickit::GRIND_FIVE_O || (s_grind == Flickit::GRIND_UNKNOWN && s_rail.hasPick && s_rail.pick == Flickit::GRIND_FIVE_O);
		Flickit::RailExitPlan plan;
		Flickit::PlanRailExit(surfaceA, surfaceB, cameFrom, leftAcross > 0.0f ? -1 : 1, s_rail.dragClass, fiveO, plan);
		const float side[3] = { across[0] * (float)plan.side, 0.0f, across[2] * (float)plan.side };
		const bool toLeft = left[0] * side[0] + left[2] * side[2] > 0.0f;

		// Started the way the trick component starts a button's trick: the flips and turns the grind
		// left for "after" first, then the script, run at once, and he is doing a trick. The script:
		// SkateInOrBail { FallingLeft or Fallingright GrindBail = Airborne moveright = step movey = step },
		// the side ahead of the buttons it would ask, no bail where there is no ground, and the step
		// to that side (Move x goes to his left) and up.
		void* flipRotate = GetComponent(skater, kSkaterFlipAndRotate);
		if (flipRotate) {
			FlipRotateAfters(flipRotate);
		}
		GameStruct params = { 0 };
		StructAddChecksum(&params, 0, toLeft ? kFallingLeft : kFallingRight);
		StructAddChecksum(&params, kGrindBail, kAirborne);
		StructAddInteger(&params, kMoveRight, toLeft ? (int32_t)Flickit::kExitStep : -(int32_t)Flickit::kExitStep);
		StructAddInteger(&params, kMoveY, (int32_t)Flickit::kExitStep);
		SetScript(script, kSkateInOrBail, &params, skater);
		StructClear(&params);
		ScriptUpdate(script);
		state[STATE_DOING_TRICK] = 1;

		s_exitPush.pending = true;
		s_exitPush.frames = 0;
		s_exitPush.side[0] = side[0];
		s_exitPush.side[1] = 0.0f;
		s_exitPush.side[2] = side[2];
		s_exitPush.speed = plan.speed;

		// For the log, once it is known how it went
		s_exit.toLeft = toLeft;
		s_exit.why = plan.why;
		s_exit.speed = plan.speed;
		s_exit.surfaceLeft = (leftAcross > 0.0f) ? surfaceA : surfaceB;
		s_exit.surfaceRight = (leftAcross > 0.0f) ? surfaceB : surfaceA;
		s_exit.inAir = false;
	}

	// Once he is in the air off the rail, Skate's push has the board going sideways at its speed
	static void PushOffRail(uint8_t* skater, const uint8_t* state) {
		if (!s_exitPush.pending) {
			return;
		}
		const int32_t now = *(const int32_t*)(state + STATE_STATE);
		if (now == 1) {
			float* velocity = (float*)(skater + OBJECT_VELOCITY);
			const float sideways = velocity[0] * s_exitPush.side[0] + velocity[2] * s_exitPush.side[2];
			if (sideways < s_exitPush.speed) {
				velocity[0] += s_exitPush.side[0] * (s_exitPush.speed - sideways);
				velocity[2] += s_exitPush.side[2] * (s_exitPush.speed - sideways);
			}
			s_exitPush.pending = false;
			s_exit.inAir = true;
			s_hasExit = true;
		}
		else if (now != 4 || ++s_exitPush.frames > kExitPushFrames) {
			// He skated in (a vert ramp next to the rail), bailed, or is still on the rail
			s_exitPush.pending = false;
			s_exit.inAir = false;
			s_hasExit = true;
		}
	}

	static void RailExitFrame(uint8_t* skater, uint8_t* core) {
		uint8_t* state = *(uint8_t**)(core + CORE_STATE);
		const bool asked = s_exitAsked;
		s_exitAsked = false;
		if (!state) {
			return;
		}
		PushOffRail(skater, state);
		if (asked && s_active) {
			StartRailExit(skater, core, state);
			// The game's own drop-down is in the air at once; THUG-SDL's directional one a frame later
			PushOffRail(skater, state);
		}
	}

	bool ExitsByScript() {
		if (!s_installed) {
			return false;
		}
		const uint8_t* symbol = (const uint8_t*)LookUpSymbol(kSkateInOrBail);
		return symbol && symbol[1] == 7;
	}

	bool TakeExit(RailExit& out) {
		if (!s_hasExit) {
			return false;
		}
		out = s_exit;
		s_hasExit = false;
		return true;
	}

	/******************************************************************/
	/*                                                                */
	/*  Rolling: a push on a button                                   */
	/******************************************************************/

	// The skater Flickit drives, on a frame Flickit is on
	static inline bool Drives(const uint8_t* core) {
		return s_active && IsLocal(core);
	}

	static inline float Speed(const uint8_t* skater) {
		const float* velocity = (const float*)(skater + OBJECT_VELOCITY);
		return std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]);
	}

	// Below this speed (units/s) a board that is rolling out is left to the game, which stops it
	// and stands the skater up
	static const float kStandSpeed = 10.0f;

	// Stands where an input component is handed what one look at the controller gave. Same
	// convention: this in ecx, one argument, which the callee takes off the stack.
	//
	// The game's ground scripts look at Left and Right first, and play the turn for them; only
	// when neither is down do they get as far as the push. In Skate the board is pushed in a turn
	// like anywhere else. So while a push of Flickit's asks for its animation and the scripts
	// are in the loop of the ground, they see neither Left nor Right (not for long at a time: a
	// push the game does not show must not keep the turn from being shown). They run before the
	// physics do: the physics get both back (GiveBackTurn) and turn the board as if nothing had
	// happened.
	static void ShowCrouch(uint8_t* skater);
	static uint8_t* StandUnder(uint8_t* anim, uint32_t stand);
	static void KeepStance(uint8_t* skater);

	/******************************************************************/
	/*                                                                */
	/*  The watchdog: the skater and his camera stay numbers          */
	/******************************************************************/

	// The game's way to an object by its name, the way its ObjectExists finds one (0x0041FD90): the
	// composite object manager at 0x0069B6C0, two tables in it (+8, then +0xC), and the lookup in a
	// table (0x0041FA60: this = the table; the address of the name, 1; ret 8). It only reads.
	static void* const* const kObjectManager = (void* const*)0x0069B6C0;
	typedef void* (__thiscall* TableLookup_t)(void* table, const uint32_t* name, int flag);
	static const TableLookup_t TableLookup = (TableLookup_t)0x0041FA60;
	// CSkater::ResetPhysics (walk, in_cleanup), a virtual function (its address is in a table at
	// 0x0063AE30), the one THUG-SDL's script command M_ResetPhysics calls for the local skater
	typedef void(__thiscall* ResetPhysics_t)(void* skater, bool walk, bool inCleanup);
	static const ResetPhysics_t ResetPhysics = (ResetPhysics_t)0x00518840;
	static const uint32_t kSkaterCam0 = 0x967C138C;           // SkaterCam0

	// Checked on their own: if these are not as expected, only the camera and the reset are left out
	static const Expect kWatchExpect[] = {
		{ 0x0041FA60, 12, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0xD1, 0x8B, 0x0A, 0x8B, 0x52, 0x04, 0x56 }, "object table lookup" },
		{ 0x0041FDAF, 6, { 0x8B, 0x35, 0xC0, 0xB6, 0x69, 0x00 }, "ObjectExists: the object manager" },
		{ 0x00518840, 12, { 0x6A, 0xFF, 0x68, 0x76, 0x93, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "CSkater::ResetPhysics" },
	};

	// The renderer's camera as it last made a view of the level (0x004B9C70, from a camera's position and
	// two of its directions): the eye at 0x0072DF44, the direction from its +0x20 at 0x0072DF5C, up (its
	// +0x10) at 0x0072DF68, and the view (0x0072DE50) and the projection (0x0072DE90) made of them. And what
	// the skater camera follows of him: the object at his +0x254, which the restart moves with him
	// (0x00517563: his position into its +0x1C by 0x0044A8E0, his up into its +0x2C by 0x0044A900).
	// All only read, except that the camera target is put back on him when it has stopped being numbers.
	static const float* const kViewEye = (const float*)0x0072DF44;
	static const float* const kViewAlong = (const float*)0x0072DF5C;
	static const float* const kViewUp = (const float*)0x0072DF68;
	static const float* const kViewMatrix = (const float*)0x0072DE50;
	static const float* const kProjMatrix = (const float*)0x0072DE90;
	enum {
		SKATER_CAMERA_TARGET = 0x254,
		TARGET_POS = 0x1C,
		TARGET_UP = 0x2C,
	};
	typedef void(__thiscall* TargetSet_t)(void* target, const float* v);
	static const TargetSet_t SetTargetPos = (TargetSet_t)0x0044A8E0;
	static const TargetSet_t SetTargetUp = (TargetSet_t)0x0044A900;

	static const Expect kViewExpect[] = {
		{ 0x004B9C70, 14, { 0x83, 0xEC, 0x1C, 0x8B, 0x44, 0x24, 0x24, 0xD9, 0x00, 0x68, 0x68, 0xDF, 0x72, 0x00 }, "the view's set-up: up" },
		{ 0x004B9C7E, 6, { 0xD9, 0x1D, 0x44, 0xDF, 0x72, 0x00 }, "the view's set-up: the eye" },
		{ 0x004B9C9A, 9, { 0xD9, 0x40, 0x20, 0xD9, 0x1D, 0x5C, 0xDF, 0x72, 0x00 }, "the view's set-up: the second direction" },
		{ 0x004B9E36, 5, { 0x68, 0x90, 0xDE, 0x72, 0x00 }, "the view's set-up: the projection" },
		{ 0x004B9E96, 5, { 0x68, 0x50, 0xDE, 0x72, 0x00 }, "the view's set-up: the view" },
		{ 0x0044A8E0, 9, { 0x8B, 0x44, 0x24, 0x04, 0xD9, 0x00, 0xD9, 0x59, 0x1C }, "the camera target's position" },
		{ 0x0044A900, 9, { 0x8B, 0x44, 0x24, 0x04, 0xD9, 0x00, 0xD9, 0x59, 0x2C }, "the camera target's up" },
		{ 0x00517563, 11, { 0x8B, 0x8E, 0x54, 0x02, 0x00, 0x00, 0xE8, 0x72, 0x33, 0xF3, 0xFF }, "the restart moves the camera target" },
	};

	// The watchdog's notes go to flickit_watchdog.txt next to the exe as well as to the log, each with the
	// time of day: that file is kept from one start of the game to the next (the log starts again at every
	// start, and is only written with the console on). Past 2 MB it starts again, the old one kept as .old.
	static char s_watchPath[MAX_PATH] = "";
	static int s_watchFile = 0;                       // 1: notes go there, -1: they cannot

	static void WatchAppend(const char* text) {
		HANDLE f = CreateFileA(s_watchPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (f == INVALID_HANDLE_VALUE) {
			return;
		}
		DWORD put = 0;
		WriteFile(f, text, (DWORD)strlen(text), &put, NULL);
		CloseHandle(f);
	}

	static void WatchFileLine(const char* text) {
		if (s_watchFile == 0) {
			s_watchFile = -1;
			char dir[MAX_PATH];
			const DWORD n = GetModuleFileNameA(NULL, dir, MAX_PATH);
			char* slash = n && n < MAX_PATH ? strrchr(dir, '\\') : 0;
			if (slash) {
				slash[1] = 0;
				sprintf_s(s_watchPath, sizeof(s_watchPath), "%sflickit_watchdog.txt", dir);
				WIN32_FILE_ATTRIBUTE_DATA a;
				if (GetFileAttributesExA(s_watchPath, GetFileExInfoStandard, &a) && (a.nFileSizeHigh || a.nFileSizeLow > 2u * 1024u * 1024u)) {
					char old[MAX_PATH];
					sprintf_s(old, sizeof(old), "%sflickit_watchdog.old.txt", dir);
					MoveFileExA(s_watchPath, old, MOVEFILE_REPLACE_EXISTING);
				}
				SYSTEMTIME t;
				GetLocalTime(&t);
				char head[160];
#ifdef FLICKIT_BUILD
				const char* build = FLICKIT_BUILD;
#else
				const char* build = "a local build";
#endif
				sprintf_s(head, sizeof(head), "\r\n=== %04u-%02u-%02u: the first note of this run of the game (Flickit %.12s) ===\r\n", t.wYear, t.wMonth, t.wDay, build);
				s_watchFile = 1;
				WatchAppend(head);
			}
		}
		if (s_watchFile > 0) {
			WatchAppend(text);
		}
	}

	// A line for the log and the file
	static void WatchLog(const char* format, ...) {
		char text[1024];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		text[sizeof(text) - 1] = 0;
		Log::TypedLog(CHN_SDL, "%s", text);
		size_t n = strlen(text);
		while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r')) {
			text[--n] = 0;
		}
		SYSTEMTIME t;
		GetLocalTime(&t);
		char line[1100];
		sprintf_s(line, sizeof(line), "%02u:%02u:%02u.%03u %s\r\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, text);
		WatchFileLine(line);
	}

	void WatchNote(const char* text) {
		WatchLog("%s", text);
	}

	static Watchdog::History<120> s_watch;            // the last two seconds he was whole
	static Watchdog::Policy s_watchPolicy;
	static Watchdog::CameraCheck s_camCheck;
	static uint32_t s_watchFrame = 0;
	static int s_watchReady = 0;                      // 1: the functions above are as expected, -1: not
	static int s_watchReports = 0;                    // reports written this session (at most kWatchReports)
	static const int kWatchReports = 12;
	static int s_watchBad = 0;                        // frames in a row his numbers were bad
	static int s_camBad = 0;                          // frames in a row the camera's were
	static int s_camTries = 0;
	static uint8_t* s_watchSkater = 0;                // the skater and camera objects last seen, for the log
	static uint8_t* s_watchCamera = 0;
	static bool s_camTold = false;
	static bool s_watchOn = true;                     // [Flickit] Watchdog
	static int s_viewReady = 0;                       // 1: the view and the camera target are where expected, -1: not
	static Watchdog::ViewPolicy s_viewPolicy;
	static Watchdog::Blackout s_blackout;
	static int s_targetBad = 0;                       // frames in a row the camera target was not numbers

	// The custom restart where he stands, as autosave does it (defined with travel, below)
	static bool RestartWhereHeIs(uint8_t* skater, const float pos[3], const float matrix[16]);

	void SetWatch(bool on) {
		s_watchOn = on;
	}

	static bool ViewReady() {
		if (s_viewReady == 0) {
			s_viewReady = 1;
			for (size_t i = 0; i < sizeof(kViewExpect) / sizeof(kViewExpect[0]); i++) {
				if (memcmp((const void*)kViewExpect[i].address, kViewExpect[i].bytes, (size_t)kViewExpect[i].size) != 0) {
					WatchLog("Watchdog: the game's code at %08X (%s) is not what was expected; the view and the camera target are not watched\n",
						kViewExpect[i].address, kViewExpect[i].what);
					s_viewReady = -1;
				}
			}
		}
		return s_viewReady > 0;
	}

	bool CameraView(float eye[3], float view[16]) {
		if (!ViewReady()) {
			return false;
		}
		memcpy(eye, kViewEye, 3 * sizeof(float));
		memcpy(view, kViewMatrix, 16 * sizeof(float));
		return Watchdog::Sane(eye, 3) && Watchdog::Sane(view, 16);
	}

	static bool WatchReady() {
		if (s_watchReady == 0) {
			s_watchReady = 1;
			for (size_t i = 0; i < sizeof(kWatchExpect) / sizeof(kWatchExpect[0]); i++) {
				if (memcmp((const void*)kWatchExpect[i].address, kWatchExpect[i].bytes, (size_t)kWatchExpect[i].size) != 0) {
					WatchLog("Watchdog: the game's code at %08X (%s) is not what was expected; it watches the skater only and does not reset him\n",
						kWatchExpect[i].address, kWatchExpect[i].what);
					s_watchReady = -1;
				}
			}
			if (Flickit::NameChecksum("SkaterCam0") != kSkaterCam0) {
				s_watchReady = -1;
			}
		}
		return s_watchReady > 0;
	}

	static uint8_t* SkaterCamera() {
		if (!WatchReady()) {
			return 0;
		}
		const uint8_t* manager = (const uint8_t*)*kObjectManager;
		if (!manager) {
			return 0;
		}
		for (int off = 8; off <= 0xC; off += 4) {
			void* table = *(void* const*)(manager + off);
			if (table) {
				void* object = TableLookup(table, &kSkaterCam0, 1);
				if (object) {
					return (uint8_t*)object;
				}
			}
		}
		return 0;
	}

	static void ReadSample(uint8_t* skater, const uint8_t* core, uint8_t* camera, Watchdog::Sample& s) {
		memset(&s, 0, sizeof(s));
		s.frame = s_watchFrame;
		s.tick = GetTickCount();
		const uint8_t* state = *(const uint8_t* const*)(core + CORE_STATE);
		s.state = state ? *(const int32_t*)(state + STATE_STATE) : -1;
		const ScriptAt at = ScriptOf(skater);
		s.script = at.running;
		s.caller = at.caller;
		memcpy(s.pos, skater + OBJECT_POSITION, sizeof(s.pos));
		memcpy(s.vel, skater + OBJECT_VELOCITY, sizeof(s.vel));
		memcpy(s.matrix, skater + OBJECT_MATRIX, sizeof(s.matrix));
		memcpy(s.display, core + CORE_DISPLAY_MATRIX, sizeof(s.display));
		const uint8_t* node = *(const uint8_t* const*)(core + CORE_RAIL_NODE);
		if (s.state == 4 && node) {
			s.onRail = true;
			memcpy(s.rail, node + RAIL_POSITION, sizeof(s.rail));
			const uint8_t* next = *(const uint8_t* const*)(node + RAIL_NEXT);
			const uint8_t* previous = *(const uint8_t* const*)(node + RAIL_PREVIOUS);
			if (next) {
				s.hasNext = true;
				memcpy(s.next, next + RAIL_POSITION, sizeof(s.next));
			}
			if (previous) {
				s.hasPrevious = true;
				memcpy(s.previous, previous + RAIL_POSITION, sizeof(s.previous));
			}
		}
		if (camera) {
			s.camKnown = true;
			memcpy(s.cam, camera + OBJECT_POSITION, sizeof(s.cam));
			memcpy(s.camMatrix, camera + OBJECT_MATRIX, sizeof(s.camMatrix));
		}
		if (ViewReady()) {
			const uint8_t* target = *(const uint8_t* const*)(skater + SKATER_CAMERA_TARGET);
			if ((uintptr_t)target >= 0x10000 && ((uintptr_t)target & 3) == 0) {
				s.targetKnown = true;
				memcpy(s.target, target + TARGET_POS, sizeof(s.target));
				memcpy(s.targetUp, target + TARGET_UP, sizeof(s.targetUp));
			}
			s.viewKnown = true;
			memcpy(s.eye, kViewEye, sizeof(s.eye));
			memcpy(s.at, kViewAlong, sizeof(s.at));
			memcpy(s.up, kViewUp, sizeof(s.up));
			memcpy(s.view, kViewMatrix, sizeof(s.view));
			memcpy(s.proj, kProjMatrix, sizeof(s.proj));
		}
		s.draws = CamRelGame::Counting() ? CamRelGame::DrawCount() : 0;
	}

	// The camera target: numbers, and an up about a unit long
	static bool TargetSane(const Watchdog::Sample& s) {
		if (!s.targetKnown) {
			return true;
		}
		const float len2 = s.targetUp[0] * s.targetUp[0] + s.targetUp[1] * s.targetUp[1] + s.targetUp[2] * s.targetUp[2];
		return Watchdog::Sane(s.target, 3) && Watchdog::Sane(s.targetUp, 3) && len2 > 0.25f && len2 < 4.0f;
	}

	static bool ViewOk(const Watchdog::Sample& s) {
		return !s.viewKnown || Watchdog::ViewSane(s.view, s.proj);
	}

	// The orientation rows of a matrix (the fourth is checked with them only where it is a position)
	static bool SkaterSane(const Watchdog::Sample& s, const char** what) {
		*what = !Watchdog::Sane(s.pos, 3) ? "position" : !Watchdog::Sane(s.vel, 3) ? "speed"
			: !Watchdog::Sane(s.matrix, 12) ? "matrix" : !Watchdog::Sane(s.display, 12) ? "drawn matrix" : 0;
		return *what == 0;
	}

	static bool CameraSane(const Watchdog::Sample& s, const char** what) {
		*what = !Watchdog::Sane(s.cam, 3) ? "camera position" : !Watchdog::Sane(s.camMatrix, 12) ? "camera matrix" : 0;
		return *what == 0;
	}

	static void LogSample(const char* label, const Watchdog::Sample& s) {
		const float speed = std::sqrt(s.vel[0] * s.vel[0] + s.vel[1] * s.vel[1] + s.vel[2] * s.vel[2]);
		char rail[160] = "";
		if (s.onRail) {
			sprintf_s(rail, sizeof(rail), ", rail node at (%.1f %.1f %.1f)%s", s.rail[0], s.rail[1], s.rail[2],
				s.hasNext ? "" : " (an end)");
			if (s.hasNext) {
				const size_t n = strlen(rail);
				sprintf_s(rail + n, sizeof(rail) - n, " to (%.1f %.1f %.1f)", s.next[0], s.next[1], s.next[2]);
			}
		}
		char cam[80] = "";
		if (s.camKnown) {
			sprintf_s(cam, sizeof(cam), ", camera at (%.1f %.1f %.1f)", s.cam[0], s.cam[1], s.cam[2]);
		}
		WatchLog("Watchdog: %s frame %u, state %d, script %08X from %08X, at (%.2f %.2f %.2f), moving (%.2f %.2f %.2f) %.1f/s, facing (%.3f %.3f %.3f)%s%s\n",
			label, s.frame, s.state, s.script, s.caller, s.pos[0], s.pos[1], s.pos[2], s.vel[0], s.vel[1], s.vel[2], speed,
			s.matrix[8], s.matrix[9], s.matrix[10], rail, cam);
	}

	// What the camera works from, for the newest frames of a report
	static void LogDetails(const char* label, const Watchdog::Sample& s) {
		WatchLog("Watchdog: %s his matrix %s, up (%.3f %.3f %.3f); drawn matrix %s; camera matrix %s, rows (%.3f %.3f %.3f) (%.3f %.3f %.3f) (%.3f %.3f %.3f)\n",
			label, Watchdog::Square(s.matrix) ? "square" : "NOT square", s.matrix[4], s.matrix[5], s.matrix[6],
			Watchdog::Square(s.display) ? "square" : "NOT square", !s.camKnown ? "unknown" : Watchdog::Square(s.camMatrix) ? "square" : "NOT square",
			s.camMatrix[0], s.camMatrix[1], s.camMatrix[2], s.camMatrix[4], s.camMatrix[5], s.camMatrix[6], s.camMatrix[8], s.camMatrix[9], s.camMatrix[10]);
		if (s.targetKnown || s.viewKnown) {
			WatchLog("Watchdog: %s camera target at (%.2f %.2f %.2f) up (%.3f %.3f %.3f)%s; the renderer's eye (%.2f %.2f %.2f), along (%.3f %.3f %.3f), up (%.3f %.3f %.3f); its view %s, row 4 (%.1f %.1f %.1f); draws %u\n",
				label, s.target[0], s.target[1], s.target[2], s.targetUp[0], s.targetUp[1], s.targetUp[2], TargetSane(s) ? "" : " NOT whole",
				s.eye[0], s.eye[1], s.eye[2], s.at[0], s.at[1], s.at[2], s.up[0], s.up[1], s.up[2],
				ViewOk(s) ? "whole" : "NOT numbers", s.view[12], s.view[13], s.view[14], s.draws);
		}
	}

	static void Report(const Watchdog::Sample& now, const char* what, const char* happened = "stopped being a number") {
		if (s_watchReports >= kWatchReports) {
			return;
		}
		s_watchReports++;
		WatchLog("Watchdog: the %s %s%s (report %d). Skater object at %08X, camera at %08X%s\n",
			what, happened, s_rail.hasPick ? " on a rail Flickit's pick started" : "", s_watchReports, (uint32_t)(uintptr_t)s_watchSkater,
			(uint32_t)(uintptr_t)s_watchCamera, (uintptr_t)s_watchSkater >= 0x80000000u ? " (the skater is above 2 GB)" : "");
		LogSample("now:", now);
		LogDetails("now:", now);
		WatchLog("Watchdog: Flickit on the rail: pick %d (%s), grind %d, drag class %d, active %d\n",
			s_rail.pick, s_rail.hasPick ? "its own" : "none", s_grind, s_rail.dragClass, s_active ? 1 : 0);
		// the frames before, oldest first: every frame of the last half second, then every fifth
		const int count = s_watch.Count();
		for (int back = count - 1; back >= 0; back--) {
			if (back >= 30 && back % 5 != 0) {
				continue;
			}
			char label[24];
			sprintf_s(label, sizeof(label), "-%d:", back + 1);
			LogSample(label, *s_watch.Back(back));
			if (back < 5) {
				LogDetails(label, *s_watch.Back(back));
			}
		}
	}

	// Writes the newest whole frame back over the skater, if it is from the last two seconds
	static bool RestoreSkater(uint8_t* skater, uint8_t* core) {
		const Watchdog::Sample* good = s_watch.Back(0);
		if (!good || s_watchFrame - good->frame > 120) {
			return false;
		}
		memcpy(skater + OBJECT_POSITION, good->pos, sizeof(good->pos));
		memcpy(skater + OBJECT_VELOCITY, good->vel, sizeof(good->vel));
		memcpy(skater + OBJECT_MATRIX, good->matrix, sizeof(good->matrix));
		memcpy(core + CORE_DISPLAY_MATRIX, good->display, sizeof(good->display));
		return true;
	}

	// The newest whole camera of the history, from skip frames back (the view is made after the frame's
	// update, so the camera of the newest whole frame may be the one that made a bad view)
	static void RestoreCamera(uint8_t* camera, int skip = 0) {
		for (int back = skip; back < s_watch.Count(); back++) {
			const Watchdog::Sample* good = s_watch.Back(back);
			const char* what;
			if (good->camKnown && CameraSane(*good, &what)) {
				memcpy(camera + OBJECT_POSITION, good->cam, sizeof(good->cam));
				memcpy(camera + OBJECT_MATRIX, good->camMatrix, sizeof(good->camMatrix));
				return;
			}
		}
	}

	// The camera target, the view and the level's drawing; returns whether the view is whole. Only when the
	// game's code for them is as expected (ViewReady, through ReadSample).
	static int s_viewBadFrames = 0;
	static bool WatchView(uint8_t* skater, const uint8_t* core, uint8_t* camera, const Watchdog::Sample& now, bool skaterOk) {
		if (!now.viewKnown) {
			return true;
		}
		// the camera target: put back on him, the way the restart puts it, when it is not numbers
		if (!TargetSane(now)) {
			if (s_targetBad++ == 0) {
				Report(now, "camera target");
			}
			uint8_t* target = *(uint8_t* const*)(skater + SKATER_CAMERA_TARGET);
			if (now.targetKnown && target && skaterOk) {
				SetTargetPos(target, (const float*)(skater + OBJECT_POSITION));
				SetTargetUp(target, (const float*)(skater + OBJECT_MATRIX) + 4);
				if (s_targetBad == 1) {
					WatchLog("Watchdog: put the camera target back on the skater\n");
				}
			}
		}
		else if (s_targetBad > 0) {
			WatchLog("Watchdog: the camera target is whole again, %d frames later\n", s_targetBad);
			s_targetBad = 0;
		}
		// the level drawn at all (the mesh draws the camera-relative drawing counts)
		if (CamRelGame::Counting()) {
			const int dark = s_blackout.Frame(now.draws);
			if (dark == 1) {
				Report(now, "level", "has not been drawn for half a second");
			}
			else if (dark < 0) {
				WatchLog("Watchdog: the level is drawn again, %d frames later\n", -dark);
			}
		}
		// the view: the camera back first, then the game's restart where he stands
		const bool viewOk = ViewOk(now);
		const uint8_t* input = *(const uint8_t* const*)(core + CORE_INPUT);
		const uint8_t* state = *(const uint8_t* const*)(core + CORE_STATE);
		const bool restartable = skaterOk && input && input[INPUT_DISABLED] == 0 && input[INPUT_NET_DISABLED] == 0
			&& state && state[STATE_DRIVING] == 0;
		const Watchdog::ViewAction action = s_viewPolicy.Frame(viewOk, restartable);
		if (viewOk) {
			if (s_viewBadFrames > 0) {
				WatchLog("Watchdog: the view is whole again, %d frames later\n", s_viewBadFrames);
				s_viewBadFrames = 0;
			}
			return true;
		}
		s_viewBadFrames++;
		if (action == Watchdog::VIEW_CAMERA) {
			Report(now, "view the level is drawn with", "stopped being numbers");
			if (camera) {
				RestoreCamera(camera, 2);
				WatchLog("Watchdog: put the camera back as it was a few frames ago\n");
			}
		}
		else if (action == Watchdog::VIEW_RESTART) {
			const bool done = RestartWhereHeIs(skater, now.pos, now.matrix);
			WatchLog(done ? "Watchdog: had the game restart him where he is, for the view (try %d)\n"
				: "Watchdog: could not restart him for the view (try %d)\n", s_viewPolicy.Restarts());
		}
		else if (action == Watchdog::VIEW_GIVE_UP) {
			WatchLog("Watchdog: the view will not come back; leaving it be until it does\n");
		}
		return false;
	}

	// Once a frame, before the physics, for the local skater
	static void Watch(uint8_t* skater, uint8_t* core) {
		s_watchFrame++;
		uint8_t* camera = SkaterCamera();
		if (skater != s_watchSkater || camera != s_watchCamera) {
			if (skater != s_watchSkater) {
				s_watch.Clear();
			}
			if (camera != s_watchCamera) {
				s_camCheck.Reset();
				s_camTold = false;
			}
			s_watchSkater = skater;
			s_watchCamera = camera;
		}
		Watchdog::Sample now;
		ReadSample(skater, core, camera, now);
		const char* what = 0;
		const bool skaterOk = SkaterSane(now, &what);
		const char* camWhat = 0;
		const bool camOk = !camera || CameraSane(now, &camWhat);
		if (skaterOk && camOk && camera && !s_camCheck.Decided()) {
			s_camCheck.Feed(now.pos, now.cam, now.camMatrix);
			if (s_camCheck.Decided() && !s_camTold) {
				s_camTold = true;
				WatchLog(s_camCheck.Trusted() ? "Watchdog: watching the skater (object at %08X) and the camera (at %08X)%s%s\n"
					: "Watchdog: watching the skater (object at %08X); what was taken for the camera (at %08X) does not behave like it%s%s\n",
					(uint32_t)(uintptr_t)skater, (uint32_t)(uintptr_t)camera, now.viewKnown ? ", what it follows of him and the view the level is drawn with" : "",
					CamRelGame::Counting() ? ", and whether the level is drawn" : "");
			}
		}
		const bool watchCam = camera && s_camCheck.Trusted();

		// the skater (if and else, not a switch: an unoptimised build makes a switch a table of addresses in
		// the code, which check_hooks.py would take for a function)
		const Watchdog::Action action = s_watchPolicy.Frame(skaterOk);
		if (action == Watchdog::KEEP) {
			if (s_watchBad > 0) {
				WatchLog("Watchdog: the skater is whole again, %d frames later\n", s_watchBad);
				s_watchBad = 0;
			}
		}
		else if (action == Watchdog::RESTORE || action == Watchdog::RESET) {
			if (s_watchBad++ == 0) {
				Report(now, what);
			}
			const bool restored = RestoreSkater(skater, core);
			if (action == Watchdog::RESTORE) {
				WatchLog(restored ? "Watchdog: put the skater back as he was a frame ago\n"
					: "Watchdog: no whole frame of the skater to put back\n");
			}
			else if (WatchReady()) {
				ResetPhysics(skater, false, false);
				WatchLog("Watchdog: put the skater back and had the game reset his physics there (try %d)\n", s_watchPolicy.Tries());
			}
		}
		else {
			s_watchBad++;
			if (action == Watchdog::GIVE_UP) {
				WatchLog("Watchdog: the skater's %s will not stay a number; leaving him be until he is whole again\n", what);
			}
		}

		// the camera, once it has shown that it is the camera
		if (watchCam && camOk && s_camBad > 0) {
			WatchLog("Watchdog: the camera is whole again, %d frames later\n", s_camBad);
			s_camBad = 0;
			s_camTries = 0;
		}
		if (watchCam && !camOk) {
			if (s_camBad++ == 0) {
				Report(now, camWhat);
			}
			if (s_camTries < 30) {
				s_camTries++;
				RestoreCamera(camera);
				if (s_camTries == 1 || s_camTries == 30) {
					WatchLog(s_camTries == 1 ? "Watchdog: put the camera back as it was\n"
						: "Watchdog: the camera will not stay a number; leaving it be\n");
				}
			}
		}

		// what the camera follows, the view the level is drawn with, and whether it is drawn at all
		const bool viewOk = WatchView(skater, core, watchCam ? camera : 0, now, skaterOk);
		if (skaterOk && (camOk || !watchCam) && viewOk && TargetSane(now)) {
			s_watch.Push(now);
		}
	}

	void LogWatch() {
		uint8_t* skater = LocalSkater();
		const uint8_t* core = skater ? (const uint8_t*)GetComponent(skater, kSkaterCorePhysics) : 0;
		if (!skater || !core) {
			WatchLog("Watchdog: no skater\n");
			return;
		}
		Watchdog::Sample now;
		ReadSample(skater, core, SkaterCamera(), now);
		const char* what = 0;
		const char* camWhat = 0;
		const bool ok = SkaterSane(now, &what);
		const bool camOk = !now.camKnown || CameraSane(now, &camWhat);
		WatchLog("Watchdog: skater object at %08X, %s; camera at %08X, %s; %d reports so far\n",
			(uint32_t)(uintptr_t)skater, ok ? "whole" : what, (uint32_t)(uintptr_t)SkaterCamera(),
			!now.camKnown ? "not found" : (camOk ? (s_camCheck.Trusted() ? "whole" : "whole (not yet trusted as the camera)") : camWhat),
			s_watchReports);
		LogSample("now:", now);
		LogDetails("now:", now);
		if (CamRelGame::Counting()) {
			WatchLog("Watchdog: the level %s\n", s_blackout.Frames() > 0 ? "was not drawn on the last frames" : "is being drawn");
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  Autosave: where he was last on the ground, back on a load     */
	/******************************************************************/

	// Skate::Instance() + 0x398: the checksum of the level loaded (its load script, load_NJ ...), as the
	// script command LevelIs reads it (0x00528DAA)
	static const uint32_t SKATE_LEVEL = 0x398;
	// The skater's custom restart, the one the pause menu's "Set Restart" keeps (his member function
	// SetCustomRestart, 0x00517405): with Set it copies his position (+0x40) to +0x2C8 and his matrix (+0x60)
	// to +0x2D8, keeps whether he walks at +0x318 and that there is one at +0x319. SkipToCustomRestart
	// (0x0051776F) goes on from there: the game's reset of his physics, then the position and matrix put back
	// with all that goes with a move (the drawn matrix, the positions of the frame before, the camera).
	enum {
		SKATER_CUSTOM_POS = 0x2C8,
		SKATER_CUSTOM_MATRIX = 0x2D8,
		SKATER_CUSTOM_WALK = 0x318,
		SKATER_CUSTOM_SET = 0x319,
	};
	// Script::RunScript (0x0040FD00), as THUG-SDL calls it: the name of a script, its parameters, the object
	typedef bool(__cdecl* RunScript_t)(uint32_t name, void* params, void* object, void* object2);
	static const RunScript_t RunGameScript = (RunScript_t)0x0040FD00;
	// The pause menu's script for "Goto Cust Restart": skater : SkipToCustomRestart
	static const uint32_t kSkipToCustomRestart = 0x3E77D9CA;  // skip_to_custom_restart

	// Script::RunScript runs a script to its end, but what it gives back is no yes or no for a script: the low
	// byte of the address its CScript had, from the free just before it returns (0x0040FE2C -> 0x0040E370 ->
	// 0x0057A960), so 0 for about one in 64. Only a missing script gives 0 for sure. So: is it a script, then run it.
	static bool RunScriptOf(uint32_t name, void* params) {
		const uint8_t* symbol = (const uint8_t*)LookUpSymbol(name);
		if (!symbol || symbol[1] != 7) {
			return false;
		}
		RunGameScript(name, params, 0, 0);
		return true;
	}

	static const Expect kAutosaveExpect[] = {
		{ 0x00517428, 18, { 0x8D, 0x46, 0x40, 0x50, 0x8D, 0x8E, 0xC8, 0x02, 0x00, 0x00, 0xC6, 0x86, 0x19, 0x03, 0x00, 0x00, 0x01, 0xE8 }, "SetCustomRestart: the position and the flag" },
		{ 0x0051743E, 10, { 0x8D, 0x4E, 0x60, 0x51, 0x8D, 0x8E, 0xD8, 0x02, 0x00, 0x00 }, "SetCustomRestart: the matrix" },
		{ 0x00517458, 6, { 0x88, 0x86, 0x18, 0x03, 0x00, 0x00 }, "SetCustomRestart: walking" },
		{ 0x0051776F, 11, { 0x3D, 0xE9, 0x19, 0x3C, 0x5A, 0x0F, 0x84, 0xE4, 0xFC, 0xFF, 0xFF }, "SkipToCustomRestart" },
		{ 0x00528DAA, 6, { 0x8B, 0x8E, 0x98, 0x03, 0x00, 0x00 }, "LevelIs: the level loaded" },
		{ 0x0040FD00, 12, { 0x6A, 0xFF, 0x68, 0x5B, 0x18, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "Script::RunScript" },
	};

	static bool s_autosaveOn = true;                  // [Flickit] Autosave
	static uint32_t s_autosaveLevels[8];              // [Flickit] AutosaveLevels, as checksums
	static int s_autosaveLevelCount = 0;
	static int s_autosaveReady = 0;                   // 1: the game is as expected, -1: not
	static Autosave::Slots s_slots;
	static Autosave::Throttle s_throttle;
	static Autosave::Session s_session;
	static bool s_slotsRead = false;
	static bool s_writeTold = false;
	static bool s_writeFailTold = false;
	static char s_savePath[MAX_PATH] = "";
	static char s_saveTemp[MAX_PATH] = "";

	void SetAutosave(bool on, const char* levels) {
		s_autosaveOn = on;
		s_autosaveLevelCount = 0;
		char name[64];
		const char* p = levels ? levels : "";
		while (*p && s_autosaveLevelCount < (int)(sizeof(s_autosaveLevels) / sizeof(s_autosaveLevels[0]))) {
			while (*p == ' ' || *p == ',') {
				p++;
			}
			size_t n = 0;
			while (p[n] && p[n] != ',' && p[n] != ' ' && n + 1 < sizeof(name)) {
				name[n] = p[n];
				n++;
			}
			if (n == 0) {
				break;
			}
			name[n] = 0;
			p += n;
			s_autosaveLevels[s_autosaveLevelCount++] = Flickit::NameChecksum(name);
		}
	}

	static bool AutosaveReady() {
		if (s_autosaveReady == 0) {
			s_autosaveReady = 1;
			for (size_t i = 0; i < sizeof(kAutosaveExpect) / sizeof(kAutosaveExpect[0]); i++) {
				if (memcmp((const void*)kAutosaveExpect[i].address, kAutosaveExpect[i].bytes, (size_t)kAutosaveExpect[i].size) != 0) {
					Log::TypedLog(CHN_SDL, "Autosave: the game's code at %08X (%s) is not what was expected; autosave is off\n",
						kAutosaveExpect[i].address, kAutosaveExpect[i].what);
					s_autosaveReady = -1;
				}
			}
			if (Flickit::NameChecksum("skip_to_custom_restart") != kSkipToCustomRestart) {
				s_autosaveReady = -1;
			}
			if (s_autosaveReady > 0) {
				// next to the exe
				char dir[MAX_PATH];
				const DWORD n = GetModuleFileNameA(NULL, dir, MAX_PATH);
				char* slash = n ? strrchr(dir, '\\') : 0;
				if (!slash) {
					s_autosaveReady = -1;
				}
				else {
					slash[1] = 0;
					sprintf_s(s_savePath, sizeof(s_savePath), "%sflickit_autosave.txt", dir);
					sprintf_s(s_saveTemp, sizeof(s_saveTemp), "%sflickit_autosave.tmp", dir);
				}
			}
		}
		return s_autosaveReady > 0;
	}

	static void ReadSlots() {
		if (s_slotsRead) {
			return;
		}
		s_slotsRead = true;
		HANDLE f = CreateFileA(s_savePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (f == INVALID_HANDLE_VALUE) {
			Log::TypedLog(CHN_SDL, "Autosave: no flickit_autosave.txt yet\n");
			return;
		}
		static char text[64 * 1024];
		DWORD got = 0;
		const BOOL ok = ReadFile(f, text, sizeof(text) - 1, &got, NULL);
		CloseHandle(f);
		text[ok ? got : 0] = 0;
		s_slots.Read(text);
		Log::TypedLog(CHN_SDL, "Autosave: read flickit_autosave.txt, %d levels in it\n", s_slots.Count());
	}

	// To a file of its own, then over the old one: a crash while writing leaves the old one whole
	static void WriteSlots() {
		const std::string text = s_slots.Text();
		HANDLE f = CreateFileA(s_saveTemp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		bool ok = f != INVALID_HANDLE_VALUE;
		if (ok) {
			DWORD put = 0;
			ok = WriteFile(f, text.data(), (DWORD)text.size(), &put, NULL) && put == (DWORD)text.size();
			CloseHandle(f);
		}
		ok = ok && MoveFileExA(s_saveTemp, s_savePath, MOVEFILE_REPLACE_EXISTING);
		if (!ok && !s_writeFailTold) {
			s_writeFailTold = true;
			Log::TypedLog(CHN_SDL, "Autosave: could not write %s (error %u)\n", s_savePath, (unsigned)GetLastError());
		}
		if (ok && !s_writeTold) {
			s_writeTold = true;
			Log::TypedLog(CHN_SDL, "Autosave: keeping the skater's spot in %s\n", s_savePath);
		}
	}

	// Skating on the ground, not in a bail, the controls his, his numbers whole
	static bool FreeOnGround(const uint8_t* skater, const uint8_t* core) {
		const uint8_t* state = *(const uint8_t* const*)(core + CORE_STATE);
		const uint8_t* input = *(const uint8_t* const*)(core + CORE_INPUT);
		if (!state || !input) {
			return false;
		}
		if (*(const int32_t*)(state + STATE_STATE) != 0 || *(const int32_t*)(state + STATE_PHYSICS) != 0
			|| state[STATE_BAILING] != 0 || state[STATE_DRIVING] != 0) {
			return false;
		}
		if (input[INPUT_DISABLED] != 0 || input[INPUT_NET_DISABLED] != 0) {
			return false;
		}
		return Watchdog::Sane((const float*)(skater + OBJECT_POSITION), 3) && Watchdog::Sane((const float*)(skater + OBJECT_MATRIX), 12);
	}

	static void RestoreSpot(uint8_t* skater, uint32_t level) {
		ReadSlots();
		const Autosave::Spot* spot = s_slots.Find(level);
		if (!spot) {
			Log::TypedLog(CHN_SDL, "Autosave: no spot kept for this level yet; he starts where the level starts\n");
			return;
		}
		memcpy(skater + SKATER_CUSTOM_POS, spot->pos, sizeof(spot->pos));
		memcpy(skater + SKATER_CUSTOM_MATRIX, spot->matrix, sizeof(spot->matrix));
		skater[SKATER_CUSTOM_WALK] = 0;
		skater[SKATER_CUSTOM_SET] = 1;
		GameStruct none = { 0 };
		const bool ran = RunScriptOf(kSkipToCustomRestart, &none);
		const uint32_t now = (uint32_t)time(0);
		const unsigned age = spot->saved && now >= spot->saved ? (unsigned)(now - spot->saved) : 0;
		Log::TypedLog(CHN_SDL, "Autosave: %s (%.1f %.1f %.1f), kept %u min %u s ago\n",
			ran ? "put the skater back where he was" : "could not run skip_to_custom_restart to put the skater back at",
			spot->pos[0], spot->pos[1], spot->pos[2], age / 60, age % 60);
		s_throttle.Wrote(*spot, GetTickCount());
	}

	static bool s_restoreAhead = false;               // autosave is still to put him back in this level

	// Once a frame, before the physics, for the local skater
	static void AutosaveFrame(uint8_t* skater, uint8_t* core) {
		s_restoreAhead = false;
		if (!s_autosaveOn || !AutosaveReady()) {
			return;
		}
		const uint8_t* skate = (const uint8_t*)*kSkate;
		if (!skate) {
			return;
		}
		const uint32_t level = *(const uint32_t*)(skate + SKATE_LEVEL);
		bool listed = false;
		for (int i = 0; i < s_autosaveLevelCount; i++) {
			listed = listed || s_autosaveLevels[i] == level;
		}
		const bool onGround = listed && FreeOnGround(skater, core);
		const Autosave::Step step = s_session.Frame(level, BigWorld::NodeLoads(), skater, onGround);
		s_restoreAhead = listed && step == Autosave::WAIT;
		if (!listed || step == Autosave::WAIT) {
			return;
		}
		if (step == Autosave::RESTORE) {
			RestoreSpot(skater, level);
			return;
		}
		if (!onGround) {
			return;
		}
		Autosave::Spot spot;
		memset(&spot, 0, sizeof(spot));
		spot.level = level;
		spot.saved = (uint32_t)time(0);
		memcpy(spot.pos, skater + OBJECT_POSITION, sizeof(spot.pos));
		memcpy(spot.matrix, skater + OBJECT_MATRIX, sizeof(spot.matrix));
		if (!Autosave::Slots::Whole(spot)) {
			return;
		}
		const uint32_t tick = GetTickCount();
		if (s_throttle.Due(spot, tick)) {
			ReadSlots();            // the other levels' spots stay in the file
			s_slots.Put(spot);
			WriteSlots();
			s_throttle.Wrote(spot, tick);
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  The map: where he is, and travel                              */
	/******************************************************************/

	static Whereabouts s_where;
	static bool s_travelAsked = false;
	static float s_travelPos[3];
	static float s_travelHeading = 0.0f;

	bool SkaterWhereabouts(Whereabouts& out) {
		if (!s_where.tick) {
			return false;
		}
		out = s_where;
		return true;
	}

	bool RequestTravel(const float pos[3], float heading) {
		if (!s_installed || !AutosaveReady() || !Watchdog::Sane(pos, 3) || !(heading == heading)) {
			return false;
		}
		memcpy(s_travelPos, pos, sizeof(s_travelPos));
		s_travelHeading = heading;
		s_travelAsked = true;
		return true;
	}

	// The custom restart with this spot, as autosave uses it: the game's reset of his physics, then the spot,
	// with all that goes with a move (the camera target too). His own custom restart goes back after, once the
	// skip has put him there; if it has not, the custom restart is left at the spot.
	static bool SkipTo(uint8_t* skater, const float spot[3], const float matrix[16], bool* ranOut) {
		uint8_t kept[SKATER_CUSTOM_SET + 1 - SKATER_CUSTOM_POS];
		memcpy(kept, skater + SKATER_CUSTOM_POS, sizeof(kept));
		float pos[4];
		memcpy(pos, skater + OBJECT_POSITION, sizeof(pos));
		memcpy(pos, spot, 3 * sizeof(float));
		memcpy(skater + SKATER_CUSTOM_POS, pos, sizeof(pos));
		memcpy(skater + SKATER_CUSTOM_MATRIX, matrix, 16 * sizeof(float));
		skater[SKATER_CUSTOM_WALK] = 0;
		skater[SKATER_CUSTOM_SET] = 1;
		GameStruct none = { 0 };
		const bool ran = RunScriptOf(kSkipToCustomRestart, &none);
		// (across only: the ground under the spot may set his height)
		const float* now = (const float*)(skater + OBJECT_POSITION);
		const bool there = ran && fabsf(now[0] - pos[0]) < 1.0f && fabsf(now[2] - pos[2]) < 1.0f;
		if (there) {
			memcpy(skater + SKATER_CUSTOM_POS, kept, sizeof(kept));
		}
		if (ranOut) {
			*ranOut = ran;
		}
		return there;
	}

	static bool RestartWhereHeIs(uint8_t* skater, const float pos[3], const float matrix[16]) {
		return AutosaveReady() && SkipTo(skater, pos, matrix, 0);
	}

	// His matrix turned to a heading, level (the rows as the game keeps them: side, up, the way he faces)
	static void HeadingMatrix(const uint8_t* skater, float heading, float m[16]) {
		memcpy(m, skater + OBJECT_MATRIX, 16 * sizeof(float));
		const float c = cosf(heading), s = sinf(heading);
		m[0] = c;
		m[1] = 0.0f;
		m[2] = -s;
		m[4] = 0.0f;
		m[5] = 1.0f;
		m[6] = 0.0f;
		m[8] = s;
		m[9] = 0.0f;
		m[10] = c;
	}

	bool PutSkater(const float pos[3], float heading) {
		uint8_t* skater = LocalSkater();
		if (!s_installed || !skater || !AutosaveReady() || !Watchdog::Sane(pos, 3) || !(heading == heading)) {
			return false;
		}
		float m[16];
		HeadingMatrix(skater, heading, m);
		return SkipTo(skater, pos, m, 0);
	}

	bool LineBlocked(const float from[3], const float to[3]) {
		uint8_t* skater = LocalSkater();
		uint8_t* core = skater ? (uint8_t*)GetComponent(skater, kSkaterCorePhysics) : 0;
		if (!s_installed || !core || !Watchdog::Sane(from, 3) || !Watchdog::Sane(to, 3)) {
			return false;
		}
		float* start = (float*)(core + CORE_LINE_START);
		float* end = (float*)(core + CORE_LINE_END);
		memcpy(start, from, 3 * sizeof(float));
		start[3] = 1.0f;
		memcpy(end, to, 3 * sizeof(float));
		end[3] = 1.0f;
		return LineHit(core, 0x10, 0);
	}

	// To the spot, with a matrix turned to its heading
	static void Travel(uint8_t* skater) {
		float m[16];
		HeadingMatrix(skater, s_travelHeading, m);
		bool ran = false;
		const bool there = SkipTo(skater, s_travelPos, m, &ran);
		Log::TypedLog(CHN_SDL, "Map: %s (%.1f %.1f %.1f) facing %.0f degrees%s\n",
			there ? "travelled to" : ran ? "the skip ran but did not put him at" : "could not run skip_to_custom_restart to travel to",
			s_travelPos[0], s_travelPos[1], s_travelPos[2], s_travelHeading * 57.29578f,
			there ? "; his own custom restart is as it was" : ran ? "; the custom restart is left at that spot" : "");
	}

	static void MapFrame(uint8_t* skater, const uint8_t* core) {
		const float* p = (const float*)(skater + OBJECT_POSITION);
		const float* m = (const float*)(skater + OBJECT_MATRIX);
		if (Watchdog::Sane(p, 3) && Watchdog::Sane(m, 12)) {
			const uint8_t* state = *(const uint8_t* const*)(core + CORE_STATE);
			const uint8_t* input = *(const uint8_t* const*)(core + CORE_INPUT);
			const uint32_t tick = GetTickCount();
			s_where.tick = tick ? tick : 1;
			memcpy(s_where.pos, p, sizeof(s_where.pos));
			s_where.heading = atan2f(m[8], m[10]);
			s_where.controls = input && input[INPUT_DISABLED] == 0 && input[INPUT_NET_DISABLED] == 0;
			s_where.driving = state && state[STATE_DRIVING] != 0;
			s_where.settled = !s_restoreAhead;
		}
		if (s_travelAsked) {
			s_travelAsked = false;
			Travel(skater);
		}
	}

	// The other players (players_game.h) and the cars (drive_game.h): him as they need him, once a frame after
	// the game has read his pad. Circle is for the nearer of a player and a car in reach.
	static void CarsFrame(uint8_t* skater, const uint8_t* core, uint8_t* input) {
		const uint8_t* state = *(const uint8_t* const*)(core + CORE_STATE);
		DriveGame::Skater s;
		s.object = skater;
		s.pos = (const float*)(skater + OBJECT_POSITION);
		s.take = input + INPUT_CIRCLE_PRESSED;
		s.otherAway = 1.0e9f;
		s.driving = state && state[STATE_DRIVING] != 0;
		s.free = state && !s.driving && state[STATE_BAILING] == 0 && *(const int32_t*)(state + STATE_STATE) == 0
			&& input[INPUT_DISABLED] == 0 && input[INPUT_NET_DISABLED] == 0
			&& Watchdog::Sane((const float*)(skater + OBJECT_POSITION), 3);
		PlayersGame::Frame(s);
		s.otherAway = PlayersGame::NearestAway();
		DriveGame::PadFrame(s);
		PlayersGame::AfterCars(s, DriveGame::CarAway());
	}

	// The news of the collision's faces in a box (BigWorld), once a second and 30 times a session at most:
	// written here, between frames, rather than from inside the collision
	static void FaceListFrame() {
		static DWORD s_last = 0;
		static int s_notes = 0;
		const DWORD now = GetTickCount();
		if (s_notes >= 30 || (s_last && now - s_last < 1000)) {
			return;
		}
		s_last = now;
		char news[400];
		if (BigWorld::FaceListNews(news, sizeof(news))) {
			WatchNote(news);
			if (++s_notes == 30) {
				WatchNote("BigWorld: no more notes on the faces in a box this session");
			}
		}
	}

	static void __fastcall PadUpdateHook(uint8_t* pad, void* /*edx*/, void* data) {
		PadUpdate(pad, data);
		if (s_turnKept && s_turnKept + INPUT_PAD == pad) {
			s_turnKept = 0;     // the game has just written both anew
		}
		uint8_t* skater = LocalSkater();
		const uint8_t* core = skater ? (const uint8_t*)GetComponent(skater, kSkaterCorePhysics) : 0;
		uint8_t* input = core ? *(uint8_t* const*)(core + CORE_INPUT) : 0;
		if (!input || input + INPUT_PAD != pad) {
			return;
		}
		if (s_watchOn) {
			Watch(skater, (uint8_t*)core);
		}
		AutosaveFrame(skater, (uint8_t*)core);
		MapFrame(skater, core);
		CarsFrame(skater, core, input);
		DayNightGame::Frame();
		FaceListFrame();
		RailExitFrame(skater, (uint8_t*)core);
		ShowCrouch(skater);
		KeepStance(skater);
		if (!s_active || (!s_standUp && (s_frame.push <= 0 || !s_frame.pushOverTurn || !s_push.Animating()))) {
			return;
		}
		if (s_standUp) {
			// A crouch of Flickit's ended without a flick. The jump button is up on this frame, and
			// the game jumps when it goes up after a crouch: so the crouch goes too, before the
			// physics get to look, and he stands up again. A button the player holds stays his.
			s_standUp = false;
			uint8_t* state = (uint8_t*)GetComponent(skater, kSkaterState);
			if (state && input[INPUT_JUMP_PRESSED] == 0) {
				state[STATE_CROUCHED] = 0;
			}
			if (s_frame.push <= 0 || !s_frame.pushOverTurn || !s_push.Animating()) {
				return;
			}
		}
		// The scripts look at Left and Right, and get to the push, in a loop of the ground (they
		// wait for the next frame in WaitOneGameFrame). While they are busy elsewhere (with a
		// landing, say) the time does not count. Nor while he crouches: then no push is played
		// (HeldHook), and his crouched turns are shown.
		const ScriptAt at = ScriptOf(skater);
		const bool loop = at.running == kGroundLoop || at.running == kNollieLoop
			|| (at.running == kWaitOneFrame && (at.caller == kGroundLoop || at.caller == kNollieLoop));
		if (!s_push.HideTurn(loop && !Crouching(skater), s_frame.seconds) || (input[INPUT_LEFT_PRESSED] == 0 && input[INPUT_RIGHT_PRESSED] == 0)) {
			return;
		}
		s_turnLeft = input[INPUT_LEFT_PRESSED];
		s_turnRight = input[INPUT_RIGHT_PRESSED];
		input[INPUT_LEFT_PRESSED] = 0;
		input[INPUT_RIGHT_PRESSED] = 0;
		s_turnKept = input;
	}

	// Stands where the physics of the ground ask "should he brake?". Same convention: this in ecx,
	// no arguments, the answer in al. The game answers yes for Down, and, with "auto kick" off,
	// for any speed under 50 units/s while the kick button is up: it stops the board dead there.
	static bool __fastcall BrakeTestHook(uint8_t* core) {
		GiveBackTurn(core);
		const bool brake = ShouldBrake(core);
		if (!Drives(core)) {
			return brake;
		}
		s_groundSeen = true;
		const uint8_t* input = *(const uint8_t* const*)(core + CORE_INPUT);
		const uint8_t* skater = *(const uint8_t* const*)(core + COMPONENT_OBJECT);
		if (!input || !skater) {
			return brake;
		}
		const bool down = input[INPUT_DOWN_PRESSED] != 0;
		if (s_frame.push > 0) {
			// A script may have switched "auto kick" back on a moment ago
			core[CORE_AUTO_KICK] = 0;
			if (down) {
				s_push.Reset();     // Down: THUG never kicks with it held, to one side or not. The foot comes back up
			}
			else if (s_push.Active() || s_frame.pushHeld) {
				// A board that is being pushed is not braking, and one that is about to be has to
				// get as far as the kick test below
				return false;
			}
		}
		if (brake && !down && (s_frame.coast > 0 || s_frame.push > 0) && Speed(skater) > kStandSpeed) {
			return false;   // nobody is braking: the board rolls out
		}
		return brake;
	}

	static int PushClipOf(uint32_t name) {
		for (int i = 0; i < PUSH_CLIPS; i++) {
			if (kPushClips[i].name == name) {
				return i;
			}
		}
		return -1;
	}

	// The animation the skater is in, and how far into it. Returns which of the push animations
	// it is, -1 for any other. Nothing is changed in the game.
	static int ReadPushAnim(const uint8_t* skater, Flickit::PushAnim& out) {
		out.current = false;
		out.running = false;
		out.time = 0.0f;
		out.length = 0.0f;
		const uint8_t* anim = (const uint8_t*)GetComponent((void*)skater, kAnimation);
		const uint8_t* channel = anim ? *(const uint8_t* const*)(anim + ANIM_FIRST) : 0;
		if (channel && *(const int32_t*)(channel + CHANNEL_SELF) == -1) {
			channel = 0;    // the head of an empty list: he is in no animation
		}
		const uint32_t name = channel ? *(const uint32_t*)(channel + CHANNEL_NAME) : 0;
		const float time = channel ? *(const float*)(channel + CHANNEL_TIME) : 0.0f;
		// The game has started an animation: there is another channel, or another name, or the
		// same animation is at an earlier point than it was
		if (channel != s_animChannel || name != s_animName || time < s_animTime) {
			s_animPlay++;
		}
		s_animChannel = channel;
		s_animName = name;
		s_animTime = time;
		out.play = s_animPlay;
		const int clip = channel ? PushClipOf(name) : -1;
		if (clip < 0) {
			return -1;
		}
		const float start = *(const float*)(channel + CHANNEL_START);
		const float end = *(const float*)(channel + CHANNEL_END);
		if (*(const int32_t*)(channel + CHANNEL_BACKWARDS) != 0 || !(end > start)) {
			return -1;      // run backwards, or of no length: nobody pushes in that
		}
		out.current = true;
		out.running = time < end;
		out.time = time - start;
		out.length = end - start;
		return clip;
	}

	// Where the skater's two feet are, in his own frame: three numbers each, the second the
	// height. The ball of each foot, as the game last posed the skeleton.
	static bool ReadToes(const uint8_t* skater, float* left, float* right) {
		const uint8_t* component = (const uint8_t*)GetComponent((void*)skater, kSkeleton);
		void* skeleton = component ? *(void* const*)(component + SKELETON_SKELETON) : 0;
		if (!skeleton) {
			return false;
		}
		float l[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float r[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		if (!BonePosition(skeleton, kToeLeft, l) || !BonePosition(skeleton, kToeRight, r)) {
			return false;
		}
		for (int i = 0; i < 3; i++) {
			left[i] = l[i];
			right[i] = r[i];
		}
		return true;
	}

	// A play of a push animation whose feet were watched is over: what they did. A play that
	// was seen whole and showed a foot pushing is what pushes go by from now on.
	static void FeetWatched() {
		s_footWatching = false;
		Flickit::FootResult result;
		s_foot.End(s_footLength, result);
		FootKnown& known = s_footKnown[s_footClip];
		bool news = false;
		if (result.why == Flickit::FOOT_OK) {
			news = !known.have || std::fabs(result.profile.down - known.profile.down) > kFootNews
				|| std::fabs(result.profile.up - known.profile.up) > kFootNews;
			known.have = true;
			known.profile = result.profile;
			known.stamp = ++s_footStamp;
		}
		else if ((s_footFirst <= 0.2f && s_footLast >= 0.85f) || (result.why != Flickit::FOOT_FEW && result.why != Flickit::FOOT_PART)) {
			// The animation ran from start to end under the watch, and no foot was found
			// pushing, or the feet could not be read. (A play that was cut short says nothing.)
			news = known.told != result.why + 1;
		}
		if (news) {
			known.told = result.why + 1;
			s_footToldClip = s_footClip;
			s_footToldLength = s_footLength;
			s_footTold = result;
			s_footTraceLooks = 0;
			if (!known.traced && result.why != Flickit::FOOT_FEW && result.why != Flickit::FOOT_PART) {
				known.traced = true;
				s_footTraceLooks = s_foot.Looks(s_footTrace, FOOT_TRACE_LOOKS);
			}
			s_footSerial++;
		}
	}

	// Once on every frame of the ground physics: the feet are watched through every play of a
	// push animation, whoever started it
	static void WatchFeet(const uint8_t* skater, const Flickit::PushAnim& anim, int clip) {
		if (s_footWatching && (clip < 0 || !anim.running || anim.play != s_footPlay)) {
			FeetWatched();
		}
		if (!s_frame.pushMeasure || clip < 0 || !anim.running) {
			s_footWatching = false;
			return;
		}
		if (!s_footWatching) {
			s_foot.Begin();
			s_footWatching = true;
			s_footPlay = anim.play;
			s_footClip = clip;
			s_footLength = anim.length;
			s_footFirst = anim.time / anim.length;
		}
		s_footLast = anim.time / anim.length;
		float left[3];
		float right[3];
		if (ReadToes(skater, left, right)) {
			s_foot.Sample(s_footLast, left, right);
		}
	}

	// Where the foot pushes in a push animation: what the feet showed in the push animation of
	// its kind that was watched last, or what the settings say. (The six of a skater standing
	// on his board move the feet alike once the game has blended into them; where the foot comes
	// down goes by what was played before, and the push before is the likeliest to be like this
	// one.)
	static int PushProfileFor(int clip, Flickit::PushProfile& out) {
		if (s_frame.pushMeasure && clip >= 0) {
			int best = -1;
			for (int i = 0; i < PUSH_CLIPS; i++) {
				if (s_footKnown[i].have && kPushClips[i].kind == kPushClips[clip].kind
					&& (best < 0 || s_footKnown[i].stamp > s_footKnown[best].stamp)) {
					best = i;
				}
			}
			if (best >= 0) {
				out = s_footKnown[best].profile;
				return best == clip ? PUSH_MEASURED : PUSH_BORROWED;
			}
		}
		Flickit::SmoothPushProfile(out, s_frame.pushFootDown, s_frame.pushFootUp);
		return PUSH_AS_SET;
	}

	// Stands where the physics of the ground ask "may he kick?", which they do on every frame the
	// skater is not braking and balances nothing. Same convention: this in ecx, no arguments, the
	// answer in al. With Flickit's push the answer is always no, and the push is given here: the
	// game's own kick, which would follow a yes, pushes for as long as it is allowed to.
	//
	// A push of Flickit's goes with the game's push animation. So this is also where the
	// skater's animation is looked at, and his feet while he is in a push animation.
	static bool __fastcall KickTestHook(uint8_t* core) {
		if (!Drives(core) || s_frame.push <= 0) {
			return ShouldKick(core);
		}
		uint8_t* skater = *(uint8_t**)(core + COMPONENT_OBJECT);
		const uint8_t* state = *(const uint8_t* const*)(core + CORE_STATE);
		const uint8_t* input = *(const uint8_t* const*)(core + CORE_INPUT);
		if (!skater || !state || !input) {
			return false;
		}
		s_pushAsked = true;
		// Where the game's own test says no whatever the speed, there is no push either: in a
		// bail, where a script has forbidden kicking, and with Down held. Nor where a script has
		// taken the controls away: the game empties its pad then, but the push buttons do not
		// come through the pad.
		if (state[STATE_BAILING] != 0 || core[CORE_NO_KICK] != 0 || input[INPUT_DOWN_PRESSED] != 0
			|| input[INPUT_DISABLED] != 0 || input[INPUT_NET_DISABLED] != 0) {
			s_push.Reset();
			return false;
		}
		const float dt = *(const float*)(core + CORE_FRAME_LENGTH);
		Flickit::PushAnim anim;
		const int clip = ReadPushAnim(skater, anim);
		WatchFeet(skater, anim, clip);
		const bool byClock = s_clock.Step(clip >= 0, dt, anim);
		if (s_clock.Serial() != s_clockSerial) {
			s_clockSerial = s_clock.Serial();
			s_clockAnim = s_animName;
		}
		Flickit::PushProfile profile;
		const int source = PushProfileFor(clip, profile);
		float* velocity = (float*)(skater + OBJECT_VELOCITY);
		const float speed = Speed(skater);
		const float add = s_push.Step(dt, speed, s_frame.pushHeld, s_frame.push, anim, profile);
		if ((clip >= 0 || byClock) && s_push.Phase() >= Flickit::PUSH_INTO && s_push.Play() == anim.play && s_pushPlay != anim.play) {
			// A push has taken up with this play of the animation: what the log says of it
			s_pushPlay = anim.play;
			s_pushClip = clip;
			s_pushLength = anim.length;
			s_pushSource = source;
		}
		if (add > 0.0f) {
			// The way the game's own kick goes: the way the skater faces, or the way he travels
			// when a script has locked that
			const float* forward = (const float*)(skater + OBJECT_FORWARD);
			float way[3] = { forward[0], forward[1], forward[2] };
			if (core[CORE_VELOCITY_LOCKED] != 0 && speed > 0.5f) {
				way[0] = velocity[0] / speed;
				way[1] = velocity[1] / speed;
				way[2] = velocity[2] / speed;
			}
			velocity[0] += way[0] * add;
			velocity[1] += way[1] * add;
			velocity[2] += way[2] * add;
		}
		return false;
	}

	// Stands where the script command CanKick asks the same question. Same convention. Two of
	// the game's scripts ask. The loop of a skater who rides nollie plays the push once for a
	// yes: that is the one a push of Flickit's answers. The other, at the end of a change of
	// stance on the ground, starts the push as a loop that the game's ground script then waits
	// on for good if it happens to pick the same animation: it gets the game's own answer.
	//
	// The nollie loop starts the push only if it is not the skater's animation already, and
	// puts another in its place only on a no. So while a push animation is his, running or
	// stopped at its end, the answer is the game's own no.
	static bool __fastcall CanKickHook(uint8_t* core) {
		const bool can = ShouldKick(core);
		if (!Drives(core) || s_frame.push <= 0 || !s_push.Animating() || s_push.AnimCurrent()) {
			return can;
		}
		const uint8_t* skater = *(const uint8_t* const*)(core + COMPONENT_OBJECT);
		const uint8_t* script = skater ? *(const uint8_t* const*)(skater + OBJECT_SCRIPT) : 0;
		if (!script || *(const uint32_t*)(script + SCRIPT_NAME) != kNollieLoop) {
			return can;
		}
		s_clock.Told();
		return true;
	}

	// Stands where the script command Held asks whether one button is down. With "auto kick" off
	// the game's scripts play the push while the Flip button is held: that button is the game's
	// own for a kick. The game does not get to see a press that is a push of Flickit's (the
	// input code keeps it back). Instead, for the skater Flickit drives, on his board on the
	// ground, the Flip button counts as held while a push of Flickit's wants its animation,
	// whichever button it came from. Same convention: this in ecx, one argument, which the
	// callee takes off the stack.
	static bool __fastcall HeldHook(uint8_t* trick, void* /*edx*/, uint32_t button) {
		const bool held = ButtonHeld(trick, button);
		if (button != kSquare || !s_active || s_frame.push <= 0 || !s_push.Animating()) {
			return held;
		}
		uint8_t* skater = LocalSkater();
		if (!skater || (uint8_t*)GetComponent(skater, kTrick) != trick) {
			return held;
		}
		const uint8_t* state = (const uint8_t*)GetComponent(skater, kSkaterState);
		if (!state || *(const int32_t*)(state + STATE_STATE) != 0 || state[STATE_DRIVING] != 0 || *(const int32_t*)(state + STATE_PHYSICS) != 0) {
			return held;    // in the air and off the board the button is what it always was
		}
		if (state[STATE_CROUCHED] != 0) {
			return held;    // a crouch comes before a push (Skate's order): no push out of a crouch
		}
		s_clock.Told();
		return true;
	}

	static int PushCutOf(uint32_t name) {
		for (int i = 0; i < PUSH_CUTS; i++) {
			if (kPushCuts[i].name == name) {
				return i;
			}
		}
		return -1;
	}

	// Stands where the script command AnimFinished asks whether the object's animation is
	// over. Same convention: this (the animation component) in ecx, no arguments, the answer
	// in al. After a landing, a skate-off from a rail onto the ground or a bump into a wall the
	// scripts wait for their animation to end (WaitAnimWhilstChecking) before they go back to
	// the loop of the ground, which is where the push is played. In Skate a push does not wait
	// for that. So for the skater Flickit drives, while a push of Flickit's wants its animation
	// and he could be pushed, the animation of one of those is over as soon as it is asked
	// about in that wait.
	static bool __fastcall AnimFinishedHook(uint8_t* anim) {
		const bool over = AnimOver(anim);
		if (over || !s_active || s_frame.push <= 0 || !s_push.Animating()) {
			return over;
		}
		uint8_t* skater = LocalSkater();
		if (!skater || (uint8_t*)GetComponent(skater, kAnimation) != anim || !CanPush(skater) || Crouching(skater)) {
			return over;    // a push that would not be played (he crouches) cuts nothing short
		}
		const ScriptAt at = ScriptOf(skater);
		if (at.running != kWaitAnimChecking
			|| (at.caller != kLand2 && at.caller != kSkateInLand && at.caller != kFlailLeft && at.caller != kFlailRight)) {
			return over;
		}
		const uint8_t* channel = *(const uint8_t* const*)(anim + ANIM_FIRST);
		if (!channel || *(const int32_t*)(channel + CHANNEL_SELF) == -1) {
			return over;
		}
		const int cut = PushCutOf(*(const uint32_t*)(channel + CHANNEL_NAME));
		if (cut < 0) {
			return over;
		}
		const float start = *(const float*)(channel + CHANNEL_START);
		const float end = *(const float*)(channel + CHANNEL_END);
		const float time = *(const float*)(channel + CHANNEL_TIME);
		s_cutClip = cut;
		s_cutShare = end > start ? (time - start) / (end - start) : 0.0f;
		s_cutSerial++;
		return true;
	}

	// Stands in the wait of a script for the end of an object's animation (Obj_WaitAnimFinished),
	// where it asks whether the first channel is over. Same convention: the channel in ecx, no
	// arguments, the answer in al. With "auto kick" off the loop of the ground plays the push and
	// waits for it to end, so for the length of the animation nothing else in the loop gets a look
	// in: not the crouch for an ollie, not a brake. In Skate the wind-up breaks into a push once
	// the push is past its start, and Down here has already stopped the push (BrakeTestHook). So
	// for the skater Flickit drives, a push animation is over when it gives way to one of those
	// (Flickit::PushGivesWay): the scripts go on, and show the crouch or the brake.
	static bool __fastcall AnimWaitHook(uint8_t* channel) {
		const bool over = ChannelOver(channel);
		if (over || !s_active || s_frame.push <= 0) {
			return over;
		}
		uint8_t* skater = LocalSkater();
		const uint8_t* anim = skater ? (const uint8_t*)GetComponent(skater, kAnimation) : 0;
		if (!anim || *(uint8_t* const*)(anim + ANIM_FIRST) != channel || !CanPush(skater)) {
			return over;
		}
		const uint32_t name = *(const uint32_t*)(channel + CHANNEL_NAME);
		const int clip = PushClipOf(name);
		if (clip < 0 && name != kMongoStep) {
			return over;
		}
		const uint8_t* core = (const uint8_t*)GetComponent(skater, kSkaterCorePhysics);
		const uint8_t* input = core ? *(const uint8_t* const*)(core + CORE_INPUT) : 0;
		const int why = Flickit::PushGivesWay(s_push.Stroking(), Crouching(skater), input && input[INPUT_DOWN_PRESSED] != 0);
		if (why == Flickit::GIVE_NONE) {
			return over;
		}
		const float start = *(const float*)(channel + CHANNEL_START);
		const float end = *(const float*)(channel + CHANNEL_END);
		const float time = *(const float*)(channel + CHANNEL_TIME);
		s_wayAnim = clip >= 0 ? kPushClips[clip].text : "MongoPush";
		s_wayTo = why;
		s_wayShare = end > start ? (time - start) / (end - start) : 0.0f;
		s_waySerial++;
		return true;
	}

	/******************************************************************/
	/*                                                                */
	/*  The crouch shown as deep as the stick's                       */
	/******************************************************************/

	// Once a frame, from the update of player one's pad: whether the stick crouches, whether the
	// game has the skater crouched, and how deep, to the Keeper (flickit_pose.h), which has his
	// animations blend for the crouch and holds on to the pose he stood in. The stick crouches
	// when Flickit drives him, its crouch is shown (CrouchPose=1), and he is on his board on the
	// ground, not in a manual.
	static void ShowCrouch(uint8_t* skater) {
		uint8_t* anim = (uint8_t*)GetComponent(skater, kAnimation);
		const uint8_t* state = (const uint8_t*)GetComponent(skater, kSkaterState);
		const uint8_t* core = (const uint8_t*)GetComponent(skater, kSkaterCorePhysics);
		const uint8_t* balance = core ? *(const uint8_t* const*)(core + CORE_BALANCE) : 0;
		const bool ground = state && *(const int32_t*)(state + STATE_STATE) == 0 && state[STATE_DRIVING] == 0
			&& *(const int32_t*)(state + STATE_PHYSICS) == 0 && state[STATE_BAILING] == 0;
		const bool stick = s_active && s_frame.crouchPose >= 0.0f && ground
			&& balance && *(const uint32_t*)(balance + BALANCE_TYPE) == 0;
		const bool crouched = state && state[STATE_CROUCHED] != 0;
		s_poseNow = (uint32_t)GameMs();
		if (s_active && s_frame.popStrength >= 0.0f) {
			// A pop out of the stick's crouch on this frame: the jump the game starts takes over at
			// once (its nollie blends for a moment, which the player's skater never does)
			s_keeper.Pop(anim);
		}
		FlickitPose::Report report;
		s_keeper.Frame(anim, ground, stick, crouched, s_frame.crouchPose, s_poseNow, report);
		if (anim && s_keeper.WantsStanding(anim)) {
			// The game plays its crouch and nothing he stood in is left to blend with (a short drop
			// lands in the crouch's bump, and the crouch idle comes after it). Between frames of the
			// loop of the ground no script waits for an animation: there a standing pose is started
			// under the crouch.
			// In the nollie loop he stands in nollie, unless the stick put him there: then the crouch
			// is the stick's up, and he stands in regular before and after it.
			const ScriptAt at = ScriptOf(skater);
			const bool nollie = at.running == kNollieLoop || (at.running == kWaitOneFrame && at.caller == kNollieLoop);
			const bool loop = nollie || at.running == kGroundLoop || (at.running == kWaitOneFrame && at.caller == kGroundLoop);
			if (loop) {
				const uint8_t* head = *(const uint8_t* const*)(anim + ANIM_FIRST);
				const uint32_t crouchAnim = head ? *(const uint32_t*)(head + CHANNEL_NAME) : 0;
				const uint32_t stand = nollie && !s_stance.Ours() ? kStandNollieIdle : kStandIdle;
				uint8_t* standing = StandUnder(anim, stand);
				if (s_keeper.Adopt(anim, standing, s_poseNow)) {
					s_poseUnderCrouch = crouchAnim;
					s_poseUnderStand = stand;
					s_poseUnderSerial++;
				}
			}
		}
		if (report.started) {
			s_poseFrom = report.from;
			s_poseSerial++;
		}
		if (report.lost) {
			s_poseLostSerial++;
		}
		if (report.missed) {
			s_poseMissAnim = report.missAnim;
			s_poseMissChannels = report.missChannels;
			s_poseMissLast = report.missLast;
			s_poseMissBlending = anim ? anim[FlickitPose::ANIM_BLENDING] : 0;
			s_poseMissSerial++;
		}
	}

	// Under a crouch with nothing standing to blend with, the animation he stands in is started with
	// a blend, and the crouch animation again on top of it, where it was: for the scripts the crouch
	// goes on as before. Returns the channel of the standing animation, or 0 if it was not started.
	static uint8_t* StandUnder(uint8_t* anim, uint32_t stand) {
		const uint8_t* head = *(const uint8_t* const*)(anim + ANIM_FIRST);
		if (!head || *(const int32_t*)(head + CHANNEL_SELF) == -1 || anim[FlickitPose::ANIM_BLENDING] == 0
			|| *(const int32_t*)(head + CHANNEL_BACKWARDS) != 0) {
			return 0;
		}
		const uint32_t crouch = *(const uint32_t*)(head + CHANNEL_NAME);
		const float start = *(const float*)(head + CHANNEL_START);
		const float end = *(const float*)(head + CHANNEL_END);
		const float time = *(const float*)(head + CHANNEL_TIME);
		const float speed = *(const float*)(head + CHANNEL_SPEED) * 60.0f;
		const int loop = *(const int32_t*)(head + FlickitPose::CHANNEL_LOOP);
		if (!(end >= start) || !(time >= start && time <= end) || !(speed > 0.0f) || loop < 0 || loop > 3
			|| !AnimExists(anim, stand)) {
			return 0;
		}
		const float length = AnimLength(anim, stand);
		if (!(length > 0.0f)) {
			return 0;
		}
		PlayAnimCore(anim, stand, 0, 0.0f, length, 1, kStandBlend, 1.0f);
		PlayAnimCore(anim, crouch, 0, start, end, loop, kStandBlend, speed);
		uint8_t* again = *(uint8_t* const*)(anim + ANIM_FIRST);
		if (!again || *(const int32_t*)(again + CHANNEL_SELF) == -1 || *(const uint32_t*)(again + CHANNEL_NAME) != crouch) {
			return 0;
		}
		*(float*)(again + CHANNEL_TIME) = time;
		uint8_t* second = FlickitPose::NextOf(again);
		if (!second || *(const int32_t*)(second + CHANNEL_SELF) == -1 || *(const uint32_t*)(second + CHANNEL_NAME) != stand) {
			return 0;
		}
		return second;
	}

	/******************************************************************/
	/*                                                                */
	/*  The stance the stick asks for                                 */
	/******************************************************************/

	// Once a frame, from the update of player one's pad, after the crouch: where the skater is, to
	// the StanceKeeper (flickit.h), and what it answers is done. His script goes to the other loop of
	// the ground, which sets the stance as it starts; this happens between frames of a loop, before
	// his script runs on this frame, so no script is waiting for anything there.
	static void KeepStance(uint8_t* skater) {
		if (!s_active) {
			return;
		}
		uint8_t* panel = (uint8_t*)GetComponent(skater, kStancePanel);
		const uint8_t* state = (const uint8_t*)GetComponent(skater, kSkaterState);
		void* script = *(void**)(skater + OBJECT_SCRIPT);
		if (!panel || !state || !script) {
			return;
		}
		const ScriptAt at = ScriptOf(skater);
		Flickit::StanceLook look;
		look.ground = *(const int32_t*)(state + STATE_STATE) == 0 && state[STATE_DRIVING] == 0
			&& *(const int32_t*)(state + STATE_PHYSICS) == 0 && state[STATE_BAILING] == 0;
		// Between frames the loops wait in WaitOneGameFrame. Every other wait in them (for a push
		// animation, say) is a script of its own as well, and is left to end by itself.
		look.regularLoop = at.running == kWaitOneFrame && at.caller == kGroundLoop;
		look.nollieLoop = at.running == kWaitOneFrame && at.caller == kNollieLoop;
		look.pressure = panel[PANEL_PRESSURE] != 0;
		const Flickit::StanceAnswer answer = s_stance.Look(look);
		if (answer.endPressure) {
			// What PressureOff does, but for the event it sends, which only the debugger hears
			panel[PANEL_PRESSURE] = 0;
		}
		if (answer.move == Flickit::STANCE_TO_NOLLIE) {
			SetScript(script, kNollieLoop, 0, skater);
		}
		else if (answer.move == Flickit::STANCE_TO_REGULAR) {
			SetScript(script, kGroundLoop, 0, skater);
		}
		if (answer.move != Flickit::STANCE_STAY) {
			s_stanceTo = answer.move == Flickit::STANCE_TO_NOLLIE ? Flickit::STANCE_NOLLIE : Flickit::STANCE_REGULAR;
			s_stancePressure = answer.endPressure;
			s_stanceLate = answer.late;
			s_stanceSerial++;
		}
	}

	// Stands in PlayAnim's start of an animation, where the blend starts. Same convention: this
	// (the animation component) in ecx, the blend's time on the stack, popped by the callee. For
	// the skater's, the pose kept for a crouch stays in the list, and the channel that begins to
	// blend out weighs what it would without it.
	static void __fastcall BlendStartHook(uint8_t* anim, void* /*edx*/, float seconds) {
		s_keeper.BeforeBlend(anim);
		BlendStart(anim, seconds);
		s_keeper.AfterBlend(anim);
	}

	// Stands in the update of an animation component, where it makes the object's pose. Same
	// convention: this in ecx, no arguments. For the skater's, the pose kept for a crouch has the
	// share the stick asks for.
	static void __fastcall PoseHook(uint8_t* anim) {
		s_keeper.BeforePose(anim, s_poseNow);
		PoseBlend(anim);
		s_keeper.AfterPose(anim);
	}

	/******************************************************************/
	/*                                                                */
	/*  Rolling: the board loses speed the way Skate's does           */
	/******************************************************************/

	// Stands where the physics of the ground apply the friction. Same convention: this in ecx, no
	// arguments, nothing answered. The game's own: a drag that grows with the square of the speed
	// and next to nothing for the wheels; with "auto kick" off, nothing at all. A bail, and a
	// script that has raised what the wheels lose (that is how the game brakes), stay the game's.
	static void __fastcall FrictionHook(uint8_t* core) {
		if (!Drives(core) || s_frame.coast <= 0) {
			GroundFriction(core);
			return;
		}
		uint8_t* skater = *(uint8_t**)(core + COMPONENT_OBJECT);
		const uint8_t* state = *(const uint8_t* const*)(core + CORE_STATE);
		if (!skater || !state || state[STATE_BAILING] != 0
			|| *(const float*)(core + CORE_ROLLING_FRICTION) != GetPhysicsFloat(kRollingFriction, 0)) {
			GroundFriction(core);
			return;
		}
		float* velocity = (float*)(skater + OBJECT_VELOCITY);
		const float speed = Speed(skater);
		if (!(speed > 0.0f)) {
			return;
		}
		const float scale = Flickit::Coast(speed, *(const float*)(core + CORE_FRAME_LENGTH), s_frame.coast) / speed;
		velocity[0] *= scale;
		velocity[1] *= scale;
		velocity[2] *= scale;
	}

	/******************************************************************/
	/*                                                                */
	/*  Rolling: the board turns the way Skate's does                 */
	/******************************************************************/

	// Stands where the physics of the ground turn the skater. Same convention: this in ecx, no
	// arguments, nothing answered. The game's own: 1.8 radians a second for Left or Right, at any
	// speed, and twice that with Down as well.
	//
	// Down changes nothing here, as in Skate, where only how far the stick is pushed to the side
	// counts. The game reads Left, Right and Down off the stick one axis at a time, once it is
	// more than 50 of its 128 steps out of the middle on that axis (0x004213A0, 0x004214D0): a
	// stick pushed all the way to one side and a quarter of the way round toward Down is Down
	// as well. Handing such a frame to the game swapped the trucks for its sharp turn, twice its
	// own, and back to straight trucks again as the thumb went up and down in a carve.
	static void __fastcall TurnHook(uint8_t* core) {
		GiveBackTurn(core);     // where the brake test did not come by (something is balanced)
		if (!Drives(core) || s_frame.steering <= 0) {
			GroundTurn(core);
			return;
		}
		uint8_t* skater = *(uint8_t**)(core + COMPONENT_OBJECT);
		const uint8_t* input = *(const uint8_t* const*)(core + CORE_INPUT);
		// Where a script has taken the controls away (the stick does not come through the game's
		// pad either), the turn is the game's
		if (!skater || !input || input[INPUT_DISABLED] != 0 || input[INPUT_NET_DISABLED] != 0) {
			s_steer.Reset();
			GroundTurn(core);
			return;
		}
		const float dt = *(const float*)(core + CORE_FRAME_LENGTH);
		const float rate = s_steer.Step(dt, s_frame.steer, Speed(skater), s_push.Phase() == Flickit::PUSH_CONTACT, s_frame.steering);
		// The game turns to the left for a positive angle
		const float angle = -rate * dt;
		if (angle == 0.0f || !(angle == angle)) {
			return;
		}
		// What the game's own turning does with its angle, in its order
		core[CORE_TURNING_LEFT] = angle > 0.0f ? 1 : 0;
		if (core[CORE_VELOCITY_LOCKED] == 0) {
			VectorTurn(skater + OBJECT_VELOCITY, angle);
		}
		MatrixTurn(skater + OBJECT_MATRIX, angle);
		MatrixTurn(core + CORE_DISPLAY_MATRIX, angle);
	}

	/******************************************************************/
	/*                                                                */
	/*  Spinning: the skater turns in the air the way Skate's does    */
	/******************************************************************/

	// Slower than this (rad/s) is no turning at all
	static const float kSpinNone = 0.001f;

	// Stands where the game's turning in the air asks how fast the skater turns, once for Left
	// and once for Right. Same convention: this (the skater) in ecx, one argument, which the
	// callee takes off the stack, the answer on the floating point stack. While Flickit's
	// turning runs (AirTurnHook, below) the answer is Flickit's.
	static float __fastcall SpinRateHook(void* skater, void* /*edx*/, uint32_t name) {
		if (s_spinning) {
			return s_spinRate;
		}
		return ScriptedStat(skater, name);
	}

	// Stands where the physics of the air turn the skater. Same convention: this in ecx, no
	// arguments, nothing answered.
	//
	// The game's own: Left or Right held turns him at 6.85 to 7.75 radians a second, once the
	// button has been down for a tenth of a second. The turning itself, the count of it for the
	// name of the trick, the turn toward the way down on a ramp and the two spin buttons all
	// stay the game's. Flickit only says how fast and which way: for this one call the game
	// sees Left or Right down, for a long time already, by the side Flickit's turning goes to,
	// and is told Flickit's rate when it asks for the skater's.
	static void __fastcall AirTurnHook(uint8_t* core) {
		if (!Drives(core) || s_frame.spin <= 0) {
			AirTurn(core);
			return;
		}
		uint8_t* input = *(uint8_t**)(core + CORE_INPUT);
		// Where a script has taken the controls away the stick does not turn him either
		if (!input || input[INPUT_DISABLED] != 0 || input[INPUT_NET_DISABLED] != 0) {
			AirTurn(core);
			return;
		}
		s_airSeen = true;
		const float rate = s_spin.Step(*(const float*)(core + CORE_FRAME_LENGTH), s_frame.steer, s_frame.spin);
		uint8_t* const left = input + INPUT_LEFT_PRESSED;
		uint8_t* const right = input + INPUT_RIGHT_PRESSED;
		const uint8_t leftWas = left[0];
		const uint8_t rightWas = right[0];
		const uint32_t leftAt = *(const uint32_t*)(left + BUTTON_PRESSED_AT);
		const uint32_t rightAt = *(const uint32_t*)(right + BUTTON_PRESSED_AT);
		left[0] = rate < -kSpinNone ? 1 : 0;
		right[0] = rate > kSpinNone ? 1 : 0;
		*(uint32_t*)(left + BUTTON_PRESSED_AT) = 0;
		*(uint32_t*)(right + BUTTON_PRESSED_AT) = 0;
		s_spinRate = rate < 0.0f ? -rate : rate;
		s_spinning = true;
		AirTurn(core);
		s_spinning = false;
		left[0] = leftWas;
		right[0] = rightWas;
		*(uint32_t*)(left + BUTTON_PRESSED_AT) = leftAt;
		*(uint32_t*)(right + BUTTON_PRESSED_AT) = rightAt;
	}

	/******************************************************************/
	/*                                                                */
	/*  Putting it in                                                 */
	/******************************************************************/

	bool Install() {
		if (s_installed) {
			return true;
		}
		if (!Available()) {
			return false;
		}

		static BYTE testAl[2] = { 0x84, 0xC0 };   // test al, al

		patchCall(kCatchTest1, (void*)&RailCatchHook);
		patchBytesM(kCatchTest1 + 5, testAl, 2);
		patchCall(kCatchTest2, (void*)&RailCatchHook);
		patchBytesM(kCatchTest2 + 5, testAl, 2);
		patchCall(kPickCall, (void*)&GrindPickHook);
		patchCall(kBalanceCall, (void*)&RailBalanceHook);
		patchCall(kMeterCall, (void*)&MeterHook);
		patchCall(kBoostCall, (void*)&RailBoostHook);
		s_reach.nearest.miss = Flickit::kNoRail;
		patchCall(kSearchCall1, (void*)&RailSearchHook);
		patchCall(kSearchCall2, (void*)&RailSearchHook);
		patchCall(kSearchMovingCall1, (void*)&RailSearchHook);
		patchCall(kSearchMovingCall2, (void*)&RailSearchHook);
		patchCall(kGravityCall1, (void*)&AirGravityHook);
		patchCall(kGravityCall2, (void*)&AirGravityHook);
		patchCall(kGravityCall3, (void*)&AirGravityHook);
		patchCall(kGravityCall4, (void*)&AirGravityHook);
		patchCall(kJumpCall, (void*)&JumpHook);
		patchCall(kJumpSpeedCall, (void*)&JumpSpeedHook);
		patchCall(kBrakeTestCall, (void*)&BrakeTestHook);
		patchCall(kKickTestCall, (void*)&KickTestHook);
		patchCall(kCanKickCall, (void*)&CanKickHook);
		patchCall(kHeldCall, (void*)&HeldHook);
		patchCall(kFrictionCall, (void*)&FrictionHook);
		patchCall(kTurnCall, (void*)&TurnHook);
		patchCall(kPadUpdateCall, (void*)&PadUpdateHook);
		patchCall(kAnimFinishedCall, (void*)&AnimFinishedHook);
		patchCall(kAnimWaitCall, (void*)&AnimWaitHook);
		patchCall(kAirTurnCall, (void*)&AirTurnHook);
		patchCall(kSpinLeftCall, (void*)&SpinRateHook);
		patchCall(kSpinRightCall, (void*)&SpinRateHook);
		patchCall(kBlendStartCall, (void*)&BlendStartHook);
		patchCall(kPoseCall, (void*)&PoseHook);

		s_installed = true;
		Log::TypedLog(CHN_SDL, "Flickit: grind hooks are in (rail catch, rail search, grind pick, rail balance, balance meter, rail boost)\n");
		Log::TypedLog(CHN_SDL, "Flickit: jump hooks are in (gravity in the air, jump, jump speed)\n");
		Log::TypedLog(CHN_SDL, "Flickit: rolling hooks are in (brake test, kick test, friction, turning, CanKick, Held, pad update, AnimFinished, animation wait)\n");
		Log::TypedLog(CHN_SDL, "Flickit: spin hooks are in (turning in the air, spin rate)\n");
		Log::TypedLog(CHN_SDL, "Flickit: crouch hooks are in (blend start, pose)\n");
		return true;
	}
}
