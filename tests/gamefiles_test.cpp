// Tests for gamefiles.h: the game's LZSS, its .pre archives and its .img.xbx pictures.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src tests/gamefiles_test.cpp -o gamefiles_test
// With the path of the game's skaterparts.pre as an argument it also reads every logo in it.

#include "gamefiles.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int s_checks = 0;
static int s_failed = 0;

static void Check(bool ok, const char* what) {
	s_checks++;
	if (!ok) {
		s_failed++;
		std::printf("FAILED: %s\n", what);
	}
}

// Made with the packer the level tools use (io_thug1/pre.py), whose files the game reads
static const uint8_t kPlain[] = { 0x54, 0x68, 0x65, 0x20, 0x6C, 0x6F, 0x67, 0x6F, 0x73, 0x20, 0x6F, 0x6E, 0x20, 0x61, 0x20, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x27, 0x73, 0x20, 0x63, 0x61, 0x72, 0x64, 0x20, 0x63, 0x6F, 0x6D, 0x65, 0x20, 0x66, 0x72, 0x6F, 0x6D, 0x20, 0x74, 0x68, 0x65, 0x20, 0x67, 0x61, 0x6D, 0x65, 0x27, 0x73, 0x20, 0x6F, 0x77, 0x6E, 0x20, 0x66, 0x69, 0x6C, 0x65, 0x73, 0x2E, 0x20, 0x54, 0x68, 0x65, 0x20, 0x6C, 0x6F, 0x67, 0x6F, 0x73, 0x20, 0x6F, 0x6E, 0x20, 0x61, 0x20, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x27, 0x73, 0x20, 0x63, 0x61, 0x72, 0x64, 0x20, 0x63, 0x6F, 0x6D, 0x65, 0x20, 0x66, 0x72, 0x6F, 0x6D, 0x20, 0x74, 0x68, 0x65, 0x20, 0x67, 0x61, 0x6D, 0x65, 0x27, 0x73, 0x20, 0x6F, 0x77, 0x6E, 0x20, 0x66, 0x69, 0x6C, 0x65, 0x73, 0x2E, 0x20, 0x54, 0x68, 0x65, 0x20, 0x6C, 0x6F, 0x67, 0x6F, 0x73, 0x20, 0x6F, 0x6E, 0x20, 0x61, 0x20, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x27, 0x73, 0x20, 0x63, 0x61, 0x72, 0x64, 0x20, 0x63, 0x6F, 0x6D, 0x65, 0x20, 0x66, 0x72, 0x6F, 0x6D, 0x20, 0x74, 0x68, 0x65, 0x20, 0x67, 0x61, 0x6D, 0x65, 0x27, 0x73, 0x20, 0x6F, 0x77, 0x6E, 0x20, 0x66, 0x69, 0x6C, 0x65, 0x73, 0x2E, 0x20, 0x54, 0x68, 0x65, 0x20, 0x6C, 0x6F, 0x67, 0x6F, 0x73, 0x20, 0x6F, 0x6E, 0x20, 0x61, 0x20, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x27, 0x73, 0x20, 0x63, 0x61, 0x72, 0x64, 0x20, 0x63, 0x6F, 0x6D, 0x65, 0x20, 0x66, 0x72, 0x6F, 0x6D, 0x20, 0x74, 0x68, 0x65, 0x20, 0x67, 0x61, 0x6D, 0x65, 0x27, 0x73, 0x20, 0x6F, 0x77, 0x6E, 0x20, 0x66, 0x69, 0x6C, 0x65, 0x73, 0x2E, 0x20, 0x54, 0x68, 0x65, 0x20, 0x6C, 0x6F, 0x67, 0x6F, 0x73, 0x20, 0x6F, 0x6E, 0x20, 0x61, 0x20, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x27, 0x73, 0x20, 0x63, 0x61, 0x72, 0x64, 0x20, 0x63, 0x6F, 0x6D, 0x65, 0x20, 0x66, 0x72, 0x6F, 0x6D, 0x20, 0x74, 0x68, 0x65, 0x20, 0x67, 0x61, 0x6D, 0x65, 0x27, 0x73, 0x20, 0x6F, 0x77, 0x6E, 0x20, 0x66, 0x69, 0x6C, 0x65, 0x73, 0x2E, 0x20, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x74, 0x61, 0x69, 0x6C };
static const uint8_t kPacked[] = { 0xFF, 0x54, 0x68, 0x65, 0x20, 0x6C, 0x6F, 0x67, 0x6F, 0xFF, 0x73, 0x20, 0x6F, 0x6E, 0x20, 0x61, 0x20, 0x70, 0xFF, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x27, 0x73, 0x20, 0xFF, 0x63, 0x61, 0x72, 0x64, 0x20, 0x63, 0x6F, 0x6D, 0xFF, 0x65, 0x20, 0x66, 0x72, 0x6F, 0x6D, 0x20, 0x74, 0xDE, 0xEF, 0xF0, 0x67, 0x61, 0x6D, 0x65, 0x03, 0x00, 0x6F, 0x77, 0xFF, 0x6E, 0x20, 0x66, 0x69, 0x6C, 0x65, 0x73, 0x2E, 0x01, 0x20, 0xEE, 0xFF, 0x00, 0x0F, 0x12, 0x0F, 0x24, 0x0F, 0x36, 0x0F, 0x48, 0x0F, 0x5A, 0x0F, 0x80, 0x6C, 0x0F, 0x7E, 0x0F, 0x90, 0x0F, 0xA2, 0x0F, 0xB4, 0x0F, 0xC6, 0x0F, 0xD8, 0x07, 0x00, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0xFF, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0xFF, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0xFF, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0xFF, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0xFF, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x74, 0x07, 0x61, 0x69, 0x6C };

static void Put32(std::vector<uint8_t>& v, uint32_t x) {
	for (int i = 0; i < 4; i++) {
		v.push_back((uint8_t)(x >> (8 * i)));
	}
}

static void AddEntry(std::vector<uint8_t>& pre, const char* name, const uint8_t* stored, uint32_t storedSize, uint32_t size, bool packed) {
	const uint32_t nameLength = (uint32_t)((std::strlen(name) + 1 + 3) & ~(size_t)3);
	Put32(pre, size);
	Put32(pre, packed ? storedSize : 0);
	Put32(pre, nameLength);
	Put32(pre, 0x12345678);
	for (uint32_t i = 0; i < nameLength; i++) {
		pre.push_back(i < std::strlen(name) ? (uint8_t)name[i] : 0);
	}
	pre.insert(pre.end(), stored, stored + storedSize);
	while (pre.size() % 4) {
		pre.push_back(0);
	}
}

static void TestLzss() {
	// three bytes as they are, then nine from the ring where they went
	const uint8_t small[] = { 0x07, 'a', 'b', 'c', 0xEE, 0xF6 };
	uint8_t out[12];
	Check(GameFiles::UnpackLzss(small, sizeof(small), out, sizeof(out)), "lzss: the small one unpacks");
	Check(std::memcmp(out, "abcabcabcabc", 12) == 0, "lzss: a match that copies what it is writing");
	std::vector<uint8_t> big(sizeof(kPlain));
	Check(GameFiles::UnpackLzss(kPacked, sizeof(kPacked), &big[0], big.size()), "lzss: the packer's unpacks");
	Check(std::memcmp(&big[0], kPlain, sizeof(kPlain)) == 0, "lzss: to what was packed");
	Check(!GameFiles::UnpackLzss(kPacked, sizeof(kPacked) - 3, &big[0], big.size()), "lzss: short data is refused");
	Check(!GameFiles::UnpackLzss(small, 5, out, sizeof(out)), "lzss: a match cut in two is refused");
}

static void TestPre() {
	std::vector<uint8_t> pre;
	Put32(pre, 0);
	pre.push_back(3);
	pre.push_back(0);
	pre.push_back(0xCD);
	pre.push_back(0xAB);
	Put32(pre, 3);
	const uint8_t stored[] = { 1, 2, 3, 4, 5 };
	AddEntry(pre, "textures\\logos\\Sht_First01.img.xbx", stored, sizeof(stored), sizeof(stored), false);
	AddEntry(pre, "Textures/Logos/Sht_Packed01.img.xbx", kPacked, sizeof(kPacked), sizeof(kPlain), true);
	AddEntry(pre, "textures\\logos\\Sht_Last.img.xbx", stored, 3, 3, false);
	std::vector<uint8_t> out;
	Check(GameFiles::FindInPre(&pre[0], pre.size(), "TEXTURES\\LOGOS\\sht_first01.img.xbx", &out), "pre: a stored entry, any case");
	Check(out.size() == 5 && std::memcmp(&out[0], stored, 5) == 0, "pre: its bytes");
	Check(GameFiles::FindInPre(&pre[0], pre.size(), "textures\\logos\\Sht_Packed01.img.xbx", &out), "pre: a packed entry, / for \\");
	Check(out.size() == sizeof(kPlain) && std::memcmp(&out[0], kPlain, sizeof(kPlain)) == 0, "pre: unpacked");
	Check(GameFiles::FindInPre(&pre[0], pre.size(), "textures\\logos\\Sht_Last.img.xbx", &out) && out.size() == 3, "pre: the last entry");
	Check(!GameFiles::FindInPre(&pre[0], pre.size(), "textures\\logos\\Sht_First01.img", &out), "pre: a name that is only the start");
	Check(!GameFiles::FindInPre(&pre[0], pre.size(), "textures\\logos\\Sht_First01.img.xbx.old", &out), "pre: a longer name");
	Check(!GameFiles::FindInPre(&pre[0], pre.size() - 8, "textures\\logos\\Sht_Last.img.xbx", &out), "pre: cut short");
	std::vector<uint8_t> bad = pre;
	bad[6] = 0;
	Check(!GameFiles::FindInPre(&bad[0], bad.size(), "textures\\logos\\Sht_First01.img.xbx", &out), "pre: not a .pre");
}

static std::vector<uint8_t> ImgHead(uint32_t w, uint32_t h, uint32_t paletteSize) {
	std::vector<uint8_t> v;
	Put32(v, 2);
	Put32(v, 0x00BD0000);
	Put32(v, w);
	Put32(v, h);
	Put32(v, 0x13);
	Put32(v, 0);
	Put32(v, w | (h << 16));
	Put32(v, paletteSize);
	return v;
}

static void TestImg() {
	// 4 by 2: the swizzle takes x's lowest bit, then y's, then x's next
	Check(GameFiles::Swizzled(1, 0, 4, 2) == 1 && GameFiles::Swizzled(0, 1, 4, 2) == 2 && GameFiles::Swizzled(2, 0, 4, 2) == 4, "swizzle: 4 by 2");
	Check(GameFiles::Swizzled(3, 1, 4, 2) == 7, "swizzle: the last of 4 by 2");
	Check(GameFiles::Swizzled(5, 3, 8, 8) == ((1 << 0) | (1 << 1) | (1 << 3) | (1 << 4)), "swizzle: square, interleaved");
	Check(GameFiles::Swizzled(0, 7, 2, 8) == ((1 << 1) | (1 << 2) | (1 << 3)), "swizzle: y's bits once x has none left");
	std::vector<uint8_t> img = ImgHead(4, 2, 64);
	for (int c = 0; c < 16; c++) {
		img.push_back((uint8_t)(10 * c));          // B
		img.push_back((uint8_t)(10 * c + 1));      // G
		img.push_back((uint8_t)(10 * c + 2));      // R
		img.push_back((uint8_t)(255 - c));         // A
	}
	std::vector<uint8_t> pixels(8);
	for (uint32_t y = 0; y < 2; y++) {
		for (uint32_t x = 0; x < 4; x++) {
			pixels[GameFiles::Swizzled(x, y, 4, 2)] = (uint8_t)(x + 4 * y);      // stored row y: colours 4y..4y+3
		}
	}
	img.insert(img.end(), pixels.begin(), pixels.end());
	GameFiles::Picture p;
	Check(GameFiles::DecodeImg(&img[0], img.size(), &p), "img: 16 colours decode");
	Check(p.w == 4 && p.h == 2 && p.bgra.size() == 32, "img: its size");
	// the top row is the one stored second
	bool top = true, bottom = true;
	for (int x = 0; x < 4; x++) {
		top = top && p.bgra[4 * x] == 10 * (4 + x) && p.bgra[4 * x + 2] == 10 * (4 + x) + 2 && p.bgra[4 * x + 3] == 255 - (4 + x);
		bottom = bottom && p.bgra[16 + 4 * x] == 10 * x && p.bgra[16 + 4 * x + 3] == 255 - x;
	}
	Check(top, "img: the top row first");
	Check(bottom, "img: the bottom row last");
	std::vector<uint8_t> wrong = img;
	wrong.back() = 16;
	Check(!GameFiles::DecodeImg(&wrong[0], wrong.size(), &p), "img: an index past the palette");
	Check(!GameFiles::DecodeImg(&img[0], img.size() - 1, &p), "img: short");
	wrong = img;
	wrong[8] = 3;
	Check(!GameFiles::DecodeImg(&wrong[0], wrong.size(), &p), "img: a width that is not a power of two");
	wrong = img;
	wrong[0] = 1;
	Check(!GameFiles::DecodeImg(&wrong[0], wrong.size(), &p), "img: another version");
	// four bytes a pixel, no palette
	std::vector<uint8_t> full = ImgHead(2, 2, 0);
	std::vector<uint8_t> px(16);
	for (uint32_t y = 0; y < 2; y++) {
		for (uint32_t x = 0; x < 2; x++) {
			const uint32_t at = GameFiles::Swizzled(x, y, 2, 2) * 4;
			px[at] = (uint8_t)(x + 2 * y);
			px[at + 3] = 200;
		}
	}
	full.insert(full.end(), px.begin(), px.end());
	Check(GameFiles::DecodeImg(&full[0], full.size(), &p) && p.bgra[0] == 2 && p.bgra[4] == 3 && p.bgra[8] == 0 && p.bgra[15] == 200, "img: 32 bits");
}

// The game's own skaterparts.pre, if given: every shirt logo in it decodes
static void TestGameFile(const char* path) {
	FILE* f = std::fopen(path, "rb");
	if (!f) {
		std::printf("FAILED: cannot open %s\n", path);
		s_failed++;
		return;
	}
	std::vector<uint8_t> all;
	uint8_t buffer[65536];
	size_t got;
	while ((got = std::fread(buffer, 1, sizeof(buffer), f)) > 0) {
		all.insert(all.end(), buffer, buffer + got);
	}
	std::fclose(f);
	static const char* const kNames[] = { "Sht_birdhouse01", "Sht_element01", "Sht_flip01", "Sht_girl01", "Sht_zero01", "Sht_hawks01",
		"Sht_vans01", "Sht_DCShoes01", "Sht_es01", "Sht_Adio01", "Sht_globe01", "Sht_circa01", "Sht_emerica01", "Sht_etnies01" };
	for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++) {
		const std::string name = std::string("textures\\logos\\") + kNames[i] + ".img.xbx";
		std::vector<uint8_t> data;
		GameFiles::Picture p;
		const bool found = GameFiles::FindInPre(&all[0], all.size(), name.c_str(), &data);
		const bool ok = found && GameFiles::DecodeImg(&data[0], data.size(), &p);
		Check(ok, name.c_str());
		if (ok) {
			uint32_t sum = 2166136261u;
			for (size_t k = 0; k < p.bgra.size(); k++) {
				sum = (sum ^ p.bgra[k]) * 16777619u;
			}
			std::printf("%s: %ux%u, %08X\n", kNames[i], p.w, p.h, sum);
		}
	}
}

int main(int argc, char** argv) {
	TestLzss();
	TestPre();
	TestImg();
	if (argc > 1) {
		TestGameFile(argv[1]);
	}
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
