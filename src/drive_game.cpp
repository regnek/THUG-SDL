#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "drive.h"
#include "drive_game.h"
#include "flickit_cars_qb.h"
#include "bigworld.h"
#include "config.h"
#include "flickit.h"
#include "flickit_game.h"
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
	// A symbol by its name: its type at +1 (7 a script, 10 a structure), its value at +0xC
	typedef uint8_t* (__cdecl* LookUpSymbol_t)(uint32_t name);
	static const LookUpSymbol_t LookUpSymbol = (LookUpSymbol_t)0x004133A0;
	// CVehicleComponent::MoveToNode, what Vehicle_MoveToRestart does with a node: the car on the ground
	// below the node's Pos, turned to its Angles, its wheels settled, still
	typedef void(__thiscall* MoveToNode_t)(void* vehicle, void* node);
	static const MoveToNode_t MoveToNode = (MoveToNode_t)0x0045BEC0;
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

	// An object: where it is, its matrix (left, up, the way it faces, a fourth row), the matrix it is drawn with
	enum {
		OBJECT_POSITION = 0x40,
		OBJECT_MATRIX = 0x60,
		OBJECT_DISPLAY_MATRIX = 0xB0,
	};
	// CVehicleComponent (0x1334 bytes, made at 0x0045B2E0): its input component (its Finalize, 0x0045E474),
	// its wheels (0x1C8 bytes each, the place of each at +0xA0, from the model's bones: 0x0045E4E1)
	enum {
		VEHICLE_INPUT = 0x1314,
		VEHICLE_WHEEL_COUNT = 0x190,
		VEHICLE_WHEELS = 0x2D0,
		WHEEL_SIZE = 0x1C8,
		WHEEL_PLACE = 0xA0,
		INPUT_TRIANGLE = 0x1A0,      // the game's picture of the pad starts at +0x20; Triangle at +0x180 of it
	};
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
		{ 0x004133A0, 12, { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0xC1, 0x25, 0xFF, 0x0F, 0x00, 0x00, 0x56 }, "Script::LookUpSymbol" },
		{ 0x0045BEC0, 12, { 0x6A, 0xFF, 0x68, 0x5B, 0x35, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "CVehicleComponent::MoveToNode" },
		{ 0x0045E467, 14, { 0x8B, 0x4E, 0x0C, 0x68, 0x28, 0xCD, 0xD7, 0x27, 0xE8, 0x4C, 0xD9, 0xFB, 0xFF, 0x8B }, "CVehicleComponent::Finalize: its input component" },
		{ 0x0045E4B4, 16, { 0xD9, 0x41, 0xA8, 0x8B, 0x86, 0xD0, 0x02, 0x00, 0x00, 0xD9, 0x5C, 0x24, 0x50, 0x83, 0xE9, 0x50 }, "CVehicleComponent::Finalize: its wheels" },
		{ 0x0045A3D0, 16, { 0x51, 0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x8E, 0x14, 0x13, 0x00, 0x00, 0xD9, 0x81, 0x38, 0x02, 0x00 }, "CVehicleComponent: its controls" },
		{ 0x0045EB49, 7, { 0x3D, 0xA6, 0x13, 0xB7, 0x93, 0x74, 0x28 }, "CVehicleComponent: Vehicle_Kick" },
		{ 0x0044A3E0, 14, { 0x8B, 0x41, 0x0C, 0x8D, 0x48, 0x60, 0x51, 0x8D, 0x88, 0xB0, 0x00, 0x00, 0x00, 0xE8 }, "CSetDisplayMatrixComponent::Update" },
		{ 0x00409F90, 13, { 0x8B, 0x44, 0x24, 0x0C, 0x8B, 0x4C, 0x24, 0x08, 0x8B, 0x54, 0x24, 0x04, 0x6A }, "Script::ParseQB, from memory" },
		{ 0x0040FD00, 12, { 0x6A, 0xFF, 0x68, 0x5B, 0x18, 0x62, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00 }, "Script::RunScript" },
		{ 0x004C6B50, 12, { 0x56, 0x8B, 0xF1, 0xF6, 0x86, 0x38, 0x01, 0x00, 0x00, 0x0C, 0x74, 0x24 }, "GameNet::Manager::InNetGame" },
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
		uint32_t stealScript, backScript, defaultTune;
		uint32_t parkedCar[Drive::Parking::kSlots];
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
	static const uint32_t kComeBackMs = 15000;     // a car of the traffic is away at least this long
	static const float kComeBackFar = 2500.0f;     // and comes back only when he is this far from its start

	struct Car {
		uint32_t name;
		char model[kModelSize];
		uint32_t tune;           // its Flickit_Car_ structure
		uint32_t setup;          // the setup of the game's drivable car it gets
		Drive::Box box;
		float start[4];          // its node's place, where it comes back
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
		char model[kModelSize];
		Drive::Box box;
	};
	static Driven s_driven;
	struct Leaving {
		bool set;
		int slot;
		char model[kModelSize];
		Drive::Box box;
		float pos[4];
		float matrix[16];
		float door[3];
		float heading;
	};
	static Leaving s_leaving;

	static bool s_down = false;              // Triangle on the frame before
	static bool s_released = true;           // let go since a car was taken
	static LARGE_INTEGER s_lastFrame;        // zero: no frame yet
	static uint32_t s_lastComeBack = 0;

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
		s_n.defaultTune = Flickit::NameChecksum("Flickit_Car_Default");
		for (int i = 0; i < Drive::Parking::kSlots; i++) {
			char name[32];
			sprintf_s(name, sizeof(name), "Flickit_ParkedCar%d", i);
			s_n.parkedCar[i] = Flickit::NameChecksum(name);
		}
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
				Log::TypedLog(CHN_SDL, "Cars: on; Triangle near a car takes it (reach %.0f), the game has Flickit's car scripts (%u bytes)\n",
					s_reach, (unsigned)sizeof(data));
			}
			else {
				s_parsed = -1;
				Log::TypedLog(CHN_SDL, "Cars: the game did not take Flickit's car scripts; the cars are off\n");
			}
		}
		return s_parsed > 0;
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

	// The level's cars: its Vehicle nodes whose models have the car's skeleton (the one the game's
	// drivable car has), and a place for each parked car after them
	static void FindCars() {
		s_levelCars = 0;
		s_parking.Clear();
		s_back.Clear();
		memset(&s_taken, 0, sizeof(s_taken));
		memset(&s_driven, 0, sizeof(s_driven));
		memset(&s_leaving, 0, sizeof(s_leaving));
		void* nodes = GetGlobalArray(s_n.nodeArray, 0);
		const uint32_t count = nodes ? *(const uint32_t*)((const uint8_t*)nodes + 8) : 0;
		int skipped = 0;
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
			TuneFor(c.model, &c);
		}
		for (int k = 0; k < Drive::Parking::kSlots; k++) {
			Car& c = s_cars[s_levelCars + k];
			memset(&c, 0, sizeof(c));
			c.name = s_n.parkedCar[k];
			c.slot = k;
		}
		Log::TypedLog(CHN_SDL, "Cars: %d cars in this level to take%s\n", s_levelCars,
			skipped ? " (and some vehicles that are not cars: buses, trams, boats)" : "");
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
		const bool ran = RunGameScript(s_n.stealScript, p, s.object, 0);
		Script::LazyStruct::s_free(p);
		Log::TypedLog(CHN_SDL, "Cars: %s %s (%08X, %s)%s\n", ran ? "took" : "could not run Flickit_CarSteal for",
			c.slot >= 0 ? "a car he had left" : "a car of the level", c.name, model,
			ran && !s.driving && !s_driven.set ? ", but he is not driving it" : "");
	}

	void PadFrame(const DriveGame::Skater& s) {
		if (!s_on || !Ready() || !Parsed() || !s.object || !s.pos || !s.triangle) {
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
		const bool down = *s.triangle != 0;
		const bool pressed = down && !s_down;
		s_down = down;
		if (!down) {
			s_released = true;
		}
		if (InNet()) {
			return;
		}
		const uint32_t now = GetTickCount();
		if (!s.driving && s_driven.set) {
			// out of the car some other way (a restart, the end of a goal): it went with PlayerVehicle
			s_driven.set = false;
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
					s_parking.Leave(c.slot);     // gone (a restart of the level's objects)
				}
				continue;
			}
			const float* pos = (const float*)(obj + OBJECT_POSITION);
			const float* m = (const float*)(obj + OBJECT_MATRIX);
			c.track.Update(pos, seconds);
			if (c.slot < 0 && s_back.Count() > 0) {
				s_back.Remove(c.name);           // made again by the level itself
			}
			if (!s.free || s.driving) {
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

		// a car of the traffic back at the start of its way, once a second at most
		if (s_back.Count() > 0 && now - s_lastComeBack > 1000) {
			s_lastComeBack = now;
			const uint32_t name = s_back.Due(now, s.pos, kComeBackMs, kComeBackFar);
			if (name) {
				Script::LazyStruct* p = Script::LazyStruct::s_create();
				if (p) {
					p->AddChecksum(s_n.car, name);
					const bool ran = RunGameScript(s_n.backScript, p, 0, 0);
					Script::LazyStruct::s_free(p);
					Log::TypedLog(CHN_SDL, "Cars: %s %08X at the start of its way\n", ran ? "put" : "could not run Flickit_CarBack to put", name);
				}
			}
		}

		if (pressed && s_released && best >= 0 && s.free && !s.driving) {
			s_released = false;
			*s.triangle = 0;                     // the game does not see the press that takes a car
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

	// Flickit_CarComesBack car = <car>: a car of the traffic, to be put back later
	static bool CFunc_CarComesBack(Script::LazyStruct* params, DummyScript*) {
		uint32_t name = 0;
		Car* c = params && params->GetChecksum(s_n.car, &name, false) ? CarNamed(name) : 0;
		return c && c->slot < 0 && s_back.Add(name, c->start, GetTickCount());
	}

	// Flickit_CarPlace: PlayerVehicle where the car taken was, the way it faced
	static bool CFunc_CarPlace(Script::LazyStruct*, DummyScript*) {
		void* vehicle = 0;
		if (!s_taken.set || !PlayerVehicle(&vehicle)) {
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
		if (s_taken.slot >= 0) {
			s_parking.Leave(s_taken.slot);
		}
		s_driven.set = true;
		strncpy_s(s_driven.model, sizeof(s_driven.model), s_taken.model, _TRUNCATE);
		s_driven.box = s_taken.box;
		Log::TypedLog(CHN_SDL, "Cars: driving it from (%.1f %.1f %.1f), facing %.0f degrees, at %.0f units/s\n",
			s_taken.pos[0], s_taken.pos[1], s_taken.pos[2], s_taken.heading * 57.29578f, s_taken.speed);
		s_taken.set = false;
		return true;
	}

	// Flickit_CarButtonUp: Triangle let go, as the car reads it
	static bool CFunc_CarButtonUp(Script::LazyStruct*, DummyScript*) {
		void* vehicle = 0;
		if (!PlayerVehicle(&vehicle)) {
			return true;
		}
		const uint8_t* input = *(uint8_t* const*)((const uint8_t*)vehicle + VEHICLE_INPUT);
		return !input || input[INPUT_TRIANGLE] == 0;
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
		memcpy(s_leaving.pos, obj + OBJECT_POSITION, sizeof(s_leaving.pos));
		memcpy(s_leaving.matrix, obj + OBJECT_MATRIX, sizeof(s_leaving.matrix));
		s_leaving.heading = Drive::Heading(s_leaving.matrix);
		strncpy_s(s_leaving.model, sizeof(s_leaving.model), s_driven.model, _TRUNCATE);
		s_leaving.box = s_driven.box;
		const Drive::Box wheels = BoxFromWheels(vehicle, s_driven.box);

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
		bool inUse[Drive::Parking::kSlots];
		for (int k = 0; k < Drive::Parking::kSlots; k++) {
			inUse[k] = ResolveToObject(s_n.parkedCar[k]) != 0;
		}
		s_leaving.slot = s_parking.Pick(inUse);
		s_parking.Leave(s_leaving.slot);
		s_leaving.set = true;
		Script::LazyStruct* back = Returned(script);
		if (back) {
			back->AddChecksum(s_n.parked, s_n.parkedCar[s_leaving.slot]);
			back->AddString(s_n.model, s_leaving.model);
			StructAddVector(back, s_n.pos, s_leaving.pos[0], s_leaving.pos[1], s_leaving.pos[2]);
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

	// Flickit_CarParked: the car left, as it stood (its matrix), on the list of cars to take
	static bool CFunc_CarParked(Script::LazyStruct*, DummyScript*) {
		if (!s_leaving.set) {
			return false;
		}
		s_leaving.set = false;
		s_driven.set = false;
		uint8_t* obj = (uint8_t*)ResolveToObject(s_n.parkedCar[s_leaving.slot]);
		if (!obj || !GetComponent(obj, s_n.staticVehicle)) {
			Log::TypedLog(CHN_SDL, "Cars: the car he got out of was not made again\n");
			return false;
		}
		memcpy(obj + OBJECT_POSITION, s_leaving.pos, sizeof(s_leaving.pos));
		memcpy(obj + OBJECT_MATRIX, s_leaving.matrix, sizeof(s_leaving.matrix));
		memcpy(obj + OBJECT_DISPLAY_MATRIX, s_leaving.matrix, sizeof(s_leaving.matrix));
		s_parking.Park(s_leaving.slot, s_leaving.model, s_leaving.box);
		Car* c = CarNamed(s_n.parkedCar[s_leaving.slot]);
		if (c) {
			c->track.Reset();
			TuneFor(s_leaving.model, c);
		}
		return true;
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
		CFuncs::AddFunction("Flickit_CarComesBack", (void*)CFunc_CarComesBack);
		CFuncs::AddFunction("Flickit_CarPlace", (void*)CFunc_CarPlace);
		CFuncs::AddFunction("Flickit_CarButtonUp", (void*)CFunc_CarButtonUp);
		CFuncs::AddFunction("Flickit_CarLeave", (void*)CFunc_CarLeave);
		CFuncs::AddFunction("Flickit_CarOut", (void*)CFunc_CarOut);
		CFuncs::AddFunction("Flickit_CarParked", (void*)CFunc_CarParked);
		s_cfuncs = true;
	}

	void Configure(const char* configFile) {
		s_on = GetPrivateProfileIntA(FLICKIT_SECTION, "Cars", 1, configFile) != 0;
		int reach = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "CarReach", 45, configFile);
		reach = reach < 10 ? 10 : (reach > 200 ? 200 : reach);
		s_reach = (float)reach;
		if (!s_on) {
			Log::TypedLog(CHN_SDL, "Cars: off\n");
		}
		else if (!FlickitGame::Installed()) {
			Log::TypedLog(CHN_SDL, "Cars: off (they need Flickit's hooks in the game: GameHooks=1)\n");
		}
	}
}
