#pragma once
// Camera-relative drawing of the level: the parts that need neither Windows nor the game, tested by
// tests/camrel_test.cpp.
//
// The game draws its level with Direct3D's fixed pipeline from points kept in world units, measured from
// New Jersey's middle, and hands it the view as one float matrix. Far from that middle the points and the
// view's move are both big numbers, and the card's float sums lose what is left when they cancel: depth
// most of all, so decals flash. Here a static mesh's points go to the card relative to a middle of its
// own, and the view for its draw is moved by that middle in double, so the big numbers cancel before
// anything is rounded to float. Everything else about the draw stays as the game made it.
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unordered_map>

namespace CamRel {
	// A 4x4 matrix as Direct3D keeps one: rows, with a point as a row vector on the left (p' = p M).
	struct Matrix {
		float m[4][4];
	};

	inline bool IsIdentity(const Matrix& a) {
		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) {
				if (a.m[r][c] != (r == c ? 1.0f : 0.0f)) {
					return false;
				}
			}
		}
		return true;
	}

	// T(c) M: does to (p - c) what M does to p. The first three rows stay as they are; the fourth gets c's
	// share, worked out in double, so that a far middle and a far camera cancel before rounding.
	inline Matrix Shifted(const Matrix& a, const double c[3]) {
		Matrix out = a;
		for (int j = 0; j < 4; j++) {
			const double v = (double)a.m[3][j] + c[0] * (double)a.m[0][j] + c[1] * (double)a.m[1][j] + c[2] * (double)a.m[2][j];
			out.m[3][j] = (float)v;
		}
		return out;
	}

	inline float ReadFloat(const uint8_t* p) {
		float f;
		memcpy(&f, p, sizeof(f));
		return f;
	}

	inline void WriteFloat(uint8_t* p, float f) {
		memcpy(p, &f, sizeof(f));
	}

	// The middle of the box around a buffer's points (the first three floats of each vertex), rounded to
	// whole units, so that p - c is exact in float for points near it. extent: the largest distance along
	// one axis from the middle to a point. False when there are no points or a number is not finite.
	inline bool Center(const uint8_t* v, uint32_t count, uint32_t stride, double c[3], double* extent) {
		if (!v || count == 0 || stride < 12) {
			return false;
		}
		double lo[3], hi[3];
		for (uint32_t i = 0; i < count; i++) {
			const uint8_t* p = v + (size_t)i * stride;
			for (int k = 0; k < 3; k++) {
				const double x = (double)ReadFloat(p + 4 * k);
				if (!(x == x) || x > 1e30 || x < -1e30) {
					return false;
				}
				if (i == 0 || x < lo[k]) {
					lo[k] = x;
				}
				if (i == 0 || x > hi[k]) {
					hi[k] = x;
				}
			}
		}
		double e = 0.0;
		for (int k = 0; k < 3; k++) {
			c[k] = floor((lo[k] + hi[k]) * 0.5 + 0.5);
			const double d = fmax(hi[k] - c[k], c[k] - lo[k]);
			if (d > e) {
				e = d;
			}
		}
		if (extent) {
			*extent = e;
		}
		return true;
	}

	// count vertices copied from src to dst with the first three floats of each made relative to c. Each
	// vertex is written once, in order (dst is a locked buffer of the card's, which is only ever written).
	inline void RebaseCopy(uint8_t* dst, const uint8_t* src, uint32_t count, uint32_t stride, const double c[3]) {
		for (uint32_t i = 0; i < count; i++) {
			const uint8_t* s = src + (size_t)i * stride;
			uint8_t* d = dst + (size_t)i * stride;
			for (int k = 0; k < 3; k++) {
				WriteFloat(d + 4 * k, (float)((double)ReadFloat(s + 4 * k) - c[k]));
			}
			memcpy(d + 12, s + 12, stride - 12);
		}
	}

	// Which of a mesh's buffers its draw has bound: with more than one, the game moves its index on to the
	// next one after binding and before drawing.
	inline uint32_t BoundBuffer(uint32_t index, uint32_t count) {
		if (count <= 1) {
			return 0;
		}
		return (index + count - 1) % count;
	}

	// One registered buffer: whose copy of the points it is (the game's buffer object), the middle its
	// points are sent relative to, and the vertex size and byte size of the copy.
	struct Entry {
		const void* vbuf;
		double c[3];
		uint32_t stride;
		uint32_t size;
	};

	// The registered buffers, by the address of the game's copy of their points in memory (which is what
	// its upload copies from, and what its buffer object points to).
	class Registry {
	public:
		void Put(const void* key, const Entry& e) {
			m_map[(uintptr_t)key] = e;
		}
		const Entry* Find(const void* key) const {
			std::unordered_map<uintptr_t, Entry>::const_iterator it = m_map.find((uintptr_t)key);
			return it == m_map.end() ? NULL : &it->second;
		}
		bool Erase(const void* key) {
			return m_map.erase((uintptr_t)key) != 0;
		}
		size_t Count() const {
			return m_map.size();
		}
		void Clear() {
			m_map.clear();
		}
		template <class F>
		void ForEach(F f) const {
			for (std::unordered_map<uintptr_t, Entry>::const_iterator it = m_map.begin(); it != m_map.end(); ++it) {
				f((const void*)it->first, it->second);
			}
		}

	private:
		std::unordered_map<uintptr_t, Entry> m_map;
	};

	// The registration rules, from a mesh's fields and its buffer's (filled in by the caller from the game's
	// objects). A mesh qualifies when the game never writes its points to the card by itself: one buffer,
	// no animated copy, no colours that wibble (both of those are written straight to the card every frame,
	// from the copy in memory), no matrix of its own, a plain position first, a copy of the points in memory,
	// not a buffer the game writes every frame, and its middle away from the origin (nearer than that the
	// level already draws well, and models sit there in their own frame).
	struct MeshInfo {
		uint32_t buffers;      // how many buffers the mesh turns through
		int32_t animated;      // nonzero: the game rewrites the buffer from its own copy each frame
		uint32_t wibble;       // nonzero: its colours wibble, the whole buffer written to the card each frame
		int32_t matrix;        // -1: no matrix of its own
		uint32_t fvf;          // its vertex format
		uint32_t stride;       // its vertex size
		uint32_t vbufFlags;    // the buffer object's flags: 1 no copy in memory, 2 the game's special case
		uint32_t usage;        // the buffer's usage (0x200: written every frame)
		bool hasCopy;          // the buffer object has a copy of the points in memory
		bool hasBuffer;        // and a buffer on the card
		uint32_t size;         // the copy's size in bytes
	};

	enum { kFvfPositionMask = 0x400E, kFvfXyz = 0x002, kUsageDynamic = 0x200 };
	static const double kNear = 1024.0;     // middles nearer the origin than this, along every axis, stay as they are

	inline bool Qualifies(const MeshInfo& m) {
		return m.buffers == 1 && m.animated == 0 && m.wibble == 0 && m.matrix == -1 && (m.fvf & kFvfPositionMask) == kFvfXyz &&
			m.stride >= 12 && (m.stride & 3) == 0 && (m.vbufFlags & 3) == 0 && (m.usage & kUsageDynamic) == 0 &&
			m.hasCopy && m.hasBuffer && m.size >= m.stride;
	}

	inline bool FarEnough(const double c[3]) {
		return fabs(c[0]) >= kNear || fabs(c[1]) >= kNear || fabs(c[2]) >= kNear;
	}
}
