// Tests for players.h: the made-up players, a run in a file and back, the turns, playing a run back, and the
// frames a recording keeps.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src/Flickit tests/players_test.cpp -o players_test

#include "players.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

static int s_checks = 0;
static int s_failed = 0;

static void Check(bool ok, const char* what) {
	s_checks++;
	if (!ok) {
		s_failed++;
		std::printf("FAILED: %s\n", what);
	}
}

static bool Near(float a, float b, float within = 0.001f) {
	return std::fabs(a - b) <= within;
}

static Players::Frame MakeFrame(uint32_t t, float x, uint32_t anim, float animTime, uint32_t flags = 0) {
	Players::Frame f;
	std::memset(&f, 0, sizeof(f));
	f.t = t;
	f.pos[0] = x;
	f.pos[1] = 10.0f;
	f.pos[2] = -x;
	f.quat[3] = 1.0f;
	f.anim = anim;
	f.animTime = animTime;
	f.animFrom = 0.0f;
	f.animTo = 2.0f;
	f.animSpeed = 1.0f;
	f.flags = flags;
	return f;
}

static void TestIdentity() {
	const Players::Identity a = Players::FakeIdentity(7);
	const Players::Identity b = Players::FakeIdentity(7);
	Check(std::memcmp(&a, &b, sizeof(a)) == 0, "the same seed, the same player");
	int differ = 0, sponsors[3] = { 0, 0, 0 };
	for (uint32_t seed = 1; seed <= 300; seed++) {
		const Players::Identity p = Players::FakeIdentity(seed);
		differ += std::strcmp(p.name, a.name) != 0 ? 1 : 0;
		Check(p.name[0] != 0 && p.name[sizeof(p.name) - 1] == 0, "a name, ended");
		Check(p.sponsor >= Players::kUnsponsored && p.sponsor <= Players::kPro, "a sponsor level");
		sponsors[p.sponsor]++;
		if (p.sponsor == Players::kUnsponsored) {
			Check(p.board[0] == 0 && p.shoes[0] == 0, "no sponsors without a sponsor level");
		}
		else {
			Check(p.board[0] != 0 && p.shoes[0] != 0, "a board company and a shoe company when sponsored");
			Check(Players::LogoOf(p.board) != NULL && Players::LogoOf(p.shoes) != NULL, "the game has a logo of both");
		}
		// the story level goes with the sponsor level: three each
		int stage = -1;
		for (int i = 0; i < Players::kStoryLevels; i++) {
			if (std::strcmp(p.level, Players::StoryLevel(i)) == 0) {
				stage = i;
			}
		}
		Check(stage >= 0 && stage / 3 == p.sponsor, "the story level goes with the sponsor level");
		Check(p.goals >= 4 && p.goals < 4 + 9 * 12 + 10, "goals");
		for (int i = 0; i < Players::kStats; i++) {
			Check(p.stats[i] >= 3 && p.stats[i] <= Players::kStatMax, "a stat in range");
			Check(p.stats[i] >= 3 + 2 * p.sponsor, "a pro's stats are higher");
		}
	}
	Check(differ > 200, "the names vary");
	Check(Players::LogoOf("dc shoes") != NULL && std::strcmp(Players::LogoOf("DC SHOES"), "textures\\logos\\Sht_DCShoes01.img.xbx") == 0,
		"a logo, by the company in any case");
	Check(Players::LogoOf("Axion") == NULL && Players::LogoOf("") == NULL && Players::LogoOf("Vansx") == NULL && Players::LogoOf("Van") == NULL,
		"no logo for a company the game has none of");
	Check(sponsors[0] > 50 && sponsors[1] > 50 && sponsors[2] > 50, "all three sponsor levels come up");
	Check(std::strcmp(Players::StatName(0), "Air") == 0 && std::strcmp(Players::StatName(9), "Manual Balance") == 0, "the stats' names");
	Check(std::strcmp(Players::StatName(10), "") == 0 && std::strcmp(Players::StoryLevel(-1), "") == 0, "nothing out of range");
	Check(std::strcmp(Players::SponsorName(Players::kPro), "Pro") == 0 && std::strcmp(Players::SponsorName(Players::kAm), "Am") == 0, "the sponsor levels' names");
	Check(Players::SeedOf("run_a.run") != Players::SeedOf("run_b.run") && Players::SeedOf("x") == Players::SeedOf("x"), "a seed from a name");
}

static void TestFile() {
	Players::Run r;
	r.level = 0x1234ABCD;
	for (int i = 0; i < 5; i++) {
		r.frames.push_back(MakeFrame((uint32_t)i * 33, (float)i * 3.5f, 0xAABBCCDD, 0.1f * (float)i, Players::kFlipped | Players::LoopFlags(Players::kCycle)));
	}
	const std::vector<uint8_t> bytes = Players::WriteRun(r);
	Check(bytes.size() == 16 + 5 * Players::kFrameBytes && Players::kFrameBytes == 56, "the size of a run's file");
	Players::Run back;
	Check(Players::ReadRun(&bytes[0], bytes.size(), &back), "read back");
	Check(back.level == r.level && back.frames.size() == 5, "its level and frames");
	bool same = true;
	for (size_t i = 0; i < 5; i++) {
		same = same && std::memcmp(&back.frames[i], &r.frames[i], sizeof(Players::Frame)) == 0;
	}
	Check(same, "the frames as they were");
	Check(back.Length() == 132, "its length");

	std::vector<uint8_t> bad = bytes;
	bad[0] ^= 1;
	Check(!Players::ReadRun(&bad[0], bad.size(), &back), "not a run");
	bad = bytes;
	bad[4] = 2;
	Check(!Players::ReadRun(&bad[0], bad.size(), &back), "another version");
	bad = bytes;
	bad.pop_back();
	Check(!Players::ReadRun(&bad[0], bad.size(), &back), "cut short");
	bad = bytes;
	bad[12] = 200;
	Check(!Players::ReadRun(&bad[0], bad.size(), &back), "a count that is not the size");
	bad = bytes;
	// the third frame's time before the second's
	const size_t third = 16 + 2 * Players::kFrameBytes;
	bad[third] = 1;
	bad[third + 1] = 0;
	Check(!Players::ReadRun(&bad[0], bad.size(), &back), "times that go back");
	bad = bytes;
	const uint32_t nan = 0x7FC00000;
	std::memcpy(&bad[16 + 4], &nan, 4);
	Check(!Players::ReadRun(&bad[0], bad.size(), &back), "a place that is not a number");
	Check(!Players::ReadRun(0, 0, &back) && !Players::ReadRun(&bytes[0], 10, &back), "nothing");
	Players::Run one;
	one.frames.push_back(MakeFrame(0, 0, 0, 0));
	const std::vector<uint8_t> b1 = Players::WriteRun(one);
	Check(!Players::ReadRun(&b1[0], b1.size(), &back), "a run of one frame is not one");
}

static void RowsFacing(float heading, float r[3][3]) {
	const float c = std::cos(heading), s = std::sin(heading);
	const float rows[3][3] = { { c, 0, -s }, { 0, 1, 0 }, { s, 0, c } };
	std::memcpy(r, rows, sizeof(rows));
}

static bool SameRows(const float a[3][3], const float b[3][3], float within) {
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (!Near(a[i][j], b[i][j], within)) {
				return false;
			}
		}
	}
	return true;
}

static void TestTurns() {
	// a turn of each kind back to the same rows: about y, tipped over, upside down, about all three
	const float headings[] = { 0.0f, 0.7f, 1.5707963f, 3.1415926f, -2.4f };
	for (size_t i = 0; i < sizeof(headings) / sizeof(headings[0]); i++) {
		float r[3][3], q[4], back[3][3];
		RowsFacing(headings[i], r);
		Players::QuatFromRows(r, q);
		Players::RowsFromQuat(q, back);
		Check(SameRows(r, back, 1e-4f), "a heading to a turn and back");
		Check(Near(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3], 1.0f, 1e-4f), "of length one");
	}
	// about x by 170 degrees (the trace below zero), and about z
	{
		const float a = 2.967f, c = std::cos(a), s = std::sin(a);
		const float r[3][3] = { { 1, 0, 0 }, { 0, c, s }, { 0, -s, c } };
		float q[4], back[3][3];
		Players::QuatFromRows(r, q);
		Players::RowsFromQuat(q, back);
		Check(SameRows(r, back, 1e-4f), "nearly upside down about x");
		const float rz[3][3] = { { c, s, 0 }, { -s, c, 0 }, { 0, 0, 1 } };
		Players::QuatFromRows(rz, q);
		Players::RowsFromQuat(q, back);
		Check(SameRows(rz, back, 1e-4f), "nearly upside down about z");
		const float ry[3][3] = { { c, 0, -s }, { 0, 1, 0 }, { s, 0, c } };
		Players::QuatFromRows(ry, q);
		Players::RowsFromQuat(q, back);
		Check(SameRows(ry, back, 1e-4f), "nearly backwards about y");
	}
	{
		// a turn about all three axes: rows of the product of three
		const float ax = 0.4f, ay = -1.1f, az = 2.0f;
		const float X[3][3] = { { 1, 0, 0 }, { 0, std::cos(ax), std::sin(ax) }, { 0, -std::sin(ax), std::cos(ax) } };
		const float Y[3][3] = { { std::cos(ay), 0, -std::sin(ay) }, { 0, 1, 0 }, { std::sin(ay), 0, std::cos(ay) } };
		const float Z[3][3] = { { std::cos(az), std::sin(az), 0 }, { -std::sin(az), std::cos(az), 0 }, { 0, 0, 1 } };
		float XY[3][3], r[3][3];
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				XY[i][j] = X[i][0] * Y[0][j] + X[i][1] * Y[1][j] + X[i][2] * Y[2][j];
			}
		}
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				r[i][j] = XY[i][0] * Z[0][j] + XY[i][1] * Z[1][j] + XY[i][2] * Z[2][j];
			}
		}
		float q[4], back[3][3];
		Players::QuatFromRows(r, q);
		Players::RowsFromQuat(q, back);
		Check(SameRows(r, back, 1e-4f), "a turn about all three axes");
	}
	// halfway between two headings is the heading halfway, the short way round
	{
		float r0[3][3], r1[3][3], q0[4], q1[4], mid[4], rows[3][3], want[3][3];
		RowsFacing(3.0f, r0);
		RowsFacing(-3.0f, r1);       // 0.28 the other way round, through pi
		Players::QuatFromRows(r0, q0);
		Players::QuatFromRows(r1, q1);
		Players::Nlerp(q0, q1, 0.5f, mid);
		Players::RowsFromQuat(mid, rows);
		RowsFacing(3.14159265f, want);
		Check(SameRows(rows, want, 1e-3f), "halfway the short way round");
		Players::Nlerp(q0, q1, 0.0f, mid);
		Players::RowsFromQuat(mid, rows);
		Check(SameRows(rows, r0, 1e-4f), "none of the way");
	}
}

static void TestSample() {
	Players::Run r;
	r.frames.push_back(MakeFrame(0, 0.0f, 1, 0.0f));
	r.frames.push_back(MakeFrame(100, 10.0f, 1, 0.1f));
	r.frames.push_back(MakeFrame(200, 30.0f, 2, 0.5f));
	r.frames.push_back(MakeFrame(400, 30.0f, 2, 0.7f));
	Players::Sample s;
	Check(Players::SampleRun(r, 50, &s) && Near(s.pos[0], 5.0f) && Near(s.pos[2], -5.0f) && s.frame == 0, "halfway between the first two");
	Check(Near(s.animTime, 0.05f), "the animation moved on from the frame before");
	Check(Players::SampleRun(r, 150, &s) && Near(s.pos[0], 20.0f) && s.frame == 1, "between the second and third");
	Check(Players::SampleRun(r, 200, &s) && Near(s.pos[0], 30.0f) && s.frame == 2 && Near(s.animTime, 0.5f), "on a frame");
	Check(Players::SampleRun(r, 450, &s) && Near(s.pos[0], 5.0f) && s.frame == 0, "round again after its end");
	Check(Players::SampleRun(r, 400 * 7 + 100, &s) && Near(s.pos[0], 10.0f) && s.frame == 1, "and again");
	Players::Run one;
	one.frames.push_back(MakeFrame(0, 0, 0, 0));
	Check(!Players::SampleRun(one, 10, &s), "nothing to play in one frame");

	// the animation's time on from a frame: held, round, there and back, backwards
	Players::Frame f = MakeFrame(0, 0, 1, 1.5f);
	Check(Near(Players::AnimTimeAfter(f, 0.25f), 1.75f), "on");
	Check(Near(Players::AnimTimeAfter(f, 1.0f), 2.0f), "held at its end");
	f.flags = Players::LoopFlags(Players::kCycle);
	Check(Near(Players::AnimTimeAfter(f, 1.0f), 0.5f), "round again");
	Check(Near(Players::AnimTimeAfter(f, 5.0f), 0.5f), "round again, more than once");
	f.flags = Players::LoopFlags(Players::kPingPong);
	Check(Near(Players::AnimTimeAfter(f, 1.0f), 1.5f), "there and back");
	Check(Near(Players::AnimTimeAfter(f, 0.75f), 1.75f), "there and part way back");
	f.flags = Players::kBackwards;
	f.animFrom = 2.0f;
	f.animTo = 0.0f;
	Check(Near(Players::AnimTimeAfter(f, 0.5f), 1.0f), "backwards");
	Check(Near(Players::AnimTimeAfter(f, 3.0f), 0.0f), "backwards, held at its end");
	f.flags = Players::kBackwards | Players::LoopFlags(Players::kCycle);
	Check(Near(Players::AnimTimeAfter(f, 2.0f), 1.5f), "backwards, round again");
	f.animSpeed = 2.0f;
	f.flags = 0;
	f.animFrom = 0.0f;
	f.animTo = 2.0f;
	Check(Near(Players::AnimTimeAfter(f, 0.1f), 1.7f), "at twice the speed");
	f.animFrom = f.animTo = 0.4f;
	f.animTime = 0.4f;
	f.flags = Players::LoopFlags(Players::kCycle);
	Check(Near(Players::AnimTimeAfter(f, 1.0f), 0.4f), "a single frame stays");
}

static void TestRestartsAndKeep() {
	const Players::Frame a = MakeFrame(0, 0, 1, 0.5f);
	Players::Frame b = MakeFrame(33, 0, 1, 0.53f);
	Check(!Players::AnimRestarted(a, b), "the same animation on");
	b.anim = 2;
	Check(Players::AnimRestarted(a, b), "another animation");
	b = MakeFrame(33, 0, 1, 0.1f);
	Check(Players::AnimRestarted(a, b), "played again from the start");
	b.flags = Players::LoopFlags(Players::kCycle);
	Players::Frame a2 = a;
	a2.flags = b.flags;
	Check(!Players::AnimRestarted(a2, b), "a loop round again is not a new start");
	b = MakeFrame(33, 0, 1, 0.53f, Players::kFlipped);
	Check(Players::AnimRestarted(a, b), "the other foot forward");
	b = MakeFrame(33, 0, 1, 0.53f);
	b.animTo = 1.0f;
	Check(Players::AnimRestarted(a, b), "another part of it");
	b = MakeFrame(33, 0, 1, 0.4f, Players::kBackwards);
	Players::Frame a3 = MakeFrame(0, 0, 1, 0.5f, Players::kBackwards);
	Check(!Players::AnimRestarted(a3, b), "backwards on");
	b.animTime = 0.9f;
	Check(Players::AnimRestarted(a3, b), "backwards from the start again");

	Players::Run r;
	Check(Players::Keep(r, MakeFrame(0, 0, 1, 0.0f)), "the first frame");
	r.frames.push_back(MakeFrame(0, 0, 1, 0.0f));
	Check(!Players::Keep(r, MakeFrame(16, 0, 1, 0.016f)), "not one 16 ms on");
	Check(Players::Keep(r, MakeFrame(33, 0, 1, 0.033f)), "one 33 ms on");
	Check(Players::Keep(r, MakeFrame(16, 0, 2, 0.0f)), "a new animation at once");
	Check(!Players::Keep(r, MakeFrame(0, 0, 2, 0.0f)), "never one at the same time");
}

static Players::AimPlayer At(float x, float y, float z, bool present = true) {
	Players::AimPlayer p;
	p.dx = x;
	p.dy = y;
	p.dz = z;
	p.present = present;
	return p;
}

static void TestAim() {
	Players::AimSettings s;
	s.reach = 150.0f;
	s.aimReach = 600.0f;
	s.cone = 0.7f;
	s.height = 300.0f;
	s.body = 20.0f;
	s.lockMs = 4000;
	Players::AimState st = { -1, 0 };
	float edge = 0.0f;
	// looking along +z
	Players::AimPlayer one[1] = { At(0, 0, 400) };
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 0, s, &st, &edge) == 0 && Near(edge, 380.0f, 0.01f), "aim: ahead, in reach");
	st.target = -1;
	one[0] = At(400, 0, 0);
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 10, s, &st, &edge) == -1, "aim: to the side and not near: nobody");
	one[0] = At(0, 0, -100);
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 20, s, &st, &edge) == 0, "aim: behind him but right by him");
	st.target = -1;
	one[0] = At(0, 400, 200);
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 30, s, &st, &edge) == -1, "aim: too far up");
	one[0] = At(0, 0, 700);
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 40, s, &st, &edge) == -1, "aim: ahead but past the reach");
	one[0] = At(450, 0, 450);
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 50, s, &st, &edge) == -1, "aim: 45 degrees off is outside the cone");
	one[0] = At(150, 0, 450);
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 60, s, &st, &edge) == 0, "aim: 18 degrees off is inside");
	// the nearer and straighter wins; the one picked stays while nearly as good
	st.target = -1;
	Players::AimPlayer two[2] = { At(0, 0, 500), At(40, 0, 300) };
	Check(Players::Aim(two, 2, 0.0f, 1.0f, 100, s, &st, &edge) == 1, "aim: the nearer of two ahead");
	two[0] = At(0, 0, 280);
	Check(Players::Aim(two, 2, 0.0f, 1.0f, 110, s, &st, &edge) == 1, "aim: the one picked stays while nearly as good");
	two[0] = At(0, 0, 120);
	Check(Players::Aim(two, 2, 0.0f, 1.0f, 120, s, &st, &edge) == 0, "aim: a much better one takes over");
	// once nobody is there, the one picked stays for the lock
	st.target = -1;
	one[0] = At(0, 0, 300);
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 1000, s, &st, &edge) == 0, "lock: picked");
	one[0] = At(300, 0, -300);
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 2000, s, &st, &edge) == 0, "lock: gone past him, still picked");
	Check(edge > 380.0f && edge < 410.0f, "lock: how far he is now");
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 5000, s, &st, &edge) == 0, "lock: up to the lock's end");
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 5001, s, &st, &edge) == -1 && st.target == -1, "lock: then nobody");
	st.target = -1;
	one[0] = At(0, 0, 300);
	Players::Aim(one, 1, 0.0f, 1.0f, 1000, s, &st, &edge);
	one[0] = At(1300, 0, 0);
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 1500, s, &st, &edge) == -1, "lock: not once he is past twice the reach");
	st.target = -1;
	one[0] = At(0, 0, 300);
	Players::Aim(one, 1, 0.0f, 1.0f, 1000, s, &st, &edge);
	one[0] = At(0, 0, 300, false);
	Check(Players::Aim(one, 1, 0.0f, 1.0f, 1100, s, &st, &edge) == -1, "lock: not once he has gone");
	st.target = -1;
	two[0] = At(0, 0, 300);
	two[1] = At(500, 0, 0);
	Check(Players::Aim(two, 2, 0.0f, 1.0f, 1000, s, &st, &edge) == 0, "lock: the first picked");
	two[0] = At(-300, 0, -300);
	two[1] = At(0, 0, 400);
	Check(Players::Aim(two, 2, 0.0f, 1.0f, 1200, s, &st, &edge) == 1, "lock: another in front takes over");
	// looking the other way round: the same with the camera turned
	st.target = -1;
	one[0] = At(-400, 0, 0);
	Check(Players::Aim(one, 1, -1.0f, 0.0f, 0, s, &st, &edge) == 0, "aim: ahead the way the camera looks");
	// no way known: the nearest in reach
	st.target = -1;
	two[0] = At(500, 0, 0);
	two[1] = At(0, 0, -300);
	Check(Players::Aim(two, 2, 0.0f, 0.0f, 0, s, &st, &edge) == 1, "aim: with no way known, the nearer");
}

// A view matrix as D3DXMatrixLookAtLH makes it (a point times the matrix), for a camera at eye turned by yaw
static void LookAt(const float eye[3], float yaw, bool transposed, float out[16]) {
	const float fx = sinf(yaw), fz = cosf(yaw);
	const float x[3] = { fz, 0.0f, -fx }, y[3] = { 0.0f, 1.0f, 0.0f }, z[3] = { fx, 0.0f, fz };
	float m[16] = {
		x[0], y[0], z[0], 0.0f,
		x[1], y[1], z[1], 0.0f,
		x[2], y[2], z[2], 0.0f,
		-(x[0] * eye[0] + x[1] * eye[1] + x[2] * eye[2]), -(y[0] * eye[0] + y[1] * eye[1] + y[2] * eye[2]),
		-(z[0] * eye[0] + z[1] * eye[1] + z[2] * eye[2]), 1.0f,
	};
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			out[r * 4 + c] = transposed ? m[c * 4 + r] : m[r * 4 + c];
		}
	}
}

static void TestCamera() {
	const float eye[3] = { 1200.0f, 300.0f, -5400.0f };
	float v[16], rx = 0.0f, rz = 0.0f;
	for (int t = 0; t < 2; t++) {
		for (int k = 0; k < 8; k++) {
			const float yaw = -2.8f + 0.8f * k;
			LookAt(eye, yaw, t == 1, v);
			const bool ok = Players::CameraRight(eye, v, &rx, &rz);
			Check(ok && Near(rx, cosf(yaw)) && Near(rz, -sinf(yaw)), t ? "camera: right, the matrix the other way round" : "camera: right");
			// what is to the camera's right is to the right
			const float b = Players::Bearing(sinf(yaw), cosf(yaw), rx, rz, cosf(yaw) * 10.0f, -sinf(yaw) * 10.0f);
			Check(Near(b, 1.5707963f, 0.001f), "bearing: the camera's right is +90 degrees");
		}
	}
	LookAt(eye, 0.0f, false, v);
	Check(Near(Players::Bearing(0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f), 0.7853982f), "bearing: ahead and right");
	Check(Near(Players::Bearing(0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f), -0.7853982f), "bearing: ahead and left");
	Check(Near(fabsf(Players::Bearing(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f)), 3.1415927f), "bearing: behind");
	// a camera on its side has no right along the ground; nor does a matrix that is not numbers
	float side[16] = { 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	const float origin[3] = { 0, 0, 0 };
	Check(!Players::CameraRight(origin, side, &rx, &rz), "camera: on its side");
	v[5] = std::nanf("");
	Check(!Players::CameraRight(eye, v, &rx, &rz), "camera: not numbers");
}

int main() {
	TestIdentity();
	TestFile();
	TestTurns();
	TestSample();
	TestRestartsAndKeep();
	TestAim();
	TestCamera();
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
