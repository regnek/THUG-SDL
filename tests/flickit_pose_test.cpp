// Tests of the crouch pose (src/flickit_pose.h). The Keeper is run on a list of animation channels
// laid out as the game lays them out, and the list is run the way the game runs it: PlayAnim with
// its start of a blend, the update of the channels, and the pose. Those are re-expressed here from
// what flickit_pose.h says of them, as the hooks see them: the start of a blend between the
// Keeper's BeforeBlend and AfterBlend, the pose between BeforePose and AfterPose.
//
// Memory for channels comes from a pool that hands out the last one given back first, as the
// game's does, so that a channel the game drops is soon a new one at the same address. Memory that
// was given back is checked to be untouched when it is handed out again.
#include "flickit_pose.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#if UINTPTR_MAX > 0xFFFFFFFFu
#include <sys/mman.h>
#endif
#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/asan_interface.h>
#define POISON(p, n) ASAN_POISON_MEMORY_REGION(p, n)
#define UNPOISON(p, n) ASAN_UNPOISON_MEMORY_REGION(p, n)
#else
#define POISON(p, n) ((void)0)
#define UNPOISON(p, n) ((void)0)
#endif

using namespace FlickitPose;

static int g_checks = 0;
static int g_failures = 0;
static const char* g_case = "";

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failures++; std::printf("  FAIL [%s] line %d: %s\n", g_case, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { g_checks++; const double a_ = (a), b_ = (b); if (!(std::fabs(a_ - b_) <= (tol))) { g_failures++; std::printf("  FAIL [%s] line %d: %s = %.6f, not %.6f\n", g_case, __LINE__, #a, a_, b_); } } while (0)

// The names the tests play (checksums as in the tables of flickit_pose.h)
static const uint32_t kIdle = 0x23DB4AEA;
static const uint32_t kCrouchIdle = 0x9529A543;
static const uint32_t kCrouchIdle2 = 0x2797680F;
static const uint32_t kCrouchLookLeft = 0xBA513E5D;
static const uint32_t kCrouchTurnLeft = 0xA1094976;
static const uint32_t kCrouchTurnLeftIdle = 0xBC6C0B31;
static const uint32_t kCrouchedLand = 0xBC825547;
static const uint32_t kOllie = 0x9B65D7B8;
static const uint32_t kPushCycle = 0x4266D67F;

static const uint8_t kFreed = 0xDD;

// The game's lists link with four bytes: memory for them comes from below 4 GB
static uint8_t* Low(size_t size) {
	static uint8_t* base = 0;
	static size_t used = 0;
	static const size_t kRoom = 1 << 20;
	if (!base) {
#if UINTPTR_MAX > 0xFFFFFFFFu
		void* p = mmap(0, kRoom, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
		if (p == MAP_FAILED) {
			std::printf("no memory below 4 GB\n");
			std::exit(2);
		}
		base = (uint8_t*)p;
#else
		base = (uint8_t*)std::malloc(kRoom);
#endif
	}
	size = (size + 15) & ~(size_t)15;
	if (used + size > kRoom) {
		std::printf("out of test memory\n");
		std::exit(2);
	}
	uint8_t* p = base + used;
	used += size;
	return p;
}

struct Pool {
	std::vector<uint8_t*> all;
	std::vector<uint8_t*> free;

	uint8_t* Get() {
		uint8_t* p;
		if (!free.empty()) {
			p = free.back();
			free.pop_back();
			UNPOISON(p, CHANNEL_SIZE);
			bool untouched = true;
			for (int i = 0; i < CHANNEL_SIZE; i++) {
				untouched = untouched && p[i] == kFreed;
			}
			CHECK(untouched);      // nothing wrote into it while it was free
		}
		else {
			p = Low(CHANNEL_SIZE);
			all.push_back(p);
		}
		std::memset(p, 0xCD, CHANNEL_SIZE);
		return p;
	}

	// With the address sanitizer on, touching a channel that has been given back stops the test
	void Put(uint8_t* p) {
		std::memset(p, kFreed, CHANNEL_SIZE);
		POISON(p, CHANNEL_SIZE);
		free.push_back(p);
	}

};

// An animation component: just its list of channels
struct Anim {
	uint8_t* mem;
	Pool* pool;
	Keeper* keeper;
	float speed;            // the speed of the animations it plays: RATE is speed / 60
	int most;               // the most channels it ever had

	Anim(Pool* p, Keeper* k) : mem(Low(0x40)), pool(p), keeper(k), speed(1.0f), most(0) {
		std::memset(mem, 0, 0x40);
		uint8_t* list = List();
		*(int32_t*)(list + NODE_KEY) = -1;
		SetNext(list, list);
		SetPrev(list, list);
	}

	uint8_t* List() { return mem + ANIM_CHANNELS; }

	int Get(uint8_t** out) { return Channels(mem, out); }

	uint8_t* First() {
		uint8_t* c[kMostChannels];
		return Get(c) > 0 ? c[0] : 0;
	}

	int Count() {
		uint8_t* c[kMostChannels];
		return Get(c);
	}

	void Remove(uint8_t* c) {
		SetNext(PrevOf(c), NextOf(c));
		SetPrev(NextOf(c), PrevOf(c));
		pool->Put(c);
	}

	// PlayAnim without a blend drops every channel first
	void Clear() {
		uint8_t* c[kMostChannels];
		const int n = Get(c);
		for (int i = 0; i < n; i++) {
			Remove(c[i]);
		}
	}

	// The start of a blend: every channel after the second goes, the first begins to blend out
	// weighing 1 less what the others still weigh (or is over at once, for a blend of no time),
	// and a new channel goes in front
	void BlendStart(float seconds) {
		uint8_t* c[kMostChannels];
		int n = Get(c);
		for (int i = 2; i < n; i++) {
			Remove(c[i]);
		}
		n = Get(c);
		if (n > 0) {
			uint8_t* head = c[0];
			if (seconds == 0.0f) {
				StateOf(head) = CHANNEL_DONE;
			}
			if (StateOf(head) == CHANNEL_PLAYING) {
				LeftOf(head) = seconds;
				StateOf(head) = CHANNEL_LEAVING;
				float others = 0.0f;
				for (int i = 1; i < n; i++) {
					if (StateOf(c[i]) == CHANNEL_LEAVING) {
						others += LeftOf(c[i]) * ScaleOf(c[i]);
					}
				}
				ScaleOf(head) = (1.0f - others) / LeftOf(head);
			}
		}
		uint8_t* fresh = pool->Get();
		*(int32_t*)(fresh + NODE_KEY) = 1;
		StateOf(fresh) = CHANNEL_PLAYING;
		LeftOf(fresh) = 0.0f;
		ScaleOf(fresh) = 0.0f;
		RateOf(fresh) = 0.0f;
		*(uint32_t*)(fresh + CHANNEL_NAME) = 0;
		uint8_t* list = List();
		SetPrev(fresh, list);
		SetNext(fresh, NextOf(list));
		SetPrev(NextOf(list), fresh);
		SetNext(list, fresh);
	}

	// PlayAnim: the channel in front gets the animation. With blending switched off (as it is for
	// the player's skater) or a blend of no time, every channel goes first.
	uint8_t* PlayAnim(uint32_t name, float blend) {
		if (blend == 0.0f || mem[ANIM_BLENDING] == 0) {
			Clear();
		}
		keeper->BeforeBlend(mem);
		BlendStart(blend);
		keeper->AfterBlend(mem);
		uint8_t* head = First();
		*(uint32_t*)(head + CHANNEL_NAME) = name;
		RateOf(head) = speed / 60.0f;
		StateOf(head) = CHANNEL_PLAYING;
		const int n = Count();
		most = n > most ? n : most;
		return head;
	}

	// The update, once a frame: the channels that blend out lose a frame's worth of what is left,
	// those that are over go, and the pose is made
	void Update(uint32_t now, float* shares, uint8_t** order, int* count) {
		uint8_t* c[kMostChannels];
		int n = Get(c);
		for (int i = 0; i < n; i++) {
			if (StateOf(c[i]) == CHANNEL_LEAVING) {
				LeftOf(c[i]) = LeftOf(c[i]) - RateOf(c[i]) * 1.0f;
				if (!(LeftOf(c[i]) > 0.0f) || LeftOf(c[i]) * ScaleOf(c[i]) < 0.0f) {
					StateOf(c[i]) = CHANNEL_DONE;
				}
			}
		}
		for (int i = 0; i < n; i++) {
			if (StateOf(c[i]) == CHANNEL_DONE) {
				Remove(c[i]);
			}
		}
		keeper->BeforePose(mem, now);
		n = Get(c);
		Shares(c, n, shares);
		for (int i = 0; i < n; i++) {
			order[i] = c[i];
		}
		*count = n;
		keeper->AfterPose(mem);
	}

	// The pose's shares: the channels after the first by their weights, the first by the number of
	// those that weigh something less their weights, all over that number
	static void Shares(uint8_t** c, int n, float* out) {
		if (n == 1) {
			out[0] = 1.0f;
			return;
		}
		float w[kMostChannels];
		int counted = 0;
		float sum = 0.0f;
		for (int i = 1; i < n; i++) {
			w[i] = WeightOf(c[i]);
			if (w[i] != 0.0f) {
				counted++;
			}
			sum += w[i];
		}
		if (counted == 0) {
			out[0] = 1.0f;
			for (int i = 1; i < n; i++) {
				out[i] = 0.0f;
			}
			return;
		}
		out[0] = ((float)counted - sum) / (float)counted;
		for (int i = 1; i < n; i++) {
			out[i] = w[i] / (float)counted;
		}
	}
};

// A run of frames: the stick (crouch or not, and how deep), the Keeper's frame, whatever the game's
// scripts play, the update with the pose
struct Run {
	Pool pool;
	Keeper keeper;
	Anim anim;
	uint32_t now;
	bool ground;
	bool scriptsFirst;      // the game's scripts run before the pad update (the other order is tried too)
	float shares[kMostChannels];
	uint8_t* order[kMostChannels];
	int count;
	int started, lost, missed;
	uint32_t from;
	bool crouched;          // the game has him crouched while the stick crouches (it follows a frame later in the game)
	Report last;

	Run() : anim(&pool, &keeper), now(10000), ground(true), scriptsFirst(false), count(0), started(0), lost(0), missed(0), from(0), crouched(true) {
		// rolling along
		anim.PlayAnim(kIdle, 0.0f);
		Frame(false, -1.0f);
	}

	struct Play {
		uint32_t name;
		float blend;
	};

	// One frame. "plays" are what the scripts play on it.
	void Frame(bool crouch, float depth, const Play* plays = 0, int nplays = 0) {
		if (scriptsFirst) {
			for (int i = 0; i < nplays; i++) {
				anim.PlayAnim(plays[i].name, plays[i].blend);
			}
		}
		Report r;
		keeper.Frame(anim.mem, ground, crouch, crouch && crouched, depth, now, r);
		if (r.started) {
			started++;
			from = r.from;
		}
		if (r.lost) {
			lost++;
		}
		if (r.missed) {
			missed++;
		}
		last = r;
		if (!scriptsFirst) {
			for (int i = 0; i < nplays; i++) {
				anim.PlayAnim(plays[i].name, plays[i].blend);
			}
		}
		anim.Update(now, shares, order, &count);
		now += 17;
	}

	void Frame(bool crouch, float depth, uint32_t name, float blend) {
		Play p = { name, blend };
		Frame(crouch, depth, &p, 1);
	}

	float ShareOf(const uint8_t* channel) const {
		for (int i = 0; i < count; i++) {
			if (order[i] == channel) {
				return shares[i];
			}
		}
		return 0.0f;
	}

	// The share of the pose in animations of a crouch, and in the others (standing)
	float CrouchShare() const {
		float s = 0.0f;
		for (int i = 0; i < count; i++) {
			if (IsCrouchAnim(NameOf(order[i]))) {
				s += shares[i];
			}
		}
		return s;
	}

	bool SharesSane() const {
		float sum = 0.0f;
		for (int i = 0; i < count; i++) {
			if (!(shares[i] >= -1e-5f && shares[i] <= 1.0f + 1e-5f)) {
				return false;
			}
			sum += shares[i];
		}
		return std::fabs(sum - 1.0f) < 1e-4f;
	}
};

static void testCrouchFollowsTheStick(bool scriptsFirst) {
	g_case = scriptsFirst ? "the crouch follows the stick (scripts first)" : "the crouch follows the stick";
	Run r;
	r.scriptsFirst = scriptsFirst;
	uint8_t* standing = r.anim.First();
	// the stick goes down: the game crouches, and plays its crouch with its usual blend
	r.Frame(true, 0.0f);
	r.Frame(true, 0.05f, kCrouchIdle, 0.3f);
	for (int i = 0; i < 2; i++) {
		r.Frame(true, 0.1f + 0.05f * i);
	}
	CHECK(r.started == 1);
	CHECK(r.from == kIdle);
	CHECK(r.keeper.Kept() == standing);
	// deeper and deeper: the pose follows, exactly
	for (int i = 0; i < 30; i++) {
		const float depth = 0.2f + 0.8f * (float)i / 29.0f;
		r.Frame(true, depth);
		CHECK(r.SharesSane());
		CHECK_NEAR(r.ShareOf(standing), depth < 1.0f ? 1.0f - depth : kLeastShare, 2e-5);
		CHECK_NEAR(r.CrouchShare(), depth < 1.0f ? depth : 1.0f - kLeastShare, 2e-5);
	}
	// all the way down for a long while: it stays, unseen
	for (int i = 0; i < 300; i++) {
		r.Frame(true, 1.0f);
	}
	CHECK(r.keeper.Kept() == standing);
	CHECK(r.ShareOf(standing) < 0.001f);
	// eased back to half way: back to half
	for (int i = 0; i < 10; i++) {
		r.Frame(true, 0.5f);
	}
	CHECK_NEAR(r.ShareOf(standing), 0.5, 2e-5);
	CHECK(r.lost == 0);
	CHECK(r.anim.most <= 3);
}

static void testCrouchAnimationsChange() {
	g_case = "the game's crouch animations change";
	Run r;
	uint8_t* standing = r.anim.First();
	r.Frame(true, 0.0f, kCrouchIdle, 0.3f);
	r.Frame(true, 0.7f);
	const uint32_t idles[] = { kCrouchIdle2, kCrouchLookLeft, kCrouchIdle, kCrouchTurnLeft, kCrouchTurnLeftIdle, kCrouchIdle2 };
	for (int k = 0; k < 6; k++) {
		uint8_t* was = r.anim.First();
		r.Frame(true, 0.7f, idles[k], 0.3f);
		// the crouch's own blend goes on as the game has it: the one it played before still has
		// all but a frame of its share, as if the stood-in pose were not there
		uint8_t* now = r.anim.First();
		CHECK_NEAR(r.ShareOf(now), 0.7 / 18.0, 0.002);
		CHECK_NEAR(r.ShareOf(was), 0.7 * 17.0 / 18.0, 0.002);
		for (int i = 0; i < 9; i++) {      // the next one comes in the middle of the blend of this one
			r.Frame(true, 0.7f);
			CHECK(r.SharesSane());
			CHECK_NEAR(r.ShareOf(standing), 0.3, 2e-5);
			CHECK_NEAR(r.CrouchShare(), 0.7, 2e-5);
			CHECK_NEAR(r.ShareOf(was), 0.7 * WeightOf(was), 2e-5);
		}
	}
	// two in one frame: a turn and its idle
	Run::Play two[] = { { kCrouchTurnLeft, 0.3f }, { kCrouchTurnLeftIdle, 0.3f } };
	r.Frame(true, 0.7f, two, 2);
	for (int i = 0; i < 30; i++) {
		r.Frame(true, 0.7f);
		CHECK_NEAR(r.ShareOf(standing), 0.3, 2e-5);
	}
	CHECK(r.keeper.Kept() == standing);
	CHECK(r.lost == 0);
	CHECK(r.anim.most <= 3);
}

static void testStandUp() {
	g_case = "standing up from half way";
	Run r;
	uint8_t* standing = r.anim.First();
	r.Frame(true, 0.0f, kCrouchIdle, 0.3f);
	for (int i = 0; i < 20; i++) {
		r.Frame(true, 0.5f);
	}
	// the stick comes back without a flick: the crouch is over, the game has not stood him up yet
	float before = 1.0f - r.CrouchShare();
	r.Frame(false, -1.0f);
	r.Frame(false, -1.0f);
	CHECK_NEAR(r.ShareOf(standing), 0.5, 2e-5);        // held while the game still crouches him
	// now it plays the animation he stands in, and the crouch goes
	r.Frame(false, -1.0f, kIdle, 0.3f);
	bool steady = true;
	for (int i = 0; i < 40; i++) {
		const float stand = 1.0f - r.CrouchShare();
		steady = steady && r.SharesSane() && stand >= before - 1e-5f;
		before = stand;
		r.Frame(false, -1.0f);
	}
	CHECK(steady);                                       // never back down into the crouch
	CHECK(r.CrouchShare() < 1e-6f);
	CHECK(r.keeper.Kept() == 0);
	CHECK(r.anim.Count() == 1);                          // the kept channel is gone, and so is the crouch
	CHECK(r.lost == 0);
}

static void testStandUpWaitsNoLonger() {
	g_case = "the crouch over, and the game crouches on";
	// The button held by the player keeps the game's crouch: the kept pose goes after a moment, and
	// he crouches as the game has it
	Run r;
	uint8_t* standing = r.anim.First();
	r.Frame(true, 0.0f, kCrouchIdle, 0.3f);
	for (int i = 0; i < 10; i++) {
		r.Frame(true, 0.4f);
	}
	int frames = 0;
	while (r.keeper.Kept() && frames < 100) {
		r.Frame(false, -1.0f);
		frames++;
	}
	CHECK(r.keeper.Kept() == 0);
	CHECK(frames * 17 >= (int)(kFadeWaitMs + kFadeMs) - 34 && frames * 17 <= (int)(kFadeWaitMs + kFadeMs) + 51);
	CHECK(r.ShareOf(standing) == 0.0f);
	CHECK(r.CrouchShare() > 0.999f);
}

static void testPop() {
	g_case = "the pop";
	for (int order = 0; order < 2; order++) {
		// the pop: the crouch ends on the frame of the pop, and the ollie starts without a blend.
		// order 0: the Keeper hears of the end first; 1: the game drops everything first.
		Run r;
		r.scriptsFirst = order == 1;
		r.Frame(true, 0.0f);
		r.Frame(true, 0.1f, kCrouchIdle, 0.3f);
		for (int i = 0; i < 10; i++) {
			r.Frame(true, 0.8f);
		}
		CHECK(r.keeper.Kept() != 0);
		uint8_t* ollie = 0;
		if (order == 0) {
			r.Frame(false, -1.0f, kOllie, 0.0f);
		}
		else {
			r.Frame(true, 0.8f, kOllie, 0.0f);   // the Keeper still thinks he crouches
		}
		ollie = r.anim.First();
		const float rate = RateOf(ollie);
		for (int i = 0; i < 5; i++) {
			r.Frame(false, -1.0f);
		}
		CHECK(r.keeper.Kept() == 0);
		CHECK(r.anim.Count() == 1);
		CHECK(r.ShareOf(ollie) == 1.0f);
		CHECK(RateOf(ollie) == rate);               // nothing touched the new channel
		CHECK(LeftOf(ollie) == 0.0f && ScaleOf(ollie) == 0.0f);
		CHECK(r.lost == (order == 1 ? 1 : 0));
	}
}

static void testCrouchInALanding() {
	g_case = "a crouch begun in a landing";
	// He lands, and pulls the stick down while the landing plays: the game plays no crouch until the
	// landing is over. Then the landing is what he stood in.
	Run r;
	r.Frame(false, -1.0f, kCrouchedLand, 0.1f);
	uint8_t* land = r.anim.First();
	for (int i = 0; i < 10; i++) {
		r.Frame(true, 0.3f);
	}
	CHECK(r.keeper.Kept() == 0);
	r.Frame(true, 0.4f, kCrouchIdle, 0.3f);
	r.Frame(true, 0.4f);
	CHECK(r.keeper.Kept() == land);
	CHECK(r.from == kCrouchedLand);
	CHECK_NEAR(r.ShareOf(land), 0.6, 2e-5);
}

static void testNotOnTheGround() {
	g_case = "not the stick's crouch";
	// A crouch of the game's own (the button), or none: nothing is touched
	// The game's crouch as it always was: it takes over at once
	Run r;
	r.Frame(false, -1.0f, kCrouchIdle, 0.3f);
	r.Frame(false, -1.0f);
	CHECK(r.keeper.Kept() == 0);
	CHECK(r.anim.Count() == 1);
	CHECK(r.anim.mem[ANIM_BLENDING] == 0);
	CHECK(r.ShareOf(r.anim.First()) == 1.0f);
	// the stick's crouch in the air: the animation he was in is not one he stood in
	Run a;
	a.ground = false;
	a.Frame(false, -1.0f, kOllie, 0.0f);
	uint8_t* ollie = a.anim.First();
	a.Frame(true, 0.5f, kCrouchIdle, 0.3f);
	a.Frame(true, 0.5f);
	CHECK(a.keeper.Kept() == 0);
	CHECK(a.lost == 0 && a.started == 0);
	CHECK(LeftOf(ollie) < 0.3f);                  // its blend runs out as the game has it
}

static void testNewSkater() {
	g_case = "another skater";
	// The skater's animation component changes in a crouch (a new level, another skater): the old
	// one is left alone, and the log hears of it
	Run r;
	r.Frame(true, 0.0f, kCrouchIdle, 0.3f);
	for (int i = 0; i < 5; i++) {
		r.Frame(true, 0.5f);
	}
	uint8_t* kept = r.keeper.Kept();
	CHECK(kept != 0);
	Anim other(&r.pool, &r.keeper);
	other.PlayAnim(kIdle, 0.0f);
	Report rep;
	r.keeper.Frame(other.mem, true, true, true, 0.5f, r.now, rep);
	CHECK(rep.lost);
	CHECK(r.keeper.Kept() == 0);
	const float left = LeftOf(kept);
	float shares[kMostChannels];
	uint8_t* order[kMostChannels];
	int n = 0;
	r.anim.Update(r.now, shares, order, &n);
	CHECK_NEAR(LeftOf(kept), left - 1.0f / 60.0f, 1e-6);
	// the same with the crouch over: its share was going anyway, nothing to tell
	Run f;
	f.Frame(true, 0.0f, kCrouchIdle, 0.3f);
	for (int i = 0; i < 5; i++) {
		f.Frame(true, 0.5f);
	}
	f.Frame(false, -1.0f);
	CHECK(f.keeper.Fading());
	Anim third(&f.pool, &f.keeper);
	third.PlayAnim(kIdle, 0.0f);
	f.keeper.Frame(third.mem, true, false, false, -1.0f, f.now, rep);
	CHECK(!rep.lost);
	CHECK(f.keeper.Kept() == 0);
}

static void testFadeWithoutFrames() {
	g_case = "the share goes without the pad update";
	// The crouch is over and the share is going; the pad update stops: the pose still ends it
	// when its time is up
	Run r;
	r.Frame(true, 0.0f, kCrouchIdle, 0.3f);
	for (int i = 0; i < 10; i++) {
		r.Frame(true, 0.5f);
	}
	r.Frame(false, -1.0f, kIdle, 0.3f);
	CHECK(r.keeper.Fading());
	float shares[kMostChannels];
	uint8_t* order[kMostChannels];
	int n = 0;
	int frames = 0;
	while (r.keeper.Kept() && frames < 100) {
		r.anim.Update(r.now, shares, order, &n);
		r.now += 17;
		frames++;
	}
	CHECK(frames * 17 <= (int)kFadeMs + 34);
	r.anim.Update(r.now, shares, order, &n);
	r.anim.Update(r.now, shares, order, &n);
	CHECK(r.anim.Count() == 1);                  // gone at once, not when its own blend runs out
}

static void testListCleared() {
	g_case = "the list cleared under it";
	// The game empties the list without starting a blend (a new animation set at a level's load):
	// the Keeper finds out on the next frame and reads nothing of what was there
	Run r;
	r.Frame(true, 0.0f, kCrouchIdle, 0.3f);
	for (int i = 0; i < 5; i++) {
		r.Frame(true, 0.5f);
	}
	CHECK(r.keeper.Kept() != 0);
	r.anim.Clear();
	r.Frame(true, 0.5f);                          // an empty list for a frame
	CHECK(r.keeper.Kept() == 0);
	CHECK(r.lost == 1);
	r.anim.BlendStart(0.3f);
	*(uint32_t*)(r.anim.First() + CHANNEL_NAME) = kIdle;
	r.Frame(true, 0.5f);
	CHECK(r.keeper.Kept() == 0);
	CHECK(r.lost == 1);
}

static void testRecrouch() {
	g_case = "crouched again while standing up";
	Run r;
	uint8_t* standing = r.anim.First();
	r.Frame(true, 0.0f, kCrouchIdle, 0.3f);
	for (int i = 0; i < 10; i++) {
		r.Frame(true, 0.6f);
	}
	r.Frame(false, -1.0f, kIdle, 0.3f);
	for (int i = 0; i < 5; i++) {
		r.Frame(false, -1.0f);
	}
	CHECK(r.keeper.Fading());
	// the stick down again before the old pose has gone: the game crouches again
	uint8_t* idle = r.anim.First();
	r.Frame(true, 0.2f, kCrouchIdle, 0.3f);
	for (int i = 0; i < 10; i++) {
		r.Frame(true, 0.3f);
	}
	CHECK(r.keeper.Holding());
	CHECK(r.keeper.Kept() == standing || r.keeper.Kept() == idle);
	float standingShare = 0.0f;
	for (int i = 0; i < r.count; i++) {
		if (!IsCrouchAnim(NameOf(r.order[i]))) {
			standingShare += r.shares[i];
		}
	}
	CHECK(r.SharesSane());
	CHECK(standingShare >= 0.7f - 1e-4f);
	CHECK(r.anim.most <= 3);
}

static void testKeeperQuiet() {
	g_case = "the Keeper not asked";
	// The pad update stops (the Keeper is not called): the kept channel runs out at the game's
	// pace, and nothing is touched after it has gone
	Run r;
	r.Frame(true, 0.0f, kCrouchIdle, 0.3f);
	for (int i = 0; i < 5; i++) {
		r.Frame(true, 0.5f);
	}
	CHECK(r.keeper.Kept() != 0);
	float shares[kMostChannels];
	uint8_t* order[kMostChannels];
	int n = 0;
	for (int i = 0; i < 90; i++) {
		r.anim.Update(r.now, shares, order, &n);
		r.now += 17;
	}
	CHECK(r.keeper.Kept() == 0);
	CHECK(n == 1);
	// and a new crouch works as before
	r.Frame(false, -1.0f, kIdle, 0.3f);
	for (int i = 0; i < 20; i++) {
		r.Frame(false, -1.0f);
	}
	uint8_t* standing = r.anim.First();
	r.Frame(true, 0.0f, kCrouchIdle, 0.3f);
	r.Frame(true, 0.25f);
	CHECK(r.keeper.Kept() == standing);
	CHECK_NEAR(r.ShareOf(standing), 0.75, 2e-5);
}

static void testOtherObjects() {
	g_case = "other objects";
	// Other animation components start blends and make poses too: theirs are left alone
	Run r;
	r.Frame(true, 0.0f, kCrouchIdle, 0.3f);
	r.Frame(true, 0.5f);
	Pool pool;
	Anim other(&pool, &r.keeper);
	other.mem[ANIM_BLENDING] = 1;
	other.PlayAnim(kIdle, 0.0f);
	other.PlayAnim(kPushCycle, 0.3f);
	uint8_t* c[kMostChannels];
	other.Get(c);
	const float left = LeftOf(c[1]);
	const float scale = ScaleOf(c[1]);
	float shares[kMostChannels];
	uint8_t* order[kMostChannels];
	int n = 0;
	other.Update(r.now, shares, order, &n);
	CHECK(n == 2);
	CHECK_NEAR(shares[1], (left - 1.0f / 60.0f) * scale, 1e-6);
	CHECK(ScaleOf(c[1]) == scale);
	r.Frame(true, 0.5f);
	CHECK(r.keeper.Kept() != 0);
	CHECK_NEAR(r.CrouchShare(), 0.5, 2e-5);
}

static void testFastAnimations() {
	g_case = "animations at other speeds";
	// A slow or fast animation counts its blend down at its own pace; the kept one does not
	const float speeds[] = { 0.5f, 2.0f };
	for (int k = 0; k < 2; k++) {
		Run r;
		r.anim.speed = speeds[k];
		r.anim.PlayAnim(kIdle, 0.3f);
		for (int i = 0; i < 30; i++) {
			r.Frame(false, -1.0f);
		}
		uint8_t* standing = r.anim.First();
		r.Frame(true, 0.0f, kCrouchIdle, 0.3f);
		for (int i = 0; i < 60; i++) {
			r.Frame(true, 0.3f);
			CHECK_NEAR(r.ShareOf(standing), 0.7, 2e-5);
		}
	}
}

static void testBlendingSwitch() {
	g_case = "blending for the crouch";
	// The player's skater does not blend: blending goes on while the stick crouches, and back to
	// what it was once the crouch is over
	{
		Run r;
		CHECK(r.anim.mem[ANIM_BLENDING] == 0);
		r.Frame(false, -1.0f, kPushCycle, 0.3f);
		CHECK(r.anim.Count() == 1);                  // the game's own: no blend
		r.Frame(true, 0.0f);                         // the stick crouches; the game a frame later
		CHECK(r.anim.mem[ANIM_BLENDING] == 1);
		r.Frame(true, 0.1f, kCrouchIdle, 0.3f);
		r.Frame(true, 0.3f);
		CHECK(r.keeper.Kept() != 0);
		CHECK(r.from == kPushCycle);
		r.Frame(false, -1.0f, kOllie, 0.0f);         // the pop
		r.Frame(false, -1.0f);
		CHECK(r.anim.mem[ANIM_BLENDING] == 0);
		r.Frame(false, -1.0f, kIdle, 0.3f);
		CHECK(r.anim.Count() == 1);
	}
	g_case = "blending back after standing up";
	{
		Run r;
		r.Frame(true, 0.0f);
		r.Frame(true, 0.1f, kCrouchIdle, 0.3f);
		for (int i = 0; i < 10; i++) {
			r.Frame(true, 0.5f);
		}
		r.Frame(false, -1.0f, kIdle, 0.3f);
		CHECK(r.anim.mem[ANIM_BLENDING] == 1);       // the stand-up blends too
		CHECK(r.anim.Count() == 3);
		for (int i = 0; i < 30; i++) {
			r.Frame(false, -1.0f);
		}
		CHECK(r.keeper.Kept() == 0);
		CHECK(r.anim.mem[ANIM_BLENDING] == 0);
	}
	g_case = "blending that was on";
	{
		Run r;
		r.anim.mem[ANIM_BLENDING] = 1;
		r.Frame(true, 0.0f);
		r.Frame(true, 0.1f, kCrouchIdle, 0.3f);
		r.Frame(true, 0.5f);
		r.Frame(false, -1.0f, kOllie, 0.0f);
		r.Frame(false, -1.0f);
		CHECK(r.anim.mem[ANIM_BLENDING] == 1);
	}
	g_case = "blending: another component";
	{
		Run r;
		r.Frame(true, 0.0f);
		CHECK(r.anim.mem[ANIM_BLENDING] == 1);
		Anim other(&r.pool, &r.keeper);
		other.PlayAnim(kIdle, 0.0f);
		Report rep;
		r.keeper.Frame(other.mem, true, false, false, -1.0f, r.now, rep);
		CHECK(r.anim.mem[ANIM_BLENDING] == 1);       // the old one is not touched any more
		CHECK(other.mem[ANIM_BLENDING] == 0);
		// and the same when the new one's stick crouches straight away
		Run q;
		q.Frame(true, 0.0f);
		Anim third(&q.pool, &q.keeper);
		third.PlayAnim(kIdle, 0.0f);
		q.keeper.Frame(third.mem, true, true, false, 0.2f, q.now, rep);
		CHECK(q.anim.mem[ANIM_BLENDING] == 1);
		CHECK(third.mem[ANIM_BLENDING] == 1);
	}
	g_case = "a crouch with nothing to keep";
	{
		// The game's crouch was already playing when the stick's began: nothing he stood in is
		// left. The log hears of it once.
		Run r;
		r.Frame(false, -1.0f, kCrouchIdle, 0.3f);
		for (int i = 0; i < 40; i++) {
			r.Frame(true, 0.5f);
		}
		CHECK(r.keeper.Kept() == 0);
		CHECK(r.missed == 1);
		CHECK(r.started == 0);
	}
}

int main() {
	testCrouchFollowsTheStick(false);
	testCrouchFollowsTheStick(true);
	testCrouchAnimationsChange();
	testStandUp();
	testStandUpWaitsNoLonger();
	testPop();
	testCrouchInALanding();
	testNotOnTheGround();
	testRecrouch();
	testKeeperQuiet();
	testOtherObjects();
	testFastAnimations();
	testNewSkater();
	testFadeWithoutFrames();
	testListCleared();
	testBlendingSwitch();
	std::printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
