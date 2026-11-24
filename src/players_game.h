#pragma once
// Flickit's players in the game: others who skate in the same world, for the online mode to come (the parts
// without the game are in players.h).
//
// For now they are fake. A key ([Flickit] RunKey, F6) starts and stops the recording of a run: where he is,
// the way he is turned and the animation he is in, up to 30 times a second, saved in flickit_runs next to the
// exe when it stops. Each run of the level is a player: a skater of the game's (one of the level's people, on
// a board, with the skater's own animations) skates it round and round, with a made-up name over his head and
// made-up sponsors, story and stats (Players::FakeIdentity, the same for the same run). The last eight runs
// of the level are there; a new one comes in as soon as it is saved, at the place it started.
//
// Skate up to one and the game's prompt box says so; Circle (the button the cars use: the nearer of a car and
// a player has it) opens his card (WorldMapGame::AskCard), or, without the map, puts the card in words.
//
// The steps the game does are scripts of Flickit's own (scripts/flickit/players.q, compiled into
// flickit_players_qb.h). It needs Flickit's hooks in the game (FlickitGame::Installed): the skater's pad
// update is where it runs. Not in a net game.
#include <stdint.h>

#include "drive_game.h"

namespace PlayersGame {
	// [Flickit] FakePlayers (1 on), PlayerReach (how close, in game units: 90 by default), RunKey (the SDL
	// scancode that starts and stops a recording: 63, F6; 0 for none)
	void Configure(const char* configFile);
	int RunKey();
	// The key was pressed (from the keyboard's events): a recording starts or stops on the next frame
	void RunKeyPressed();

	// Once the game has read its own scripts: the player scripts go to the game (else in the first level)
	void AtScriptsLoaded();

	// Once a frame, after the game has read the local skater's pad, before the cars: the recording, the
	// players' moves, and which of them is in reach
	void Frame(const DriveGame::Skater& s);
	// How far the player in reach is, for the cars to give way to (a large number with none)
	float NearestAway();
	// After the cars, with how far the car in reach is (a large number with none): the prompt for the player
	// in reach if he is the nearer, and Circle for his card
	void AfterCars(const DriveGame::Skater& s, float carAway);
}
