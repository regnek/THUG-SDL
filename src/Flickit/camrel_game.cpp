// Camera-relative drawing of the level (see camrel.h): the hooks in the game's mesh code.
//
// The game's mesh (sMesh) keeps its vertices in a buffer object of its own (0x24 bytes, made by
// 0x004B3B20, unmade by 0x004B6480) that holds a copy of them in memory and a Direct3D vertex buffer; every
// upload to the card copies the whole copy across (the unlock at 0x004B3C10, and 0x004B3D10 when the device
// is made again). The copy in memory stays in world units for everything in the game that reads it; only
// what goes to the card for a registered mesh is relative to its middle, and only that mesh's draw (the two
// DrawIndexedPrimitive calls in the mesh draw at 0x004B5120, the only draw of a mesh's buffer) has the view
// moved to match. The fixed pipeline keeps everything that hangs on camera space as it was (fog, texture
// coordinates made from camera space, the two directional lights); the game uses no clip planes.
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "d3d9.h"
#include "camrel.h"
#include "camrel_game.h"
#include "game.h"
#include "patch.h"
#include "Logger/Logger.h"

#define RENDER_SECTION "Render"

namespace CamRelGame {
	// The buffer object
	enum {
		VBUF_SIZE = 0x00,           // bytes
		VBUF_USAGE = 0x04,          // Direct3D usage (0x200: written every frame)
		VBUF_BUFFER = 0x08,         // IDirect3DVertexBuffer9*
		VBUF_COPY = 0x0C,           // the copy in memory, what every upload copies from
		VBUF_LOCK_OFFSET = 0x10,    // the unlock's range and flags, set by whoever "locked" it
		VBUF_LOCK_SIZE = 0x14,
		VBUF_LOCK_FLAGS = 0x18,
		VBUF_FLAGS = 0x1C,          // 1: no copy in memory, 2: special, 4: a part of a shared buffer
	};

	// The mesh
	enum {
		MESH_STRIDE = 0x18,         // byte: vertex size
		MESH_BUFFER_INDEX = 0x19,   // byte: the buffer the next draw binds (0x004B532D)
		MESH_BUFFERS = 0x1A,        // byte: how many buffers it turns through (0x004B53E1)
		MESH_FVF = 0x38,            // its vertex format (0x004B5274)
		MESH_WIBBLE = 0x84,         // nonzero: colours that wibble, the buffer written to the card each frame (0x004B4620)
		MESH_ANIMATED = 0x90,       // nonzero: rewritten from its own copy each frame (0x004B5327)
		MESH_MATRIX = 0x94,         // -1, or a matrix the draw puts on the world (0x004B51E8)
		MESH_VBUFS = 0xC8,          // its buffer objects (0x004B5332)
	};

	typedef int(__thiscall* VbufUnlock_t)(void* vbuf);
	static const VbufUnlock_t VbufUnlock = (VbufUnlock_t)0x004B3C10;
	typedef void(__cdecl* GameFree_t)(void* p);
	static const GameFree_t GameFree = (GameFree_t)0x005F9924;

	// The places that change, with what is around them, and the code the hooks lean on
	static const Game::Expect kExpect[] = {
		{ 0x004B3CE0, 9, { 0x8B, 0x45, 0xFC, 0x50, 0xE8, 0x97, 0x42, 0x14, 0x00 }, "the unlock's copy to the card (memcpy)" },
		{ 0x004B3D80, 12, { 0x8B, 0x44, 0x24, 0x08, 0x51, 0x52, 0x50, 0xE8, 0xF4, 0x41, 0x14, 0x00 }, "the remade buffer's copy to the card (memcpy)" },
		{ 0x004B64B0, 13, { 0x8B, 0x46, 0x0C, 0x85, 0xC0, 0x74, 0x09, 0x50, 0xE8, 0x67, 0x34, 0x14, 0x00 }, "the buffer object's free of its copy" },
		{ 0x004B5491, 22, { 0x52, 0x0F, 0xB7, 0x56, 0x22, 0x55, 0x52, 0x8B, 0x56, 0x4C, 0x55, 0x55, 0x52, 0x50,
			0x8B, 0x81, 0x48, 0x01, 0x00, 0x00, 0xFF, 0xD0 }, "the mesh draw's DrawIndexedPrimitive (one pass)" },
		{ 0x004B54E5, 25, { 0x52, 0x0F, 0xB7, 0x56, 0x22, 0x6A, 0x00, 0x52, 0x8B, 0x56, 0x4C, 0x6A, 0x00, 0x6A, 0x00, 0x52, 0x50,
			0x8B, 0x81, 0x48, 0x01, 0x00, 0x00, 0xFF, 0xD0 }, "the mesh draw's DrawIndexedPrimitive (passes)" },
		{ 0x004B6286, 22, { 0x8B, 0x4C, 0x24, 0x2C, 0x64, 0x89, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x59, 0x5F, 0x5E, 0x5D, 0x5B,
			0x83, 0xC4, 0x24, 0xC2, 0x40, 0x00 }, "the end of the mesh setup" },
		{ 0x004B57E7, 6, { 0x8B, 0xF1, 0x89, 0x74, 0x24, 0x14 }, "the mesh setup keeps its mesh at [esp+14h]" },
		{ 0x004B3C10, 9, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x89, 0x4D, 0xF8 }, "the buffer object's unlock" },
		{ 0x004B532D, 12, { 0x0F, 0xB6, 0x46, 0x19, 0x57, 0x8B, 0xBC, 0x86, 0xC8, 0x00, 0x00, 0x00 }, "the mesh draw's buffer" },
		{ 0x004B53E1, 15, { 0x8A, 0x4E, 0x1A, 0x80, 0xF9, 0x01, 0x76, 0x10, 0x0F, 0xB6, 0x46, 0x19, 0x83, 0xC0, 0x01 }, "the mesh draw's turn to the next buffer" },
		{ 0x004B6480, 8, { 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x08, 0x8B, 0x08 }, "the buffer object's unmaking" },
	};

	static CamRel::Registry s_registry;
	static CRITICAL_SECTION s_lock;
	static bool s_installed = false;
	static bool s_on = false;              // registered meshes are on the card relative to their middles
	static int s_key = 64;                 // F7

	// For the log: what was registered since it was last told, and since the start
	static uint32_t s_newMeshes = 0;
	static uint32_t s_newVertices = 0;
	static double s_newExtent = 0.0;       // the largest distance from a middle to a point
	static double s_newFar = 0.0;          // the farthest middle (along an axis)
	static uint32_t s_allMeshes = 0;
	static uint32_t s_partial = 0;         // uploads that were not whole vertices (the tail went as it was)
	static volatile uint32_t s_draws = 0;  // every mesh drawn, for the watchdog
	static bool s_toldWorld = false;

	struct Locked {
		Locked() { EnterCriticalSection(&s_lock); }
		~Locked() { LeaveCriticalSection(&s_lock); }
	};

	static inline uint8_t U8(const void* p, uint32_t off) {
		return *((const uint8_t*)p + off);
	}
	static inline uint32_t U32(const void* p, uint32_t off) {
		return *(const uint32_t*)((const uint8_t*)p + off);
	}
	static inline void* Ptr(const void* p, uint32_t off) {
		return *(void* const*)((const uint8_t*)p + off);
	}

	// The game's upload of a buffer object, the way its own code asks for one (0x004B62A0 does the same):
	// the whole copy, from the start.
	static void Upload(void* vbuf) {
		uint8_t* v = (uint8_t*)vbuf;
		*(uint32_t*)(v + VBUF_LOCK_OFFSET) = 0;
		*(uint32_t*)(v + VBUF_LOCK_SIZE) = *(const uint32_t*)(v + VBUF_SIZE);
		*(uint32_t*)(v + VBUF_LOCK_FLAGS) = 0;
		VbufUnlock(vbuf);
	}

	// In the place of the memcpy in the game's two uploads: (dst, src, size), the caller pops.
	static void* __cdecl UploadCopy(void* dst, const void* src, size_t n) {
		if (s_on) {
			Locked locked;
			const CamRel::Entry* e = s_registry.Find(src);
			if (e) {
				const uint32_t count = (uint32_t)(n / e->stride);
				CamRel::RebaseCopy((uint8_t*)dst, (const uint8_t*)src, count, e->stride, e->c);
				const size_t done = (size_t)count * e->stride;
				if (done < n) {
					memcpy((uint8_t*)dst + done, (const uint8_t*)src + done, n - done);
					s_partial++;
				}
				return dst;
			}
		}
		return memcpy(dst, src, n);
	}

	// In the place of the free of a buffer object's copy when the object is unmade: (p), the caller pops.
	static void __cdecl FreeCopy(void* p) {
		{
			Locked locked;
			s_registry.Erase(p);
		}
		GameFree(p);
	}

	// At the end of the mesh setup, with its mesh.
	static void __cdecl RegisterMesh(void* mesh) {
		if (!mesh) {
			return;
		}
		void* vbuf = Ptr(mesh, MESH_VBUFS);
		if (!vbuf) {
			return;
		}
		CamRel::MeshInfo m;
		m.buffers = U8(mesh, MESH_BUFFERS);
		m.animated = (int32_t)U32(mesh, MESH_ANIMATED);
		m.wibble = U32(mesh, MESH_WIBBLE);
		m.matrix = (int32_t)U32(mesh, MESH_MATRIX);
		m.fvf = U32(mesh, MESH_FVF);
		m.stride = U8(mesh, MESH_STRIDE);
		m.vbufFlags = U32(vbuf, VBUF_FLAGS);
		m.usage = U32(vbuf, VBUF_USAGE);
		m.hasCopy = Ptr(vbuf, VBUF_COPY) != NULL;
		m.hasBuffer = Ptr(vbuf, VBUF_BUFFER) != NULL;
		m.size = U32(vbuf, VBUF_SIZE);
		if (!CamRel::Qualifies(m)) {
			return;
		}
		const uint8_t* copy = (const uint8_t*)Ptr(vbuf, VBUF_COPY);
		const uint32_t count = m.size / m.stride;
		CamRel::Entry e;
		double extent = 0.0;
		if (!CamRel::Center(copy, count, m.stride, e.c, &extent) || !CamRel::FarEnough(e.c)) {
			return;
		}
		e.vbuf = vbuf;
		e.stride = m.stride;
		e.size = m.size;
		{
			Locked locked;
			s_registry.Put(copy, e);
		}
		s_newMeshes++;
		s_allMeshes++;
		s_newVertices += count;
		if (extent > s_newExtent) {
			s_newExtent = extent;
		}
		for (int k = 0; k < 3; k++) {
			const double a = e.c[k] < 0 ? -e.c[k] : e.c[k];
			if (a > s_newFar) {
				s_newFar = a;
			}
		}
		if (s_on) {
			Upload(vbuf);       // the setup sent it in world units: again, relative to its middle
		}
	}

	static void LogNew() {
		size_t count;
		{
			Locked locked;
			count = s_registry.Count();
		}
		Log::TypedLog(CHN_DLL, "CameraRelative: %u meshes (%u vertices) set up relative to their middles since the last note; middles up to %.0f out, points up to %.0f from their middle; %u registered%s\n",
			s_newMeshes, s_newVertices, s_newFar, s_newExtent, (unsigned)count, s_on ? "" : " (switched off: drawn as the game does)");
		s_newMeshes = 0;
		s_newVertices = 0;
		s_newExtent = 0.0;
		s_newFar = 0.0;
	}

	// In the place of the device's DrawIndexedPrimitive in the mesh draw, with the mesh first (from the stub
	// below); pops all eight.
	static HRESULT __stdcall DrawMesh(void* mesh, IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT base, UINT minIndex,
		UINT vertices, UINT start, UINT count) {
		s_draws++;
		if (s_newMeshes) {
			LogNew();
		}
		CamRel::Entry entry;
		bool registered = false;
		// (a mesh the game writes to the card by itself each frame went there in world units)
		if (s_on && mesh && U32(mesh, MESH_ANIMATED) == 0 && U32(mesh, MESH_WIBBLE) == 0) {
			const uint32_t n = U8(mesh, MESH_BUFFERS);
			const uint32_t i = CamRel::BoundBuffer(U8(mesh, MESH_BUFFER_INDEX), n);
			void* vbuf = Ptr(mesh, MESH_VBUFS + 4 * i);
			if (vbuf) {
				Locked locked;
				const CamRel::Entry* e = s_registry.Find(Ptr(vbuf, VBUF_COPY));
				if (e && e->vbuf == vbuf) {
					entry = *e;
					registered = true;
				}
			}
		}
		if (!registered) {
			return dev->DrawIndexedPrimitive(type, base, minIndex, vertices, start, count);
		}
		CamRel::Matrix world;
		if (FAILED(dev->GetTransform(D3DTS_WORLD, (D3DMATRIX*)&world))) {
			return dev->DrawIndexedPrimitive(type, base, minIndex, vertices, start, count);
		}
		if (CamRel::IsIdentity(world)) {
			// the level: the view is moved by the middle, worked out in double
			CamRel::Matrix view;
			if (FAILED(dev->GetTransform(D3DTS_VIEW, (D3DMATRIX*)&view))) {
				return dev->DrawIndexedPrimitive(type, base, minIndex, vertices, start, count);
			}
			const CamRel::Matrix moved = CamRel::Shifted(view, entry.c);
			dev->SetTransform(D3DTS_VIEW, (const D3DMATRIX*)&moved);
			const HRESULT hr = dev->DrawIndexedPrimitive(type, base, minIndex, vertices, start, count);
			dev->SetTransform(D3DTS_VIEW, (const D3DMATRIX*)&view);
			return hr;
		}
		// drawn with a world of its own: that world takes the middle back (right, but no finer than before)
		if (!s_toldWorld) {
			s_toldWorld = true;
			Log::TypedLog(CHN_DLL, "CameraRelative: a registered mesh drawn with a world of its own (row 4 %.1f %.1f %.1f); its world takes its middle (%.0f %.0f %.0f) back\n",
				world.m[3][0], world.m[3][1], world.m[3][2], entry.c[0], entry.c[1], entry.c[2]);
		}
		const CamRel::Matrix moved = CamRel::Shifted(world, entry.c);
		dev->SetTransform(D3DTS_WORLD, (const D3DMATRIX*)&moved);
		const HRESULT hr = dev->DrawIndexedPrimitive(type, base, minIndex, vertices, start, count);
		dev->SetTransform(D3DTS_WORLD, (const D3DMATRIX*)&world);
		return hr;
	}

	// In the place of "mov eax, [ecx+148h] / call eax" in the mesh draw (the device and its six arguments
	// are on the stack, the mesh in esi): DrawMesh gets the mesh as a first argument and pops all eight.
	static __declspec(naked) void DrawStub() {
		__asm {
			pop eax
			push esi
			push eax
			jmp DrawMesh
		}
	}

	// In the place of the first two instructions of the end of the mesh setup (the stack as the setup's
	// body had it, its mesh at [esp+14h]): registers the mesh, does those two, and goes on to the third.
	static uint32_t s_setupBack = 0x004B6291;
	static __declspec(naked) void SetupEndStub() {
		__asm {
			pushad
			push dword ptr [esp + 0x34]
			call RegisterMesh
			add esp, 4
			popad
			mov ecx, dword ptr [esp + 0x2C]
			mov dword ptr fs:[0], ecx
			jmp dword ptr [s_setupBack]
		}
	}

	void AtStartup(const char* configFile) {
		const bool on = GetPrivateProfileInt(RENDER_SECTION, "CameraRelative", 1, configFile) != 0;
		s_key = (int)GetPrivateProfileInt(RENDER_SECTION, "CameraRelativeKey", 64, configFile);
		if (!on) {
			Log::TypedLog(CHN_DLL, "CameraRelative: off ([Render] CameraRelative=0); the level is drawn as the game does\n");
			return;
		}
		for (size_t i = 0; i < sizeof(kExpect) / sizeof(kExpect[0]); i++) {
			if (!Game::Matches(kExpect[i])) {
				Game::Note("CameraRelative: the game's code at %08X (%s) is not what was expected; the level is drawn as the game does\n",
					kExpect[i].address, kExpect[i].what);
				return;
			}
		}
		InitializeCriticalSection(&s_lock);
		patchCall((void*)0x004B3CE4, (void*)UploadCopy);
		patchCall((void*)0x004B3D87, (void*)UploadCopy);
		patchCall((void*)0x004B64B8, (void*)FreeCopy);
		patchCall((void*)0x004B549F, (void*)DrawStub);
		patchNop((void*)0x004B54A4, 3);
		patchCall((void*)0x004B54F6, (void*)DrawStub);
		patchNop((void*)0x004B54FB, 3);
		patchJump((void*)0x004B6286, (void*)SetupEndStub);
		patchNop((void*)0x004B628B, 6);
		s_installed = true;
		s_on = true;
		Log::TypedLog(CHN_DLL, "CameraRelative: on; static meshes away from the origin go to the card relative to their middles, and their view is moved to match in double (key %d switches it)\n", s_key);
	}

	int ToggleKey() {
		return s_installed ? s_key : 0;
	}

	void Toggle() {
		if (!s_installed) {
			return;
		}
		s_on = !s_on;
		std::vector<void*> vbufs;
		{
			Locked locked;
			vbufs.reserve(s_registry.Count());
			s_registry.ForEach([&vbufs](const void*, const CamRel::Entry& e) { vbufs.push_back((void*)e.vbuf); });
		}
		for (size_t i = 0; i < vbufs.size(); i++) {
			Upload(vbufs[i]);
		}
		Log::TypedLog(CHN_DLL, "CameraRelative: switched %s; %u meshes sent to the card again %s\n", s_on ? "on" : "off",
			(unsigned)vbufs.size(), s_on ? "relative to their middles" : "in world units");
	}

	bool Counting() {
		return s_installed;
	}

	uint32_t DrawCount() {
		return s_draws;
	}

	void LogStatus() {
		if (!s_installed) {
			Log::TypedLog(CHN_DLL, "CameraRelative: not in (off in the ini, or the game's code was not as expected)\n");
			return;
		}
		size_t count;
		{
			Locked locked;
			count = s_registry.Count();
		}
		Log::TypedLog(CHN_DLL, "CameraRelative: %s; %u meshes registered now, %u since the start; %u uploads with a part vertex\n",
			s_on ? "on" : "switched off", (unsigned)count, s_allMeshes, s_partial);
	}
}
