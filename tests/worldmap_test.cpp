// Tests for worldmap.h: the spots file, the map file, the view, the controls and the button that opens it.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src/Flickit tests/worldmap_test.cpp -o worldmap_test

#include "worldmap.h"

#include <cmath>
#include <cstdio>
#include <cstring>
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

static bool Near(double a, double b, double e = 1e-6) {
	return std::fabs(a - b) <= e;
}

static void TestSpots() {
	WorldMap::Spot s;
	Check(WorldMap::ParseSpotLine("467.2 -42.0 2922.9 73.8 | New Jersey | Jersey start", &s), "a spot reads");
	Check(Near(s.x, 467.2) && Near(s.y, -42.0) && Near(s.z, 2922.9) && Near(s.heading, 73.8 * 3.14159265358979 / 180.0, 1e-9),
		"its numbers, the heading in radians");
	Check(s.district == "New Jersey" && s.name == "Jersey start", "its district and name, trimmed");
	Check(WorldMap::ParseSpotLine("  1 2 3 0 |Tampa|  Bro Bowl  \r", &s) && s.name == "Bro Bowl" && s.district == "Tampa", "spaces and a CR at the end");
	Check(!WorldMap::ParseSpotLine("# 1 2 3 4 | a | b", &s), "a comment is not a spot");
	Check(!WorldMap::ParseSpotLine("   ", &s), "a blank line is not a spot");
	Check(!WorldMap::ParseSpotLine("1 2 3 4 | only a district", &s), "one bar is not enough");
	Check(!WorldMap::ParseSpotLine("1 2 3 | a | b", &s), "three numbers are not enough");
	Check(!WorldMap::ParseSpotLine("1 2 3 4 5 | a | b", &s), "five numbers are too many");
	Check(!WorldMap::ParseSpotLine("1 2 x 4 | a | b", &s), "a word where a number goes");
	Check(!WorldMap::ParseSpotLine("1 2 3 4 | a |   ", &s), "no name");
	Check(!WorldMap::ParseSpotLine("1e300 2 3 4 | a | b", &s), "a number out of the world");
	Check(WorldMap::ParseSpotLine("1 2 3 4 | a | b | c", &s) && s.name == "b | c", "a bar in the name stays in the name");

	const std::vector<WorldMap::Spot> all = WorldMap::ParseSpots(
		"# header\n\n1 2 3 0 | A | one\r\nbad line\n4 5 6 90 | B | two\n7 8 9 180 | A | three");
	Check(all.size() == 3 && all[0].name == "one" && all[1].name == "two" && all[2].name == "three", "a file: the good lines, in order");
	Check(Near(all[1].heading, 3.14159265358979 / 2, 1e-9), "and the last line with no newline");

	WorldMap::Spot back;
	Check(WorldMap::ParseSpotLine(WorldMap::FormatSpot(all[1]), &back) && back.name == "two" && back.district == "B" && Near(back.x, 4)
		&& Near(back.heading, all[1].heading, 1e-3), "a spot written out reads back");

	std::vector<WorldMap::Spot> mine;
	Check(WorldMap::NextMarkName(mine) == "Spot 1", "the first marked spot");
	WorldMap::Spot m = all[0];
	m.name = "Spot 3";
	mine.push_back(m);
	m.name = "Spot 1";
	mine.push_back(m);
	Check(WorldMap::NextMarkName(mine) == "Spot 4", "one more than the highest");
	m.name = "Spot 9x";
	mine.push_back(m);
	m.name = "Spot -7";
	mine.push_back(m);
	m.name = "Spots 20";
	mine.push_back(m);
	m.name = "Spot +40";
	mine.push_back(m);
	m.name = "Spot  50";
	mine.push_back(m);
	Check(WorldMap::NextMarkName(mine) == "Spot 4", "names that only start like a marked one do not count");
	m.name = "Spot 12";
	mine.push_back(m);
	Check(WorldMap::NextMarkName(mine) == "Spot 13", "and two figures do");
}

static void PutU32(std::vector<uint8_t>& b, uint32_t v) {
	b.insert(b.end(), (uint8_t*)&v, (uint8_t*)&v + 4);
}
static void PutF32(std::vector<uint8_t>& b, float v) {
	b.insert(b.end(), (uint8_t*)&v, (uint8_t*)&v + 4);
}
static void PutI16(std::vector<uint8_t>& b, int16_t v) {
	b.insert(b.end(), (uint8_t*)&v, (uint8_t*)&v + 2);
}

// A small map file: an 8x8 texture with a 8x4 picture, levels 8x8 4x4 2x2 1x1, a 4x4 font with two letters
static std::vector<uint8_t> SmallMap(int levels = 4, uint32_t badLevel = 99, int16_t glyphX = 0) {
	std::vector<uint8_t> b;
	const char magic[8] = { 'F', 'L', 'K', 'M', 'A', 'P', '1', 0 };
	b.insert(b.end(), magic, magic + 8);
	PutU32(b, 1);
	PutU32(b, 0xF6C822D4);
	PutF32(b, -100);
	PutF32(b, -50);
	PutF32(b, 100);
	PutF32(b, 50);
	PutU32(b, 8);
	PutU32(b, 8);
	PutU32(b, 8);
	PutU32(b, 4);
	PutU32(b, levels);
	uint32_t w = 8, h = 8;
	for (int i = 0; i < levels; i++) {
		const uint32_t n = WorldMap::Dxt1Size(w, h) + ((uint32_t)i == badLevel ? 8 : 0);
		PutU32(b, n);
		b.insert(b.end(), n, (uint8_t)i);
		w = w > 1 ? w / 2 : 1;
		h = h > 1 ? h / 2 : 1;
	}
	PutU32(b, 4);
	PutU32(b, 4);
	b.insert(b.end(), 4 * 4 * 4, 0xFF);
	PutI16(b, 30);
	PutI16(b, 24);
	PutU32(b, 2);
	const uint32_t codes[2] = { 'A', '?' };
	for (int i = 0; i < 2; i++) {
		PutU32(b, codes[i]);
		PutI16(b, i == 0 ? glyphX : 0);
		PutI16(b, 0);
		PutI16(b, 2);
		PutI16(b, 3);
		PutI16(b, 1);
		PutI16(b, 2);
		PutI16(b, (int16_t)(10 + i));
		PutI16(b, 0);
	}
	return b;
}

static void TestMapFile() {
	Check(WorldMap::Dxt1Size(4096, 4096) == 8388608u && WorldMap::Dxt1Size(1, 1) == 8 && WorldMap::Dxt1Size(2, 2) == 8 && WorldMap::Dxt1Size(6, 6) == 32,
		"DXT1 sizes: 8 bytes a 4x4 block, at least one");
	std::string why;
	WorldMap::MapFile m;
	std::vector<uint8_t> b = SmallMap();
	Check(WorldMap::ParseMap(&b[0], b.size(), &m, &why), "a map file reads");
	Check(m.level == 0xF6C822D4 && m.x0 == -100 && m.z1 == 50 && m.texW == 8 && m.imgH == 4, "its header");
	Check(m.levels.size() == 4 && m.levels[0].w == 8 && m.levels[1].w == 4 && m.levels[3].w == 1 && m.levels[2].data[0] == 2, "its picture levels, largest first");
	Check(m.fontW == 4 && m.font && m.font[0] == 0xFF && m.line == 30 && m.ascent == 24 && m.glyphs.size() == 2, "its font");
	Check(WorldMap::FindGlyph(m, 'A') && WorldMap::FindGlyph(m, 'A')->advance == 10 && !WorldMap::FindGlyph(m, 'B'), "its letters");
	Check(WorldMap::TextWidth(m, "AA") == 20 && WorldMap::TextWidth(m, "AB") == 21, "text width; a letter it lacks counts as '?'");

	std::vector<uint8_t> bad = b;
	bad[0] = 'X';
	Check(!WorldMap::ParseMap(&bad[0], bad.size(), &m, &why), "not a map file");
	bad = SmallMap(4, 1);
	Check(!WorldMap::ParseMap(&bad[0], bad.size(), &m, &why), "a picture level of the wrong size");
	int read = 0;
	for (size_t cut = 0; cut < b.size(); cut += 3) {
		read += WorldMap::ParseMap(&b[0], cut, &m, &why);
	}
	Check(read == 0 && WorldMap::ParseMap(&b[0], b.size() - 1, &m, &why) == false, "a map file cut short is turned down, wherever it is cut");
	bad = SmallMap(4, 99, 3);
	Check(!WorldMap::ParseMap(&bad[0], bad.size(), &m, &why), "a letter outside the font");
	bad = SmallMap(0);
	Check(!WorldMap::ParseMap(&bad[0], bad.size(), &m, &why), "no picture at all");
	{
		// the line height is after the levels and the font: 52 + (4+32) + 3 * (4+8) + 8 + 64
		const size_t at = 52 + 36 + 36 + 8 + 64;
		bad = b;
		Check(bad[at] == 30 && bad[at + 2] == 24, "(the line height where the test thinks it is)");
		bad[at] = 0;
		Check(!WorldMap::ParseMap(&bad[0], bad.size(), &m, &why), "a font with no line height");
		bad = b;
		bad[at + 2] = 31;
		Check(!WorldMap::ParseMap(&bad[0], bad.size(), &m, &why), "a font that rises above its line");
	}
	bad = b;
	bad[16] = 0;
	bad[17] = 0;
	bad[18] = 0xC8;         // x0 = 400, past x1
	bad[19] = 0x43;
	Check(!WorldMap::ParseMap(&bad[0], bad.size(), &m, &why), "a world box inside out");
	bad = b;
	bad[28] = bad[29] = 0;  // z1 = NaN
	bad[30] = 0xC0;
	bad[31] = 0x7F;
	Check(!WorldMap::ParseMap(&bad[0], bad.size(), &m, &why), "a world box that is not a number");
	bad = b;
	bad[32] = 12;           // texW 12
	Check(!WorldMap::ParseMap(&bad[0], bad.size(), &m, &why), "a texture that is not a power of two");
	bad = b;
	bad[40] = 16;           // imgW 16, wider than its texture
	Check(!WorldMap::ParseMap(&bad[0], bad.size(), &m, &why), "a picture bigger than its texture");
}

static void TestView() {
	std::vector<uint8_t> b = SmallMap();
	std::string why;
	WorldMap::MapFile m;
	WorldMap::ParseMap(&b[0], b.size(), &m, &why);
	WorldMap::View v = { 1, 1, 50 };
	WorldMap::Clamp(&v, m, 1920, 1080);
	Check(v.upp == WorldMap::kMostIn, "a world smaller than the screen at the closest: the closest, whatever was asked");
	m.x0 = -10000;
	m.x1 = 10000;
	m.z0 = -5000;
	m.z1 = 5000;
	v.cx = 50000;
	v.cz = -50000;
	v.upp = 0.01;
	WorldMap::Clamp(&v, m, 1920, 1080);
	Check(v.cx == 10000 && v.cz == -5000, "the cursor stays on the map");
	Check(v.upp == WorldMap::kMostIn, "no closer than the closest");
	v.upp = 0.01;
	WorldMap::Clamp(&v, m, 3840, 2160);
	Check(Near(v.upp, WorldMap::kMostIn / 2, 1e-12), "the closest is as close on a screen of more pixels: half the units a pixel at twice the lines");
	v.upp = 1e6;
	WorldMap::Clamp(&v, m, 1920, 1080);
	Check(Near(v.upp, WorldMap::MostOut(m, 1920, 1080)) && Near(v.upp, (20000.0 / 1920) * 1.1, 1e-9), "no farther than the whole map on the screen");
	Check(Near(WorldMap::MostOut(m, 1000, 1000), 22.0, 1e-9) && Near(WorldMap::MostOut(m, 1000, 100), 110.0, 1e-9),
		"whichever side of the world fills its side of the screen first counts");
	v.upp = std::nan("");
	v.cx = std::nan("");
	WorldMap::Clamp(&v, m, 1920, 1080);
	Check(v.upp == WorldMap::kMostIn && v.cx == -10000, "a view that is not a number comes back");

	WorldMap::View w = { 10, 20, 2 };
	double sx, sy;
	WorldMap::ToScreen(w, 1920, 1080, 30, 0, &sx, &sy);
	Check(Near(sx, 970) && Near(sy, 530), "world to screen: the cursor is the middle, z goes down the screen");

	std::vector<WorldMap::Spot> spots = WorldMap::ParseSpots("0 0 0 0 | A | a\n100 0 0 0 | A | b\n30 0 10 0 | B | c\n");
	WorldMap::View c = { 20, 0, 1 };
	Check(WorldMap::SpotAtCursor(spots, c, 30) == 2, "the spot nearest the cursor");
	Check(WorldMap::SpotAtCursor(spots, c, 10) == -1, "none within the radius");
	c.upp = 4;
	Check(WorldMap::SpotAtCursor(spots, c, 10) == 2 && WorldMap::SpotAtCursor(spots, c, 3) == -1, "the radius is in screen pixels");
	Check(WorldMap::SpotAtCursor(std::vector<WorldMap::Spot>(), c, 1000) == -1, "no spots, none picked");

	c.cx = 95;
	c.cz = 0;
	Check(WorldMap::NextSpot(spots, c, -1) == 1, "next with none picked: the nearest, however far");
	Check(WorldMap::NextSpot(spots, c, 1) == 2 && WorldMap::NextSpot(spots, c, 2) == 0, "next after one picked: the one after it, round to the first");
	Check(WorldMap::NextSpot(spots, c, 7) == 1, "a pick that is not there counts as none");
	Check(WorldMap::NextSpot(std::vector<WorldMap::Spot>(), c, -1) == -1, "no spots, no next");

	const std::vector<WorldMap::DistrictMark> ds = WorldMap::Districts(spots);
	Check(ds.size() == 2 && ds[0].name == "A" && Near(ds[0].x, 50) && ds[1].name == "B" && ds[1].index == 1, "districts: the middle of their spots, in the order they come");
	Check(WorldMap::DistrictIndex(ds, "B") == 1 && WorldMap::DistrictIndex(ds, "C") == -1, "a district's colour index");
}

static WorldMap::Pad Idle() {
	WorldMap::Pad p;
	std::memset(&p, 0, sizeof(p));
	return p;
}

static void TestControls() {
	std::vector<uint8_t> b = SmallMap();
	std::string why;
	WorldMap::MapFile m;
	WorldMap::ParseMap(&b[0], b.size(), &m, &why);
	m.x0 = -100000;
	m.x1 = 100000;
	m.z0 = -100000;
	m.z1 = 100000;
	WorldMap::Controls c;
	WorldMap::View v = { 0, 0, 10 };
	WorldMap::Pad p = Idle();
	p.lx = 0.2f;
	c.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	Check(v.cx == 0 && v.cz == 0, "a stick inside the dead zone moves nothing");
	p.lx = 1.0f;
	c.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	Check(Near(v.cx, WorldMap::kPanPerSecond / 60 * 10, 1e-6) && v.cz == 0, "full stick right: the map moves its speed times the zoom");
	const double one = v.cx;
	v.cx = 0;
	p.lx = 0.6f;
	c.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	const double part = (0.6 - 0.22) / (1 - 0.22);
	Check(Near(v.cx, one * part * part, 1e-3), "a part of the way is gentler than its share: the square of how far past the dead zone");
	v.cx = 0;
	p.lx = -1.0f;
	c.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	Check(Near(v.cx, -one, 1e-6), "and full stick left the other way");
	v.cx = 0;
	p.lx = 1.0f;
	c.Frame(p, 1.0, &v, m, 1920, 1080);
	Check(Near(v.cx, WorldMap::kPanPerSecond * 0.1 * 10, 1e-6), "a long frame counts as a tenth of a second at most");
	v.cx = 0;
	p = Idle();
	p.up = true;
	c.Frame(p, 1.0 / 60, &v, m, 1920, 720);
	Check(v.cx == 0 && Near(v.cz, -WorldMap::kPanPerSecond * (720.0 / 1080) / 60 * 10, 1e-6), "the d-pad up: full speed, scaled to the screen");

	p = Idle();
	p.rt = 1.0f;
	v.upp = 10;
	c.Frame(p, 1.0, &v, m, 1920, 1080);
	Check(Near(v.upp, 10 / std::pow(WorldMap::kZoomPerSecond, 0.1), 1e-9), "the right trigger zooms in");
	p.rt = 0;
	p.lt = 1.0f;
	const double before = v.upp;
	c.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	Check(v.upp > before, "the left trigger zooms out");

	// the glide to a spot
	WorldMap::Controls g;
	WorldMap::View gv = { 0, 0, 10 };
	g.GlideTo(5000, -3000);
	p = Idle();
	g.Frame(p, 1.0 / 60, &gv, m, 1920, 1080);
	Check(g.Gliding() && gv.cx > 0 && gv.cx < 5000 && gv.cz < 0 && gv.cz > -3000, "a glide goes part of the way in a frame");
	int frames = 1;
	while (g.Gliding() && frames < 600) {
		g.Frame(p, 1.0 / 60, &gv, m, 1920, 1080);
		frames++;
	}
	Check(!g.Gliding() && gv.cx == 5000 && gv.cz == -3000 && frames <= 30, "and gets there, exactly, in half a second at most");
	g.GlideTo(-5000, 0);
	p.lx = 1.0f;
	const double from = gv.cx;
	g.Frame(p, 1.0 / 60, &gv, m, 1920, 1080);
	Check(!g.Gliding() && gv.cx > from, "the stick takes over from a glide");
	p = Idle();
	g.GlideTo(-5000, 0);
	g.Reset();
	g.Frame(p, 1.0 / 60, &gv, m, 1920, 1080);
	Check(!g.Gliding() && gv.cx > from, "and a reset ends one");

	// buttons held when it opened do nothing until let go
	WorldMap::Controls fresh;
	p = Idle();
	p.a = true;
	p.back = true;
	WorldMap::Actions a = fresh.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	Check(!a.travel && !a.close, "buttons down as it opens do nothing");
	a = fresh.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	Check(!a.travel && !a.close, "nor while they stay down");
	p = Idle();
	a = fresh.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	Check(!a.travel && !a.close, "nor when let go");
	p.a = true;
	a = fresh.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	Check(a.travel && !a.close, "pressed again: it travels");
	a = fresh.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	Check(!a.travel, "once a press");
	int closers = 0;
	for (int k = 0; k < 3; k++) {
		WorldMap::Controls cc;
		p = Idle();
		cc.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
		p.b = k == 0;
		p.back = k == 1;
		p.start = k == 2;
		const WorldMap::Actions got = cc.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
		closers += got.close && !got.travel && !got.next && !got.mark;
	}
	Check(closers == 3, "B, Back and Start each close it");
	WorldMap::Controls cx;
	p = Idle();
	cx.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	p.x = true;
	a = cx.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	Check(a.next && !a.mark, "X goes to the next spot");
	p = Idle();
	cx.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	p.y = true;
	a = cx.Frame(p, 1.0 / 60, &v, m, 1920, 1080);
	Check(a.mark && !a.next, "Y marks a spot");
}

static void TestHoldGate() {
	WorldMap::HoldGate g(400);
	bool pass, open;
	int passes = 0, opens = 0;
	uint32_t t = 1000;
	for (int i = 0; i < 10; i++, t += 16) {          // held 160 ms
		g.Frame(true, t, &pass, &open);
		passes += pass;
		opens += open;
	}
	Check(passes == 0 && opens == 0, "while held, short of the hold: the game sees nothing yet");
	for (int i = 0; i < 10; i++, t += 16) {
		g.Frame(false, t, &pass, &open);
		passes += pass;
		opens += open;
	}
	Check(passes == WorldMap::HoldGate::kPulse && opens == 0, "let go early: the game gets its press, for a few frames");
	passes = opens = 0;
	int openedAt = -1;
	for (int i = 0; i < 40; i++, t += 16) {          // held 640 ms
		g.Frame(true, t, &pass, &open);
		passes += pass;
		if (open) {
			opens++;
			openedAt = i;
		}
	}
	Check(opens == 1 && openedAt == 25 && passes == 0, "held long enough: the map opens, once, at the hold");
	for (int i = 0; i < 10; i++, t += 16) {
		g.Frame(false, t, &pass, &open);
		passes += pass;
	}
	Check(passes == 0, "and letting go after that passes nothing");
	g.Frame(true, t, &pass, &open);
	Check(g.Charge(true, t + 200) > 0.49 && g.Charge(true, t + 200) < 0.51 && g.Charge(false, t + 200) == 0, "how far through the hold");
	g.Spend(true);
	passes = opens = 0;
	for (int i = 0; i < 40; i++, t += 16) {
		g.Frame(i < 20, t, &pass, &open);
		passes += pass;
		opens += open;
	}
	Check(passes == 0 && opens == 0, "spent (the map closed with it down): nothing until it is pressed again");
	WorldMap::HoldGate now(0);
	now.Frame(true, 5000, &pass, &open);
	Check(open && !pass, "a hold of 0: opens on the press");
	now.Frame(false, 5016, &pass, &open);
	Check(!pass, "and never passes the press on");
}

int main() {
	TestSpots();
	TestMapFile();
	TestView();
	TestControls();
	TestHoldGate();
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
