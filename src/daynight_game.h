#pragma once
// Flickit's day and night in the game (the parts without the game are in daynight.h).
//
// A day goes by in DayMinutes of play (24 by default, a game hour a minute; the clock stands still while the game
// is paused or loading). Once a second the level gets the look of the hour through the game's own time of day:
// the colour of its geometry and its sky, and the lights the skater, the cars and the people are lit by. Indoor
// geometry stays as it is. At night the level has its own night, as the game's time of day gives it: its lights
// for the night come on one after another, its lit windows and lamps (New Jersey's) and its sounds for the night;
// and the cars' headlights come on from dusk to morning. DayKey (F5) puts the clock two hours on. Nothing happens
// in net games.
//
// The steps are scripts of Flickit's own (scripts/flickit/daynight.q, compiled into flickit_daynight_qb.h).
namespace DayNightGame {
	// [Flickit] DayNight (1 on), DayMinutes (a whole day, in minutes of play: 24; 0 stands the clock still),
	// DayStart (the hour the clock starts at: 10), DayKey (the key that puts it two hours on: 62, F5)
	void Configure(const char* configFile);

	// Once the game has read its own scripts: the day and night scripts go to the game
	void AtScriptsLoaded();

	// Once a frame of play, after the game has read the local skater's pad
	void Frame();

	int Key();
	void KeyPressed();
}
