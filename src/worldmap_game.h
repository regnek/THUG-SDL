#pragma once
// Flickit's map in the game (the parts without the game are in worldmap.h).
//
// A picture of the joined world from above, drawn over the frame just before the device's EndScene, with
// the spots of flickit_spots.txt on it and the skater. Holding the button that turns the camera (Back) opens
// it; a tap of that button still turns the camera, when it is let go. M on the keyboard opens it as well.
// While it is up the game is paused (the game's own pause, without its menu) and sees nothing of the
// controller or the keyboard; A travels to the spot under the cursor through the game's custom restart (see
// FlickitGame::RequestTravel), Y marks where the skater is as a spot of his own.
//
// The card of another player (players_game.h) is drawn here too, with the map's letters: who he is, the logos
// of the companies he rides for (the game's own pictures, read from its data\pre\skaterparts.pre), his story
// and his stats, and below them what can be done with him (Play S.K.A.T.E., Race, Join party: not built yet,
// they come with online play). The game is not paused under it, as other players would not be online, and
// it takes only the d-pad (and the keyboard's arrows): up and down pick, right takes, left closes. The rest of
// the controller is the game's, so the skater skates on (Flickit's sticks, the pushes, the tricks); the d-pad
// is kept from the game while the card is up and until it is let go after (DpadTaken, for input.cpp).
//
// The list of the players there opens on its own button ([Flickit] PlayersButton, the d-pad's down) or key
// (PlayersKey, P): the nearest first, how far each is and which way (an arrow, up where the camera looks). It
// takes the d-pad the same way: up and down pick, right opens the card of the one picked (left on that card
// comes back to the list), left closes. Both sit at the right of the screen, out of the skater's way.
//
// It needs Flickit's hooks in the game (FlickitGame::Installed): the pad update is where the skater is read
// and where he is moved. Everything here runs on the game's main thread: the look at the controller and the
// end of the frame are both there.
#include <stdint.h>
#include <SDL.h>

#include "players.h"

namespace WorldMapGame {
	// [Flickit] Map, MapButton, MapHoldMs, MapKey, PlayersButton, PlayersKey; the map and its spots from next
	// to the exe; and the drawing put in at the end of the frame (once). Again when the settings are read again.
	void Configure(const char* configFile);

	// The button that turns the camera, as player one's controller has it on this frame (in a level, not in
	// a menu): whether the game is to see it down. When it is the map's button too, a hold opens the map
	// and only a tap gets to the game, when it is let go.
	bool CameraButton(int port, bool down, int button, bool inMenu);

	// After the look at player one's keyboard and controller, before the buttons are turned the way the game
	// reads them: the map's controls, and while it is up (and until the buttons down when it closed are let
	// go) the game's picture of the pad left blank. cameraButton is the controller's camera button. inMenu:
	// whether the game counts as in a menu (for opening); menuUp: whether a menu of its own is on the screen,
	// for closing the map under one (the game counts any pause as a menu, the map's own as well).
	void AfterPoll(uint8_t* controlData, SDL_GameController* controller, int cameraButton, bool inMenu, bool menuUp);

	bool IsOpen();

	// A player's card, asked for from the game's frame (Circle by him): it opens at the next look at the
	// controller, if it can then. False when there is no card to draw (the map is off or has no file).
	bool AskCard(const Players::Identity& who);
	// The card or the players' list is up (or the card is to open)
	bool CardOpen();
	// Whether player one's d-pad is the card's or the list's on this frame (they are up, or it is still down
	// from closing one): then the game is not to get it. controller: player one's.
	bool DpadTaken(SDL_GameController* controller);
}
