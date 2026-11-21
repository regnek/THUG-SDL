#pragma once
// Flickit's map: the parts that need neither Windows nor the game, tested by tests/worldmap_test.cpp.
//
// The map is a picture of the world from above (flickit_map.bin, made by the level build's make_map.py)
// with the spots of flickit_spots.txt on it. The map stays still under a cursor in the middle of the
// screen; the stick moves the map, the triggers zoom, and the spot nearest the cursor is the one picked.
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

namespace WorldMap {
	/******************************************************************/
	/*  The spots                                                     */
	/******************************************************************/

	struct Spot {
		double x, y, z;
		double heading;         // radians; 0 faces +z, pi/2 faces +x (the game's node angles)
		std::string district;
		std::string name;
	};

	inline std::string Trim(const std::string& s) {
		size_t a = 0, b = s.size();
		while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) {
			a++;
		}
		while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) {
			b--;
		}
		return s.substr(a, b - a);
	}

	inline bool Finite(double v) {
		return v == v && v < 1e9 && v > -1e9;
	}

	// "x y z heading | district | name", heading in degrees. False for comments, blank lines and anything
	// that does not read.
	inline bool ParseSpotLine(const std::string& raw, Spot* out) {
		const std::string line = Trim(raw);
		if (line.empty() || line[0] == '#') {
			return false;
		}
		const size_t bar1 = line.find('|');
		const size_t bar2 = bar1 == std::string::npos ? std::string::npos : line.find('|', bar1 + 1);
		if (bar2 == std::string::npos) {
			return false;
		}
		const std::string nums = line.substr(0, bar1);
		double v[4];
		const char* p = nums.c_str();
		for (int i = 0; i < 4; i++) {
			char* end = NULL;
			v[i] = strtod(p, &end);
			if (end == p || !Finite(v[i])) {
				return false;
			}
			p = end;
		}
		while (*p == ' ' || *p == '\t') {
			p++;
		}
		if (*p) {
			return false;       // more than four numbers
		}
		Spot s;
		s.x = v[0];
		s.y = v[1];
		s.z = v[2];
		s.heading = v[3] * 3.14159265358979323846 / 180.0;
		s.district = Trim(line.substr(bar1 + 1, bar2 - bar1 - 1));
		s.name = Trim(line.substr(bar2 + 1));
		if (s.name.empty()) {
			return false;
		}
		*out = s;
		return true;
	}

	inline std::vector<Spot> ParseSpots(const std::string& text) {
		std::vector<Spot> out;
		size_t at = 0;
		while (at < text.size()) {
			size_t nl = text.find('\n', at);
			if (nl == std::string::npos) {
				nl = text.size();
			}
			Spot s;
			if (ParseSpotLine(text.substr(at, nl - at), &s)) {
				out.push_back(s);
			}
			at = nl + 1;
		}
		return out;
	}

	inline std::string FormatSpot(const Spot& s) {
		char buf[96];
		snprintf(buf, sizeof(buf), "%.1f %.1f %.1f %.1f", s.x, s.y, s.z, s.heading * 180.0 / 3.14159265358979323846);
		return std::string(buf) + " | " + s.district + " | " + s.name;
	}

	// The name for the next spot the player marks: "Spot n", one more than the highest there is
	inline std::string NextMarkName(const std::vector<Spot>& spots) {
		int most = 0;
		for (size_t i = 0; i < spots.size(); i++) {
			int n = 0;
			if (sscanf(spots[i].name.c_str(), "Spot %d", &n) == 1 && n > most) {
				most = n;
			}
		}
		char buf[32];
		snprintf(buf, sizeof(buf), "Spot %d", most + 1);
		return buf;
	}

	/******************************************************************/
	/*  The map file                                                  */
	/******************************************************************/

	struct Glyph {
		uint32_t code;
		int16_t x, y, w, h;         // in the atlas
		int16_t xoff, yoff;         // from the pen, down from the top of the line
		int16_t advance;
	};

	struct Level {
		const uint8_t* data;
		uint32_t size;
		uint32_t w, h;
	};

	struct MapFile {
		uint32_t level;             // the load script's checksum
		float x0, z0, x1, z1;       // the world the picture covers (x to the right, z down)
		uint32_t texW, texH;        // the texture
		uint32_t imgW, imgH;        // the picture, in its top left corner
		std::vector<Level> levels;  // DXT1, largest first
		uint32_t fontW, fontH;
		const uint8_t* font;        // B G R A
		int16_t line, ascent;
		std::vector<Glyph> glyphs;
	};

	inline uint32_t Dxt1Size(uint32_t w, uint32_t h) {
		const uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4;
		return (bw ? bw : 1) * (bh ? bh : 1) * 8;
	}

	class Reader {
	public:
		Reader(const uint8_t* data, size_t size) : m_data(data), m_size(size), m_at(0), m_ok(true) {}
		bool Take(void* out, size_t n) {
			if (!m_ok || n > m_size - m_at) {
				m_ok = false;
				return false;
			}
			memcpy(out, m_data + m_at, n);
			m_at += n;
			return true;
		}
		const uint8_t* Skip(size_t n) {
			if (!m_ok || n > m_size - m_at) {
				m_ok = false;
				return NULL;
			}
			const uint8_t* p = m_data + m_at;
			m_at += n;
			return p;
		}
		uint32_t U32() {
			uint32_t v = 0;
			Take(&v, 4);
			return v;
		}
		int16_t I16() {
			int16_t v = 0;
			Take(&v, 2);
			return v;
		}
		float F32() {
			float v = 0;
			Take(&v, 4);
			return v;
		}
		bool Ok() const {
			return m_ok;
		}

	private:
		const uint8_t* m_data;
		size_t m_size, m_at;
		bool m_ok;
	};

	inline bool IsPowerOfTwo(uint32_t v) {
		return v && !(v & (v - 1));
	}

	// The pointers in out point into data, which has to stay as long as they are used.
	inline bool ParseMap(const uint8_t* data, size_t size, MapFile* out, std::string* why) {
		Reader r(data, size);
		char magic[8];
		if (!r.Take(magic, 8) || memcmp(magic, "FLKMAP1\0", 8) != 0) {
			*why = "not a Flickit map file";
			return false;
		}
		MapFile m;
		if (r.U32() != 1) {
			*why = "a map file of another version";
			return false;
		}
		m.level = r.U32();
		m.x0 = r.F32();
		m.z0 = r.F32();
		m.x1 = r.F32();
		m.z1 = r.F32();
		m.texW = r.U32();
		m.texH = r.U32();
		m.imgW = r.U32();
		m.imgH = r.U32();
		const uint32_t levels = r.U32();
		if (!r.Ok() || !(m.x1 > m.x0) || !(m.z1 > m.z0) || !IsPowerOfTwo(m.texW) || !IsPowerOfTwo(m.texH) || m.texW > 8192 || m.texH > 8192 ||
			m.imgW == 0 || m.imgH == 0 || m.imgW > m.texW || m.imgH > m.texH || levels == 0 || levels > 14) {
			*why = "the map file's header does not hold together";
			return false;
		}
		uint32_t w = m.texW, h = m.texH;
		for (uint32_t i = 0; i < levels; i++) {
			Level lv;
			lv.size = r.U32();
			lv.w = w;
			lv.h = h;
			if (lv.size != Dxt1Size(w, h)) {
				*why = "a picture level of the wrong size";
				return false;
			}
			lv.data = r.Skip(lv.size);
			if (!lv.data) {
				*why = "the map file ends early";
				return false;
			}
			m.levels.push_back(lv);
			w = w > 1 ? w / 2 : 1;
			h = h > 1 ? h / 2 : 1;
		}
		m.fontW = r.U32();
		m.fontH = r.U32();
		if (!r.Ok() || !IsPowerOfTwo(m.fontW) || !IsPowerOfTwo(m.fontH) || m.fontW > 4096 || m.fontH > 4096) {
			*why = "the font's size does not hold together";
			return false;
		}
		m.font = r.Skip((size_t)m.fontW * m.fontH * 4);
		m.line = r.I16();
		m.ascent = r.I16();
		const uint32_t glyphs = r.U32();
		if (!m.font || !r.Ok() || glyphs > 1024) {
			*why = "the map file ends early";
			return false;
		}
		if (m.line <= 0 || m.ascent < 0 || m.ascent > m.line) {
			*why = "the font's line does not hold together";
			return false;
		}
		for (uint32_t i = 0; i < glyphs; i++) {
			Glyph g;
			g.code = r.U32();
			g.x = r.I16();
			g.y = r.I16();
			g.w = r.I16();
			g.h = r.I16();
			g.xoff = r.I16();
			g.yoff = r.I16();
			g.advance = r.I16();
			r.I16();
			if (!r.Ok() || g.x < 0 || g.y < 0 || g.w < 0 || g.h < 0 || (uint32_t)(g.x + g.w) > m.fontW || (uint32_t)(g.y + g.h) > m.fontH) {
				*why = "a letter outside the font";
				return false;
			}
			m.glyphs.push_back(g);
		}
		*out = m;
		return true;
	}

	inline const Glyph* FindGlyph(const MapFile& m, uint32_t code) {
		for (size_t i = 0; i < m.glyphs.size(); i++) {
			if (m.glyphs[i].code == code) {
				return &m.glyphs[i];
			}
		}
		return NULL;
	}

	// The width of a line of text in the font's pixels (letters not in the font count as '?')
	inline double TextWidth(const MapFile& m, const std::string& text) {
		double w = 0;
		for (size_t i = 0; i < text.size(); i++) {
			const Glyph* g = FindGlyph(m, (uint8_t)text[i]);
			if (!g) {
				g = FindGlyph(m, '?');
			}
			if (g) {
				w += g->advance;
			}
		}
		return w;
	}

	/******************************************************************/
	/*  What is on the screen                                         */
	/******************************************************************/

	struct View {
		double cx, cz;      // the world under the cursor, in the middle of the screen
		double upp;         // world units a pixel
	};

	// World units a pixel at the closest, on a screen 1080 lines high (the picture has 12 a texel: closer
	// than this it is only a blur)
	static const double kMostIn = 4.0;

	inline double MostIn(double sh) {
		return kMostIn * 1080.0 / (sh > 1 ? sh : 1);
	}

	// The whole world on the screen, with a margin
	inline double MostOut(const MapFile& m, double sw, double sh) {
		const double a = (m.x1 - m.x0) / (sw > 1 ? sw : 1), b = (m.z1 - m.z0) / (sh > 1 ? sh : 1);
		return (a > b ? a : b) * 1.1;
	}

	inline void Clamp(View* v, const MapFile& m, double sw, double sh) {
		const double in = MostIn(sh), whole = MostOut(m, sw, sh), out = whole > in ? whole : in;
		if (!(v->upp >= in)) {
			v->upp = in;
		}
		if (v->upp > out) {
			v->upp = out;
		}
		if (!(v->cx >= m.x0)) {
			v->cx = m.x0;
		}
		if (v->cx > m.x1) {
			v->cx = m.x1;
		}
		if (!(v->cz >= m.z0)) {
			v->cz = m.z0;
		}
		if (v->cz > m.z1) {
			v->cz = m.z1;
		}
	}

	inline void ToScreen(const View& v, double sw, double sh, double x, double z, double* sx, double* sy) {
		*sx = sw * 0.5 + (x - v.cx) / v.upp;
		*sy = sh * 0.5 + (z - v.cz) / v.upp;
	}

	// The spot nearest the cursor, within radius pixels; -1 for none
	inline int SpotAtCursor(const std::vector<Spot>& spots, const View& v, double radius) {
		int best = -1;
		double bestD = radius * radius;
		for (size_t i = 0; i < spots.size(); i++) {
			const double dx = (spots[i].x - v.cx) / v.upp, dz = (spots[i].z - v.cz) / v.upp;
			const double d = dx * dx + dz * dz;
			if (d <= bestD) {
				bestD = d;
				best = (int)i;
			}
		}
		return best;
	}

	// The spot X goes to: the one after the spot picked, or with none picked the one nearest the cursor
	inline int NextSpot(const std::vector<Spot>& spots, const View& v, int picked) {
		if (spots.empty()) {
			return -1;
		}
		if (picked >= 0 && picked < (int)spots.size()) {
			return (picked + 1) % (int)spots.size();
		}
		int best = 0;
		double bestD = -1;
		for (size_t i = 0; i < spots.size(); i++) {
			const double dx = spots[i].x - v.cx, dz = spots[i].z - v.cz, d = dx * dx + dz * dz;
			if (bestD < 0 || d < bestD) {
				bestD = d;
				best = (int)i;
			}
		}
		return best;
	}

	// The middle of each district's spots, for its name on the map
	struct DistrictMark {
		std::string name;
		double x, z;
		int index;          // its colour: the order the districts first come in
	};

	inline std::vector<DistrictMark> Districts(const std::vector<Spot>& spots) {
		std::vector<DistrictMark> out;
		std::vector<int> counts;
		for (size_t i = 0; i < spots.size(); i++) {
			size_t k = 0;
			while (k < out.size() && out[k].name != spots[i].district) {
				k++;
			}
			if (k == out.size()) {
				DistrictMark d;
				d.name = spots[i].district;
				d.x = d.z = 0;
				d.index = (int)k;
				out.push_back(d);
				counts.push_back(0);
			}
			out[k].x += spots[i].x;
			out[k].z += spots[i].z;
			counts[k]++;
		}
		for (size_t k = 0; k < out.size(); k++) {
			out[k].x /= counts[k];
			out[k].z /= counts[k];
		}
		return out;
	}

	inline int DistrictIndex(const std::vector<DistrictMark>& ds, const std::string& name) {
		for (size_t k = 0; k < ds.size(); k++) {
			if (ds[k].name == name) {
				return ds[k].index;
			}
		}
		return -1;
	}

	/******************************************************************/
	/*  The controls                                                  */
	/******************************************************************/

	// One frame of the controller and keyboard, as read
	struct Pad {
		float lx, ly;                   // the left stick, -1..1 (down and right positive)
		float lt, rt;                   // the triggers, 0..1
		bool up, down, left, right;     // the d-pad, or the arrow keys
		bool a, b, x, y, back, start;   // and the keys that do the same
	};

	struct Actions {
		bool travel, close, mark, next;
	};

	static const float kDeadzone = 0.22f;
	static const double kPanPerSecond = 1100.0;     // screen pixels a second at full stick, at 1080 lines
	static const double kZoomPerSecond = 3.0;       // a full trigger for a second: this much closer

	class Controls {
	public:
		Controls() {
			Reset();
		}
		// Buttons down when the map opens count only after they have been let go: a press is a frame down
		// after a frame up, and it starts as if every button were down
		void Reset() {
			m_was[0] = m_was[1] = m_was[2] = m_was[3] = m_was[4] = m_was[5] = true;
			m_glide = false;
		}
		// The map slides over to put this under the cursor, until it is there or the stick moves it
		void GlideTo(double x, double z) {
			m_glide = true;
			m_gx = x;
			m_gz = z;
		}
		bool Gliding() const {
			return m_glide;
		}
		Actions Frame(const Pad& pad, double dt, View* v, const MapFile& m, double sw, double sh) {
			const double ui = sh / 1080.0;
			double mx = Stick(pad.lx), my = Stick(pad.ly);
			if (pad.left) mx = -1;
			if (pad.right) mx = 1;
			if (pad.up) my = -1;
			if (pad.down) my = 1;
			if (dt > 0.1) {
				dt = 0.1;           // a long frame (a load, a hitch) does not throw the map across
			}
			if (dt < 0) {
				dt = 0;
			}
			if (mx != 0 || my != 0) {
				m_glide = false;
			}
			v->cx += mx * kPanPerSecond * ui * dt * v->upp;
			v->cz += my * kPanPerSecond * ui * dt * v->upp;
			if (m_glide) {
				// most of the way in a quarter of a second, then the last of it
				const double k = 1.0 - pow(0.0001, dt * 4.0);
				v->cx += (m_gx - v->cx) * k;
				v->cz += (m_gz - v->cz) * k;
				const double dx = (m_gx - v->cx) / v->upp, dz = (m_gz - v->cz) / v->upp;
				if (dx * dx + dz * dz < 0.25) {
					v->cx = m_gx;
					v->cz = m_gz;
					m_glide = false;
				}
			}
			const double zoom = (double)pad.rt - (double)pad.lt;
			if (zoom != 0.0) {
				v->upp /= pow(kZoomPerSecond, zoom * dt);
			}
			Clamp(v, m, sw, sh);
			// (every button's edge is looked at on every frame, so none of them is evaluated lazily)
			Actions a;
			a.travel = Edge(0, pad.a);
			const bool b = Edge(1, pad.b), back = Edge(2, pad.back), start = Edge(3, pad.start);
			a.close = b || back || start;
			a.next = Edge(4, pad.x);
			a.mark = Edge(5, pad.y);
			return a;
		}

	private:
		static double Stick(float v) {
			const float a = v < 0 ? -v : v;
			if (a < kDeadzone) {
				return 0.0;
			}
			const double s = (a - kDeadzone) / (1.0f - kDeadzone);
			return (v < 0 ? -s : s) * s;        // gentle near the middle
		}
		bool Edge(int i, bool down) {
			const bool fire = down && !m_was[i];
			m_was[i] = down;
			return fire;
		}
		bool m_was[6];
		bool m_glide;
		double m_gx, m_gz;
	};

	// The button that opens the map, when it is the one that also turns the camera: held, it opens the map;
	// let go before then, the game gets its press after all, for a few frames.
	class HoldGate {
	public:
		explicit HoldGate(uint32_t holdMs = 400) : m_hold(holdMs), m_was(false), m_since(0), m_spent(false), m_pulse(0) {}
		void SetHold(uint32_t ms) {
			m_hold = ms;
		}
		// pass: the game should see the button down this frame; open: open the map now
		void Frame(bool down, uint32_t now, bool* pass, bool* open) {
			*pass = false;
			*open = false;
			if (m_pulse > 0) {
				m_pulse--;
				*pass = true;
			}
			if (down && !m_was) {
				m_since = now;
				m_spent = false;
			}
			if (down && !m_spent && now - m_since >= m_hold) {
				m_spent = true;
				*open = true;
			}
			if (!down && m_was && !m_spent) {
				m_pulse = kPulse - 1;
				*pass = true;
			}
			m_was = down;
		}
		// After the map: a button still down when it closed opens nothing and passes nothing when let go
		void Spend(bool down) {
			m_was = down;
			m_spent = true;
			m_pulse = 0;
		}
		// How far through the hold it is, 0..1 (0 when not held)
		double Charge(bool down, uint32_t now) const {
			if (!down || !m_was || m_spent || m_hold == 0) {
				return 0.0;
			}
			const double c = (double)(now - m_since) / m_hold;
			return c > 1.0 ? 1.0 : c;
		}
		static const int kPulse = 3;

	private:
		uint32_t m_hold;
		bool m_was;
		uint32_t m_since;
		bool m_spent;
		int m_pulse;
	};
}
