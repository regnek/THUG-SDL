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

	enum {
		// any component
		COMPONENT_OBJECT = 0x0C,        // the object it belongs to

		// the skater object
		OBJECT_SCRIPT = 0x10,           // its script
		OBJECT_LEFT = 0x60,             // the way its left side points (the first row of its matrix; "Move x = 1" in a script goes there)
		OBJECT_VELOCITY = 0xA0,

		// Script::CScript
		SCRIPT_PARAMS = 0x14,           // the parameters of the script that is running

		// CSkaterCorePhysicsComponent
		CORE_FRAME_LENGTH = 0x8C,       // seconds, the frame its physics are working on
		CORE_RAIL_NODE = 0x22C,         // the rail it is on: the node the segment starts at
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
		INPUT_GRIND_PRESSED = 0x1A0,    // the Grind button (Triangle) is down

		// CSkaterStateComponent
		STATE_STATE = 0x18,             // 0 ground, 1 air, 2 wall, 3 lip, 4 rail, 5 wallplant
		STATE_PHYSICS = 0x1C,           // 0 skating, 1 walking
		STATE_VERT_AIR = 0x30,
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
			|| Flickit::NameChecksum("SkaterCorePhysics") != kSkaterCorePhysics || Flickit::NameChecksum("slide") != kSlideBalance) {
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

	void SetFrame(const Frame& frame) {
		s_frame = frame;
		s_active = true;
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

	void BeginFrame() {
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

		s_installed = true;
		Log::TypedLog(CHN_SDL, "Flickit: grind hooks are in (rail catch, rail search, grind pick, rail balance, rail boost)\n");
		return true;
	}
}
