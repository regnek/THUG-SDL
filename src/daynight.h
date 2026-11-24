#pragma once
// Flickit's day and night: the parts without the game (daynight_game.cpp puts them in the game).
//
// A day goes by in DayMinutes (24 by default: a game hour a minute, as GTA has it). How the level looks at each
// hour comes from a few keys (night, dawn, day, dusk, night again); between two keys it eases from one to the
// other. A look is what the game's own time of day sets (timeofday.q, which the story uses to turn New Jersey
// from day to night): the colour the level's geometry and its sky are drawn with (SetSceneColor, 128 for as it
// is), and the lights the skater, the cars and the people are lit by (set_level_lights: an ambient colour and
// two lights from a direction, each with how much it counts). The level's own lights for the night and for the
// evening (the game's FakeLights) and the cars' headlights switch at set hours.
//
// Pure, tested by tests/daynight_test.cpp.
#include <math.h>
#include <stdint.h>

namespace DayNight {
	struct Light {
		float rgb[3];
		float mod;              // how much it counts (the game's mod_factor)
	};

	struct Look {
		float level[3];         // the geometry's colour, 128 as it is
		float sky[3];
		Light ambient;
		Light dir[2];
	};

	struct Key {
		float hour;
		Look look;
	};

	// The directions of the two lights: New Jersey's (the joined world's own), all day
	static const float kHeading[2] = { 50.0f, 240.0f };
	static const float kPitch[2] = { 330.0f, 330.0f };

	inline const Key* Keys(int* count) {
		static const Key k[] = {
			// night: the city's geometry a dim blue, the sky near black, cold light
			{ 0.0f, { { 48, 60, 78 }, { 18, 22, 32 }, { { 30, 38, 48 }, 0.3f }, { { { 70, 95, 110 }, 1.0f }, { { 15, 28, 32 }, 0.3f } } } },
			{ 5.0f, { { 48, 60, 78 }, { 18, 22, 32 }, { { 30, 38, 48 }, 0.3f }, { { { 70, 95, 110 }, 1.0f }, { { 15, 28, 32 }, 0.3f } } } },
			// dawn: cool and pink
			{ 6.5f, { { 96, 88, 104 }, { 78, 70, 88 }, { { 52, 52, 58 }, 0.3f }, { { { 120, 96, 92 }, 1.0f }, { { 44, 46, 60 }, 0.4f } } } },
			// day: the level as it is, lit as New Jersey is (Levels.q)
			{ 8.0f, { { 128, 128, 128 }, { 128, 128, 128 }, { { 69, 72, 79 }, 0.3f }, { { { 130, 120, 97 }, 1.4f }, { { 58, 62, 66 }, 0.4f } } } },
			{ 17.0f, { { 128, 128, 128 }, { 128, 128, 128 }, { { 69, 72, 79 }, 0.3f }, { { { 130, 120, 97 }, 1.4f }, { { 58, 62, 66 }, 0.4f } } } },
			// dusk: warm, a low orange sun
			{ 18.75f, { { 118, 92, 70 }, { 96, 70, 50 }, { { 62, 54, 46 }, 0.4f }, { { { 135, 90, 50 }, 1.1f }, { { 48, 42, 40 }, 0.4f } } } },
			{ 20.25f, { { 48, 60, 78 }, { 18, 22, 32 }, { { 30, 38, 48 }, 0.3f }, { { { 70, 95, 110 }, 1.0f }, { { 15, 28, 32 }, 0.3f } } } },
			{ 24.0f, { { 48, 60, 78 }, { 18, 22, 32 }, { { 30, 38, 48 }, 0.3f }, { { { 70, 95, 110 }, 1.0f }, { { 15, 28, 32 }, 0.3f } } } },
		};
		*count = (int)(sizeof(k) / sizeof(k[0]));
		return k;
	}

	inline float Wrap(float hour) {
		if (!(hour > -1.0e6f && hour < 1.0e6f)) {
			return 0.0f;
		}
		hour = fmodf(hour, 24.0f);
		return hour < 0.0f ? hour + 24.0f : hour;
	}

	inline float Ease(float t) {
		t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
		return t * t * (3.0f - 2.0f * t);
	}

	inline float Mix(float a, float b, float t) {
		return a + (b - a) * t;
	}

	inline void MixLight(const Light& a, const Light& b, float t, Light* out) {
		for (int i = 0; i < 3; i++) {
			out->rgb[i] = Mix(a.rgb[i], b.rgb[i], t);
		}
		out->mod = Mix(a.mod, b.mod, t);
	}

	inline void MixLook(const Look& a, const Look& b, float t, Look* out) {
		for (int i = 0; i < 3; i++) {
			out->level[i] = Mix(a.level[i], b.level[i], t);
			out->sky[i] = Mix(a.sky[i], b.sky[i], t);
		}
		MixLight(a.ambient, b.ambient, t, &out->ambient);
		MixLight(a.dir[0], b.dir[0], t, &out->dir[0]);
		MixLight(a.dir[1], b.dir[1], t, &out->dir[1]);
	}

	// How the level looks at this hour
	inline Look LookAt(float hour) {
		hour = Wrap(hour);
		int n = 0;
		const Key* k = Keys(&n);
		for (int i = 0; i + 1 < n; i++) {
			if (hour >= k[i].hour && hour <= k[i + 1].hour) {
				const float span = k[i + 1].hour - k[i].hour;
				Look out;
				MixLook(k[i].look, k[i + 1].look, span > 0.0f ? Ease((hour - k[i].hour) / span) : 0.0f, &out);
				return out;
			}
		}
		return k[0].look;
	}

	// The level's lights for the night (TRG_levellight...) and for the evening (TRG_Evening_levellight...), and
	// the cars' headlights
	inline bool NightLights(float hour) {
		hour = Wrap(hour);
		return hour >= 19.5f || hour < 6.5f;
	}
	inline bool EveningLights(float hour) {
		hour = Wrap(hour);
		return hour >= 18.0f && hour < 20.5f;
	}
	inline bool Headlights(float hour) {
		hour = Wrap(hour);
		return hour >= 18.5f || hour < 7.0f;
	}

	// A colour as the game's scripts give SetSceneColor one: red + green * 256 + blue * 65536, each 0..255
	inline int Packed(const float rgb[3]) {
		int c[3];
		for (int i = 0; i < 3; i++) {
			const float v = rgb[i] < 0.0f ? 0.0f : (rgb[i] > 255.0f ? 255.0f : rgb[i]);
			c[i] = (int)(v + 0.5f);
		}
		return c[0] + c[1] * 256 + c[2] * 65536;
	}

	// The time of day: an hour from 0 to 24 that goes round once in dayMinutes of play
	class Clock {
	public:
		Clock() : m_hour(10.0f), m_hoursPerSecond(24.0f / (24.0f * 60.0f)) {}
		void Set(float hour, float dayMinutes) {
			m_hour = Wrap(hour);
			m_hoursPerSecond = dayMinutes > 0.0f ? 24.0f / (dayMinutes * 60.0f) : 0.0f;
		}
		// seconds of play went by (a long gap, a pause or a load, counts for nothing)
		void Advance(float seconds) {
			if (seconds > 0.0f && seconds <= 0.25f) {
				m_hour = Wrap(m_hour + seconds * m_hoursPerSecond);
			}
		}
		float Hour() const {
			return m_hour;
		}
		void SetHour(float hour) {
			m_hour = Wrap(hour);
		}
	private:
		float m_hour;
		float m_hoursPerSecond;
	};
}
