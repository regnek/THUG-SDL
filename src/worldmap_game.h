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
// It needs Flickit's hooks in the game (FlickitGame::Installed): the pad update is where the skater is read
// and where he is moved. Everything here runs on the game's main thread: the look at the controller and the
// end of the frame are both there.
#include <stdint.h>
#include <SDL.h>

namespace WorldMapGame {
	// [Flickit] Map, MapButton, MapHoldMs, MapKey; the map and its spots from next to the exe; and the
	// drawing put in at the end of the frame (once). Again when the settings are read again.
	void Configure(const char* configFile);

	// The button that turns the camera, as player one's controller has it on this frame (in a level, not in
	// a menu): whether the game is to see it down. When it is the map's button too, a hold opens the map
	// and only a tap gets to the game, when it is let go.
	bool CameraButton(int port, bool down, int button, bool inMenu);

	// After the look at player one's keyboard and controller, before the buttons are turned the way the game
	// reads them: the map's controls, and while it is up (and until the buttons down when it closed are let
	// go) the game's picture of the pad left blank. cameraButton is the controller's camera button.
	void AfterPoll(uint8_t* controlData, SDL_GameController* controller, int cameraButton, bool inMenu);

	bool IsOpen();
}
