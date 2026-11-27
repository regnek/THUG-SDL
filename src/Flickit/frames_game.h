#pragma once
#include <stdint.h>
#include <intrin.h>

// The watch on the game's frames (frames.h): how long they take, how a frame's time splits, and where the
// game's thread is when another thread looks, about once a millisecond. When a stretch of ten seconds ran
// slow, what it came to goes to flickit_watchdog.txt: the frames, the modules and the places seen most, and
// where those were called from. The addresses are the game's own ("exe 004B5120"), the mod's ("mod+1A2B4",
// from the start of thugsdl.dll: the linker's map of the build names the function) or another module's
// ("d3d9.dll+4F210").
namespace FramesGame {
	// [Flickit] FrameWatch (1 on)
	void Configure(const char* configFile);

	// Once a frame, on the game's thread, where it reads the first pad
	void Boundary();

	// The level and all that is drawn over it are drawn: just before the device's EndScene
	void RenderEnd();

	// Something that may change the frames is about to be switched (the camera-relative drawing, by its key).
	// What the stretch so far came to is written, slow or not; a new one starts at the next frame, after the
	// switch has done its work, and that one is written too.
	void Switching();

	// The draws of a frame so far, for the mesh draw to add to (camrel_game.cpp), in ticks of the processor's
	// clock
	struct DrawTally {
		uint32_t on;            // the watch is on: the draws are timed
		uint32_t draws;
		uint32_t shifted;       // of them, drawn relative to their middles
		uint64_t first;         // when the first of the frame began
		uint64_t inCall;        // inside Direct3D's draw calls
		uint64_t own;           // the rest of the mesh draw's hook
	};
	extern DrawTally g_draw;

	inline bool Timing() {
		return g_draw.on != 0;
	}
	inline uint64_t Tick() {
		return __rdtsc();
	}
	// A mesh was drawn: its hook began at start and spent inCall of its time in Direct3D
	inline void Drawn(uint64_t start, uint64_t inCall, bool shifted) {
		const uint64_t end = __rdtsc();
		if (!g_draw.draws) {
			g_draw.first = start;
		}
		g_draw.draws++;
		g_draw.shifted += shifted ? 1 : 0;
		g_draw.inCall += inCall;
		g_draw.own += (end - start) - inCall;
	}
}
