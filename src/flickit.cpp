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
		s.nollie = 0;
		s.nollieTaps = 2;
		s.triggerGrabs = 1;
		s.autoGrind = 1;

		s.deadzone = 25;
		s.engage = 60;
		s.release = 45;
		s.flick = 50;
		s.snap = 75;

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
	}

	void Sanitize(Settings& s) {
		s.enabled = s.enabled ? 1 : 0;
		s.debug = s.debug ? 1 : 0;
		s.kickflipSide = s.kickflipSide ? 1 : 0;
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
					std::strncpy(cur->name, arg, MAX_PATTERN_NAME - 1);
					cur->name[MAX_PATTERN_NAME - 1] = '\0';
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
		m_armed = false;   // the stick has to pass through the center before the first wind-up
		m_start = DIR_NONE;
		m_cur = DIR_NONE;
		m_curSince = 0;
		m_popTime = 0;
		m_hasPopped = false;
		m_cand = DIR_NONE;
		m_candSince = 0;

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
	}

	bool Recognizer::Busy() const {
		return m_phase != PH_IDLE || m_queued > 0 || m_grabHeld || m_grindPending || m_tapsLeft > 0;
	}

	void Recognizer::BeginWindup(int wind, int sector, uint32_t now, const Settings& cfg) {
		m_phase = PH_WINDUP;
		m_wind = wind;
		m_start = sector;
		m_cur = sector;
		m_curSince = now;

		// Crouching again ends the grind hold of the previous pop
		m_grindPending = false;

		if (wind == WIND_NOLLIE) {
			m_tapsLeft = cfg.nollieTaps;
			m_tapDown = true;
			m_tapStageStart = now;
			m_tapFrames = 0;
		}
		else {
			m_tapsLeft = 0;
		}
	}

	void Recognizer::Pop(uint32_t now, const Settings& cfg) {
		m_phase = PH_TRACK;
		m_popTime = now;
		m_hasPopped = true;
		m_cand = DIR_NONE;
		m_candSince = now;
		m_armed = false;
		m_tapsLeft = 0;
		m_tapDown = false;

		if (cfg.autoGrind) {
			m_grindPending = true;
			m_grindFrom = now + (uint32_t)cfg.autoGrindDelay;
			m_grindUntil = m_grindFrom + (uint32_t)cfg.autoGrindHold;
		}

		// The game has to see Ollie released before any new direction or button goes down.
		// Presses that are already running carry on, presses still waiting are pushed back.
		const uint32_t earliest = AfterPop(cfg);
		for (int i = 0; i < m_queued; i++) {
			if (m_queue[i].stage == 0 && Since(m_queue[i].notBefore, earliest) < 0) {
				m_queue[i].notBefore = earliest;
			}
		}
	}

	uint32_t Recognizer::AfterPop(const Settings& cfg) const {
		return m_popTime + (uint32_t)(cfg.afterPop > 0 ? cfg.afterPop : 1);
	}

	void Recognizer::Finalize(int end, uint32_t now, const Settings& cfg, Output& out) {
		const int slot = MapFlip(m_wind, m_start, end, cfg.kickflipSide);

		out.gesture = (slot == DIR_NONE) ? GESTURE_POP : GESTURE_FLIP;
		out.windup = m_wind;
		out.start = m_start;
		out.end = end;
		out.slot = slot;
		out.elapsed = now - m_popTime;

		if (slot != DIR_NONE) {
			// Never on the frame of the pop itself: the game has to see Ollie released first
			Enqueue(BTN_FLIP, slot, AfterPop(cfg));
		}
		m_phase = PH_IDLE;
	}

	void Recognizer::FinalizePattern(const Pattern& p, const PatternMatch& m, uint32_t now, const Settings& cfg, Output& out) {
		out.gesture = (p.slot == DIR_NONE) ? GESTURE_POP : GESTURE_FLIP;
		out.windup = m_wind;
		out.start = m_start;
		out.end = DIR_NONE;
		out.slot = p.slot;
		out.elapsed = now - m_popTime;
		out.pattern = p.name;
		out.speed = m.speed;
		out.ticks = m.ticks;

		if (p.slot != DIR_NONE) {
			for (int i = 0; i < p.taps; i++) {
				Enqueue(BTN_FLIP, p.slot, AfterPop(cfg));
			}
		}
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
				if (Since(now, p.notBefore) >= 0) {
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

		const uint32_t now = in.now;
		const float dead = (float)cfg.deadzone * 0.01f;
		const float engage = (float)cfg.engage * 0.01f;
		const float release = (float)cfg.release * 0.01f;
		const float flick = (float)cfg.flick * 0.01f;
		const float snap = (float)cfg.snap * 0.01f;

		float mag = std::sqrt(in.rx * in.rx + in.ry * in.ry);
		if (mag > 1.0f) {
			mag = 1.0f;
		}
		const bool hasAngle = mag > 0.001f;
		const float angle = hasAngle ? AngleDeg(in.rx, in.ry) : 0.0f;
		const int sector = hasAngle ? Sector(in.rx, in.ry) : DIR_NONE;

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
			// KickflipSide mirrors the stick, so every pattern is mirrored with it
			const float px = cfg.kickflipSide ? -in.rx : in.rx;
			for (int i = 0; i < ticks; i++) {
				PatternMatch m;
				if (m_matcher.Tick(*m_patterns, px, -in.ry, cfg.missTicks, cfg.nollie != 0, m)) {
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

		if (m_phase == PH_IDLE && m_armed && mag >= engage) {
			if (InWindupHalf(angle, WIND_OLLIE, 0.0f)) {
				BeginWindup(WIND_OLLIE, sector, now, cfg);
			}
			else if (cfg.nollie && InWindupHalf(angle, WIND_NOLLIE, 0.0f)) {
				BeginWindup(WIND_NOLLIE, sector, now, cfg);
			}
		}

		if (m_phase == PH_WINDUP) {
			const bool inHalf = hasAngle && InWindupHalf(angle, m_wind, kHalfMargin);
			if (mag < release || !inHalf) {
				Pop(now, cfg);
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

		if (usePatterns) {
			if (matched && (m_phase == PH_WINDUP || m_phase == PH_TRACK)) {
				const Pattern& p = m_patterns->At(match.index);
				if ((p.nollie != 0) == (m_wind == WIND_NOLLIE)) {
					if (m_phase == PH_WINDUP) {
						// The pattern ended inside the wind-up area: this is the pop
						Pop(now, cfg);
					}
					FinalizePattern(p, match, now, cfg, out);
				}
			}
			if (m_phase == PH_TRACK) {
				if (hasAngle && mag >= engage && InWindupHalf(angle, m_wind, 0.0f)) {
					// Pulled straight back into the wind-up: that pop had no flick
					const int wind = m_wind;
					Finalize(DIR_NONE, now, cfg, out);
					BeginWindup(wind, sector, now, cfg);
				}
				else if (Since(now, m_popTime) >= cfg.flickWindow) {
					// No pattern was completed in time: a plain ollie
					Finalize(DIR_NONE, now, cfg, out);
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
					Finalize(m_cand, now, cfg, out);
				}
				else if (Since(now, m_candSince) >= cfg.settle) {
					// A drag that came to rest
					Finalize(m_cand, now, cfg, out);
				}
			}
			else if (m_cand != DIR_NONE && mag < release) {
				// The stick sprang back
				Finalize(m_cand, now, cfg, out);
			}
			else if (hasAngle && mag >= engage && InWindupHalf(angle, m_wind, 0.0f)) {
				// Pulled straight back into the wind-up: that pop had no flick
				const int wind = m_wind;
				Finalize(DIR_NONE, now, cfg, out);
				BeginWindup(wind, sector, now, cfg);
			}

			if (m_phase == PH_TRACK && Since(now, m_popTime) >= cfg.flickWindow) {
				Finalize(m_cand, now, cfg, out);
			}
		}

		out.ollie = (m_phase == PH_WINDUP);

		if (m_phase == PH_WINDUP) {
			RunTaps(now, out);
		}

		if (cfg.triggerGrabs) {
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
		m_prevLeftTrigger = in.leftTrigger;
		m_prevRightTrigger = in.rightTrigger;

		RunQueue(in, cfg, out);

		if (m_grindPending) {
			if (Since(now, m_grindUntil) >= 0) {
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
			out.ollie = m_lastOllie;
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
	}
}
