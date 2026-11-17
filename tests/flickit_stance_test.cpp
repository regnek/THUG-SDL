// Offline tests for the stance the stick asks for: Flickit::Output::stance from the recognizer, the
// StanceKeeper that the game's side asks what to do with it, and what both rely on in THUG's
// scripts. No game, SDL or Windows needed:
//   g++ -std=c++11 -Wall -Wextra -I src tests/flickit_stance_test.cpp src/flickit.cpp -o flickit_stance_test
//   ./flickit_stance_test scripts/qb/source/scripts/game/skater

#include "flickit.h"
#include "flickit_pose.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

using namespace Flickit;

static int g_checks = 0;
static int g_failures = 0;
static std::string g_case;

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failures++; printf("  FAIL [%s] line %d: %s\n", g_case.c_str(), __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { g_checks++; long long va = (long long)(a), vb = (long long)(b); if (va != vb) { g_failures++; printf("  FAIL [%s] line %d: %s == %s (%lld vs %lld)\n", g_case.c_str(), __LINE__, #a, #b, va, vb); } } while (0)

static const float kPi = 3.14159265f;

// ---------------------------------------------------------------------------------------------
// What the stance relies on in THUG's scripts
// ---------------------------------------------------------------------------------------------

static std::string readFile(const std::string& path) {
	std::string out;
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return out;
	char buffer[4096];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) out.append(buffer, n);
	fclose(f);
	return out;
}

// The text of "SCRIPT name ... ENDSCRIPT", empty if there is none
static std::string scriptBody(const std::string& text, const char* name) {
	const std::string key = std::string("SCRIPT ") + name;
	size_t at = 0;
	while ((at = text.find(key, at)) != std::string::npos) {
		const size_t after = at + key.size();
		const bool lineStart = at == 0 || text[at - 1] == '\n';
		const bool wordEnd = after >= text.size() || text[after] == ' ' || text[after] == '\t' || text[after] == '\r' || text[after] == '\n';
		if (lineStart && wordEnd) {
			const size_t end = text.find("ENDSCRIPT", after);
			return end == std::string::npos ? std::string() : text.substr(at, end - at);
		}
		at = after;
	}
	return std::string();
}

static size_t find(const std::string& text, const char* what, size_t from = 0) {
	return text.find(what, from);
}

static bool has(const std::string& text, const char* what) {
	return text.find(what) != std::string::npos;
}

// The global "name = ..." line, empty if there is none
static std::string globalLine(const std::string& text, const char* name) {
	const std::string key = std::string(name) + " = ";
	size_t at = 0;
	while ((at = text.find(key, at)) != std::string::npos) {
		if (at == 0 || text[at - 1] == '\n') {
			const size_t end = text.find('\n', at);
			return text.substr(at, end == std::string::npos ? std::string::npos : end - at);
		}
		at += key.size();
	}
	return std::string();
}

static void testScripts(const std::string& dir) {
	g_case = "THUG's scripts";
	const std::string tricks = readFile(dir + "/tricks.q");
	const std::string ground = readFile(dir + "/groundtricks.q");
	const std::string air = readFile(dir + "/airtricks.q");
	CHECK(!tricks.empty() && !ground.empty() && !air.empty());
	if (tricks.empty() || ground.empty() || air.empty()) {
		printf("  (the scripts are not in %s)\n", dir.c_str());
		return;
	}

	// The nollie loop sets the stance as it starts, and the jump from it is the game's Nollie. It
	// waits for the next frame in WaitOneGameFrame: that is where the game's side looks.
	const std::string nollieLoop = scriptBody(tricks, "OnGroundNollieAI");
	CHECK(!nollieLoop.empty());
	const size_t loopStart = find(nollieLoop, "BEGIN");
	CHECK(loopStart != std::string::npos);
	CHECK(find(nollieLoop, "NollieOn") < loopStart);
	CHECK(find(nollieLoop, "SetException Ex = Ollied Scr = Nollie") < loopStart);
	CHECK(find(nollieLoop, "waitonegameframe", loopStart) != std::string::npos);
	// Crouched in it he plays the nollie crouch, which the crouch pose knows as a crouch
	int crouches = 0;
	for (size_t at = 0; (at = nollieLoop.find("PlayAnim Anim = NollieCrouch", at)) != std::string::npos; at++) {
		size_t end = at + strlen("PlayAnim Anim = ");
		size_t stop = end;
		while (stop < nollieLoop.size() && nollieLoop[stop] != ' ' && nollieLoop[stop] != '\t' && nollieLoop[stop] != '\r' && nollieLoop[stop] != '\n') stop++;
		const std::string anim = nollieLoop.substr(end, stop - end);
		CHECK(FlickitPose::IsCrouchAnim(NameChecksum(anim.c_str())));
		crouches++;
	}
	CHECK_EQ(crouches, 5);

	// The regular loop takes him out of nollie as it starts, and waits the same way
	const std::string regularLoop = scriptBody(tricks, "OnGroundAi");
	CHECK(!regularLoop.empty());
	const size_t regularStart = find(regularLoop, "BEGIN");
	CHECK(regularStart != std::string::npos);
	CHECK(find(regularLoop, "NollieOff") < regularStart);
	CHECK(find(regularLoop, "waitonegameframe", regularStart) != std::string::npos);
	// Out of a crouch he coasts, and that plays the idle he stands in
	CHECK(has(scriptBody(tricks, "just_coasting"), "PlayAnim Anim = Idle NoRestart"));

	// The landing takes him out of nollie and out of pressure
	const std::string land = scriptBody(tricks, "Land2");
	CHECK(has(land, "NollieOff") && has(land, "PressureOff"));

	// The jump from the nollie loop: a nollie, which keeps the stance in the air
	const std::string nollie = scriptBody(ground, "Nollie");
	CHECK(has(nollie, "Jump") && has(nollie, "NollieOn") && has(nollie, "PlayAnim Anim = Nollie"));
	// An ollie in nollie stance is a nollie as well
	CHECK(has(scriptBody(ground, "Ollie"), "IF InNollie"));

	// A flip trick in nollie plays its nollie animation, and the stance ends a moment into it
	const std::string flip = scriptBody(air, "FlipTrick");
	const size_t inNollie = find(flip, "IF InNollie");
	CHECK(inNollie != std::string::npos);
	CHECK(find(flip, "PlayAnim Anim = <Nollie>", inNollie) != std::string::npos);
	CHECK(find(flip, "NollieOff", inNollie) != std::string::npos);

	// The tricks Skate's N_ names press have nollie animations in THUG's default layout, and so
	// do the second presses that make the 360 shove-its and the 360 flip
	const char* withNollie[] = {
		"Trick_Kickflip", "Trick_Heelflip", "Trick_PopShoveIt", "Trick_FSShoveIt", "Trick_Impossible",
		"Trick_VarialKickflip", "Trick_VarialHeelflip", "Trick_Hardflip", "Trick_InwardHeelflip",
		"Extra360ShoveIt", "ExtraFS360ShoveIt", "Extra_VarialKickflip",
	};
	for (size_t i = 0; i < sizeof(withNollie) / sizeof(withNollie[0]); i++) {
		const std::string line = globalLine(air, withNollie[i]);
		CHECK(!line.empty());
		CHECK(has(line, " Nollie = "));
	}
	// The second presses for the 360 heelflip, 360 hardflip and 360 inward heelflip have none:
	// those play their regular animation from nollie too
	const char* without[] = { "Extra_VarialHeelflip", "Extra_360Hardflip", "Extra_360InwardHeelflip" };
	for (size_t i = 0; i < sizeof(without) / sizeof(without[0]); i++) {
		const std::string line = globalLine(air, without[i]);
		CHECK(!line.empty());
		CHECK(!has(line, " Nollie = "));
	}

	// The Nollie button (L2) goes round regular, pressure, nollie: two presses from regular to
	// nollie, which is what NollieTaps=2 presses where the game's side does not keep the stance
	CHECK(has(ground, "{ Trigger = { Press , L2 , 20 } Scr = ToggleNollieRegular"));
	const std::string toggle = scriptBody(ground, "ApplyStanceToggle");
	const size_t inN = find(toggle, "IF InNollie");
	const size_t inP = find(toggle, "IF InPressure");
	const size_t otherwise = find(toggle, "ELSE", inP);
	CHECK(inN != std::string::npos && inP != std::string::npos && otherwise != std::string::npos);
	CHECK(find(toggle, "NollieOff", inN) < inP);                       // nollie -> regular
	CHECK(find(toggle, "NollieOn", inP) < otherwise);                  // pressure -> nollie
	CHECK(find(toggle, "PressureOn", otherwise) != std::string::npos); // regular -> pressure
	Settings cfg;
	SetDefaults(cfg);
	CHECK_EQ(cfg.nollieTaps, 2);
}

// ---------------------------------------------------------------------------------------------
// The StanceKeeper
// ---------------------------------------------------------------------------------------------

static StanceLook where(bool ground, bool regularLoop, bool nollieLoop, bool pressure = false) {
	StanceLook l;
	l.ground = ground;
	l.regularLoop = regularLoop;
	l.nollieLoop = nollieLoop;
	l.pressure = pressure;
	return l;
}

static const StanceLook kRegular = where(true, true, false);
static const StanceLook kNollie = where(true, false, true);
static const StanceLook kBusy = where(true, false, false);       // a landing, a brake, a manual
static const StanceLook kAir = where(false, false, false);

static StanceAnswer frame(StanceKeeper& k, int asked, const StanceLook& look) {
	k.Ask(asked);
	return k.Look(look);
}

static void testKeeper() {
	g_case = "keeper: crouch up and stand up";
	{
		StanceKeeper k;
		StanceAnswer a = frame(k, STANCE_ANY, kRegular);
		CHECK_EQ(a.move, STANCE_STAY);
		a = frame(k, STANCE_NOLLIE, kRegular);
		CHECK_EQ(a.move, STANCE_TO_NOLLIE);
		CHECK(!a.endPressure && !a.late);
		CHECK(k.Ours());
		for (int i = 0; i < 100; i++) {
			a = frame(k, STANCE_NOLLIE, kNollie);
			CHECK_EQ(a.move, STANCE_STAY);
		}
		CHECK(k.Ours());
		// the frame the crouch ends without a pop
		a = frame(k, STANCE_REGULAR, kNollie);
		CHECK_EQ(a.move, STANCE_TO_REGULAR);
		CHECK(!a.late);
		CHECK(!k.Ours());
		for (int i = 0; i < 100; i++) {
			a = frame(k, STANCE_ANY, kRegular);
			CHECK_EQ(a.move, STANCE_STAY);
		}
	}

	g_case = "keeper: regular waits until he is in a loop";
	{
		StanceKeeper k;
		frame(k, STANCE_NOLLIE, kRegular);
		frame(k, STANCE_NOLLIE, kNollie);
		CHECK_EQ(frame(k, STANCE_REGULAR, kBusy).move, STANCE_STAY);   // the crouch ended in a bump into a wall
		for (int i = 0; i < 50; i++) {
			CHECK_EQ(frame(k, STANCE_ANY, kBusy).move, STANCE_STAY);
		}
		CHECK_EQ(frame(k, STANCE_ANY, kNollie).move, STANCE_TO_REGULAR);
		CHECK_EQ(frame(k, STANCE_ANY, kRegular).move, STANCE_STAY);
		CHECK_EQ(frame(k, STANCE_ANY, kNollie).move, STANCE_STAY);      // nothing left to do
	}

	g_case = "keeper: a new crouch up takes over a wait for regular";
	{
		StanceKeeper k;
		frame(k, STANCE_NOLLIE, kRegular);
		frame(k, STANCE_REGULAR, kBusy);
		CHECK_EQ(frame(k, STANCE_NOLLIE, kNollie).move, STANCE_STAY);
		CHECK_EQ(frame(k, STANCE_NOLLIE, kNollie).move, STANCE_STAY);
		// and its pop is a nollie: nothing is left over that would take him to regular
		CHECK_EQ(frame(k, STANCE_ANY, kNollie).move, STANCE_STAY);
		CHECK_EQ(frame(k, STANCE_ANY, kAir).move, STANCE_STAY);
	}

	g_case = "keeper: the pop";
	{
		StanceKeeper k;
		frame(k, STANCE_NOLLIE, kRegular);
		for (int i = 0; i < 10; i++) frame(k, STANCE_NOLLIE, kNollie);
		// The pop asks for nothing, and he is still in the nollie loop on that frame: the jump is
		// the game's Nollie from there, so nothing may move him
		CHECK_EQ(frame(k, STANCE_ANY, kNollie).move, STANCE_STAY);
		CHECK_EQ(frame(k, STANCE_ANY, kAir).move, STANCE_STAY);
		CHECK(!k.Ours());
		for (int i = 0; i < 100; i++) {
			CHECK_EQ(frame(k, STANCE_ANY, kAir).move, STANCE_STAY);
		}
		// the landing takes him to the regular loop by itself
		CHECK_EQ(frame(k, STANCE_ANY, kBusy).move, STANCE_STAY);
		for (int i = 0; i < 100; i++) {
			CHECK_EQ(frame(k, STANCE_ANY, kRegular).move, STANCE_STAY);
		}
	}

	g_case = "keeper: a pop the game does not jump for";
	{
		StanceKeeper k;
		frame(k, STANCE_NOLLIE, kRegular);
		frame(k, STANCE_NOLLIE, kNollie);
		for (int i = 1; i < kStanceQuietLooks; i++) {
			CHECK_EQ(frame(k, STANCE_ANY, kNollie).move, STANCE_STAY);
		}
		const StanceAnswer a = frame(k, STANCE_ANY, kNollie);
		CHECK_EQ(a.move, STANCE_TO_REGULAR);
		CHECK(a.late);
		CHECK(!k.Ours());
		for (int i = 0; i < 100; i++) {
			CHECK_EQ(frame(k, STANCE_ANY, kRegular).move, STANCE_STAY);
		}
	}

	g_case = "keeper: a crouch up again starts the clock over";
	{
		StanceKeeper k;
		frame(k, STANCE_NOLLIE, kRegular);
		for (int i = 0; i < kStanceQuietLooks - 5; i++) frame(k, STANCE_ANY, kNollie);
		frame(k, STANCE_NOLLIE, kNollie);
		for (int i = 1; i < kStanceQuietLooks; i++) {
			CHECK_EQ(frame(k, STANCE_ANY, kNollie).move, STANCE_STAY);
		}
		CHECK_EQ(frame(k, STANCE_ANY, kNollie).move, STANCE_TO_REGULAR);
	}

	g_case = "keeper: the game's own nollie";
	{
		// The Nollie button put him in nollie: he stays there, until the stick crouches down
		StanceKeeper k;
		for (int i = 0; i < 100; i++) {
			CHECK_EQ(frame(k, STANCE_ANY, kNollie).move, STANCE_STAY);
		}
		CHECK(!k.Ours());
		CHECK_EQ(frame(k, STANCE_REGULAR, kNollie).move, STANCE_TO_REGULAR);
		CHECK_EQ(frame(k, STANCE_REGULAR, kRegular).move, STANCE_STAY);
		// and a crouch up in it is nollie already: no move, and not the stick's, so no clock
		StanceKeeper j;
		CHECK_EQ(frame(j, STANCE_NOLLIE, kNollie).move, STANCE_STAY);
		CHECK(!j.Ours());
		for (int i = 0; i < 100; i++) {
			CHECK_EQ(frame(j, STANCE_ANY, kNollie).move, STANCE_STAY);
		}
	}

	g_case = "keeper: out of the nollie loop and back";
	{
		// A bump into a wall in the crouch up: the game's flail goes back to the nollie loop
		// itself. The stick's clock is over: what ended it has its own way back.
		StanceKeeper k;
		frame(k, STANCE_NOLLIE, kRegular);
		frame(k, STANCE_NOLLIE, kNollie);
		frame(k, STANCE_ANY, kBusy);
		CHECK(!k.Ours());
		for (int i = 0; i < 100; i++) {
			CHECK_EQ(frame(k, STANCE_ANY, kNollie).move, STANCE_STAY);
		}
	}

	g_case = "keeper: pressure";
	{
		StanceKeeper k;
		StanceAnswer a = frame(k, STANCE_NOLLIE, where(true, true, false, true));
		CHECK_EQ(a.move, STANCE_TO_NOLLIE);
		CHECK(a.endPressure);
		a = frame(k, STANCE_NOLLIE, where(true, false, true, false));
		CHECK(!a.endPressure);
		// a crouch down leaves the game's pressure stance as it is
		StanceKeeper j;
		a = frame(j, STANCE_REGULAR, where(true, true, false, true));
		CHECK_EQ(a.move, STANCE_STAY);
		CHECK(!a.endPressure);
		a = frame(j, STANCE_ANY, where(true, true, false, true));
		CHECK(!a.endPressure);
	}

	g_case = "keeper: not on the ground";
	{
		StanceKeeper k;
		// in a bail his script may be anywhere: nothing is done
		CHECK_EQ(frame(k, STANCE_NOLLIE, where(false, true, false)).move, STANCE_STAY);
		CHECK_EQ(frame(k, STANCE_REGULAR, where(false, false, true)).move, STANCE_STAY);
		CHECK(!k.Ours());
	}

	g_case = "keeper: reset";
	{
		StanceKeeper k;
		frame(k, STANCE_NOLLIE, kRegular);
		frame(k, STANCE_REGULAR, kBusy);
		k.Reset();
		CHECK(!k.Ours());
		CHECK_EQ(k.Look(kNollie).move, STANCE_STAY);
		CHECK_EQ(k.Look(kNollie).move, STANCE_STAY);
	}
}

// ---------------------------------------------------------------------------------------------
// The recognizer's Output::stance
// ---------------------------------------------------------------------------------------------

struct Sim {
	Recognizer rec;
	Settings cfg;
	double hz;
	double clock;
	float x, y;
	int state;
	bool keeps;
	bool jumps;           // as the game does: a pop on the ground (Ollie let go, not to stand up) takes him into the air
	std::vector<Output> outs;

	explicit Sim(double rate) : hz(rate), clock(0.0), x(0), y(0), state(STATE_GROUND), keeps(true), jumps(true) {
		SetDefaults(cfg);
		Sanitize(cfg);
		hold(50);
	}

	void step() {
		Input in;
		in.now = 1000 + (uint32_t)floor(clock + 0.5);
		in.rx = x;
		in.ry = y;
		in.state = state;
		in.railCatch = true;
		in.keepsStance = keeps;
		Output o;
		rec.Update(in, cfg, o);
		if (jumps && state == STATE_GROUND && !outs.empty() && outs.back().ollie && !o.ollie && !o.standUp) {
			state = STATE_AIR;
		}
		outs.push_back(o);
		clock += 1000.0 / hz;
	}

	void hold(double ms) {
		int n = (int)floor(ms * hz / 1000.0 + 0.5);
		if (n < 1) n = 1;
		for (int i = 0; i < n; i++) step();
	}

	void moveTo(float tx, float ty, double ms) {
		int n = (int)floor(ms * hz / 1000.0 + 0.5);
		if (n < 1) n = 1;
		const float sx = x, sy = y;
		for (int i = 1; i <= n; i++) {
			const float f = (float)i / (float)n;
			x = sx + (tx - sx) * f;
			y = sy + (ty - sy) * f;
			step();
		}
	}

	// to the rim at the given angle (degrees, 0 = right, 90 = up)
	void toward(float angleDeg, double ms, float radius = 1.0f) {
		moveTo(radius * cosf(angleDeg * kPi / 180.0f), radius * sinf(angleDeg * kPi / 180.0f), ms);
	}

	// along the rim to the given angle, the short way round
	void arcTo(float angleDeg, double ms) {
		int n = (int)floor(ms * hz / 1000.0 + 0.5);
		if (n < 1) n = 1;
		const float a0 = atan2f(y, x) * 180.0f / kPi;
		float delta = angleDeg - a0;
		while (delta > 180.0f) delta -= 360.0f;
		while (delta < -180.0f) delta += 360.0f;
		for (int i = 1; i <= n; i++) {
			const float a = (a0 + delta * (float)i / (float)n) * kPi / 180.0f;
			x = cosf(a);
			y = sinf(a);
			step();
		}
	}

	void spring(double ms = 40) { moveTo(0, 0, ms); }

	size_t mark() const { return outs.size(); }

	int count(bool Output::*what, size_t from = 0) const {
		int n = 0;
		for (size_t i = from; i < outs.size(); i++) if (outs[i].*what) n++;
		return n;
	}

	int stances(int stance, size_t from = 0) const {
		int n = 0;
		for (size_t i = from; i < outs.size(); i++) if (outs[i].stance == stance) n++;
		return n;
	}

	// the first frame from `from` on where the button is down / up, -1 if none
	int firstDown(bool Output::*what, size_t from = 0) const {
		for (size_t i = from; i < outs.size(); i++) if (outs[i].*what) return (int)i;
		return -1;
	}
	int firstUp(bool Output::*what, size_t from) const {
		for (size_t i = from; i < outs.size(); i++) if (!(outs[i].*what)) return (int)i;
		return -1;
	}
};

static void testRecognizer(double hz) {
	char name[128];

	snprintf(name, sizeof(name), "crouch up, back to the centre, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		CHECK_EQ(s.cfg.nollie, 1);              // on by default
		CHECK_EQ(s.stances(STANCE_ANY), (int)s.outs.size());
		const size_t m = s.mark();
		s.toward(90, 60);
		s.hold(300);
		const int crouch = s.firstDown(&Output::ollie, m);
		CHECK(crouch >= 0);
		// from the frame the crouch begins, every frame of it asks for nollie
		for (size_t i = m; i < s.outs.size(); i++) {
			CHECK_EQ(s.outs[i].stance, (int)i >= crouch ? STANCE_NOLLIE : STANCE_ANY);
			if ((int)i >= crouch) CHECK(s.outs[i].ollie);
		}
		CHECK_EQ(s.count(&Output::nollie), 0);   // the game keeps the stance: no Nollie button
		const size_t back = s.mark();
		s.spring();
		s.hold(400);
		CHECK_EQ(s.count(&Output::standUp, back), 1);
		const int up = s.firstDown(&Output::standUp, back);
		for (size_t i = back; i < s.outs.size(); i++) {
			const int want = (int)i < up ? STANCE_NOLLIE : ((int)i == up ? STANCE_REGULAR : STANCE_ANY);
			CHECK_EQ(s.outs[i].stance, want);
		}
		CHECK(!s.outs.back().ollie);
		CHECK_EQ(s.count(&Output::flip), 0);
		CHECK_EQ(s.count(&Output::nollie), 0);
	}

	snprintf(name, sizeof(name), "nollie kickflip, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		const size_t m = s.mark();
		s.toward(90, 60);
		s.hold(300);
		s.toward(-45, 35);                      // flick down-right: the kickflip, upside down
		s.spring();
		s.hold(400);
		const int crouch = s.firstDown(&Output::ollie, m);
		const int pop = crouch >= 0 ? s.firstUp(&Output::ollie, (size_t)crouch) : -1;
		CHECK(crouch >= 0 && pop > crouch);
		for (size_t i = m; i < s.outs.size(); i++) {
			const bool crouched = (int)i >= crouch && (int)i < pop;
			CHECK_EQ(s.outs[i].stance, crouched ? STANCE_NOLLIE : STANCE_ANY);
		}
		CHECK_EQ(s.count(&Output::standUp), 0);
		CHECK_EQ(s.stances(STANCE_REGULAR), 0);
		int flips = 0, dir = DIR_NONE;
		for (size_t i = 0; i < s.outs.size(); i++) {
			if (s.outs[i].flip && (i == 0 || !s.outs[i - 1].flip)) { flips++; dir = s.outs[i].dir; }
		}
		CHECK_EQ(flips, 1);
		CHECK_EQ(dir, DIR_L);
	}

	snprintf(name, sizeof(name), "crouch down asks for regular, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		const size_t m = s.mark();
		s.toward(-90, 60);
		s.hold(300);
		const int crouch = s.firstDown(&Output::ollie, m);
		CHECK(crouch >= 0);
		for (size_t i = (size_t)crouch; i < s.outs.size(); i++) CHECK_EQ(s.outs[i].stance, STANCE_REGULAR);
		const size_t flick = s.mark();
		s.toward(80, 35);
		s.spring();
		s.hold(300);
		const int pop = s.firstUp(&Output::ollie, flick);
		CHECK(pop >= 0);
		for (size_t i = (size_t)pop; i < s.outs.size(); i++) CHECK_EQ(s.outs[i].stance, STANCE_ANY);
		CHECK_EQ(s.stances(STANCE_NOLLIE), 0);
		// and back at the centre without a flick: regular was asked for all along, nothing more
		Sim t(hz);
		t.toward(-90, 60);
		t.hold(300);
		const size_t back = t.mark();
		t.spring();
		t.hold(400);
		CHECK_EQ(t.count(&Output::standUp, back), 1);
		const int up = t.firstDown(&Output::standUp, back);
		CHECK_EQ(t.outs[(size_t)up].stance, STANCE_ANY);
		CHECK_EQ(t.stances(STANCE_NOLLIE), 0);
	}

	snprintf(name, sizeof(name), "without the game's side the Nollie button is tapped, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		s.keeps = false;
		s.toward(90, 60);
		s.hold(300);
		std::vector<int> taps;
		for (size_t i = 0; i < s.outs.size(); i++) {
			if (s.outs[i].nollie && (i == 0 || !s.outs[i - 1].nollie)) taps.push_back((int)i);
		}
		CHECK_EQ(taps.size(), 2u);
		CHECK(s.stances(STANCE_NOLLIE) > 0);    // still said: nobody listens
	}

	snprintf(name, sizeof(name), "Nollie=0, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		s.cfg.nollie = 0;
		s.toward(90, 60);
		s.hold(300);
		s.spring();
		s.hold(300);
		s.toward(-90, 60);
		s.hold(300);
		s.spring();
		s.hold(300);
		CHECK_EQ(s.stances(STANCE_ANY), (int)s.outs.size());
		CHECK_EQ(s.count(&Output::nollie), 0);
		CHECK_EQ(s.count(&Output::standUp), 1);  // the crouch down
	}

	snprintf(name, sizeof(name), "in the air stick up leans the board, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		s.state = STATE_AIR;
		const size_t m = s.mark();
		s.toward(90, 60);
		s.hold(300);
		CHECK_EQ(s.count(&Output::ollie, m), 0);
		CHECK_EQ(s.stances(STANCE_ANY, m), (int)(s.outs.size() - m));
		// a landing with the stick still up is a crouch up, as a landing with it down is a crouch
		s.state = STATE_GROUND;
		const size_t land = s.mark();
		s.hold(200);
		CHECK(s.outs.back().ollie);
		CHECK_EQ(s.outs.back().stance, STANCE_NOLLIE);
		const size_t back = s.mark();
		s.spring();
		s.hold(400);
		CHECK_EQ(s.count(&Output::standUp, back), 1);
		CHECK(s.count(&Output::ollie, land) > 0);
		// Up in the air leans the nose, and that is all it does: back at the centre the next push
		// starts the lean over (left first here, then up), as it always did
		Sim t(hz);
		t.state = STATE_AIR;
		t.hold(50);                             // a push that began on the ground is no lean
		t.toward(90, 50);
		t.hold(100);
		CHECK(t.rec.GetTilt().valid && t.rec.GetTilt().first == TILT_UP);
		t.spring();
		t.hold(50);
		t.toward(180, 50);
		t.hold(80);
		t.arcTo(90, 100);
		t.hold(50);
		CHECK_EQ(t.rec.GetTilt().first, TILT_SIDE);
		CHECK(t.rec.GetTilt().firstLeft);
	}

	snprintf(name, sizeof(name), "on a rail nothing is asked for, %g Hz", hz);
	g_case = name;
	{
		Sim s(hz);
		s.state = STATE_RAIL;
		const size_t m = s.mark();
		s.toward(90, 60);
		s.hold(300);
		CHECK_EQ(s.count(&Output::ollie, m), 0);
		CHECK_EQ(s.stances(STANCE_ANY, m), (int)(s.outs.size() - m));
	}

	snprintf(name, sizeof(name), "a crouch carried off the ground, %g Hz", hz);
	g_case = name;
	{
		// Rolled off a ledge with the stick up: Ollie stays down while the game would still jump,
		// and the stance is only for the ground
		Sim s(hz);
		s.toward(90, 60);
		s.hold(200);
		s.state = STATE_AIR;
		const size_t air = s.mark();
		s.hold(200);
		CHECK(s.outs.back().ollie);
		s.hold(600);
		CHECK(!s.outs.back().ollie);
		// once the game no longer jumps for it, the stick up is the board's lean, as it is
		// anywhere in the air
		CHECK(s.outs.back().stance == STANCE_NOLLIE || s.outs.back().stance == STANCE_ANY);
		for (size_t i = air; i < s.outs.size(); i++) {
			if (!s.outs[i].ollie) CHECK_EQ(s.outs[i].stance, STANCE_ANY);
		}
		// nor is a flick from up there a flip trick in the air
		s.toward(-45, 35);
		s.spring();
		s.hold(300);
		CHECK_EQ(s.count(&Output::flip, air), 0);
	}
}

// Stick patterns of my own in the clock notation the pattern parser has for tests: the nollie
// ones are the regular ones upside down
static const char* kPatterns =
	"global_tolerance_dist 0.4\n"
	"global_tolerance_time 10.0\n"
	"global_anticipation_delay 15\n"
	"pattern Ollie\n\ttolerance_dist 0.5\n\tclock 6 1.0\n\tclock 12 1.0\n"
	"pattern Kickflip\n\tmin_time 3\n\tclock 6 0.7\n\tclock 2 1.0\n"
	"pattern Heelflip\n\tclock 6 0.7\n\tclock 10 1.0\n"
	"pattern PopShuvit\n\ttolerance_dist 0.35\n\tclock 6 1.0\n\tclock 4 1.0\n\tclock 2 1.0\n"
	"pattern Nollie\n\ttolerance_dist 0.5\n\tclock 12 1.0\n\tclock 6 1.0\n"
	"pattern N_Kickflip\n\tmin_time 3\n\tclock 12 0.7\n\tclock 4 1.0\n"
	"pattern N_Heelflip\n\tclock 12 0.7\n\tclock 8 1.0\n"
	"pattern N_PopShuvit\n\ttolerance_dist 0.35\n\tclock 12 1.0\n\tclock 2 1.0\n\tclock 4 1.0\n";

static void testPatterns(double hz) {
	char name[128];
	PatternSet set;
	set.Parse(kPatterns, strlen(kPatterns));

	// Each nollie pattern drawn as such, and its regular one drawn upside down: the same trick
	// slot, and the stance asked for while the stick crouches. to: where the flick ends, for the
	// nollie (degrees); rim: a shove-it, round the rim by 2 o'clock (4 o'clock for the regular one)
	struct Case { const char* nollie; const char* regular; float to; bool rim; int slot; };
	const Case cases[] = {
		{ "Nollie", "Ollie", -90.0f, false, DIR_NONE },
		{ "N_Kickflip", "Kickflip", -30.0f, false, DIR_L },
		{ "N_Heelflip", "Heelflip", -150.0f, false, DIR_R },
		{ "N_PopShuvit", "PopShuvit", -30.0f, true, DIR_D },
	};
	for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
		for (int up = 1; up >= 0; up--) {
			const float sign = up ? 1.0f : -1.0f;
			const char* want = up ? cases[c].nollie : cases[c].regular;
			snprintf(name, sizeof(name), "pattern %s, %g Hz", want, hz);
			g_case = name;
			Sim s(hz);
			s.rec.UsePatterns(&set);
			const size_t m = s.mark();
			s.toward(90.0f * sign, 60);
			s.hold(300);
			if (cases[c].rim) {
				s.toward(30.0f * sign, 50);
				s.toward(cases[c].to * sign, 50);
			}
			else {
				s.toward(cases[c].to * sign, 40);
			}
			s.hold(120);
			s.spring();
			s.hold(400);
			const int crouch = s.firstDown(&Output::ollie, m);
			const int pop = crouch >= 0 ? s.firstUp(&Output::ollie, (size_t)crouch) : -1;
			CHECK(crouch >= 0 && pop > crouch);
			const char* pattern = 0;
			int flips = 0, dir = DIR_NONE;
			for (size_t i = m; i < s.outs.size(); i++) {
				if (s.outs[i].pattern) pattern = s.outs[i].pattern;
				if (s.outs[i].flip && !s.outs[i - 1].flip) { flips++; dir = s.outs[i].dir; }
				const bool crouched = (int)i >= crouch && (int)i < pop;
				CHECK_EQ(s.outs[i].stance, crouched ? (up ? STANCE_NOLLIE : STANCE_REGULAR) : STANCE_ANY);
			}
			CHECK(pattern && strcmp(pattern, want) == 0);
			if (pattern && strcmp(pattern, want) != 0) printf("    came out as %s\n", pattern);
			CHECK_EQ(flips, cases[c].slot == DIR_NONE ? 0 : 1);
			if (cases[c].slot != DIR_NONE) CHECK_EQ(dir, cases[c].slot);
			CHECK_EQ(s.count(&Output::standUp), 0);
		}
	}
}

int main(int argc, char** argv) {
	const std::string scriptDir = argc > 1 ? argv[1] : "scripts/qb/source/scripts/game/skater";
	testScripts(scriptDir);
	testKeeper();
	const double rates[] = { 30.0, 60.0, 120.0, 144.0, 240.0 };
	for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
		const int before = g_failures;
		testRecognizer(rates[i]);
		testPatterns(rates[i]);
		printf("  %3g Hz: %s\n", rates[i], g_failures == before ? "ok" : "FAILED");
	}
	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
