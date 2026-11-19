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

	static uint32_t s_reserved = 0;      // bytes the test holds below 2 GB
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
	static uint32_t TakeLowAddressSpace() {
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		const uint32_t grain = si.dwAllocationGranularity;
		uint32_t taken = 0;
		uint32_t at = (uint32_t)(uintptr_t)si.lpMinimumApplicationAddress;
		while (at < kTwoGB) {
			MEMORY_BASIC_INFORMATION mbi;
			if (VirtualQuery((LPCVOID)(uintptr_t)at, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.RegionSize == 0) {
				break;
			}
			const uint32_t start = (uint32_t)(uintptr_t)mbi.BaseAddress;
			uint32_t end = start + (uint32_t)mbi.RegionSize;
			if (end < start || end > kTwoGB) {
				end = kTwoGB;
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

	void AtStartup(const char* configFile) {
		EnlargeScriptMemory(configFile);
		const bool laa = LargeAddressAware();
		MEMORYSTATUSEX m;
		m.dwLength = sizeof(m);
		const uint64_t space = GlobalMemoryStatusEx(&m) ? m.ullTotalVirtual : 0;
		Log::TypedLog(CHN_DLL, "Memory: %u MB of address space; THUG-SDL.exe %s\n", (unsigned)(space >> 20),
			laa ? "has the large-address flag" : "does not have the large-address flag");
		if (!GetPrivateProfileIntA(MEMORY_SECTION, "HighMemoryTest", 0, configFile)) {
			return;
		}
		if (!laa || space < 0xC0000000ull) {
			Log::TypedLog(CHN_DLL, "Memory: HighMemoryTest needs the large-address flag on THUG-SDL.exe and a 64-bit Windows, so it is off\n");
			return;
		}
		s_reserved = TakeLowAddressSpace();
		Log::TypedLog(CHN_DLL, "Memory: HighMemoryTest holds %u MB below 2 GB (reserved, not used), so the game's memory lands above it\n",
			s_reserved >> 20);
	}

	void LogStatus() {
		MEMORYSTATUSEX m;
		m.dwLength = sizeof(m);
		if (GlobalMemoryStatusEx(&m)) {
			const uint64_t used = m.ullTotalVirtual - m.ullAvailVirtual - s_reserved;
			Log::TypedLog(CHN_DLL, "Memory: %u MB of the %u MB address space in use%s\n", (unsigned)(used >> 20),
				(unsigned)(m.ullTotalVirtual >> 20), s_reserved ? ", besides what HighMemoryTest holds" : "");
		}
		const uint32_t start = *kMainBlockStart;
		if (start) {
			Log::TypedLog(CHN_DLL, "Memory: the game's main block is %u MB at 0x%08X to 0x%08X, %s 2 GB\n", kMainBlockSize >> 20,
				start, start + kMainBlockSize, start >= kTwoGB ? "above" : "below");
		}
	}

	void LogOnceReady() {
		if (!s_loggedReady && *kMainBlockStart) {
			s_loggedReady = true;
			LogStatus();
		}
	}
}
