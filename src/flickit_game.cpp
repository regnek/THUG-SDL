#include "flickit_game.h"

#include <windows.h>
#include <string.h>
#include <cmath>

#include "patch.h"
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

	enum {
		// any component
		COMPONENT_OBJECT = 0x0C,        // the object it belongs to

		// the skater object
		OBJECT_SCRIPT = 0x10,           // its script
		OBJECT_LEFT = 0x60,             // the way its left side points (the first row of its matrix; "Move x = 1" in a script goes there)
		OBJECT_MATRIX = 0x60,           // that matrix
		OBJECT_FORWARD = 0x80,          // the way it faces (the third row)
		OBJECT_VELOCITY = 0xA0,
		OBJECT_PLAYER = 0x2BC,          // which player's skater it is

		// Script::CScript
		SCRIPT_PARAMS = 0x14,           // the parameters of the script that is running
		SCRIPT_NAME = 0xBC,             // and the checksum of its name

		// CSkaterCorePhysicsComponent
		CORE_DISPLAY_MATRIX = 0x18,     // the matrix the skater is drawn with
		CORE_FRAME_LENGTH = 0x8C,       // seconds, the frame its physics are working on
		CORE_RAIL_NODE = 0x22C,         // the rail it is on: the node the segment starts at
		CORE_AUTO_KICK = 0x2D0,         // one byte: he kicks without being asked (the game's option)
		CORE_NO_KICK = 0x2D8,           // one byte: a script has forbidden kicking (CanKickOff, in a revert)
		CORE_ROLLING_FRICTION = 0x2DC,  // what the wheels lose; scripts raise it to brake
		CORE_VELOCITY_LOCKED = 0x34C,   // one byte: turning does not turn the direction of travel
		CORE_TURNING_LEFT = 0x365,      // one byte: the last turn on the ground went to the left
		CORE_GRIND_TWEAK = 0x368,
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
		INPUT_GRIND_PRESSED = 0x1A0,    // the Grind button (Triangle) is down
		INPUT_DISABLED = 0x498,         // one byte: a script has taken the controls away (DisablePlayerInput)
		INPUT_NET_DISABLED = 0x499,     // one byte: the same for a network game

		// Skate::Instance()
		SKATE_AUTO_KICK = 0x3B1,        // one byte for each player, eight bytes apart: the option "auto kick"
		SKATE_PLAYER_SIZE = 8,

		// CSkaterStateComponent
		STATE_STATE = 0x18,             // 0 ground, 1 air, 2 wall, 3 lip, 4 rail, 5 wallplant
		STATE_PHYSICS = 0x1C,           // 0 skating, 1 walking
		STATE_VERT_AIR = 0x30,
		STATE_BAILING = 0x70,           // one byte: in a bail
		STATE_DOING_TRICK = 0xD8,
		STATE_DRIVING = 0x114,

		// the balance record of a rail (CSkaterBalanceTrickComponent + 0x7C)
		BALANCE_RECORD = 0x7C,
		BALANCE_LEAN = 0x30,
		BALANCE_LEAN_SPEED = 0x34,
	};

	// Name checksums
	static const uint32_t kSkaterState = 0x43686585;          // SkaterState
	static const uint32_t kSkaterCorePhysics = 0x5BD63B29;    // SkaterCorePhysics
	static const uint32_t kSlideBalance = 0x8D10119D;         // slide: the balance of a slide. A grind's is "grind"
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
	static const uint32_t kAirRotation = 0x4957DB43;          // Physics_Air_Rotation_stat

	// Where the game's code is changed
	static uint8_t* const kCatchTest1 = (uint8_t*)0x0056CB60;   // cmp byte ptr [eax+1A0h], 0   in maybe_stick_to_rail
	static uint8_t* const kCatchTest2 = (uint8_t*)0x0056FA36;   // cmp byte ptr [edx+1A0h], 0   the same test in the skater's update
	static uint8_t* const kPickCall = (uint8_t*)0x0056AFDC;     // call do_grind_trick          at the end of stick_to_rail
	static uint8_t* const kBalanceCall = (uint8_t*)0x0056D065;  // call the balance update      in the rail physics
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
	static uint8_t* const kSpinLeftCall = (uint8_t*)0x0055F2FD;
	static uint8_t* const kSpinRightCall = (uint8_t*)0x0055F361;

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
		{ 0x0055F2F5, 13, { 0x8B, 0x4E, 0x0C, 0x68, 0x43, 0xDB, 0x57, 0x49, 0xE8, 0xCE, 0x77, 0xFB, 0xFF }, "spin rate for Left" },
		{ 0x0055F359, 13, { 0x8B, 0x4E, 0x0C, 0x68, 0x43, 0xDB, 0x57, 0x49, 0xE8, 0x6A, 0x77, 0xFB, 0xFF }, "spin rate for Right" },
		// the functions that get called
		{ 0x004F94F0, 12, { 0x53, 0x56, 0x57, 0x8B, 0xF9, 0x33, 0xF6, 0xE8, 0x84, 0xC0, 0xF3, 0xFF }, "GetLocalSkater" },
		{ 0x0041BDC0, 12, { 0x8B, 0x81, 0xF0, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x12, 0x8B, 0x4C }, "GetComponent" },
		{ 0x005608C0, 12, { 0x6A, 0xFF, 0x68, 0x0B, 0xB4, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "do_grind_trick" },
		{ 0x0050B9C0, 12, { 0x6A, 0xFF, 0x68, 0x5B, 0x8D, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "balance update" },
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
		// the game's own "is he skating" test: the two offsets ReadSkater uses for it
		{ 0x0044F8A0, 18, { 0x8B, 0x41, 0x2C, 0x80, 0xB8, 0x14, 0x01, 0x00, 0x00, 0x00, 0x75, 0x0C, 0x83, 0x78, 0x1C, 0x00, 0x75, 0x06 }, "skating test" },
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
			|| Flickit::NameChecksum("OnGroundNollieAI") != kNollieLoop || Flickit::NameChecksum("Physics_Air_Rotation_stat") != kAirRotation) {
			s_available = false;
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

	static bool s_hasPick = false;
	static Pick s_pick;

	static void HoldAutoKick(bool off);
	static void WatchStick(const Frame& frame);

	void SetFrame(const Frame& frame) {
		s_frame = frame;
		s_active = true;
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
			s_steer.Reset();
			s_spin.Reset();
			s_airSeen = false;
			s_airIdle = 0;
		}
		GiveBackTurnNow();
		s_active = false;
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
		const uint8_t* core = (const uint8_t*)GetComponent(skater, kSkaterCorePhysics);
		const uint8_t* balance = core ? *(const uint8_t* const*)(core + CORE_BALANCE) : 0;
		const uint8_t* input = core ? *(const uint8_t* const*)(core + CORE_INPUT) : 0;
		out.rolling = out.skating && out.state == Flickit::STATE_GROUND && state[STATE_BAILING] == 0
			&& balance != 0 && *(const uint32_t*)(balance + BALANCE_TYPE) == 0 && core[CORE_NO_KICK] == 0
			&& input != 0 && input[INPUT_DISABLED] == 0 && input[INPUT_NET_DISABLED] == 0
			&& s_groundIdle <= kPushIdleMost;
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
	static void __fastcall JumpHook(uint8_t* core, void* /*edx*/, void* params) {
		s_jumping = (s_active && s_frame.pop > 0 && IsLocal(core)) ? core : 0;
		DoJump(core, params);
		s_jumping = 0;
	}

	// Stands where do_jump asks the script for a speed of its own. By then the speed from the
	// stats is in "speed" already, and what is there after this call is what the jump gets. Same
	// convention as CStruct::GetFloat: this in ecx, three arguments, the callee takes them off.
	static bool __fastcall JumpSpeedHook(void* params, void* /*edx*/, uint32_t name, float* speed, int assert) {
		const bool given = StructGetFloat(params, name, speed, assert);
		if (s_jumping && speed) {
			*speed *= Flickit::JumpPopFactor(s_frame.pop);
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

	// Starts a grind script by name, the way do_grind_trick starts the one it found in its table
	static bool StartGrindScript(uint8_t* core, const char* prefix, bool railOnLeft, bool backwards, bool flipped) {
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
		SetScript(*(void**)(skater + OBJECT_SCRIPT), checksum, 0, skater);
		ScriptUpdate(*(void**)(skater + OBJECT_SCRIPT));
		state[STATE_DOING_TRICK] = 1;
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

		int grind = Flickit::GrindFromTilt(s_frame.tilt, left != s_frame.mirror);

		s_pick.grind = grind;
		s_pick.railOnLeft = left;
		s_pick.byName = false;
		s_pick.tilt = s_frame.tilt;
		s_hasPick = true;

		Flickit::GrindPlan plan;
		Flickit::PlanGrind(grind, left, plan);
		if (plan.row < 0) {
			if (StartGrindScript(core, plan.prefix, left, (backwards & 0xFF) != 0, (flipped & 0xFF) != 0)) {
				s_pick.started = grind;
				s_pick.byName = true;
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

	// Stands in for the balance update of a grind or slide. With the game's own "perfect rail" cheat
	// the lean and its speed are set to zero right after this call; the same is done here, without
	// the cheat being on. record: the balance component + 0x7C.
	static void __fastcall RailBalanceHook(uint8_t* record, void* /*edx*/) {
		BalanceUpdate(record);
		if (!s_active || (!s_frame.noBalance && s_frame.drag <= 0 && !s_frame.stepOff)) {
			return;
		}
		uint8_t* balance = record - BALANCE_RECORD;
		if (!IsLocal(balance)) {
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
	// like anywhere else. So for the frame in which a push of Flickit's asks for its animation,
	// the scripts see neither Left nor Right. They run before the physics do: the physics get
	// both back (GiveBackTurn) and turn the board as if nothing had happened.
	static void __fastcall PadUpdateHook(uint8_t* pad, void* /*edx*/, void* data) {
		PadUpdate(pad, data);
		if (s_turnKept && s_turnKept + INPUT_PAD == pad) {
			s_turnKept = 0;     // the game has just written both anew
		}
		if (!s_active || s_frame.push <= 0 || !s_frame.pushOverTurn || s_push.Phase() != Flickit::PUSH_ASKED) {
			return;
		}
		uint8_t* skater = LocalSkater();
		const uint8_t* core = skater ? (const uint8_t*)GetComponent(skater, kSkaterCorePhysics) : 0;
		uint8_t* input = core ? *(uint8_t* const*)(core + CORE_INPUT) : 0;
		if (!input || input + INPUT_PAD != pad || (input[INPUT_LEFT_PRESSED] == 0 && input[INPUT_RIGHT_PRESSED] == 0)) {
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
				s_push.Reset();     // Down brakes, or turns sharply: either way the foot comes back up
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

	// Stands where the physics of the ground ask "may he kick?", which they do on every frame the
	// skater is not braking and balances nothing. Same convention: this in ecx, no arguments, the
	// answer in al. With Flickit's push the answer is always no, and the push is given here: the
	// game's own kick, which would follow a yes, pushes for as long as it is allowed to.
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
		float* velocity = (float*)(skater + OBJECT_VELOCITY);
		const float speed = Speed(skater);
		const float add = s_push.Step(*(const float*)(core + CORE_FRAME_LENGTH), speed, s_frame.pushHeld, s_frame.push, s_frame.pushTiming);
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
	static bool __fastcall CanKickHook(uint8_t* core) {
		const bool can = ShouldKick(core);
		if (!Drives(core) || s_frame.push <= 0 || !s_push.Animating()) {
			return can;
		}
		const uint8_t* skater = *(const uint8_t* const*)(core + COMPONENT_OBJECT);
		const uint8_t* script = skater ? *(const uint8_t* const*)(skater + OBJECT_SCRIPT) : 0;
		if (!script || *(const uint32_t*)(script + SCRIPT_NAME) != kNollieLoop) {
			return can;
		}
		// The script that asked is about to play the push: its time starts now
		s_push.Taken();
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
		// The script that asked is about to play the push (or it is one that only wanted to
		// know; then the one that plays it asks within the next frames): its time starts now
		s_push.Taken();
		return true;
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
	// speed, and twice that with Down. With Down the turn stays the game's.
	static void __fastcall TurnHook(uint8_t* core) {
		GiveBackTurn(core);     // where the brake test did not come by (something is balanced)
		if (!Drives(core) || s_frame.steering <= 0) {
			GroundTurn(core);
			return;
		}
		uint8_t* skater = *(uint8_t**)(core + COMPONENT_OBJECT);
		const uint8_t* input = *(const uint8_t* const*)(core + CORE_INPUT);
		// With Down, and where a script has taken the controls away (the stick does not come
		// through the game's pad either), the turn is the game's
		if (!skater || !input || input[INPUT_DOWN_PRESSED] != 0 || input[INPUT_DISABLED] != 0 || input[INPUT_NET_DISABLED] != 0) {
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
		patchCall(kAirTurnCall, (void*)&AirTurnHook);
		patchCall(kSpinLeftCall, (void*)&SpinRateHook);
		patchCall(kSpinRightCall, (void*)&SpinRateHook);

		s_installed = true;
		Log::TypedLog(CHN_SDL, "Flickit: grind hooks are in (rail catch, rail search, grind pick, rail balance, rail boost)\n");
		Log::TypedLog(CHN_SDL, "Flickit: jump hooks are in (gravity in the air, jump, jump speed)\n");
		Log::TypedLog(CHN_SDL, "Flickit: rolling hooks are in (brake test, kick test, friction, turning, CanKick, Held, pad update)\n");
		Log::TypedLog(CHN_SDL, "Flickit: spin hooks are in (turning in the air, spin rate)\n");
		return true;
	}
}
