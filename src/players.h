#pragma once
// Players: the skaters of others in your world, for the online mode to come. The parts that need neither
// Windows nor the game, tested by tests/players_test.cpp.
//
// Each player has an identity (who he is: a name, his sponsors, how far his story is, his stats) and a run (where
// he is and what he does, frame by frame: his place, the way he is turned, the animation he plays). A real player's
// will come over the network; for now the runs are ones you record yourself, and each plays back in a loop as a
// fake player with a made-up identity (players_game.h).
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <vector>

namespace Players {
	/******************************************************************/
	/*                                                                */
	/*  Who a player is                                               */
	/*                                                                */
	/******************************************************************/

	// The game's ten stats, as a skater profile has them (air, run, ollie, speed, spin, switch, flip_speed,
	// rail_balance, lip_balance, manual_balance)
	enum { kStats = 10, kStatMax = 10 };
	inline const char* StatName(int i) {
		static const char* const names[kStats] = { "Air", "Run", "Ollie", "Speed", "Spin", "Switch", "Flip Speed",
			"Rail Balance", "Lip Balance", "Manual Balance" };
		return i >= 0 && i < kStats ? names[i] : "";
	}

	enum Sponsor { kUnsponsored, kAm, kPro };
	inline const char* SponsorName(int s) {
		return s == kPro ? "Pro" : s == kAm ? "Am" : "Unsponsored";
	}

	struct Identity {
		char name[32];
		int sponsor;            // Sponsor
		char board[32];         // the board company he rides for ("" with none)
		char shoes[32];         // the shoe company
		char level[32];         // where his story is
		int goals;              // goals he has done
		int stats[kStats];      // 0 to kStatMax
	};

	// A number for each step of a seed: the same seed always gives the same numbers (xorshift)
	struct Dice {
		uint32_t s;
		explicit Dice(uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
		uint32_t Next() {
			s ^= s << 13;
			s ^= s >> 17;
			s ^= s << 5;
			return s;
		}
		int Below(int n) {
			return n > 0 ? (int)(Next() % (uint32_t)n) : 0;
		}
	};

	// Where a player's story is: the game's story levels in their order (Levels.q, onlinestats.q), and the
	// sponsor he has by then (the board company is chosen after Tampa, and he turns pro at Slam City Jam)
	enum { kStoryLevels = 9 };
	inline const char* StoryLevel(int i) {
		static const char* const levels[kStoryLevels] = { "New Jersey", "Manhattan", "Tampa", "San Diego", "Hawaii",
			"Vancouver", "Slam City Jam", "Moscow", "Hotter Than Hell" };
		return i >= 0 && i < kStoryLevels ? levels[i] : "";
	}

	// The logo of a company on a player's card: the picture the game puts on a shirt for it (cas_logos.q, its
	// front logos), an entry of skaterparts.pre; NULL for a company the game has no logo of. Any case.
	inline const char* LogoOf(const char* company) {
		static const char* const logos[][2] = {
			{ "Birdhouse", "textures\\logos\\Sht_birdhouse01.img.xbx" },
			{ "Element", "textures\\logos\\Sht_element01.img.xbx" },
			{ "Flip", "textures\\logos\\Sht_flip01.img.xbx" },
			{ "Girl", "textures\\logos\\Sht_girl01.img.xbx" },
			{ "Zero", "textures\\logos\\Sht_zero01.img.xbx" },
			{ "Hawk Shoes", "textures\\logos\\Sht_hawks01.img.xbx" },
			{ "Vans", "textures\\logos\\Sht_vans01.img.xbx" },
			{ "DC Shoes", "textures\\logos\\Sht_DCShoes01.img.xbx" },
			{ "eS", "textures\\logos\\Sht_es01.img.xbx" },
			{ "Adio", "textures\\logos\\Sht_Adio01.img.xbx" },
			{ "Globe", "textures\\logos\\Sht_globe01.img.xbx" },
			{ "Circa", "textures\\logos\\Sht_circa01.img.xbx" },
			{ "Emerica", "textures\\logos\\Sht_emerica01.img.xbx" },
			{ "Etnies", "textures\\logos\\Sht_etnies01.img.xbx" },
		};
		for (size_t i = 0; i < sizeof(logos) / sizeof(logos[0]); i++) {
			const char* a = logos[i][0];
			const char* b = company;
			while (*a && *b && (*a | 0x20) == (*b | 0x20)) {
				a++;
				b++;
			}
			if (!*a && !*b) {
				return logos[i][1];
			}
		}
		return NULL;
	}

	// A made-up player for a run: the same seed always has the same one. The names are made up (online names,
	// as a player has); the board companies are the story's sponsors (select_sponsor.q), the shoe companies the
	// pros' (skater_profile.q) that the game has a logo of, and DC Shoes in the place of Axion, which it has not.
	inline Identity FakeIdentity(uint32_t seed) {
		static const char* const names[] = { "DexRivera", "kaia_b", "milo.vance", "RosaDelgado", "teo_lq", "junpark",
			"AvaMercer", "nico_r", "samoduya", "LenaFox", "ozzyk", "ivy.tran", "RafeMoreno", "cleo_b", "kwame_a",
			"TessHollis" };
		static const char* const boards[] = { "Birdhouse", "Element", "Flip", "Girl", "Zero" };
		static const char* const shoes[] = { "Hawk Shoes", "Vans", "DC Shoes", "eS", "Adio", "Globe", "Circa", "Emerica",
			"Etnies" };
		Dice d(seed * 2654435761u + 1u);
		Identity id;
		memset(&id, 0, sizeof(id));
		strncpy(id.name, names[d.Below((int)(sizeof(names) / sizeof(names[0])))], sizeof(id.name) - 1);
		id.sponsor = d.Below(3);
		if (id.sponsor != kUnsponsored) {
			strncpy(id.board, boards[d.Below((int)(sizeof(boards) / sizeof(boards[0])))], sizeof(id.board) - 1);
			strncpy(id.shoes, shoes[d.Below((int)(sizeof(shoes) / sizeof(shoes[0])))], sizeof(id.shoes) - 1);
		}
		// three story levels for each sponsor level: further on, the more he is sponsored
		const int stage = id.sponsor * 3 + d.Below(3);
		strncpy(id.level, StoryLevel(stage), sizeof(id.level) - 1);
		id.goals = 4 + stage * 12 + d.Below(10);
		const int base = 3 + id.sponsor * 2;                         // 3, 5, 7: a created skater starts at 3
		for (int i = 0; i < kStats; i++) {
			const int v = base + d.Below(4);
			id.stats[i] = v > kStatMax ? kStatMax : v;
		}
		return id;
	}

	// A seed from a name (FNV-1a): a run's file name gives its player
	inline uint32_t SeedOf(const char* s) {
		uint32_t h = 2166136261u;
		for (; s && *s; s++) {
			h = (h ^ (uint8_t)*s) * 16777619u;
		}
		return h;
	}

	/******************************************************************/
	/*                                                                */
	/*  What a player does: a run                                     */
	/*                                                                */
	/******************************************************************/

	// How the animation plays, as the game's animation channel has it: mirrored (Obj_Flip), backwards (from
	// is after to), and how it loops (Obj_PlayAnim's Cycle, PingPong, Wobble; none holds the last frame)
	enum {
		kFlipped = 1,
		kBackwards = 2,
		kLoopShift = 4,
		kLoopMask = 3 << kLoopShift,
	};
	enum { kHold = 0, kCycle = 1, kPingPong = 2, kWobble = 3 };
	inline int LoopOf(uint32_t flags) {
		return (int)((flags & kLoopMask) >> kLoopShift);
	}
	inline uint32_t LoopFlags(int loop) {
		return ((uint32_t)loop << kLoopShift) & kLoopMask;
	}

	struct Frame {
		uint32_t t;             // ms from the start of the run
		float pos[3];           // where he is
		float quat[4];          // the way he is turned (x y z w)
		uint32_t anim;          // the checksum of the animation he plays (0: none)
		float animTime;         // seconds into it, now
		float animFrom;         // from and to, in seconds, as it was played (from after to: backwards)
		float animTo;
		float animSpeed;        // 1 as made
		uint32_t flags;
	};

	struct Run {
		uint32_t level;         // the level it was recorded in (its load script's checksum)
		std::vector<Frame> frames;

		uint32_t Length() const {
			return frames.empty() ? 0 : frames.back().t;
		}
	};

	// A run in a file: "FKRN", the version, the level, the count, then the frames as they are
	static const uint32_t kRunMagic = 0x4E524B46;       // "FKRN"
	static const uint32_t kRunVersion = 1;
	static const size_t kMaxFrames = 30 * 60 * 5;       // five minutes at 30 a second

	inline void PutU32(std::vector<uint8_t>& out, uint32_t v) {
		const uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
		out.insert(out.end(), b, b + 4);
	}
	inline void PutF32(std::vector<uint8_t>& out, float f) {
		uint32_t v;
		memcpy(&v, &f, 4);
		PutU32(out, v);
	}
	inline uint32_t GetU32(const uint8_t* p) {
		return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	}
	inline float GetF32(const uint8_t* p) {
		const uint32_t v = GetU32(p);
		float f;
		memcpy(&f, &v, 4);
		return f;
	}

	enum { kFrameBytes = 4 + 12 + 16 + 4 + 16 + 4 };

	inline std::vector<uint8_t> WriteRun(const Run& r) {
		std::vector<uint8_t> out;
		out.reserve(16 + r.frames.size() * kFrameBytes);
		PutU32(out, kRunMagic);
		PutU32(out, kRunVersion);
		PutU32(out, r.level);
		PutU32(out, (uint32_t)r.frames.size());
		for (size_t i = 0; i < r.frames.size(); i++) {
			const Frame& f = r.frames[i];
			PutU32(out, f.t);
			for (int k = 0; k < 3; k++) PutF32(out, f.pos[k]);
			for (int k = 0; k < 4; k++) PutF32(out, f.quat[k]);
			PutU32(out, f.anim);
			PutF32(out, f.animTime);
			PutF32(out, f.animFrom);
			PutF32(out, f.animTo);
			PutF32(out, f.animSpeed);
			PutU32(out, f.flags);
		}
		return out;
	}

	inline bool Finite(float f) {
		return f == f && f > -1.0e7f && f < 1.0e7f;
	}

	// False for anything that is not a whole run of ours: times that go back, numbers that are not numbers
	inline bool ReadRun(const uint8_t* data, size_t size, Run* out) {
		if (!data || size < 16 || GetU32(data) != kRunMagic || GetU32(data + 4) != kRunVersion) {
			return false;
		}
		const uint32_t count = GetU32(data + 12);
		if (count < 2 || count > kMaxFrames || size != 16 + (size_t)count * kFrameBytes) {
			return false;
		}
		Run r;
		r.level = GetU32(data + 8);
		r.frames.resize(count);
		const uint8_t* p = data + 16;
		for (uint32_t i = 0; i < count; i++, p += kFrameBytes) {
			Frame& f = r.frames[i];
			f.t = GetU32(p);
			for (int k = 0; k < 3; k++) f.pos[k] = GetF32(p + 4 + 4 * k);
			for (int k = 0; k < 4; k++) f.quat[k] = GetF32(p + 16 + 4 * k);
			f.anim = GetU32(p + 32);
			f.animTime = GetF32(p + 36);
			f.animFrom = GetF32(p + 40);
			f.animTo = GetF32(p + 44);
			f.animSpeed = GetF32(p + 48);
			f.flags = GetU32(p + 52);
			bool ok = Finite(f.animTime) && Finite(f.animFrom) && Finite(f.animTo) && Finite(f.animSpeed);
			for (int k = 0; k < 3; k++) ok = ok && Finite(f.pos[k]);
			for (int k = 0; k < 4; k++) ok = ok && Finite(f.quat[k]);
			if (!ok || (i > 0 && f.t <= r.frames[i - 1].t)) {
				return false;
			}
		}
		*out = r;
		return true;
	}

	/******************************************************************/
	/*                                                                */
	/*  Turns                                                         */
	/*                                                                */
	/******************************************************************/

	// From the rows of the game's matrix (left, up, the way it faces) to a quaternion and back. The game's rows
	// are its axes; any one way of reading them does, so long as back is the same way.
	inline void QuatFromRows(const float r[3][3], float q[4]) {
		const float trace = r[0][0] + r[1][1] + r[2][2];
		if (trace > 0.0f) {
			const float s = sqrtf(trace + 1.0f) * 2.0f;
			q[3] = 0.25f * s;
			q[0] = (r[1][2] - r[2][1]) / s;
			q[1] = (r[2][0] - r[0][2]) / s;
			q[2] = (r[0][1] - r[1][0]) / s;
		}
		else if (r[0][0] > r[1][1] && r[0][0] > r[2][2]) {
			const float s = sqrtf(1.0f + r[0][0] - r[1][1] - r[2][2]) * 2.0f;
			q[3] = (r[1][2] - r[2][1]) / s;
			q[0] = 0.25f * s;
			q[1] = (r[1][0] + r[0][1]) / s;
			q[2] = (r[2][0] + r[0][2]) / s;
		}
		else if (r[1][1] > r[2][2]) {
			const float s = sqrtf(1.0f + r[1][1] - r[0][0] - r[2][2]) * 2.0f;
			q[3] = (r[2][0] - r[0][2]) / s;
			q[0] = (r[1][0] + r[0][1]) / s;
			q[1] = 0.25f * s;
			q[2] = (r[2][1] + r[1][2]) / s;
		}
		else {
			const float s = sqrtf(1.0f + r[2][2] - r[0][0] - r[1][1]) * 2.0f;
			q[3] = (r[0][1] - r[1][0]) / s;
			q[0] = (r[2][0] + r[0][2]) / s;
			q[1] = (r[2][1] + r[1][2]) / s;
			q[2] = 0.25f * s;
		}
		const float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
		for (int k = 0; k < 4; k++) {
			q[k] = n > 0.0f ? q[k] / n : (k == 3 ? 1.0f : 0.0f);
		}
	}

	inline void RowsFromQuat(const float q[4], float r[3][3]) {
		const float x = q[0], y = q[1], z = q[2], w = q[3];
		r[0][0] = 1.0f - 2.0f * (y * y + z * z);
		r[0][1] = 2.0f * (x * y + z * w);
		r[0][2] = 2.0f * (x * z - y * w);
		r[1][0] = 2.0f * (x * y - z * w);
		r[1][1] = 1.0f - 2.0f * (x * x + z * z);
		r[1][2] = 2.0f * (y * z + x * w);
		r[2][0] = 2.0f * (x * z + y * w);
		r[2][1] = 2.0f * (y * z - x * w);
		r[2][2] = 1.0f - 2.0f * (x * x + y * y);
	}

	// Between two turns, k of the way (the shorter way round)
	inline void Nlerp(const float a[4], const float b[4], float k, float out[4]) {
		const float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
		const float sign = dot < 0.0f ? -1.0f : 1.0f;
		float n = 0.0f;
		for (int i = 0; i < 4; i++) {
			out[i] = a[i] * (1.0f - k) + sign * b[i] * k;
			n += out[i] * out[i];
		}
		n = sqrtf(n);
		for (int i = 0; i < 4; i++) {
			out[i] = n > 0.0f ? out[i] / n : (i == 3 ? 1.0f : 0.0f);
		}
	}

	/******************************************************************/
	/*                                                                */
	/*  Playing a run back                                            */
	/*                                                                */
	/******************************************************************/

	struct Sample {
		float pos[3];
		float quat[4];
		size_t frame;           // the frame at or before the time (its animation is the one now)
		float animTime;         // that animation's time now, moved on from that frame
	};

	// An animation's time t seconds on from a frame's, as the game plays it: held at the end, round again, there
	// and back
	inline float AnimTimeAfter(const Frame& f, float seconds) {
		const float lo = f.animFrom < f.animTo ? f.animFrom : f.animTo;
		const float hi = f.animFrom < f.animTo ? f.animTo : f.animFrom;
		const float dir = (f.flags & kBackwards) ? -1.0f : 1.0f;
		float t = f.animTime + seconds * f.animSpeed * dir;
		const float len = hi - lo;
		const int loop = LoopOf(f.flags);
		if (len > 1.0e-4f && loop == kCycle) {
			t = lo + fmodf(t - lo, len);
			t = t < lo ? t + len : t;
		}
		else if (len > 1.0e-4f && loop == kPingPong) {
			float x = fmodf(t - lo, 2.0f * len);
			x = x < 0.0f ? x + 2.0f * len : x;
			t = x > len ? lo + 2.0f * len - x : lo + x;
		}
		return t < lo ? lo : (t > hi ? hi : t);
	}

	// Where the run is at t ms, round and round: the place and turn between the frames either side, the
	// animation of the frame before
	inline bool SampleRun(const Run& r, uint32_t t, Sample* out) {
		if (r.frames.size() < 2) {
			return false;
		}
		const uint32_t length = r.Length();
		const uint32_t at = length ? t % length : 0;
		size_t lo = 0, hi = r.frames.size() - 1;
		while (hi - lo > 1) {
			const size_t mid = (lo + hi) / 2;
			if (r.frames[mid].t <= at) {
				lo = mid;
			}
			else {
				hi = mid;
			}
		}
		const Frame& a = r.frames[lo];
		const Frame& b = r.frames[hi];
		const float k = b.t > a.t ? (float)(at - a.t) / (float)(b.t - a.t) : 0.0f;
		const float kk = k < 0.0f ? 0.0f : (k > 1.0f ? 1.0f : k);
		for (int i = 0; i < 3; i++) {
			out->pos[i] = a.pos[i] + (b.pos[i] - a.pos[i]) * kk;
		}
		Nlerp(a.quat, b.quat, kk, out->quat);
		out->frame = lo;
		out->animTime = AnimTimeAfter(a, (float)(at - a.t) * 0.001f);
		return true;
	}

	// Is the animation of a frame one to start anew, after the frame before? Another animation, the other foot
	// forward, the other way, or (for one that does not loop) its time back at the start: it was played again
	inline bool AnimRestarted(const Frame& before, const Frame& now) {
		if (before.anim != now.anim || ((before.flags ^ now.flags) & (kFlipped | kBackwards | kLoopMask))) {
			return true;
		}
		if (before.animFrom != now.animFrom || before.animTo != now.animTo) {
			return true;
		}
		if (LoopOf(now.flags) != kHold) {
			return false;
		}
		const float step = now.animTime - before.animTime;
		return (now.flags & kBackwards) ? step > 0.05f : step < -0.05f;
	}

	// Whether a frame of the skater goes into the run being recorded: the first, then one each kGapMs at most,
	// and any where the animation starts anew (so its start is where it was)
	static const uint32_t kGapMs = 33;
	inline bool Keep(const Run& r, const Frame& f) {
		if (r.frames.empty()) {
			return true;
		}
		const Frame& last = r.frames.back();
		return f.t > last.t && (f.t - last.t >= kGapMs || AnimRestarted(last, f));
	}

	/******************************************************************/
	/*  Which player Circle is for                                    */
	/******************************************************************/

	// The camera's right along the ground (x and z, a unit long), from the renderer's view matrix: the
	// matrix is taken whichever way round it is kept (the one that takes the eye to the middle of the view).
	// False when it has no right along the ground (the camera looks straight down) or is not numbers.
	inline bool CameraRight(const float eye[3], const float view[16], float* rx, float* rz) {
		for (int i = 0; i < 16; i++) {
			if (!Finite(view[i])) {
				return false;
			}
		}
		const float* v = view;
		const float rowsOff = fabsf(eye[0] * v[0] + eye[1] * v[4] + eye[2] * v[8] + v[12])
			+ fabsf(eye[0] * v[1] + eye[1] * v[5] + eye[2] * v[9] + v[13])
			+ fabsf(eye[0] * v[2] + eye[1] * v[6] + eye[2] * v[10] + v[14]);
		const float colsOff = fabsf(v[0] * eye[0] + v[1] * eye[1] + v[2] * eye[2] + v[3])
			+ fabsf(v[4] * eye[0] + v[5] * eye[1] + v[6] * eye[2] + v[7])
			+ fabsf(v[8] * eye[0] + v[9] * eye[1] + v[10] * eye[2] + v[11]);
		// a point times the matrix (D3D's way): the view's x is the first column (0, 4, 8); the matrix times a
		// point: its first row (0, 1, 2)
		const float x = v[0];
		const float z = rowsOff <= colsOff ? v[8] : v[2];
		const float len = sqrtf(x * x + z * z);
		if (!(len > 0.1f) || !Finite(len)) {
			return false;
		}
		*rx = x / len;
		*rz = z / len;
		return true;
	}

	// The way to someone from where one looks: 0 straight ahead, to the right positive, in radians.
	// (fx, fz) the way one looks and (rx, rz) one's right, along the ground; (dx, dz) the way to him.
	inline float Bearing(float fx, float fz, float rx, float rz, float dx, float dz) {
		return atan2f(dx * rx + dz * rz, dx * fx + dz * fz);
	}

	struct AimSettings {
		float reach;            // this near (from the edge of him), he can be anywhere around
		float aimReach;         // this near, he is in front, where the camera looks
		float cone;             // radians either side of where the camera looks
		float height;           // and no farther up or down than this
		float body;             // from his middle to his edge
		uint32_t lockMs;        // the one picked stays picked this long once nobody is in front
	};

	struct AimState {
		int target;             // -1: nobody
		uint32_t seenAt;        // when he was last one Circle could be for
	};

	// From the skater to a player (dx, dy, dz); present: the player is there
	struct AimPlayer {
		float dx, dy, dz;
		bool present;
	};

	inline bool AimCandidate(const AimPlayer& p, float fx, float fz, const AimSettings& s, float* edge, float* score) {
		if (!p.present || fabsf(p.dy) > s.height) {
			return false;
		}
		const float flat = sqrtf(p.dx * p.dx + p.dz * p.dz);
		const float away = sqrtf(flat * flat + p.dy * p.dy);
		*edge = away > s.body ? away - s.body : 0.0f;
		float angle = 0.0f;
		if (flat > 1.0f && (fx != 0.0f || fz != 0.0f)) {
			float c = (p.dx * fx + p.dz * fz) / flat;
			c = c > 1.0f ? 1.0f : c < -1.0f ? -1.0f : c;
			angle = acosf(c);
		}
		const bool ahead = angle <= s.cone && away <= s.aimReach;
		if (!ahead && *edge > s.reach) {
			return false;
		}
		const float k = s.cone > 0.0f ? angle / s.cone : 0.0f;
		*score = (*edge + 1.0f) * (1.0f + 2.0f * k * k);
		return true;
	}

	// Who Circle is for now: the best of those in front of the skater where the camera looks (fx, fz: a unit
	// along the ground) or right by him, the nearer and the straighter ahead the better; the one picked
	// before stays while he is nearly as good, and once nobody is there, for lockMs, while he is no farther
	// than twice aimReach. Returns his index, or -1; edge: how far he is from the skater (from his edge).
	inline int Aim(const AimPlayer* players, int n, float fx, float fz, uint32_t now, const AimSettings& s, AimState* st, float* edge) {
		int best = -1;
		float bestScore = 0.0f, bestEdge = 0.0f, oldScore = 0.0f, oldEdge = 0.0f;
		bool oldThere = false;
		for (int i = 0; i < n; i++) {
			float e, sc;
			if (!AimCandidate(players[i], fx, fz, s, &e, &sc)) {
				continue;
			}
			if (i == st->target) {
				oldThere = true;
				oldScore = sc;
				oldEdge = e;
			}
			if (best < 0 || sc < bestScore) {
				best = i;
				bestScore = sc;
				bestEdge = e;
			}
		}
		if (best >= 0) {
			if (oldThere && oldScore <= bestScore * 1.3f) {
				best = st->target;
				bestEdge = oldEdge;
			}
			st->target = best;
			st->seenAt = now;
			*edge = bestEdge;
			return best;
		}
		const int t = st->target;
		if (t >= 0 && t < n && players[t].present && now - st->seenAt <= s.lockMs) {
			const AimPlayer& p = players[t];
			const float away = sqrtf(p.dx * p.dx + p.dy * p.dy + p.dz * p.dz);
			if (away <= 2.0f * s.aimReach) {
				*edge = away > s.body ? away - s.body : 0.0f;
				return t;
			}
		}
		st->target = -1;
		*edge = 1.0e9f;
		return -1;
	}
}
