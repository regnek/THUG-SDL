#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "bigworld.h"
#include "facelist.h"
#include "game.h"
#include "nodenames.h"
#include "patch.h"
#include "Logger/Logger.h"

#define MEMORY_SECTION "Memory"

namespace BigWorld {
	/******************************************************************/
	/*                                                                */
	/*  Node names                                                    */
	/*                                                                */
	/******************************************************************/

	// The game's two functions for its table (nothing else touches the table at 0x007C08A0):
	// 0x00530F50 fills it (called by LoadNodeArray at 0x0052CB9F, and at 0x0054F440), no arguments,
	// plain ret; 0x00530AE0 looks a name up (25 callers), one argument (the name), answers the node's
	// number or -1 in eax, plain ret, the caller pops. Both keep ecx (push ecx / pop ecx) and every
	// register the compiler keeps. A jump to ours goes over the first five bytes of each.
	static const uint32_t kFillAt = 0x00530F50;
	static const uint32_t kFindAt = 0x00530AE0;
	static const uint8_t kFillBytes[] = { 0x51, 0x53, 0x56, 0x57, 0x83, 0xCB, 0xFF, 0x8B, 0xC3 };   // push ecx/ebx/esi/edi, or ebx, -1, mov eax, ebx
	static const uint8_t kFindBytes[] = { 0x51, 0x57, 0x6A, 0x00, 0x68, 0xC5, 0xEC, 0x72, 0xC4 };   // push ecx/edi, push 0, push "NodeArray"

	static const uint32_t kName = 0xA1DC81F9;        // "Name"

	// The game's own functions, as its two use them (game.h): the level's nodes, a global array of structures
	// ("NodeArray"); a structure in an array; a checksum in a structure. Game::Ready is not asked here: the
	// two functions that are replaced are compared with what is expected below, and they call these three.
	using Game::kNodeArray;
	using Game::GetGlobalArray;
	using Game::ArrayStructure;
	using Game::StructChecksum;

	static NodeNames::Table s_names;

	static uint32_t NodeCount(void* nodes) {
		return *(const uint32_t*)((const uint8_t*)nodes + 8);   // CArray: the size at +8, as the game's loop reads it
	}

	static bool NameOfNode(void* nodes, uint32_t index, uint32_t* name) {
		void* node = ArrayStructure(nodes, index);
		*name = 0;
		return node && StructChecksum(node, kName, name, 0);
	}

	static uint32_t s_loads = 0;        // level loads (the table is made once for each)

	uint32_t NodeLoads() {
		return s_loads;
	}

	static void __cdecl FillNodeNames() {
		s_loads++;
		void* nodes = GetGlobalArray(kNodeArray, 0);
		const uint32_t count = nodes ? NodeCount(nodes) : 0;
		if (!s_names.Build(count, NameOfNode, nodes)) {
			Log::TypedLog(CHN_DLL, "BigWorld: no room for a table of %u node names; nodes cannot be found by name\n", count);
			return;
		}
		Log::TypedLog(CHN_DLL, "BigWorld: %u of the level's %u nodes have names, in a table of %u (the game's has 16384)\n",
			s_names.Named(), count, s_names.Size());
	}

	static int32_t __cdecl FindNodeByName(uint32_t name) {
		void* nodes = GetGlobalArray(kNodeArray, 0);
		if (!nodes) {
			return -1;
		}
		return s_names.Find(name, NodeCount(nodes), NameOfNode, nodes);
	}

	// In the place of 0x00530F50. Every register but eax comes back as it went in.
	static __declspec(naked) void FillHook() {
		__asm {
			push ecx
			push edx
			call FillNodeNames
			pop edx
			pop ecx
			ret
		}
	}

	// In the place of 0x00530AE0: the name is the caller's argument, at [esp+4] on the way in, which
	// the caller pops. Every register but eax comes back as it went in.
	static __declspec(naked) void FindHook() {
		__asm {
			push ecx
			push edx
			push dword ptr [esp + 12]
			call FindNodeByName
			add esp, 4
			pop edx
			pop ecx
			ret
		}
	}

	static void BigNodeNames() {
		if (memcmp((const void*)(uintptr_t)kFillAt, kFillBytes, sizeof(kFillBytes)) != 0 ||
			memcmp((const void*)(uintptr_t)kFindAt, kFindBytes, sizeof(kFindBytes)) != 0) {
			Log::TypedLog(CHN_DLL, "BigWorld: the game's node name table is not where it was expected, left as it is\n");
			return;
		}
		patchJump((void*)(uintptr_t)kFillAt, (void*)FillHook);
		patchJump((void*)(uintptr_t)kFindAt, (void*)FindHook);
		Log::TypedLog(CHN_DLL, "BigWorld: node names in a table as big as the level needs (the game's has 16384 slots)\n");
	}

	/******************************************************************/
	/*                                                                */
	/*  Level lights                                                  */
	/*                                                                */
	/******************************************************************/

	// The game's level lights: a count at 0x006FF9D4 and 36-byte slots numbered from 1 at
	// 0x006FFAB4 + 36 n, 256 of them. 0x00479E30 hands out the next slot, or 0 when the count is at
	// 256 (cmp eax, 100h); 0x00479E60 sets the count to 0; 0x00479F10, 0x00479F50 and 0x0047A540 go
	// through the first count slots. Those five are all the code that knows where the slots are: the
	// seven addresses below and the 256. The slots move into the DLL, 2048 of them; the count stays.
	static const uint32_t kLights = 2048;
	static const uint32_t kSlotSize = 36;
	static const uint32_t kCapAt = 0x00479E36;          // cmp eax, 100h: the 100h
	static __declspec(align(16)) uint8_t s_lightSlots[kSlotSize * (kLights + 1)];

	struct LightAddress {
		uint32_t at;        // where the address is in the code
		uint32_t stock;     // the game's address there
		uint32_t offset;    // the same place in the new slots, from their start (slot 0)
	};
	static const LightAddress kLightAddresses[] = {
		{ 0x00479E4A, 0x006FFAB4, 0x00 },   // 0x00479E30, lea eax, [eax*4 + slots]: slot n at slots + 36 n
		{ 0x00479F22, 0x006FFAF8, 0x44 },   // 0x00479F10, mov ecx, slot 1 + 20h
		{ 0x00479F3E, 0x006FFAD8, 0x24 },   //             lea eax, [eax*4 + slot 1]
		{ 0x00479F82, 0x006FFAE0, 0x2C },   // 0x00479F50, mov esi, slot 1 + 8
		{ 0x0047A03B, 0x006FFAD8, 0x24 },   //             lea eax, [eax*4 + slot 1]
		{ 0x0047A559, 0x006FFAF8, 0x44 },   // 0x0047A540, mov ecx, slot 1 + 20h
		{ 0x0047A579, 0x006FFAD8, 0x24 },   //             lea eax, [eax*4 + slot 1]
	};
	static const int kLightAddressCount = sizeof(kLightAddresses) / sizeof(kLightAddresses[0]);

	static void BigLights() {
		bool expected = *(const uint8_t*)(uintptr_t)(kCapAt - 1) == 0x3D && *(const uint32_t*)(uintptr_t)kCapAt == 0x100;
		for (int i = 0; expected && i < kLightAddressCount; i++) {
			expected = *(const uint32_t*)(uintptr_t)kLightAddresses[i].at == kLightAddresses[i].stock;
		}
		if (!expected) {
			Log::TypedLog(CHN_DLL, "BigWorld: the game's level lights are not where they were expected, left at 256\n");
			return;
		}
		const uint32_t slots = (uint32_t)(uintptr_t)s_lightSlots;
		for (int i = 0; i < kLightAddressCount; i++) {
			patchDWord((void*)(uintptr_t)kLightAddresses[i].at, slots + kLightAddresses[i].offset);
		}
		patchDWord((void*)(uintptr_t)kCapAt, kLights);
		Log::TypedLog(CHN_DLL, "BigWorld: room for %u level lights (the game has 256)\n", kLights);
	}

	/******************************************************************/
	/*                                                                */
	/*  The faces in a box                                            */
	/*                                                                */
	/******************************************************************/

	// The game's collision finds the faces of a collision object that a box reaches through the object's BSP
	// tree (0x0041AFD0, called from 0x004176B1, 0x00419330 and 0x0041966B, which go through the list it gives
	// back). Its walk (0x0041AF00) appends the face numbers of each leaf the box reaches, 16 bits each, to one
	// static list at 0x006985D8, the count at 0x006985D0, without looking at the list's size: 6000 numbers, and
	// right after them the component manager's list heads (0x0069B4B8, one per type of component), its own
	// pointer (0x0069B6B8) and the object manager's (0x0069B6C0). A box that reached more faces than that wrote
	// face numbers over those, two consecutive ones where a pointer was: the crashes while driving across the
	// joined world (0x0041FBB3 and 0x0041CBC0 with 0x003B003A, 0x004515F7 with 0x001B001A in the suspend
	// components' list). The list moves into the DLL with room for 65536, and a leaf that does not fit is cut
	// short and counted (src/Flickit/facelist.h).
	static const uint32_t kFaceCap = 65536;
	static const uint32_t kGameFaceCap = 6000;
	static const uint32_t kLeafAt = 0x0041AF92;          // the walk at a leaf: movzx edx, word ptr [esi+2] ...
	static const uint32_t kLeafEnd = 0x0041AFC1;         // add [count], edx; pop edi, esi, ebp, ebx; ret 8
	static const uint32_t kListAt = 0x0041B00D;          // mov eax, list: what 0x0041AFD0 gives back
	static const uint8_t kLeafBytes[] = { 0x0F, 0xB7, 0x56, 0x02, 0x85, 0xD2, 0xA1, 0xD0, 0x85, 0x69, 0x00, 0x8B, 0x76, 0x04,
		0x8D, 0x04, 0x45, 0xD8, 0x85, 0x69, 0x00 };
	static const uint8_t kLeafEndBytes[] = { 0x01, 0x15, 0xD0, 0x85, 0x69, 0x00, 0x5F, 0x5E, 0x5D, 0x5B, 0xC2, 0x08, 0x00 };
	static uint16_t s_faces[kFaceCap];
	static volatile uint32_t* const kFaceCount = (volatile uint32_t*)0x006985D0;
	static FaceList::Tally s_faceTally = { 0, 0, 0 };
	static bool s_facesOn = false;

	static void __cdecl AppendFaces(const uint16_t* from, uint32_t n) {
		FaceList::Append(s_faces, kFaceCap, kFaceCount, from, n, kGameFaceCap, &s_faceTally);
	}

	// In the place of 0x0041AF92: esi is the leaf (its count at +2, its numbers at [+4]); the walk's four
	// registers are on the stack (edi, esi, ebp, ebx) and its two arguments after its return. Its end is the
	// walk's own (0x0041AFC7), which its other way out jumps to as well.
	static __declspec(naked) void LeafHook() {
		__asm {
			movzx edx, word ptr [esi + 2]
			mov eax, dword ptr [esi + 4]
			push edx
			push eax
			call AppendFaces
			add esp, 8
			pop edi
			pop esi
			pop ebp
			pop ebx
			ret 8
		}
	}

	static void BigFaceList() {
		if (memcmp((const void*)(uintptr_t)kLeafAt, kLeafBytes, sizeof(kLeafBytes)) != 0
			|| memcmp((const void*)(uintptr_t)kLeafEnd, kLeafEndBytes, sizeof(kLeafEndBytes)) != 0
			|| *(const uint8_t*)(uintptr_t)(kListAt - 1) != 0xB8 || *(const uint32_t*)(uintptr_t)kListAt != 0x006985D8) {
			Log::TypedLog(CHN_DLL, "BigWorld: the game's list of the faces in a box is not where it was expected, left at 6000 (it overruns past that)\n");
			return;
		}
		patchDWord((void*)(uintptr_t)kListAt, (uint32_t)(uintptr_t)s_faces);
		patchJump((void*)(uintptr_t)kLeafAt, (void*)LeafHook);
		s_facesOn = true;
		Log::TypedLog(CHN_DLL, "BigWorld: room for %u faces in a box of the collision (the game has 6000, and wrote past them over its managers' pointers)\n", kFaceCap);
	}

	bool FaceListNews(char* text, size_t size) {
		static FaceList::Told s_told = FaceList::FirstTold(kGameFaceCap);
		if (!s_facesOn) {
			return false;
		}
		switch (FaceList::NewsOf(s_faceTally, &s_told)) {
		case FaceList::kDropped:
			Str::Format(text, size, "BigWorld: a box in the collision reached more than %u faces; %u face numbers did not fit, all boxes together "
				"(those faces were not tried); the most a box reached: %u\n", kFaceCap, s_faceTally.dropped, s_faceTally.most);
			return true;
		case FaceList::kMost:
			Str::Format(text, size, "BigWorld: a box in the collision reached %u faces, past the game's list of 6000 (past that it wrote over its "
				"managers' pointers: the crashes while driving); %u boxes past 6000 so far\n", s_faceTally.most, s_faceTally.past);
			return true;
		default:
			return false;
		}
	}

	void AtStartup(const char* configFile) {
		if (GetPrivateProfileIntA(MEMORY_SECTION, "BigNodeNames", 1, configFile)) {
			BigNodeNames();
		}
		else {
			Log::TypedLog(CHN_DLL, "BigWorld: the game's own node name table, 16384 slots (BigNodeNames=0)\n");
		}
		if (GetPrivateProfileIntA(MEMORY_SECTION, "BigLights", 1, configFile)) {
			BigLights();
		}
		else {
			Log::TypedLog(CHN_DLL, "BigWorld: the game's 256 level lights (BigLights=0)\n");
		}
		if (GetPrivateProfileIntA(MEMORY_SECTION, "BigFaceList", 1, configFile)) {
			BigFaceList();
		}
		else {
			Log::TypedLog(CHN_DLL, "BigWorld: the game's list of 6000 faces in a box, which overruns (BigFaceList=0)\n");
		}
	}
}
