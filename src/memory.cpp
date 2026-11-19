#include <windows.h>
#include <stdint.h>

#include "memory.h"
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
