#pragma once
// The crouch shown as deep as the stick is pulled.
//
// THUG shows a crouch the way it shows any change of animation. The crouch animation starts,
// and the animation the skater stood in is blended out over a moment, its share of the pose
// going down to nothing. Each animation is a blend channel of the skater's animation component,
// in a list that is newest first: the first channel plays, the ones after it blend out.
//
// The Keeper keeps the channel the skater stood in when a crouch of the stick's begins, for as
// long as the crouch lasts, and gives it the share of the pose that the stick asks for: all of
// it with the stick only just pulled, none of it pulled all the way. The crouch animations
// that the game plays share the rest the way the game would share all of it without that
// channel. When the crouch ends without a pop, its share goes, over the time the game blends in
// (0.3 s), once the game has started the animation he stands in. A pop starts its animation
// without a blend, and the game drops every channel itself.
//
// What it relies on, read from THUG-SDL.exe:
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
	};

	class Keeper {
	public:
		Keeper() { Forget(); m_lastAnim = 0; m_last = 0; m_lastName = 0; m_lostSeen = false; m_saved = 0; m_blendSaved = false; }

		// Once a frame, for the first player's skater: his animation component (0 for none),
		// whether he is on his board on the ground, whether he crouches with the stick and how
		// deep (0, only just, to 1, all the way), and the game's clock
		void Frame(uint8_t* anim, bool ground, bool crouch, float depth, uint32_t now, Report& report) {
			report.started = false;
			report.from = 0;
			report.lost = m_lostSeen;
			m_lostSeen = false;
			uint8_t* channels[kMostChannels];
			const int n = anim ? Channels(anim, channels) : 0;
			if (m_channel && (anim != m_anim || Index(channels, n, m_channel) < 1 || !Still(m_channel))) {
				report.lost = report.lost || !m_fading;
				Forget();
			}
			uint8_t* const head = n > 0 ? channels[0] : 0;
			// The animation he stands in: the one to come back to from a crouch
			if (head && ground && StateOf(head) == CHANNEL_PLAYING && !IsCrouchAnim(NameOf(head))) {
				m_lastAnim = anim;
				m_last = head;
				m_lastName = NameOf(head);
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
			m_share = share > kLeastShare ? share : kLeastShare;
			Hold();
			KeepSecond(anim, channels, n);
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
		float Share() const { return m_share; }

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

		uint8_t* m_lastAnim;        // the channel playing when he last stood on the ground
		uint8_t* m_last;
		uint32_t m_lastName;
		bool m_lostSeen;            // the channel was dropped between two frames

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
