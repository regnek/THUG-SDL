#pragma once
// The game's own files, read without the game: the entries of its .pre archives (unpacked with the LZSS
// the game packs them with) and its .img.xbx pictures. Flickit reads them from the game's data folder at
// run time (the logos on a player's card come from skaterparts.pre); nothing of them is in Flickit's files.
//
// A .pre archive (version 3): uint32 total size, uint16 version, uint16 0xABCD, uint32 count; then per
// entry uint32 size, uint32 packed size (0: stored as it is), uint32 name length (with its padding),
// uint32 the name's checksum, the name, the data (the packed size or the size), padded to 4 bytes.
//
// An .img.xbx (version 2): eight uint32s (version, a checksum, width, height, a format, 0, the original
// width and height as two uint16s, the palette's size in bytes), the palette (B, G, R, A per colour), then
// one byte per pixel, an index into the palette; the pixels are swizzled (the bits of x and y interleaved,
// x's first, while both have bits left) and the bottom row comes first. A picture without a palette has
// four bytes per pixel (B, G, R, A), swizzled the same way.
//
// Pure, tested by tests/gamefiles_test.cpp.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vector>

namespace GameFiles {
	static const uint32_t kMostEntrySize = 64u << 20;       // no entry of the game's is near this
	static const uint32_t kMostNameLength = 1024;

	inline uint32_t U32(const uint8_t* p) {
		return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	}

	// LZSS as the game unpacks a .pre entry: a ring of 4096 bytes, written from 4078; each flag byte's bits,
	// the lowest first, say what follows: 1 a byte as it is, 0 two bytes, a place in the ring (12 bits) and a
	// length (4 bits, plus 3). False when src runs out before size bytes are out.
	inline bool UnpackLzss(const uint8_t* src, size_t n, uint8_t* out, size_t size) {
		uint8_t ring[4096];
		memset(ring, 0, sizeof(ring));
		uint32_t r = 4078;
		size_t i = 0, o = 0;
		while (o < size) {
			if (i >= n) {
				return false;
			}
			const uint8_t flags = src[i++];
			for (int bit = 0; bit < 8 && o < size; bit++) {
				if (flags >> bit & 1) {
					if (i >= n) {
						return false;
					}
					const uint8_t c = src[i++];
					out[o++] = c;
					ring[r] = c;
					r = (r + 1) & 4095;
				}
				else {
					if (i + 1 >= n) {
						return false;
					}
					const uint32_t pos = src[i] | ((uint32_t)(src[i + 1] & 0xF0) << 4);
					const uint32_t length = (uint32_t)(src[i + 1] & 0x0F) + 3;
					i += 2;
					for (uint32_t k = 0; k < length && o < size; k++) {
						const uint8_t c = ring[(pos + k) & 4095];
						out[o++] = c;
						ring[r] = c;
						r = (r + 1) & 4095;
					}
				}
			}
		}
		return true;
	}

	// The archive's own head (12 bytes): its count of entries, or false when it is not a .pre of version 3
	inline bool ReadPreHead(const uint8_t* head, uint32_t* count) {
		if (head[4] != 3 || head[5] != 0 || head[6] != 0xCD || head[7] != 0xAB) {
			return false;
		}
		*count = U32(head + 8);
		return true;
	}

	struct EntryHead {
		uint32_t size;          // unpacked
		uint32_t packed;        // 0: stored as it is
		uint32_t nameLength;    // with its padding
		uint32_t checksum;
	};

	// An entry's head (16 bytes); false when it cannot be one
	inline bool ReadEntryHead(const uint8_t* head, EntryHead* out) {
		out->size = U32(head);
		out->packed = U32(head + 4);
		out->nameLength = U32(head + 8);
		out->checksum = U32(head + 12);
		return out->size <= kMostEntrySize && out->packed <= kMostEntrySize && out->nameLength > 0
			&& out->nameLength <= kMostNameLength;
	}

	// The bytes of the entry in the archive after its name: stored, and with the padding after them
	inline uint32_t Stored(const EntryHead& h) {
		return h.packed ? h.packed : h.size;
	}
	inline uint32_t Padded(uint32_t n) {
		return (n + 3u) & ~3u;
	}

	// Names as the game writes them: letters of either case, \ and / the same. The stored name ends at its
	// first 0 or at its length.
	inline bool SameName(const char* stored, size_t length, const char* name) {
		size_t i = 0;
		for (; i < length && stored[i]; i++) {
			char a = stored[i], b = name[i];
			if (!b) {
				return false;
			}
			a = a >= 'A' && a <= 'Z' ? (char)(a - 'A' + 'a') : a == '/' ? '\\' : a;
			b = b >= 'A' && b <= 'Z' ? (char)(b - 'A' + 'a') : b == '/' ? '\\' : b;
			if (a != b) {
				return false;
			}
		}
		return name[i] == 0;
	}

	// An entry's data, unpacked
	inline bool Unpack(const EntryHead& h, const uint8_t* stored, std::vector<uint8_t>* out) {
		out->assign(h.size, 0);
		if (!h.packed) {
			if (h.size) {
				memcpy(&(*out)[0], stored, h.size);
			}
			return true;
		}
		return h.size == 0 || UnpackLzss(stored, h.packed, &(*out)[0], h.size);
	}

	// Goes through an archive's entries in order, from a source of its bytes: src.Read(to, n) gives the next n
	// bytes (false when there are not that many left) and src.Skip(n) passes over n (past the end is no error:
	// the next Read says so). want(name, nameLength) says whether an entry's data is asked for; got(name,
	// nameLength, head, stored) is given it as the archive has it (Stored(head) bytes, for Unpack) and answers
	// whether to go on. False when the source is not an archive; true otherwise, however far it came (an entry
	// that cannot be read ends it).
	template <class Source, class Want, class Got>
	bool WalkPre(Source& src, Want want, Got got) {
		uint8_t head[16];
		uint32_t count = 0;
		if (!src.Read(head, 12) || !ReadPreHead(head, &count)) {
			return false;
		}
		std::vector<char> name;
		std::vector<uint8_t> stored;
		for (uint32_t e = 0; e < count; e++) {
			EntryHead h;
			if (!src.Read(head, 16) || !ReadEntryHead(head, &h)) {
				break;
			}
			name.resize(h.nameLength);
			if (!src.Read(&name[0], h.nameLength)) {
				break;
			}
			const uint32_t bytes = Stored(h);
			if (!want(&name[0], (size_t)h.nameLength)) {
				if (!src.Skip(Padded(bytes))) {
					break;
				}
				continue;
			}
			stored.resize(bytes ? bytes : 1);
			if (bytes && !src.Read(&stored[0], bytes)) {
				break;
			}
			if (!got(&name[0], (size_t)h.nameLength, h, &stored[0]) || !src.Skip(Padded(bytes) - bytes)) {
				break;
			}
		}
		return true;
	}

	// Where pixel (x, y) is in a swizzled picture (y counted as stored: from the bottom)
	inline uint32_t Swizzled(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
		uint32_t out = 0, bit = 0;
		for (uint32_t sx = 1, sy = 1; sx < w || sy < h;) {
			if (sx < w) {
				out |= (x & sx ? 1u : 0u) << bit++;
				sx <<= 1;
			}
			if (sy < h) {
				out |= (y & sy ? 1u : 0u) << bit++;
				sy <<= 1;
			}
		}
		return out;
	}

	struct Picture {
		uint32_t w, h;
		std::vector<uint8_t> bgra;      // B, G, R, A per pixel, the top row first (D3DFMT_A8R8G8B8's order)
	};

	inline bool PowerOfTwo(uint32_t v) {
		return v && !(v & (v - 1));
	}

	// An .img.xbx as a picture; false for any other kind (or a file that does not add up)
	inline bool DecodeImg(const uint8_t* d, size_t n, Picture* out) {
		if (n < 32 || U32(d) != 2) {
			return false;
		}
		const uint32_t w = U32(d + 8), h = U32(d + 12), paletteSize = U32(d + 28);
		if (!PowerOfTwo(w) || !PowerOfTwo(h) || w > 1024 || h > 1024) {
			return false;
		}
		const size_t pixels = (size_t)w * h;
		const uint8_t* palette = d + 32;
		const uint8_t* data = palette + paletteSize;
		out->w = w;
		out->h = h;
		out->bgra.assign(pixels * 4, 0);
		if (paletteSize == 64 || paletteSize == 1024) {
			if (n != 32 + paletteSize + pixels) {
				return false;
			}
			const uint32_t colours = paletteSize / 4;
			for (uint32_t y = 0; y < h; y++) {
				uint8_t* row = &out->bgra[(size_t)(h - 1 - y) * w * 4];
				for (uint32_t x = 0; x < w; x++) {
					const uint8_t index = data[Swizzled(x, y, w, h)];
					if (index >= colours) {
						return false;
					}
					memcpy(row + 4 * x, palette + 4 * index, 4);
				}
			}
			return true;
		}
		if (paletteSize == 0) {
			if (n != 32 + pixels * 4) {
				return false;
			}
			for (uint32_t y = 0; y < h; y++) {
				uint8_t* row = &out->bgra[(size_t)(h - 1 - y) * w * 4];
				for (uint32_t x = 0; x < w; x++) {
					memcpy(row + 4 * x, data + 4 * (size_t)Swizzled(x, y, w, h), 4);
				}
			}
			return true;
		}
		return false;
	}
}
