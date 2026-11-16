#include "flickit_game.h"

#include <windows.h>
#include <string.h>

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

	enum {
		// any component
		COMPONENT_OBJECT = 0x0C,        // the object it belongs to

		// the skater object
		OBJECT_SCRIPT = 0x10,           // its script

		// Script::CScript
		SCRIPT_PARAMS = 0x14,           // the parameters of the script that is running

		// CSkaterCorePhysicsComponent
		CORE_GRIND_TWEAK = 0x368,
		CORE_INPUT = 0x4B4,             // CInputComponent
		CORE_STATE = 0x4D0,             // CSkaterStateComponent

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
		// the four places that get changed, with the instruction before each
		{ 0x0056CB5A, 15, { 0x8B, 0x86, 0xB4, 0x04, 0x00, 0x00, 0x80, 0xB8, 0xA0, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x84 }, "rail catch test" },
		{ 0x0056FA30, 15, { 0x8B, 0x96, 0xB4, 0x04, 0x00, 0x00, 0x80, 0xBA, 0xA0, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x84 }, "rail catch test in the update" },
		{ 0x0056AFDA, 7, { 0x8B, 0xCE, 0xE8, 0xDF, 0x58, 0xFF, 0xFF }, "grind pick" },
		{ 0x0056D062, 8, { 0x8D, 0x48, 0x7C, 0xE8, 0x56, 0xE9, 0xF9, 0xFF }, "rail balance" },
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
		if (Flickit::NameChecksum("SkaterState") != kSkaterState || Flickit::NameChecksum("SkaterFlipAndRotate") != kSkaterFlipAndRotate) {
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

	void BeginFrame() {
		s_active = false;
	}

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
		s_lastSkater = skater;
		s_lastState = out.state;

		out.grind = s_grind;
		out.grindReady = s_grindReady;
		out.grindSerial = s_grindSerial;
		return true;
	}

	/******************************************************************/
	/*                                                                */
	/*  Rails catch without the Grind button                          */
	/******************************************************************/

	// The game asks "is the Grind button down?" before it looks for a rail under a skater in the air.
	// Both places that ask call this instead. core: the skater's CSkaterCorePhysicsComponent.
	static bool __cdecl RailCatchAllowed(const uint8_t* core) {
		const uint8_t* input = *(const uint8_t* const*)(core + CORE_INPUT);
		if (input && input[INPUT_GRIND_PRESSED] != 0) {
			return true;    // the button, as it always was
		}
		if (!s_active || !s_frame.catchRails || !IsLocal(core)) {
			return false;
		}
		// Out of a ramp the coping is left alone: lip tricks and grinding the coping stay on the button
		const uint8_t* state = *(const uint8_t* const*)(core + CORE_STATE);
		if (!state || state[STATE_VERT_AIR] != 0) {
			return false;
		}
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
		if (!s_active || !s_frame.pickGrind || s_frame.classicGrind || !IsLocal(core)) {
			// Not Flickit's skater, or the player holds the Grind button: the direction buttons pick
			DoGrindTrick(core, direction, railOnLeft, parallel, backwards, flipped);
			return;
		}

		const bool left = (railOnLeft & 0xFF) != 0;
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
				return;
			}
			grind = plan.fallback;
			Flickit::PlanGrind(grind, left, plan);
		}
		s_pick.started = grind;
		DoGrindTrick(core, (uint32_t)plan.row, railOnLeft, plan.parallel < 0 ? parallel : (uint32_t)plan.parallel, backwards, flipped);
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
		if (s_active && s_frame.noBalance && IsLocal(record - BALANCE_RECORD)) {
			*(float*)(record + BALANCE_LEAN) = 0.0f;
			*(float*)(record + BALANCE_LEAN_SPEED) = 0.0f;
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

		s_installed = true;
		Log::TypedLog(CHN_SDL, "Flickit: grind hooks are in (rail catch, grind pick, rail balance)\n");
		return true;
	}
}
