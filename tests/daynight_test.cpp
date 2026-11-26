// Tests for daynight.h: the keys, the easing between them, the lights' hours, the clock.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src/Flickit tests/daynight_test.cpp -o daynight_test

#include "daynight.h"

#include <cmath>
#include <cstdio>

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

static float Brightness(const DayNight::Look& l) {
	return (l.level[0] + l.level[1] + l.level[2]) / 3.0f;
}

int main() {
	// the keys go round the day in order, and the day ends as it began
	int n = 0;
	const DayNight::Key* k = DayNight::Keys(&n);
	Check(n >= 4 && Near(k[0].hour, 0.0f) && Near(k[n - 1].hour, 24.0f), "keys: from midnight to midnight");
	bool ordered = true;
	for (int i = 0; i + 1 < n; i++) {
		ordered = ordered && k[i].hour < k[i + 1].hour;
	}
	Check(ordered, "keys: in order");
	Check(DayNight::Packed(k[0].look.level) == DayNight::Packed(k[n - 1].look.level), "keys: midnight the same at both ends");

	// the day is the level as it is
	const DayNight::Look noon = DayNight::LookAt(12.0f);
	Check(Near(noon.level[0], 128.0f) && Near(noon.level[1], 128.0f) && Near(noon.level[2], 128.0f), "noon: the geometry as it is");
	Check(DayNight::Packed(noon.sky) == 128 + 128 * 256 + 128 * 65536, "noon: the sky as it is");
	Check(Near(noon.dir[0].mod, 1.4f), "noon: New Jersey's own sun");

	// night darker and bluer than day; dusk warmer than dawn
	const DayNight::Look night = DayNight::LookAt(2.0f);
	Check(Brightness(night) < Brightness(noon) * 0.6f, "night: darker");
	Check(night.level[2] > night.level[0], "night: bluer than it is red");
	const DayNight::Look dusk = DayNight::LookAt(18.75f), dawn = DayNight::LookAt(6.5f);
	Check(dusk.level[0] > dusk.level[2] && dawn.level[2] > dawn.level[0], "dusk warm, dawn cool");

	// easing: on a key exactly its look, halfway between two keys halfway, smooth (no jumps from minute to minute)
	Check(Near(DayNight::LookAt(8.0f).level[0], 128.0f) && Near(DayNight::LookAt(6.5f).level[0], 96.0f), "keys: their own looks");
	const DayNight::Look half = DayNight::LookAt(7.25f);
	Check(Near(half.level[0], (96.0f + 128.0f) * 0.5f, 0.05f), "halfway: halfway");
	float most = 0.0f;
	for (int m = 0; m < 24 * 60; m++) {
		const float a = Brightness(DayNight::LookAt(m / 60.0f)), b = Brightness(DayNight::LookAt((m + 1) / 60.0f));
		most = std::fabs(a - b) > most ? std::fabs(a - b) : most;
	}
	Check(most < 1.5f, "a game minute changes the brightness by little");
	Check(DayNight::Packed(DayNight::LookAt(-1.0f).level) == DayNight::Packed(DayNight::LookAt(23.0f).level), "hours wrap round");
	Check(DayNight::Packed(DayNight::LookAt(48.0f + 12.0f).level) == DayNight::Packed(noon.level), "days later, the same noon");

	// the lights
	Check(DayNight::NightLights(23.0f) && DayNight::NightLights(3.0f) && !DayNight::NightLights(12.0f), "night lights at night");
	Check(DayNight::NightLights(6.4f) && !DayNight::NightLights(6.6f) && DayNight::NightLights(19.6f) && !DayNight::NightLights(19.4f), "night lights: their hours");
	Check(DayNight::EveningLights(19.0f) && !DayNight::EveningLights(21.0f) && !DayNight::EveningLights(12.0f), "evening lights in the evening");
	Check(DayNight::Headlights(22.0f) && DayNight::Headlights(6.0f) && !DayNight::Headlights(12.0f) && DayNight::Headlights(18.6f), "headlights from dusk to morning");

	// packing as the scripts do it
	const float c[3] = { 1.0f, 2.0f, 3.0f };
	Check(DayNight::Packed(c) == 1 + 2 * 256 + 3 * 65536, "packed: red + green * 256 + blue * 65536");
	const float wild[3] = { -5.0f, 300.0f, 127.6f };
	Check(DayNight::Packed(wild) == 0 + 255 * 256 + 128 * 65536, "packed: each kept to 0..255, rounded");

	// the clock: 24 minutes a day is an hour a minute
	DayNight::Clock clock;
	clock.Set(10.0f, 24.0f);
	for (int i = 0; i < 60 * 60; i++) {
		clock.Advance(1.0f / 60.0f);
	}
	Check(Near(clock.Hour(), 11.0f, 0.01f), "clock: a minute of play is a game hour");
	clock.Advance(5.0f);
	Check(Near(clock.Hour(), 11.0f, 0.01f), "clock: a long gap (a pause, a load) counts for nothing");
	clock.Set(23.5f, 24.0f);
	for (int i = 0; i < 60 * 60; i++) {
		clock.Advance(1.0f / 60.0f);
	}
	Check(Near(clock.Hour(), 0.5f, 0.01f), "clock: round past midnight");
	clock.Set(12.0f, 0.0f);
	clock.Advance(0.1f);
	Check(Near(clock.Hour(), 12.0f), "clock: stopped with no day length");

	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
