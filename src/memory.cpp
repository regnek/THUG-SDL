#include <windows.h>
#include <stdint.h>

#include "memory.h"
#include "patch.h"
#include "Logger/Logger.h"

#define MEMORY_SECTION "Memory"

namespace Memory {
	static const uint32_t kTwoGB = 0x80000000u;
	// The game's main block of memory (its memory manager's region): the start as the game's own
	// setup stores it (0x0057C475), and the size THUG-SDL gives it (patchStaticValues, 360 MB)
	static const volatile uint32_t* const kMainBlockStart = (const volatile uint32_t*)0x007CA060;
	static const uint32_t kMainBlockSize = 0x16800000;
	// Where its size is in the game's code: push size; call malloc; mov [0x007CA060], eax; add eax, size
	static const uint32_t kMainBlockSizeAt[2] = { 0x0057C460, 0x0057C47B };
	static const uint32_t kMainBlockCall = 0x0057C470;   // the call to malloc, which is at 0x005F9755
	static const uint32_t kGameMalloc = 0x005F9755;
	static uint32_t s_mainBlockSize = kMainBlockSize;

	static uint32_t s_reserved = 0;      // bytes held (reserved, never used) by HighMemoryTest or StayBelow2GB
	static const char* s_held = "";      // which of the two
	static bool s_loggedReady = false;

	// The game's script memory: a heap of its own (carved out of the main block) and, inside it,
	// fixed pools of the objects scripts and level node arrays are made of. The game makes them all
	// once at start-up from the sizes below, which are pushed as constants. When a pool runs out its
	// allocator hands back nothing and the caller writes through it, so the game crashes. New
	// Jersey alone fills about 80% of the vector, component and structure pools; two levels joined
	// into one need about one and a half times what the pools hold. BigScriptMemory=1 (the default)
	// makes each pool and the heap 8 times as big: the sizes the THUGPLUS block in config.cpp lists.
	// The heap grows from 3.9 MB to 31.6 MB of the 360 MB main block.
	// The same switch enlarges the pool every hash table in the game takes its overflow items from
	// (12 bytes each, made at start-up): 12500 of them run out while a joined New Jersey and
	// Manhattan load (the crash is the null write at 0x0049D4C5, in the hash table's insert).
	struct ScriptSize {
		uint32_t at;        // a push of the size: 0x68 and the 32-bit value
		uint32_t stock;     // what is there when the DLL loads (after patchStaticValues)
		uint32_t big;
		const char* what;
	};
	static const ScriptSize kScriptSizes[] = {
		{ 0x0040A668, 0x12C,    0x960,     "script cache entries" },
		{ 0x0040A686, 0x14820,  0xA4100,   "components" },
		{ 0x0040A6A4, 0x4970,   0x24B80,   "structures" },
		{ 0x0040A6C2, 0x2580,   0x12C00,   "vectors" },
		{ 0x0040A6E0, 0x3E8,    0x1F40,    "pairs" },
		{ 0x0040A701, 0x1D4C,   0xEA60,    "arrays" },
		{ 0x0040A71F, 0x3E80,   0x109A0,   "names" },        // the game has 0x2134; patchStaticValues makes it 0x3E80
		{ 0x0040A73D, 0x12C,    0x960,     "running scripts" },
		{ 0x0057B9EF, 0x3C5550, 0x1E2AA80, "bytes of script heap" },
		{ 0x004BFC3D, 0x30D4,   0x186A0,   "hash table items" },
	};
	enum { kHeapSize = 8, kHashItems = 9 };
	static const int kScriptSizeCount = sizeof(kScriptSizes) / sizeof(kScriptSizes[0]);

	static void EnlargeScriptMemory(const char* configFile) {
		if (!GetPrivateProfileIntA(MEMORY_SECTION, "BigScriptMemory", 1, configFile)) {
			Log::TypedLog(CHN_DLL, "Memory: script memory as the game has it (BigScriptMemory=0)\n");
			return;
		}
		for (int i = 0; i < kScriptSizeCount; i++) {
			const uint8_t* code = (const uint8_t*)(uintptr_t)kScriptSizes[i].at;
			const uint32_t value = *(const uint32_t*)(code + 1);
			if (code[0] != 0x68 || value != kScriptSizes[i].stock) {
				Log::TypedLog(CHN_DLL, "Memory: the size of the %s is not where it was expected (0x%08X), script memory left as it is\n",
					kScriptSizes[i].what, kScriptSizes[i].at);
				return;
			}
		}
		for (int i = 0; i < kScriptSizeCount; i++) {
			patchDWord((void*)(uintptr_t)(kScriptSizes[i].at + 1), kScriptSizes[i].big);
		}
		const uint32_t heap = kScriptSizes[kHeapSize].big;
		Log::TypedLog(CHN_DLL, "Memory: script memory 8 times as big: a %u.%u MB heap, %u components, %u structures, %u vectors, %u arrays, %u names, %u running scripts; %u hash table items\n",
			heap / 1000000, (heap / 100000) % 10, kScriptSizes[1].big, kScriptSizes[2].big, kScriptSizes[3].big, kScriptSizes[5].big,
			kScriptSizes[6].big, kScriptSizes[7].big, kScriptSizes[kHashItems].big);
	}

	static bool LargeAddressAware() {
		const uint8_t* base = (const uint8_t*)GetModuleHandleA(NULL);
		const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
		const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
		return (nt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) != 0;
	}

	// Reserve every free stretch below 2 GB (address space only, nothing committed)
	static uint32_t TakeAddressSpace(uint32_t from, uint32_t to) {
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		const uint32_t grain = si.dwAllocationGranularity;
		uint32_t taken = 0;
		uint32_t at = from;
		if (at < (uint32_t)(uintptr_t)si.lpMinimumApplicationAddress) {
			at = (uint32_t)(uintptr_t)si.lpMinimumApplicationAddress;
		}
		const uint32_t top = (uint32_t)(uintptr_t)si.lpMaximumApplicationAddress;
		if (to > top) {
			to = top;
		}
		while (at < to) {
			MEMORY_BASIC_INFORMATION mbi;
			if (VirtualQuery((LPCVOID)(uintptr_t)at, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.RegionSize == 0) {
				break;
			}
			const uint32_t start = (uint32_t)(uintptr_t)mbi.BaseAddress;
			uint32_t end = start + (uint32_t)mbi.RegionSize;
			if (end < start || end > to) {
				end = to;
			}
			if (mbi.State == MEM_FREE) {
				// Reservations start on the allocation grain; the tail of a grain behind
				// another allocation cannot be had
				const uint32_t from = (start + grain - 1) & ~(grain - 1);
				if (end > from && VirtualAlloc((LPVOID)(uintptr_t)from, end - from, MEM_RESERVE, PAGE_NOACCESS)) {
					taken += end - from;
				}
			}
			if (end <= at) {
				break;
			}
			at = end;
		}
		return taken;
	}

	// [Memory] MainHeapMB: the size of the game's main block, where a level's files load and its scene is
	// built. THUG-SDL makes it 360 MB. A scene file sits in it whole while the level loads (New Jersey, Manhattan
	// and Tampa at 4x: 230 MB), so a world of more districts needs a bigger one. That needs the large-address
	// flag: with 2 GB of address space a bigger block may not be had at all. So the default is 1024 MB with
	// the flag and THUG-SDL's 360 MB without; it never goes below 360 MB, and without the flag never above.
	// Once the block is to be bigger, the game's call to malloc for it comes here: if there is no room for
	// it, the block is THUG-SDL's 360 MB instead of a crash at start-up. The add of the size to the start
	// (0x0057C47A) runs after this returns, so it can still be changed.
	static void* __cdecl AllocMainBlock(size_t size) {
		typedef void* (__cdecl* MallocFn)(size_t);
		const MallocFn gameMalloc = (MallocFn)(uintptr_t)kGameMalloc;
		void* block = gameMalloc(size);
		if (block || size <= kMainBlockSize) {
			return block;
		}
		Log::TypedLog(CHN_DLL, "Memory: no room for a %u MB main block, so it is %u MB\n",
			(unsigned)(size >> 20), (unsigned)(kMainBlockSize >> 20));
		s_mainBlockSize = kMainBlockSize;
		patchDWord((void*)(uintptr_t)kMainBlockSizeAt[1], kMainBlockSize);
		return gameMalloc(kMainBlockSize);
	}

	static void MainBlock(const char* configFile, bool high) {
		const UINT stockMB = kMainBlockSize >> 20;
		UINT mb = GetPrivateProfileIntA(MEMORY_SECTION, "MainHeapMB", high ? 1024 : stockMB, configFile);
		if (mb < stockMB) {
			mb = stockMB;
		}
		if (!high && mb > stockMB) {
			Log::TypedLog(CHN_DLL, "Memory: MainHeapMB=%u needs the address space above 2 GB (the large-address flag on THUG-SDL.exe, and StayBelow2GB=0); the main block stays %u MB\n", mb, stockMB);
			mb = stockMB;
		}
		if (mb > 1536) {
			mb = 1536;
		}
		if (mb == stockMB) {
			Log::TypedLog(CHN_DLL, "Memory: the game's main block stays %u MB\n", stockMB);
			return;
		}
		for (int i = 0; i < 2; i++) {
			if (*(const uint32_t*)(uintptr_t)kMainBlockSizeAt[i] != kMainBlockSize) {
				Log::TypedLog(CHN_DLL, "Memory: the size of the game's main block is not where it was expected (0x%08X), left at %u MB\n",
					kMainBlockSizeAt[i], stockMB);
				return;
			}
		}
		const uint8_t* call = (const uint8_t*)(uintptr_t)kMainBlockCall;
		if (call[0] != 0xE8 || kMainBlockCall + 5 + *(const uint32_t*)(call + 1) != kGameMalloc) {
			Log::TypedLog(CHN_DLL, "Memory: the game's call for its main block is not where it was expected (0x%08X), left at %u MB\n",
				kMainBlockCall, stockMB);
			return;
		}
		s_mainBlockSize = (uint32_t)mb << 20;
		for (int i = 0; i < 2; i++) {
			patchDWord((void*)(uintptr_t)kMainBlockSizeAt[i], s_mainBlockSize);
		}
		patchCall((void*)(uintptr_t)kMainBlockCall, (void*)AllocMainBlock);
		Log::TypedLog(CHN_DLL, "Memory: the game's main block will be %u MB (THUG-SDL makes it %u)\n", mb, stockMB);
	}

	void AtStartup(const char* configFile) {
		EnlargeScriptMemory(configFile);
		const bool laa = LargeAddressAware();
		MEMORYSTATUSEX m;
		m.dwLength = sizeof(m);
		const uint64_t space = GlobalMemoryStatusEx(&m) ? m.ullTotalVirtual : 0;
		const bool four = laa && space >= 0xC0000000ull;
		// [Memory] StayBelow2GB=1: with the flag on the exe, hold everything above 2 GB, so the game runs in
		// the 2 GB it was made for (to tell whether something goes wrong only up there)
		const bool low = four && GetPrivateProfileIntA(MEMORY_SECTION, "StayBelow2GB", 0, configFile) != 0;
		MainBlock(configFile, laa && !low);
		Log::TypedLog(CHN_DLL, "Memory: %u MB of address space; THUG-SDL.exe %s\n", (unsigned)(space >> 20),
			laa ? "has the large-address flag" : "does not have the large-address flag");
		if (low) {
			s_reserved = TakeAddressSpace(kTwoGB, 0xFFFFFFFFu);
			s_held = "StayBelow2GB";
			Log::TypedLog(CHN_DLL, "Memory: StayBelow2GB holds %u MB above 2 GB (reserved, not used), so the game's memory stays below it\n",
				s_reserved >> 20);
			return;
		}
		if (!GetPrivateProfileIntA(MEMORY_SECTION, "HighMemoryTest", 0, configFile)) {
			return;
		}
		if (!four) {
			Log::TypedLog(CHN_DLL, "Memory: HighMemoryTest needs the large-address flag on THUG-SDL.exe and a 64-bit Windows, so it is off\n");
			return;
		}
		s_reserved = TakeAddressSpace(0, kTwoGB);
		s_held = "HighMemoryTest";
		Log::TypedLog(CHN_DLL, "Memory: HighMemoryTest holds %u MB below 2 GB (reserved, not used), so the game's memory lands above it\n",
			s_reserved >> 20);
	}

	void LogStatus() {
		MEMORYSTATUSEX m;
		m.dwLength = sizeof(m);
		if (GlobalMemoryStatusEx(&m)) {
			const uint64_t used = m.ullTotalVirtual - m.ullAvailVirtual - s_reserved;
			Log::TypedLog(CHN_DLL, "Memory: %u MB of the %u MB address space in use%s%s\n", (unsigned)(used >> 20),
				(unsigned)(m.ullTotalVirtual >> 20), s_reserved ? ", besides what is held for " : "", s_reserved ? s_held : "");
		}
		const uint32_t start = *kMainBlockStart;
		if (start) {
			Log::TypedLog(CHN_DLL, "Memory: the game's main block is %u MB at 0x%08X to 0x%08X, %s 2 GB\n", s_mainBlockSize >> 20,
				start, start + s_mainBlockSize, start >= kTwoGB ? "above" : "below");
		}
	}

	void LogOnceReady() {
		if (!s_loggedReady && *kMainBlockStart) {
			s_loggedReady = true;
			LogStatus();
		}
	}
}
