#include "flickit.h"

#include <cmath>

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

	Recognizer::Recognizer() {
		Reset();
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

		if (m_phase == PH_TRACK) {
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
