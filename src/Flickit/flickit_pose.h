#pragma once
// The crouch shown as deep as the stick is pulled.
//
// An animation component plays each animation in a blend channel, in a list that is newest
// first: the first channel plays, the ones after it blend out, their share of the pose going
// down to nothing over the blend's time. THUG blends that way for the people in its levels and
// the trick editor's skater, which switch it on (Obj_EnableAnimBlending). The player's skater
// never does: every animation he starts drops the one before, and the crouch animation takes
// over at once.
//
// While the stick crouches, the Keeper switches blending on for the skater, so that the
// channel he stood in is still there when the game starts its crouch animation. It keeps that
// channel for as long as the crouch lasts and gives it the share of the pose that the stick asks
// for: all of it with the stick only just pulled, none of it pulled all the way. The crouch
// animations that the game plays share the rest the way the game would share all of it without
// that channel. When the crouch ends without a pop, its share goes over 0.3 s (the game's blend
// time) once the game has started the animation he stands in, and blending is switched back
// to what it was. A pop starts its animation without a blend, and the game drops every channel.
// Not every jump does: THUG's nollie starts its animation with a short blend. So a pop lets go of
// the kept channel at once and switches blending back (Pop), before the game starts the jump: as
// the player's skater never blends, the jump's animation then takes over at once, the nollie's too.
//
// Some crouches start with nothing standing to blend with: a short drop lands in THUG's crouch
// bump, and the crouch idle follows it. Then the game side starts the idle he stands in under the
// crouch (WantsStanding, Adopt): the crouch animation plays on as it was, and the standing pose
// takes its share over a fifth of a second.
//
// What it relies on, read from THUG-SDL.exe:
// - the switch: a byte at +0x34 of the component, 0 when it is made; PlayAnim drops every
//   channel first when it is 0 or the blend has no time (0x00432281)
// - the list: the component's own node at +0x18 (key -1), each node's next at +0x0C and the one
//   before at +0x10; +0x24 (the list's next) is the channel playing
// - a channel's state at +0x34: 2 plays, 1 blends out, 0 is over (the next update takes it out).
//   While it blends out it weighs LEFT x SCALE (0x00470B50); every frame LEFT goes down by RATE,
//   and at no LEFT or a weight under 0 the channel is over (0x00470A80)
// - a blend starts (0x00430F20): every channel after the second is taken out, the first starts to
//   blend out over the blend's time, weighing 1 less what those after it still weigh, and a new
//   channel goes in front
// - the pose (0x00431B60, 0x00430680) counts the channels that blend out and do weigh something,
//   gives the playing one that count less their weights, and divides every weight by the count
//
// Pure: it reads and writes the channels through the layout below, and knows nothing else of the
// game, so that it can be tried on a list built in a test.
#include <stdint.h>

namespace FlickitPose {
	enum Layout {
		ANIM_BLENDING = 0x34,           // the animation component's switch: not 0, its animations blend
		ANIM_CHANNELS = 0x18,           // the list's own node in the animation component
		NODE_KEY = 0x04,                // -1 in the list's own node
		NODE_NEXT = 0x0C,
		NODE_PREV = 0x10,
		CHANNEL_NAME = 0x30,            // the checksum of the animation's name
		CHANNEL_STATE = 0x34,           // CHANNEL_PLAYING, CHANNEL_LEAVING or CHANNEL_DONE
		CHANNEL_RATE = 0x4C,            // how much of LEFT goes in a 60th of a second
		CHANNEL_LEFT = 0x50,            // what is left of its blend
		CHANNEL_SCALE = 0x54,           // while it blends out it weighs LEFT x SCALE
		CHANNEL_SIZE = 0x74,
	};
	enum ChannelState { CHANNEL_DONE = 0, CHANNEL_LEAVING = 1, CHANNEL_PLAYING = 2 };

	// No list of the game's is longer than three; a list is never walked further than this
	static const int kMostChannels = 8;
	// The least share the kept channel has, with the stick all the way down: it stays in the list,
	// to take back its share if the stick eases off, and is not seen
	static const float kLeastShare = 0.0001f;
	// The share of a crouch that ended without a pop goes over this long, as the game blends
	static const uint32_t kFadeMs = 300;
	// and starts to go once the game plays the animation he stands up in, or after this long
	static const uint32_t kFadeWaitMs = 200;

	struct Named {
		uint32_t crc;
		const char* name;
	};

	// The animations the game's loops on the ground play while the skater crouches (tricks.q:
	// DoCrouch_slope, the crouched turns, the crouch in the nollie loop)
	static const Named kCrouchAnims[] = {
		{ 0x9529A543, "CrouchIdle" },
		{ 0x2797680F, "CrouchIdle2" },
		{ 0xCEF4CD3A, "CrouchIdle4" },
		{ 0xDCC9490E, "CrouchLookDown" },
		{ 0xBA513E5D, "CrouchLookLeft" },
		{ 0x30BB6863, "CrouchLookRight" },
		{ 0x0038E8B7, "CrouchShiftFoot" },
		{ 0xDEF1154C, "CrouchIdleAdjustFeet" },
		{ 0xA1094976, "CrouchTurnLeft" },
		{ 0x71108CC0, "CrouchTurnLeft2" },
		{ 0x9C1CC954, "CrouchTurnRight" },
		{ 0xA44DD8A4, "CrouchTurnRight2" },
		{ 0xBC6C0B31, "CrouchTurnLeftIdle" },
		{ 0x99B53DB1, "CrouchTurnLeftIdle2" },
		{ 0x38BFDD01, "CrouchTurnRightIdle" },
		{ 0xBFE8DECB, "CrouchTurnRightIdle2" },
		{ 0x991F66BC, "CrouchPush1" },
		{ 0xEAB36A08, "CrouchBumpUp" },
		{ 0x50159F24, "CrouchBumpDown" },
		{ 0x33746A70, "NollieCrouchIdle" },
		{ 0xE9C4EA80, "NollieCrouchTurnLeft" },
		{ 0xC88A53DE, "NollieCrouchTurnRight" },
	};
	static const int CROUCH_ANIMS = (int)(sizeof(kCrouchAnims) / sizeof(kCrouchAnims[0]));

	// Animations he often stands in before a crouch, for the log only
	static const Named kStandAnims[] = {
		{ 0x23DB4AEA, "Idle" },
		{ 0x375C1DE2, "TurnLeft" },
		{ 0x6BE8CB5D, "TurnRight" },
		{ 0x3B5B64CE, "TurnLeftIdle" },
		{ 0x153A05E3, "TurnRightIdle" },
		{ 0x4266D67F, "PushCycle" },
		{ 0xFF10285F, "MongoPushCycle" },
		{ 0x16751E0B, "MongoPush" },
		{ 0xBC825547, "CrouchedLand" },
		{ 0xB9DA565C, "CrouchedLand1" },
		{ 0x20D307E6, "CrouchedLand2" },
		{ 0x57D43770, "CrouchedLand3" },
		{ 0x490A6675, "StretchLegsLand" },
		{ 0xC72E8DD2, "SketchyLand" },
		{ 0x39C49D4F, "SketchyLand1" },
		{ 0x05AE68AE, "SketchyCrouchLand" },
		{ 0x57FF2A27, "Land" },
		{ 0xFC69157A, "NollieSkatingIdle" },
		{ 0x2D6396B4, "NollieSkatingTurnLeft" },
		{ 0xE9FA0017, "NollieSkatingTurnRight" },
		{ 0xCE215C07, "SlowDownBrake" },
		{ 0x299EE86B, "SlowDownBrake_Mongo" },
		{ 0x6656FE62, "BrakeIdle" },
		{ 0xDB200042, "MongoBrakeIdle" },
		{ 0x47273509, "IdleBump" },
		{ 0xC51801BC, "CessSlide180_FS" },
		{ 0xA174C4B8, "CessSlide180_BS" },
		{ 0x860F4FD8, "PushCycleArms1" },
		{ 0x1F061E62, "PushCycleArms2" },
		{ 0x68012EF4, "PushCycleArms3" },
		{ 0xF665BB57, "PushCycleArms4" },
		{ 0x81628BC1, "PushCycleArms5" },
		{ 0x4C99321E, "SkateIdle" },
	};
	static const int STAND_ANIMS = (int)(sizeof(kStandAnims) / sizeof(kStandAnims[0]));

	inline bool IsCrouchAnim(uint32_t name) {
		for (int i = 0; i < CROUCH_ANIMS; i++) {
			if (kCrouchAnims[i].crc == name) {
				return true;
			}
		}
		return false;
	}

	// The name of an animation, if it is one of those above; 0 otherwise
	inline const char* AnimName(uint32_t name) {
		for (int i = 0; i < CROUCH_ANIMS; i++) {
			if (kCrouchAnims[i].crc == name) {
				return kCrouchAnims[i].name;
			}
		}
		for (int i = 0; i < STAND_ANIMS; i++) {
			if (kStandAnims[i].crc == name) {
				return kStandAnims[i].name;
			}
		}
		return 0;
	}

	// The game is 32-bit: a link in its list is four bytes. (A test built for 64 bits keeps its
	// lists in memory below 4 GB.)
	inline uint8_t* LinkAt(const uint8_t* node, int at) { return (uint8_t*)(uintptr_t)*(const uint32_t*)(node + at); }
	inline void SetLink(uint8_t* node, int at, const uint8_t* to) { *(uint32_t*)(node + at) = (uint32_t)(uintptr_t)to; }
	inline uint8_t* NextOf(const uint8_t* node) { return LinkAt(node, NODE_NEXT); }
	inline uint8_t* PrevOf(const uint8_t* node) { return LinkAt(node, NODE_PREV); }
	inline void SetNext(uint8_t* node, const uint8_t* to) { SetLink(node, NODE_NEXT, to); }
	inline void SetPrev(uint8_t* node, const uint8_t* to) { SetLink(node, NODE_PREV, to); }
	inline int32_t& StateOf(uint8_t* channel) { return *(int32_t*)(channel + CHANNEL_STATE); }
	inline float& LeftOf(uint8_t* channel) { return *(float*)(channel + CHANNEL_LEFT); }
	inline float& ScaleOf(uint8_t* channel) { return *(float*)(channel + CHANNEL_SCALE); }
	inline float& RateOf(uint8_t* channel) { return *(float*)(channel + CHANNEL_RATE); }
	inline uint32_t NameOf(const uint8_t* channel) { return *(const uint32_t*)(channel + CHANNEL_NAME); }

	// The channels of an animation component, newest (the one playing) first. Returns how many.
	inline int Channels(uint8_t* anim, uint8_t** out) {
		uint8_t* list = anim + ANIM_CHANNELS;
		int n = 0;
		for (uint8_t* node = NextOf(list); node && node != list && *(const int32_t*)(node + NODE_KEY) != -1 && n < kMostChannels; node = NextOf(node)) {
			out[n++] = node;
		}
		return n;
	}

	// What a channel weighs in the pose before the pose shares it out, as the game has it
	inline float WeightOf(uint8_t* channel) {
		switch (StateOf(channel)) {
		case CHANNEL_PLAYING: return 1.0f;
		case CHANNEL_LEAVING: return LeftOf(channel) * ScaleOf(channel);
		default: return 0.0f;
		}
	}

	// What the Keeper did on a frame, for the log
	struct Report {
		bool started;           // a crouch has begun to follow the stick,
		uint32_t from;          // with the skater's pose from this animation
		bool lost;              // the game dropped the channel kept for the crouch
		bool missed;            // a crouch of the stick's that has found nothing to keep (once a crouch):
		uint32_t missAnim;      // the animation he is in,
		int missChannels;       // how many channels there are,
		uint32_t missLast;      // and the animation he last stood in (0: none seen)
	};

	// A crouch that has looked for the channel he stood in this long without finding it is reported
	static const uint32_t kMissMs = 300;
	// A standing pose started under a crouch takes its share at this pace (all of it in this long)
	static const uint32_t kAdoptMs = 200;

	// A channel's own fields, as the channel's Set (0x00470440) writes them
	enum ChannelTimes {
		CHANNEL_START = 0x14,           // where its animation starts (the lesser of start and end),
		CHANNEL_TIME = 0x18,            // where it is,
		CHANNEL_END = 0x1C,             // where it ends,
		CHANNEL_SPEED = 0x20,           // how fast, per 60th of a second,
		CHANNEL_BACKWARDS = 0x24,       // not 0: from the end to the start,
		CHANNEL_LOOP = 0x28,            // 0 once, 1 Cycle, 2 PingPong, 3 wobble
	};

	class Keeper {
	public:
		Keeper() {
			Forget();
			m_lastAnim = 0;
			m_last = 0;
			m_lastName = 0;
			m_lostSeen = false;
			m_saved = 0;
			m_blendSaved = false;
			m_switchAnim = 0;
			m_switchWas = 0;
			m_looking = false;
			m_lookingSince = 0;
			m_missTold = false;
			m_wantsStanding = false;
			m_lookingAnim = 0;
			m_standingTried = false;
			m_seen = 0;
			m_seenName = 0;
			m_seenOnGround = false;
		}

		// Once a frame, for the first player's skater: his animation component (0 for none),
		// whether he is on his board on the ground, whether the stick crouches (Flickit's crouch,
		// on the ground), whether the game has him crouched, how deep the stick's crouch is (0,
		// only just, to 1, all the way), and the game's clock
		void Frame(uint8_t* anim, bool ground, bool stick, bool crouched, float depth, uint32_t now, Report& report) {
			m_wantsStanding = false;
			report.started = false;
			report.from = 0;
			report.lost = m_lostSeen;
			report.missed = false;
			report.missAnim = 0;
			report.missChannels = 0;
			report.missLast = 0;
			m_lostSeen = false;
			uint8_t* channels[kMostChannels];
			const int n = anim ? Channels(anim, channels) : 0;
			if (m_channel && (anim != m_anim || Index(channels, n, m_channel) < 1 || !Still(m_channel))) {
				report.lost = report.lost || !m_fading;
				Forget();
			}
			uint8_t* const head = n > 0 ? channels[0] : 0;
			// Where the animation playing was started: one of the air, still playing on the frame he
			// lands, is not one he stands in
			if (head != m_seen || (head && NameOf(head) != m_seenName)) {
				m_seen = head;
				m_seenName = head ? NameOf(head) : 0;
				m_seenOnGround = ground;
			}
			// The animation he stands in: the one to come back to from a crouch
			if (head && ground && m_seenOnGround && StateOf(head) == CHANNEL_PLAYING && !IsCrouchAnim(NameOf(head))) {
				m_lastAnim = anim;
				m_last = head;
				m_lastName = NameOf(head);
			}
			// His animations blend while the stick crouches and while a kept channel's share goes:
			// the crouch the game starts finds the channel he stood in there, and so does his
			// standing up
			if (stick || m_channel) {
				SwitchOn(anim);
			}
			else {
				SwitchBack(anim);
			}
			const bool crouch = stick && crouched;
			if (!crouch || m_channel) {
				m_looking = false;
				m_missTold = false;
			}
			if (!crouch) {
				m_standingTried = false;
			}
			if (!crouch) {
				if (m_channel && !m_fading) {
					m_fading = true;
					m_fadeFrom = m_share;
					m_fadeAsked = now;
					m_fadeAt = 0;
					m_fadeStarted = false;
				}
				if (m_channel) {
					Fade(head, now);
				}
				return;
			}
			if (!m_channel) {
				// The crouch has begun: the channel he stood in blends out behind the crouch's
				const int at = (m_last && m_lastAnim == anim) ? Index(channels, n, m_last) : -1;
				if (at < 1 || StateOf(m_last) != CHANNEL_LEAVING || NameOf(m_last) != m_lastName) {
					// The game plays its crouch and nothing he stood in is left: a standing pose can
					// be started under it (once a crouch)
					m_wantsStanding = head && IsCrouchAnim(NameOf(head)) && !m_standingTried;
					m_lookingAnim = anim;
					if (!m_looking) {
						m_looking = true;
						m_lookingSince = now;
					}
					else if (!m_missTold && (uint32_t)(now - m_lookingSince) >= kMissMs) {
						m_missTold = true;
						report.missed = true;
						report.missAnim = head ? NameOf(head) : 0;
						report.missChannels = n;
						report.missLast = (m_last && m_lastAnim == anim) ? m_lastName : 0;
					}
					return;
				}
				m_anim = anim;
				m_channel = m_last;
				m_name = m_lastName;
				report.started = true;
				report.from = m_name;
			}
			m_fading = false;
			const float d = depth > 0.0f ? (depth < 1.0f ? depth : 1.0f) : 0.0f;
			const float share = 1.0f - d;
			float want = share > kLeastShare ? share : kLeastShare;
			if (m_adopting) {
				// A standing pose started under the crouch comes in over a moment
				const float most = m_adoptFrom + (float)(uint32_t)(now - m_adoptAt) / (float)kAdoptMs;
				if (want > most) {
					want = most;
				}
				else {
					m_adopting = false;
				}
			}
			m_share = want;
			Hold();
			KeepSecond(anim, channels, n);
		}

		// Whether the last frame's crouch found nothing standing to blend with while the game plays
		// its crouch, so that a standing pose is to be started under it (see Adopt)
		bool WantsStanding(const uint8_t* anim) const { return m_wantsStanding && anim == m_lookingAnim; }

		// The game side has started a standing animation under the crouch: the channel it is in
		// (second in the list, blending out) is the one to keep from now. Its share comes in from
		// nothing at kAdoptMs. Returns false if that channel is not one to keep.
		bool Adopt(uint8_t* anim, uint8_t* channel, uint32_t now) {
			m_standingTried = true;
			m_wantsStanding = false;
			if (m_channel || !anim || !channel) {
				return false;
			}
			uint8_t* channels[kMostChannels];
			const int n = Channels(anim, channels);
			if (Index(channels, n, channel) < 1 || StateOf(channel) != CHANNEL_LEAVING) {
				return false;
			}
			m_anim = anim;
			m_channel = channel;
			m_name = NameOf(channel);
			m_fading = false;
			m_adopting = true;
			m_adoptFrom = kLeastShare;
			m_adoptAt = now;
			m_share = kLeastShare;
			m_looking = false;
			Hold();
			KeepSecond(anim, channels, n);
			return true;
		}

		// The stick's crouch ended in a pop on this frame, before the game starts the jump: the kept
		// channel goes now, and blending is switched back to what it was, so that the jump's
		// animation takes over at once, with a blend or without (the game's nollie has one)
		void Pop(uint8_t* anim) {
			if (m_channel && anim == m_anim) {
				uint8_t* channels[kMostChannels];
				const int n = Channels(anim, channels);
				if (Index(channels, n, m_channel) >= 1 && Still(m_channel)) {
					StateOf(m_channel) = CHANNEL_DONE;
				}
			}
			if (m_channel) {
				Forget();
			}
			m_wantsStanding = false;
			SwitchBack(anim);
		}

		// The game is about to start a blend in the channels of "anim". The kept channel is put
		// second, so that the start of the blend keeps it, and weighs nothing while the blend
		// starts, so that the channel that begins to blend out weighs what it would without it.
		void BeforeBlend(uint8_t* anim) {
			m_blendSaved = false;
			if (!m_channel || anim != m_anim) {
				return;
			}
			uint8_t* channels[kMostChannels];
			const int n = Channels(anim, channels);
			if (Index(channels, n, m_channel) < 1 || !Still(m_channel)) {
				m_lostSeen = m_lostSeen || !m_fading;
				Forget();
				return;
			}
			KeepSecond(anim, channels, n);
			m_blendScale = ScaleOf(m_channel);
			ScaleOf(m_channel) = 0.0f;
			m_blendSaved = true;
		}

		void AfterBlend(uint8_t* anim) {
			if (m_blendSaved && m_channel && anim == m_anim) {
				ScaleOf(m_channel) = m_blendScale;
			}
			m_blendSaved = false;
		}

		// The game is about to blend the channels of "anim" into the pose. The weights are set for
		// the shares the pose is to have, and put back by AfterPose.
		void BeforePose(uint8_t* anim, uint32_t now) {
			m_saved = 0;
			if (!m_channel || anim != m_anim) {
				return;
			}
			uint8_t* channels[kMostChannels];
			const int n = Channels(anim, channels);
			if (Index(channels, n, m_channel) < 1 || !Still(m_channel)) {
				m_lostSeen = m_lostSeen || !m_fading;
				Forget();
				return;
			}
			float share = m_share;
			if (m_fading) {
				FadeStart(channels[0], now);
				share = FadeShare(now);
				if (!(share > 0.0f)) {
					// Its share is gone: the game takes it out on its next update
					StateOf(m_channel) = CHANNEL_DONE;
					Forget();
					return;
				}
			}
			// The other channels blending out, and how many of them the game would count without
			// this one: its pose gives each the weight over that count, and the playing one the rest
			float want[kMostChannels];
			int counted = 0;
			for (int i = 1; i < n; i++) {
				want[i] = channels[i] == m_channel ? 0.0f : WeightOf(channels[i]);
				if (want[i] != 0.0f) {
					counted++;
				}
			}
			// The kept one is to have "share" of the pose, the others the rest of it, shared out as
			// the game would share all of it. The game divides each weight by the number of channels
			// after the first that weigh something, so each weight is that number times the share.
			int weighing = 1;
			for (int i = 1; i < n; i++) {
				if (channels[i] == m_channel) {
					want[i] = share;
				}
				else {
					const float w = counted > 0 ? (1.0f - share) * want[i] / (float)counted : 0.0f;
					want[i] = w > 0.0f ? w : 0.0f;
					if (want[i] > 0.0f) {
						weighing++;
					}
				}
			}
			for (int i = 1; i < n && m_saved < kMostChannels; i++) {
				uint8_t* const c = channels[i];
				if (StateOf(c) != CHANNEL_LEAVING || !(LeftOf(c) > 0.0f)) {
					continue;
				}
				m_savedChannel[m_saved] = c;
				m_savedScale[m_saved] = ScaleOf(c);
				m_saved++;
				ScaleOf(c) = (float)weighing * want[i] / LeftOf(c);
			}
		}

		void AfterPose(uint8_t* anim) {
			if (anim == m_anim) {
				for (int i = 0; i < m_saved; i++) {
					ScaleOf(m_savedChannel[i]) = m_savedScale[i];
				}
			}
			m_saved = 0;
		}

		bool Holding() const { return m_channel != 0 && !m_fading; }
		bool Fading() const { return m_channel != 0 && m_fading; }
		uint8_t* Kept() const { return m_channel; }

	private:
		uint8_t* m_anim;            // the animation component of the channel kept
		uint8_t* m_channel;         // the channel the skater stood in, kept for the crouch
		uint32_t m_name;            // its animation
		float m_share;              // its share of the pose
		bool m_fading;              // the crouch is over: its share is going
		float m_fadeFrom;
		uint32_t m_fadeAsked;       // when the crouch ended
		uint32_t m_fadeAt;          // when its share began to go
		bool m_fadeStarted;

		const uint8_t* m_seen;      // the channel playing on the last frame, its animation,
		uint32_t m_seenName;
		bool m_seenOnGround;        // and whether it was first seen with him on the ground

		uint8_t* m_lastAnim;        // the channel playing when he last stood on the ground
		uint8_t* m_last;
		uint32_t m_lastName;
		bool m_lostSeen;            // the channel was dropped between two frames

		bool m_adopting;            // the kept channel was started under the crouch: its share comes in,
		float m_adoptFrom;          // from this,
		uint32_t m_adoptAt;         // at this time
		bool m_wantsStanding;       // the crouch found nothing standing to blend with,
		uint8_t* m_lookingAnim;     // in this component,
		bool m_standingTried;       // and a standing pose has been tried for this crouch

		uint8_t* m_switchAnim;      // the component whose blending the Keeper switched on,
		uint8_t m_switchWas;        // and what the switch was before
		bool m_looking;             // a crouch is looking for the channel he stood in,
		uint32_t m_lookingSince;    // since then,
		bool m_missTold;            // and the log has heard that it found none

		uint8_t* m_savedChannel[kMostChannels];
		float m_savedScale[kMostChannels];
		int m_saved;
		float m_blendScale;
		bool m_blendSaved;

		void Forget() {
			m_anim = 0;
			m_channel = 0;
			m_name = 0;
			m_share = 1.0f;
			m_adopting = false;
			m_adoptFrom = 0.0f;
			m_adoptAt = 0;
			m_fading = false;
			m_fadeFrom = 0.0f;
			m_fadeAsked = 0;
			m_fadeAt = 0;
			m_fadeStarted = false;
		}

		static int Index(uint8_t* const* channels, int n, const uint8_t* channel) {
			for (int i = 0; i < n; i++) {
				if (channels[i] == channel) {
					return i;
				}
			}
			return -1;
		}

		// Still the channel that was kept: blending out, with the same animation
		bool Still(uint8_t* channel) const {
			return StateOf(channel) == CHANNEL_LEAVING && NameOf(channel) == m_name;
		}

		// Its own blend is held where it is: it does not run out while it is kept. Should the
		// Keeper not be called for a while, it runs out at the game's pace, in about a second.
		void Hold() {
			LeftOf(m_channel) = 1.0f;
			ScaleOf(m_channel) = m_share;
		}

		// The share starts to go once the game plays an animation he stands in, or a moment after
		// the crouch is over at the latest
		void FadeStart(const uint8_t* head, uint32_t now) {
			if (!m_fadeStarted && ((head && !IsCrouchAnim(NameOf(head))) || (uint32_t)(now - m_fadeAsked) >= kFadeWaitMs)) {
				m_fadeStarted = true;
				m_fadeAt = now;
			}
		}

		void Fade(uint8_t* head, uint32_t now) {
			FadeStart(head, now);
			const float share = FadeShare(now);
			if (!(share > 0.0f)) {
				StateOf(m_channel) = CHANNEL_DONE;
				Forget();
				return;
			}
			m_share = share;
			Hold();
		}

		float FadeShare(uint32_t now) const {
			if (!m_fadeStarted) {
				return m_fadeFrom;
			}
			const uint32_t gone = (uint32_t)(now - m_fadeAt);
			if (gone >= kFadeMs) {
				return 0.0f;
			}
			return m_fadeFrom * (1.0f - (float)gone / (float)kFadeMs);
		}

		// Blending on for the skater's animations, remembering what it was
		void SwitchOn(uint8_t* anim) {
			if (m_switchAnim && m_switchAnim != anim) {
				m_switchAnim = 0;           // another component now: the old one is not touched
			}
			if (!anim || m_switchAnim == anim) {
				return;
			}
			m_switchAnim = anim;
			m_switchWas = anim[ANIM_BLENDING];
			anim[ANIM_BLENDING] = 1;
		}

		// and back to what it was
		void SwitchBack(uint8_t* anim) {
			if (m_switchAnim && m_switchAnim == anim) {
				anim[ANIM_BLENDING] = m_switchWas;
			}
			m_switchAnim = 0;
		}

		// The kept channel second in the list, right after the one playing
		void KeepSecond(uint8_t* anim, uint8_t* const* channels, int n) {
			(void)anim;
			if (n < 2 || channels[1] == m_channel || channels[0] == m_channel) {
				return;
			}
			uint8_t* const c = m_channel;
			SetNext(PrevOf(c), NextOf(c));
			SetPrev(NextOf(c), PrevOf(c));
			uint8_t* const head = channels[0];
			SetNext(c, NextOf(head));
			SetPrev(c, head);
			SetPrev(NextOf(head), c);
			SetNext(head, c);
		}
	};
}
