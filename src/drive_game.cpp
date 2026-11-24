#include <windows.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crash.h"
#include "drive.h"
#include "drive_game.h"
#include "peds.h"
#include "flickit_cars_qb.h"
#include "bigworld.h"
#include "config.h"
#include "flickit.h"
#include "flickit_game.h"
#include "patch.h"
#include "QB/CFuncs.h"
#include "QB/LazyArray.h"
#include "QB/LazyStruct.h"
#include "Logger/Logger.h"

#define FLICKIT_SECTION "Flickit"

namespace DriveGame {
	/******************************************************************/
	/*                                                                */
	/*  The game's own functions                                      */
	/*                                                                */
	/******************************************************************/

	// Obj::ResolveToObject: an object by its name (a car made from a node has the node's), or 0
	typedef void* (__cdecl* ResolveToObject_t)(uint32_t id);
	static const ResolveToObject_t ResolveToObject = (ResolveToObject_t)0x0041E750;
	// A component of an object by the checksum of its type, or 0
	typedef void* (__thiscall* GetComponent_t)(void* object, uint32_t type);
	static const GetComponent_t GetComponent = (GetComponent_t)0x0041BDC0;
	// A global array by its name (the level's NodeArray), or 0; a structure in an array
	typedef void* (__cdecl* GetGlobalArray_t)(uint32_t name, int assertIfMissing);
	static const GetGlobalArray_t GetGlobalArray = (GetGlobalArray_t)0x00413650;
	typedef void* (__thiscall* ArrayStructure_t)(void* array, uint32_t index);
	static const ArrayStructure_t ArrayStructure = (ArrayStructure_t)0x00408350;
	// Script::CStruct: a checksum, a text, a vector (four floats, the last 1), an array, by name. These
	// look into the structures a node names (its ncomp_ ones) as well.
	typedef bool(__thiscall* StructChecksum_t)(void* s, uint32_t name, uint32_t* out, int assertIfMissing);
	static const StructChecksum_t StructChecksum = (StructChecksum_t)0x004110B0;
	typedef bool(__thiscall* StructText_t)(void* s, uint32_t name, const char** out, int assertIfMissing);
	static const StructText_t StructText = (StructText_t)0x00410CE0;
	typedef bool(__thiscall* StructVector_t)(void* s, uint32_t name, float* out, int assertIfMissing);
	static const StructVector_t StructVector = (StructVector_t)0x004116A0;
	typedef bool(__thiscall* StructArray_t)(void* s, uint32_t name, Script::LazyArray** out, int assertIfMissing);
	static const StructArray_t StructArray = (StructArray_t)0x00410FC0;
	typedef void(__thiscall* StructAddVector_t)(void* s, uint32_t name, float x, float y, float z);
	static const StructAddVector_t StructAddVector = (StructAddVector_t)0x004124D0;
	// CStruct::ContainsFlag: a name in it with no value (CreatedAtStart), in the structures it names too
	typedef bool(__thiscall* StructFlag_t)(void* s, uint32_t flag);
	// CStruct::GetInteger
	typedef bool(__thiscall* StructInteger_t)(void* s, uint32_t name, int* out, int assertIfMissing);
	static const StructInteger_t StructInteger = (StructInteger_t)0x00410D80;
	static const StructFlag_t StructFlag = (StructFlag_t)0x004112A0;
	// A symbol by its name: its type at +1 (7 a script, 10 a structure), its value at +0xC
	typedef uint8_t* (__cdecl* LookUpSymbol_t)(uint32_t name);
	static const LookUpSymbol_t LookUpSymbol = (LookUpSymbol_t)0x004133A0;
	// CVehicleComponent::MoveToNode, what Vehicle_MoveToRestart does with a node: the car on the ground
	// below the node's Pos, turned to its Angles, its wheels settled, still
	typedef void(__thiscall* MoveToNode_t)(void* vehicle, void* node);
	static const MoveToNode_t MoveToNode = (MoveToNode_t)0x0045BEC0;
	// A model's hierarchy (the matrices of its parts, 80 bytes a part, the matrix at +0x10 of each), from the
	// model component's [+0x208], as CVehicleComponent::Finalize gets it
	typedef const uint8_t* (__thiscall* ModelHierarchy_t)(void* modelData);
	static const ModelHierarchy_t ModelHierarchy = (ModelHierarchy_t)0x0047DB60;
	// Script::ParseQB for a file in memory, as the game reads a cutscene's scripts
	typedef void(__cdecl* ParseQBFromMemory_t)(const char* name, uint8_t* data, int flag);
	static const ParseQBFromMemory_t ParseQBFromMemory = (ParseQBFromMemory_t)0x00409F90;
	// Script::RunScript: the name of a script, its parameters, the object it runs on
	typedef bool(__cdecl* RunScript_t)(uint32_t name, void* params, void* object, void* object2);
	static const RunScript_t RunGameScript = (RunScript_t)0x0040FD00;
	// GameNet::Manager::InNetGame
	typedef bool(__thiscall* InNetGame_t)(void* net);
	static const InNetGame_t InNetGame = (InNetGame_t)0x004C6B50;
	static void** const kNetManager = (void**)0x0076914C;
	// CFeeler, the game's line into the level (the skater's physics has one at +0xA8, which its own line test
	// uses): its two ends (four floats each), what it ignores (0x10, 0 as the skater's test has it), and whether
	// the line hits the level, the moving objects too or not (MoveToNode looks for the ground without them).
	// After a hit it has the place it hit and the face's normal there (0x00539921, 0x0053993F).
	typedef void(__thiscall* FeelerLine_t)(void* feeler, const float* start, const float* end);
	static const FeelerLine_t FeelerLine = (FeelerLine_t)0x005396C0;
	typedef void(__thiscall* FeelerIgnore_t)(void* feeler, uint32_t ignore, uint32_t ignore2);
	static const FeelerIgnore_t FeelerIgnore = (FeelerIgnore_t)0x005395F0;
	typedef bool(__thiscall* FeelerHit_t)(void* feeler, int movables, int distant);
	static const FeelerHit_t FeelerHit = (FeelerHit_t)0x005397E0;
	// CVehicleComponent: its speeds from its momentum and its spin's (as Vehicle_Kick does after a kick)
	typedef void(__thiscall* VehicleSpeeds_t)(void* vehicle);
	static const VehicleSpeeds_t VehicleSpeeds = (VehicleSpeeds_t)0x00457AD0;

	// An object: where it is, its matrix (left, up, the way it faces, a fourth row), the matrix it is drawn with
	enum {
		OBJECT_TAGS = 0x20,              // its tags (a structure, as GetTags gives them: 0x0041EFFE)
		OBJECT_POSITION = 0x40,
		OBJECT_MATRIX = 0x60,
		OBJECT_DISPLAY_MATRIX = 0xB0,
	};
	// CVehicleComponent (0x1334 bytes, made at 0x0045B2E0): its input component (its Finalize, 0x0045E474),
	// its wheels (0x1C8 bytes each, the place of each at +0xA0, from the model's bones: 0x0045E4E1)
	enum {
		VEHICLE_INPUT = 0x1314,
		VEHICLE_BODY_PLACE = 0x2BC,  // where the body is drawn: x y z w, from the centre of mass (Finalize)
		MODEL_DATA = 0x208,          // the model component's model, for its hierarchy
		HIERARCHY_BODY_X = 0x90,     // part 1 (the body): its matrix at +0x60, the x of its place at +0x30 of that
		VEHICLE_WHEEL_COUNT = 0x190,
		VEHICLE_WHEELS = 0x2D0,
		WHEEL_SIZE = 0x1C8,
		WHEEL_PLACE = 0xA0,
		INPUT_TRIANGLE = 0x1A0,      // the game's picture of the pad starts at +0x20; Triangle at +0x180 of it
	};
	// CVehicleComponent's body (Vehicle_Kick 0x0045EB78, its speeds 0x00457AD0, its setup 0x0045B4CC): its
	// momentum and its spin's in the world (four floats each), awake and how long it has been still (Vehicle_Wake
	// sets 1 and 0), its speed and spin from them, its mass and moment of inertia as 386.4 over the setup's
	enum {
		VEHICLE_MOMENTUM = 0x28,
		VEHICLE_SPIN_MOMENTUM = 0x48,
		VEHICLE_AWAKE = 0x58,
		VEHICLE_STILL = 0x5C,
		VEHICLE_VELOCITY = 0xA4,
		VEHICLE_SPIN = 0xB4,
		VEHICLE_INV_MASS = 0x158,
		VEHICLE_INV_INERTIA = 0x15C,  // about its left, up, the way it faces
		CORE_FEELER = 0xA8,           // the skater's physics' feeler
		FEELER_POINT = 0x78,
		FEELER_NORMAL = 0x88,
	};
	static const uint32_t kSkaterCorePhysics = 0x5BD63B29;    // SkaterCorePhysics

	enum {
		SYMBOL_SCRIPT = 7,
		SYMBOL_STRUCTURE = 10,
	};

	struct Expect {
		uint32_t address;
		int size;
		uint8_t bytes[16];
		const char* what;
	};
	static const Expect kExpect[] = {
		{ 0x0041E750, 12, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x0D, 0xC0, 0xB6, 0x69, 0x00, 0x50, 0xE8 }, "Obj::ResolveToObject" },
		{ 0x0041BDC0, 12, { 0x8B, 0x81, 0xF0, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x12, 0x8B, 0x4C }, "CCompositeObject::GetComponent" },
		{ 0x00413650, 12, { 0x8B, 0x44, 0x24, 0x04, 0x50, 0xE8, 0x46, 0xFD, 0xFF, 0xFF, 0x83, 0xC4 }, "Script::GetArray" },
		{ 0x00408350, 12, { 0x83, 0x79, 0x08, 0x01, 0x8B, 0x01, 0x74, 0x07, 0x8B, 0x4C, 0x24, 0x04 }, "CArray::GetStructure" },
		{ 0x004110B0, 12, { 0x51, 0x53, 0x56, 0x8B, 0x31, 0x32, 0xDB, 0x85, 0xF6, 0x89, 0x4C, 0x24 }, "CStruct::GetChecksum" },
		{ 0x00410CE0, 12, { 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x8D, 0x44, 0x24, 0x0C, 0x50, 0x6A }, "CStruct::GetText" },
		{ 0x004116A0, 12, { 0x56, 0x8D, 0x44, 0x24, 0x08, 0x50, 0x8B, 0xF1, 0x8B, 0x4C, 0x24, 0x0C }, "CStruct::GetVector" },
		{ 0x00410FC0, 12, { 0x56, 0x8D, 0x44, 0x24, 0x08, 0x50, 0x8B, 0xF1, 0x8B, 0x4C, 0x24, 0x0C }, "CStruct::GetArray" },
		{ 0x004124D0, 12, { 0x6A, 0xFF, 0x68, 0xD6, 0x19, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "CStruct::AddVector" },
		{ 0x004112A0, 12, { 0x56, 0x8B, 0x31, 0x85, 0xF6, 0x57, 0x74, 0x41, 0x8B, 0x7C, 0x24, 0x0C }, "CStruct::ContainsFlag" },
		{ 0x004133A0, 12, { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0xC1, 0x25, 0xFF, 0x0F, 0x00, 0x00, 0x56 }, "Script::LookUpSymbol" },
		{ 0x0045BEC0, 12, { 0x6A, 0xFF, 0x68, 0x5B, 0x35, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "CVehicleComponent::MoveToNode" },
		{ 0x0047DB60, 12, { 0x66, 0x83, 0xB9, 0xAA, 0x01, 0x00, 0x00, 0x01, 0x75, 0x1C, 0x8B, 0x89 }, "CModel: its hierarchy" },
		{ 0x0045E49A, 11, { 0x8B, 0x88, 0x08, 0x02, 0x00, 0x00, 0xE8, 0xBB, 0xF6, 0x01, 0x00 }, "CVehicleComponent::Finalize: the model's hierarchy" },
		{ 0x0045E509, 9, { 0x8D, 0x47, 0x60, 0x50, 0x8D, 0x4C, 0x24, 0x2C, 0xE8 }, "CVehicleComponent::Finalize: the body's part" },
		{ 0x0045E516, 6, { 0xD9, 0x44, 0x24, 0x58, 0xD9, 0xE0 }, "CVehicleComponent::Finalize: the body's x, the other way round" },
		{ 0x0045A6E9, 12, { 0xD9, 0x85, 0xBC, 0x02, 0x00, 0x00, 0xD9, 0x5E, 0x30, 0xD9, 0x85, 0xC0 }, "CVehicleComponent: the body drawn at its place" },
		{ 0x0045E467, 14, { 0x8B, 0x4E, 0x0C, 0x68, 0x28, 0xCD, 0xD7, 0x27, 0xE8, 0x4C, 0xD9, 0xFB, 0xFF, 0x8B }, "CVehicleComponent::Finalize: its input component" },
		{ 0x0045E4B4, 16, { 0xD9, 0x41, 0xA8, 0x8B, 0x86, 0xD0, 0x02, 0x00, 0x00, 0xD9, 0x5C, 0x24, 0x50, 0x83, 0xE9, 0x50 }, "CVehicleComponent::Finalize: its wheels" },
		{ 0x0045A3D0, 16, { 0x51, 0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x8E, 0x14, 0x13, 0x00, 0x00, 0xD9, 0x81, 0x38, 0x02, 0x00 }, "CVehicleComponent: its controls" },
		{ 0x0045EB49, 7, { 0x3D, 0xA6, 0x13, 0xB7, 0x93, 0x74, 0x28 }, "CVehicleComponent: Vehicle_Kick" },
		{ 0x0044A3E0, 14, { 0x8B, 0x41, 0x0C, 0x8D, 0x48, 0x60, 0x51, 0x8D, 0x88, 0xB0, 0x00, 0x00, 0x00, 0xE8 }, "CSetDisplayMatrixComponent::Update" },
		{ 0x00409F90, 13, { 0x8B, 0x44, 0x24, 0x0C, 0x8B, 0x4C, 0x24, 0x08, 0x8B, 0x54, 0x24, 0x04, 0x6A }, "Script::ParseQB, from memory" },
		{ 0x0040FD00, 12, { 0x6A, 0xFF, 0x68, 0x5B, 0x18, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "Script::RunScript" },
		{ 0x004C6B50, 12, { 0x56, 0x8B, 0xF1, 0xF6, 0x86, 0x38, 0x01, 0x00, 0x00, 0x0C, 0x74, 0x24 }, "GameNet::Manager::InNetGame" },
	};
	// What the crashes use besides (crashes off, the cars on, if these are not as expected)
	static const Expect kCrashExpect[] = {
		{ 0x005396C0, 14, { 0x8B, 0x44, 0x24, 0x04, 0xD9, 0x00, 0xD9, 0x19, 0xD9, 0x40, 0x04, 0xD9, 0x59, 0x04 }, "CFeeler: its line" },
		{ 0x005395F0, 14, { 0x66, 0x8B, 0x44, 0x24, 0x04, 0x66, 0x8B, 0x54, 0x24, 0x08, 0x66, 0x89, 0x41, 0x4C }, "CFeeler: what it ignores" },
		{ 0x005397E0, 14, { 0x83, 0xEC, 0x28, 0x53, 0x55, 0x56, 0x8B, 0xF1, 0x57, 0x8D, 0x4C, 0x24, 0x18, 0xE8 }, "CFeeler: does it hit" },
		{ 0x005398AD, 9, { 0x80, 0x7C, 0x24, 0x3C, 0x00, 0x88, 0x44, 0x24, 0x13 }, "CFeeler: the moving objects or not" },
		{ 0x00539921, 6, { 0xD9, 0x46, 0x34, 0xD9, 0x5E, 0x78 }, "CFeeler: the place it hit" },
		{ 0x0053993F, 9, { 0xD9, 0x45, 0x00, 0xD9, 0x9E, 0x88, 0x00, 0x00, 0x00 }, "CFeeler: the face's normal" },
		{ 0x00560302, 10, { 0x02, 0x00, 0x00, 0x8D, 0xBE, 0xA8, 0x00, 0x00, 0x00, 0x51 }, "the skater's line test: its feeler" },
		{ 0x00457AD0, 9, { 0x83, 0xEC, 0x14, 0xD9, 0x81, 0x58, 0x01, 0x00, 0x00 }, "CVehicleComponent: its speeds" },
		{ 0x00457B05, 10, { 0xD9, 0x44, 0x24, 0x04, 0xD9, 0x99, 0xA4, 0x00, 0x00, 0x00 }, "CVehicleComponent: its speed" },
		{ 0x0045EB5C, 14, { 0xC7, 0x46, 0x58, 0x01, 0x00, 0x00, 0x00, 0xC7, 0x46, 0x5C, 0x00, 0x00, 0x00, 0x00 }, "Vehicle_Wake" },
		{ 0x0045EC14, 6, { 0xD9, 0x86, 0x58, 0x01, 0x00, 0x00 }, "Vehicle_Kick: its mass" },
		{ 0x0045EC28, 3, { 0x8D, 0x4E, 0x28 }, "Vehicle_Kick: its momentum" },
		{ 0x0045EC4D, 3, { 0x8D, 0x4E, 0x48 }, "Vehicle_Kick: its spin's" },
		{ 0x0045B4F1, 8, { 0xD9, 0x06, 0xDC, 0x3D, 0xC8, 0x04, 0x63, 0x00 }, "CVehicleComponent: mass as 386.4 over the setup's" },
		{ 0x0045B530, 6, { 0xD9, 0x87, 0x60, 0x01, 0x00, 0x00 }, "CVehicleComponent: its moment of inertia about up" },
	};
	// What running people over uses besides the crashes' (people go through cars, as before, if not as expected)
	static const Expect kPedExpect[] = {
		{ 0x00410D80, 12, { 0x56, 0x8D, 0x44, 0x24, 0x08, 0x50, 0x8B, 0xF1, 0x8B, 0x4C, 0x24, 0x0C }, "CStruct::GetInteger" },
		{ 0x0041EFFE, 5, { 0x8B, 0x76, 0x20, 0x85, 0xF6 }, "CObject: GetTags, its tags" },
		{ 0x0041F101, 5, { 0x3D, 0xC7, 0x37, 0x49, 0x71 }, "CObject: Obj_SwitchScript" },
		// CPedLogicComponent::Update: it moves someone only in its states lip_trick, generic and generic_skater, so
		// in fall, where Flickit_PedDown puts them, they are where Flickit puts them
		{ 0x0044224E, 16, { 0x8B, 0x46, 0x18, 0x3D, 0x7B, 0x16, 0x67, 0x63, 0x74, 0x22, 0x3D, 0x2C, 0x5E, 0xB0, 0x69, 0x74 }, "CPedLogicComponent::Update: its states" },
	};

	/******************************************************************/
	/*                                                                */
	/*  State                                                         */
	/*                                                                */
	/******************************************************************/

	// The checksums of the names used
	struct Names {
		uint32_t nodeArray, className, vehicle, skeletonName, car, model, name, pos, angles;
		uint32_t carPhysics, staticVehicle, vehicleComponent, playerVehicle, colliders, setup, tune, speed, parked;
		uint32_t stealScript, backScript, goesBackScript, defaultTune, promptScript, promptOffScript, createdAtStart;
		uint32_t cameraHeight, cameraDistance;
		uint32_t parkedCar[Drive::Parking::kSlots];
		// the crashes'
		uint32_t knockedScript, fxScript, fxOffScript, boomScript, burntScript, hitSoundScript, onFireScript;
		uint32_t blowOutScript, blastHimScript, fx, parts, offset, vol, pitch;
		uint32_t smokeParts[4], fireParts;
		// people run over
		uint32_t pedestrian, pedLogic, fallDownAnims, layIdleAnims, goalId, isGoalPed, ped;
		uint32_t pedHitScript, pedLandScript, pedGoneScript, pedBackScript;
	};
	static Names s_n;

	static bool s_on = true;                 // [Flickit] Cars
	static float s_reach = 45.0f;            // [Flickit] CarReach
	static int s_ready = 0;                  // 1: the game is as expected, -1: not
	static int s_parsed = 0;                 // 1: the scripts are the game's, -1: they could not be
	static bool s_cfuncs = false;
	// The level the cars were found for: BigWorld's count of level loads (0 while BigNodeNames is off),
	// the NodeArray and its size, and the level's load script (Skate::Instance() + 0x398, as LevelIs reads it)
	struct LevelKey {
		uint32_t loads;
		const void* nodes;
		uint32_t count;
		uint32_t level;
		bool operator!=(const LevelKey& o) const {
			return loads != o.loads || nodes != o.nodes || count != o.count || level != o.level;
		}
	};
	static LevelKey s_level = { 0, 0, 0, 0 };
	static uint8_t** const kSkate = (uint8_t**)0x0076A788;
	static const uint32_t SKATE_LEVEL = 0x398;

	enum { kMaxCars = 256, kModelSize = 128 };
	// A car he got out of stands this long, then goes back: it goes, and the car of the level it was comes
	// back at the start of its way, both only when he is kFar from there ([Flickit] CarParkedTime, seconds;
	// 0: it stays). So the level always has the cars it had: the one he drives or left is one of them.
	static uint32_t s_standMs = 120000;
	static const float kFar = 2500.0f;

	struct Car {
		uint32_t name;
		char model[kModelSize];
		uint32_t tune;           // its Flickit_Car_ structure
		uint32_t setup;          // the setup of the game's drivable car it gets
		Drive::Box box;
		float start[4];          // its node's place, where it comes back
		bool atStart;            // there from the level's start (CreatedAtStart): it comes back; not a goal's car
		Drive::Track track;
		int slot;                // -1 for a car of the level, else its parking slot
	};
	static Car s_cars[kMaxCars + Drive::Parking::kSlots];
	static int s_levelCars = 0;              // the level's; the parked ones follow, one for each slot

	static Drive::Parking s_parking;
	static Drive::Comeback s_back;

	// The car being taken (Flickit_CarTake to Flickit_CarPlace), the car driven, the car being left
	struct Taken {
		bool set;
		uint32_t name;
		uint32_t origin;         // the car of the level it is (itself, or the one a parked car was)
		int slot;
		char model[kModelSize];
		Drive::Box box;
		float pos[4];
		float heading;
		float speed;
	};
	static Taken s_taken;
	struct Driven {
		bool set;
		uint32_t origin;
		char model[kModelSize];
		Drive::Box box;
	};
	static Driven s_driven;
	struct Leaving {
		bool set;
		uint32_t origin;
		int slot;
		char model[kModelSize];
		Drive::Box box;
		float pos[4];
		float matrix[16];
		float door[3];
		float heading;
		float parkPos[4];            // where the car is left: its model on the ground (Drive::GroundUnder)
		float parkMatrix[16];
	};
	static Leaving s_leaving;

	static bool s_prompt = false;            // the game's "Press Circle to drive" box is up
	static uint32_t s_promptSeen = 0;        // the last time a car was in reach
	static const uint32_t kPromptLingerMs = 400;   // it stays this long after (the edge of the reach, an ollie)
	static bool s_down = false;              // Circle on the frame before (his own press)
	static bool s_grabMade = false;          // Flickit's own Circle (a trigger grab) on this frame's pad data
	static bool s_released = true;           // let go since a car was taken
	static LARGE_INTEGER s_lastFrame;        // zero: no frame yet
	static uint32_t s_lastComeBack = 0;
	static uint32_t s_lastGoesBack = 0;
	static float s_carAway = 1.0e9f;         // the nearest car in reach on this frame (CarAway)

	// Crashes ([Flickit] CarCrashes, CarDamage and CarLaunch in percent: how much a hit hurts, how high a hard
	// hit throws). His car is the game's own body, pushed through its momentum; a car of the traffic follows
	// its way, and a hit that would change its speed enough knocks it off it: it becomes a loose car (one of
	// the parking slots, Flickit_ParkedCar...), a box of Flickit's own (crash.h) that tumbles against the level
	// until it rests, and can be taken like a car he left. A car he left is a loose car too.
	static bool s_crashes = true;
	static int s_crashReady = 0;             // 1: the game is as the crashes expect, -1: not
	static float s_damageScale = 1.0f;
	static float s_launchScale = 1.0f;
	struct Loose {
		bool made;                       // its body is from its object
		Crash::Tumbler body;
		Crash::Health health;
		uint32_t groundSeen;             // the last time there was ground under it
	};
	static Loose s_loose[Drive::Parking::kSlots];
	static Crash::Health s_drivenHealth;     // his car's
	static float s_drivenLastVel[3];
	static float s_drivenPushed[3];          // how much Flickit changed its speed since
	static bool s_drivenHaveVel = false;
	static int s_lastParked = -1;            // the slot of the car he got out of last
	static bool s_boomPending = false;       // his car blew up with him in it, and he is on his way out
	static int s_boomSlot = -1;              // the slot of the car he left in the blast
	static uint32_t s_boomSince = 0;
	static float s_boomVel[3];               // how his car was going
	// What each car shows of its damage (slot k, or kFxDriven for his): its smoke (0 none, 1, heavy 2, burnt
	// 3) and whether it burns, on which object
	enum { kFxDriven = Drive::Parking::kSlots, kFxCars = Drive::Parking::kSlots + 1 };
	struct Fx {
		uint32_t car;
		int smoke;
		bool fire;
		uint32_t smokeName, fireName;    // the effects' own names (0: none)
	};
	static Fx s_fx[kFxCars];
	static uint32_t s_lastSound = 0;
	static uint8_t* s_feeler = 0;            // the skater's physics' feeler, on this frame
	static void ResetCrashes();

	// People run over ([Flickit] RunOver, RunOverSpeed): someone in the way of his car, or of a loose car thrown
	// about, is thrown (peds.h), falls the game's own way and stays down (Flickit_PedHit); once they have lain
	// there a while and he is far from them and from the start of their way, they are back on it: gone, and made
	// again from their node (Flickit_PedGone, then Flickit_PedBack a moment later). Not a goal's people, and not
	// those without the game's falling down (no ped logic: Eric in a cutscene; no FallDownAnims: the animals).
	enum { kMaxPeds = 256, kPedsAFrame = 3 };
	enum { kPedThere, kPedFlying, kPedLying, kPedGone };
	struct Ped {
		uint32_t name;
		float start[4];                  // its node's place: where they are made again
		int state;
		Peds::Flight flight;
		uint32_t since;                  // when they went into their state
		bool told;                       // the log said why they cannot be run over
	};
	static Ped s_peds[kMaxPeds];
	static int s_pedCount = 0;
	static bool s_runOver = true;
	static float s_runOverFrom = 60.0f;
	static int s_pedReady = 0;               // 1: the game is as expected, -1: not
	static const uint32_t kLieMs = 10000;    // they lie there this long at the least
	static const float kPedFar = 3000.0f;    // and he is this far from them, and from the start of their way
	static void FindPeds(void* nodes, uint32_t count);

	/******************************************************************/
	/*                                                                */
	/*  Checks                                                        */
	/*                                                                */
	/******************************************************************/

	static void InitNames() {
		s_n.nodeArray = Flickit::NameChecksum("NodeArray");
		s_n.className = Flickit::NameChecksum("Class");
		s_n.vehicle = Flickit::NameChecksum("Vehicle");
		s_n.skeletonName = Flickit::NameChecksum("SkeletonName");
		s_n.car = Flickit::NameChecksum("car");
		s_n.model = Flickit::NameChecksum("model");
		s_n.name = Flickit::NameChecksum("Name");
		s_n.pos = Flickit::NameChecksum("Pos");
		s_n.angles = Flickit::NameChecksum("Angles");
		s_n.carPhysics = Flickit::NameChecksum("CarPhysics");
		s_n.staticVehicle = Flickit::NameChecksum("StaticVehicle");
		s_n.vehicleComponent = s_n.vehicle;
		s_n.playerVehicle = Flickit::NameChecksum("PlayerVehicle");
		s_n.colliders = Flickit::NameChecksum("colliders");
		s_n.setup = Flickit::NameChecksum("setup");
		s_n.tune = Flickit::NameChecksum("tune");
		s_n.speed = Flickit::NameChecksum("speed");
		s_n.parked = Flickit::NameChecksum("parked");
		s_n.stealScript = Flickit::NameChecksum("Flickit_CarSteal");
		s_n.backScript = Flickit::NameChecksum("Flickit_CarBack");
		s_n.goesBackScript = Flickit::NameChecksum("Flickit_CarGoesBack");
		s_n.createdAtStart = Flickit::NameChecksum("CreatedAtStart");
		s_n.cameraHeight = Flickit::NameChecksum("camera_height");
		s_n.cameraDistance = Flickit::NameChecksum("camera_distance");
		s_n.defaultTune = Flickit::NameChecksum("Flickit_Car_Default");
		s_n.promptScript = Flickit::NameChecksum("Flickit_CarPrompt");
		s_n.promptOffScript = Flickit::NameChecksum("Flickit_CarPromptOff");
		for (int i = 0; i < Drive::Parking::kSlots; i++) {
			char name[32];
			sprintf_s(name, sizeof(name), "Flickit_ParkedCar%d", i);
			s_n.parkedCar[i] = Flickit::NameChecksum(name);
		}
		s_n.knockedScript = Flickit::NameChecksum("Flickit_CarKnocked");
		s_n.fxScript = Flickit::NameChecksum("Flickit_CarFx");
		s_n.fxOffScript = Flickit::NameChecksum("Flickit_CarFxOff");
		s_n.boomScript = Flickit::NameChecksum("Flickit_CarBoom");
		s_n.burntScript = Flickit::NameChecksum("Flickit_CarBurnt");
		s_n.hitSoundScript = Flickit::NameChecksum("Flickit_CarHitSound");
		s_n.onFireScript = Flickit::NameChecksum("Flickit_CarOnFire");
		s_n.blowOutScript = Flickit::NameChecksum("Flickit_CarBlowOut");
		s_n.blastHimScript = Flickit::NameChecksum("Flickit_CarBlastHim");
		s_n.fx = Flickit::NameChecksum("fx");
		s_n.parts = Flickit::NameChecksum("parts");
		s_n.offset = Flickit::NameChecksum("offset");
		s_n.vol = Flickit::NameChecksum("vol");
		s_n.pitch = Flickit::NameChecksum("pitch");
		s_n.smokeParts[0] = 0;
		s_n.smokeParts[1] = Flickit::NameChecksum("Flickit_FxSmoke");
		s_n.smokeParts[2] = Flickit::NameChecksum("Flickit_FxSmokeHeavy");
		s_n.smokeParts[3] = Flickit::NameChecksum("Flickit_FxSmokeBurnt");
		s_n.fireParts = Flickit::NameChecksum("Flickit_FxFire");
		s_n.pedestrian = Flickit::NameChecksum("Pedestrian");
		s_n.pedLogic = Flickit::NameChecksum("pedlogic");
		s_n.fallDownAnims = Flickit::NameChecksum("FallDownAnims");
		s_n.layIdleAnims = Flickit::NameChecksum("LayIdleAnims");
		s_n.goalId = Flickit::NameChecksum("goal_id");
		s_n.isGoalPed = Flickit::NameChecksum("is_goal_ped");
		s_n.ped = Flickit::NameChecksum("ped");
		s_n.pedHitScript = Flickit::NameChecksum("Flickit_PedHit");
		s_n.pedLandScript = Flickit::NameChecksum("Flickit_PedLand");
		s_n.pedGoneScript = Flickit::NameChecksum("Flickit_PedGone");
		s_n.pedBackScript = Flickit::NameChecksum("Flickit_PedBack");
	}

	static bool Ready() {
		if (s_ready == 0) {
			s_ready = 1;
			for (size_t i = 0; i < sizeof(kExpect) / sizeof(kExpect[0]); i++) {
				if (memcmp((const void*)kExpect[i].address, kExpect[i].bytes, (size_t)kExpect[i].size) != 0) {
					Log::TypedLog(CHN_SDL, "Cars: the game's code at %08X (%s) is not what was expected; the cars are off\n",
						kExpect[i].address, kExpect[i].what);
					s_ready = -1;
				}
			}
			InitNames();
			// the game's checksums of two names it uses here, to be sure the function is the game's
			if (s_n.nodeArray != 0xC472ECC5 || s_n.playerVehicle != 0x824C6A24) {
				Log::TypedLog(CHN_SDL, "Cars: the checksums of names are not the game's; the cars are off\n");
				s_ready = -1;
			}
			if (!s_cfuncs) {
				Log::TypedLog(CHN_SDL, "Cars: the script commands are not there; the cars are off\n");
				s_ready = -1;
			}
		}
		return s_ready > 0;
	}

	// The scripts of cars.q, given to the game once (it keeps its own copy of each script)
	static bool Parsed() {
		if (s_parsed == 0) {
			static uint8_t data[sizeof(flickit_cars_qb)];
			memcpy(data, flickit_cars_qb, sizeof(data));
			ParseQBFromMemory("scripts\\flickit\\cars.qb", data, 1);
			const uint8_t* steal = LookUpSymbol(s_n.stealScript);
			const uint8_t* tune = LookUpSymbol(s_n.defaultTune);
			if (steal && steal[1] == SYMBOL_SCRIPT && tune && tune[1] == SYMBOL_STRUCTURE) {
				s_parsed = 1;
				Log::TypedLog(CHN_SDL, "Cars: on; Circle near a car takes it (reach %.0f); a car he left goes back after %u s (0: it stays); the game has Flickit's car scripts (%u bytes)\n",
					s_reach, s_standMs / 1000, (unsigned)sizeof(data));
			}
			else {
				s_parsed = -1;
				Log::TypedLog(CHN_SDL, "Cars: the game did not take Flickit's car scripts; the cars are off\n");
			}
		}
		return s_parsed > 0;
	}

	// Script::RunScript runs a script to its end, but what it gives back is no yes or no for a script: the low
	// byte of the address its CScript had, from the free just before it returns (0x0040FE2C -> 0x0040E370 ->
	// 0x0057A960), so 0 for about one in 64. Only a missing script gives 0 for sure. So: is it a script, then run it.
	static bool RunScriptOf(uint32_t name, void* params, void* object) {
		const uint8_t* symbol = LookUpSymbol(name);
		if (!symbol || symbol[1] != SYMBOL_SCRIPT) {
			return false;
		}
		RunGameScript(name, params, object, 0);
		return true;
	}

	static bool InNet() {
		void* net = *kNetManager;
		return net && InNetGame(net);
	}

	static float FrameSeconds() {
		LARGE_INTEGER now, freq;
		QueryPerformanceCounter(&now);
		QueryPerformanceFrequency(&freq);
		float s = 0.0f;
		if (s_lastFrame.QuadPart && freq.QuadPart) {
			s = (float)((double)(now.QuadPart - s_lastFrame.QuadPart) / (double)freq.QuadPart);
		}
		s_lastFrame = now;
		return s;
	}

	/******************************************************************/
	/*                                                                */
	/*  The cars of the level                                         */
	/*                                                                */
	/******************************************************************/

	static void* StructOfSymbol(uint32_t name) {
		const uint8_t* sym = LookUpSymbol(name);
		return sym && sym[1] == SYMBOL_STRUCTURE ? *(void* const*)(sym + 0xC) : 0;
	}

	// An array's element: a pointer for arrays and vectors; one element is kept in the array itself
	static void* Element(const Script::LazyArray* a, int i) {
		if (!a || i < 0 || i >= a->length) {
			return 0;
		}
		return a->length == 1 ? (void*)a->mp_array_data : ((void**)a->mp_array_data)[i];
	}

	// The box of a setup's (or a tuning's) colliders, if it has them
	static bool BoxOf(void* s, Drive::Box* out) {
		Script::LazyArray* all = 0;
		if (!s || !StructArray(s, s_n.colliders, &all, 0) || !all || all->length < 1) {
			return false;
		}
		const Script::LazyArray* base = (const Script::LazyArray*)Element(all, 0);
		if (!base || base->length < 3) {
			return false;
		}
		const float* a = (const float*)Element(base, 0);
		const float* b = (const float*)Element(base, 1);
		const float* c = (const float*)Element(base, 2);
		if (!a || !b || !c) {
			return false;
		}
		float top = 0.0f;
		for (int r = 0; r < all->length; r++) {
			const Script::LazyArray* rect = (const Script::LazyArray*)Element(all, r);
			for (int k = 0; rect && k < rect->length; k++) {
				const float* v = (const float*)Element(rect, k);
				if (v && v[1] > top) {
					top = v[1];
				}
			}
		}
		return Drive::BoxFromColliders(a, b, c, top + 8.0f, out);
	}

	// The tuning of cars.q for a model, the setup it names, and the box of the car
	static void TuneFor(const char* model, Car* c) {
		char name[96];
		void* tune = 0;
		c->tune = 0;
		if (Drive::TuneName(model, name, sizeof(name))) {
			c->tune = Flickit::NameChecksum(name);
			tune = StructOfSymbol(c->tune);
		}
		if (!tune) {
			c->tune = s_n.defaultTune;
			tune = StructOfSymbol(c->tune);
		}
		c->setup = 0;
		if (tune) {
			StructChecksum(tune, s_n.setup, &c->setup, 0);
		}
		if (!BoxOf(tune, &c->box) && !BoxOf(c->setup ? StructOfSymbol(c->setup) : 0, &c->box)) {
			c->box = Drive::UsualBox();
		}
	}

	static const char* ParkedModel(int slot) {
		const char* m = s_parking.Model(slot);
		return m ? m : "";
	}

	// The game's prompt for a car to drive ("Press Circle to drive", as car_scripts.q shows it for a car of the
	// level's own), while a car is in reach and he could take it; only the box: Circle itself is read here
	static void Prompt(bool want, uint32_t now) {
		if (want) {
			s_promptSeen = now;
			if (!s_prompt) {
				s_prompt = RunScriptOf(s_n.promptScript, 0, 0);
			}
		}
		else if (s_prompt && now - s_promptSeen > kPromptLingerMs) {
			RunGameScript(s_n.promptOffScript, 0, 0, 0);
			s_prompt = false;
		}
	}

	// The level's cars: its Vehicle nodes whose models have the car's skeleton (the one the game's
	// drivable car has), and a place for each parked car after them
	static void FindCars() {
		if (s_prompt) {
			RunGameScript(s_n.promptOffScript, 0, 0, 0);
			s_prompt = false;
		}
		s_levelCars = 0;
		s_parking.Clear();
		s_back.Clear();
		memset(&s_taken, 0, sizeof(s_taken));
		memset(&s_driven, 0, sizeof(s_driven));
		memset(&s_leaving, 0, sizeof(s_leaving));
		ResetCrashes();
		void* nodes = GetGlobalArray(s_n.nodeArray, 0);
		const uint32_t count = nodes ? *(const uint32_t*)((const uint8_t*)nodes + 8) : 0;
		int skipped = 0, atStart = 0;
		for (uint32_t i = 0; i < count && s_levelCars < kMaxCars; i++) {
			void* node = ArrayStructure(nodes, i);
			uint32_t cls = 0, name = 0, skeleton = s_n.car;
			const char* model = 0;
			if (!node || !StructChecksum(node, s_n.className, &cls, 0) || cls != s_n.vehicle) {
				continue;
			}
			StructChecksum(node, s_n.skeletonName, &skeleton, 0);
			if (!StructChecksum(node, s_n.name, &name, 0) || !name || !StructText(node, s_n.model, &model, 0) || !model || skeleton != s_n.car) {
				skipped++;
				continue;
			}
			Car& c = s_cars[s_levelCars++];
			memset(&c, 0, sizeof(c));
			c.name = name;
			strncpy_s(c.model, sizeof(c.model), model, _TRUNCATE);
			c.slot = -1;
			c.start[3] = 1.0f;
			StructVector(node, s_n.pos, c.start, 0);
			c.atStart = StructFlag(node, s_n.createdAtStart);
			atStart += c.atStart ? 1 : 0;
			TuneFor(c.model, &c);
		}
		FindPeds(nodes, count);
		for (int k = 0; k < Drive::Parking::kSlots; k++) {
			Car& c = s_cars[s_levelCars + k];
			memset(&c, 0, sizeof(c));
			c.name = s_n.parkedCar[k];
			c.slot = k;
		}
		Log::TypedLog(CHN_SDL, "Cars: %d cars in this level to take, %d of them there from its start%s\n", s_levelCars,
			atStart, skipped ? "; and some vehicles that are not cars: buses, trams, boats" : "");
	}

	static Car* CarNamed(uint32_t name) {
		for (int i = 0; i < s_levelCars + Drive::Parking::kSlots; i++) {
			if (s_cars[i].name == name) {
				return &s_cars[i];
			}
		}
		return 0;
	}

	// The object of a car, if it is there and is one: a car of the level has the traffic's physics, a
	// parked one the static car of the game's own
	static uint8_t* CarObject(const Car& c) {
		if (c.slot >= 0 && !s_parking.Used(c.slot)) {
			return 0;
		}
		uint8_t* obj = (uint8_t*)ResolveToObject(c.name);
		if (!obj || !GetComponent(obj, c.slot >= 0 ? s_n.staticVehicle : s_n.carPhysics)) {
			return 0;
		}
		return obj;
	}

	static uint8_t* PlayerVehicle(void** vehicle) {
		uint8_t* obj = (uint8_t*)ResolveToObject(s_n.playerVehicle);
		void* v = obj ? GetComponent(obj, s_n.vehicleComponent) : 0;
		if (vehicle) {
			*vehicle = v;
		}
		return v ? obj : 0;
	}

	/******************************************************************/
	/*                                                                */
	/*  The frame                                                     */
	/*                                                                */
	/******************************************************************/

	// A car of the level that is gone for good (nothing of it in the level now: the car he drove went with a
	// restart, the car he left made room for another): back at the start of its way once he is far from
	// there, if it was there from the level's start
	static void ComeBackLater(uint32_t origin, uint32_t now, const char* why) {
		const Car* o = origin ? CarNamed(origin) : 0;
		if (!o || o->slot >= 0 || !o->atStart) {
			return;
		}
		if (s_back.Add(origin, o->start, now)) {
			Log::TypedLog(CHN_SDL, "Cars: %08X comes back at the start of its way when he is away from there (%s)\n", origin, why);
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  Crashes                                                       */
	/*                                                                */
	/******************************************************************/

	static const float kInvMassUsual = 386.4f / 2943.0f;   // PoliceCarSetup's: the setup most cars get
	static const float kKnockFrom = 60.0f;      // a car of the traffic whose speed a hit would change this much goes off its way
	static const float kLaunchFrom = 700.0f;    // cars that meet this fast: the one hit is thrown up and over
	static const float kSoundFrom = 120.0f;
	static const float kDamageFloor = 100.0f;   // a hit's change of speed past this is damage
	static const float kLandingFloor = 300.0f;  // a loose car's landing, or the wall it meets
	static const float kWallFloor = 260.0f;     // his car's own change of speed in a frame (a wall, a post) past this
	static const float kBlastRadius = 600.0f;
	static const float kBlastHim = 350.0f;      // he is knocked down on foot this near
	static const int kKnocksAFrame = 2;
	static const int kKnocksABlast = 3;
	static const uint32_t kFallMs = 5000;       // a loose car with no ground under it this long fell out of the level

	static bool Finite(const float* v, int n) {
		for (int i = 0; i < n; i++) {
			if (!(v[i] > -1.0e7f && v[i] < 1.0e7f)) {
				return false;
			}
		}
		return true;
	}

	// A line for flickit_watchdog.txt (and the log): what the crashes did, a few hundred a session at most
	static int s_notes = 0;
	static void Note(const char* format, ...) {
		char text[400];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		text[sizeof(text) - 1] = 0;
		if (s_notes < 400) {
			s_notes++;
			FlickitGame::WatchNote(text);
		}
		else {
			Log::TypedLog(CHN_SDL, "%s", text);
		}
	}

	static bool CrashReady() {
		if (s_crashReady == 0) {
			s_crashReady = 1;
			for (size_t i = 0; i < sizeof(kCrashExpect) / sizeof(kCrashExpect[0]); i++) {
				if (memcmp((const void*)kCrashExpect[i].address, kCrashExpect[i].bytes, (size_t)kCrashExpect[i].size) != 0) {
					Note("Crashes: the game's code at %08X (%s) is not what was expected; the cars go through each other as before\n",
						kCrashExpect[i].address, kCrashExpect[i].what);
					s_crashReady = -1;
				}
			}
			if (s_crashReady > 0) {
				Note("Crashes: on; damage %.0f%%, launch %.0f%%\n", s_damageScale * 100.0f, s_launchScale * 100.0f);
			}
		}
		return s_crashReady > 0;
	}

	// Does the line hit the level itself (not its moving objects: a loose car's own collision would be in the
	// way), and where, and the face's normal there. Through the skater's physics' feeler, as its own line test.
	static bool WorldHit(const float from[3], const float to[3], float point[3], float normal[3]) {
		if (!s_feeler || !Finite(from, 3) || !Finite(to, 3)) {
			return false;
		}
		const float a[4] = { from[0], from[1], from[2], 1.0f };
		const float b[4] = { to[0], to[1], to[2], 1.0f };
		FeelerLine(s_feeler, a, b);
		FeelerIgnore(s_feeler, 0x10, 0);
		if (!FeelerHit(s_feeler, 0, 0)) {
			return false;
		}
		const float* p = (const float*)(s_feeler + FEELER_POINT);
		const float* n = (const float*)(s_feeler + FEELER_NORMAL);
		if (!Finite(p, 3) || !Finite(n, 3)) {
			return false;
		}
		memcpy(point, p, 3 * sizeof(float));
		memcpy(normal, n, 3 * sizeof(float));
		return Crash::Normalize3(normal);
	}

	// A car's mass for its size: the usual car's (most of the cars have PoliceCarSetup), more for more of it
	// along the ground
	static float InvMassFor(const Drive::Box& b) {
		const Drive::Box u = Drive::UsualBox();
		const float area = (b.front - b.rear) * b.halfWidth;
		float k = area > 1.0f ? ((u.front - u.rear) * u.halfWidth) / area : 1.0f;
		k = k < 0.4f ? 0.4f : (k > 2.5f ? 2.5f : k);
		return kInvMassUsual * k;
	}

	// About up, as a solid box of it
	static float InvInertiaFor(const Drive::Box& b, float invMass) {
		const float w = 2.0f * b.halfWidth, l = b.front - b.rear;
		return invMass * 12.0f / (w * w + l * l);
	}

	static void RowsOf(const float* m, float rows[3][3]) {
		for (int k = 0; k < 3; k++) {
			for (int i = 0; i < 3; i++) {
				rows[k][i] = m[k * 4 + i];
			}
		}
	}

	static void ResetLoose(int slot) {
		if (slot < 0 || slot >= Drive::Parking::kSlots) {
			return;
		}
		memset(&s_loose[slot], 0, sizeof(s_loose[slot]));
		s_loose[slot].health = Crash::Whole();
	}

	static void ResetCrashes() {
		for (int k = 0; k < Drive::Parking::kSlots; k++) {
			ResetLoose(k);
		}
		memset(s_fx, 0, sizeof(s_fx));       // (a new level: the effects went with the old one)
		s_drivenHealth = Crash::Whole();
		s_drivenHaveVel = false;
		s_lastParked = -1;
		s_boomPending = false;
		s_boomSlot = -1;
	}

	// The body of a loose car from its object, where it stands now, at rest
	static void MakeLoose(int slot, const uint8_t* obj) {
		Loose& l = s_loose[slot];
		const Drive::Box* box = s_parking.BoxOf(slot);
		const Drive::Box b = box ? *box : Drive::UsualBox();
		float rows[3][3];
		RowsOf((const float*)(obj + OBJECT_MATRIX), rows);
		Crash::MakeTumbler((const float*)(obj + OBJECT_POSITION), rows, b.halfWidth, b.rear, b.front, b.height, InvMassFor(b), &l.body);
		l.made = true;
		l.groundSeen = GetTickCount();
	}

	// A loose car's object where its body is
	static void PlaceLoose(int slot, uint8_t* obj) {
		const Loose& l = s_loose[slot];
		const Drive::Box* box = s_parking.BoxOf(slot);
		const Drive::Box b = box ? *box : Drive::UsualBox();
		float origin[3];
		Crash::OriginOf(l.body, b.rear, b.front, origin);
		if (!Finite(origin, 3) || !Finite(&l.body.rows[0][0], 9)) {
			return;
		}
		float* pos = (float*)(obj + OBJECT_POSITION);
		float* m = (float*)(obj + OBJECT_MATRIX);
		float* d = (float*)(obj + OBJECT_DISPLAY_MATRIX);
		memcpy(pos, origin, sizeof(origin));
		for (int k = 0; k < 3; k++) {
			for (int i = 0; i < 3; i++) {
				m[k * 4 + i] = l.body.rows[k][i];
				d[k * 4 + i] = l.body.rows[k][i];
			}
			m[k * 4 + 3] = 0.0f;
			d[k * 4 + 3] = 0.0f;
		}
	}

	// Can he take this car he left (or that was knocked)? Not on fire or burnt out, on its wheels, nearly still.
	static bool Takeable(int slot) {
		const Loose& l = s_loose[slot];
		if (l.health.burning || l.health.burnt) {
			return false;
		}
		return !l.made || (Crash::Upright(l.body) && Crash::Length3(l.body.vel) < 100.0f);
	}

	/* The effects */

	// The game's particle systems are kept by name (Nx::CParticleManager at 0x006FF6E8, a hash table that takes
	// two of one name: 0x0049D470) and taken out by name when their object is deleted, which is after the frame it
	// died in (0x0043FE24, 0x0041F9E0 takes out the first of that name it meets). An effect made again under the
	// name of one that died in the same frame left the dead one in the table, and the table's update called it
	// after its memory went to something else: the crash at 0x00421BEA (its update took its stack 4 off). The
	// game's own scripts wait a frame between the two (Emit_SpecialTrickParticles). Each effect here has a name
	// of its own that is never used again (Flickit_CarFx<n>).
	static uint32_t s_fxMade = 0;

	static uint32_t NewFxName() {
		char name[32];
		sprintf_s(name, sizeof(name), "Flickit_CarFx%u", ++s_fxMade);
		return Flickit::NameChecksum(name);
	}

	static void FxOff(uint32_t fx) {
		if (!fx) {
			return;
		}
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (!p) {
			return;
		}
		p->AddChecksum(s_n.fx, fx);
		RunGameScript(s_n.fxOffScript, p, 0, 0);
		Script::LazyStruct::s_free(p);
	}

	// An effect on a car (Flickit_CarFx): at its engine, a little behind its front, from where the car is now
	// (the game's lock moves it with the car from then on). Its name, 0 if there was no car.
	static uint32_t FxOn(uint32_t car, uint32_t parts, const Drive::Box& box) {
		const uint8_t* obj = (const uint8_t*)ResolveToObject(car);
		if (!obj || !parts) {
			return 0;
		}
		const float* pos = (const float*)(obj + OBJECT_POSITION);
		const float* m = (const float*)(obj + OBJECT_MATRIX);
		const float offset[3] = { 0.0f, box.height * 0.55f, box.front - (box.front - box.rear) * 0.22f };
		float at[3];
		for (int i = 0; i < 3; i++) {
			at[i] = pos[i] + m[i] * offset[0] + m[4 + i] * offset[1] + m[8 + i] * offset[2];
		}
		if (!Finite(at, 3)) {
			return 0;
		}
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (!p) {
			return 0;
		}
		const uint32_t fx = NewFxName();
		p->AddChecksum(s_n.fx, fx);
		p->AddChecksum(s_n.car, car);
		p->AddChecksum(s_n.parts, parts);
		StructAddVector(p, s_n.offset, offset[0], offset[1], offset[2]);
		StructAddVector(p, s_n.pos, at[0], at[1], at[2]);
		RunGameScript(s_n.fxScript, p, 0, 0);
		Script::LazyStruct::s_free(p);
		return fx;
	}

	static void KillFx(int i) {
		if (i < 0 || i >= kFxCars) {
			return;
		}
		FxOff(s_fx[i].smokeName);
		FxOff(s_fx[i].fireName);
		memset(&s_fx[i], 0, sizeof(s_fx[i]));
	}

	// What a car shows for how damaged it is: smoke, heavy smoke, fire (with heavy smoke), the smoke of a car
	// burnt out; made again when it is another object
	static void ShowFx(int i, uint32_t car, const Drive::Box& box, Crash::Stage stage) {
		const int smoke = stage == Crash::kSmoking ? 1 : (stage == Crash::kSmokingHard || stage == Crash::kBurning) ? 2 : stage == Crash::kBurnt ? 3 : 0;
		const bool fire = stage == Crash::kBurning;
		Fx& f = s_fx[i];
		if (f.car == car && f.smoke == smoke && f.fire == fire) {
			return;
		}
		const bool other = f.car != car;
		if (other || f.smoke != smoke) {
			FxOff(f.smokeName);
			f.smokeName = smoke ? FxOn(car, s_n.smokeParts[smoke], box) : 0;
		}
		if (other || f.fire != fire) {
			FxOff(f.fireName);
			f.fireName = fire ? FxOn(car, s_n.fireParts, box) : 0;
		}
		f.car = car;
		f.smoke = smoke;
		f.fire = fire;
	}

	static void Effects(bool driving) {
		for (int k = 0; k < Drive::Parking::kSlots; k++) {
			const Drive::Box* box = s_parking.BoxOf(k);
			if (!box || !ResolveToObject(s_n.parkedCar[k])) {
				if (s_fx[k].car) {
					KillFx(k);
				}
				continue;
			}
			ShowFx(k, s_n.parkedCar[k], *box, Crash::StageOf(s_loose[k].health));
		}
		if (driving && s_driven.set) {
			ShowFx(kFxDriven, s_n.playerVehicle, s_driven.box, Crash::StageOf(s_drivenHealth));
		}
		else if (s_fx[kFxDriven].car) {
			KillFx(kFxDriven);
		}
	}

	/* Sounds */

	// How near a place is to him for its sound: 1 there, nothing from 3000 away
	static float Near(const float at[3], const float* him) {
		const float dx = at[0] - him[0], dy = at[1] - him[1], dz = at[2] - him[2];
		const float away = sqrtf(dx * dx + dy * dy + dz * dz);
		return away < 3000.0f ? 1.0f - away / 3000.0f : 0.0f;
	}

	static void HitSound(float speed, const float at[3], const float* him, uint32_t now) {
		const float closeBy = Near(at, him);
		if (now - s_lastSound < 120 || closeBy < 0.05f) {
			return;
		}
		float vol = speed * 0.12f;
		vol = (vol < 40.0f ? 40.0f : (vol > 170.0f ? 170.0f : vol)) * closeBy;
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (!p) {
			return;
		}
		p->AddFloat(s_n.vol, vol);
		p->AddFloat(s_n.pitch, 85.0f + (float)((now * 7919u) % 30u));
		RunGameScript(s_n.hitSoundScript, p, 0, 0);
		Script::LazyStruct::s_free(p);
		s_lastSound = now;
	}

	/* His car: the game's own body */

	// An impulse on his car, at the touch along the ground at the height of its place (from above or below: at
	// its place), through its momentum and its spin's; then its speeds from them, awake
	static void PushHis(uint8_t* vehicle, const float* pos, const Crash::Contact& c, const float impulse[3], float sign) {
		float* momentum = (float*)(vehicle + VEHICLE_MOMENTUM);
		float* spinMomentum = (float*)(vehicle + VEHICLE_SPIN_MOMENTUM);
		const float invMass = *(const float*)(vehicle + VEHICLE_INV_MASS);
		const bool vertical = fabsf(c.normal[1]) > 0.5f;
		const float r[3] = { vertical ? 0.0f : c.point[0] - pos[0], 0.0f, vertical ? 0.0f : c.point[1] - pos[2] };
		const float j[3] = { impulse[0] * sign, impulse[1] * sign, impulse[2] * sign };
		float torque[3];
		Crash::Cross3(r, j, torque);
		for (int i = 0; i < 3; i++) {
			momentum[i] += j[i];
			spinMomentum[i] += torque[i];
			s_drivenPushed[i] += j[i] * invMass;
		}
		VehicleSpeeds(vehicle);
		*(uint32_t*)(vehicle + VEHICLE_AWAKE) = 1;
		*(uint32_t*)(vehicle + VEHICLE_STILL) = 0;
	}

	// His car thrown: up units a second, roll radians a second about axis (in the world), at its place
	static void ThrowHis(uint8_t* vehicle, float up, const float dv[3], float roll, const float axis[3]) {
		float* momentum = (float*)(vehicle + VEHICLE_MOMENTUM);
		float* spinMomentum = (float*)(vehicle + VEHICLE_SPIN_MOMENTUM);
		const float invMass = *(const float*)(vehicle + VEHICLE_INV_MASS);
		const float invRoll = *(const float*)(vehicle + VEHICLE_INV_INERTIA);
		if (!(invMass > 0.001f && invMass < 10.0f)) {
			return;
		}
		const bool rolls = invRoll > 1.0e-8f && invRoll < 1.0f;
		for (int i = 0; i < 3; i++) {
			const float v = (i == 1 ? up : 0.0f) + (dv ? dv[i] : 0.0f);
			momentum[i] += v / invMass;
			if (rolls) {
				spinMomentum[i] += axis[i] * roll / invRoll;
			}
			s_drivenPushed[i] += v;
		}
		VehicleSpeeds(vehicle);
		*(uint32_t*)(vehicle + VEHICLE_AWAKE) = 1;
		*(uint32_t*)(vehicle + VEHICLE_STILL) = 0;
	}

	static void HurtHis(float damage, uint32_t now, const char* how) {
		if (damage <= 0.0f) {
			return;
		}
		const Crash::Stage was = Crash::StageOf(s_drivenHealth);
		if (Crash::Hurt(&s_drivenHealth, damage, now) == Crash::kCaughtFire) {
			RunGameScript(s_n.onFireScript, 0, 0, 0);
			Note("Crashes: his car caught fire (%s); it blows up in %u s\n", how, Crash::kBurnMs / 1000);
		}
		else if (Crash::StageOf(s_drivenHealth) != was) {
			Note("Crashes: his car is %s (%s, %.0f left)\n", Crash::StageOf(s_drivenHealth) == Crash::kSmoking ? "smoking" : "smoking hard",
				how, s_drivenHealth.hp);
		}
	}

	static void HurtLoose(int slot, float damage, uint32_t now, const char* how) {
		if (damage <= 0.0f) {
			return;
		}
		if (Crash::Hurt(&s_loose[slot].health, damage, now) == Crash::kCaughtFire) {
			Note("Crashes: the car in slot %d caught fire (%s)\n", slot, how);
		}
	}

	// His car's own crashes into the level: its speed along the ground changed more in a frame than the game's
	// driving and Flickit's pushes do
	static void HisWalls(const uint8_t* vehicle, const float* at, const float* him, uint32_t now) {
		const float* v = (const float*)(vehicle + VEHICLE_VELOCITY);
		if (!Finite(v, 3)) {
			return;
		}
		if (s_drivenHaveVel) {
			const float dx = v[0] - (s_drivenLastVel[0] + s_drivenPushed[0]);
			const float dz = v[2] - (s_drivenLastVel[2] + s_drivenPushed[2]);
			const float change = sqrtf(dx * dx + dz * dz);
			if (change > kWallFloor) {
				HurtHis(Crash::Damage(change, kWallFloor, s_damageScale), now, "it hit the level");
				HitSound(change * 1.3f, at, him, now);
			}
		}
		memcpy(s_drivenLastVel, v, sizeof(s_drivenLastVel));
		memset(s_drivenPushed, 0, sizeof(s_drivenPushed));
		s_drivenHaveVel = true;
	}

	/* The cars that touch */

	enum { kHis, kTraffic, kLoose };
	struct Party {
		int kind;
		int index;                   // the car (s_cars) of the traffic, the slot of a loose car
		Crash::Body body;
		float invMass, invInertia;   // as if it could move (a car of the traffic cannot, until it is knocked)
		float radius;
		bool awake;                  // his car, or a loose car that moves
		bool knocked;
	};
	static Party s_parties[kMaxCars + Drive::Parking::kSlots + 1];
	static int s_partyCount = 0;

	struct Knock {
		int car;
		float vel[3];
		float spin;
		Crash::Launch launch;
		float damage;
	};

	// His car's box, its speeds and its mass, from the game's body
	static bool HisBody(const uint8_t* his, const uint8_t* vehicle, Crash::Body* out) {
		if (!his || !vehicle) {
			return false;
		}
		memset(out, 0, sizeof(*out));
		const float* pos = (const float*)(his + OBJECT_POSITION);
		const float* m = (const float*)(his + OBJECT_MATRIX);
		const float* v = (const float*)(vehicle + VEHICLE_VELOCITY);
		const float* w = (const float*)(vehicle + VEHICLE_SPIN);
		const float invMass = *(const float*)(vehicle + VEHICLE_INV_MASS);
		const float invInertia = *(const float*)(vehicle + VEHICLE_INV_INERTIA + 4);
		const Drive::Box& b = s_driven.box;
		if (!Finite(pos, 3) || !Finite(m, 12) || !Finite(v, 3) || !Finite(w, 3) || !(invMass > 0.001f && invMass < 10.0f) || !(invInertia > 1.0e-8f && invInertia < 1.0f)
			|| !Crash::MakeBox(pos, m, m + 8, b.halfWidth, b.rear, b.front, b.height, out)) {
			return false;
		}
		// it turns about its place (the game's body), and its speeds are its place's
		out->pivot[0] = pos[0];
		out->pivot[1] = pos[2];
		memcpy(out->vel, v, sizeof(out->vel));
		out->spin = w[1];
		out->invMass = invMass;
		out->invInertia = invInertia;
		return true;
	}

	static void AddParty(const Party& p) {
		if (s_partyCount < (int)(sizeof(s_parties) / sizeof(s_parties[0]))) {
			s_parties[s_partyCount] = p;
			s_parties[s_partyCount].radius = sqrtf(p.body.halfX * p.body.halfX + p.body.halfZ * p.body.halfZ);
			s_partyCount++;
		}
	}

	static void Parties(const uint8_t* his, const uint8_t* vehicle) {
		s_partyCount = 0;
		Party p;
		memset(&p, 0, sizeof(p));
		if (HisBody(his, vehicle, &p.body)) {
			p.kind = kHis;
			p.index = -1;
			p.invMass = p.body.invMass;
			p.invInertia = p.body.invInertia;
			p.awake = true;
			AddParty(p);
		}
		for (int i = 0; i < s_levelCars; i++) {
			const Car& c = s_cars[i];
			const uint8_t* obj = CarObject(c);
			if (!obj) {
				continue;
			}
			memset(&p, 0, sizeof(p));
			const float* pos = (const float*)(obj + OBJECT_POSITION);
			const float* m = (const float*)(obj + OBJECT_MATRIX);
			if (!Finite(pos, 3) || !Finite(m, 12) || !Crash::MakeBox(pos, m, m + 8, c.box.halfWidth, c.box.rear, c.box.front, c.box.height, &p.body)) {
				continue;
			}
			if (c.track.frames >= 2) {
				memcpy(p.body.vel, c.track.vel, sizeof(p.body.vel));
			}
			p.kind = kTraffic;
			p.index = i;
			p.invMass = InvMassFor(c.box);
			p.invInertia = InvInertiaFor(c.box, p.invMass);
			AddParty(p);
		}
		for (int k = 0; k < Drive::Parking::kSlots; k++) {
			const uint8_t* obj = s_parking.Used(k) ? (const uint8_t*)ResolveToObject(s_n.parkedCar[k]) : 0;
			if (!obj || !GetComponent((void*)obj, s_n.staticVehicle)) {
				continue;
			}
			if (!s_loose[k].made) {
				MakeLoose(k, obj);
			}
			memset(&p, 0, sizeof(p));
			if (!Crash::FootprintOf(s_loose[k].body, &p.body)) {
				continue;
			}
			p.kind = kLoose;
			p.index = k;
			p.invMass = p.body.invMass;
			p.invInertia = p.body.invInertia;
			p.awake = !s_loose[k].body.asleep;
			AddParty(p);
		}
	}

	// A car of the traffic is knocked off its way by what hit it: his car, or a loose car thrown at it
	static bool Knocks(const Party& by) {
		return by.kind == kHis || (by.kind == kLoose && by.awake && Crash::Length3(by.body.vel) > 150.0f);
	}

	static void QueueKnock(Knock* knocks, int* count, const Party& t, const float dv[3], float spin, const Crash::Launch& launch, float damage) {
		for (int i = 0; i < *count; i++) {
			if (knocks[i].car == t.index) {
				return;
			}
		}
		if (*count >= kKnocksAFrame) {
			return;
		}
		Knock& k = knocks[(*count)++];
		k.car = t.index;
		for (int i = 0; i < 3; i++) {
			k.vel[i] = t.body.vel[i] + dv[i];
		}
		k.spin = t.body.spin + spin;
		k.launch = launch;
		k.damage = damage;
	}

	// The touches of this frame: each car that moves (his, a loose one) against every other near it, and the
	// cars of the traffic against loose cars standing in their way. Knocks: the cars of the traffic to knock off.
	static int Touch(uint8_t* his, uint8_t* vehicle, const DriveGame::Skater& s, uint32_t now, Knock* knocks) {
		const Crash::Settings set = Crash::DefaultSettings();
		int count = 0;
		for (int i = 0; i < s_partyCount; i++) {
			for (int j = i + 1; j < s_partyCount; j++) {
				Party& A = s_parties[i];
				Party& B = s_parties[j];
				if (A.kind == kTraffic && B.kind == kTraffic) {
					continue;
				}
				if (!A.awake && !B.awake) {
					// two loose cars at rest, or a car of the traffic and a loose one: only if the traffic moves
					const Party* t = A.kind == kTraffic ? &A : B.kind == kTraffic ? &B : 0;
					if (!t || Crash::Dot3(t->body.vel, t->body.vel) < 25.0f) {
						continue;
					}
				}
				const float dx = A.body.centre[0] - B.body.centre[0], dz = A.body.centre[1] - B.body.centre[1];
				const float reach = A.radius + B.radius;
				if (dx * dx + dz * dz > reach * reach) {
					continue;
				}
				Crash::Contact c;
				if (!Crash::Overlap(A.body, B.body, &c)) {
					continue;
				}
				Crash::Body a = A.body, b = B.body;
				a.invMass = A.invMass;
				a.invInertia = A.invInertia;
				b.invMass = B.invMass;
				b.invInertia = B.invInertia;
				Crash::Result r;
				if (!Crash::Resolve(a, b, c, set, &r)) {
					continue;
				}
				const float at[3] = { c.point[0], 0.5f * ((A.body.low > B.body.low ? A.body.low : B.body.low) + (A.body.high < B.body.high ? A.body.high : B.body.high)), c.point[1] };
				if (r.approach >= kSoundFrom) {
					HitSound(r.approach, at, s.pos, now);
				}
				const float share = r.hitA + r.hitB > 1.0f ? 2.0f / (r.hitA + r.hitB) : 0.0f;
				for (int side = 0; side < 2; side++) {
					Party& X = side == 0 ? A : B;
					const Party& O = side == 0 ? B : A;
					const float sign = side == 0 ? 1.0f : -1.0f;
					const float hit = side == 0 ? r.hitA : r.hitB;
					const float spin = side == 0 ? r.spinA : r.spinB;
					const float invMass = side == 0 ? a.invMass : b.invMass;
					const float push[3] = { c.normal[0] * sign, c.normal[1] * sign, c.normal[2] * sign };
					float scale = hit * share;
					scale = scale > 1.5f ? 1.5f : scale;
					const float damage = Crash::Damage(hit, kDamageFloor, s_damageScale);
					const float dv[3] = { r.impulse[0] * sign * invMass, r.impulse[1] * sign * invMass, r.impulse[2] * sign * invMass };
					if (X.kind == kTraffic) {
						// it goes on its way unless hit hard enough by something that knocks it off
						if (!X.knocked && hit >= kKnockFrom && Knocks(O)) {
							X.knocked = true;
							QueueKnock(knocks, &count, X, dv, spin, Crash::LaunchFor(r.approach, push, kLaunchFrom, s_launchScale * scale), damage);
						}
						continue;
					}
					if (X.kind == kHis) {
						PushHis(vehicle, (const float*)(his + OBJECT_POSITION), c, r.impulse, sign);
						const Crash::Launch l = Crash::LaunchFor(r.approach, push, kLaunchFrom + 300.0f, 0.5f * s_launchScale * scale);
						if (l.up > 0.0f) {
							ThrowHis(vehicle, l.up, 0, l.roll, l.axis);
						}
						HurtHis(damage, now, "a crash");
					}
					else {
						Loose& l = s_loose[X.index];
						Crash::TakeHit(&l.body, c, r.impulse, sign);
						const Crash::Launch launch = Crash::LaunchFor(r.approach, push, kLaunchFrom, s_launchScale * scale);
						if (launch.up > 0.0f) {
							Crash::Throw(&l.body, launch.up, launch.roll, launch.axis);
						}
						HurtLoose(X.index, damage, now, "a crash");
						X.awake = true;
					}
					for (int k = 0; k < 3; k++) {
						X.body.vel[k] += dv[k];
					}
					X.body.spin += spin;
				}
			}
		}
		return count;
	}

	// A car of the traffic knocked off its way: it goes, and a car of the same model stands in its place under the
	// name of a free slot (or of the loose car left longest ago away from him, which makes room), moving as the
	// hit left it. False when there is no room (it goes on its way).
	static bool KnockOff(const Knock& k, const DriveGame::Skater& s, uint32_t now, const char* why) {
		Car& c = s_cars[k.car];
		const uint8_t* obj = CarObject(c);
		if (!obj || c.slot >= 0) {
			return false;
		}
		bool inUse[Drive::Parking::kSlots], away[Drive::Parking::kSlots];
		for (int i = 0; i < Drive::Parking::kSlots; i++) {
			const uint8_t* there = (const uint8_t*)ResolveToObject(s_n.parkedCar[i]);
			inUse[i] = s_parking.Used(i) || there;
			away[i] = false;
			if (there && s.pos) {
				const float* at = (const float*)(there + OBJECT_POSITION);
				const Loose& l = s_loose[i];
				away[i] = Drive::FarApart(at, s.pos, kFar) && !l.health.burning && (!l.made || l.body.asleep);
			}
		}
		const int slot = s_parking.Room(inUse, away);
		if (slot < 0) {
			return false;
		}
		if (inUse[slot]) {
			ComeBackLater(s_parking.Origin(slot), now, "a car knocked off its way made room");
			KillFx(slot);
			s_parking.Leave(slot);
		}
		float pos[4], m[16];
		memcpy(pos, obj + OBJECT_POSITION, sizeof(pos));
		memcpy(m, obj + OBJECT_MATRIX, sizeof(m));
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (!p) {
			return false;
		}
		p->AddChecksum(s_n.car, c.name);
		p->AddChecksum(s_n.parked, s_n.parkedCar[slot]);
		p->AddString(s_n.model, c.model);
		StructAddVector(p, s_n.pos, pos[0], pos[1], pos[2]);
		const bool ran = RunScriptOf(s_n.knockedScript, p, 0);
		Script::LazyStruct::s_free(p);
		c.track.Reset();
		ResetLoose(slot);
		uint8_t* made = (uint8_t*)ResolveToObject(s_n.parkedCar[slot]);
		if (!ran || !made || !GetComponent(made, s_n.staticVehicle)) {
			Note("Crashes: %08X was knocked off its way, but %s\n", c.name, ran ? "no car stood in its place" : "Flickit_CarKnocked did not run");
			if (!ResolveToObject(c.name)) {
				ComeBackLater(c.name, now, "it was knocked off its way");
			}
			return false;
		}
		memcpy(made + OBJECT_POSITION, pos, sizeof(pos));
		memcpy(made + OBJECT_MATRIX, m, 12 * sizeof(float));
		memcpy(made + OBJECT_DISPLAY_MATRIX, m, 12 * sizeof(float));
		s_parking.Park(slot, c.model, c.box, c.name, now);
		Car* left = CarNamed(s_n.parkedCar[slot]);
		if (left) {
			left->track.Reset();
			TuneFor(c.model, left);
		}
		Loose& l = s_loose[slot];
		MakeLoose(slot, made);
		memcpy(l.body.vel, k.vel, sizeof(l.body.vel));
		l.body.w[1] = k.spin;
		Crash::Wake(&l.body);
		if (k.launch.up > 0.0f || k.launch.roll > 0.0f) {
			Crash::Throw(&l.body, k.launch.up, k.launch.roll, k.launch.axis);
		}
		HurtLoose(slot, k.damage, now, why);
		Note("Crashes: %08X (%s) knocked off its way (%s) at (%.0f %.0f %.0f), going %.0f units/s%s; slot %d\n", c.name, c.model, why,
			pos[0], pos[1], pos[2], Crash::Length3(l.body.vel), k.launch.up > 0.0f ? ", thrown up" : "", slot);
		return true;
	}

	// The planes a loose car meets: the ground under its middle, and the walls each way along the ground it
	// faces and across, as far as its corners reach (the level's own faces; the cars are the touches)
	static int PlanesFor(const Crash::Tumbler& t, Crash::Plane* out, bool* ground) {
		int n = 0;
		*ground = false;
		float point[3], normal[3];
		const float reach = Crash::Length3(t.half);
		const float below[3] = { t.com[0], t.com[1] - reach - 400.0f, t.com[2] };
		const float from[3] = { t.com[0], t.com[1] + 20.0f, t.com[2] };
		const float higher[3] = { t.com[0], t.com[1] + 150.0f, t.com[2] };
		if ((WorldHit(from, below, point, normal) && normal[1] > 0.3f) ||
			(WorldHit(higher, below, point, normal) && normal[1] > 0.3f && point[1] < t.com[1] + 40.0f)) {
			memcpy(out[n].point, point, sizeof(point));
			memcpy(out[n].normal, normal, sizeof(normal));
			n++;
			*ground = point[1] > t.com[1] - reach - 30.0f;
		}
		Crash::Body f;
		if (!Crash::FootprintOf(t, &f)) {
			return n;
		}
		const float ways[4][2] = { { f.axisZ[0], f.axisZ[1] }, { -f.axisZ[0], -f.axisZ[1] }, { f.axisX[0], f.axisX[1] }, { -f.axisX[0], -f.axisX[1] } };
		for (int i = 0; i < 4; i++) {
			const float length = reach + 25.0f;
			const float to[3] = { t.com[0] + ways[i][0] * length, t.com[1], t.com[2] + ways[i][1] * length };
			if (WorldHit(t.com, to, point, normal) && normal[1] < 0.7f && normal[0] * ways[i][0] + normal[2] * ways[i][1] < 0.0f) {
				memcpy(out[n].point, point, sizeof(point));
				memcpy(out[n].normal, normal, sizeof(normal));
				n++;
			}
		}
		return n;
	}

	// A loose car goes for good (it fell out of the level): the car of the level it was comes back later
	static void LooseGoes(int slot, uint32_t now, const char* why) {
		KillFx(slot);
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (p) {
			p->AddChecksum(s_n.parked, s_n.parkedCar[slot]);
			RunGameScript(s_n.goesBackScript, p, 0, 0);
			Script::LazyStruct::s_free(p);
		}
		ComeBackLater(s_parking.Origin(slot), now, why);
		s_parking.Leave(slot);
		ResetLoose(slot);
		Car* c = CarNamed(s_n.parkedCar[slot]);
		if (c) {
			c->track.Reset();
		}
		Note("Crashes: the car in slot %d went (%s)\n", slot, why);
	}

	// The loose cars that move, a frame on: against the level, then their objects where they are
	static void Tumbles(float dt, uint32_t now, const DriveGame::Skater& s) {
		const Crash::TumbleSettings set = Crash::DefaultTumble();
		for (int k = 0; k < Drive::Parking::kSlots; k++) {
			Loose& l = s_loose[k];
			if (!l.made || l.body.asleep || !s_parking.Used(k)) {
				continue;
			}
			uint8_t* obj = (uint8_t*)ResolveToObject(s_n.parkedCar[k]);
			if (!obj) {
				continue;
			}
			Crash::Plane planes[5];
			bool ground = false;
			const int n = PlanesFor(l.body, planes, &ground);
			if (ground) {
				l.groundSeen = now;
			}
			else if (now - l.groundSeen > kFallMs) {
				LooseGoes(k, now, "it fell out of the level");
				continue;
			}
			const Crash::TumbleResult r = Crash::Tumble(&l.body, dt, planes, n, set);
			if (r.hit > kLandingFloor) {
				HurtLoose(k, Crash::Damage(r.hit, kLandingFloor, s_damageScale), now, "it landed hard");
				HitSound(r.hit, l.body.com, s.pos, now);
			}
			PlaceLoose(k, obj);
			if (r.cameToRest) {
				Note("Crashes: the car in slot %d came to rest at (%.0f %.0f %.0f), %s\n", k, l.body.com[0], l.body.com[1], l.body.com[2],
					Crash::Upright(l.body) ? "on its wheels" : l.body.rows[1][1] < -0.7f ? "on its roof" : "on its side");
			}
		}
	}

	/* Fire, and blowing up */

	// The push of a blast at at on everything near it: the loose cars, his car, the cars of the traffic (knocked
	// off their way), and him on foot (knocked down). Damage too: a car near enough catches fire in its turn.
	static void Blast(const float at[3], int except, uint32_t now, const DriveGame::Skater& s, uint8_t* his, uint8_t* vehicle, bool knockHim) {
		for (int k = 0; k < Drive::Parking::kSlots; k++) {
			Loose& l = s_loose[k];
			if (k == except || !l.made || !s_parking.Used(k) || !ResolveToObject(s_n.parkedCar[k])) {
				continue;
			}
			float d[3] = { l.body.com[0] - at[0], 0.0f, l.body.com[2] - at[2] };
			const float away = Crash::Length3(d);
			const float push = Crash::Blast(away, kBlastRadius);
			if (push <= 0.0f) {
				continue;
			}
			if (!Crash::Normalize3(d)) {
				d[0] = 1.0f;
			}
			// pushed below its middle: it goes over away from the blast
			const float r[3] = { 0.0f, -l.body.half[1] * 0.6f, 0.0f };
			const float p[3] = { d[0] * 700.0f * push / l.body.invMass, 380.0f * push / l.body.invMass, d[2] * 700.0f * push / l.body.invMass };
			Crash::Push(&l.body, r, p);
			Crash::Wake(&l.body);
			HurtLoose(k, 900.0f * push * s_damageScale, now, "a blast");
		}
		if (his && vehicle) {
			const float* pos = (const float*)(his + OBJECT_POSITION);
			float d[3] = { pos[0] - at[0], 0.0f, pos[2] - at[2] };
			const float push = Crash::Blast(Crash::Length3(d), kBlastRadius);
			if (push > 0.0f) {
				if (!Crash::Normalize3(d)) {
					d[0] = 1.0f;
				}
				const float dv[3] = { d[0] * 600.0f * push, 0.0f, d[2] * 600.0f * push };
				const float axis[3] = { d[2], 0.0f, -d[0] };
				ThrowHis(vehicle, 300.0f * push, dv, 0.0f, axis);
				HurtHis(900.0f * push * s_damageScale, now, "a blast");
			}
		}
		int knocked = 0;
		for (int i = 0; i < s_levelCars && knocked < kKnocksABlast; i++) {
			const Car& c = s_cars[i];
			const uint8_t* obj = CarObject(c);
			if (!obj) {
				continue;
			}
			const float* pos = (const float*)(obj + OBJECT_POSITION);
			float d[3] = { pos[0] - at[0], 0.0f, pos[2] - at[2] };
			const float push = Crash::Blast(Crash::Length3(d), kBlastRadius);
			if (push < 0.1f) {
				continue;
			}
			if (!Crash::Normalize3(d)) {
				d[0] = 1.0f;
			}
			Knock k;
			memset(&k, 0, sizeof(k));
			k.car = i;
			for (int j = 0; j < 3; j++) {
				k.vel[j] = (c.track.frames >= 2 ? c.track.vel[j] : 0.0f) + d[j] * 700.0f * push;
			}
			k.vel[1] += 380.0f * push;
			k.launch.roll = 3.0f * push;
			k.launch.axis[0] = d[2];
			k.launch.axis[2] = -d[0];
			k.damage = 900.0f * push * s_damageScale;
			knocked += KnockOff(k, s, now, "a blast") ? 1 : 0;
		}
		if (knockHim && !s.driving && s.pos && s.object) {
			const float dx = s.pos[0] - at[0], dy = s.pos[1] - at[1], dz = s.pos[2] - at[2];
			if (dx * dx + dy * dy + dz * dz < kBlastHim * kBlastHim) {
				RunGameScript(s_n.blastHimScript, 0, s.object, 0);
				Note("Crashes: the blast knocked him down\n");
			}
		}
	}

	// The fireball and its bang at a place
	static void Boom(const float at[3], const float* him) {
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (p) {
			StructAddVector(p, s_n.pos, at[0], at[1], at[2]);
			p->AddFloat(s_n.vol, 60.0f + 390.0f * (him ? Near(at, him) : 1.0f));
			RunGameScript(s_n.boomScript, p, 0, 0);
			Script::LazyStruct::s_free(p);
		}
	}

	// A loose car burnt out: dark, thrown up and over
	static void Burnt(int slot, uint32_t now) {
		Loose& l = s_loose[slot];
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (p) {
			p->AddChecksum(s_n.car, s_n.parkedCar[slot]);
			RunGameScript(s_n.burntScript, p, 0, 0);
			Script::LazyStruct::s_free(p);
		}
		const float turn = (float)(now % 628u) * 0.01f;
		const float axis[3] = { cosf(turn), 0.0f, sinf(turn) };
		const float launch = s_launchScale > 0.3f ? s_launchScale : 0.3f;
		Crash::Throw(&l.body, (400.0f + (float)(now % 120u)) * launch, (2.0f + (float)(now % 17u) * 0.1f) * launch, axis);
	}

	// A loose car blows up: the fireball, burnt dark, thrown up and over, and the blast
	static void BlowUp(int slot, uint32_t now, const DriveGame::Skater& s, uint8_t* his, uint8_t* vehicle) {
		uint8_t* obj = (uint8_t*)ResolveToObject(s_n.parkedCar[slot]);
		if (!obj) {
			return;
		}
		Loose& l = s_loose[slot];
		if (!l.made) {
			MakeLoose(slot, obj);
		}
		Boom(l.body.com, s.pos);
		Burnt(slot, now);
		Blast(l.body.com, slot, now, s, his, vehicle, true);
		Note("Crashes: the car in slot %d blew up at (%.0f %.0f %.0f)\n", slot, l.body.com[0], l.body.com[1], l.body.com[2]);
	}

	// His car blowing up with him in it: the fireball and the blast now; out the way he gets out (a spawned
	// script, as the game's own exit: Flickit_CarBlowOut), knocked down, and the car he leaves behind (where it
	// was, as a car he left) burnt and thrown, going as his car went, once it stands there (Flickit_CarParked)
	static void BlowUpHis(const DriveGame::Skater& s, const uint8_t* his, const uint8_t* vehicle, uint32_t now) {
		const float* v = (const float*)(vehicle + VEHICLE_VELOCITY);
		memset(s_boomVel, 0, sizeof(s_boomVel));
		if (Finite(v, 3)) {
			memcpy(s_boomVel, v, sizeof(s_boomVel));
		}
		const float* pos = (const float*)(his + OBJECT_POSITION);
		const float at[3] = { pos[0], pos[1] + 20.0f, pos[2] };
		Boom(at, s.pos);
		Blast(at, -1, now, s, 0, 0, false);
		s_boomPending = RunScriptOf(s_n.blowOutScript, 0, s.object);
		s_boomSlot = -1;
		s_boomSince = now;
		Note("Crashes: his car blew up with him in it at (%.0f %.0f %.0f)%s\n", pos[0], pos[1], pos[2],
			s_boomPending ? "; he is thrown out" : ", but Flickit_CarBlowOut did not run");
	}

	static void AfterBlowOut(uint32_t now) {
		if (!s_boomPending) {
			return;
		}
		uint8_t* obj = s_boomSlot >= 0 ? (uint8_t*)ResolveToObject(s_n.parkedCar[s_boomSlot]) : 0;
		if (obj) {
			MakeLoose(s_boomSlot, obj);
			memcpy(s_loose[s_boomSlot].body.vel, s_boomVel, sizeof(s_boomVel));
			Crash::Wake(&s_loose[s_boomSlot].body);
			Burnt(s_boomSlot, now);
			s_boomPending = false;
			Note("Crashes: he is out; the car he was in is burnt out in slot %d\n", s_boomSlot);
		}
		else if (now - s_boomSince > 3000) {
			s_boomPending = false;
			Note("Crashes: his car blew up, but he did not get out of it\n");
		}
	}

	static void Burning(uint32_t now, const DriveGame::Skater& s, uint8_t* his, uint8_t* vehicle) {
		for (int k = 0; k < Drive::Parking::kSlots; k++) {
			if (s_parking.Used(k) && Crash::Tick(&s_loose[k].health, now) == Crash::kBlewUp) {
				BlowUp(k, now, s, his, vehicle);
			}
		}
		if (his && vehicle && s_driven.set && Crash::Tick(&s_drivenHealth, now) == Crash::kBlewUp) {
			BlowUpHis(s, his, vehicle, now);
		}
	}

	// The crashes of this frame, once the cars' places and speeds are known
	static void CrashFrame(const DriveGame::Skater& s, uint32_t now, float seconds) {
		if (!s_crashes || !CrashReady()) {
			return;
		}
		uint8_t* core = (uint8_t*)GetComponent(s.object, kSkaterCorePhysics);
		s_feeler = core ? core + CORE_FEELER : 0;
		// a frame after a pause (or a load) moves nothing
		const float dt = seconds > 0.25f ? 0.0f : (seconds > 0.05f ? 0.05f : seconds);
		void* vehicle = 0;
		uint8_t* his = s.driving && s_driven.set ? PlayerVehicle(&vehicle) : 0;
		if (his) {
			HisWalls((const uint8_t*)vehicle, (const float*)(his + OBJECT_POSITION), s.pos, now);
		}
		else {
			s_drivenHaveVel = false;
		}
		Parties(his, (const uint8_t*)vehicle);
		Knock knocks[kKnocksAFrame];
		const int count = Touch(his, (uint8_t*)vehicle, s, now, knocks);
		for (int i = 0; i < count; i++) {
			KnockOff(knocks[i], s, now, "a crash");
		}
		AfterBlowOut(now);
		Tumbles(dt, now, s);
		Burning(now, s, his, (uint8_t*)vehicle);
		Effects(s.driving);
	}

	/******************************************************************/
	/*                                                                */
	/*  People run over                                               */
	/*                                                                */
	/******************************************************************/

	static bool PedReady() {
		if (s_pedReady == 0) {
			s_pedReady = 1;
			for (size_t i = 0; i < sizeof(kPedExpect) / sizeof(kPedExpect[0]); i++) {
				if (memcmp((const void*)kPedExpect[i].address, kPedExpect[i].bytes, (size_t)kPedExpect[i].size) != 0) {
					Note("People: the game's code at %08X (%s) is not what was expected; cars go through people as before\n",
						kPedExpect[i].address, kPedExpect[i].what);
					s_pedReady = -1;
				}
			}
			const uint8_t* hit = LookUpSymbol(s_n.pedHitScript);
			if (s_pedReady > 0 && !(hit && hit[1] == SYMBOL_SCRIPT)) {
				Note("People: the game does not have Flickit_PedHit; cars go through people as before\n");
				s_pedReady = -1;
			}
		}
		return s_pedReady > 0;
	}

	// The level's people: every node of the class Pedestrian
	static void FindPeds(void* nodes, uint32_t count) {
		s_pedCount = 0;
		for (uint32_t i = 0; nodes && i < count && s_pedCount < kMaxPeds; i++) {
			void* node = ArrayStructure(nodes, i);
			uint32_t cls = 0, name = 0;
			if (!node || !StructChecksum(node, s_n.className, &cls, 0) || cls != s_n.pedestrian || !StructChecksum(node, s_n.name, &name, 0) || !name) {
				continue;
			}
			Ped& p = s_peds[s_pedCount++];
			memset(&p, 0, sizeof(p));
			p.name = name;
			p.start[3] = 1.0f;
			StructVector(node, s_n.pos, p.start, 0);
		}
		Log::TypedLog(CHN_SDL, "People: %d in this level (the nodes of the class Pedestrian)%s\n", s_pedCount,
			s_runOver ? "" : "; cars go through them (RunOver=0)");
	}

	// Why someone cannot be run over (0: they can): no ped logic, none of the game's falling down, a goal's
	static const char* NotRunOver(uint8_t* obj) {
		if (!GetComponent(obj, s_n.pedLogic)) {
			return "no ped logic";
		}
		void* tags = *(void**)(obj + OBJECT_TAGS);
		Script::LazyArray* anims = 0;
		if (!tags || !StructArray(tags, s_n.fallDownAnims, &anims, 0) || !anims || !StructArray(tags, s_n.layIdleAnims, &anims, 0) || !anims) {
			return "none of the game's falling down for them";
		}
		uint32_t goal = 0;
		int goalPed = 0;
		if (StructChecksum(tags, s_n.goalId, &goal, 0) || (StructInteger(tags, s_n.isGoalPed, &goalPed, 0) && goalPed)) {
			return "a goal's";
		}
		return 0;
	}

	// Someone where their flight has them, facing the way it says (their matrix and the one they are drawn with)
	static void PlacePed(uint8_t* obj, const Peds::Flight& f) {
		if (!Finite(f.pos, 3) || !Finite(f.facing, 2)) {
			return;
		}
		memcpy(obj + OBJECT_POSITION, f.pos, sizeof(f.pos));
		const float fx = f.facing[0], fz = f.facing[1];
		const float rows[3][3] = { { fz, 0.0f, -fx }, { 0.0f, 1.0f, 0.0f }, { fx, 0.0f, fz } };
		float* m = (float*)(obj + OBJECT_MATRIX);
		float* d = (float*)(obj + OBJECT_DISPLAY_MATRIX);
		for (int k = 0; k < 3; k++) {
			for (int i = 0; i < 3; i++) {
				m[k * 4 + i] = rows[k][i];
				d[k * 4 + i] = rows[k][i];
			}
			m[k * 4 + 3] = 0.0f;
			d[k * 4 + 3] = 0.0f;
		}
	}

	// The level about someone in flight this step: the ground under where they are going (a face that looks up),
	// and a wall between here and there at the height of their middle
	static Peds::Level PedLevel(const Peds::Flight& f, float dt) {
		Peds::Level l;
		l.ground = false;
		l.groundY = 0.0f;
		l.wall = false;
		const float next[3] = { f.pos[0] + f.vel[0] * dt, f.pos[1] + (f.landed ? 0.0f : f.vel[1] * dt), f.pos[2] + f.vel[2] * dt };
		const float top = (f.pos[1] > next[1] ? f.pos[1] : next[1]) + 20.0f;
		const float from[3] = { next[0], top, next[2] };
		const float to[3] = { next[0], next[1] - 600.0f, next[2] };
		float point[3], normal[3];
		if (WorldHit(from, to, point, normal) && normal[1] > 0.5f) {
			l.ground = true;
			l.groundY = point[1];
		}
		const float a[3] = { f.pos[0], f.pos[1] + 36.0f, f.pos[2] };
		const float b[3] = { next[0], next[1] + 36.0f, next[2] };
		if ((a[0] != b[0] || a[2] != b[2]) && WorldHit(a, b, point, normal) && fabsf(normal[1]) < 0.7f) {
			l.wall = true;
		}
		return l;
	}

	static void RunOver(Ped& p, uint8_t* obj, const Crash::Body& car, const Peds::Strike& st, const DriveGame::Skater& s, uint32_t now) {
		const float* at = (const float*)(obj + OBJECT_POSITION);
		const float feet[3] = { at[0], at[1], at[2] };
		// their own script becomes Flickit_PedDown (PedReady saw that the game has it)
		RunScriptOf(s_n.pedHitScript, 0, obj);
		Peds::Settings set = Peds::DefaultSettings();
		set.from = s_runOverFrom;
		Peds::Throw(car, feet, st, set, &p.flight);
		PlacePed(obj, p.flight);
		HitSound(st.speed * 0.7f, feet, s.pos, now);
		p.state = kPedFlying;
		p.since = now;
		Note("People: %08X run over at %.0f (%.0f %.0f %.0f), thrown at %.0f, %.0f up\n", p.name, st.speed, feet[0], feet[1], feet[2],
			sqrtf(p.flight.vel[0] * p.flight.vel[0] + p.flight.vel[2] * p.flight.vel[2]), p.flight.vel[1]);
	}

	// The people of this frame: anyone in the way of his car or of a loose car thrown about, run over; the thrown
	// in flight; those who have lain there long enough with him far away, back on their way
	static void PedsFrame(const DriveGame::Skater& s, uint32_t now, float seconds) {
		if (!s_runOver || s_pedCount == 0 || !CrashReady() || !PedReady()) {
			return;
		}
		uint8_t* core = (uint8_t*)GetComponent(s.object, kSkaterCorePhysics);
		s_feeler = core ? core + CORE_FEELER : 0;
		const float dt = seconds > 0.25f ? 0.0f : (seconds > 0.05f ? 0.05f : seconds);
		Crash::Body hitters[1 + Drive::Parking::kSlots];
		int count = 0;
		void* vehicle = 0;
		uint8_t* his = s.driving && s_driven.set ? PlayerVehicle(&vehicle) : 0;
		if (HisBody(his, (const uint8_t*)vehicle, &hitters[count])) {
			count++;
		}
		for (int k = 0; s_crashes && k < Drive::Parking::kSlots; k++) {
			const Loose& l = s_loose[k];
			if (l.made && !l.body.asleep && Crash::Length3(l.body.vel) > 200.0f && Crash::FootprintOf(l.body, &hitters[count])) {
				count++;
			}
		}
		Peds::Settings set = Peds::DefaultSettings();
		set.from = s_runOverFrom;
		int downed = 0;
		for (int i = 0; i < s_pedCount; i++) {
			Ped& p = s_peds[i];
			if (p.state == kPedThere && (count == 0 || downed >= kPedsAFrame)) {
				continue;
			}
			if (p.state == kPedGone) {
				// a moment after they went: made again at the start of their way
				if (now - p.since >= 250) {
					Script::LazyStruct* params = Script::LazyStruct::s_create();
					if (params) {
						params->AddChecksum(s_n.ped, p.name);
						RunGameScript(s_n.pedBackScript, params, 0, 0);
						Script::LazyStruct::s_free(params);
					}
					p.state = kPedThere;
					p.since = now;
					Note("People: %08X back at the start of their way\n", p.name);
				}
				continue;
			}
			uint8_t* obj = (uint8_t*)ResolveToObject(p.name);
			if (!obj) {
				// not there (yet, or any more: the level's own doing)
				p.state = kPedThere;
				continue;
			}
			if (p.state == kPedThere) {
				const float* at = (const float*)(obj + OBJECT_POSITION);
				if (!Finite(at, 3)) {
					continue;
				}
				for (int h = 0; h < count; h++) {
					const Crash::Body& car = hitters[h];
					const float dx = at[0] - car.centre[0], dz = at[2] - car.centre[1];
					const float reach = car.halfX + car.halfZ + Peds::kRadius;
					Peds::Strike st;
					if (dx * dx + dz * dz > reach * reach || !Peds::Struck(car, at, &st) || st.speed < s_runOverFrom) {
						continue;
					}
					const char* no = NotRunOver(obj);
					if (no) {
						if (!p.told) {
							Note("People: %08X cannot be run over (%s)\n", p.name, no);
							p.told = true;
						}
						break;
					}
					RunOver(p, obj, car, st, s, now);
					downed++;
					break;
				}
				continue;
			}
			if (p.state == kPedFlying) {
				if (dt <= 0.0f) {
					continue;
				}
				const float fall = -p.flight.vel[1];
				const Peds::Level level = PedLevel(p.flight, dt);
				if (Peds::Step(&p.flight, dt, level, set)) {
					Script::LazyStruct* params = Script::LazyStruct::s_create();
					if (params) {
						const float vol = 60.0f + 0.15f * (fall > 0.0f ? fall : 0.0f);
						params->AddFloat(s_n.vol, (vol > 150.0f ? 150.0f : vol) * Near(p.flight.pos, s.pos));
						RunGameScript(s_n.pedLandScript, params, obj, 0);
						Script::LazyStruct::s_free(params);
					}
				}
				PlacePed(obj, p.flight);
				if (p.flight.still) {
					p.state = kPedLying;
					p.since = now;
				}
				continue;
			}
			// lying there: back on their way once they have lain long enough and he is far away
			if (Peds::BackOnTheirWay((float)(now - p.since) / 1000.0f, (float)kLieMs / 1000.0f, p.flight.pos, p.start, s.pos, kPedFar)) {
				Script::LazyStruct* params = Script::LazyStruct::s_create();
				if (params) {
					params->AddChecksum(s_n.ped, p.name);
					RunGameScript(s_n.pedGoneScript, params, 0, 0);
					Script::LazyStruct::s_free(params);
				}
				p.state = kPedGone;
				p.since = now;
			}
		}
	}

	// A car he left that has stood long enough goes back, one at a time: it goes, and in the same frame the
	// car of the level it was comes back at the start of its way (unless that car is there again already, or
	// was a goal's), when he is far from both places
	static void GoBack(const DriveGame::Skater& s, uint32_t now) {
		for (int k = 0; k < Drive::Parking::kSlots; k++) {
			Car& p = s_cars[s_levelCars + k];
			const uint8_t* obj = CarObject(p);
			if (!obj) {
				continue;
			}
			const uint32_t origin = s_parking.Origin(k);
			const Car* o = origin ? CarNamed(origin) : 0;
			const bool appears = o && o->slot < 0 && o->atStart && !ResolveToObject(origin);
			float at[3];
			memcpy(at, obj + OBJECT_POSITION, sizeof(at));
			if (!Drive::GoesBack(now, s_parking.ParkedAt(k), s_standMs, at, s.pos, appears ? o->start : 0, kFar)) {
				continue;
			}
			Script::LazyStruct* params = Script::LazyStruct::s_create();
			if (!params) {
				return;
			}
			params->AddChecksum(s_n.parked, p.name);
			if (appears) {
				params->AddChecksum(s_n.car, origin);
			}
			const uint32_t stood = (now - s_parking.ParkedAt(k)) / 1000;
			KillFx(k);
			const bool ran = RunScriptOf(s_n.goesBackScript, params, 0);
			Script::LazyStruct::s_free(params);
			s_parking.Leave(k);
			ResetLoose(k);
			p.track.Reset();
			if (appears) {
				s_back.Remove(origin);
			}
			if (!ran) {
				Log::TypedLog(CHN_SDL, "Cars: could not run Flickit_CarGoesBack for the car he left at (%.0f %.0f %.0f)\n", at[0], at[1], at[2]);
			}
			else if (appears) {
				Log::TypedLog(CHN_SDL, "Cars: the car he left at (%.0f %.0f %.0f) %u s ago went back: %08X is at the start of its way again\n",
					at[0], at[1], at[2], stood, origin);
			}
			else {
				Log::TypedLog(CHN_SDL, "Cars: the car he left at (%.0f %.0f %.0f) %u s ago went (%s)\n", at[0], at[1], at[2], stood,
					!o ? "a car of no known origin" : o->atStart ? "the car it was is there again" : "it was a goal's car");
			}
			return;
		}
	}

	static void Steal(Car& c, const DriveGame::Skater& s) {
		Script::LazyStruct* p = Script::LazyStruct::s_create();
		if (!p) {
			return;
		}
		const char* model = c.slot >= 0 ? ParkedModel(c.slot) : c.model;
		p->AddChecksum(s_n.car, c.name);
		p->AddString(s_n.model, (char*)model);
		p->AddChecksum(s_n.setup, c.setup);
		p->AddChecksum(s_n.tune, c.tune);
		memset(&s_taken, 0, sizeof(s_taken));
		s_prompt = false;                    // the steal script takes the prompt away
		const bool ran = RunScriptOf(s_n.stealScript, p, s.object);
		Script::LazyStruct::s_free(p);
		Log::TypedLog(CHN_SDL, "Cars: %s %s (%08X, %s)%s\n", ran ? "took" : "could not run Flickit_CarSteal for",
			c.slot >= 0 ? "a car he had left" : "a car of the level", c.name, model,
			ran && !s.driving && !s_driven.set ? ", but he is not driving it" : "");
	}

	void GrabMade(bool made) {
		s_grabMade = made;
	}

	bool FlickitGrab() {
		return s_grabMade;
	}

	float CarAway() {
		return s_carAway;
	}

	void PadFrame(const DriveGame::Skater& s) {
		s_carAway = 1.0e9f;
		if (!s_on || !Ready() || !Parsed() || !s.object || !s.pos || !s.take) {
			return;
		}
		LevelKey key;
		key.loads = BigWorld::NodeLoads();
		key.nodes = GetGlobalArray(s_n.nodeArray, 0);
		key.count = key.nodes ? *(const uint32_t*)((const uint8_t*)key.nodes + 8) : 0;
		key.level = *kSkate ? *(const uint32_t*)(*kSkate + SKATE_LEVEL) : 0;
		if (key != s_level) {
			s_level = key;
			FindCars();
		}
		const float seconds = FrameSeconds();
		const bool down = *s.take != 0 && !s_grabMade;     // a grab Flickit made for a trigger takes no car
		const bool pressed = down && !s_down;
		s_down = down;
		if (!down) {
			s_released = true;
		}
		if (InNet()) {
			return;
		}
		const uint32_t now = GetTickCount();
		if (s_taken.set && !s.driving) {
			// taken on a frame before, and nothing to drive came of it (Flickit_CarPlace did not run): a car of
			// the level that went in the taking comes back
			s_taken.set = false;
			if (s_taken.slot < 0 && !ResolveToObject(s_taken.name)) {
				ComeBackLater(s_taken.origin, now, "it was taken, but there was no car to drive");
			}
		}
		if (!s.driving && s_driven.set && !s_leaving.set) {
			// out of the car some other way (a restart, the end of a goal): it went with PlayerVehicle
			s_driven.set = false;
			ComeBackLater(s_driven.origin, now, "the car he drove went with a restart");
			KillFx(kFxDriven);
		}

		// where each car is and how fast it goes, and the nearest in reach
		int best = -1;
		float bestAway = 1.0e9f;
		for (int i = 0; i < s_levelCars + Drive::Parking::kSlots; i++) {
			Car& c = s_cars[i];
			const uint8_t* obj = CarObject(c);
			if (!obj) {
				c.track.Reset();
				if (c.slot >= 0 && s_parking.Used(c.slot) && !ResolveToObject(c.name)) {
					// gone (a restart of the level's objects)
					ComeBackLater(s_parking.Origin(c.slot), now, "the car he left went with a restart");
					s_parking.Leave(c.slot);
					KillFx(c.slot);
					ResetLoose(c.slot);
				}
				continue;
			}
			const float* pos = (const float*)(obj + OBJECT_POSITION);
			const float* m = (const float*)(obj + OBJECT_MATRIX);
			c.track.Update(pos, seconds);
			if (c.slot < 0 && s_back.Count() > 0) {
				s_back.Remove(c.name);           // made again by the level itself
			}
			if (!s.free || s.driving || (c.slot >= 0 && !Takeable(c.slot))) {
				continue;
			}
			const Drive::Box* box = c.slot >= 0 ? s_parking.BoxOf(c.slot) : &c.box;
			float local[3], away = 0.0f;
			Drive::Local(pos, m, s.pos, local);
			if (box && Drive::InReach(*box, s_reach, local, &away) && away < bestAway) {
				best = i;
				bestAway = away;
			}
		}

		CrashFrame(s, now, seconds);
		PedsFrame(s, now, seconds);

		// another player nearer than the car has Circle (players_game.h)
		s_carAway = best >= 0 ? bestAway : 1.0e9f;
		if (best >= 0 && s.otherAway < bestAway) {
			best = -1;
		}

		// a car he left long enough ago back where it came from, once a second at most
		if (s_standMs && now - s_lastGoesBack > 1000) {
			s_lastGoesBack = now;
			GoBack(s, now);
		}

		// a car of the level that went for good back at the start of its way, once a second at most
		if (s_back.Count() > 0 && now - s_lastComeBack > 1000) {
			s_lastComeBack = now;
			const uint32_t name = s_back.Due(now, s.pos, 0, kFar);
			if (name) {
				Script::LazyStruct* p = Script::LazyStruct::s_create();
				if (p) {
					p->AddChecksum(s_n.car, name);
					const bool ran = RunScriptOf(s_n.backScript, p, 0);
					Script::LazyStruct::s_free(p);
					Log::TypedLog(CHN_SDL, "Cars: %s %08X at the start of its way\n", ran ? "put" : "could not run Flickit_CarBack to put", name);
				}
			}
		}

		Prompt(best >= 0 && s.free && !s.driving, now);

		if (pressed && s_released && best >= 0 && s.free && !s.driving) {
			s_released = false;
			*s.take = 0;                         // the game does not see the press that takes a car
			Steal(s_cars[best], s);
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  The script commands                                           */
	/*                                                                */
	/******************************************************************/

	static Script::LazyStruct* Returned(DummyScript* script) {
		return script ? script->GetParams : 0;
	}

	// Flickit_CarTake car = <car>: where it is, the way it faces and how fast it goes, now; speed back
	static bool CFunc_CarTake(Script::LazyStruct* params, DummyScript* script) {
		uint32_t name = 0;
		if (!params || !params->GetChecksum(s_n.car, &name, false)) {
			return false;
		}
		Car* c = CarNamed(name);
		const uint8_t* obj = c ? CarObject(*c) : 0;
		if (!obj) {
			return false;
		}
		const float* m = (const float*)(obj + OBJECT_MATRIX);
		memset(&s_taken, 0, sizeof(s_taken));
		s_taken.name = name;
		s_taken.origin = c->slot >= 0 ? s_parking.Origin(c->slot) : name;
		s_taken.slot = c->slot;
		strncpy_s(s_taken.model, sizeof(s_taken.model), c->slot >= 0 ? ParkedModel(c->slot) : c->model, _TRUNCATE);
		s_taken.box = c->slot >= 0 && s_parking.BoxOf(c->slot) ? *s_parking.BoxOf(c->slot) : c->box;
		memcpy(s_taken.pos, obj + OBJECT_POSITION, sizeof(s_taken.pos));
		s_taken.heading = Drive::Heading(m);
		s_taken.speed = c->track.Forward(m);
		s_taken.set = true;
		Script::LazyStruct* back = Returned(script);
		if (back) {
			back->AddFloat(s_n.speed, s_taken.speed);
		}
		return true;
	}

	// The body drawn on its wheels. CVehicleComponent::Finalize takes the place of the body from the model's
	// hierarchy as (-x, z, -y) (0x0045E516), and the drawing puts the body there turned from the model's axes
	// with x as it is (0x0045A6E9): x the other way round. The body of each of THUG's own drivable cars sits at
	// x = 0 and it did not show; the NY police car's has its place 34 units to the side, and was drawn twice
	// that far off its wheels. The place is only drawn with (the colliders are the setup's), so x is put right
	// here: -x from the centre of mass becomes x from it, 2x along. Returns that x.
	static float BodyOnItsWheels(const uint8_t* obj, void* vehicle) {
		const uint8_t* model = (const uint8_t*)GetComponent((void*)obj, s_n.model);
		void* data = model ? *(void* const*)(model + MODEL_DATA) : 0;
		const uint8_t* hierarchy = data ? ModelHierarchy(data) : 0;
		if (!hierarchy) {
			return 0.0f;
		}
		const float x = *(const float*)(hierarchy + HIERARCHY_BODY_X);
		if (!(x > -500.0f && x < 500.0f)) {
			return 0.0f;
		}
		float* place = (float*)((uint8_t*)vehicle + VEHICLE_BODY_PLACE);
		place[0] += 2.0f * x;
		return x;
	}

	// Flickit_CarPlace: PlayerVehicle where the car taken was, the way it faced, its body on its wheels;
	// camera_height and camera_distance back, for the camera to suit its size
	static bool CFunc_CarPlace(Script::LazyStruct*, DummyScript* script) {
		void* vehicle = 0;
		const uint8_t* obj = s_taken.set ? PlayerVehicle(&vehicle) : 0;
		if (!obj) {
			return false;
		}
		Script::LazyStruct* node = Script::LazyStruct::s_create();
		if (!node) {
			return false;
		}
		StructAddVector(node, s_n.pos, s_taken.pos[0], s_taken.pos[1], s_taken.pos[2]);
		StructAddVector(node, s_n.angles, 0.0f, s_taken.heading, 0.0f);
		MoveToNode(vehicle, node);
		Script::LazyStruct::s_free(node);
		const float bodyX = BodyOnItsWheels(obj, vehicle);
		const Drive::CameraPlace camera = Drive::CameraFor(s_taken.box);
		Script::LazyStruct* back = Returned(script);
		if (back) {
			back->AddFloat(s_n.cameraHeight, camera.height);
			back->AddFloat(s_n.cameraDistance, camera.distance);
		}
		if (s_taken.slot >= 0) {
			s_parking.Leave(s_taken.slot);
			s_drivenHealth = s_loose[s_taken.slot].health;
			KillFx(s_taken.slot);
			ResetLoose(s_taken.slot);
		}
		else {
			s_drivenHealth = Crash::Whole();
		}
		s_drivenHaveVel = false;
		s_driven.set = true;
		s_driven.origin = s_taken.origin;
		strncpy_s(s_driven.model, sizeof(s_driven.model), s_taken.model, _TRUNCATE);
		s_driven.box = s_taken.box;
		Log::TypedLog(CHN_SDL, "Cars: driving it from (%.1f %.1f %.1f), facing %.0f degrees, at %.0f units/s; its body's place %.1f to the side; the camera %.0f up, %.0f back\n",
			s_taken.pos[0], s_taken.pos[1], s_taken.pos[2], s_taken.heading * 57.29578f, s_taken.speed, bodyX, camera.height, camera.distance);
		s_taken.set = false;
		return true;
	}

	// Flickit_CarButtonUp: Triangle (the button that gets him out) let go, as the car reads it
	static bool CFunc_CarButtonUp(Script::LazyStruct*, DummyScript*) {
		void* vehicle = 0;
		if (!PlayerVehicle(&vehicle)) {
			return true;
		}
		const uint8_t* input = *(uint8_t* const*)((const uint8_t*)vehicle + VEHICLE_INPUT);
		return !input || input[INPUT_TRIANGLE] == 0;
	}

	// The height of the ground under (x, z) between top and bottom, by the skater's own line test (which only
	// says whether a line hits): halved down fourteen times, to about a sixtieth of a unit over 230
	static bool GroundAt(float x, float z, float top, float bottom, float* y) {
		const float from[3] = { x, top, z };
		const float to[3] = { x, bottom, z };
		if (!FlickitGame::LineBlocked(from, to)) {
			return false;
		}
		float clear = top, hits = bottom;        // the line down to clear hits nothing, down to hits it does
		for (int i = 0; i < 14; i++) {
			const float mid[3] = { x, 0.5f * (clear + hits), z };
			if (FlickitGame::LineBlocked(from, mid)) {
				hits = mid[1];
			}
			else {
				clear = mid[1];
			}
		}
		*y = hits;
		return true;
	}

	// Where the car is left: its model's origin on the ground under its middle and wheels, on the ground's
	// slope (the driven car's place is its body, up on its springs: a car parked there floats)
	static void ParkPlace(const void* vehicle, Leaving* l) {
		memcpy(l->parkPos, l->pos, sizeof(l->parkPos));
		memcpy(l->parkMatrix, l->matrix, sizeof(l->parkMatrix));
		float probes[9][3];
		bool found[9];
		int n = 0;
		probes[n][0] = 0.0f;
		probes[n][2] = 0.0f;
		n++;
		const uint8_t* v = (const uint8_t*)vehicle;
		const int count = *(const int*)(v + VEHICLE_WHEEL_COUNT);
		const uint8_t* wheels = *(const uint8_t* const*)(v + VEHICLE_WHEELS);
		if (wheels && count >= 2 && count <= 8) {
			for (int i = 0; i < count; i++) {
				const float* p = (const float*)(wheels + i * WHEEL_SIZE + WHEEL_PLACE);
				probes[n][0] = p[0];
				probes[n][2] = p[2];
				n++;
			}
		}
		else {
			const float side = l->box.halfWidth * 0.8f;
			const float corners[4][2] = { { side, l->box.front * 0.7f }, { -side, l->box.front * 0.7f }, { side, l->box.rear * 0.7f }, { -side, l->box.rear * 0.7f } };
			for (int i = 0; i < 4; i++) {
				probes[n][0] = corners[i][0];
				probes[n][2] = corners[i][1];
				n++;
			}
		}
		const float fx = sinf(l->heading), fz = cosf(l->heading);
		const float lx = fz, lz = -fx;
		int got = 0;
		for (int i = 0; i < n; i++) {
			const float x = l->pos[0] + lx * probes[i][0] + fx * probes[i][2];
			const float z = l->pos[2] + lz * probes[i][0] + fz * probes[i][2];
			found[i] = GroundAt(x, z, l->pos[1] + 30.0f, l->pos[1] - 200.0f, &probes[i][1]);
			got += found[i] ? 1 : 0;
		}
		if (!Drive::GroundUnder(l->parkPos, l->parkMatrix, l->heading, probes, found, n)) {
			Log::TypedLog(CHN_SDL, "Cars: no ground under the car; it is left where it stood\n");
			return;
		}
		Log::TypedLog(CHN_SDL, "Cars: left on the ground at %.1f (its body was at %.1f), %d of %d places under it found\n",
			l->parkPos[1], l->pos[1], got, n);
	}

	// The box of the car driven from its wheels, for a model with no box of its own
	static Drive::Box BoxFromWheels(const void* vehicle, const Drive::Box& fallback) {
		const uint8_t* v = (const uint8_t*)vehicle;
		const int count = *(const int*)(v + VEHICLE_WHEEL_COUNT);
		const uint8_t* wheels = *(const uint8_t* const*)(v + VEHICLE_WHEELS);
		if (!wheels || count < 2 || count > 8) {
			return fallback;
		}
		float halfWidth = 0.0f, front = -1.0e9f, rear = 1.0e9f;
		for (int i = 0; i < count; i++) {
			const float* p = (const float*)(wheels + i * WHEEL_SIZE + WHEEL_PLACE);
			halfWidth = fabsf(p[0]) > halfWidth ? fabsf(p[0]) : halfWidth;
			front = p[2] > front ? p[2] : front;
			rear = p[2] < rear ? p[2] : rear;
		}
		Drive::Box b = fallback;
		if (halfWidth > 5.0f && front - rear > 20.0f) {
			b.halfWidth = halfWidth + 8.0f;
			b.front = front + 40.0f;
			b.rear = rear - 45.0f;
		}
		return b;
	}

	// Flickit_CarLeave: what the car is and where it stands, the place beside it he gets out at, and the
	// name it is left under; parked, model, pos back
	static bool CFunc_CarLeave(Script::LazyStruct*, DummyScript* script) {
		void* vehicle = 0;
		const uint8_t* obj = PlayerVehicle(&vehicle);
		if (!obj || !s_driven.set) {
			return false;
		}
		memset(&s_leaving, 0, sizeof(s_leaving));
		s_leaving.origin = s_driven.origin;
		memcpy(s_leaving.pos, obj + OBJECT_POSITION, sizeof(s_leaving.pos));
		memcpy(s_leaving.matrix, obj + OBJECT_MATRIX, sizeof(s_leaving.matrix));
		s_leaving.heading = Drive::Heading(s_leaving.matrix);
		strncpy_s(s_leaving.model, sizeof(s_leaving.model), s_driven.model, _TRUNCATE);
		s_leaving.box = s_driven.box;
		const Drive::Box wheels = BoxFromWheels(vehicle, s_driven.box);
		ParkPlace(vehicle, &s_leaving);

		// beside the driver's door if there is room (nothing between the seat and there, and ground under
		// it), else the other side, behind, in front; on the roof if there is no room at all
		float doors[4][3];
		Drive::Doors(s_leaving.pos, s_leaving.heading, s_leaving.box, 26.0f, doors);
		const float seat[3] = { s_leaving.pos[0], s_leaving.pos[1] + 36.0f, s_leaving.pos[2] };
		int picked = -1;
		for (int i = 0; i < 4 && picked < 0; i++) {
			const float at[3] = { doors[i][0], doors[i][1] + 36.0f, doors[i][2] };
			const float below[3] = { doors[i][0], doors[i][1] - 80.0f, doors[i][2] };
			if (!FlickitGame::LineBlocked(seat, at) && FlickitGame::LineBlocked(at, below)) {
				picked = i;
			}
		}
		if (picked >= 0) {
			memcpy(s_leaving.door, doors[picked], sizeof(s_leaving.door));
		}
		else {
			s_leaving.door[0] = s_leaving.pos[0];
			s_leaving.door[1] = s_leaving.pos[1] + wheels.height + 10.0f;
			s_leaving.door[2] = s_leaving.pos[2];
		}
		bool inUse[Drive::Parking::kSlots], away[Drive::Parking::kSlots];
		for (int k = 0; k < Drive::Parking::kSlots; k++) {
			const uint8_t* there = (const uint8_t*)ResolveToObject(s_n.parkedCar[k]);
			inUse[k] = there != 0;
			away[k] = there && Drive::FarApart((const float*)(there + OBJECT_POSITION), s_leaving.pos, kFar);
		}
		s_leaving.slot = s_parking.Pick(inUse, away);
		if (inUse[s_leaving.slot]) {
			// the one left longest ago goes (Flickit_CarExit), to make room
			ComeBackLater(s_parking.Origin(s_leaving.slot), GetTickCount(), "the car he left made room for another");
			KillFx(s_leaving.slot);
		}
		s_parking.Leave(s_leaving.slot);
		ResetLoose(s_leaving.slot);
		s_leaving.set = true;
		Script::LazyStruct* back = Returned(script);
		if (back) {
			back->AddChecksum(s_n.parked, s_n.parkedCar[s_leaving.slot]);
			back->AddString(s_n.model, s_leaving.model);
			StructAddVector(back, s_n.pos, s_leaving.parkPos[0], s_leaving.parkPos[1], s_leaving.parkPos[2]);
		}
		static const char* const kWhere[4] = { "by the driver's door", "on the other side", "behind it", "in front of it" };
		Log::TypedLog(CHN_SDL, "Cars: getting out at (%.1f %.1f %.1f), %s\n", s_leaving.door[0], s_leaving.door[1], s_leaving.door[2],
			picked >= 0 ? kWhere[picked] : "on the roof (no room around it)");
		return true;
	}

	// Flickit_CarOut: him at the place picked, facing the car's way, through the game's custom restart
	static bool CFunc_CarOut(Script::LazyStruct*, DummyScript*) {
		if (!s_leaving.set) {
			return false;
		}
		const bool there = FlickitGame::PutSkater(s_leaving.door, s_leaving.heading);
		if (!there) {
			Log::TypedLog(CHN_SDL, "Cars: the restart did not put him beside the car\n");
		}
		return there;
	}

	// Flickit_CarParked: the car left on the ground under it (ParkPlace), on the list of cars to take
	static bool CFunc_CarParked(Script::LazyStruct*, DummyScript*) {
		if (!s_leaving.set) {
			return false;
		}
		s_leaving.set = false;
		s_driven.set = false;
		uint8_t* obj = (uint8_t*)ResolveToObject(s_n.parkedCar[s_leaving.slot]);
		if (!obj || !GetComponent(obj, s_n.staticVehicle)) {
			Log::TypedLog(CHN_SDL, "Cars: the car he got out of was not made again\n");
			ComeBackLater(s_leaving.origin, GetTickCount(), "the car he got out of was not made again");
			return false;
		}
		memcpy(obj + OBJECT_POSITION, s_leaving.parkPos, sizeof(s_leaving.parkPos));
		memcpy(obj + OBJECT_MATRIX, s_leaving.parkMatrix, sizeof(s_leaving.parkMatrix));
		memcpy(obj + OBJECT_DISPLAY_MATRIX, s_leaving.parkMatrix, sizeof(s_leaving.parkMatrix));
		s_parking.Park(s_leaving.slot, s_leaving.model, s_leaving.box, s_leaving.origin, GetTickCount());
		Car* c = CarNamed(s_n.parkedCar[s_leaving.slot]);
		if (c) {
			c->track.Reset();
			TuneFor(s_leaving.model, c);
		}
		// as damaged as his car was (and burning on, if it was): the effects move to it
		ResetLoose(s_leaving.slot);
		s_loose[s_leaving.slot].health = s_drivenHealth;
		KillFx(kFxDriven);
		s_drivenHealth = Crash::Whole();
		s_lastParked = s_leaving.slot;
		if (s_boomPending) {
			s_boomSlot = s_leaving.slot;
		}
		return true;
	}

	/******************************************************************/
	/*                                                                */
	/*  A trigger face with no node                                   */
	/*                                                                */
	/******************************************************************/

	// 0x00456840 runs the TriggerScript of a trigger face something touched (0x004569E0 calls it for the faces
	// a feeler of a car or of the skater meets, and the skater's physics call it as well): this a component of
	// what touched it, then the kind of touch, the name of the face's node, the nodes of the object the face is
	// on (its NodeArray component's, or none for the level's faces), and that object. It finds the node in
	// those nodes, or in the level's (0x00530AE0, -1 for none), and reads its TriggerScript without a look at
	// what it found: a node not in the level's nodes is CArray::GetStructure(-1), and an object whose nodes
	// are an empty list leaves no node at all. Either reads through nothing in CStruct::GetChecksum: the crash
	// at 0x004110B3 (a car into the barricades on Jersey's highway). An object's own nodes that do not name the
	// face run the TriggerScript of their first node instead.
	//
	// The function now starts in TriggerTouched, which finds the node the way the game does and runs the game's
	// own function only when there is one; else it writes down once which node was missing. The game's first
	// three instructions go to a piece of code of the DLL's that goes on in the game's function after them.
	typedef void(__thiscall* TriggerTouched_t)(void* self, uint32_t touch, uint32_t node, void* nodes, void* object);
	typedef int32_t(__cdecl* FindNamedNode_t)(uint32_t name, int assertIfMissing);
	static const FindNamedNode_t FindNamedNode = (FindNamedNode_t)0x00530AE0;   // (BigWorld's when BigNodeNames is on)
	static const uint32_t kTriggerTouched = 0x00456840;
	static const uint8_t kTriggerStart[] = { 0x83, 0xEC, 0x08, 0x53, 0x8B, 0x5C, 0x24, 0x18, 0x56, 0x57, 0x33, 0xFF };   // sub esp, 8 / push ebx / mov ebx, [esp+18h] / ...
	static const uint8_t kTriggerEnd[] = { 0xC2, 0x10, 0x00 };          // its ret 10h at 0x0045696A
	static const uint8_t kTriggerRead[] = { 0x53, 0x8D, 0x44, 0x24, 0x20, 0x50, 0x68, 0x99, 0xA2, 0xA8, 0x2C, 0x8B, 0xCF };   // GetChecksum(TriggerScript) on the node it found
	enum { kTriggerCopied = 8 };                                        // the three instructions moved: 8 bytes
	static uint8_t* s_triggerGame = 0;                                  // the moved instructions, then a jump back
	static bool s_triggerHooked = false;
	static uint32_t s_triggerSaid[32];
	static int s_triggerSaidCount = 0;

	static const char* ObjectNodesName(const void* nodes, uint32_t node, int32_t* found) {
		const uint32_t count = *(const uint32_t*)((const uint8_t*)nodes + 8);
		*found = -1;
		if (count == 0) {
			return "the object's own nodes are an empty list";
		}
		for (uint32_t i = 0; i < count; i++) {
			void* n = ArrayStructure((void*)nodes, i);
			uint32_t name = 0;
			if (n && StructChecksum(n, s_n.name, &name, 0) && name == node) {
				*found = (int32_t)i;
				return 0;
			}
		}
		return "the object's own nodes have no node of that name";
	}

	static void TriggerMissing(uint32_t node, const void* object, const char* why) {
		for (int i = 0; i < s_triggerSaidCount; i++) {
			if (s_triggerSaid[i] == node) {
				return;
			}
		}
		if (s_triggerSaidCount < (int)(sizeof(s_triggerSaid) / sizeof(s_triggerSaid[0]))) {
			s_triggerSaid[s_triggerSaidCount++] = node;
		}
		char text[256];
		if (object) {
			const float* p = (const float*)((const uint8_t*)object + OBJECT_POSITION);
			sprintf_s(text, sizeof(text), "Triggers: a trigger face of node %08X was touched, on object %08X at (%.0f %.0f %.0f): %s; nothing was run (the game read through nothing here: the crash at 0x004110B3)",
				node, *(const uint32_t*)((const uint8_t*)object + 0x14), p[0], p[1], p[2], why);
		}
		else {
			sprintf_s(text, sizeof(text), "Triggers: a trigger face of node %08X was touched, in the level's collision: %s; nothing was run (the game read through nothing here: the crash at 0x004110B3)",
				node, why);
		}
		Log::TypedLog(CHN_SDL, "%s\n", text);
		FlickitGame::WatchNote(text);
	}

	static void __fastcall TriggerTouched(void* self, void* /*edx*/, uint32_t touch, uint32_t node, void* nodes, void* object) {
		if (s_n.nodeArray == 0) {
			InitNames();
		}
		const char* why = 0;
		if (nodes) {
			int32_t found = -1;
			why = ObjectNodesName(nodes, node, &found);
		}
		else {
			const int32_t index = FindNamedNode(node, 0);
			void* level = GetGlobalArray(s_n.nodeArray, 0);
			const uint32_t count = level ? *(const uint32_t*)((const uint8_t*)level + 8) : 0;
			if (index < 0 || (uint32_t)index >= count || !ArrayStructure(level, (uint32_t)index)) {
				why = "the level has no node of that name";
			}
		}
		if (why) {
			TriggerMissing(node, object, why);
			return;
		}
		// the object's own node is looked up in the level's as well, for the script's parameters: an object
		// that is not a node of the level (a car he left, his car) is left out of that, as the game would
		// have it if it had looked
		if (object) {
			const int32_t index = FindNamedNode(*(const uint32_t*)((const uint8_t*)object + 0x14), 0);
			void* level = GetGlobalArray(s_n.nodeArray, 0);
			const uint32_t count = level ? *(const uint32_t*)((const uint8_t*)level + 8) : 0;
			if (index < 0 || (uint32_t)index >= count) {
				object = 0;
			}
		}
		((TriggerTouched_t)s_triggerGame)(self, touch, node, nodes, object);
	}

	static void HookTriggers() {
		if (s_triggerHooked) {
			return;
		}
		if (memcmp((const void*)(uintptr_t)kTriggerTouched, kTriggerStart, sizeof(kTriggerStart)) != 0
			|| memcmp((const void*)(uintptr_t)0x0045696A, kTriggerEnd, sizeof(kTriggerEnd)) != 0
			|| memcmp((const void*)(uintptr_t)0x004568FE, kTriggerRead, sizeof(kTriggerRead)) != 0) {
			Log::TypedLog(CHN_SDL, "Triggers: the game's code for a touched trigger face is not what was expected; left as it is\n");
			return;
		}
		uint8_t* code = (uint8_t*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (!code) {
			Log::TypedLog(CHN_SDL, "Triggers: no memory for the hook of a touched trigger face; left as it is\n");
			return;
		}
		memcpy(code, (const void*)(uintptr_t)kTriggerTouched, kTriggerCopied);
		code[kTriggerCopied] = 0xE9;
		*(uint32_t*)(code + kTriggerCopied + 1) = (kTriggerTouched + kTriggerCopied) - ((uint32_t)(uintptr_t)code + kTriggerCopied + 5);
		FlushInstructionCache(GetCurrentProcess(), code, 32);
		s_triggerGame = code;
		patchJump((void*)(uintptr_t)kTriggerTouched, (void*)&TriggerTouched);
		FlushInstructionCache(GetCurrentProcess(), (void*)(uintptr_t)kTriggerTouched, 8);
		s_triggerHooked = true;
		Log::TypedLog(CHN_SDL, "Triggers: a trigger face whose node is missing is left alone (it crashed the game at 0x004110B3)\n");
	}

	/******************************************************************/
	/*                                                                */
	/*  Settings                                                      */
	/*                                                                */
	/******************************************************************/

	void AtScriptsLoaded() {
		if (s_on && Ready()) {
			Parsed();
		}
	}

	void AddCFuncs() {
		if (s_cfuncs) {
			return;
		}
		CFuncs::AddFunction("Flickit_CarTake", (void*)CFunc_CarTake);
		CFuncs::AddFunction("Flickit_CarPlace", (void*)CFunc_CarPlace);
		CFuncs::AddFunction("Flickit_CarButtonUp", (void*)CFunc_CarButtonUp);
		CFuncs::AddFunction("Flickit_CarLeave", (void*)CFunc_CarLeave);
		CFuncs::AddFunction("Flickit_CarOut", (void*)CFunc_CarOut);
		CFuncs::AddFunction("Flickit_CarParked", (void*)CFunc_CarParked);
		s_cfuncs = true;
	}

	void Configure(const char* configFile) {
		// (for cars or not: the skater touches trigger faces too)
		if (FlickitGame::Installed()) {
			HookTriggers();
		}
		s_on = GetPrivateProfileIntA(FLICKIT_SECTION, "Cars", 1, configFile) != 0;
		int reach = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "CarReach", 45, configFile);
		reach = reach < 10 ? 10 : (reach > 200 ? 200 : reach);
		s_reach = (float)reach;
		int stand = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "CarParkedTime", 120, configFile);
		stand = stand <= 0 ? 0 : (stand < 10 ? 10 : (stand > 3600 ? 3600 : stand));
		s_standMs = (uint32_t)stand * 1000u;
		s_crashes = GetPrivateProfileIntA(FLICKIT_SECTION, "CarCrashes", 1, configFile) != 0;
		int damage = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "CarDamage", 100, configFile);
		damage = damage < 0 ? 0 : (damage > 500 ? 500 : damage);
		s_damageScale = (float)damage / 100.0f;
		int launch = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "CarLaunch", 100, configFile);
		launch = launch < 0 ? 0 : (launch > 300 ? 300 : launch);
		s_launchScale = (float)launch / 100.0f;
		s_runOver = GetPrivateProfileIntA(FLICKIT_SECTION, "RunOver", 1, configFile) != 0;
		int from = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "RunOverSpeed", 60, configFile);
		from = from < 10 ? 10 : (from > 2000 ? 2000 : from);
		s_runOverFrom = (float)from;
		if (!s_crashes) {
			Log::TypedLog(CHN_SDL, "Crashes: off (CarCrashes=0): the cars go through each other\n");
		}
		if (!s_on) {
			Log::TypedLog(CHN_SDL, "Cars: off\n");
		}
		else if (!FlickitGame::Installed()) {
			Log::TypedLog(CHN_SDL, "Cars: off (they need Flickit's hooks in the game: GameHooks=1)\n");
		}
	}
}
