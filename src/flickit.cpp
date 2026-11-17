#include "flickit.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace Flickit {

	// Staying inside a wind-up tolerates this much angular drift past the sector edge,
	// so a thumb resting near "down-left" does not pop by accident.
	static const float kHalfMargin = 10.0f;

	// Minimum press and gap of one Nollie button tap
	static const int kTapMs = 30;

	// A clean push of the stick in the air is read this long after it left the centre, when it has
	// arrived where it was going
	static const int kFirstPushMs = 40;

	// A crouch that is carried off the ground lets go of Ollie after this long in the air. The game
	// still jumps for a third of a second after the ground is gone (Skater_Late_Jump_Slop), so the
	// button is released only once that has passed and the release does nothing.
	static const int kAirReleaseMs = 450;

	// Switching grinds on a rail: press and gap of the two buttons, and how long the game gets to
	// start the new grind before the switch may be sent again
	static const int kSwitchTapMs = 30;
	static const int kSwitchGapMs = 15;
	static const int kSwitchWaitMs = 600;
	static const int kSwitchWaitFrames = 24;

	// The speed of the stick is its travel over about this long
	static const int kSpeedWindowMs = 60;

	// The crouch follows how far the stick is pulled. As Skate's does, it gets deeper over a fifth
	// of a second at most (from none to a full one), and comes up at once. A pull to this share of
	// the stick's travel is a full crouch, the most pop there is (Skate: a wind-up starts at 0.9).
	static const int kCrouchRampMs = 200;
	static const int kCrouchDropMs = 50;
	static const float kCrouchFull = 0.9f;
	// How strong a pop is, is how deep the crouch was just before the flick: the deepest it was in
	// this long before the stick left it
	static const int kCrouchPeakMs = 150;

	// Stepping off a rail: how long the button is pressed, and how long the game gets to act on
	// it before it is pressed again
	static const int kOffTapMs = 50;
	static const int kOffWaitMs = 500;
	static const int kOffWaitFrames = 20;

	// Gravity and pop: the least a setting other than 0 can be, in percent of Skate's
	static const int kJumpLeastPercent = 25;
	// The same for the push and the steering
	static const int kRollLeastPercent = 25;
	static const int kSpinLeastPercent = 25;

	static inline int32_t Since(uint32_t now, uint32_t then) {
		return (int32_t)(now - then);
	}

	static inline int Clamp(int v, int lo, int hi) {
		return v < lo ? lo : (v > hi ? hi : v);
	}

	void SetDefaults(Settings& s) {
		s.enabled = 1;
		s.toggleButton = 8;      // SDL_CONTROLLER_BUTTON_RIGHTSTICK
		s.reloadKey = 66;        // SDL_SCANCODE_F9
		s.debug = 1;

		s.kickflipSide = 0;
		s.switchMirror = 1;
		s.nollie = 1;
		s.nollieTaps = 2;
		s.triggerGrabs = 1;
		s.autoGrind = 1;

		s.deadzone = 25;
		s.engage = 35;
		s.release = 30;
		s.flick = 40;
		s.snap = 60;

		s.flickWindow = 200;
		s.settle = 50;
		s.dwell = 60;
		s.afterPop = 10;
		s.dirLead = 15;
		s.pulse = 30;
		s.autoGrindDelay = 200;
		s.autoGrindHold = 1500;

		s.patterns = 1;
		s.missTicks = 10;

		s.gameHooks = 1;
		s.grindCatch = 1;
		s.grindPick = 1;
		s.grindSwitch = 1;
		s.grindBalance = 0;
		s.grindMirror = 0;
		s.pauseOffBoard = 1;
		s.tilt = 50;
		s.tiltSettle = 150;
		s.switchHold = 250;
		s.railTap = 50;
		s.flickSpeed = 100;
		s.grindDrag = 100;
		s.grindExit = 1;
		s.grindBoost = 0;
		s.grindReach = 100;
		s.airGravity = 100;
		s.jumpPop = 100;
		s.crouchPose = 1;

		s.push = 100;
		s.pushOnOllie = 1;
		s.pushOverTurn = 1;
		s.pushMeasure = 1;
		s.pushFootDown = 8;
		s.pushFootUp = 32;
		s.coast = 100;
		s.steering = 100;

		s.spin = 100;
	}

	void Sanitize(Settings& s) {
		s.enabled = s.enabled ? 1 : 0;
		s.debug = s.debug ? 1 : 0;
		s.kickflipSide = s.kickflipSide ? 1 : 0;
		s.switchMirror = s.switchMirror ? 1 : 0;
		s.nollie = s.nollie ? 1 : 0;
		s.nollieTaps = Clamp(s.nollieTaps, 1, 3);
		s.triggerGrabs = s.triggerGrabs ? 1 : 0;
		s.autoGrind = s.autoGrind ? 1 : 0;

		s.deadzone = Clamp(s.deadzone, 5, 60);
		s.release = Clamp(s.release, s.deadzone + 5, 90);
		s.engage = Clamp(s.engage, s.release + 5, 98);
		s.flick = Clamp(s.flick, 20, 98);
		s.snap = Clamp(s.snap, s.flick, 100);

		s.flickWindow = Clamp(s.flickWindow, 50, 600);
		s.settle = Clamp(s.settle, 0, 300);
		s.dwell = Clamp(s.dwell, 0, 500);
		s.afterPop = Clamp(s.afterPop, 1, 200);
		s.dirLead = Clamp(s.dirLead, 0, 100);
		s.pulse = Clamp(s.pulse, 10, 300);
		s.autoGrindDelay = Clamp(s.autoGrindDelay, 0, 2000);
		s.autoGrindHold = Clamp(s.autoGrindHold, 0, 10000);

		s.patterns = s.patterns ? 1 : 0;
		s.missTicks = Clamp(s.missTicks, 1, 60);

		s.gameHooks = s.gameHooks ? 1 : 0;
		s.grindCatch = s.grindCatch ? 1 : 0;
		s.grindPick = s.grindPick ? 1 : 0;
		s.grindSwitch = s.grindSwitch ? 1 : 0;
		s.grindBalance = s.grindBalance ? 1 : 0;
		s.grindMirror = s.grindMirror ? 1 : 0;
		s.pauseOffBoard = s.pauseOffBoard ? 1 : 0;
		s.tilt = Clamp(s.tilt, s.deadzone + 5, 95);
		s.tiltSettle = Clamp(s.tiltSettle, 0, 1000);
		s.switchHold = Clamp(s.switchHold, 50, 2000);
		s.railTap = Clamp(s.railTap, 10, 300);
		s.flickSpeed = Clamp(s.flickSpeed, 0, 1000);
		s.grindDrag = Clamp(s.grindDrag, 0, 1000);
		s.grindExit = s.grindExit ? 1 : 0;
		s.grindBoost = s.grindBoost ? 1 : 0;
		s.grindReach = Clamp(s.grindReach, 0, kReachMostPercent);
		s.airGravity = s.airGravity <= 0 ? 0 : Clamp(s.airGravity, kJumpLeastPercent, kJumpMostPercent);
		s.jumpPop = s.jumpPop <= 0 ? 0 : Clamp(s.jumpPop, kJumpLeastPercent, kJumpMostPercent);
		s.crouchPose = Clamp(s.crouchPose, 0, 1);

		s.push = s.push <= 0 ? 0 : Clamp(s.push, kRollLeastPercent, kRollMostPercent);
		s.pushOnOllie = s.pushOnOllie ? 1 : 0;
		s.pushOverTurn = s.pushOverTurn ? 1 : 0;
		s.pushMeasure = s.pushMeasure ? 1 : 0;
		s.pushFootDown = Clamp(s.pushFootDown, 0, 90);
		s.pushFootUp = Clamp(s.pushFootUp, s.pushFootDown + 5, 100);
		s.coast = Clamp(s.coast, 0, 1000);
		s.steering = s.steering <= 0 ? 0 : Clamp(s.steering, kRollLeastPercent, kRollMostPercent);

		s.spin = s.spin <= 0 ? 0 : Clamp(s.spin, kSpinLeastPercent, kSpinMostPercent);
	}

	float AngleDeg(float x, float y) {
		float a = std::atan2(y, x) * 57.29577951f;
		if (a < 0.0f) {
			a += 360.0f;
		}
		return a;
	}

	int Sector(float x, float y) {
		const float a = AngleDeg(x, y);
		return ((int)std::floor((a + 22.5f) / 45.0f)) & 7;
	}

	bool InWindupHalf(float angleDeg, int wind, float marginDeg) {
		const float center = (wind == WIND_OLLIE) ? 270.0f : 90.0f;
		float diff = std::fabs(angleDeg - center);
		if (diff > 180.0f) {
			diff = 360.0f - diff;
		}
		return diff <= 67.5f + marginDeg;
	}

	// How many of the eight directions lie between two (0 to 4)
	static int SectorGap(int a, int b) {
		const int d = (a - b) & 7;
		return d > 4 ? 8 - d : d;
	}

	static int MirrorX(int d) {
		switch (d) {
		case DIR_R: return DIR_L;
		case DIR_L: return DIR_R;
		case DIR_UR: return DIR_UL;
		case DIR_UL: return DIR_UR;
		case DIR_DR: return DIR_DL;
		case DIR_DL: return DIR_DR;
		default: return d;
		}
	}

	static int MirrorY(int d) {
		switch (d) {
		case DIR_U: return DIR_D;
		case DIR_D: return DIR_U;
		case DIR_UR: return DIR_DR;
		case DIR_DR: return DIR_UR;
		case DIR_UL: return DIR_DL;
		case DIR_DL: return DIR_UL;
		default: return d;
		}
	}

	// Which flip slot a flick asks for. The answer is the direction that goes with the
	// Flip button, so with THUG's default trick layout:
	//   Left = Kickflip, Right = Heelflip, Up = Impossible, Down = Pop Shove-It,
	//   UpLeft = Hardflip, UpRight = Inward Heelflip,
	//   DownLeft = Varial Kickflip, DownRight = Varial Heelflip
	int MapFlip(int wind, int start, int end, int kickflipSide) {
		if (end == DIR_NONE || start == DIR_NONE) {
			return DIR_NONE;
		}
		if (wind == WIND_NOLLIE) {
			start = MirrorY(start);
			end = MirrorY(end);
		}
		if (kickflipSide == 1) {
			start = MirrorX(start);
			end = MirrorX(end);
		}

		// From here on: wind-up in the lower half, kickflip to the upper right
		switch (start) {
		case DIR_D:
			switch (end) {
			case DIR_UR: return DIR_L;    // Kickflip
			case DIR_UL: return DIR_R;    // Heelflip
			case DIR_R: return DIR_D;     // Pop Shove-It (backside)
			case DIR_L: return DIR_U;     // frontside shove-it slot
			default: return DIR_NONE;     // straight up: ollie
			}
		case DIR_DL:
			switch (end) {
			case DIR_UR: return DIR_DL;   // Varial Kickflip
			case DIR_U: return DIR_UR;    // Inward Heelflip
			case DIR_UL: return DIR_UR;
			case DIR_R: return DIR_D;
			case DIR_L: return DIR_U;
			default: return DIR_NONE;
			}
		case DIR_DR:
			switch (end) {
			case DIR_UL: return DIR_DR;   // Varial Heelflip
			case DIR_U: return DIR_UL;    // Hardflip
			case DIR_UR: return DIR_UL;
			case DIR_L: return DIR_U;
			case DIR_R: return DIR_D;
			default: return DIR_NONE;
			}
		default:
			return DIR_NONE;
		}
	}

	const char* DirName(int dir) {
		switch (dir) {
		case DIR_R: return "R";
		case DIR_UR: return "UR";
		case DIR_U: return "U";
		case DIR_UL: return "UL";
		case DIR_L: return "L";
		case DIR_DL: return "DL";
		case DIR_D: return "D";
		case DIR_DR: return "DR";
		default: return "-";
		}
	}

	const char* SlotName(int slot) {
		switch (slot) {
		case DIR_L: return "Left (Kickflip)";
		case DIR_R: return "Right (Heelflip)";
		case DIR_U: return "Up (Impossible)";
		case DIR_D: return "Down (Pop Shove-It)";
		case DIR_UL: return "UpLeft (Hardflip)";
		case DIR_UR: return "UpRight (Inward Heelflip)";
		case DIR_DL: return "DownLeft (Varial Kickflip)";
		case DIR_DR: return "DownRight (Varial Heelflip)";
		default: return "none";
		}
	}

	// ------------------------------------------------------------------------------------------
	// Stick patterns
	// ------------------------------------------------------------------------------------------

	// Trick names as Skate's pattern files spell them, and the THUG flip slot each one gets.
	// The 360 versions are the base trick plus a second tap of Flip, which is how THUG does them.
	struct TrickName {
		const char* name;
		int slot;
		int taps;
	};

	static const TrickName kTrickNames[] = {
		{ "Ollie", DIR_NONE, 0 },
		{ "Kickflip", DIR_L, 1 },
		{ "Heelflip", DIR_R, 1 },
		{ "PopShuvit", DIR_D, 1 },
		{ "FSPopShuvit", DIR_U, 1 },
		{ "VarialKickflip", DIR_DL, 1 },
		{ "VarialHeelflip", DIR_DR, 1 },
		{ "Hardflip", DIR_UL, 1 },
		{ "InwardHeelflip", DIR_UR, 1 },
		{ "360PopShuvit", DIR_D, 2 },
		{ "FS360PopShuvit", DIR_U, 2 },
		{ "360Flip", DIR_DL, 2 },
		{ "Laserflip", DIR_DR, 2 },
		{ "360Hardflip", DIR_UL, 2 },
		{ "360InwardHeelflip", DIR_UR, 2 },
	};

	int NamePattern(Pattern& p) {
		p.known = 0;
		p.slot = DIR_NONE;
		p.taps = 0;
		p.nollie = 0;

		const char* name = p.name;
		if (std::strcmp(name, "Nollie") == 0) {
			p.known = 1;
			p.nollie = 1;
			return 1;
		}
		if (std::strncmp(name, "N_", 2) == 0) {
			p.nollie = 1;
			name += 2;
		}
		for (size_t i = 0; i < sizeof(kTrickNames) / sizeof(kTrickNames[0]); i++) {
			if (std::strcmp(name, kTrickNames[i].name) == 0) {
				if (p.nollie && kTrickNames[i].slot == DIR_NONE) {
					break;   // "N_Ollie" is not a name the files use
				}
				p.known = 1;
				p.slot = kTrickNames[i].slot;
				p.taps = kTrickNames[i].taps;
				return 1;
			}
		}
		p.nollie = 0;
		return 0;
	}

	PatternSet::PatternSet() {
		Clear();
	}

	void PatternSet::Clear() {
		m_count = 0;
	}

	// Copies the next whitespace separated word of the text into word. Returns false at the end.
	static bool NextWord(const char* text, size_t length, size_t& pos, char* word, size_t wordSize) {
		while (pos < length && (unsigned char)text[pos] <= ' ') {
			pos++;
		}
		if (pos >= length) {
			return false;
		}
		size_t n = 0;
		while (pos < length && (unsigned char)text[pos] > ' ') {
			if (n + 1 < wordSize) {
				word[n++] = text[pos];
			}
			pos++;
		}
		word[n] = '\0';
		return true;
	}

	int PatternSet::Parse(const char* text, size_t length) {
		float globalTolerance = 0.4f;
		Pattern* cur = 0;
		size_t pos = 0;
		char word[64];
		char arg[64];

		while (NextWord(text, length, pos, word, sizeof(word))) {
			if (std::strcmp(word, "pattern") == 0) {
				cur = 0;
				if (!NextWord(text, length, pos, arg, sizeof(arg))) {
					break;
				}
				if (m_count < MAX_PATTERNS) {
					cur = &m_patterns[m_count++];
					size_t k = 0;
					for (; k + 1 < (size_t)MAX_PATTERN_NAME && arg[k] != '\0'; k++) {
						cur->name[k] = arg[k];
					}
					cur->name[k] = '\0';
					cur->tolerance = globalTolerance;
					cur->count = 0;
					NamePattern(*cur);
				}
			}
			else if (std::strcmp(word, "coord") == 0 || std::strcmp(word, "clock") == 0) {
				char second[64];
				if (!NextWord(text, length, pos, arg, sizeof(arg)) || !NextWord(text, length, pos, second, sizeof(second))) {
					break;
				}
				float x = (float)std::atof(arg);
				float y = (float)std::atof(second);
				if (word[1] == 'l') {
					// clock face: hour, radius. 12 is up, 3 is right, 0 is the center
					const int hour = std::atoi(arg);
					const float radius = y;
					const float a = (float)hour * 0.5235987756f;
					x = (hour == 0) ? 0.0f : std::sin(a) * radius;
					y = (hour == 0) ? 0.0f : -std::cos(a) * radius;
				}
				if (cur && cur->count < MAX_PATTERN_POINTS) {
					cur->x[cur->count] = x;
					cur->y[cur->count] = y;
					cur->count++;
				}
			}
			else if (std::strcmp(word, "tolerance_dist") == 0 || std::strcmp(word, "global_tolerance_dist") == 0) {
				if (!NextWord(text, length, pos, arg, sizeof(arg))) {
					break;
				}
				float t = (float)std::atof(arg);
				if (t < 0.05f) {
					t = 0.05f;
				}
				if (word[0] == 'g') {
					globalTolerance = t;
				}
				else if (cur) {
					cur->tolerance = t;
				}
			}
			else {
				// min_time, global_tolerance_time and the like: one number, not used
				const char c = word[0];
				if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
					size_t peek = pos;
					if (NextWord(text, length, peek, arg, sizeof(arg))) {
						const char d = arg[0];
						if ((d >= '0' && d <= '9') || d == '-' || d == '+' || d == '.') {
							pos = peek;
						}
					}
				}
			}
		}

		// A pattern needs a start and an end
		int kept = 0;
		for (int i = 0; i < m_count; i++) {
			if (m_patterns[i].count >= 2) {
				if (kept != i) {
					m_patterns[kept] = m_patterns[i];
				}
				kept++;
			}
		}
		m_count = kept;
		return m_count;
	}

	PatternMatcher::PatternMatcher() {
		Reset();
	}

	void PatternMatcher::Reset() {
		std::memset(m_state, 0, sizeof(m_state));
		m_pending = false;
		m_pendingRating = 0.0f;
		m_pendingPoints = 0;
		m_grace = 0;
	}

	static inline float Dist2(float ax, float ay, float bx, float by) {
		const float dx = ax - bx;
		const float dy = ay - by;
		return dx * dx + dy * dy;
	}

	bool PatternMatcher::Tick(const PatternSet& set, float x, float yDown, int missTicks, bool nollie, PatternMatch& out) {
		// A completed pattern is rated by its length first: with n points the rating grows with n^4,
		// so a varial that also passes a kickflip's two points wins. Then accuracy and speed count.
		static const float kErrorFloor = 0.15f;
		// Ticks per point at which the speed reads 1 and 0
		static const float kFastTicks = 1.75f;
		static const float kSlowTicks = 4.4f;
		// How long a completed pattern waits for a longer one that is one point short
		static const int kGraceTicks = 2;

		int best = -1;
		float bestRating = 0.0f;
		PatternMatch bestMatch;
		bestMatch.index = 0;
		bestMatch.speed = 0.0f;
		bestMatch.ticks = 0;
		bestMatch.error = 0.0f;
		const int count = set.Count();

		for (int i = 0; i < count; i++) {
			const Pattern& p = set.At(i);
			State& st = m_state[i];
			if (!p.known || (p.nollie && !nollie)) {
				st.active = 0;
				continue;
			}
			const float tol2 = p.tolerance * p.tolerance;

			if (!st.active) {
				// While a result is waiting, nothing new starts: the gesture is over
				const float d2 = Dist2(x, yDown, p.x[0], p.y[0]);
				if (!m_pending && d2 <= tol2) {
					st.active = 1;
					st.next = 1;
					st.misses = 0;
					st.ticks = 1;
					st.error = d2;
				}
				continue;
			}

			const float d2 = Dist2(x, yDown, p.x[st.next], p.y[st.next]);
			if (d2 <= tol2) {
				st.error += d2;
				st.ticks++;
				st.misses = 0;
				if (st.next == p.count - 1) {
					const float n = (float)p.count;
					float err = st.error > kErrorFloor ? st.error : kErrorFloor;
					err /= n;
					if (err > kErrorFloor) {
						err = kErrorFloor;
					}
					const float rating = (n * n * n * n) / (err * (float)st.ticks);
					// Equal ratings (same length, same speed, both accurate enough): the closer one
					if (best < 0 || rating > bestRating || (rating == bestRating && st.error < bestMatch.error)) {
						best = i;
						bestRating = rating;
						bestMatch.index = i;
						bestMatch.ticks = st.ticks;
						bestMatch.error = st.error;
					}
					st.active = 0;
				}
				else {
					st.next++;
				}
			}
			else if (st.next == 1 && Dist2(x, yDown, p.x[0], p.y[0]) <= tol2) {
				// Resting on the start point: the clock has not started yet
			}
			else {
				st.ticks++;
				st.misses++;
				if ((int)st.misses > missTicks) {
					st.active = 0;
				}
			}
		}

		if (best >= 0 && (!m_pending || bestRating > m_pendingRating)) {
			if (!m_pending) {
				m_grace = kGraceTicks;
			}
			m_pending = true;
			m_pendingMatch = bestMatch;
			m_pendingRating = bestRating;
			m_pendingPoints = set.At(best).count;
		}
		if (!m_pending) {
			return false;
		}

		if (m_grace > 0) {
			for (int i = 0; i < count; i++) {
				const Pattern& p = set.At(i);
				const State& st = m_state[i];
				if (st.active && p.count > m_pendingPoints && st.next == p.count - 1) {
					m_grace--;
					return false;
				}
			}
		}

		out = m_pendingMatch;
		const float perPoint = (float)out.ticks / (float)m_pendingPoints;
		if (perPoint <= kFastTicks) {
			out.speed = 1.0f;
		}
		else if (perPoint >= kSlowTicks) {
			out.speed = 0.0f;
		}
		else {
			out.speed = (kSlowTicks - perPoint) / (kSlowTicks - kFastTicks);
		}

		// One gesture, one trick: every pattern starts over
		Reset();
		return true;
	}

	// ------------------------------------------------------------------------------------------
	// Board tilt
	// ------------------------------------------------------------------------------------------

	static int TiltClassOf(int dir) {
		switch (dir) {
		case DIR_U:
		case DIR_UL:
		case DIR_UR:
			return TILT_UP;
		case DIR_D:
		case DIR_DL:
		case DIR_DR:
			return TILT_DOWN;
		case DIR_L:
		case DIR_R:
			return TILT_SIDE;
		default:
			return TILT_NONE;
		}
	}

	int GrindFromTilt(const Tilt& t, bool railOnLeft) {
		if (!t.valid || t.cur == DIR_NONE || t.first == TILT_NONE) {
			return GRIND_FIFTY;
		}
		const bool left = (t.cur == DIR_L || t.cur == DIR_UL || t.cur == DIR_DL);
		const bool up = (t.cur == DIR_U);
		const bool down = (t.cur == DIR_D);
		const bool upDiag = (t.cur == DIR_UL || t.cur == DIR_UR);
		const bool downDiag = (t.cur == DIR_DL || t.cur == DIR_DR);
		const bool side = (t.cur == DIR_L || t.cur == DIR_R);
		// Only read where the stick is off the vertical
		const bool toward = (left == railOnLeft);
		const bool firstToward = (t.firstLeft == railOnLeft);

		switch (t.first) {
		case TILT_UP:
			if (up) {
				return GRIND_NOSEGRIND;
			}
			if (upDiag) {
				return toward ? GRIND_CROOKED : GRIND_OVERCROOK;
			}
			if (side || downDiag) {
				return toward ? GRIND_NOSESLIDE : GRIND_NOSEBLUNT;
			}
			return GRIND_FIVE_O;      // rolled all the way round
		case TILT_DOWN:
			if (down) {
				return GRIND_FIVE_O;
			}
			if (downDiag) {
				return toward ? GRIND_FIVE_O : GRIND_SALAD;
			}
			if (side || upDiag) {
				return toward ? GRIND_TAILSLIDE : GRIND_BLUNTSLIDE;
			}
			return GRIND_NOSEGRIND;
		default:
			if (side) {
				return GRIND_BOARDLIP;
			}
			if (up || upDiag) {
				return firstToward ? GRIND_FEEBLE : GRIND_SMITH;
			}
			// Skate has the willy and the overwilly here, which THUG does not know
			return firstToward ? GRIND_CROOKED : GRIND_OVERCROOK;
		}
	}

	const char* GrindName(int grind) {
		switch (grind) {
		case GRIND_FIFTY: return "50-50";
		case GRIND_FIVE_O: return "5-0";
		case GRIND_NOSEGRIND: return "nosegrind";
		case GRIND_CROOKED: return "crooked";
		case GRIND_OVERCROOK: return "overcrook";
		case GRIND_FEEBLE: return "feeble";
		case GRIND_SMITH: return "smith";
		case GRIND_SALAD: return "salad";
		case GRIND_BOARDLIP: return "boardslide / lipslide";
		case GRIND_TAILSLIDE: return "tailslide";
		case GRIND_NOSESLIDE: return "noseslide";
		case GRIND_BLUNTSLIDE: return "bluntslide";
		case GRIND_NOSEBLUNT: return "nosebluntslide";
		case GRIND_OTHER: return "other";
		default: return "unknown";
		}
	}

	const char* StateName(int state) {
		switch (state) {
		case STATE_GROUND: return "ground";
		case STATE_AIR: return "air";
		case STATE_WALL: return "wall";
		case STATE_LIP: return "lip";
		case STATE_RAIL: return "rail";
		case STATE_OTHER: return "other";
		default: return "unknown";
		}
	}

	void PlanGrind(int grind, bool railOnLeft, GrindPlan& plan) {
		plan.row = 0;
		plan.parallel = -1;
		plan.prefix = 0;
		plan.fallback = GRIND_FIFTY;

		switch (grind) {
		case GRIND_NOSEGRIND:
			plan.row = 1;             // Up
			break;
		case GRIND_FIVE_O:
			plan.row = 2;             // Down
			break;
		// Left and Right are noseslide and tailslide, but only for a board that lands along the rail
		case GRIND_TAILSLIDE:
			plan.row = railOnLeft ? 4 : 3;
			plan.parallel = 1;
			break;
		case GRIND_NOSESLIDE:
			plan.row = railOnLeft ? 3 : 4;
			plan.parallel = 1;
			break;
		// Up + Left / Up + Right: crooked toward the rail, overcrook away from it
		case GRIND_CROOKED:
			plan.row = railOnLeft ? 5 : 6;
			break;
		case GRIND_OVERCROOK:
			plan.row = railOnLeft ? 6 : 5;
			break;
		// Down + Left / Down + Right: feeble toward the rail, smith away from it
		case GRIND_FEEBLE:
			plan.row = railOnLeft ? 7 : 8;
			break;
		case GRIND_SMITH:
			plan.row = railOnLeft ? 8 : 7;
			break;
		// No direction, and a board that lands across the rail: boardslide or lipslide
		case GRIND_BOARDLIP:
			plan.row = 0;
			plan.parallel = 0;
			break;
		case GRIND_BLUNTSLIDE:
			plan.row = -1;
			plan.prefix = "Trick_Bluntslide";
			plan.fallback = GRIND_TAILSLIDE;
			break;
		case GRIND_NOSEBLUNT:
			plan.row = -1;
			plan.prefix = "Trick_Nosebluntslide";
			plan.fallback = GRIND_NOSESLIDE;
			break;
		case GRIND_SALAD:
			plan.row = -1;
			plan.prefix = "Trick_Salad";
			plan.fallback = GRIND_FIVE_O;
			break;
		default:
			// GRIND_FIFTY: no direction, and the landing as the game found it
			break;
		}
	}

	const char* GrindSuffix(bool railOnLeft, bool backwards, bool flipped) {
		const bool frontside = (railOnLeft != flipped);
		if (backwards) {
			return frontside ? "_FS_180" : "_BS_180";
		}
		return frontside ? "_FS" : "_BS";
	}

	uint32_t NameChecksum(const char* name) {
		uint32_t crc = 0xFFFFFFFFu;
		for (const unsigned char* p = (const unsigned char*)name; *p; p++) {
			unsigned char c = *p;
			if (c >= 'A' && c <= 'Z') {
				c = (unsigned char)(c - 'A' + 'a');
			}
			crc ^= c;
			for (int bit = 0; bit < 8; bit++) {
				crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
			}
		}
		return crc;
	}

	/******************************************************************/
	/*                                                                */
	/*  Losing speed on a rail                                        */
	/******************************************************************/

	const float kUnitsPerMetre = 80.0f;
	const float kStopSpeed = 0.15f;
	const float kStopAfter = 1.0f;
	const float kCreepSpeed = 0.05f;

	// Skate's drag is a force on its board: 0.48 * (the grind) * (the surface) * (the time on the
	// rail). The board it pushes is deck, two trucks and four wheels: 11.56 kg.
	static const float kDragBase = 0.48f;
	static const float kBoardMass = 11.56f;
	// by DragClass: on a rail, on a ledge
	static const float kDragOfGrind[4][2] = {
		{ 50.0f, 40.0f },   // on the trucks
		{ 70.0f, 80.0f },   // truck and deck
		{ 60.0f, 60.0f },   // nose and tail slides
		{ 55.0f, 55.0f },   // the board across the rail
	};
	// by RailSurface
	static const float kDragOfSurface[3] = { 0.6f, 1.3f, 1.7f };
	// A board that was out of speed counts as moving again above this many times kStopSpeed
	static const float kMovingAgain = 4.0f;

	int SurfaceOfTerrain(int terrain) {
		switch (terrain) {
		// Metal of every kind, wires, cables and chains
		case 3: case 4: case 5: case 6: case 7:         // METALSMOOTH, METALROUGH, METALCORRUGATED, METALGRATING, METALTIN
		case 32: case 33: case 38:                      // CHAINLINK, METALFUTURE, METALFENCE
		case 39: case 41: case 43: case 44: case 45:    // GRINDTRAIN, GRINDWIRE, GRINDROUNDMETALPOLE, GRINDCHAINLINK, GRINDMETAL
		case 50: case 51: case 52: case 54:             // GRINDELECTRICWIRE, GRINDCABLE, GRINDCHAIN, GRINDNEONLIGHT
		case 57: case 58: case 59:                      // GRINDBRASSRAIL, GRINDCATWALK, GRINDTANKTURRET
		// and what is as slick: ice, plastic, glass
		case 25: case 28: case 29:                      // ICE, PLEXIGLASS, FIBERGLASS
		case 49: case 53: case 55:                      // GRINDPLASTIC, GRINDPLASTICBARRIER, GRINDGLASSMONSTER
			return SURFACE_RAIL;
		case 2: case 14: case 16: case 17: case 18:     // CONCROUGH, BRICK, ASPHALT, ROCK, GRAVEL
		case 20: case 21: case 22: case 23: case 27:    // GRASS, GRASSDRIED, DIRT, DIRTPACKED, SAND
		case 40: case 47: case 56:                      // GRINDROPE, GRINDWOODLOG, GRINDBANYONTREE
			return SURFACE_ROUGH;
		default:
			// concrete, wood, tiles, and every number this list does not know
			return SURFACE_LEDGE;
		}
	}

	const char* SurfaceName(int surface) {
		switch (surface) {
		case SURFACE_RAIL: return "rail";
		case SURFACE_LEDGE: return "ledge";
		case SURFACE_ROUGH: return "rough ledge";
		default: return "?";
		}
	}

	int DragClassOf(int grind, bool slide) {
		if (slide) {
			// A board that lands across a rail with no lean asked for is in a board- or lipslide
			return (grind == GRIND_BOARDLIP || grind == GRIND_FIFTY) ? DRAG_BOARDSLIDE : DRAG_TIPSLIDE;
		}
		switch (grind) {
		case GRIND_UNKNOWN:
		case GRIND_FIFTY:
		case GRIND_FIVE_O:
		case GRIND_NOSEGRIND:
			return DRAG_TRUCKS;
		default:
			return DRAG_ANGLED;
		}
	}

	const char* DragClassName(int dragClass) {
		switch (dragClass) {
		case DRAG_TRUCKS: return "on the trucks";
		case DRAG_ANGLED: return "truck and deck";
		case DRAG_TIPSLIDE: return "nose or tail slide";
		case DRAG_BOARDSLIDE: return "boardslide";
		default: return "?";
		}
	}

	float DragRamp(float seconds) {
		static const float kAt[4] = { 0.0f, 1.0f, 2.2f, 4.0f };
		static const float kShare[4] = { 0.63f, 0.72f, 0.83f, 1.0f };
		if (!(seconds > kAt[0])) {
			return kShare[0];
		}
		for (int i = 1; i < 4; i++) {
			if (seconds < kAt[i]) {
				return kShare[i - 1] + (kShare[i] - kShare[i - 1]) * (seconds - kAt[i - 1]) / (kAt[i] - kAt[i - 1]);
			}
		}
		return kShare[3];
	}

	float SkateDrag(int dragClass, int surface, float seconds) {
		if (dragClass < DRAG_TRUCKS || dragClass > DRAG_BOARDSLIDE) {
			dragClass = DRAG_TRUCKS;
		}
		if (surface < SURFACE_RAIL || surface > SURFACE_ROUGH) {
			surface = SURFACE_LEDGE;
		}
		const float force = kDragBase * kDragOfGrind[dragClass][surface == SURFACE_RAIL ? 0 : 1] * kDragOfSurface[surface] * DragRamp(seconds);
		return force / kBoardMass;
	}

	RailRun::RailRun() {
		Begin();
	}

	void RailRun::Begin() {
		m_seconds = 0.0f;
		m_drag = 0.0f;
		m_speed = 0.0f;
		m_wantsOff = false;
	}

	float RailRun::Step(float dt, float speed, float level, int dragClass, int surface, bool turned, int dragPercent, bool mayStepOff) {
		// Whatever the game hands over, nothing here may turn into a wild number
		if (!(dt > 0.0f)) {
			dt = 0.0f;
		}
		else if (dt > 0.1f) {
			dt = 0.1f;
		}
		if (!(speed > 0.0f)) {
			speed = 0.0f;
		}
		if (!(level > 0.0f)) {
			level = 0.0f;
		}
		else if (level > 1.0f) {
			level = 1.0f;
		}

		const float stop = kStopSpeed * kUnitsPerMetre;
		const float creep = kCreepSpeed * kUnitsPerMetre;

		m_drag = 0.0f;
		if (dragPercent > 0) {
			m_drag = SkateDrag(dragClass, surface, m_seconds) * level * kUnitsPerMetre * (float)dragPercent * 0.01f;
			// The drag slows the board down. It never stops it dead and never turns it around: the
			// game's rail code reads the direction of travel off the speed.
			if (speed > creep) {
				speed -= m_drag * dt;
				if (speed < creep) {
					speed = creep;
				}
			}
		}
		m_seconds += dt;
		m_speed = speed;

		if (!mayStepOff) {
			m_wantsOff = false;
		}
		else if (m_seconds > kStopAfter && (speed < stop || turned)) {
			m_wantsOff = true;
		}
		else if (m_wantsOff && speed > stop * kMovingAgain) {
			// It did not get off, and a slope has it moving again
			m_wantsOff = false;
		}
		return speed;
	}

	// ------------------------------------------------------------------------------------------
	// How far a rail reaches for the board
	// ------------------------------------------------------------------------------------------

	// Skate's helper, in metres: the most the middle of the board may be about to miss a rail by
	// and still be helped, whichever way the board is turned; and the most the board is moved
	static const float kHelpMiddle = 0.243f;
	static const float kHelpInAll = 0.2f;

	const int kReachMostPercent = 400;
	const float kReachLevel = kHelpMiddle * kUnitsPerMetre;                        // 19.4
	const float kReachDown = kHelpInAll * kUnitsPerMetre;                          // 16
	const float kReachUp = 0.5f * kHelpInAll * kUnitsPerMetre;                     // 8
	const float kReachEnd = kHelpInAll * kUnitsPerMetre;                           // 16
	const float kReachBehind = 8.0f;
	const float kNoRail = 1.0e9f;

	// Moving away from a rail: more than this share of the speed goes straight away from it
	static const float kAwayShare = 0.25f;
	// A rail that stands on end: less than this share of it is level
	static const float kUprightShare = 0.05f;
	// What a rail the board is leaving counts as: out of reach
	static const float kAwayMiss = 1.5f;
	// Less than this in every direction, in one frame, is a board that has not moved
	static const float kNoMove = 1.0e-5f;
	// A piece of rail shorter than this is none
	static const float kNoPiece = 0.01f;

	static inline float Dot3(const float* a, const float* b) {
		return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	}

	static inline float Clamp01(float v) {
		return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
	}

	static inline bool Finite3(const float* v) {
		// A number that is not one fails every comparison with itself
		return v[0] - v[0] == 0.0f && v[1] - v[1] == 0.0f && v[2] - v[2] == 0.0f;
	}

	// The two points, one on each of two straight pieces, that are closest to each other: s along
	// p1..q1, t along p2..q2, both 0..1. Where the pieces run side by side, the end of the first
	// one is used (the board's place now, not a frame ago).
	static void ClosestOnPieces(const float* p1, const float* q1, const float* p2, const float* q2, float& s, float& t) {
		const float d1[3] = { q1[0] - p1[0], q1[1] - p1[1], q1[2] - p1[2] };
		const float d2[3] = { q2[0] - p2[0], q2[1] - p2[1], q2[2] - p2[2] };
		const float r[3] = { p1[0] - p2[0], p1[1] - p2[1], p1[2] - p2[2] };
		const float a = Dot3(d1, d1);
		const float e = Dot3(d2, d2);
		const float f = Dot3(d2, r);
		const float tiny = 1.0e-6f;

		if (a <= tiny && e <= tiny) {
			s = 1.0f;
			t = 0.0f;
			return;
		}
		if (a <= tiny) {
			s = 1.0f;
			t = Clamp01(f / e);
			return;
		}
		const float c = Dot3(d1, r);
		if (e <= tiny) {
			t = 0.0f;
			s = Clamp01(-c / a);
			return;
		}
		const float b = Dot3(d1, d2);
		const float denominator = a * e - b * b;
		if (denominator > 1.0e-5f * a * e) {
			s = Clamp01((b * f - c * e) / denominator);
		}
		else {
			s = 1.0f;
		}
		t = (b * s + f) / e;
		if (t < 0.0f) {
			t = 0.0f;
			s = Clamp01(-c / a);
		}
		else if (t > 1.0f) {
			t = 1.0f;
			s = Clamp01((b - c) / a);
		}
	}

	RailPass PassRail(const float* a, const float* b, const float* from, const float* to, int percent) {
		RailPass out;
		out.miss = kNoRail;
		out.side = 0.0f;
		out.lift = 0.0f;
		out.past = 0.0f;
		out.away = false;
		out.point[0] = from ? from[0] : 0.0f;
		out.point[1] = from ? from[1] : 0.0f;
		out.point[2] = from ? from[2] : 0.0f;
		if (!a || !b || !from || percent <= 0 || !Finite3(a) || !Finite3(b) || !Finite3(from) || (to && !Finite3(to))) {
			return out;
		}

		const float scale = (float)percent * 0.01f;
		const float move[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
		float rail[3] = { 0.0f, 0.0f, 0.0f };
		float railLength = 0.0f;
		if (to) {
			rail[0] = to[0] - from[0];
			rail[1] = to[1] - from[1];
			rail[2] = to[2] - from[2];
			railLength = std::sqrt(Dot3(rail, rail));
		}

		if (railLength < 0.01f) {
			// A rail that is one point: how near the board's path comes to it
			const float moved = Dot3(move, move);
			float s = 1.0f;
			if (moved > 1.0e-6f) {
				const float toPoint[3] = { from[0] - a[0], from[1] - a[1], from[2] - a[2] };
				s = Clamp01(Dot3(toPoint, move) / moved);
			}
			const float dx = from[0] - (a[0] + s * move[0]);
			const float dy = from[1] - (a[1] + s * move[1]);
			const float dz = from[2] - (a[2] + s * move[2]);
			out.side = std::sqrt(dx * dx + dz * dz);
			out.lift = dy;
		}
		else {
			float s, t;
			ClosestOnPieces(a, b, from, to, s, t);
			out.point[0] = from[0] + t * rail[0];
			out.point[1] = from[1] + t * rail[1];
			out.point[2] = from[2] + t * rail[2];
			// From the board to the rail
			const float gap[3] = {
				out.point[0] - (a[0] + s * move[0]),
				out.point[1] - (a[1] + s * move[1]),
				out.point[2] - (a[2] + s * move[2])
			};
			const float along[3] = { rail[0] / railLength, rail[1] / railLength, rail[2] / railLength };
			const float railLevel = std::sqrt(rail[0] * rail[0] + rail[2] * rail[2]);
			// Beyond an end of the rail the gap has a part along it
			out.past = Dot3(gap, along);

			if (railLevel < kUprightShare * railLength) {
				// A rail that stands on end has no side and no top: the board is so far from
				// its line, and above or below its ends or not
				const float off[3] = { gap[0] - out.past * along[0], gap[1] - out.past * along[1], gap[2] - out.past * along[2] };
				out.side = std::sqrt(Dot3(off, off));
			}
			else {
				const float levelX = rail[0] / railLevel;
				const float levelZ = rail[2] / railLevel;
				// Level and square to the rail, and square to the rail in the upright plane through it
				const float side[3] = { levelZ, 0.0f, -levelX };
				const float up[3] = {
					along[1] * side[2],
					along[2] * side[0] - along[0] * side[2],
					-along[1] * side[0]
				};
				out.side = Dot3(gap, side);
				out.lift = Dot3(gap, up);

				// A rail the board has left behind is not come back to: neither one it has crossed
				// nor one whose end it has flown off
				const float moveLevel = std::sqrt(move[0] * move[0] + move[2] * move[2]);
				const float toward = (move[0] * side[0] + move[2] * side[2]) * (out.side >= 0.0f ? 1.0f : -1.0f);
				if (-toward > kAwayShare * moveLevel && std::fabs(out.side) > scale * kReachBehind) {
					out.away = true;
				}
				const float towardEnd = Dot3(move, along) * (out.past >= 0.0f ? 1.0f : -1.0f);
				if (-towardEnd > kAwayShare * std::sqrt(Dot3(move, move)) && std::fabs(out.past) > scale * kReachBehind) {
					out.away = true;
				}
			}
		}

		// One rounded shape around the rail: each distance against its own reach, and the three
		// together as the distance from the middle of that shape
		const float beside = out.side / (scale * kReachLevel);
		const float height = out.lift > 0.0f ? out.lift / (scale * kReachUp) : out.lift / (scale * kReachDown);
		const float end = out.past / (scale * kReachEnd);
		float miss = std::sqrt(beside * beside + height * height + end * end);
		if (out.away && miss < kAwayMiss) {
			miss = kAwayMiss;
		}
		// Not a number: no rail
		if (!(miss >= 0.0f)) {
			miss = kNoRail;
			out.away = false;
		}
		out.miss = miss;
		return out;
	}

	static inline const unsigned char* PointerAt(const unsigned char* p) {
		const unsigned char* value;
		memcpy(&value, p, sizeof(value));
		return value;
	}

	static inline unsigned int FlagsAt(const unsigned char* p) {
		uint32_t value;
		memcpy(&value, p, sizeof(value));
		return value;
	}

	const unsigned char* FindRail(const RailNodes& nodes, const float* a, const float* b,
		const unsigned char* ignore, float minCos, int percent, RailTest test, RailPass& pass) {
		pass.miss = kNoRail;
		pass.side = pass.lift = pass.past = 0.0f;
		pass.away = false;
		pass.point[0] = pass.point[1] = pass.point[2] = 0.0f;
		if (!nodes.first || nodes.count <= 0 || nodes.stride <= 0 || !a || !b || percent <= 0 || !Finite3(a) || !Finite3(b)) {
			return 0;
		}
		// A board that has not moved in this frame takes no rail. The game's own search finds
		// none then either (it has no line to measure from).
		if (std::fabs(b[0] - a[0]) < kNoMove && std::fabs(b[1] - a[1]) < kNoMove && std::fabs(b[2] - a[2]) < kNoMove) {
			return 0;
		}

		// The box the board moved through, grown by the longest reach there is. Nothing outside it
		// can be within reach, and a little more is looked at so that the rail that came closest
		// can be told.
		const float scale = (float)percent * 0.01f;
		const float grow = 2.5f * scale * (kReachLevel > kReachDown ? kReachLevel : kReachDown);
		float low[3], high[3];
		for (int i = 0; i < 3; i++) {
			low[i] = (a[i] < b[i] ? a[i] : b[i]) - grow;
			high[i] = (a[i] > b[i] ? a[i] : b[i]) + grow;
		}

		// The game's rule for parks from the editor: only rails that run about the way the board goes
		const bool angleRule = minCos < 1.0f;
		const float moveX = b[0] - a[0];
		const float moveZ = b[2] - a[2];
		const float moveLevel = std::sqrt(moveX * moveX + moveZ * moveZ);

		const unsigned char* best = 0;       // within reach
		const unsigned char* nearest = 0;    // or not
		RailPass nearestPass = pass;

		const unsigned char* node = nodes.first;
		for (int i = 0; i < nodes.count; i++, node += nodes.stride) {
			const unsigned int flags = FlagsAt(node + nodes.flags);
			if ((flags & nodes.skips) != 0 || (flags & nodes.needs) != nodes.needs || node == ignore) {
				continue;
			}
			const unsigned char* next = PointerAt(node + nodes.next);
			if (!next && PointerAt(node + nodes.previous)) {
				continue;    // the last node of a rail: its piece belongs to the node before it
			}
			const float* boxMin = (const float*)(node + nodes.boxMin);
			const float* boxMax = (const float*)(node + nodes.boxMax);
			if (boxMin[0] > high[0] || boxMax[0] < low[0] || boxMin[1] > high[1] || boxMax[1] < low[1] || boxMin[2] > high[2] || boxMax[2] < low[2]) {
				continue;
			}

			const float* from = (const float*)(node + nodes.position);
			const float* to = next ? (const float*)(next + nodes.position) : 0;
			if (to) {
				// A piece without length is no rail to stand on, and the game's own search
				// passes it over. The pieces next to it end where it is.
				const float piece[3] = { to[0] - from[0], to[1] - from[1], to[2] - from[2] };
				if (!(Dot3(piece, piece) >= kNoPiece * kNoPiece)) {
					continue;
				}
			}
			const RailPass one = PassRail(a, b, from, to, percent);
			if (!(one.miss < nearestPass.miss)) {
				continue;
			}
			if (angleRule && to && moveLevel > 1.0e-3f) {
				const float railX = to[0] - from[0];
				const float railZ = to[2] - from[2];
				const float railLevel = std::sqrt(railX * railX + railZ * railZ);
				// (a rail that stands on end runs no way at all, and is left out as well)
				if (railLevel <= 1.0e-3f || std::fabs(moveX * railX + moveZ * railZ) < minCos * moveLevel * railLevel) {
					continue;
				}
			}
			if (one.miss <= 1.0f && test && to && !test(from, to)) {
				continue;
			}
			nearest = node;
			nearestPass = one;
			if (one.miss <= 1.0f) {
				best = node;
			}
		}

		pass = nearestPass;
		if (!nearest) {
			pass.miss = kNoRail;
			pass.away = false;
		}
		return best;
	}

	// ------------------------------------------------------------------------------------------
	// Jumps
	// ------------------------------------------------------------------------------------------

	// THUG (physics.q): Physics_Air_Gravity, Physics_Jump_Speed_min_Stat, Physics_Jump_Speed_Stat
	const float kThugAirGravity = 1350.0f;
	const float kThugPopTap = 350.0f;
	const float kThugPopWorst = 414.0f;
	const float kThugPopBest = 450.0f;
	const int kJumpMostPercent = 400;

	// Skate: gravity in m/s2, and how far a full pop lifts the rider, in metres
	static const float kSkateGravity = 9.8f;
	static const float kSkateFullPop = 0.76f;

	float AirGravityFactor(int percent) {
		if (percent <= 0) {
			return 1.0f;
		}
		return (float)percent * 0.01f * kSkateGravity * kUnitsPerMetre / kThugAirGravity;
	}

	float JumpPopFactor(int percent) {
		if (percent <= 0) {
			return 1.0f;
		}
		// The speed that lifts Skate's rider by a full pop, for the best pop THUG has
		const float speed = std::sqrt(2.0f * kSkateGravity * kSkateFullPop) * kUnitsPerMetre;
		return (float)percent * 0.01f * speed / kThugPopBest;
	}

	// Skate: how high the rider rises for a quick pop, and for a full one
	static const float kSkateQuickPop = 0.38f;

	float PopDepthShare(float strength) {
		if (!(strength > 0.0f)) {
			strength = 0.0f;
		}
		if (strength > 1.0f) {
			strength = 1.0f;
		}
		// The speed of a jump goes with the root of the height
		return std::sqrt((kSkateQuickPop + (kSkateFullPop - kSkateQuickPop) * strength) / kSkateFullPop);
	}

	FlatJump JumpOnFlat(float thugPop, int gravityPercent, int popPercent) {
		FlatJump out;
		out.height = 0.0f;
		out.seconds = 0.0f;
		if (!(thugPop > 0.0f)) {
			return out;
		}
		const float speed = thugPop * JumpPopFactor(popPercent);
		const float gravity = kThugAirGravity * AirGravityFactor(gravityPercent);
		out.height = speed * speed / (2.0f * gravity);
		out.seconds = 2.0f * speed / gravity;
		return out;
	}

	// ------------------------------------------------------------------------------------------
	// Rolling: pushing, coasting, steering
	// ------------------------------------------------------------------------------------------

	const float kPushTopSpeed = 8.5f;
	const float kPushLastSpeed = 8.3f;
	const float kPushTap = 0.75f;
	const float kPushMost = 4.0f;
	const float kPushFullHold = 0.25f;
	// THUG (physics.q): Physics_Ground_Rotation
	const float kThugGroundTurn = 1.8f;
	const float kTurnGrip = 0.8f;
	const int kRollMostPercent = 400;
	const int kSpinMostPercent = 400;

	// Skate's numbers come as curves of eight points with straight lines between them. Before the
	// first point and after the last the curve stays where it is.
	struct Curve8 {
		float x[8];
		float y[8];
	};

	static float At(const Curve8& c, float x) {
		if (!(x > c.x[0])) {
			return c.y[0];
		}
		for (int i = 1; i < 8; i++) {
			if (x < c.x[i]) {
				const float span = c.x[i] - c.x[i - 1];
				if (!(span > 0.0f)) {
					return c.y[i];
				}
				return c.y[i - 1] + (c.y[i] - c.y[i - 1]) * (x - c.x[i - 1]) / span;
			}
		}
		return c.y[7];
	}

	// The speed a push is asked to add (m/s), over the share of the full hold the button was down
	static const Curve8 kPushOfHold = {
		{ 0.0f, 0.2417f, 0.3426f, 0.4197f, 0.5153f, 0.7676f, 0.8845f, 1.0f },
		{ 0.75f, 1.0446f, 1.4304f, 2.0411f, 2.5232f, 2.9732f, 3.5036f, 4.5f }
	};
	// How long the button is down for the full push (s), over the speed (m/s)
	static const Curve8 kFullHoldOfSpeed = {
		{ 0.0f, 0.3239f, 1.0383f, 2.8785f, 4.6169f, 5.5538f, 6.6713f, 8.4774f },
		{ 0.2268f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f }
	};
	// The most a push adds in one tick (m/s): this at a standstill, and this at the top speed
	static const float kPushStepSlow = 0.6f;
	static const float kPushStepFast = 0.5f;
	static const float kSkateTick = 1.0f / 60.0f;
	// What the four wheels take off the speed (m/s2), over the speed (m/s)
	static const Curve8 kRollLoss = {
		{ 0.0f, 1.1451f, 2.2823f, 3.3313f, 4.1962f, 5.1572f, 6.5265f, 8.0f },
		{ 0.1286f, 0.1518f, 0.2071f, 0.2929f, 0.3857f, 0.4411f, 0.4804f, 0.5f }
	};
	// What the air takes on top of that (m/s2), over the speed (m/s): nothing up to the speed a push reaches
	static const Curve8 kAirLoss = {
		{ 0.0f, 8.1205f, 10.4213f, 12.0454f, 12.8575f, 14.3322f, 18.4039f, 26.9782f },
		{ 0.0f, 0.0f, 0.03f, 0.1629f, 0.6f, 0.9041f, 1.05f, 1.2f }
	};
	// How far the trucks turn: this many radians at most ...
	static const float kTruckMost = 0.6f;
	// ... times this, over how far the stick is pushed
	static const Curve8 kTruckOfStick = {
		{ 0.0f, 0.0586f, 0.25f, 0.375f, 0.5088f, 0.6904f, 0.8577f, 1.0f },
		{ 0.0f, 0.0f, 0.0338f, 0.0777f, 0.1723f, 0.3345f, 0.5642f, 0.8345f }
	};
	// ... times this, over the speed as a share of 50 m/s
	static const Curve8 kTruckOfSpeed = {
		{ 0.057f, 0.1238f, 0.1731f, 0.2191f, 0.2979f, 0.4f, 0.5f, 1.0f },
		{ 1.0f, 0.9696f, 0.848f, 0.6081f, 0.3953f, 0.3f, 0.28f, 0.27f }
	};
	static const float kTruckSpeedScale = 50.0f;
	// Each truck sits this far from the middle of the board (m)
	static const float kTruckOffset = 0.243f;
	// How fast the stick and the trucks follow, as the share of what is missing that is made up in one tick
	static const float kStickEase = 0.7f;
	static const float kTruckEase = 0.2f;
	// A pushing foot takes turning away: down to this share, by this much a tick, and back by that much
	static const float kPushTurnLeast = 0.5f;
	static const float kPushTurnDown = 0.015f;
	static const float kPushTurnUp = 0.05f;
	// Standing still the board turns on the spot the way THUG's does. That fades out up to this
	// speed (units/s), where the trucks have long taken over.
	static const float kPivotSpeed = 100.0f;
	// A frame longer than this is a hitch, not a frame
	static const float kLongFrame = 0.1f;
	// A tap gives up when the game has shown no push animation for it in this long (s)
	static const float kWishMost = 0.75f;
	// While a push asks for its animation the game's scripts are kept from seeing Left and Right
	// for this long (s) out of every ...
	static const float kHideFor = 0.2f;
	static const float kHidePeriod = 0.8f;
	// An animation that was within this many frames of its end when it was last seen ran out
	static const float kRanOutSteps = 1.5f;
	// A foot starts to push in the first 95 hundredths of its animation, and pushes for 2 at least
	static const float kPushDownMost = 0.95f;
	static const float kPushSpanLeast = 0.02f;
	// The feet in a push animation. At least this many looks at them, ...
	static const int kFootSamplesLeast = 10;
	// ... the first this early in the animation, the last this late, no two further apart than this
	static const float kFootFirstBy = 0.2f;
	static const float kFootLastFrom = 0.85f;
	static const float kFootGapMost = 0.2f;
	// The foot that pushes gets at least this far below where the other stands (the game's units; a board is 4 to 5 high)
	static const float kFootDropLeast = 1.5f;
	// Its stroke starts where it gets this far below where the other stands, on its way down
	static const float kFootBelowBoard = 1.5f;
	// It is on the ground while it is within this share of that drop of its lowest point
	static const float kFootGroundShare = 0.2f;
	// Where it goes back this far at least (units), the speed goes with that
	static const float kFootSweepLeast = 4.0f;
	// It has turned when it has come forward again by this much (units)
	static const float kFootTurnBack = 3.0f;
	// The way it travels on the ground gives its stroke a direction from this far on (units)
	static const float kFootGroundWayLeast = 2.0f;
	// Its stroke starts when it has left the first fiftieth of that way behind, and ends when the last is ahead
	static const float kFootEdge = 0.02f;
	// The stroke takes at least this share of the animation, and this long (s)
	static const float kFootSpanLeast = 0.04f;
	static const float kFootSecondsLeast = 0.05f;

	static inline float Clampf(float v, float lo, float hi) {
		return v < lo ? lo : (v > hi ? hi : v);
	}

	float PushStrength(float heldSeconds, float speed) {
		const float full = At(kFullHoldOfSpeed, speed);
		const float share = heldSeconds > 0.0f ? Clampf(heldSeconds / full, 0.0f, 1.0f) : 0.0f;
		const float asked = At(kPushOfHold, share);
		return asked > kPushMost ? kPushMost : asked;
	}

	float PushStep(float speed) {
		const float share = Clampf(speed / kPushTopSpeed, 0.0f, 1.0f);
		return kPushStepSlow + (kPushStepFast - kPushStepSlow) * share;
	}

	float CoastLoss(float speed) {
		if (speed < 0.0f) {
			speed = -speed;
		}
		return At(kRollLoss, speed) + At(kAirLoss, speed);
	}

	float Coast(float speed, float dt, int percent) {
		if (percent <= 0 || !(dt > 0.0f) || !(speed > 0.0f)) {
			return speed;
		}
		const float loss = CoastLoss(speed / kUnitsPerMetre) * kUnitsPerMetre * (float)percent * 0.01f * dt;
		return loss >= speed ? 0.0f : speed - loss;
	}

	void SmoothPushProfile(PushProfile& out, float down, float up) {
		if (!(down > 0.0f)) {
			down = 0.0f;
		}
		if (down > kPushDownMost) {
			down = kPushDownMost;
		}
		if (!(up > down + kPushSpanLeast)) {
			up = down + kPushSpanLeast;
		}
		if (up > 1.0f) {
			up = 1.0f;
		}
		out.down = down;
		out.up = up;
		for (int k = 0; k < PUSH_KNOTS; k++) {
			const float t = (float)k / (float)(PUSH_KNOTS - 1);
			out.done[k] = t * t * (3.0f - 2.0f * t);
		}
	}

	float PushDone(const PushProfile& profile, float share) {
		if (!(share > profile.down)) {
			return 0.0f;
		}
		if (!(share < profile.up)) {
			return 1.0f;
		}
		const float at = (share - profile.down) / (profile.up - profile.down) * (float)(PUSH_KNOTS - 1);
		int i = (int)at;
		if (i > PUSH_KNOTS - 2) {
			i = PUSH_KNOTS - 2;
		}
		return profile.done[i] + (profile.done[i + 1] - profile.done[i]) * (at - (float)i);
	}

	PushRun::PushRun() {
		m_held = false;
		m_hold = 0.0f;
		m_serial = 0;
		m_pushHold = 0.0f;
		m_asked = 0.0f;
		m_added = 0.0f;
		m_from = 0.0f;
		m_to = 0.0f;
		m_play = 0;
		m_seenPlay = 0;
		m_askPlay = 0;
		m_animCurrent = false;
		m_animRunning = false;
		SmoothPushProfile(m_profile, 0.0f, 1.0f);
		Reset();
	}

	void PushRun::Reset() {
		m_phase = PUSH_NONE;
		m_share = 0.0f;
		m_shareStep = 0.0f;
		m_askTime = 0.0f;
		m_hideTime = 0.0f;
		m_asking = false;
		m_tracking = false;
		m_target = 0.0f;
	}

	bool PushRun::Active() const {
		return m_phase == PUSH_ASKED || m_phase == PUSH_INTO || m_phase == PUSH_CONTACT;
	}

	bool PushRun::Animating() const {
		// While a push animation runs the game's scripts are busy with it
		return m_phase == PUSH_ASKED && !m_animRunning;
	}

	int PushGivesWay(bool stroking, bool crouched, bool down) {
		if (down) {
			return GIVE_DOWN;
		}
		if (crouched && !stroking) {
			return GIVE_CROUCH;
		}
		return GIVE_NONE;
	}

	bool PushRun::HideTurn(bool free, float dt) {
		if (!Animating() || !free) {
			return false;
		}
		const bool hide = std::fmod(m_hideTime, kHidePeriod) < kHideFor;
		if (dt > 0.0f) {
			m_hideTime += dt > kLongFrame ? kLongFrame : dt;
		}
		return hide;
	}

	// While the foot pushes the board is taken up to the speed the push is for, by as much of
	// its stroke as the foot has behind it (done, 0 to 1). Whatever else pulls at the board
	// meanwhile is made up for. But no faster than Skate lets a push come in.
	float PushRun::Pull(float speed, float dt, float units, float done) {
		const float want = m_from + (m_target - m_from) * done - speed;
		const float most = PushStep(speed / units) * (dt / kSkateTick) * units;
		const float add = want > most ? most : want;
		if (!(add > 0.0f)) {
			return 0.0f;
		}
		m_added += add;
		return add;
	}

	float PushRun::Step(float dt, float speed, bool held, int percent, const PushAnim& anim, const PushProfile& profile) {
		// The button. A press asks for a push; how long it stays down is what makes the push strong.
		const bool pressed = held && !m_held;
		const bool released = !held && m_held;
		m_held = held;
		if (pressed) {
			m_hold = 0.0f;
			m_asking = true;
		}
		if (released) {
			m_tracking = false;
		}
		const bool running = anim.current && anim.running && anim.length > 0.0f;
		m_animCurrent = anim.current;
		m_animRunning = running;
		const uint32_t seen = m_seenPlay;   // the play of an animation the frame before was in
		m_seenPlay = anim.play;
		if (percent <= 0 || !(dt > 0.0f)) {
			return 0.0f;
		}
		if (dt > kLongFrame) {
			dt = kLongFrame;
		}
		if (held) {
			m_hold += dt;
		}
		if (m_tracking) {
			m_pushHold = m_hold;
		}

		const float units = kUnitsPerMetre * (float)percent * 0.01f;   // the game's units for one of Skate's m/s
		const float top = kPushTopSpeed * units;
		const float last = kPushLastSpeed * units;
		// How far into its animation the skater is. One that has stopped at its end is at its end.
		const float share = running ? Clampf(anim.time / anim.length, 0.0f, 1.0f) : 1.0f;

		float add = 0.0f;
		if (m_phase >= PUSH_INTO && (!anim.current || anim.play != m_play)) {
			// The play of the animation this push went with is gone, and the push with it
			if (m_phase == PUSH_CONTACT) {
				if (m_share + kRanOutSteps * m_shareStep >= 1.0f) {
					// It ran to its end between two frames: the foot has finished its stroke
					add += Pull(speed, dt, units, 1.0f);
				}
				m_to = speed + add;
				m_serial++;
			}
			m_tracking = false;
			m_phase = PUSH_NONE;
		}

		if (m_phase == PUSH_NONE && (m_asking || held)) {
			m_asking = false;
			// From the speed pushing gets to on, no push starts. A press is not kept for later;
			// a button that stays down pushes again when the board has slowed.
			if (speed + add < last) {
				m_phase = PUSH_ASKED;
				m_askTime = 0.0f;
				m_hideTime = 0.0f;
				m_askPlay = seen;
				// The press this push comes from: the one that is down, or a tap that is over
				m_tracking = held;
				m_pushHold = m_hold;
			}
		}

		if (m_phase == PUSH_ASKED) {
			if (running && (share < profile.down || anim.play != m_askPlay)) {
				// The game plays a push animation that it has started since the push was
				// wanted, or one whose foot is not down yet: that is this push
				if (speed + add < last) {
					m_phase = PUSH_INTO;
					m_play = anim.play;
					m_profile = profile;
					m_share = share;
					m_shareStep = 0.0f;
				}
				else {
					m_phase = PUSH_NONE;    // the board has got as fast as pushing gets meanwhile
				}
			}
			else if (!running) {
				// The game could show the push now. A tap does not wait for that for long.
				m_askTime += dt;
				if (!held && m_askTime > kWishMost) {
					m_phase = PUSH_NONE;
				}
			}
		}

		if (m_phase >= PUSH_INTO) {
			if (m_phase == PUSH_INTO && share >= m_profile.down) {
				// The foot starts to push
				m_from = speed + add;
				m_added = 0.0f;
				m_phase = PUSH_CONTACT;
			}
			if (m_phase == PUSH_CONTACT) {
				// How long the button has been down says how strong the push is. A button
				// that is still down counts for more as the foot pushes on, until it is let
				// go or the foot has pushed.
				m_asked = PushStrength(m_pushHold, m_from / units) * units;
				// never past the top speed
				m_target = m_from + m_asked;
				if (m_target > top) {
					m_target = top;
				}
				add += Pull(speed + add, dt, units, PushDone(m_profile, share));
				if (share >= m_profile.up) {
					m_tracking = false;
					m_phase = PUSH_RECOVER;
					m_to = speed + add;
					m_serial++;
				}
			}
			if (share > m_share) {
				m_shareStep = share - m_share;
			}
			m_share = share;
		}
		return add;
	}

	const int kPushClockAfter = 3;
	const float kPushClockLength = 0.8f;

	PushClock::PushClock() {
		m_play = 0x80000000u;
		m_serial = 0;
		Reset();
	}

	void PushClock::Reset() {
		m_told = 0;
		m_blind = false;
		m_running = false;
		m_time = 0.0f;
	}

	void PushClock::Told() {
		if (m_told < kPushClockAfter) {
			m_told++;
		}
		if (m_blind && !m_running) {
			// by the clock: this is the moment the push starts
			m_running = true;
			m_time = 0.0f;
			m_play++;
		}
	}

	bool PushClock::Step(bool seen, float dt, PushAnim& anim) {
		if (seen) {
			// he is in a push animation: what the game plays is seen
			Reset();
			return false;
		}
		if (!m_blind && m_told >= kPushClockAfter) {
			m_blind = true;
			m_serial++;
		}
		if (!m_blind || !m_running) {
			return false;
		}
		anim.current = true;
		anim.running = m_time < kPushClockLength;
		anim.time = anim.running ? m_time : kPushClockLength;
		anim.length = kPushClockLength;
		anim.play = m_play;
		if (!anim.running) {
			m_running = false;      // it stands at its end for this one frame
		}
		else if (dt > 0.0f) {
			m_time += dt > kLongFrame ? kLongFrame : dt;
		}
		return true;
	}

	FootWatch::FootWatch() {
		Begin();
	}

	void FootWatch::Begin() {
		for (int b = 0; b < BINS; b++) {
			m_have[b] = false;
		}
	}

	int FootWatch::Samples() const {
		int n = 0;
		for (int b = 0; b < BINS; b++) {
			if (m_have[b]) {
				n++;
			}
		}
		return n;
	}

	void FootWatch::Sample(float share, const float* left, const float* right) {
		if (!left || !right || !(share >= 0.0f) || !(share <= 1.0f) || !Finite3(left) || !Finite3(right)) {
			return;
		}
		// One look is kept for every 128th of the animation: the last one
		int bin = (int)(share * (float)BINS);
		if (bin > BINS - 1) {
			bin = BINS - 1;
		}
		m_have[bin] = true;
		m_share[bin] = share;
		for (int i = 0; i < 3; i++) {
			m_left[bin][i] = left[i];
			m_right[bin][i] = right[i];
		}
	}

	// How far a point is along a way on the ground (the first and the third numbers)
	static inline float Along(const float* p, float ux, float uz) {
		return p[0] * ux + p[2] * uz;
	}

	int FootWatch::Looks(float* out, int most) const {
		int n = 0;
		for (int b = 0; b < BINS && n < most; b++) {
			if (!m_have[b]) {
				continue;
			}
			float* look = out + 7 * n;
			look[0] = m_share[b];
			for (int i = 0; i < 3; i++) {
				look[1 + i] = m_left[b][i];
				look[4 + i] = m_right[b][i];
			}
			n++;
		}
		return n;
	}

	void FootWatch::End(float seconds, FootResult& out) const {
		out.why = FOOT_FEW;
		out.left = false;
		out.swept = false;
		out.board = 0.0f;
		out.drop = 0.0f;
		out.floor = 0.0f;
		out.sweep = 0.0f;
		out.lowFrom = 0.0f;
		out.lowTo = 0.0f;
		SmoothPushProfile(out.profile, 0.0f, 1.0f);
		int at[BINS];
		int n = 0;
		for (int b = 0; b < BINS; b++) {
			if (m_have[b]) {
				at[n++] = b;
			}
		}
		out.samples = n;
		if (n < kFootSamplesLeast) {
			return;
		}
		out.why = FOOT_PART;
		if (m_share[at[0]] > kFootFirstBy || m_share[at[n - 1]] < kFootLastFrom) {
			return;
		}
		for (int i = 1; i < n; i++) {
			if (m_share[at[i]] - m_share[at[i - 1]] > kFootGapMost) {
				return;
			}
		}

		// The foot that pushes: the one that gets lower. The other stands on the board, at the
		// middle of its heights.
		float lowLeft = m_left[at[0]][1];
		float lowRight = m_right[at[0]][1];
		for (int i = 1; i < n; i++) {
			if (m_left[at[i]][1] < lowLeft) {
				lowLeft = m_left[at[i]][1];
			}
			if (m_right[at[i]][1] < lowRight) {
				lowRight = m_right[at[i]][1];
			}
		}
		out.left = lowLeft <= lowRight;
		const float (*foot)[3] = out.left ? m_left : m_right;
		const float (*other)[3] = out.left ? m_right : m_left;
		float heights[BINS];
		for (int i = 0; i < n; i++) {
			// in order, as they come
			const float h = other[at[i]][1];
			int j = i;
			while (j > 0 && heights[j - 1] > h) {
				heights[j] = heights[j - 1];
				j--;
			}
			heights[j] = h;
		}
		const float board = (n & 1) ? heights[n / 2] : 0.5f * (heights[n / 2 - 1] + heights[n / 2]);
		const float low = out.left ? lowLeft : lowRight;
		out.board = board;
		out.floor = low;
		out.drop = board - low;
		out.why = FOOT_NO_DROP;
		if (!(out.drop >= kFootDropLeast)) {
			return;
		}

		// It is on the ground around its lowest point, for as long as it stays near that
		const float line = low + kFootGroundShare * out.drop;
		int lowest = 0;
		for (int i = 1; i < n; i++) {
			if (foot[at[i]][1] < foot[at[lowest]][1]) {
				lowest = i;
			}
		}
		int first = lowest;
		int last = lowest;
		while (first > 0 && foot[at[first - 1]][1] < line) {
			first--;
		}
		while (last < n - 1 && foot[at[last + 1]][1] < line) {
			last++;
		}
		out.lowFrom = m_share[at[first]];
		out.lowTo = m_share[at[last]];
		out.why = FOOT_SHORT;

		// Its stroke goes the way it travels while on the ground, if it travels there at all ...
		const float gx = foot[at[last]][0] - foot[at[first]][0];
		const float gz = foot[at[last]][2] - foot[at[first]][2];
		const float glen = std::sqrt(gx * gx + gz * gz);
		out.sweep = glen;
		if (glen >= kFootGroundWayLeast) {
			const float ux = gx / glen;
			const float uz = gz / glen;
			// ... from the farthest forward it is between coming down below the board and
			// leaving the ground ...
			int below = lowest;
			while (below > 0 && foot[at[below - 1]][1] < board - kFootBelowBoard) {
				below--;
			}
			int a = below;
			for (int i = below; i <= last; i++) {
				if (Along(foot[at[i]], ux, uz) < Along(foot[at[a]], ux, uz)) {
					a = i;
				}
			}
			// ... to the farthest back it gets after that, on the ground or as the leg follows
			// through, until it turns and comes forward again
			int b = a;
			for (int i = a; i < n; i++) {
				const float along = Along(foot[at[i]], ux, uz);
				const float best = Along(foot[at[b]], ux, uz);
				if (along > best) {
					b = i;
				}
				else if (best - along > kFootTurnBack) {
					break;
				}
			}
			const float way = Along(foot[at[b]], ux, uz) - Along(foot[at[a]], ux, uz);
			out.sweep = way;
			if (way >= kFootSweepLeast) {
				// How far along that way it is at every look: nothing at the one point, all of it at the other
				float along[BINS];
				for (int i = a; i <= b; i++) {
					along[i] = Along(foot[at[i]], ux, uz) - Along(foot[at[a]], ux, uz);
				}
				// Trimmed to where the foot is on its way: from the last look at which it has not
				// left yet to the first at which it has arrived
				const float edge = kFootEdge * way;
				int from = a;
				for (int i = a; i <= b; i++) {
					if (along[i] <= edge) {
						from = i;
					}
				}
				int to = b;
				for (int i = b; i > from; i--) {
					if (along[i] >= way - edge) {
						to = i;
					}
				}
				const float down = m_share[at[from]];
				const float up = m_share[at[to]];
				if (up - down >= kFootSpanLeast && (up - down) * seconds >= kFootSecondsLeast) {
					// How much of the way is behind the foot at every look, never falling ...
					out.profile.down = down;
					out.profile.up = up;
					const float span = along[to] - along[from];
					float done[BINS];
					float best = 0.0f;
					for (int i = from; i <= to; i++) {
						const float d = Clamp01((along[i] - along[from]) / span);
						if (d > best) {
							best = d;
						}
						done[i] = best;
					}
					// ... and at even steps between its start and its end
					int i = from;
					for (int k = 0; k < PUSH_KNOTS; k++) {
						const float share = down + (up - down) * (float)k / (float)(PUSH_KNOTS - 1);
						while (i < to - 1 && m_share[at[i + 1]] <= share) {
							i++;
						}
						const float t = Clamp01((share - m_share[at[i]]) / (m_share[at[i + 1]] - m_share[at[i]]));
						out.profile.done[k] = done[i] + (done[i + 1] - done[i]) * t;
					}
					out.profile.done[0] = 0.0f;
					out.profile.done[PUSH_KNOTS - 1] = 1.0f;
					out.swept = true;
					out.why = FOOT_OK;
					return;
				}
			}
		}

		// It stands where it was put (or its travel is over in no time): the speed comes in
		// over the time it is on the ground, gently at both ends
		const float down = out.lowFrom;
		const float up = out.lowTo;
		if (!(up - down >= kFootSpanLeast) || !((up - down) * seconds >= kFootSecondsLeast)) {
			return;
		}
		SmoothPushProfile(out.profile, down, up);
		out.why = FOOT_OK;
	}

	// The trucks' angle that the stick asks for at this speed, 0 or more. stick: 0 to 1.
	static float TruckAngle(float stick, float metresPerSecond) {
		return kTruckMost * At(kTruckOfStick, stick) * At(kTruckOfSpeed, Clampf(metresPerSecond / kTruckSpeedScale, 0.0f, 1.0f));
	}

	// How fast the board turns with its trucks at this angle (0 or more), in radians per second
	static float RateOfAngle(float angle, float speed, int percent) {
		const float v = speed / kUnitsPerMetre;
		// The board goes where its trucks point ...
		float rate = v * std::tan(angle) / kTruckOffset;
		// ... as far as the wheels grip
		if (v > 0.01f) {
			const float grip = kTurnGrip * 9.8f * (float)percent * 0.01f / v;
			if (rate > grip) {
				rate = grip;
			}
		}
		// At a standstill there is nothing for the trucks to steer: THUG's own turn on the spot
		if (speed < kPivotSpeed) {
			const float most = kTruckMost * kTruckOfStick.y[7];
			const float pivot = kThugGroundTurn * Clampf(angle / most, 0.0f, 1.0f) * (1.0f - (speed > 0.0f ? speed : 0.0f) / kPivotSpeed);
			if (pivot > rate) {
				rate = pivot;
			}
		}
		return rate;
	}

	float TurnRate(float stick, float speed, int percent) {
		if (percent <= 0) {
			return 0.0f;
		}
		if (speed < 0.0f) {
			speed = -speed;
		}
		stick = Clampf(stick < 0.0f ? -stick : stick, 0.0f, 1.0f);
		return RateOfAngle(TruckAngle(stick, speed / kUnitsPerMetre), speed, percent);
	}

	Steer::Steer() {
		Reset();
	}

	void Steer::Reset() {
		m_stick = 0.0f;
		m_angle = 0.0f;
		m_ramp = 1.0f;
		m_carry = 0.0f;
	}

	float Steer::Step(float dt, float stick, float speed, bool pushing, int percent) {
		if (percent <= 0 || !(dt > 0.0f)) {
			return 0.0f;
		}
		if (dt > kLongFrame) {
			dt = kLongFrame;
		}
		if (speed < 0.0f) {
			speed = -speed;
		}
		if (!(stick == stick)) {
			stick = 0.0f;
		}
		stick = Clampf(stick, -1.0f, 1.0f);

		// Skate eases the stick and the trucks by a share per tick of a 60th of a second. So that
		// a frame of any length comes out the same, the easing is done in those ticks here too.
		m_carry += dt;
		while (m_carry >= kSkateTick - 1.0e-5f) {
			m_carry -= kSkateTick;
			m_stick += (stick - m_stick) * kStickEase;
			m_ramp = Clampf(m_ramp + (pushing ? -kPushTurnDown : kPushTurnUp), kPushTurnLeast, 1.0f);
			const float side = m_stick < 0.0f ? -1.0f : 1.0f;
			const float wanted = side * TruckAngle(m_stick * side, speed / kUnitsPerMetre) * m_ramp;
			m_angle += (wanted - m_angle) * kTruckEase;
		}
		if (m_carry < 0.0f) {
			m_carry = 0.0f;
		}

		const float turn = m_angle < 0.0f ? -1.0f : 1.0f;
		return turn * RateOfAngle(m_angle * turn, speed, percent);
	}

	// ------------------------------------------------------------------------------------------
	// Spinning in the air
	// ------------------------------------------------------------------------------------------

	// THUG (physics.q): Physics_Air_Rotation_stat, Physics_Air_No_Rotate_Time
	const float kThugAirSpinWorst = 6.85f;
	const float kThugAirSpinBest = 7.75f;
	const float kThugAirSpinWait = 0.1f;
	// Skate 3, physics_bodyspin
	const float kSpinLeast = 0.4f;

	// rad/s with the stick at its end, over the seconds since the board left the ground
	static const Curve8 kSpinOfTime = {
		{ 0.0f, 0.0521f, 0.127f, 0.2443f, 0.3681f, 0.5635f, 0.9055f, 2.0f },
		{ 3.15f, 6.3643f, 7.7786f, 7.9393f, 7.7143f, 6.975f, 5.6893f, 3.6643f }
	};
	// The most the rate changes in one tick (rad/s), over the same seconds
	static const Curve8 kSpinStepOfTime = {
		{ 0.0f, 0.1156f, 0.2557f, 0.3941f, 0.5326f, 0.6775f, 0.8779f, 1.0f },
		{ 0.9929f, 0.7607f, 0.5071f, 0.35f, 0.225f, 0.1714f, 0.1571f, 0.1536f }
	};
	// What a flick counts for, over when it happened: seconds, the board leaves the ground at 0
	static const Curve8 kFlickOfTime = {
		{ -0.4967f, -0.3746f, -0.2541f, 0.0f, 0.1352f, 0.228f, 0.3371f, 0.5f },
		{ 0.0f, 0.05f, 1.0f, 1.0f, 1.0f, 0.6286f, 0.3857f, 0.2857f }
	};
	// A flick: how far the stick moved in one tick, times this, ...
	static const float kFlickGain = 1.5f;
	// ... if that times where the stick now is comes to more than this (it moved outward, and not just a little)
	static const float kFlickLeast = 0.1f;
	// The rate comes down by no more than this in one tick (rad/s)
	static const float kSpinSlowStep = 0.2f;
	// A stick closer to its centre than this has been let go
	static const float kStickNone = 1.52588e-05f;
	// Then the turning goes on from the eased stick, which keeps this much of itself a tick
	static const float kSpinCarry = 0.96f;
	// The eased stick makes up this share of what it is behind in one tick
	static const float kSpinEase = 0.2f;
	// The trail of the stick takes a new look at most this often (s): an eighth of a tick
	static const float kTrailStep = 1.0f / 480.0f;

	static inline float Absf(float v) {
		return v < 0.0f ? -v : v;
	}

	float SpinRate(float seconds) {
		return At(kSpinOfTime, seconds);
	}

	Spin::Spin() {
		Reset();
	}

	void Spin::Reset() {
		for (int i = 0; i < HISTORY; i++) {
			m_history[i] = 0.0f;
		}
		m_index = 0;
		m_trailCount = 0;
		m_flick = 0.0f;
		m_smooth = 0.0f;
		m_air = false;
		m_time = 0.0f;
		m_peak = 0.0f;
		m_rate = 0.0f;
		m_carry = 0.0f;
		m_feed = 0.0f;
		m_turned = 0.0f;
	}

	// The stick on one frame: how fast it is moving outward. Skate measures that as the way the
	// stick has gone since the tick before, a 60th of a second ago. Frames do not fall on ticks,
	// so here it is the way the stick has gone in the last 60th of a second, read off a short
	// trail of where it was (with a straight line between two looks at it). At 60 frames a
	// second that is Skate's own measure. The best of the frames since the last tick is that
	// tick's.
	void Spin::Watch(float dt, float stick) {
		for (int i = 0; i < m_trailCount; i++) {
			m_trailAge[i] += dt;
		}
		// Where the stick was a tick ago. The trail runs from the latest look back. With no
		// trail yet it was where it is: the first look says nothing about how it got there.
		float then = stick;
		if (m_trailCount > 0) {
			float newerAge = 0.0f;
			float newerStick = stick;
			then = m_trailStick[m_trailCount - 1];      // the trail is shorter than a tick: as far back as it goes
			for (int i = 0; i < m_trailCount; i++) {
				if (m_trailAge[i] >= kSkateTick) {
					const float span = m_trailAge[i] - newerAge;
					then = span > 0.0f ? newerStick + (m_trailStick[i] - newerStick) * (kSkateTick - newerAge) / span : m_trailStick[i];
					break;
				}
				newerAge = m_trailAge[i];
				newerStick = m_trailStick[i];
			}
		}
		const float moved = (stick - then) * kFlickGain;
		const float flick = moved * stick > kFlickLeast ? Clampf(moved, -1.0f, 1.0f) : 0.0f;
		if (Absf(flick) > Absf(m_flick)) {
			m_flick = flick;
		}
		// This look joins the trail, unless the last one is only a moment old. The oldest drops
		// off the end: the trail is long enough to reach back a tick whatever the frame rate.
		if (m_trailCount == 0 || m_trailAge[0] >= kTrailStep) {
			const int keep = m_trailCount < TRAIL - 1 ? m_trailCount : TRAIL - 1;
			for (int i = keep; i > 0; i--) {
				m_trailAge[i] = m_trailAge[i - 1];
				m_trailStick[i] = m_trailStick[i - 1];
			}
			m_trailAge[0] = 0.0f;
			// A stick that is let go in the air counts as being where the turning still is
			m_trailStick[0] = (m_air && Absf(stick) < kStickNone) ? m_smooth : stick;
			m_trailCount = keep + 1;
		}
	}

	// One of Skate's ticks, a 60th of a second
	void Spin::Tick(float stick, bool air) {
		const float flick = m_flick;
		m_history[m_index] = flick;
		m_index = (m_index + 1) % HISTORY;

		if (!air) {
			Land();
			m_smooth += (stick - m_smooth) * kSpinEase;
			return;
		}
		m_feed = 0.0f;

		if (!m_air) {
			// The board has just left the ground: the best flick of the half second before,
			// each for what its time counts. The one of this tick is a tick old by now.
			m_air = true;
			m_time = 0.0f;
			m_turned = 0.0f;
			m_peak = 0.0f;
			float when = 0.0f;
			int i = m_index;
			for (int k = 1; k < HISTORY; k++) {
				i = (i + HISTORY - 1) % HISTORY;
				when -= kSkateTick;
				const float counts = At(kFlickOfTime, when) * m_history[i];
				if (Absf(counts) > Absf(m_peak)) {
					m_peak = counts;
				}
			}
		}
		// A flick in the air counts too, the sooner the more
		const float counts = At(kFlickOfTime, m_time) * flick;
		if (Absf(counts) > Absf(m_peak)) {
			m_peak = counts;
		}
		const float before = m_time;
		m_time += kSkateTick;

		// The stick, or where it was when it was let go
		float input = stick;
		if (Absf(stick) < kStickNone) {
			m_smooth *= kSpinCarry;
			input = m_smooth;
			// and that is where the stick counts as having been, when it is thrown again
			m_trailCount = 1;
			m_trailAge[0] = 0.0f;
			m_trailStick[0] = m_smooth;
		}
		else {
			m_smooth += (stick - m_smooth) * kSpinEase;
		}

		const float target = At(kSpinOfTime, m_time) * input;
		if (target * m_peak < 0.0f) {
			m_peak = 0.0f;      // flicked one way, turning the other: the flick is void
		}
		// Skate counts the turning the other way round from the stick
		const float wanted = -(kSpinLeast + (1.0f - kSpinLeast) * Absf(m_peak)) * target;
		// The rate may grow by more in a tick than it may shrink. (Skate takes a rate of nothing
		// for one that turns to the left, so its turn to one side starts a tick late and ends up
		// 7 degrees short in a full pop. Here both sides start like its quicker one.)
		const float step = At(kSpinStepOfTime, before);
		const float slow = step < kSpinSlowStep ? step : kSpinSlowStep;
		const float up = m_rate < 0.0f ? slow : step;
		const float down = m_rate > 0.0f ? -slow : -step;
		m_rate += Clampf(wanted - m_rate, down, up);
	}

	void Spin::Land() {
		m_air = false;
		m_rate = 0.0f;
		m_carry = 0.0f;
	}

	void Spin::Feed(float dt, float stick) {
		if (!(dt > 0.0f)) {
			return;
		}
		if (dt > kLongFrame) {
			dt = kLongFrame;
		}
		if (!(stick == stick)) {
			stick = 0.0f;
		}
		stick = Clampf(stick, -1.0f, 1.0f);
		Watch(dt, stick);
		m_feed += dt;
		bool ticked = false;
		while (m_feed >= kSkateTick - 1.0e-5f) {
			m_feed -= kSkateTick;
			Tick(stick, false);
			ticked = true;
		}
		if (m_feed < 0.0f) {
			m_feed = 0.0f;
		}
		if (ticked) {
			m_flick = 0.0f;
		}
	}

	float Spin::Step(float dt, float stick, int percent) {
		if (percent <= 0 || !(dt > 0.0f)) {
			return 0.0f;
		}
		if (dt > kLongFrame) {
			dt = kLongFrame;
		}
		if (!(stick == stick)) {
			stick = 0.0f;
		}
		stick = Clampf(stick, -1.0f, 1.0f);
		Watch(dt, stick);
		m_carry += dt;
		bool ticked = false;
		while (m_carry >= kSkateTick - 1.0e-5f) {
			m_carry -= kSkateTick;
			Tick(stick, true);
			ticked = true;
		}
		if (m_carry < 0.0f) {
			m_carry = 0.0f;
		}
		if (ticked) {
			m_flick = 0.0f;
		}
		const float rate = -m_rate * (float)percent * 0.01f;
		m_turned += rate * dt;
		return rate;
	}

	// ------------------------------------------------------------------------------------------
	// The stance the stick asks for
	// ------------------------------------------------------------------------------------------

	StanceKeeper::StanceKeeper() {
		Reset();
	}

	void StanceKeeper::Reset() {
		m_asked = STANCE_ANY;
		m_back = false;
		m_ours = false;
		m_quiet = 0;
	}

	void StanceKeeper::Ask(int stance) {
		m_asked = stance;
		if (stance == STANCE_REGULAR) {
			m_back = true;
		}
		else if (stance == STANCE_NOLLIE) {
			m_back = false;
		}
	}

	StanceAnswer StanceKeeper::Look(const StanceLook& look) {
		StanceAnswer a;
		a.move = STANCE_STAY;
		a.endPressure = false;
		a.late = false;
		if (!look.ground || !look.nollieLoop) {
			// Whatever took him out of the nollie loop (a pop, a bump into a wall) has its own way back
			m_ours = false;
			m_quiet = 0;
		}
		if (!look.ground || (!look.regularLoop && !look.nollieLoop)) {
			return a;    // busy elsewhere: later
		}
		if (m_asked == STANCE_NOLLIE) {
			m_quiet = 0;
			a.endPressure = look.pressure;
			if (look.regularLoop) {
				a.move = STANCE_TO_NOLLIE;
				m_ours = true;
			}
			return a;
		}
		if (look.nollieLoop && m_ours && !m_back && ++m_quiet >= kStanceQuietLooks) {
			// The stick put him in nollie and has not asked for it for a while, and he is still in
			// its loop on the ground: what ended the crouch took him nowhere
			m_back = true;
			a.late = true;
		}
		if (m_back) {
			if (look.nollieLoop) {
				a.move = STANCE_TO_REGULAR;
			}
			m_back = false;
			m_ours = false;
			m_quiet = 0;
		}
		return a;
	}

	TiltTracker::TiltTracker() {
		m_expectEnd = false;
		m_flickAt = 0;
		Reset();
	}

	void TiltTracker::Reset() {
		m_expectEnd = false;
		m_airborne = false;
		m_deflected = false;
		m_clean = false;
		m_latched = false;
		m_since = 0;
		m_tilt.valid = false;
		m_tilt.first = TILT_NONE;
		m_tilt.firstLeft = false;
		m_tilt.cur = DIR_NONE;
	}

	void TiltTracker::Flick(uint32_t now, bool markNow, bool expectEnd) {
		if (markNow && m_deflected) {
			m_clean = false;
			m_latched = false;
			m_since = now;
			m_tilt.valid = false;
			m_tilt.first = TILT_NONE;
		}
		m_expectEnd = expectEnd;
		m_flickAt = now;
	}

	void TiltTracker::Update(uint32_t now, float x, float y, bool airborne, const Settings& cfg) {
		const bool tookOff = airborne && !m_airborne;
		m_airborne = airborne;

		const float high = (float)cfg.tilt * 0.01f;
		float low = high - 0.15f;
		const float dead = (float)cfg.deadzone * 0.01f;
		if (low < dead) {
			low = dead;
		}
		const float mag = std::sqrt(x * x + y * y);

		// The stick is followed in every state, so that a push can be told from what is left of a
		// flick or of a crouch
		if (!m_deflected) {
			if (mag >= high) {
				m_deflected = true;
				m_latched = false;
				m_since = now;
				// The first push after a flick left the wind-up is where that flick ends
				const bool flickEnd = m_expectEnd && Since(now, m_flickAt) < cfg.flickWindow + 100;
				m_expectEnd = false;
				m_clean = airborne && !flickEnd;
			}
		}
		else if (mag < low) {
			m_deflected = false;
		}

		if (m_deflected && tookOff) {
			// Held since before the board left the ground or the rail
			m_clean = false;
			m_latched = false;
			m_since = now;
		}

		if (!m_deflected || !airborne) {
			m_tilt.valid = false;
			m_tilt.first = TILT_NONE;
			m_tilt.cur = DIR_NONE;
			return;
		}

		const int sector = Sector(x, y);
		const int32_t held = Since(now, m_since);
		m_tilt.cur = sector;

		if (m_clean) {
			// A push is read a moment after the stick left the centre, when it has arrived
			if (!m_latched) {
				m_tilt.first = TiltClassOf(sector);
				m_tilt.firstLeft = x < 0.0f;
				if (held >= kFirstPushMs) {
					m_latched = true;
				}
			}
			m_tilt.valid = true;
		}
		else if (held >= cfg.tiltSettle) {
			// Anything else has to be held to count, and counts as pushed where it is held
			if (!m_latched) {
				m_tilt.first = TiltClassOf(sector);
				m_tilt.firstLeft = x < 0.0f;
				m_latched = true;
			}
			m_tilt.valid = true;
		}
		else {
			m_tilt.valid = false;
			m_tilt.first = TILT_NONE;
		}
	}

	// ------------------------------------------------------------------------------------------
	// Recognizer
	// ------------------------------------------------------------------------------------------

	Recognizer::Recognizer() {
		m_patterns = 0;
		Reset();
	}

	void Recognizer::UsePatterns(const PatternSet* set) {
		m_patterns = set;
		m_matcher.Reset();
		m_tickInit = false;
	}

	void Recognizer::Reset() {
		m_phase = PH_IDLE;
		m_wind = WIND_OLLIE;
		m_mirror = false;
		m_armed = false;   // the stick has to pass through the center before the first wind-up
		m_start = DIR_NONE;
		m_cur = DIR_NONE;
		m_curSince = 0;
		m_popTime = 0;
		m_trackStart = 0;
		m_hasPopped = false;
		m_cand = DIR_NONE;
		m_candSince = 0;

		m_holdOllie = true;
		m_track = TRACK_POP;
		m_railTap = false;
		m_railTapStart = 0;
		m_railTapFrames = 0;

		m_lastState = STATE_UNKNOWN;
		m_airSince = 0;

		m_histCount = 0;
		m_histNext = 0;
		m_hasFast = false;
		m_lastFast = 0;

		m_tapsLeft = 0;
		m_tapDown = false;
		m_tapStageStart = 0;
		m_tapFrames = 0;

		m_queued = 0;

		m_grabHeld = false;
		m_prevLeftTrigger = false;
		m_prevRightTrigger = false;

		m_grindPending = false;
		m_grindFrom = 0;
		m_grindUntil = 0;

		m_entering = false;
		m_enteringButton = BTN_FLIP;
		m_lastOllie = false;
		m_lastGrind = false;
		m_lastNollie = false;
		m_lastGrab = false;

		m_matcher.Reset();
		m_tickLast = 0;
		m_tickAccum = 0;
		m_tickInit = false;

		m_tilt.Reset();

		m_zone = ZONE_NONE;
		m_zoneSince = 0;
		m_railStick = false;
		m_seqStep = SEQ_IDLE;
		m_seqTarget = GRIND_UNKNOWN;
		m_seqStepStart = 0;
		m_seqFrames = 0;
		m_switchWait = false;
		m_switchSerial = 0;
		m_switchSent = 0;
		m_switchWaitFrames = 0;

		m_offDown = false;
		m_offSide = 0;
		m_offStart = 0;
		m_offFrames = 0;
		m_offWait = false;
		m_offSent = 0;
		m_offWaitFrames = 0;

		m_crouch = 0.0f;
		m_crouchAt = 0;
		m_crouchCount = 0;
		m_crouchNext = 0;
		m_leaveStart = 0;
		m_leaveStrength = 0.0f;
		m_restSince = 0;
		m_resting = false;
		m_popOut = -1.0f;
		m_hold = false;
		m_holdSlot = DIR_NONE;
		m_holdSector = DIR_NONE;
		m_holdLevel = 0;
	}

	// Once a frame while there is a crouch: how deep it is now, for a stick at "target" (0 to 1)
	void Recognizer::NoteCrouch(uint32_t now, float target) {
		const int32_t dt = m_crouchCount > 0 ? Since(now, m_crouchAt) : 0;
		const float step = dt > 0 ? (float)dt : 0.0f;
		if (target > m_crouch) {
			const float most = m_crouch + step / (float)kCrouchRampMs;
			m_crouch = target < most ? target : most;
		}
		else {
			const float least = m_crouch - step / (float)kCrouchDropMs;
			m_crouch = target > least ? target : least;
		}
		m_crouchAt = now;
		m_crouchTime[m_crouchNext] = now;
		m_crouchDepth[m_crouchNext] = m_crouch;
		m_crouchNext = (m_crouchNext + 1) % CROUCH_HISTORY;
		if (m_crouchCount < CROUCH_HISTORY) {
			m_crouchCount++;
		}
	}

	// How strong a pop out of the crouch is now: the deepest the crouch was over the last moments,
	// from a pull that only just crouches (0, a quick pop) to a full one (1)
	float Recognizer::CrouchStrength(uint32_t now, const Settings& cfg) const {
		float deepest = 0.0f;
		for (int k = 1; k <= m_crouchCount; k++) {
			const int i = (m_crouchNext - k + CROUCH_HISTORY) % CROUCH_HISTORY;
			if (k > 1 && Since(now, m_crouchTime[i]) > kCrouchPeakMs) {
				break;
			}
			if (m_crouchDepth[i] > deepest) {
				deepest = m_crouchDepth[i];
			}
		}
		const float least = (float)cfg.engage * 0.01f;
		if (!(kCrouchFull > least)) {
			return 1.0f;
		}
		const float share = (deepest - least) / (kCrouchFull - least);
		return share < 0.0f ? 0.0f : (share > 1.0f ? 1.0f : share);
	}

	// The stick is back in the crouch it left a moment ago. While it was out the crouch was shown as
	// it was, and the pop would have been as strong (m_leaveStrength): so it is again from here on.
	void Recognizer::ResumeCrouch(uint32_t now, const Settings& cfg) {
		const float least = (float)cfg.engage * 0.01f;
		const float held = least + m_leaveStrength * (kCrouchFull - least);
		if (held > m_crouch) {
			m_crouch = held;
		}
		NoteCrouch(now, m_crouch);
	}

	// The crouch ends without a pop: Ollie goes up, and the game is told that this is no jump
	void Recognizer::StandUp(const Input& in, Output& out) {
		m_phase = PH_IDLE;
		m_holdOllie = false;
		m_tapsLeft = 0;
		m_tapDown = false;
		m_crouchCount = 0;
		m_crouch = 0.0f;
		out.standUp = true;
		(void)in;
	}

	// A flip trick was asked for: if the stick stays where its flick ended, the trick may be taken
	// round again (see Input::flipMore)
	void Recognizer::HoldFlip(int slot, int sector, const Input& in) {
		m_hold = slot != DIR_NONE && sector != DIR_NONE;
		m_holdSlot = slot;
		m_holdSector = sector;
		m_holdLevel = in.flipLevel;
	}

	bool Recognizer::Busy() const {
		return m_phase != PH_IDLE || m_queued > 0 || m_grabHeld || m_grindPending || m_tapsLeft > 0 || m_railTap || m_seqStep != SEQ_IDLE || m_offDown;
	}

	// A crouch up begins. Where the game keeps the stance the stick asks for (Output::stance) it
	// does the rest; else the Nollie button is pressed as often as it takes from regular to nollie.
	void Recognizer::StartNollieTaps(const Input& in, const Settings& cfg) {
		if (m_wind == WIND_NOLLIE && cfg.nollie && !in.keepsStance) {
			m_tapsLeft = cfg.nollieTaps;
			m_tapDown = true;
			m_tapStageStart = in.now;
			m_tapFrames = 0;
		}
		else {
			m_tapsLeft = 0;
		}
	}

	void Recognizer::BeginWindup(int wind, int sector, const Input& in, const Settings& cfg) {
		m_phase = PH_WINDUP;
		m_wind = wind;
		m_start = sector;
		m_cur = sector;
		m_curSince = in.now;

		// On the ground this is the crouch: Ollie goes down. In the air and on a rail the stick
		// moves the board and the button stays up.
		m_holdOllie = !(in.state == STATE_AIR || in.state == STATE_RAIL);
		m_tapsLeft = 0;

		if (m_holdOllie) {
			// Crouching again ends the grind hold of the previous pop
			m_grindPending = false;
			StartNollieTaps(in, cfg);
		}
	}

	void Recognizer::BeginTrack(uint32_t now, int track) {
		m_phase = PH_TRACK;
		m_track = track;
		m_trackStart = now;
		m_cand = DIR_NONE;
		m_candSince = now;
		m_armed = false;
		m_tapsLeft = 0;
		m_tapDown = false;
	}

	void Recognizer::ArmGrindHold(const Input& in, const Settings& cfg) {
		// Only where the game still wants the Grind button to catch a rail
		if (cfg.autoGrind && !in.railCatch) {
			// A hold that is down already (a flick in the air) stays down and lasts longer
			const bool down = m_grindPending && Since(in.now, m_grindFrom) >= 0;
			m_grindPending = true;
			if (!down) {
				m_grindFrom = in.now + (uint32_t)cfg.autoGrindDelay;
			}
			m_grindUntil = in.now + (uint32_t)cfg.autoGrindDelay + (uint32_t)cfg.autoGrindHold;
		}
	}

	void Recognizer::Popped(uint32_t now, const Settings& cfg) {
		m_popTime = now;
		m_hasPopped = true;

		// The game has to see Ollie released before any new direction or button goes down.
		// Presses that are already running carry on, presses still waiting are pushed back.
		const uint32_t earliest = AfterPop(cfg);
		for (int i = 0; i < m_queued; i++) {
			if (m_queue[i].stage == 0 && Since(m_queue[i].notBefore, earliest) < 0) {
				m_queue[i].notBefore = earliest;
			}
		}
	}

	// The stick has left the wind-up. nearCentre: it is on its way through the middle, as opposed
	// to having been rolled out to a side.
	void Recognizer::LeaveWindup(const Input& in, const Settings& cfg, bool nearCentre) {
		if (m_holdOllie) {
			// The crouch ends: Ollie is let go, and that is the pop. How strong: how deep the
			// crouch was just before the stick left it.
			m_popOut = (m_phase == PH_LEAVE) ? m_leaveStrength : CrouchStrength(in.now, cfg);
			m_crouchCount = 0;
			m_crouch = 0.0f;
			BeginTrack(in.now, TRACK_POP);
			ArmGrindHold(in, cfg);
			Popped(in.now, cfg);
			m_tilt.Flick(in.now, true, nearCentre);
		}
		else if (in.state == STATE_RAIL) {
			// On a rail letting go is not a pop. Wait and see whether this is a flick.
			BeginTrack(in.now, TRACK_RAIL);
			m_tilt.Flick(in.now, false, nearCentre);
		}
		else {
			// In the air nothing was held. A flick across the stick will be a flip; rolling the
			// stick round to a side goes on tilting the board.
			BeginTrack(in.now, TRACK_AIR);
			m_tilt.Flick(in.now, false, nearCentre);
		}
	}

	// Called once per frame: remembers where the stick was and whether it is moving at flick speed
	void Recognizer::NoteSpeed(const Input& in, const Settings& cfg) {
		// The sample about kSpeedWindowMs back, or the previous one if frames are longer than that
		int best = -1;
		for (int k = 1; k <= m_histCount; k++) {
			const int i = (m_histNext - k + HISTORY) % HISTORY;
			const int32_t age = Since(in.now, m_histTime[i]);
			if (age > kSpeedWindowMs && best >= 0) {
				break;
			}
			best = i;
			if (age > kSpeedWindowMs) {
				break;
			}
		}
		if (best >= 0) {
			const int32_t dt = Since(in.now, m_histTime[best]);
			if (dt > 0 && dt <= 250) {
				const float dx = in.rx - m_histX[best];
				const float dy = in.ry - m_histY[best];
				// percent of full deflection per 100 ms
				const float speed = std::sqrt(dx * dx + dy * dy) * 10000.0f / (float)dt;
				if (speed >= (float)cfg.flickSpeed) {
					m_hasFast = true;
					m_lastFast = in.now;
				}
			}
		}
		m_histTime[m_histNext] = in.now;
		m_histX[m_histNext] = in.rx;
		m_histY[m_histNext] = in.ry;
		m_histNext = (m_histNext + 1) % HISTORY;
		if (m_histCount < HISTORY) {
			m_histCount++;
		}
	}

	// Did the stick move at flick speed on its way out of the wind-up?
	bool Recognizer::WasFast(const Settings& cfg) const {
		if (cfg.flickSpeed <= 0) {
			return true;
		}
		return m_hasFast && Since(m_lastFast, m_trackStart) >= -kSpeedWindowMs;
	}

	// A flick on a rail: Ollie is pressed for a moment, and letting it go is the pop
	void Recognizer::RailPop(const Input& in, const Settings& cfg) {
		if (!m_railTap) {
			m_railTap = true;
			m_railTapStart = in.now;
			m_railTapFrames = 0;
		}

		// Presses left over from before, that have not put their button down yet, are dropped: from
		// here on there is this pop and the trick that goes with it
		int kept = 0;
		for (int i = 0; i < m_queued; i++) {
			if (m_queue[i].stage == 2) {
				m_queue[kept++] = m_queue[i];
			}
		}
		m_queued = kept;

		// Nothing else is sent to the rail from here on
		m_seqStep = SEQ_IDLE;
		m_switchWait = true;
		m_switchSerial = in.grindSerial;
		m_switchSent = in.now;
		m_switchWaitFrames = 0;

		ArmGrindHold(in, cfg);
		m_tilt.Flick(in.now, true, false);
	}

	uint32_t Recognizer::AfterPop(const Settings& cfg) const {
		return m_popTime + (uint32_t)(cfg.afterPop > 0 ? cfg.afterPop : 1);
	}

	void Recognizer::Finalize(int end, const Input& in, const Settings& cfg, Output& out) {
		if (m_track != TRACK_POP) {
			// Nothing has been pressed so far. On a rail any flick pops, and the stick going back to
			// the centre does not. In the air only a flick into the far half is a flip.
			bool isFlick = (end != DIR_NONE) && WasFast(cfg);
			if (isFlick && m_track == TRACK_AIR) {
				isFlick = TiltClassOf(end) == (m_wind == WIND_OLLIE ? TILT_UP : TILT_DOWN);
			}
			if (!isFlick) {
				m_tilt.Flick(in.now, false, false);
				m_track = TRACK_POP;
				m_phase = PH_IDLE;
				return;
			}
		}

		const int slot = MapFlip(m_wind, m_start, end, m_mirror ? 1 : 0);

		out.gesture = (slot == DIR_NONE) ? GESTURE_POP : GESTURE_FLIP;
		out.windup = m_wind;
		out.start = m_start;
		out.end = end;
		out.slot = slot;
		out.elapsed = in.now - m_trackStart;

		if (m_track == TRACK_RAIL) {
			RailPop(in, cfg);
		}
		else {
			if (m_track == TRACK_AIR) {
				ArmGrindHold(in, cfg);
			}
			// The flick is over: where the stick is now is its end, not a tilt
			m_tilt.Flick(in.now, true, false);
		}
		if (slot != DIR_NONE) {
			// Never on the frame of the pop itself: the game has to see Ollie released first.
			// After a pop on a rail the press waits for the Ollie tap to end (see RunQueue).
			Enqueue(BTN_FLIP, slot, AfterPop(cfg));
			// A stick that stays out where the flick ended holds the trick (see Update)
			HoldFlip(slot, end, in);
		}
		m_track = TRACK_POP;
		m_phase = PH_IDLE;
	}

	void Recognizer::FinalizePattern(const Pattern& p, const PatternMatch& m, const Input& in, const Settings& cfg, Output& out) {
		out.gesture = (p.slot == DIR_NONE) ? GESTURE_POP : GESTURE_FLIP;
		out.windup = m_wind;
		out.start = m_start;
		out.end = DIR_NONE;
		out.slot = p.slot;
		out.elapsed = in.now - m_trackStart;
		out.pattern = p.name;
		out.speed = m.speed;
		out.ticks = m.ticks;

		if (m_track == TRACK_RAIL) {
			// A completed pattern is a flick
			RailPop(in, cfg);
		}
		else {
			if (m_track == TRACK_AIR) {
				ArmGrindHold(in, cfg);
			}
			m_tilt.Flick(in.now, true, false);
		}
		if (p.slot != DIR_NONE) {
			for (int i = 0; i < p.taps; i++) {
				Enqueue(BTN_FLIP, p.slot, AfterPop(cfg));
			}
			// The pattern ends where the stick is: if it stays there, the trick may be held
			const float mag = std::sqrt(in.rx * in.rx + in.ry * in.ry);
			HoldFlip(p.slot, mag >= (float)cfg.release * 0.01f ? Sector(in.rx, in.ry) : DIR_NONE, in);
		}
		m_track = TRACK_POP;
		m_phase = PH_IDLE;
	}

	void Recognizer::Enqueue(int button, int dir, uint32_t notBefore) {
		if (m_queued >= MAX_QUEUE) {
			return;
		}
		Pulse& p = m_queue[m_queued++];
		p.button = button;
		p.dir = dir;
		p.notBefore = notBefore;
		p.stage = 0;
		p.stageStart = 0;
		p.frames = 0;
	}

	void Recognizer::RunTaps(uint32_t now, Output& out) {
		if (m_tapsLeft <= 0) {
			return;
		}
		if (m_tapFrames >= 2 && Since(now, m_tapStageStart) >= kTapMs) {
			if (m_tapDown) {
				m_tapDown = false;
				m_tapsLeft--;
			}
			else {
				m_tapDown = true;
			}
			m_tapStageStart = now;
			m_tapFrames = 0;
		}
		if (m_tapsLeft > 0) {
			out.nollie = m_tapDown;
			m_tapFrames++;
		}
	}

	void Recognizer::RunQueue(const Input& in, const Settings& cfg, Output& out) {
		const uint32_t now = in.now;
		bool releaseGrab = false;
		m_entering = false;

		if (m_queued > 0) {
			Pulse& p = m_queue[0];

			// Advance. Every stage is shown to the game for at least one frame (direction)
			// or two frames (direction + button), whatever the frame rate is.
			if (p.stage == 0) {
				// While Ollie is down for the pop out of a grind nothing else starts
				if (!m_railTap && Since(now, p.notBefore) >= 0) {
					p.stage = 1;
					p.stageStart = now;
					p.frames = 0;
				}
			}
			else if (p.stage == 1) {
				if (p.frames >= 1 && Since(now, p.stageStart) >= cfg.dirLead) {
					p.stage = 2;
					p.stageStart = now;
					p.frames = 0;
				}
			}
			else if (p.frames >= 2 && Since(now, p.stageStart) >= cfg.pulse) {
				if (p.button == BTN_GRAB && (in.leftTrigger || in.rightTrigger)) {
					m_grabHeld = true;
				}
				for (int i = 1; i < m_queued; i++) {
					m_queue[i - 1] = m_queue[i];
				}
				m_queued--;
				// The next press waits for the following frame, so the button is seen released in between
			}
		}

		if (m_queued > 0) {
			Pulse& p = m_queue[0];
			if (p.stage == 1) {
				out.dir = p.dir;
				out.ownsDirection = true;
				if (p.button == BTN_GRAB) {
					releaseGrab = true;   // let go of a held grab before pressing the next one
				}
				m_entering = true;
				m_enteringButton = p.button;
				p.frames++;
			}
			else if (p.stage == 2) {
				out.dir = p.dir;
				out.ownsDirection = true;
				if (p.button == BTN_FLIP) {
					out.flip = true;
				}
				else {
					out.grab = true;
				}
				if (p.frames == 0) {
					// the frame the button goes down
					m_entering = true;
					m_enteringButton = p.button;
				}
				p.frames++;
			}
		}

		if (m_grabHeld && !releaseGrab) {
			out.grab = true;
		}
	}

	// On a rail the stick at rest says which of the three plain grinds it wants: lower half 5-0,
	// upper half nosegrind, centre 50-50. THUG switches between grinds on two button presses in a
	// row (Flip then Grab for the 5-0, Grind then Grab for the nosegrind, Grind twice for the
	// 50-50), and takes them only once a grind has settled, which is what in.grindReady reports.
	void Recognizer::RunRailSwitch(const Input& in, const Settings& cfg, float mag, int sector, Output& out) {
		const uint32_t now = in.now;
		const float dead = (float)cfg.deadzone * 0.01f;
		const float tilt = (float)cfg.tilt * 0.01f;

		int zone = ZONE_NONE;
		if (mag < dead) {
			zone = ZONE_CENTER;
		}
		else if (mag >= tilt) {
			const int cls = TiltClassOf(sector);
			if (cls == TILT_DOWN) {
				zone = ZONE_DOWN;
			}
			else if (cls == TILT_UP) {
				zone = ZONE_UP;
			}
		}
		if (zone != m_zone) {
			m_zone = zone;
			m_zoneSince = now;
		}
		const bool rested = zone != ZONE_NONE && Since(now, m_zoneSince) >= cfg.switchHold;
		if (rested && zone != ZONE_CENTER) {
			m_railStick = true;
		}

		if (!cfg.grindSwitch || m_railTap) {
			m_seqStep = SEQ_IDLE;
			return;
		}

		if (m_seqStep == SEQ_IDLE) {
			if (m_offDown || (cfg.grindExit && in.railOff)) {
				// The board is out of speed and stepping off this rail: no other grind is started on it
				return;
			}
			if (m_switchWait) {
				// Sent already. The game reports the new grind a moment later; if it never does, the
				// presses were lost and may go out again.
				m_switchWaitFrames++;
				if (in.grindSerial != m_switchSerial) {
					m_switchWait = false;
				}
				else if (Since(now, m_switchSent) >= kSwitchWaitMs && m_switchWaitFrames >= kSwitchWaitFrames) {
					m_switchWait = false;
				}
				if (m_switchWait) {
					return;
				}
			}
			if (!in.grindReady || !rested || m_phase == PH_TRACK) {
				return;
			}
			if (in.grind != GRIND_FIFTY && in.grind != GRIND_FIVE_O && in.grind != GRIND_NOSEGRIND) {
				return;
			}
			int want = GRIND_UNKNOWN;
			if (zone == ZONE_DOWN) {
				want = GRIND_FIVE_O;
			}
			else if (zone == ZONE_UP) {
				want = GRIND_NOSEGRIND;
			}
			else if (m_railStick) {
				// Back to the centre. A grind the stick never touched is left alone.
				want = GRIND_FIFTY;
			}
			if (want == GRIND_UNKNOWN || want == in.grind) {
				return;
			}
			m_seqTarget = want;
			m_seqStep = SEQ_FIRST;
			m_seqStepStart = now;
			m_seqFrames = 0;
			out.switchTo = want;
		}
		else {
			const int need = (m_seqStep == SEQ_GAP) ? kSwitchGapMs : kSwitchTapMs;
			if (m_seqFrames >= 2 && Since(now, m_seqStepStart) >= need) {
				if (m_seqStep == SEQ_SECOND) {
					m_seqStep = SEQ_IDLE;
					m_switchWait = true;
					m_switchSerial = in.grindSerial;
					m_switchSent = now;
					m_switchWaitFrames = 0;
				}
				else {
					m_seqStep++;
					m_seqStepStart = now;
					m_seqFrames = 0;
				}
			}
		}

		if (m_seqStep == SEQ_FIRST) {
			if (m_seqTarget == GRIND_FIVE_O) {
				out.flip = true;
			}
			else {
				out.grind = true;
			}
			m_seqFrames++;
		}
		else if (m_seqStep == SEQ_SECOND) {
			if (m_seqTarget == GRIND_FIFTY) {
				out.grind = true;
			}
			else {
				out.grab = true;
			}
			m_seqFrames++;
		}
		else if (m_seqStep == SEQ_GAP) {
			m_seqFrames++;
		}
	}

	// The game says the board has stopped on this rail (RailRun::WantsOff). THUG has a button that
	// takes the skater off a rail, and it is pressed here: for a moment, together with the side to
	// step off to. A skater who is still on the rail half a second later gets it again.
	void Recognizer::RunStepOff(const Input& in, const Settings& cfg, Output& out) {
		const uint32_t now = in.now;

		if (m_railTap) {
			// The player pops off the rail: that is the way off, and Ollie the only button for it.
			// The frame Ollie is let go the game has not jumped yet, so nothing is pressed then
			// either: a board that is still on the rail after that gets the button once the wait
			// below has run.
			m_offDown = false;
			m_offWait = true;
			m_offSent = now;
			m_offWaitFrames = 0;
			return;
		}

		// A trick press that is on its way has the buttons and the direction to itself
		const bool others = out.flip || out.grab || out.grind || out.ownsDirection;

		if (m_offDown) {
			if (others || (m_offFrames >= 2 && Since(now, m_offStart) >= kOffTapMs)) {
				m_offDown = false;
				m_offWait = true;
				m_offSent = now;
				m_offWaitFrames = 0;
			}
		}
		else if (m_offWait) {
			m_offWaitFrames++;
			if (Since(now, m_offSent) >= kOffWaitMs && m_offWaitFrames >= kOffWaitFrames) {
				m_offWait = false;
			}
		}
		else if (cfg.grindExit && in.railOff && !others && m_phase != PH_TRACK && m_seqStep == SEQ_IDLE && m_queued == 0) {
			// Not while a flick may still turn into a pop, and not in the middle of other presses
			m_offDown = true;
			m_offSide = in.offSide < 0 ? -1 : (in.offSide > 0 ? 1 : 0);
			m_offStart = now;
			m_offFrames = 0;
			out.stepStarted = true;
		}

		if (m_offDown) {
			out.stepOff = true;
			out.stepSide = m_offSide;
			m_offFrames++;
		}
	}

	void Recognizer::Update(const Input& in, const Settings& cfg, Output& out) {
		out.ollie = false;
		out.flip = false;
		out.grab = false;
		out.grind = false;
		out.nollie = false;
		out.dir = DIR_NONE;
		out.ownsDirection = false;
		out.gesture = GESTURE_NONE;
		out.windup = WIND_OLLIE;
		out.start = DIR_NONE;
		out.end = DIR_NONE;
		out.slot = DIR_NONE;
		out.elapsed = 0;
		out.pattern = 0;
		out.speed = 0.0f;
		out.ticks = 0;
		out.state = in.state;
		out.switchTo = GRIND_UNKNOWN;
		out.stepOff = false;
		out.stepSide = 0;
		out.stepStarted = false;
		out.popStrength = -1.0f;
		out.standUp = false;
		out.crouchPose = -1.0f;
		out.stance = STANCE_ANY;
		out.mirrored = m_mirror;
		out.holding = false;
		out.flipAgain = false;
		m_popOut = -1.0f;

		const uint32_t now = in.now;
		const float dead = (float)cfg.deadzone * 0.01f;
		const float engage = (float)cfg.engage * 0.01f;
		const float release = (float)cfg.release * 0.01f;
		const float flick = (float)cfg.flick * 0.01f;
		const float snap = (float)cfg.snap * 0.01f;

		const bool onRail = (in.state == STATE_RAIL);
		const bool inAir = (in.state == STATE_AIR);

		float mag = std::sqrt(in.rx * in.rx + in.ry * in.ry);
		if (mag > 1.0f) {
			mag = 1.0f;
		}
		const bool hasAngle = mag > 0.001f;
		const float angle = hasAngle ? AngleDeg(in.rx, in.ry) : 0.0f;
		const int sector = hasAngle ? Sector(in.rx, in.ry) : DIR_NONE;

		// How the board leaned on the last frame: if this frame is the first one on a rail, that is
		// what it touched down with
		const bool tiltWasValid = m_tilt.Get().valid;
		m_tilt.Update(now, in.rx, in.ry, inAir, cfg);
		NoteSpeed(in, cfg);

		if (inAir && m_lastState != STATE_AIR) {
			m_airSince = now;
		}
		if (onRail && m_lastState != STATE_RAIL) {
			// Whatever the stick does as the board touches down is deliberate: it may be the start
			// of a wind-up without having passed the centre first. Nothing is pressed for it.
			m_armed = true;
			m_railStick = tiltWasValid;
			m_zone = ZONE_NONE;
			m_zoneSince = now;
			m_seqStep = SEQ_IDLE;
			m_switchWait = false;
			m_offDown = false;
			m_offWait = false;
			// The grab ends where the grind begins, and one that was about to start does not
			m_grabHeld = false;
			int kept = 0;
			for (int i = 0; i < m_queued; i++) {
				if (m_queue[i].button != BTN_GRAB || m_queue[i].stage == 2) {
					m_queue[kept++] = m_queue[i];
				}
			}
			m_queued = kept;
		}
		if (!onRail) {
			m_seqStep = SEQ_IDLE;
			m_switchWait = false;
			m_offDown = false;
			m_offWait = false;
		}

		// The Ollie press of a pop out of a grind ends here. Trick presses that waited for it may
		// start once the game has seen the button up.
		bool tapEnded = false;
		if (m_railTap && m_railTapFrames >= 2 && Since(now, m_railTapStart) >= cfg.railTap) {
			m_railTap = false;
			tapEnded = true;
			Popped(now, cfg);
		}

		// Which way round the flicks are read: KickflipSide, and mirrored once more while he rides
		// switch, since Skate's flicks go by the rider's feet (switch, the kickflip is on the other
		// side). A gesture keeps the side it began on: the side is taken up between gestures only,
		// and patterns half drawn the other way round start over.
		const bool mirror = (cfg.kickflipSide != 0) != (cfg.switchMirror != 0 && in.switched);
		if (m_phase == PH_IDLE && mirror != m_mirror) {
			m_mirror = mirror;
			m_matcher.Reset();
		}

		// Stick patterns run on a fixed 60 Hz clock, whatever the frame rate is: the stick position of
		// this frame is fed once per tick that has passed.
		const bool usePatterns = cfg.patterns && m_patterns != 0 && m_patterns->Count() > 0;
		bool matched = false;
		PatternMatch match;
		match.index = 0;
		match.speed = 0.0f;
		match.ticks = 0;
		match.error = 0.0f;
		if (usePatterns) {
			int ticks = 1;
			if (m_tickInit) {
				int32_t elapsed = Since(now, m_tickLast);
				if (elapsed < 0 || elapsed > 200) {
					elapsed = 200;
				}
				m_tickAccum += (int)elapsed * 3;
				ticks = m_tickAccum / 50;
				m_tickAccum -= ticks * 50;
				if (ticks > 8) {
					ticks = 8;
				}
			}
			else {
				m_tickInit = true;
				m_tickAccum = 0;
			}
			m_tickLast = now;
			// KickflipSide mirrors the stick, and so does riding switch: every pattern is mirrored with it
			const float px = m_mirror ? -in.rx : in.rx;
			// On a rail a flick pops in either direction, so the patterns that start at the top count there
			const bool nolliePatterns = cfg.nollie != 0 || onRail;
			for (int i = 0; i < ticks; i++) {
				PatternMatch m;
				if (m_matcher.Tick(*m_patterns, px, -in.ry, cfg.missTicks, nolliePatterns, m)) {
					matched = true;
					match = m;
				}
			}
		}

		// A new wind-up needs the stick to have been through the center, or out to a side.
		// That keeps the overshoot of a flick from being read as the next wind-up.
		if (mag < dead) {
			m_armed = true;
		}
		else if (m_phase == PH_IDLE && mag >= engage && (sector == DIR_L || sector == DIR_R)) {
			m_armed = true;
		}

		// Stick up is a wind-up on a rail, and with Nollie on the ground: the crouch for a nollie. In
		// the air it leans the board, as it always did.
		const bool nollieHere = onRail || (cfg.nollie && !inAir);
		if (m_phase == PH_IDLE && m_armed && mag >= engage) {
			if (InWindupHalf(angle, WIND_OLLIE, 0.0f)) {
				BeginWindup(WIND_OLLIE, sector, in, cfg);
			}
			else if (nollieHere && InWindupHalf(angle, WIND_NOLLIE, 0.0f)) {
				BeginWindup(WIND_NOLLIE, sector, in, cfg);
			}
		}

		if (m_phase == PH_WINDUP && m_wind == WIND_NOLLIE && !nollieHere && !m_holdOllie) {
			// Off the rail (with Nollie off), or in the air: a wind-up up is over. A crouch up carried
			// off the ground keeps Ollie down for as long as the game would still jump (below).
			m_phase = PH_IDLE;
			m_armed = false;
		}

		if (m_phase == PH_WINDUP || m_phase == PH_LEAVE) {
			if (!m_holdOllie) {
				if (!inAir && !onRail) {
					// Begun in the air or on a rail, and now the board is on the ground: this is a crouch
					m_holdOllie = true;
					m_grindPending = false;
					StartNollieTaps(in, cfg);
				}
			}
			else if (inAir && Since(now, m_airSince) >= kAirReleaseMs) {
				// A crouch carried off the ground. Past the time in which the game would still jump,
				// the button is let go without anything happening, so that landing on a rail with
				// the stick held is not a crouch that pops when the stick is released.
				m_holdOllie = false;
				m_tapsLeft = 0;
				m_tapDown = false;
				if (m_phase == PH_LEAVE) {
					m_phase = PH_IDLE;
				}
			}
		}

		if (m_phase == PH_WINDUP) {
			const bool inHalf = hasAngle && InWindupHalf(angle, m_wind, kHalfMargin);
			if (m_holdOllie) {
				// The crouch follows the stick
				NoteCrouch(now, inHalf ? mag : 0.0f);
			}
			if ((mag < release || !inHalf) && m_holdOllie) {
				// The stick leaves the crouch. Ollie stays down: a flick pops (below), the stick
				// coming to rest at the centre stands him up again without one.
				m_leaveStrength = CrouchStrength(now, cfg);
				m_phase = PH_LEAVE;
				m_leaveStart = now;
				m_resting = false;
			}
			else if (mag < release || !inHalf) {
				LeaveWindup(in, cfg, mag < release);
			}
			else if (InWindupHalf(angle, m_wind, 0.0f)) {
				if (sector != m_cur) {
					m_cur = sector;
					m_curSince = now;
				}
				else if (Since(now, m_curSince) >= cfg.dwell) {
					m_start = sector;
				}
			}
		}

		if (m_phase == PH_LEAVE) {
			// The crouch stays as it was while the stick is out of it: what comes next is a pop,
			// a stand-up, or the stick back in the crouch, taking it up where it was.
			// Out of the crouch and well away from it in any other direction: the flick, and the pop.
			// With patterns only a pattern pops, as in Skate.
			const bool away = hasAngle && !InWindupHalf(angle, m_wind, 0.0f) && mag >= flick;
			if (!usePatterns && away) {
				LeaveWindup(in, cfg, true);
			}
		}

		if (usePatterns) {
			if (matched && (m_phase == PH_WINDUP || m_phase == PH_TRACK || m_phase == PH_LEAVE)) {
				const Pattern& p = m_patterns->At(match.index);
				// In the air, with nothing held, only a pattern that ends across the stick is a flip.
				// One that ends at a side or back where it began was the board being tilted.
				const bool inAirGesture = (m_phase == PH_WINDUP || m_phase == PH_LEAVE) ? (!m_holdOllie && !onRail) : (m_track == TRACK_AIR);
				const bool across = hasAngle && InWindupHalf(angle, 1 - m_wind, 0.0f);
				if ((p.nollie != 0) == (m_wind == WIND_NOLLIE) && (!inAirGesture || across)) {
					if (m_phase == PH_WINDUP || m_phase == PH_LEAVE) {
						// The pattern is complete: this is the pop
						LeaveWindup(in, cfg, m_phase == PH_LEAVE);
					}
					// Where nothing was held, a pattern drawn slowly is not a flick
					if (m_track == TRACK_POP || WasFast(cfg)) {
						FinalizePattern(p, match, in, cfg, out);
					}
				}
			}
			if (m_phase == PH_TRACK) {
				if (hasAngle && mag >= engage && InWindupHalf(angle, m_wind, 0.0f)) {
					// Pulled straight back into the wind-up: that pop had no flick
					const int wind = m_wind;
					Finalize(DIR_NONE, in, cfg, out);
					BeginWindup(wind, sector, in, cfg);
				}
				else if (Since(now, m_trackStart) >= cfg.flickWindow) {
					// No pattern was completed in time: a plain ollie
					Finalize(DIR_NONE, in, cfg, out);
				}
			}
		}
		else if (m_phase == PH_TRACK) {
			const bool validEnd = hasAngle && !InWindupHalf(angle, m_wind, 0.0f);
			const bool opposite = hasAngle && InWindupHalf(angle, 1 - m_wind, 0.0f);

			if (validEnd && mag >= flick) {
				if (sector != m_cand) {
					m_cand = sector;
					m_candSince = now;
				}
				if (opposite && mag >= snap) {
					// A clear flick into the far half: nothing further can change the answer
					Finalize(m_cand, in, cfg, out);
				}
				else if (Since(now, m_candSince) >= cfg.settle) {
					// A drag that came to rest
					Finalize(m_cand, in, cfg, out);
				}
			}
			else if (m_cand != DIR_NONE && mag < release) {
				// The stick sprang back
				Finalize(m_cand, in, cfg, out);
			}
			else if (hasAngle && mag >= engage && InWindupHalf(angle, m_wind, 0.0f)) {
				// Pulled straight back into the wind-up: that pop had no flick
				const int wind = m_wind;
				Finalize(DIR_NONE, in, cfg, out);
				BeginWindup(wind, sector, in, cfg);
			}

			if (m_phase == PH_TRACK && Since(now, m_trackStart) >= cfg.flickWindow) {
				Finalize(m_cand, in, cfg, out);
			}
		}

		if (m_phase == PH_LEAVE) {
			if (hasAngle && mag >= engage && InWindupHalf(angle, m_wind, 0.0f)) {
				// Back into the crouch, which takes up as deep as it was when the stick left it
				m_phase = PH_WINDUP;
				ResumeCrouch(now, cfg);
				if (sector != m_cur) {
					m_cur = sector;
					m_curSince = now;
				}
			}
			else {
				// At rest in the centre, or no flick in time: he stands up again
				if (mag < dead) {
					if (!m_resting) {
						m_resting = true;
						m_restSince = now;
					}
				}
				else {
					m_resting = false;
				}
				if ((m_resting && Since(now, m_restSince) >= cfg.settle) || Since(now, m_leaveStart) >= cfg.flickWindow) {
					StandUp(in, out);
					m_armed = mag < dead;
				}
			}
		}

		// The frame a pop out of a grind lets go of Ollie, the button is up whatever else is going on
		out.ollie = !tapEnded && (((m_phase == PH_WINDUP || m_phase == PH_LEAVE) && m_holdOllie) || m_railTap);

		if ((m_phase == PH_WINDUP || m_phase == PH_LEAVE) && m_holdOllie) {
			RunTaps(now, out);
		}

		// The stick held where a flip trick's flick ended: while the game has another rotation of the
		// trick to give (see Input::flipMore), the trick button is pressed once more for it
		if (m_hold) {
			const bool still = hasAngle && mag >= release && m_holdSector != DIR_NONE && SectorGap(sector, m_holdSector) <= 1;
			if (!still || m_phase == PH_WINDUP || m_phase == PH_LEAVE) {
				m_hold = false;
			}
			else if (in.flipMore && in.flipLevel != m_holdLevel) {
				Enqueue(BTN_FLIP, m_holdSlot, now);
				m_holdLevel = in.flipLevel;
				out.flipAgain = true;
			}
		}
		out.holding = m_hold;
		out.popStrength = m_popOut;
		if ((m_phase == PH_WINDUP || m_phase == PH_LEAVE) && m_holdOllie) {
			// As deep as a pop would be now. A flick swings the stick out of the crouch, and the
			// moment it takes is not the skater standing up.
			out.crouchPose = m_phase == PH_LEAVE ? m_leaveStrength : CrouchStrength(now, cfg);
		}
		// The stance: nollie while the stick crouches up, regular while it crouches down, and
		// regular again when a crouch up ends without a pop. A pop asks for nothing: the game's jump
		// from nollie is the nollie, and its landing takes him out of it.
		if (cfg.nollie) {
			if ((m_phase == PH_WINDUP || m_phase == PH_LEAVE) && m_holdOllie) {
				out.stance = m_wind == WIND_NOLLIE ? STANCE_NOLLIE : STANCE_REGULAR;
			}
			else if (out.standUp && m_wind == WIND_NOLLIE) {
				out.stance = STANCE_REGULAR;
			}
		}

		if (cfg.triggerGrabs && !onRail) {
			uint32_t grabStart = now;
			if (m_hasPopped && Since(AfterPop(cfg), now) > 0) {
				grabStart = AfterPop(cfg);
			}
			if (in.leftTrigger && !m_prevLeftTrigger) {
				Enqueue(BTN_GRAB, DIR_L, grabStart);
				if (out.gesture == GESTURE_NONE) {
					out.gesture = GESTURE_GRAB;
					out.slot = DIR_L;
				}
			}
			if (in.rightTrigger && !m_prevRightTrigger) {
				Enqueue(BTN_GRAB, DIR_R, grabStart);
				if (out.gesture == GESTURE_NONE) {
					out.gesture = GESTURE_GRAB;
					out.slot = DIR_R;
				}
			}
			if (!in.leftTrigger && !in.rightTrigger) {
				m_grabHeld = false;
			}
		}
		else {
			m_grabHeld = false;
		}
		// No grabs on a rail: there Grab is one of the buttons that switch grinds. A trigger that is
		// still held when the board leaves the rail grabs then.
		m_prevLeftTrigger = in.leftTrigger && !onRail;
		m_prevRightTrigger = in.rightTrigger && !onRail;

		RunQueue(in, cfg, out);

		if (m_grindPending) {
			if (in.railCatch || onRail || Since(now, m_grindUntil) >= 0) {
				m_grindPending = false;
			}
			else if (Since(now, m_grindFrom) >= 0) {
				out.grind = true;
			}
		}

		// A precaution: the game may want the direction and the button of a trick back to back,
		// with no other button event in between. So from the frame the direction goes down to
		// the frame the button goes down, every other button keeps the state it had. A pop, a
		// crouch or the grind hold that falls into that gap waits the one or two frames until
		// the press is in.
		if (m_entering) {
			if (!tapEnded) {
				out.ollie = m_lastOllie;
			}
			out.grind = m_lastGrind;
			out.nollie = m_lastNollie;
			if (m_enteringButton == BTN_FLIP) {
				out.grab = m_lastGrab;
			}
		}
		m_lastOllie = out.ollie;
		m_lastGrind = out.grind;
		m_lastNollie = out.nollie;
		m_lastGrab = out.grab;

		// The presses that switch grinds and the one that steps off the rail come on top and are
		// not part of the above
		if (onRail) {
			RunStepOff(in, cfg, out);
			RunRailSwitch(in, cfg, mag, sector, out);
		}

		if (m_railTap && out.ollie) {
			m_railTapFrames++;
		}
		m_lastState = in.state;
		out.mirrored = m_mirror;
	}
}
