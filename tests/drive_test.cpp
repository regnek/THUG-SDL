// Tests for drive.h: the reach of a car, its speed, the places to get out at, the names of the tunings,
// the cars that come back, the cars left parked and the ground they are left on.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src tests/drive_test.cpp -o drive_test

#include "drive.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static int s_checks = 0;
static int s_failed = 0;

static void Check(bool ok, const char* what) {
	s_checks++;
	if (!ok) {
		s_failed++;
		std::printf("FAILED: %s\n", what);
	}
}

static bool Near(float a, float b, float within = 0.01f) {
	return std::fabs(a - b) <= within;
}

static const float kPi = 3.14159265f;

// The rows of a matrix facing heading, level, as the game keeps them: left, up, the way it faces
static void Facing(float heading, float m[12]) {
	const float c = std::cos(heading), s = std::sin(heading);
	const float rows[12] = { c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0 };
	std::memcpy(m, rows, sizeof(rows));
}

static void TestBox() {
	// the base of the game's own colliders for its police car: x 33 / -33, z 150 at the front, -65 at the back
	const float a[3] = { 33, 25, 150 }, b[3] = { -33, 25, 150 }, c[3] = { 33, 25, -65 };
	Drive::Box box;
	Check(Drive::BoxFromColliders(a, b, c, 55, &box), "a box from the police car's colliders");
	Check(Near(box.halfWidth, 33) && Near(box.front, 150) && Near(box.rear, -65) && Near(box.height, 55), "its numbers");
	// the corners the other way round still give the same box
	Check(Drive::BoxFromColliders(c, b, a, 55, &box) && Near(box.front, 150) && Near(box.rear, -65), "front and back either way");
	const float flat[3] = { 33, 25, 150 };
	Check(!Drive::BoxFromColliders(a, flat, c, 55, &box), "no width: no box");
	Check(!Drive::BoxFromColliders(a, b, a, 55, &box), "no length: no box");
	Check(!Drive::BoxFromColliders(a, b, c, -3, &box), "no height: no box");
	const Drive::Box usual = Drive::UsualBox();
	Check(usual.front > 100 && usual.rear < -40 && usual.halfWidth > 30, "the usual box is a car's");
}

static void TestReach() {
	const float pos[3] = { 1000, 50, -2000 };
	float m[12];
	Facing(kPi / 2, m);    // facing +x: its left is -z
	float local[3];
	const float ahead[3] = { 1100, 50, -2000 };
	Drive::Local(pos, m, ahead, local);
	Check(Near(local[0], 0) && Near(local[1], 0) && Near(local[2], 100), "100 ahead is 100 along");
	const float left[3] = { 1000, 80, -2060 };
	Drive::Local(pos, m, left, local);
	Check(Near(local[0], 60) && Near(local[1], 30) && Near(local[2], 0), "60 to its left (-z when it faces +x)");

	const Drive::Box box = Drive::UsualBox();
	float away = -1;
	const float door[3] = { 60, 0, 40 };
	Check(Drive::InReach(box, 45, door, &away) && Near(away, 22), "at the driver's door, 22 from the body");
	const float against[3] = { 20, 10, 40 };
	Check(Drive::InReach(box, 45, against, &away) && Near(away, 0), "against the body: 0");
	const float wide[3] = { 90, 0, 40 };
	Check(!Drive::InReach(box, 45, wide, &away), "52 from the side: out of reach");
	const float corner[3] = { 38 + 30, 0, 150 + 30 };
	Check(Drive::InReach(box, 45, corner, &away) && Near(away, std::sqrt(1800.0f), 0.1f), "off the front corner: the distance to it");
	const float far[3] = { 0, 0, 150 + 46 };
	Check(!Drive::InReach(box, 45, far, &away), "46 in front: out of reach");
	const float high[3] = { 50, 62 + 46, 0 };
	Check(!Drive::InReach(box, 45, high, &away), "above the roof: out of reach");
	const float low[3] = { 50, -61, 0 };
	Check(!Drive::InReach(box, 45, low, &away), "a storey below: out of reach");
	const float kerb[3] = { 50, -20, 0 };
	Check(Drive::InReach(box, 45, kerb, &away), "a little lower (a kerb): in reach");
}

static void TestHeading() {
	const float headings[] = { 0.0f, 0.7f, kPi / 2, 2.9f, -1.2f, -kPi / 2 };
	for (float h : headings) {
		float m[12];
		Facing(h, m);
		Check(Near(Drive::Heading(m), h, 0.0005f), "the heading back from its matrix");
	}
}

static void TestTrack() {
	Drive::Track t;
	t.Reset();
	float m[12];
	Facing(0, m);
	float p[3] = { 0, 0, 0 };
	t.Update(p, 1.0f / 60);
	Check(t.Forward(m) == 0, "one place: no speed yet");
	for (int i = 0; i < 30; i++) {
		p[2] += 600.0f / 60;
		t.Update(p, 1.0f / 60);
	}
	Check(Near(t.Forward(m), 600, 1), "600 a second along z");
	Facing(kPi, m);
	Check(t.Forward(m) == 0, "seen from behind it goes backwards: 0");
	Facing(0, m);
	p[0] += 20000;    // put somewhere else
	t.Update(p, 1.0f / 60);
	Check(t.Forward(m) == 0 && t.frames == 0, "a jump: no speed");
	t.Update(p, 2.0f);
	Check(t.frames == 0, "a long gap: starts again");
	for (int i = 0; i < 10; i++) {
		p[2] += 5;
		t.Update(p, 1.0f / 30);
	}
	Check(Near(t.Forward(m), 150, 1), "150 a second at 30 frames a second");
}

static void TestDoors() {
	const float pos[3] = { -500, 12, 300 };
	float m[12];
	const float heading = 0.9f;
	Facing(heading, m);
	const Drive::Box box = Drive::UsualBox();
	float doors[4][3];
	Drive::Doors(pos, heading, box, 25, doors);
	float l[3];
	Drive::Local(pos, m, doors[0], l);
	Check(Near(l[0], box.halfWidth + 25) && Near(l[1], 0), "first: the driver's side (its left), 25 from the body");
	Check(l[2] > box.rear && l[2] < box.front, "by the front seats");
	Drive::Local(pos, m, doors[1], l);
	Check(Near(l[0], -(box.halfWidth + 25)), "then the other side");
	Drive::Local(pos, m, doors[2], l);
	Check(Near(l[0], 0) && Near(l[2], box.rear - 25), "then behind");
	Drive::Local(pos, m, doors[3], l);
	Check(Near(l[0], 0) && Near(l[2], box.front + 25), "then in front");
	Check(Near(doors[0][1], 12) && Near(doors[3][1], 12), "at the car's height");
}

static void TestTuneName() {
	char out[64];
	Check(Drive::TuneName("veh\\veh_suv_drg\\veh_suv_drg.mdl", out, sizeof(out)) && std::strcmp(out, "Flickit_Car_veh_suv_drg") == 0, "the file name");
	Check(Drive::TuneName("veh\\Veh_SDKart_Security\\Veh_SDKart_Security.mdl", out, sizeof(out)) && std::strcmp(out, "Flickit_Car_veh_sdkart_security") == 0, "lower case");
	Check(Drive::TuneName("veh/fl/veh_policecar_fl/veh_policecar_fl.mdl", out, sizeof(out)) && std::strcmp(out, "Flickit_Car_veh_policecar_fl") == 0, "forward slashes");
	Check(Drive::TuneName("veh_custom", out, sizeof(out)) && std::strcmp(out, "Flickit_Car_veh_custom") == 0, "no folders, no ending");
	Check(!Drive::TuneName("veh\\", out, sizeof(out)), "no file name");
	Check(!Drive::TuneName("veh\\veh_custom\\veh_custom.mdl", out, 20), "no room");
	Check(!Drive::TuneName(0, out, sizeof(out)), "no model");
}

static void TestComeback() {
	Drive::Comeback c;
	const float start[3] = { 0, 0, 0 };
	Check(c.Add(0x111, start, 1000), "a car");
	const float near[3] = { 100, 0, 0 };
	const float far[3] = { 5000, 0, 0 };
	Check(c.Due(5000, far, 15000, 2500) == 0, "not before the wait");
	Check(c.Due(20000, near, 15000, 2500) == 0, "not when he is near its start");
	Check(c.Due(20000, far, 15000, 2500) == 0x111 && c.Count() == 0, "back, and off the list");
	Check(c.Due(40000, far, 15000, 2500) == 0, "only once");

	c.Add(0x222, start, 1000);
	c.Add(0x222, start, 9000);
	Check(c.Count() == 1, "taken twice: once on the list");
	Check(c.Due(20000, far, 15000, 2500) == 0 && c.Due(24001, far, 15000, 2500) == 0x222, "the wait from the second time");

	c.Add(0x333, start, 0);
	c.Remove(0x333);
	Check(c.Count() == 0, "made again by the level: off the list");

	for (int i = 0; i < Drive::Comeback::kMax; i++) {
		Check(c.Add(0x1000 + i, start, 0), "room");
	}
	Check(!c.Add(0x9999, start, 0), "full");
	Check(c.Due(0xFFFFFFF0u, far, 15000, 2500) == 0x1000 && c.NameAt(0) == 0x1001, "in order");
	// GetTickCount turns over after 49 days: the wait still counts across
	c.Clear();
	c.Add(0x444, start, 0xFFFFF000u);
	Check(c.Due(0x00000100u, far, 15000, 2500) == 0, "across the turn, too soon");
	Check(c.Due(0x00003000u, far, 15000, 2500) == 0x444, "across the turn, in time");
}

static void TestParking() {
	Drive::Parking p;
	bool inUse[Drive::Parking::kSlots];
	for (int i = 0; i < Drive::Parking::kSlots; i++) {
		inUse[i] = false;
	}
	Check(p.Pick(inUse) == 0, "the first free");
	const Drive::Box box = Drive::UsualBox();
	for (int i = 0; i < Drive::Parking::kSlots; i++) {
		p.Park(i, i == 2 ? "veh\\veh_pickup\\veh_pickup.mdl" : "veh\\veh_custom\\veh_custom.mdl", box);
		inUse[i] = true;
	}
	Check(p.Pick(inUse) == 0, "all there: the one left longest ago");
	p.Park(0, "veh\\veh_minivan\\veh_minivan.mdl", box);
	Check(p.Pick(inUse) == 1, "then the next oldest");
	inUse[3] = false;
	Check(p.Pick(inUse) == 3, "a car gone (taken again): its name is free");
	Check(std::strcmp(p.Model(2), "veh\\veh_pickup\\veh_pickup.mdl") == 0, "the model kept");
	p.Leave(2);
	Check(p.Model(2) == 0 && p.BoxOf(2) == 0, "taken again: nothing kept");
	char longName[300];
	std::memset(longName, 'a', sizeof(longName) - 1);
	longName[sizeof(longName) - 1] = 0;
	p.Park(1, longName, box);
	Check(std::strlen(p.Model(1)) == Drive::Parking::kModel - 1, "a long model name cut to fit");
	Check(p.Model(Drive::Parking::kSlots) == 0 && !p.Used(-1), "no such slot");

	// room for a car knocked off its way: a free name, else the oldest of those away from him, else none
	bool away[Drive::Parking::kSlots];
	for (int i = 0; i < Drive::Parking::kSlots; i++) {
		inUse[i] = true;
		away[i] = false;
	}
	Check(p.Room(inUse, away) == -1 && p.Room(inUse, 0) == -1, "room: all there and near him, none");
	away[5] = true;
	away[6] = true;
	p.Park(5, "veh\\veh_custom\\veh_custom.mdl", box);
	Check(p.Room(inUse, away) == 6, "room: the oldest of those away");
	Check(p.Pick(inUse, away) == 6, "pick: the oldest away before the oldest of all");
	away[5] = away[6] = false;
	const int oldest = p.Pick(inUse, away);
	Check(oldest >= 0 && oldest != 5 && oldest != 1, "pick: with none away, the oldest of all");
	inUse[8] = false;
	Check(p.Room(inUse, away) == 8 && p.Pick(inUse, away) == 8, "room: a free name first");

	// The car of the level each was, and when he got out of it
	p.Park(3, "veh\\veh_custom\\veh_custom.mdl", box, 0xABCD, 50000);
	Check(p.Origin(3) == 0xABCD && p.ParkedAt(3) == 50000, "the car it was and when, kept");
	p.Park(3, "veh\\veh_custom\\veh_custom.mdl", box, 0xABCD, 90000);
	Check(p.ParkedAt(3) == 90000, "left again: the time from then");
	p.Leave(3);
	Check(p.Origin(3) == 0 && p.ParkedAt(3) == 0, "taken again: not kept with the slot");
	Check(p.Origin(-1) == 0 && p.ParkedAt(Drive::Parking::kSlots) == 0, "no such slot, no car");
}

static void TestCamera() {
	const Drive::CameraPlace usual = Drive::CameraFor(Drive::UsualBox());
	Check(usual.height == 55.0f && usual.distance == 240.0f, "a car of the usual size: the game's own camera");
	Drive::Box small = Drive::UsualBox();
	small.height = 40.0f;
	small.front = 100.0f;
	const Drive::CameraPlace s = Drive::CameraFor(small);
	Check(s.height == 55.0f && s.distance == 240.0f, "a smaller one: the same");
	// the DC truck: colliders up to 142 (a box of 150), from -64 to 205
	Drive::Box truck;
	truck.halfWidth = 50.0f;
	truck.rear = -64.0f;
	truck.front = 205.0f;
	truck.height = 150.0f;
	const Drive::CameraPlace t = Drive::CameraFor(truck);
	Check(t.height > truck.height && t.height < truck.height + 40.0f, "the truck: the camera above its roof");
	Check(t.distance > 350.0f && t.distance < 450.0f, "and farther back");
}

static void TestGoesBack() {
	const float parked[3] = { 1000, 0, 0 };
	const float start[3] = { -3000, 0, 0 };
	const float near[3] = { 1200, 0, 0 };
	const float farFromBoth[3] = { 4000, 0, 3000 };
	const float nearStart[3] = { -2900, 0, 0 };
	const uint32_t two = 120000;
	Check(!Drive::GoesBack(100000, 0, two, parked, farFromBoth, start, 2500), "not before two minutes");
	Check(Drive::GoesBack(120000, 0, two, parked, farFromBoth, start, 2500), "after two minutes, far from both");
	Check(!Drive::GoesBack(500000, 0, two, parked, near, start, 2500), "not while he is near it");
	Check(!Drive::GoesBack(500000, 0, two, parked, nearStart, start, 2500), "nor near where the car it was comes back");
	Check(Drive::GoesBack(500000, 0, two, parked, nearStart, 0, 2500), "near there is fine when nothing comes back there");
	Check(!Drive::GoesBack(500000, 0, 0, parked, farFromBoth, start, 2500), "a stand of 0: it stays");
	// up and down count too
	const float above[3] = { 1000, 2000, 0 };
	Check(!Drive::GoesBack(500000, 0, two, parked, above, 0, 2500), "2000 above it is near");
	// GetTickCount turns over after 49 days
	Check(!Drive::GoesBack(0x00001000u, 0xFFFFF000u, two, parked, farFromBoth, start, 2500), "across the turn, too soon");
	Check(Drive::GoesBack(0x0001F000u, 0xFFFFF000u, two, parked, farFromBoth, start, 2500), "across the turn, in time");
	Check(Drive::FarApart(parked, start, 4000) && !Drive::FarApart(parked, start, 4001), "4000 apart");
}

static void TestGround() {
	const float heading = 0.7f;
	float wantLevel[12];
	Facing(heading, wantLevel);
	// the middle and four wheels (x along the car's left, z the way it faces)
	const float at[5][2] = { { 0, 0 }, { 30, 110 }, { -30, 110 }, { 30, -40 }, { -30, -40 } };
	float probes[5][3];
	bool found[5];
	// flat ground at -6, the car's body up on its springs at 14
	for (int i = 0; i < 5; i++) {
		probes[i][0] = at[i][0];
		probes[i][1] = -6.0f;
		probes[i][2] = at[i][1];
		found[i] = true;
	}
	float pos[3] = { 100, 14, 200 }, m[12];
	Check(Drive::GroundUnder(pos, m, heading, probes, found, 5), "flat: ground found");
	bool level = true;
	for (int k = 0; k < 12; k++) {
		level = level && Near(m[k], wantLevel[k], 0.001f);
	}
	Check(Near(pos[1], -6.0f) && Near(pos[0], 100) && Near(pos[2], 200) && level, "flat: on the ground, level, the way it faced");

	// up a slope of one in ten ahead and one in twenty to its left
	for (int i = 0; i < 5; i++) {
		probes[i][1] = 10.0f + 0.1f * at[i][1] + 0.05f * at[i][0];
	}
	found[2] = false;                       // and one wheel over nothing
	pos[1] = 30;
	Check(Drive::GroundUnder(pos, m, heading, probes, found, 5), "slope: ground found");
	Check(Near(pos[1], 10.0f), "slope: the height of the plane at its middle");
	const float aheadLen = std::sqrt(1.0f + 0.01f);
	Check(Near(m[8], std::sin(heading) / aheadLen, 0.001f) && Near(m[9], 0.1f / aheadLen, 0.001f) && Near(m[10], std::cos(heading) / aheadLen, 0.001f),
		"slope: it faces up the slope ahead");
	Check(Near(m[1], 0.05f, 0.003f), "slope: its left side up the slope to the left");
	const float dotUA = m[4] * m[8] + m[5] * m[9] + m[6] * m[10], dotUL = m[4] * m[0] + m[5] * m[1] + m[6] * m[2];
	const float lenU = std::sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
	Check(Near(dotUA, 0.0f, 0.0001f) && Near(dotUL, 0.0f, 0.0001f) && Near(lenU, 1.0f, 0.0001f) && m[5] > 0.99f, "slope: square rows, up mostly up");

	// one wheel on top of a kerb post: not a plane to trust, level on the ground at its middle
	for (int i = 0; i < 5; i++) {
		probes[i][1] = -6.0f;
		found[i] = true;
	}
	probes[1][1] = 34.0f;
	pos[1] = 14;
	Check(Drive::GroundUnder(pos, m, heading, probes, found, 5) && Near(pos[1], -6.0f) && Near(m[5], 1.0f), "a kerb post under a wheel: level at its middle");

	// only one probe: level at it; none: nothing changes
	for (int i = 0; i < 5; i++) {
		found[i] = i == 3;
	}
	pos[1] = 14;
	Check(Drive::GroundUnder(pos, m, heading, probes, found, 5) && Near(pos[1], -6.0f) && Near(m[5], 1.0f), "one probe: level at it");
	for (int i = 0; i < 5; i++) {
		found[i] = false;
	}
	pos[1] = 14;
	Check(!Drive::GroundUnder(pos, m, heading, probes, found, 5) && Near(pos[1], 14.0f), "no ground: nothing changes");
}

int main() {
	TestBox();
	TestReach();
	TestHeading();
	TestTrack();
	TestDoors();
	TestTuneName();
	TestComeback();
	TestParking();
	TestGoesBack();
	TestCamera();
	TestGround();
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed == 0 ? 0 : 1;
}
