// Flickit's map in the game (see worldmap_game.h).
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <SDL.h>

#include "d3d9.h"
#include "flickit_game.h"
#include "game.h"
#include "gamefiles.h"
#include "patch.h"
#include "players_game.h"
#include "worldmap.h"
#include "worldmap_game.h"
#include "Logger/Logger.h"

#define FLICKIT_SECTION "Flickit"

namespace WorldMapGame {
	/******************************************************************/
	/*  The game                                                      */
	/******************************************************************/

	static IDirect3DDevice9** const kDevice = (IDirect3DDevice9**)0x007CEC6C;
	static uint8_t** const kFrontEnd = (uint8_t**)0x0076A79C;        // Mdl::FrontEnd
	enum {
		FRONTEND_PAUSED = 0x30,     // byte: the game is paused (the script command GameIsPaused)
	};
	// FrontEnd::PauseGame(bool), what the script commands PauseGame and UnPauseGame call: the skaters, the
	// objects, the scripts and the sound, and the flag
	typedef void(__thiscall* PauseGame_t)(void* frontEnd, bool pause);
	static const PauseGame_t PauseGame = (PauseGame_t)0x00503D20;
	// The end of the frame: "mov eax, [device] / mov ecx, [eax] / mov edx, [ecx+0A8h] / push eax / call edx",
	// the device's EndScene. The first of those becomes a call to FrameEndStub.
	static const uint32_t kFrameEnd = 0x004980CE;

	// (the level loaded and whether this is a net game are game.h's, checked there: Game::Ready)
	static const Game::Expect kExpect[] = {
		{ 0x004980CE, 16, { 0xA1, 0x6C, 0xEC, 0x7C, 0x00, 0x8B, 0x08, 0x8B, 0x91, 0xA8, 0x00, 0x00, 0x00, 0x50, 0xFF, 0xD2 }, "the end of the frame: the device's EndScene" },
		{ 0x00527850, 13, { 0x8B, 0x0D, 0x9C, 0xA7, 0x76, 0x00, 0x6A, 0x01, 0xE8, 0xC3, 0xC4, 0xFD, 0xFF }, "the script command PauseGame" },
		{ 0x00527870, 13, { 0x8B, 0x0D, 0x9C, 0xA7, 0x76, 0x00, 0x6A, 0x00, 0xE8, 0xA3, 0xC4, 0xFD, 0xFF }, "the script command UnPauseGame" },
		{ 0x00527880, 13, { 0xA1, 0x9C, 0xA7, 0x76, 0x00, 0x80, 0x78, 0x30, 0x00, 0x0F, 0x95, 0xC0, 0xC3 }, "the script command GameIsPaused" },
		{ 0x00503D20, 13, { 0x51, 0x53, 0x8B, 0x5C, 0x24, 0x0C, 0x3A, 0x59, 0x30, 0x89, 0x4C, 0x24, 0x04 }, "FrontEnd::PauseGame" },
	};

	/******************************************************************/
	/*  What it has                                                   */
	/******************************************************************/

	static const char* const kMine = "My spots";      // the district of the spots the player marks
	static const int kDim = 40;                       // the game behind the card and the list: this much black (of 255)
	static const int kPanel = 200;                    // the card and the list: this solid (of 255), the game seen through
	static const double kPickRadius = 40.0;           // pixels at 1080 lines: a spot this near the cursor is picked
	static const double kOpenZoom = 10.0;             // units a pixel at 1080 lines the first time it opens

	static bool s_checked = false;      // the game's code compared
	static bool s_codeOk = false;
	static bool s_patched = false;      // the drawing is in
	static bool s_ready = false;        // on, with a map
	static int s_button = SDL_CONTROLLER_BUTTON_BACK;     // [Flickit] MapButton
	static int s_key = SDL_SCANCODE_M;                    // [Flickit] MapKey
	static std::vector<uint8_t> s_file;
	static WorldMap::MapFile s_map;
	static std::vector<WorldMap::Spot> s_spots;
	static std::vector<WorldMap::DistrictMark> s_districts;

	static WorldMap::HoldGate s_gate(350);
	static WorldMap::Controls s_controls;
	static WorldMap::View s_view = { 0, 0, kOpenZoom };
	static double s_zoom = kOpenZoom;   // the zoom to open at, in units a pixel at 1080 lines
	static bool s_open = false;
	static bool s_gateOpened = false;   // the hold has opened it on this frame
	static double s_charge = 0.0;       // how far through the hold, for the ring
	static bool s_keyWas = true;
	static bool s_buttonWas = true;     // a map button of its own (not the camera's)
	static bool s_holdOff = false;      // after the map: the game sees no buttons until they are let go
	static uint32_t s_holdOffSince = 0;
	static double s_lastPoll = 0.0;
	static FlickitGame::Whereabouts s_where;
	static int s_picked = -1;           // the spot under the cursor
	static int s_gliding = -1;          // the spot the map is gliding to
	static bool s_keyboardLast = false; // the help line names the keys
	static bool s_playStation = false;  // or the PlayStation's buttons
	static int s_screenW = 1920, s_screenH = 1080;

	static std::string s_message;       // a line over the help, for a moment
	static uint32_t s_messageUntil = 0;
	static std::string s_toast, s_toastDistrict;    // after a travel, with the map closed
	static uint32_t s_toastAt = 0;
	static const uint32_t kToastMs = 2600;

	// The card of another player (players_game.h): what can be done with him, and whether it is built yet
	struct CardItem {
		const char* text;
		bool built;
	};
	static const CardItem kCardItems[] = {
		{ "Play S.K.A.T.E.", false },
		{ "Race", false },
		{ "Join party", false },
		{ "Close", true },
	};
	enum { kCardItemCount = sizeof(kCardItems) / sizeof(kCardItems[0]) };
	static bool s_cardAsked = false;    // asked for on a frame of the game's, to open at the next look at the pad
	static Players::Identity s_cardWho;
	static bool s_cardOpen = false;
	static int s_cardPick = 0;

	// The card and the list take only the d-pad (and the keyboard's arrows): the game gets the rest of the
	// controller, so he skates on. The directions as they were on the look before: a press is one going down.
	struct Dpad {
		bool up, down, left, right;
	};
	static Dpad s_cardWas = { true, true, true, true };
	static bool s_dpadHold = false;     // after the card or the list: the d-pad kept from the game until let go
	static uint32_t s_dpadHoldSince = 0;
	static std::string s_cardMessage;
	static uint32_t s_cardMessageUntil = 0;
	static bool s_cardFromList = false; // opened from the players' list: closing it goes back to the list
	static uint32_t s_overlayLevel = 0; // the level the card or the list opened in

	// The list of the players there (players_game.h), on its own button (the d-pad's down) or key
	static int s_listButton = SDL_CONTROLLER_BUTTON_DPAD_DOWN;   // [Flickit] PlayersButton
	static int s_listKey = SDL_SCANCODE_P;                        // [Flickit] PlayersKey
	static bool s_listOpen = false;
	static int s_listPick = 0;          // the row picked
	static int s_listSlot = -1;         // whose row it is: the pick stays with him as the rows move
	static Dpad s_listWas = { true, true, true, true };
	static bool s_listButtonWas = true; // down on the look before (down at the start: not a press)
	static bool s_listKeyWas = true;
	static std::vector<PlayersGame::Listed> s_listRows;

	// The logos of the companies on a card: pictures of the game's own, read from its skaterparts.pre
	struct Logo {
		std::string company;
		GameFiles::Picture picture;     // w 0: not there
		IDirect3DTexture9* tex;         // of s_texDevice
		bool texTried;
	};
	static std::vector<Logo> s_logos;

	static void Message(const char* text) {
		s_message = text;
		s_messageUntil = GetTickCount() + 2500;
	}

	/******************************************************************/
	/*  The files                                                     */
	/******************************************************************/

	static bool ReadWhole(const char* path, std::vector<uint8_t>* out) {
		return Game::ReadWhole(path, out, 256u * 1024u * 1024u);
	}

	static std::string Path(const char* name) {
		return std::string(Game::ExeDir()) + name;
	}

	static void ReadSpots() {
		std::vector<uint8_t> text;
		s_spots.clear();
		if (ReadWhole(Path("flickit_spots.txt").c_str(), &text) && !text.empty()) {
			s_spots = WorldMap::ParseSpots(std::string((const char*)&text[0], text.size()));
		}
		s_districts = WorldMap::Districts(s_spots);
		Log::TypedLog(CHN_SDL, "Map: %u spots in flickit_spots.txt, in %u districts\n", (unsigned)s_spots.size(), (unsigned)s_districts.size());
	}

	// To the end of flickit_spots.txt, on a line of its own
	static bool AppendSpot(const WorldMap::Spot& s) {
		const std::string path = Path("flickit_spots.txt");
		std::vector<uint8_t> was;
		const bool there = ReadWhole(path.c_str(), &was);
		HANDLE f = CreateFileA(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (f == INVALID_HANDLE_VALUE) {
			return false;
		}
		std::string line;
		if (!there || was.empty()) {
			line = "# Flickit map spots, one a line: x y z heading | district | name (heading in degrees, 0 faces +z)\n";
		}
		else if (was.back() != '\n') {
			line = "\r\n";
		}
		line += WorldMap::FormatSpot(s) + "\r\n";
		DWORD put = 0;
		const bool ok = WriteFile(f, line.data(), (DWORD)line.size(), &put, NULL) && put == (DWORD)line.size();
		CloseHandle(f);
		return ok;
	}

	/******************************************************************/
	/*  Whether it may open                                           */
	/******************************************************************/

	// NULL while the game is being played by a skater who has his controls, else why not (for the log): what
	// the map and a card both need before they open. w: where he is.
	static const char* NotPlaying(bool inMenu, FlickitGame::Whereabouts* w) {
		if (inMenu) {
			return "a menu is up";
		}
		const uint8_t* frontEnd = *kFrontEnd;
		if (!frontEnd || frontEnd[FRONTEND_PAUSED]) {
			return "the game is paused";
		}
		if (Game::InNet()) {
			return "this is a net game";
		}
		if (!FlickitGame::SkaterWhereabouts(*w) || GetTickCount() - w->tick > 500) {
			return "no skater has been seen";
		}
		if (!w->controls) {
			return "the skater's controls are taken";
		}
		return NULL;
	}

	// NULL when the map can open now, else why not (for the log)
	static const char* Refusal(bool inMenu) {
		if (!s_ready) {
			return "it is off";
		}
		FlickitGame::Whereabouts w;
		const char* why = NotPlaying(inMenu, &w);
		if (why) {
			return why;
		}
		if (Game::LevelScript() != s_map.level) {
			return "the map is of another level";
		}
		if (!w.settled) {
			return "autosave is still to put him back";
		}
		return NULL;
	}

	// The size of what the game draws to, for the first frame of the map
	static void ScreenSize() {
		IDirect3DDevice9* dev = *kDevice;
		IDirect3DSurface9* rt = NULL;
		if (!dev || FAILED(dev->GetRenderTarget(0, &rt)) || !rt) {
			return;
		}
		D3DSURFACE_DESC desc;
		if (SUCCEEDED(rt->GetDesc(&desc)) && desc.Width > 0 && desc.Height > 0) {
			s_screenW = (int)desc.Width;
			s_screenH = (int)desc.Height;
		}
		rt->Release();
	}

	static void Open() {
		ScreenSize();
		FlickitGame::SkaterWhereabouts(s_where);
		s_view.cx = s_where.pos[0];
		s_view.cz = s_where.pos[2];
		s_view.upp = s_zoom * 1080.0 / (s_screenH > 1 ? s_screenH : 1080);
		WorldMap::Clamp(&s_view, s_map, s_screenW, s_screenH);
		s_controls.Reset();
		s_picked = -1;
		s_gliding = -1;
		s_message.clear();
		s_toast.clear();
		PauseGame(*kFrontEnd, true);
		s_open = true;
		Log::TypedLog(CHN_SDL, "Map: open, the skater at %.0f %.0f %.0f\n", s_where.pos[0], s_where.pos[1], s_where.pos[2]);
	}

	static void Close(bool unpause, bool mapButtonDown) {
		s_open = false;
		s_zoom = s_view.upp * (s_screenH > 1 ? s_screenH : 1080) / 1080.0;
		const uint8_t* frontEnd = *kFrontEnd;
		if (unpause && frontEnd && frontEnd[FRONTEND_PAUSED]) {
			PauseGame(*kFrontEnd, false);
		}
		s_holdOff = true;
		s_holdOffSince = GetTickCount();
		s_gate.Spend(mapButtonDown);
		s_charge = 0.0;
	}

	/******************************************************************/
	/*  The controls                                                  */
	/******************************************************************/

	static float Axis(SDL_GameController* c, SDL_GameControllerAxis a) {
		return (float)SDL_GameControllerGetAxis(c, a) / 32767.0f;
	}

	static bool Button(SDL_GameController* c, int b) {
		return b >= 0 && b < SDL_CONTROLLER_BUTTON_MAX && SDL_GameControllerGetButton(c, (SDL_GameControllerButton)b) != 0;
	}

	static bool Key(const Uint8* keys, int k) {
		return k > 0 && k < SDL_NUM_SCANCODES && keys[k] != 0;
	}

	// The keys the map reads
	static const int kMapKeys[] = {
		SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_W, SDL_SCANCODE_A, SDL_SCANCODE_S,
		SDL_SCANCODE_D, SDL_SCANCODE_Q, SDL_SCANCODE_E, SDL_SCANCODE_PAGEUP, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_EQUALS,
		SDL_SCANCODE_MINUS, SDL_SCANCODE_KP_PLUS, SDL_SCANCODE_KP_MINUS, SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER,
		SDL_SCANCODE_SPACE, SDL_SCANCODE_TAB, SDL_SCANCODE_F, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_BACKSPACE,
	};

	static WorldMap::Pad ReadPad(SDL_GameController* c, const Uint8* keys) {
		WorldMap::Pad p;
		memset(&p, 0, sizeof(p));
		if (c) {
			p.lx = Axis(c, SDL_CONTROLLER_AXIS_LEFTX);
			p.ly = Axis(c, SDL_CONTROLLER_AXIS_LEFTY);
			p.lt = Axis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
			p.rt = Axis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
			p.up = Button(c, SDL_CONTROLLER_BUTTON_DPAD_UP);
			p.down = Button(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
			p.left = Button(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
			p.right = Button(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
			p.a = Button(c, SDL_CONTROLLER_BUTTON_A);
			p.b = Button(c, SDL_CONTROLLER_BUTTON_B);
			p.x = Button(c, SDL_CONTROLLER_BUTTON_X);
			p.y = Button(c, SDL_CONTROLLER_BUTTON_Y);
			p.back = Button(c, s_button) || Button(c, SDL_CONTROLLER_BUTTON_BACK);
			p.start = Button(c, SDL_CONTROLLER_BUTTON_START);
		}
		bool keyed = false;
		for (size_t i = 0; i < sizeof(kMapKeys) / sizeof(kMapKeys[0]); i++) {
			keyed = keyed || Key(keys, kMapKeys[i]);
		}
		keyed = keyed || Key(keys, s_key);
		if (keyed) {
			s_keyboardLast = true;
		}
		else if (c && (p.a || p.b || p.x || p.y || p.up || p.down || p.left || p.right || fabsf(p.lx) > 0.3f || fabsf(p.ly) > 0.3f
			|| p.lt > 0.3f || p.rt > 0.3f)) {
			s_keyboardLast = false;
		}
		p.up = p.up || Key(keys, SDL_SCANCODE_UP) || Key(keys, SDL_SCANCODE_W);
		p.down = p.down || Key(keys, SDL_SCANCODE_DOWN) || Key(keys, SDL_SCANCODE_S);
		p.left = p.left || Key(keys, SDL_SCANCODE_LEFT) || Key(keys, SDL_SCANCODE_A);
		p.right = p.right || Key(keys, SDL_SCANCODE_RIGHT) || Key(keys, SDL_SCANCODE_D);
		if (Key(keys, SDL_SCANCODE_E) || Key(keys, SDL_SCANCODE_PAGEUP) || Key(keys, SDL_SCANCODE_EQUALS) || Key(keys, SDL_SCANCODE_KP_PLUS)) {
			p.rt = 1.0f;
		}
		if (Key(keys, SDL_SCANCODE_Q) || Key(keys, SDL_SCANCODE_PAGEDOWN) || Key(keys, SDL_SCANCODE_MINUS) || Key(keys, SDL_SCANCODE_KP_MINUS)) {
			p.lt = 1.0f;
		}
		p.a = p.a || Key(keys, SDL_SCANCODE_RETURN) || Key(keys, SDL_SCANCODE_KP_ENTER) || Key(keys, SDL_SCANCODE_SPACE);
		p.b = p.b || Key(keys, SDL_SCANCODE_ESCAPE) || Key(keys, SDL_SCANCODE_BACKSPACE);
		p.back = p.back || Key(keys, s_key);
		p.x = p.x || Key(keys, SDL_SCANCODE_TAB);
		p.y = p.y || Key(keys, SDL_SCANCODE_F);
		return p;
	}

	// Anything down that the game would take for a press
	static bool AnyHeld(SDL_GameController* c, const Uint8* keys) {
		if (c) {
			for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) {
				if (Button(c, b)) {
					return true;
				}
			}
			if (Axis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 0.3f || Axis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0.3f) {
				return true;
			}
		}
		for (size_t i = 0; i < sizeof(kMapKeys) / sizeof(kMapKeys[0]); i++) {
			if (Key(keys, kMapKeys[i])) {
				return true;
			}
		}
		return Key(keys, s_key);
	}

	// The game's picture of the pad as if nothing were down (bits set for buttons down, inverted after this)
	static void Blank(uint8_t* cd, bool sticks) {
		cd[2] = 0;
		cd[3] = 0;
		for (int i = 8; i <= 20; i++) {
			cd[i] = 0;
		}
		if (sticks) {
			cd[4] = cd[5] = cd[6] = cd[7] = 127;
		}
	}

	// The d-pad and the keyboard's arrows (the game steers with WASD, not the arrows)
	static Dpad ReadDpad(SDL_GameController* c, const Uint8* keys) {
		Dpad d;
		d.up = (c && Button(c, SDL_CONTROLLER_BUTTON_DPAD_UP)) || Key(keys, SDL_SCANCODE_UP);
		d.down = (c && Button(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) || Key(keys, SDL_SCANCODE_DOWN);
		d.left = (c && Button(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) || Key(keys, SDL_SCANCODE_LEFT);
		d.right = (c && Button(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) || Key(keys, SDL_SCANCODE_RIGHT);
		if (Key(keys, SDL_SCANCODE_UP) || Key(keys, SDL_SCANCODE_DOWN) || Key(keys, SDL_SCANCODE_LEFT) || Key(keys, SDL_SCANCODE_RIGHT)) {
			s_keyboardLast = true;
		}
		else if (c && (d.up || d.down || d.left || d.right)) {
			s_keyboardLast = false;
		}
		return d;
	}

	static bool AnyDpad(SDL_GameController* c) {
		return c && (Button(c, SDL_CONTROLLER_BUTTON_DPAD_UP) || Button(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN)
			|| Button(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || Button(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT));
	}

	static void MarkSpot() {
		WorldMap::Spot s;
		s.x = s_where.pos[0];
		s.y = s_where.pos[1];
		s.z = s_where.pos[2];
		s.heading = s_where.heading;
		s.district = kMine;
		s.name = WorldMap::NextMarkName(s_spots);
		// what goes in the file is what comes back out of it
		WorldMap::Spot kept;
		if (!WorldMap::ParseSpotLine(WorldMap::FormatSpot(s), &kept) || !AppendSpot(kept)) {
			Message("Could not write flickit_spots.txt");
			Log::TypedLog(CHN_SDL, "Map: could not add a spot to %s (error %u)\n", Path("flickit_spots.txt").c_str(), (unsigned)GetLastError());
			return;
		}
		s_spots.push_back(kept);
		s_districts = WorldMap::Districts(s_spots);
		s_gliding = (int)s_spots.size() - 1;
		s_controls.GlideTo(kept.x, kept.z);
		const std::string text = "Marked " + kept.name + " where you are";
		Message(text.c_str());
		Log::TypedLog(CHN_SDL, "Map: marked %s at %.0f %.0f %.0f\n", kept.name.c_str(), kept.x, kept.y, kept.z);
	}

	/******************************************************************/
	/*  The card of another player                                    */
	/******************************************************************/

	// NULL when the card can open now, else why not (for the log)
	static const char* CardRefusal(bool inMenu) {
		if (!s_ready) {
			return "the map is off";
		}
		FlickitGame::Whereabouts w;
		return NotPlaying(inMenu, &w);
	}

	static Logo* FindLogo(const char* company) {
		for (size_t i = 0; i < s_logos.size(); i++) {
			if (_stricmp(s_logos[i].company.c_str(), company) == 0) {
				return &s_logos[i];
			}
		}
		return NULL;
	}

	// A file as GameFiles::WalkPre reads an archive
	struct FileSource {
		FILE* f;
		bool Read(void* to, size_t n) {
			return fread(to, 1, n, f) == n;
		}
		bool Skip(size_t n) {
			return fseek(f, (long)n, SEEK_CUR) == 0;
		}
	};

	// The logos of these companies, read from the game's data\pre\skaterparts.pre the first time each is
	// wanted, in one pass over it (the other entries are passed by their heads)
	static void ReadLogos(const char* const* companies, int n) {
		std::vector<size_t> todo;
		for (int i = 0; i < n; i++) {
			if (!companies[i] || !companies[i][0] || FindLogo(companies[i])) {
				continue;
			}
			Logo l;
			l.company = companies[i];
			l.picture.w = l.picture.h = 0;
			l.tex = NULL;
			l.texTried = false;
			s_logos.push_back(l);
			if (Players::LogoOf(companies[i])) {
				todo.push_back(s_logos.size() - 1);
			}
			else {
				Log::TypedLog(CHN_SDL, "Map: the game has no logo of %s; the card has its name\n", companies[i]);
			}
		}
		if (todo.empty()) {
			return;
		}
		const std::string path = Path("data\\pre\\skaterparts.pre");
		FILE* f = NULL;
		if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) {
			Log::TypedLog(CHN_SDL, "Map: cannot open %s; the card has the companies' names\n", path.c_str());
			return;
		}
		FileSource src = { f };
		size_t found = 0;
		Logo* want = NULL;
		const bool archive = GameFiles::WalkPre(src,
			[&](const char* name, size_t length) {
				want = NULL;
				for (size_t k = 0; k < todo.size() && !want; k++) {
					Logo& l = s_logos[todo[k]];
					if (l.picture.w == 0 && GameFiles::SameName(name, length, Players::LogoOf(l.company.c_str()))) {
						want = &l;
					}
				}
				return want != NULL;
			},
			[&](const char*, size_t, const GameFiles::EntryHead& h, const uint8_t* stored) {
				std::vector<uint8_t> data;
				if (GameFiles::Unpack(h, stored, &data) && !data.empty() && GameFiles::DecodeImg(&data[0], data.size(), &want->picture)) {
					found++;
				}
				else {
					want->picture = GameFiles::Picture();
					want->picture.w = want->picture.h = 0;
					Log::TypedLog(CHN_SDL, "Map: the logo of %s in skaterparts.pre is not a picture Flickit can read\n", want->company.c_str());
				}
				return found < todo.size();
			});
		if (!archive) {
			Log::TypedLog(CHN_SDL, "Map: %s is not a .pre archive of the game's\n", path.c_str());
		}
		fclose(f);
		Log::TypedLog(CHN_SDL, "Map: %u of %u logos read from %s\n", (unsigned)found, (unsigned)todo.size(), path.c_str());
	}

	static D3DCOLOR SponsorColor(int sponsor) {
		return sponsor == Players::kPro ? D3DCOLOR_ARGB(255, 255, 196, 70) : sponsor == Players::kAm ? D3DCOLOR_ARGB(255, 96, 182, 255)
			: D3DCOLOR_ARGB(255, 178, 184, 196);
	}

	// NULL while the card or the list can stay up, else why not: the game goes on under them
	static const char* OverlayEnd(bool menuUp) {
		if (menuUp) {
			return "a menu of the game's came up";
		}
		if (Game::InNet()) {
			return "a net game";
		}
		if (Game::LevelScript() != s_overlayLevel) {
			return "the level changed";
		}
		FlickitGame::Whereabouts w;
		if (FlickitGame::SkaterWhereabouts(w) && GetTickCount() - w.tick <= 500 && (!w.controls || w.driving)) {
			return "the skater's controls were taken";
		}
		return NULL;
	}

	static void OpenList(const Dpad& now);

	static void OpenCard(const Dpad& now) {
		ScreenSize();
		s_cardPick = 0;
		s_cardWas = now;                // a direction down already does nothing
		s_cardMessage.clear();
		s_toast.clear();
		s_overlayLevel = Game::LevelScript();
		if (s_cardWho.sponsor != Players::kUnsponsored) {
			const char* companies[2] = { s_cardWho.board, s_cardWho.shoes };
			ReadLogos(companies, 2);
		}
		s_cardOpen = true;
		Log::TypedLog(CHN_SDL, "Map: the card of %s (%s, %s), open%s\n", s_cardWho.name, Players::SponsorName(s_cardWho.sponsor), s_cardWho.level,
			s_cardFromList ? ", from the list" : "");
	}

	// The d-pad stays the card's and the list's until it is let go, so the press that closed one does not
	// reach the game
	static void HoldDpad() {
		s_dpadHold = true;
		s_dpadHoldSince = GetTickCount();
	}

	// The game goes on under the card. back: to the list, if it came from there.
	static void CloseCard(bool back, const Dpad& now) {
		s_cardOpen = false;
		if (back && s_cardFromList) {
			s_cardFromList = false;
			OpenList(now);
			return;
		}
		s_cardFromList = false;
		HoldDpad();
	}

	// Up and down choose, right takes, left closes (back to the list, if it came from there)
	static void CardFrame(SDL_GameController* c, const Uint8* keys, bool menuUp) {
		const Dpad d = ReadDpad(c, keys);
		const char* end = OverlayEnd(menuUp);
		if (end) {
			Log::TypedLog(CHN_SDL, "Map: the card closed: %s\n", end);
			s_cardFromList = false;
			CloseCard(false, d);
			return;
		}
		int move = 0;
		if (d.up && !s_cardWas.up) {
			move = -1;
		}
		if (d.down && !s_cardWas.down) {
			move = 1;
		}
		s_cardPick = (s_cardPick + move + kCardItemCount) % kCardItemCount;
		const bool choose = d.right && !s_cardWas.right;
		const bool close = d.left && !s_cardWas.left;
		s_cardWas = d;
		if (close || (choose && kCardItems[s_cardPick].built)) {
			Log::TypedLog(CHN_SDL, "Map: the card of %s, closed\n", s_cardWho.name);
			CloseCard(true, d);
			return;
		}
		if (choose) {
			s_cardMessage = std::string(kCardItems[s_cardPick].text) + " is not built yet: it comes with online play.";
			s_cardMessageUntil = GetTickCount() + 2600;
			Log::TypedLog(CHN_SDL, "Map: %s with %s: not built yet\n", kCardItems[s_cardPick].text, s_cardWho.name);
		}
	}

	bool AskCard(const Players::Identity& who) {
		if (!s_ready || s_open || s_cardOpen || s_listOpen) {
			return false;
		}
		s_cardWho = who;
		s_cardFromList = false;
		s_cardAsked = true;
		return true;
	}

	bool CardOpen() {
		return s_cardOpen || s_cardAsked || s_listOpen;
	}

	/******************************************************************/
	/*  The list of the players there                                 */
	/******************************************************************/

	static const char* ListRefusal(bool inMenu) {
		const char* why = CardRefusal(inMenu);
		if (why) {
			return why;
		}
		FlickitGame::Whereabouts w;
		if (FlickitGame::SkaterWhereabouts(w) && w.driving) {
			return "he is in a car";
		}
		return NULL;
	}

	// The pick on the row of the player picked before, if he is still there
	static void ListRows() {
		PlayersGame::ListPlayers(s_listRows);
		const int n = (int)s_listRows.size();
		int at = -1;
		for (int i = 0; i < n; i++) {
			if (s_listRows[i].slot == s_listSlot) {
				at = i;
			}
		}
		s_listPick = at >= 0 ? at : (s_listPick >= n ? n - 1 : s_listPick);
		s_listPick = s_listPick < 0 ? 0 : s_listPick;
		s_listSlot = n ? s_listRows[s_listPick].slot : -1;
	}

	static void OpenList(const Dpad& now) {
		ScreenSize();
		s_listWas = now;
		s_overlayLevel = Game::LevelScript();
		ListRows();
		s_listOpen = true;
	}

	static void CloseList() {
		s_listOpen = false;
		HoldDpad();
	}

	// Up and down choose, right opens the card of the one picked, left (or the list's key) closes
	static void ListFrame(SDL_GameController* c, const Uint8* keys, bool menuUp, bool keyPressed) {
		const Dpad d = ReadDpad(c, keys);
		const char* end = OverlayEnd(menuUp);
		if (end) {
			Log::TypedLog(CHN_SDL, "Map: the players' list closed: %s\n", end);
			CloseList();
			return;
		}
		ListRows();
		const int n = (int)s_listRows.size();
		int move = 0;
		if (d.up && !s_listWas.up) {
			move = -1;
		}
		if (d.down && !s_listWas.down) {
			move = 1;
		}
		if (n > 0 && move) {
			s_listPick = (s_listPick + move + n) % n;
			s_listSlot = s_listRows[s_listPick].slot;
		}
		const bool choose = d.right && !s_listWas.right;
		const bool close = (d.left && !s_listWas.left) || keyPressed;
		s_listWas = d;
		if (close) {
			CloseList();
		}
		else if (choose && n > 0) {
			s_listOpen = false;
			s_cardWho = s_listRows[s_listPick].who;
			s_cardFromList = true;
			OpenCard(d);
		}
	}

	bool DpadTaken(SDL_GameController* controller) {
		if (s_cardOpen || s_listOpen) {
			return true;
		}
		if (s_dpadHold) {
			SDL_GameController* c = controller && SDL_GameControllerGetAttached(controller) ? controller : NULL;
			if (AnyDpad(c) && GetTickCount() - s_dpadHoldSince < 1500) {
				return true;
			}
			s_dpadHold = false;
		}
		return false;
	}

	static void TravelTo(int i, bool mapButtonDown) {
		const WorldMap::Spot& s = s_spots[i];
		if (s_where.driving) {
			Message("Get out of the car to travel");
			return;
		}
		const float pos[3] = { (float)s.x, (float)s.y, (float)s.z };
		if (!FlickitGame::RequestTravel(pos, (float)s.heading)) {
			Message("Travel is off: the game's restart is not as expected");
			return;
		}
		s_toast = s.name;
		s_toastDistrict = s.district;
		s_toastAt = GetTickCount();
		Log::TypedLog(CHN_SDL, "Map: travel to %s (%s)\n", s.name.c_str(), s.district.c_str());
		Close(true, mapButtonDown);
	}

	static void OpenFrame(uint8_t* cd, SDL_GameController* c, const Uint8* keys, double dt, bool menuUp) {
		if (menuUp) {
			// a menu of the game's came up over it (the pause is the menu's now)
			Log::TypedLog(CHN_SDL, "Map: closed for a menu of the game's\n");
			Close(false, c && Button(c, s_button));
			return;
		}
		if (Game::LevelScript() != s_map.level || Game::InNet() || !*kFrontEnd || !(*kFrontEnd)[FRONTEND_PAUSED]) {
			// the level went, or something else unpaused the game
			Log::TypedLog(CHN_SDL, "Map: closed by the game\n");
			Close(false, c && Button(c, s_button));
			Blank(cd, true);
			return;
		}
		const WorldMap::Pad p = ReadPad(c, keys);
		const WorldMap::Actions a = s_controls.Frame(p, dt, &s_view, s_map, s_screenW, s_screenH);
		const double ui = s_screenH / 1080.0;
		if (!s_controls.Gliding()) {
			s_gliding = -1;
		}
		s_picked = s_gliding >= 0 ? s_gliding : WorldMap::SpotAtCursor(s_spots, s_view, kPickRadius * ui);
		if (a.next) {
			const int n = WorldMap::NextSpot(s_spots, s_view, s_picked);
			if (n >= 0) {
				s_gliding = n;
				s_picked = n;
				s_controls.GlideTo(s_spots[n].x, s_spots[n].z);
			}
		}
		if (a.mark) {
			MarkSpot();
		}
		if (a.travel) {
			if (s_picked >= 0) {
				TravelTo(s_picked, c && Button(c, s_button));
			}
			else {
				Message(s_spots.empty() ? "No spots: flickit_spots.txt is not next to the game" : "Put the cursor on a spot to travel there");
			}
		}
		if (s_open && a.close) {
			Close(true, c && Button(c, s_button));
		}
		Blank(cd, true);
	}

	bool CameraButton(int port, bool down, int button, bool inMenu) {
		if (port != 0 || !s_ready || button != s_button) {
			return down;
		}
		if (s_open) {
			s_gate.Spend(down);
			return false;
		}
		if (Refusal(inMenu)) {
			s_gate.Spend(down);
			s_charge = 0.0;
			return down;
		}
		const uint32_t now = GetTickCount();
		bool pass = false, open = false;
		s_gate.Frame(down, now, &pass, &open);
		s_charge = s_gate.Charge(down, now);
		if (open) {
			s_gateOpened = true;
			s_charge = 0.0;
		}
		return pass;
	}

	void AfterPoll(uint8_t* cd, SDL_GameController* controller, int cameraButton, bool inMenu, bool menuUp) {
		if (!s_ready) {
			return;
		}
		const double t = Game::Seconds();
		const double dt = s_lastPoll > 0.0 ? t - s_lastPoll : 0.0;
		s_lastPoll = t;
		SDL_GameController* c = controller && SDL_GameControllerGetAttached(controller) ? controller : NULL;
		if (c) {
			const SDL_GameControllerType type = SDL_GameControllerGetType(c);
			s_playStation = type == SDL_CONTROLLER_TYPE_PS3 || type == SDL_CONTROLLER_TYPE_PS4 || type == SDL_CONTROLLER_TYPE_PS5;
		}
		const Uint8* keys = SDL_GetKeyboardState(NULL);
		const bool keyDown = Key(keys, s_key);
		const bool ownButton = s_button != cameraButton;
		const bool buttonDown = ownButton && c && Button(c, s_button);
		const bool listDown = c && Button(c, s_listButton);
		const bool listKeyDown = Key(keys, s_listKey);
		const bool listPress = listDown && !s_listButtonWas;
		const bool listKeyPress = listKeyDown && !s_listKeyWas;
		s_listButtonWas = listDown;
		s_listKeyWas = listKeyDown;

		if (s_open) {
			OpenFrame(cd, c, keys, dt, menuUp);
			s_keyWas = keyDown;
			s_buttonWas = buttonDown;
			s_gateOpened = false;
			return;
		}
		// the card and the list leave the game the controller but for the d-pad (input.cpp keeps that from it)
		if (s_cardOpen) {
			CardFrame(c, keys, menuUp);
			s_keyWas = keyDown;
			s_buttonWas = buttonDown;
			s_gateOpened = false;
			return;
		}
		if (s_listOpen) {
			ListFrame(c, keys, menuUp, listKeyPress);
			s_keyWas = keyDown;
			s_buttonWas = buttonDown;
			s_gateOpened = false;
			return;
		}
		if (s_cardAsked) {
			s_cardAsked = false;
			const char* why = CardRefusal(inMenu);
			if (why) {
				Log::TypedLog(CHN_SDL, "Map: the card of %s, not now: %s\n", s_cardWho.name, why);
			}
			else {
				OpenCard(ReadDpad(c, keys));
				s_keyWas = keyDown;
				s_buttonWas = buttonDown;
				s_gateOpened = false;
				return;
			}
		}
		// a press of its own (the button went down on this look), even while buttons let go of after the card are
		// kept from the game
		if (listPress || listKeyPress) {
			const char* why = ListRefusal(inMenu);
			if (why) {
				Log::TypedLog(CHN_SDL, "Map: the players' list, not now: %s\n", why);
			}
			else {
				OpenList(ReadDpad(c, keys));
				Log::TypedLog(CHN_SDL, "Map: the players' list, open (%u there)\n", (unsigned)s_listRows.size());
				// the press that opened it is in this frame's pad already: not the game's
				if (listPress && s_listButton == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
					cd[2] &= (uint8_t)~(0x01 << 6);
					cd[11] = 0;
				}
				s_keyWas = keyDown;
				s_buttonWas = buttonDown;
				s_gateOpened = false;
				return;
			}
		}
		if (s_holdOff) {
			if (AnyHeld(c, keys) && GetTickCount() - s_holdOffSince < 1500) {
				Blank(cd, false);
			}
			else {
				s_holdOff = false;
			}
		}
		bool ask = false;
		if (s_gateOpened) {
			ask = true;
			s_keyboardLast = false;
		}
		s_gateOpened = false;
		if (keyDown && !s_keyWas) {
			ask = true;
			s_keyboardLast = true;
		}
		if (buttonDown && !s_buttonWas) {
			ask = true;
			s_keyboardLast = false;
		}
		s_keyWas = keyDown;
		s_buttonWas = buttonDown;
		if (!ask) {
			return;
		}
		const char* why = Refusal(inMenu);
		if (why) {
			Log::TypedLog(CHN_SDL, "Map: not now: %s\n", why);
			return;
		}
		Open();
		Blank(cd, true);
	}

	/******************************************************************/
	/*  The drawing                                                   */
	/******************************************************************/

	struct Vertex {
		float x, y, z, rhw;
		D3DCOLOR color;
		float u, v;
	};
	static const DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

	static IDirect3DDevice9* s_texDevice = NULL;      // the device the textures are of
	static IDirect3DTexture9* s_mapTex = NULL;
	static IDirect3DTexture9* s_fontTex = NULL;
	static bool s_texTried = false;
	static float s_mapU = 1.0f, s_mapV = 1.0f;        // the picture's corner in the texture
	static std::vector<Vertex> s_tris;
	static double s_ui = 1.0;

	static D3DCOLOR Rgba(int r, int g, int b, int a) {
		a = a < 0 ? 0 : a > 255 ? 255 : a;
		return D3DCOLOR_ARGB(a, r, g, b);
	}

	static D3DCOLOR Faded(D3DCOLOR c, double k) {
		const int a = (int)(((c >> 24) & 0xFF) * (k < 0 ? 0 : k > 1 ? 1 : k) + 0.5);
		return (c & 0x00FFFFFF) | ((D3DCOLOR)a << 24);
	}

	// The districts' colours, in the order they first come in the spots file; the player's own are green
	static D3DCOLOR DistrictColor(const std::string& district, int alpha) {
		static const int kColors[][3] = { { 255, 190, 70 }, { 90, 180, 255 }, { 255, 105, 170 }, { 190, 140, 255 }, { 255, 130, 80 }, { 80, 220, 220 } };
		if (district == kMine) {
			return Rgba(120, 225, 130, alpha);
		}
		const int i = WorldMap::DistrictIndex(s_districts, district);
		const int* c = kColors[(i < 0 ? 0 : i) % (int)(sizeof(kColors) / sizeof(kColors[0]))];
		return Rgba(c[0], c[1], c[2], alpha);
	}

	static void Put(float x, float y, D3DCOLOR c, float u, float v) {
		const Vertex p = { x - 0.5f, y - 0.5f, 0.0f, 1.0f, c, u, v };
		s_tris.push_back(p);
	}

	static void Quad(float x0, float y0, float x1, float y1, D3DCOLOR c, float u0 = 0, float v0 = 0, float u1 = 0, float v1 = 0) {
		Put(x0, y0, c, u0, v0);
		Put(x1, y0, c, u1, v0);
		Put(x0, y1, c, u0, v1);
		Put(x1, y0, c, u1, v0);
		Put(x1, y1, c, u1, v1);
		Put(x0, y1, c, u0, v1);
	}

	static void Tri(float ax, float ay, float bx, float by, float cx, float cy, D3DCOLOR c) {
		Put(ax, ay, c, 0, 0);
		Put(bx, by, c, 0, 0);
		Put(cx, cy, c, 0, 0);
	}

	static void Diamond(float x, float y, float r, D3DCOLOR c) {
		Tri(x, y - r, x + r, y, x, y + r, c);
		Tri(x, y - r, x, y + r, x - r, y, c);
	}

	// A ring between two radii, from angle a0 to a1 (radians, clockwise on the screen from the right)
	static void Ring(float cx, float cy, float r0, float r1, double a0, double a1, D3DCOLOR c) {
		const int segs = 40;
		for (int i = 0; i < segs; i++) {
			const double t0 = a0 + (a1 - a0) * i / segs, t1 = a0 + (a1 - a0) * (i + 1) / segs;
			const float c0 = (float)cos(t0), s0 = (float)sin(t0), c1 = (float)cos(t1), s1 = (float)sin(t1);
			Tri(cx + c0 * r0, cy + s0 * r0, cx + c0 * r1, cy + s0 * r1, cx + c1 * r1, cy + s1 * r1, c);
			Tri(cx + c0 * r0, cy + s0 * r0, cx + c1 * r1, cy + s1 * r1, cx + c1 * r0, cy + s1 * r0, c);
		}
	}

	// The skater: an arrow the way he faces (z goes down the screen)
	static void Arrow(float x, float y, float r, double heading, D3DCOLOR c) {
		const float fx = (float)sin(heading), fy = (float)cos(heading), sx = fy, sy = -fx;
		const float tx = x + fx * r, ty = y + fy * r;
		const float lx = x - fx * r * 0.7f + sx * r * 0.75f, ly = y - fy * r * 0.7f + sy * r * 0.75f;
		const float rx = x - fx * r * 0.7f - sx * r * 0.75f, ry = y - fy * r * 0.7f - sy * r * 0.75f;
		const float nx = x - fx * r * 0.3f, ny = y - fy * r * 0.3f;
		Tri(tx, ty, lx, ly, nx, ny, c);
		Tri(tx, ty, nx, ny, rx, ry, c);
	}

	static void Flush(IDirect3DDevice9* dev, IDirect3DTexture9* tex) {
		if (s_tris.empty()) {
			return;
		}
		dev->SetTexture(0, tex);
		const DWORD op = tex ? D3DTOP_MODULATE : D3DTOP_SELECTARG2;
		dev->SetTextureStageState(0, D3DTSS_COLOROP, op);
		dev->SetTextureStageState(0, D3DTSS_ALPHAOP, op);
		dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)(s_tris.size() / 3), &s_tris[0], sizeof(Vertex));
		s_tris.clear();
	}

	// Text, its top at y, px the height of its line; tracking adds space after every letter (in lines)
	static float TextWidth(const std::string& s, float px, float tracking = 0.0f) {
		return (float)(WorldMap::TextWidth(s_map, s) * px / s_map.line) + (s.empty() ? 0.0f : tracking * px * (float)(s.size() - 1));
	}

	static void Glyphs(const std::string& s, float pen, float top, float px, float tracking, D3DCOLOR c) {
		const float k = px / s_map.line;
		const float fw = (float)s_map.fontW, fh = (float)s_map.fontH;
		for (size_t i = 0; i < s.size(); i++) {
			const WorldMap::Glyph* g = WorldMap::FindGlyph(s_map, (uint8_t)s[i]);
			if (!g) {
				g = WorldMap::FindGlyph(s_map, '?');
			}
			if (!g) {
				continue;
			}
			if (s[i] != ' ') {
				const float x0 = floorf(pen + g->xoff * k + 0.5f), y0 = floorf(top + g->yoff * k + 0.5f);
				Quad(x0, y0, x0 + g->w * k, y0 + g->h * k, c, g->x / fw, g->y / fh, (g->x + g->w) / fw, (g->y + g->h) / fh);
			}
			pen += g->advance * k + tracking * px;
		}
	}

	// align: -1 from x, 0 centred on x, 1 ending at x
	static void Text(const std::string& s, float x, float top, float px, D3DCOLOR c, int align, float tracking = 0.0f, bool shadow = true) {
		const float w = TextWidth(s, px, tracking);
		const float pen = align < 0 ? x : align == 0 ? x - w * 0.5f : x - w;
		if (shadow) {
			const float d = (float)(2.0 * s_ui > 1.0 ? 2.0 * s_ui : 1.0);
			Glyphs(s, pen + d, top + d, px, tracking, Faded(Rgba(0, 0, 0, 200), ((c >> 24) & 0xFF) / 255.0));
		}
		Glyphs(s, pen, top, px, tracking, c);
	}

	static std::string Upper(const std::string& s) {
		std::string u = s;
		for (size_t i = 0; i < u.size(); i++) {
			if (u[i] >= 'a' && u[i] <= 'z') {
				u[i] = (char)(u[i] - 'a' + 'A');
			}
		}
		return u;
	}

	// The textures, once a device: MANAGED, so a reset of the device keeps them
	static void MakeTextures(IDirect3DDevice9* dev) {
		if (dev != s_texDevice) {
			// a device of its own: the old one's textures went with it
			s_texDevice = dev;
			s_mapTex = NULL;
			s_fontTex = NULL;
			s_texTried = false;
			for (size_t i = 0; i < s_logos.size(); i++) {
				s_logos[i].tex = NULL;
				s_logos[i].texTried = false;
			}
		}
		if (s_texTried) {
			return;
		}
		s_texTried = true;
		// the picture, from a smaller level down if the card will not take the largest
		const uint32_t levels = (uint32_t)s_map.levels.size();
		for (uint32_t skip = 0; skip < levels && !s_mapTex; skip++) {
			const WorldMap::Level& top = s_map.levels[skip];
			IDirect3DTexture9* t = NULL;
			if (FAILED(dev->CreateTexture(top.w, top.h, levels - skip, 0, D3DFMT_DXT1, D3DPOOL_MANAGED, &t, NULL)) || !t) {
				continue;
			}
			bool ok = true;
			for (uint32_t i = skip; i < levels && ok; i++) {
				const WorldMap::Level& lv = s_map.levels[i];
				D3DLOCKED_RECT r;
				if (FAILED(t->LockRect(i - skip, &r, NULL, 0))) {
					ok = false;
					break;
				}
				const uint32_t across = ((lv.w + 3) / 4 ? (lv.w + 3) / 4 : 1) * 8, rows = (lv.h + 3) / 4 ? (lv.h + 3) / 4 : 1;
				for (uint32_t y = 0; y < rows; y++) {
					memcpy((uint8_t*)r.pBits + (size_t)y * r.Pitch, lv.data + (size_t)y * across, across);
				}
				t->UnlockRect(i - skip);
			}
			if (!ok) {
				t->Release();
				continue;
			}
			s_mapTex = t;
			Log::TypedLog(CHN_SDL, "Map: the picture is a %ux%u texture%s\n", top.w, top.h, skip ? " (the card would not take the largest)" : "");
		}
		if (!s_mapTex) {
			Log::TypedLog(CHN_SDL, "Map: the card would not take the picture; the map shows only its spots\n");
		}
		s_mapU = (float)s_map.imgW / s_map.texW;
		s_mapV = (float)s_map.imgH / s_map.texH;
		// the font, with one level down for small text
		IDirect3DTexture9* f = NULL;
		if (SUCCEEDED(dev->CreateTexture(s_map.fontW, s_map.fontH, 2, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &f, NULL)) && f) {
			D3DLOCKED_RECT r;
			if (SUCCEEDED(f->LockRect(0, &r, NULL, 0))) {
				for (uint32_t y = 0; y < s_map.fontH; y++) {
					memcpy((uint8_t*)r.pBits + (size_t)y * r.Pitch, s_map.font + (size_t)y * s_map.fontW * 4, s_map.fontW * 4);
				}
				f->UnlockRect(0);
			}
			if (SUCCEEDED(f->LockRect(1, &r, NULL, 0))) {
				const uint32_t w = s_map.fontW / 2 ? s_map.fontW / 2 : 1, h = s_map.fontH / 2 ? s_map.fontH / 2 : 1;
				for (uint32_t y = 0; y < h; y++) {
					uint8_t* out = (uint8_t*)r.pBits + (size_t)y * r.Pitch;
					for (uint32_t x = 0; x < w; x++) {
						int a = 0;
						for (int k = 0; k < 4; k++) {
							const uint32_t sx = (2 * x + (k & 1)) % s_map.fontW, sy = (2 * y + (k >> 1)) % s_map.fontH;
							a += s_map.font[((size_t)sy * s_map.fontW + sx) * 4 + 3];
						}
						out[4 * x + 0] = out[4 * x + 1] = out[4 * x + 2] = 255;
						out[4 * x + 3] = (uint8_t)((a + 2) / 4);
					}
				}
				f->UnlockRect(1);
			}
			s_fontTex = f;
		}
		else {
			Log::TypedLog(CHN_SDL, "Map: the card would not take the font; the map has no names on it\n");
		}
	}

	// A logo's texture, made the first time it is drawn: MANAGED, as the others
	static IDirect3DTexture9* LogoTexture(IDirect3DDevice9* dev, Logo& l) {
		if (l.tex || l.texTried || !l.picture.w) {
			return l.tex;
		}
		l.texTried = true;
		IDirect3DTexture9* t = NULL;
		if (FAILED(dev->CreateTexture(l.picture.w, l.picture.h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, NULL)) || !t) {
			Log::TypedLog(CHN_SDL, "Map: the card would not take the logo of %s\n", l.company.c_str());
			return NULL;
		}
		D3DLOCKED_RECT r;
		if (FAILED(t->LockRect(0, &r, NULL, 0))) {
			t->Release();
			return NULL;
		}
		for (uint32_t y = 0; y < l.picture.h; y++) {
			memcpy((uint8_t*)r.pBits + (size_t)y * r.Pitch, &l.picture.bgra[(size_t)y * l.picture.w * 4], (size_t)l.picture.w * 4);
		}
		t->UnlockRect(0);
		l.tex = t;
		return t;
	}

	static void SetStates(IDirect3DDevice9* dev, UINT w, UINT h) {
		D3DVIEWPORT9 vp = { 0, 0, w, h, 0.0f, 1.0f };
		dev->SetViewport(&vp);
		dev->SetVertexShader(NULL);
		dev->SetPixelShader(NULL);
		dev->SetFVF(kFvf);
		dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
		dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		dev->SetRenderState(D3DRS_LIGHTING, FALSE);
		dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
		dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		dev->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
		dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
		dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
		dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		dev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
		dev->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
		dev->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
		dev->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
		dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		dev->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		dev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
		dev->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0);
		dev->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, 0);
		dev->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);
	}

	struct Box {
		float x0, y0, x1, y1;
	};

	static bool Overlaps(const std::vector<Box>& boxes, const Box& b) {
		for (size_t i = 0; i < boxes.size(); i++) {
			if (b.x0 < boxes[i].x1 && b.x1 > boxes[i].x0 && b.y0 < boxes[i].y1 && b.y1 > boxes[i].y0) {
				return true;
			}
		}
		return false;
	}

	// The help line: what each button does, the buttons in a colour of their own
	static void Help(float y, float px) {
		static const char* const kPad[][2] = { { "Stick", "Move" }, { "A", "Travel" }, { "X", "Next spot" }, { "Y", "Mark where you are" }, { "LT / RT", "Zoom" }, { "B", "Close" } };
		static const char* const kPs[][2] = { { "Stick", "Move" }, { "Cross", "Travel" }, { "Square", "Next spot" }, { "Triangle", "Mark where you are" }, { "L2 / R2", "Zoom" }, { "Circle", "Close" } };
		static const char* const kKeys[][2] = { { "Arrows", "Move" }, { "Enter", "Travel" }, { "Tab", "Next spot" }, { "F", "Mark where you are" }, { "Q / E", "Zoom" }, { "Esc", "Close" } };
		const char* const(*items)[2] = s_keyboardLast ? kKeys : s_playStation ? kPs : kPad;
		const int n = 6;
		const float gap = px * 0.45f, space = px * 1.4f;
		float total = 0.0f;
		for (int i = 0; i < n; i++) {
			total += TextWidth(items[i][0], px) + gap + TextWidth(items[i][1], px) + (i + 1 < n ? space : 0.0f);
		}
		float x = s_screenW * 0.5f - total * 0.5f;
		for (int i = 0; i < n; i++) {
			Text(items[i][0], x, y, px, Rgba(255, 210, 120, 255), -1, 0.0f, false);
			x += TextWidth(items[i][0], px) + gap;
			Text(items[i][1], x, y, px, Rgba(235, 238, 245, 230), -1, 0.0f, false);
			x += TextWidth(items[i][1], px) + space;
		}
	}

	static void DrawMap(IDirect3DDevice9* dev) {
		const float W = (float)s_screenW, H = (float)s_screenH, ui = (float)s_ui;
		const double upp1080 = s_view.upp * s_screenH / 1080.0;
		// the world
		Quad(0, 0, W, H, Rgba(8, 10, 14, 255));
		Flush(dev, NULL);
		double ax, ay, bx, by;
		WorldMap::ToScreen(s_view, W, H, s_map.x0, s_map.z0, &ax, &ay);
		WorldMap::ToScreen(s_view, W, H, s_map.x1, s_map.z1, &bx, &by);
		if (s_mapTex) {
			Quad((float)ax, (float)ay, (float)bx, (float)by, Rgba(255, 255, 255, 255), 0, 0, s_mapU, s_mapV);
			Flush(dev, s_mapTex);
		}
		// the districts' names, from far out
		const double districtsShow = (upp1080 - 9.0) / 5.0;
		if (s_fontTex && districtsShow > 0.0) {
			for (size_t i = 0; i < s_districts.size(); i++) {
				if (s_districts[i].name == kMine) {
					continue;
				}
				double sx, sy;
				WorldMap::ToScreen(s_view, W, H, s_districts[i].x, s_districts[i].z, &sx, &sy);
				const float px = 46.0f * ui;
				Text(Upper(s_districts[i].name), (float)sx, (float)sy - px * 0.5f, px, Faded(Rgba(240, 242, 248, 255), districtsShow * 0.6), 0, 0.12f);
			}
			Flush(dev, s_fontTex);
		}
		// the spots
		std::vector<float> sxs(s_spots.size()), sys(s_spots.size());
		for (size_t i = 0; i < s_spots.size(); i++) {
			double sx, sy;
			WorldMap::ToScreen(s_view, W, H, s_spots[i].x, s_spots[i].z, &sx, &sy);
			sxs[i] = (float)sx;
			sys[i] = (float)sy;
		}
		for (int pass = 0; pass < 2; pass++) {
			for (size_t i = 0; i < s_spots.size(); i++) {
				const bool picked = (int)i == s_picked;
				if (picked != (pass == 1) || sxs[i] < -40 || sys[i] < -40 || sxs[i] > W + 40 || sys[i] > H + 40) {
					continue;
				}
				const float r = (picked ? 12.0f : 7.5f) * ui;
				Diamond(sxs[i], sys[i], r + 2.5f * ui, picked ? Rgba(255, 255, 255, 255) : Rgba(0, 0, 0, 200));
				Diamond(sxs[i], sys[i], r, DistrictColor(s_spots[i].district, 255));
			}
		}
		// the skater
		{
			double sx, sy;
			WorldMap::ToScreen(s_view, W, H, s_where.pos[0], s_where.pos[2], &sx, &sy);
			Arrow((float)sx, (float)sy, 17.0f * ui, s_where.heading, Rgba(0, 0, 0, 220));
			Arrow((float)sx, (float)sy, 13.0f * ui, s_where.heading, Rgba(255, 255, 255, 255));
		}
		// the cursor
		const D3DCOLOR cursor = s_picked >= 0 ? DistrictColor(s_spots[s_picked].district, 255) : Rgba(255, 255, 255, 200);
		Ring(W * 0.5f, H * 0.5f, 19.0f * ui, 23.0f * ui, 0.0, 6.2831853, Rgba(0, 0, 0, 140));
		Ring(W * 0.5f, H * 0.5f, 20.0f * ui, 22.0f * ui, 0.0, 6.2831853, cursor);
		Quad(W * 0.5f - 1.5f * ui, H * 0.5f - 1.5f * ui, W * 0.5f + 1.5f * ui, H * 0.5f + 1.5f * ui, cursor);
		// the bands for the words
		const float band = 58.0f * ui;
		Quad(0, H - band, W, H, Rgba(6, 8, 12, 215));
		Quad(0, 0, W, band, Rgba(6, 8, 12, 150));
		Flush(dev, NULL);
		if (!s_fontTex) {
			return;
		}
		// the spots' names, close in, the picked one always (first, so it is never left out)
		std::vector<Box> taken;
		const bool namesShow = upp1080 <= 8.0;
		for (int pass = 0; pass < 2; pass++) {
			for (size_t i = 0; i < s_spots.size(); i++) {
				const bool picked = (int)i == s_picked;
				if ((pass == 0) != picked || (!picked && !namesShow)) {
					continue;
				}
				const float px = 23.0f * ui;      // (the picked one's name is big in the card at the bottom)
				const float x = sxs[i] + (picked ? 20.0f : 13.0f) * ui, top = sys[i] - px * 0.55f;
				const Box b = { x - 4 * ui, top, x + TextWidth(s_spots[i].name, px) + 4 * ui, top + px };
				if (b.x1 < 0 || b.x0 > W || b.y1 < band || b.y0 > H - band || (!picked && Overlaps(taken, b))) {
					continue;
				}
				taken.push_back(b);
				Text(s_spots[i].name, x, top, px, picked ? Rgba(255, 255, 255, 255) : Rgba(235, 238, 245, 225), -1);
			}
		}
		// the words
		Text("MAP", 28.0f * ui, (band - 30.0f * ui) * 0.5f, 30.0f * ui, Rgba(255, 255, 255, 235), -1, 0.25f);
		{
			char count[48];
			Str::Format(count, sizeof(count), "%u spots", (unsigned)s_spots.size());
			Text(count, W - 28.0f * ui, (band - 22.0f * ui) * 0.5f, 22.0f * ui, Rgba(200, 205, 215, 200), 1);
		}
		if (s_picked >= 0) {
			const WorldMap::Spot& s = s_spots[s_picked];
			Text(Upper(s.district), W * 0.5f, H - band - 40.0f * ui, 20.0f * ui, DistrictColor(s.district, 255), 0, 0.15f);
			Text(s.name, W * 0.5f, H - band - 84.0f * ui, 40.0f * ui, Rgba(255, 255, 255, 255), 0);
		}
		if (!s_message.empty() && (int32_t)(s_messageUntil - GetTickCount()) > 0) {
			Text(s_message, W * 0.5f, band + 24.0f * ui, 26.0f * ui, Rgba(255, 210, 120, 255), 0);
		}
		Help(H - band + (band - 22.0f * ui) * 0.5f, 22.0f * ui);
		Flush(dev, s_fontTex);
	}

	// With the map closed: the ring of a hold of the map's button, and the name of where he went
	static void DrawOver(IDirect3DDevice9* dev) {
		const float W = (float)s_screenW, H = (float)s_screenH, ui = (float)s_ui;
		if (s_charge > 0.35) {
			const float cx = W - 70.0f * ui, cy = H - 70.0f * ui;
			const double k = (s_charge - 0.35) / 0.65;
			Ring(cx, cy, 24.0f * ui, 30.0f * ui, 0.0, 6.2831853, Rgba(0, 0, 0, (int)(130 * (0.4 + 0.6 * k))));
			Ring(cx, cy, 25.0f * ui, 29.0f * ui, -1.5707963, -1.5707963 + 6.2831853 * s_charge, Rgba(255, 255, 255, 235));
			Flush(dev, NULL);
			if (s_fontTex) {
				Text("MAP", cx + 1.0f * ui, cy - 9.0f * ui, 18.0f * ui, Rgba(255, 255, 255, 235), 0, 0.15f);
				Flush(dev, s_fontTex);
			}
		}
		const uint32_t age = GetTickCount() - s_toastAt;
		if (!s_toast.empty() && age < kToastMs && s_fontTex) {
			const double fade = age < 200 ? age / 200.0 : age > kToastMs - 500 ? (kToastMs - age) / 500.0 : 1.0;
			Text(s_toast, W * 0.5f, H * 0.16f, 44.0f * ui, Faded(Rgba(255, 255, 255, 255), fade), 0);
			Text(Upper(s_toastDistrict), W * 0.5f, H * 0.16f + 50.0f * ui, 22.0f * ui, Faded(DistrictColor(s_toastDistrict, 255), fade), 0, 0.15f);
			Flush(dev, s_fontTex);
		}
	}

	// A line of what the buttons do, centred on cx: the buttons in a colour of their own
	static void ButtonLine(const char* const (*items)[2], int n, float cx, float y, float px) {
		const float gap = px * 0.45f, space = px * 1.4f;
		float total = 0.0f;
		for (int i = 0; i < n; i++) {
			total += TextWidth(items[i][0], px) + gap + TextWidth(items[i][1], px) + (i + 1 < n ? space : 0.0f);
		}
		float x = cx - total * 0.5f;
		for (int i = 0; i < n; i++) {
			Text(items[i][0], x, y, px, Rgba(255, 210, 120, 255), -1);
			x += TextWidth(items[i][0], px) + gap;
			Text(items[i][1], x, y, px, Rgba(235, 238, 245, 230), -1);
			x += TextWidth(items[i][1], px) + space;
		}
	}

	// The card and the list sit at the right of the screen, out of the way of the skater in the middle: he
	// skates on while they are up
	static float OverlayLeft(float width) {
		const float W = (float)s_screenW, ui = (float)s_ui;
		const float x0 = floorf(W - 48.0f * ui - width);
		return x0 < 40.0f * ui ? 40.0f * ui : x0;
	}

	// The card of another player: who he is at the top (his sponsor level, his name, the companies he rides
	// for), his story and his stats in the middle, and what can be done with him below
	static void DrawCard(IDirect3DDevice9* dev) {
		const float W = (float)s_screenW, H = (float)s_screenH, ui = (float)s_ui;
		const Players::Identity& p = s_cardWho;
		const D3DCOLOR accent = SponsorColor(p.sponsor);
		const D3DCOLOR white = Rgba(255, 255, 255, 255), soft = Rgba(226, 230, 238, 235), dim = Rgba(146, 153, 167, 235);
		const D3DCOLOR line = Rgba(255, 255, 255, 34);
		const float cw = 640.0f * ui, ch = 896.0f * ui, pad = 40.0f * ui;
		const float x0 = OverlayLeft(cw);
		const float y0 = floorf((H - ch) * 0.5f - 16.0f * ui), x1 = x0 + cw, y1 = y0 + ch;
		const float L = x0 + pad, R = x1 - pad;
		const float d1 = y0 + 206.0f * ui, d2 = y0 + 306.0f * ui, d3 = y0 + 634.0f * ui;
		const float statsTop = y0 + 356.0f * ui, rowPitch = 54.0f * ui, colGap = 36.0f * ui;
		const float colW = (R - L - colGap) * 0.5f;
		const float menuTop = y0 + 650.0f * ui, itemPitch = 50.0f * ui, itemH = 46.0f * ui;
		const std::string level = Upper(Players::SponsorName(p.sponsor));
		const float badgePx = 17.0f * ui, badgeW = TextWidth(level, badgePx, 0.14f) + 22.0f * ui;

		// the game behind it a little dimmed (it goes on), the card (the game seen through it), its colour at
		// the top, the lines between its parts
		Quad(0, 0, W, H, Rgba(0, 0, 0, kDim));
		Quad(x0 - 1.0f * ui, y0 - 1.0f * ui, x1 + 1.0f * ui, y1 + 1.0f * ui, Rgba(255, 255, 255, 30));
		Quad(x0, y0, x1, y1, Rgba(12, 14, 20, kPanel));
		Quad(x0, y0, x1, y0 + 5.0f * ui, accent);
		Quad(L, d1, R, d1 + 1.0f * ui, line);
		Quad(L, d2, R, d2 + 1.0f * ui, line);
		Quad(L, d3, R, d3 + 1.0f * ui, line);
		// the badge of his sponsor level
		Quad(L, y0 + 30.0f * ui, L + badgeW, y0 + 58.0f * ui, accent);
		// the stats' bars: ten steps each
		const float stepGap = 3.0f * ui, stepW = (colW - 9.0f * stepGap) / 10.0f;
		for (int k = 0; k < Players::kStats; k++) {
			const float cx = L + (k / 5) * (colW + colGap), top = statsTop + (k % 5) * rowPitch + 28.0f * ui;
			for (int i = 0; i < 10; i++) {
				const float sx = cx + i * (stepW + stepGap);
				Quad(sx, top, sx + stepW, top + 8.0f * ui, i < p.stats[k] ? accent : Rgba(255, 255, 255, 30));
			}
		}
		// the pick in the menu
		{
			const float top = menuTop + s_cardPick * itemPitch;
			Quad(x0 + 16.0f * ui, top, x1 - 16.0f * ui, top + itemH, Rgba(255, 255, 255, 24));
			Quad(x0 + 16.0f * ui, top, x0 + 20.0f * ui, top + itemH, accent);
		}
		Flush(dev, NULL);
		// the logos of the companies he rides for, the board's and the shoes' (their names where there is none)
		const float logoTop = y0 + 132.0f * ui, logoW = 128.0f * ui, logoH = 64.0f * ui;
		const char* companies[2] = { p.board, p.shoes };
		bool logoDrawn[2] = { false, false };
		for (int k = 0; k < 2 && p.sponsor != Players::kUnsponsored; k++) {
			Logo* l = FindLogo(companies[k]);
			IDirect3DTexture9* t = l ? LogoTexture(dev, *l) : NULL;
			if (t) {
				const float lx = k == 0 ? L : L + colW + colGap;
				Quad(lx, logoTop, lx + logoW, logoTop + logoH, Rgba(255, 255, 255, 255), 0.0f, 0.0f, 1.0f, 1.0f);
				Flush(dev, t);
				logoDrawn[k] = true;
			}
		}
		if (!s_fontTex) {
			return;
		}

		// who he is
		Text(level, L + 11.0f * ui, y0 + 35.0f * ui, badgePx, Rgba(12, 14, 20, 255), -1, 0.14f, false);
		Text("TEST PLAYER", R, y0 + 38.0f * ui, 15.0f * ui, dim, 1, 0.16f, false);
		{
			float px = 52.0f * ui;
			const float room = R - L;
			const float w = TextWidth(p.name, px);
			px = w > room ? px * room / w : px;
			Text(p.name, L, y0 + 70.0f * ui, px, white, -1);
		}
		if (p.sponsor == Players::kUnsponsored) {
			Text("No sponsors yet", L, y0 + 150.0f * ui, 25.0f * ui, dim, -1, 0.0f, false);
		}
		else {
			for (int k = 0; k < 2; k++) {
				if (!logoDrawn[k]) {
					Text(companies[k], k == 0 ? L : L + colW + colGap, y0 + 150.0f * ui, 27.0f * ui, soft, -1, 0.0f, false);
				}
			}
		}
		// his story
		{
			char goals[16];
			Str::Format(goals, sizeof(goals), "%d", p.goals);
			Text("STORY", L, y0 + 226.0f * ui, 15.0f * ui, dim, -1, 0.16f, false);
			Text("GOALS", R, y0 + 226.0f * ui, 15.0f * ui, dim, 1, 0.16f, false);
			Text(p.level, L, y0 + 248.0f * ui, 28.0f * ui, white, -1, 0.0f, false);
			Text(goals, R, y0 + 248.0f * ui, 28.0f * ui, white, 1, 0.0f, false);
		}
		// his stats
		Text("STATS", L, y0 + 326.0f * ui, 15.0f * ui, dim, -1, 0.16f, false);
		for (int k = 0; k < Players::kStats; k++) {
			const float cx = L + (k / 5) * (colW + colGap), top = statsTop + (k % 5) * rowPitch;
			char value[8];
			Str::Format(value, sizeof(value), "%d", p.stats[k]);
			Text(Players::StatName(k), cx, top, 19.0f * ui, soft, -1, 0.0f, false);
			Text(value, cx + colW, top, 19.0f * ui, white, 1, 0.0f, false);
		}
		// what can be done with him
		for (int i = 0; i < kCardItemCount; i++) {
			const float top = menuTop + i * itemPitch;
			const bool picked = i == s_cardPick;
			const char* text = i == kCardItemCount - 1 && s_cardFromList ? "Back to the list" : kCardItems[i].text;
			Text(text, L, top + 9.0f * ui, 27.0f * ui, picked ? white : Rgba(220, 224, 232, 185), -1, 0.0f, false);
			if (!kCardItems[i].built) {
				Text("SOON", R, top + 16.0f * ui, 14.0f * ui, Rgba(255, 210, 120, picked ? 220 : 150), 1, 0.16f, false);
			}
		}
		if (!s_cardMessage.empty() && (int32_t)(s_cardMessageUntil - GetTickCount()) > 0) {
			Text(s_cardMessage, (x0 + x1) * 0.5f, y0 + 858.0f * ui, 20.0f * ui, Rgba(255, 210, 120, 255), 0, 0.0f, false);
		}
		// the buttons, under the card
		static const char* const kPad[][2] = { { "D-pad up / down", "Choose" }, { "Right", "Select" }, { "Left", "Close" } };
		static const char* const kKeys[][2] = { { "Up / Down", "Choose" }, { "Right", "Select" }, { "Left", "Close" } };
		static const char* const kPadBack[][2] = { { "D-pad up / down", "Choose" }, { "Right", "Select" }, { "Left", "Back" } };
		static const char* const kKeysBack[][2] = { { "Up / Down", "Choose" }, { "Right", "Select" }, { "Left", "Back" } };
		const char* const(*items)[2] = s_cardFromList ? (s_keyboardLast ? kKeysBack : kPadBack) : (s_keyboardLast ? kKeys : kPad);
		ButtonLine(items, 3, (x0 + x1) * 0.5f, y1 + 18.0f * ui, 21.0f * ui);
		Flush(dev, s_fontTex);
	}

	// The players there, the nearest first: his sponsor level's colour, his name, how far he is and which way
	// (an arrow, up where the camera looks)
	static void DrawList(IDirect3DDevice9* dev) {
		const float W = (float)s_screenW, H = (float)s_screenH, ui = (float)s_ui;
		const int n = (int)s_listRows.size();
		const float cw = 560.0f * ui, pad = 36.0f * ui, rowH = 78.0f * ui, headH = 104.0f * ui;
		const float ch = headH + (n ? n : 1) * rowH + 22.0f * ui;
		const float x0 = OverlayLeft(cw);
		const float y0 = floorf((H - ch) * 0.5f - 16.0f * ui), x1 = x0 + cw, y1 = y0 + ch;
		const float L = x0 + pad, R = x1 - pad;
		const D3DCOLOR white = Rgba(255, 255, 255, 255), dim = Rgba(146, 153, 167, 235), gold = Rgba(255, 210, 120, 255);
		Quad(0, 0, W, H, Rgba(0, 0, 0, kDim));
		Quad(x0 - 1.0f * ui, y0 - 1.0f * ui, x1 + 1.0f * ui, y1 + 1.0f * ui, Rgba(255, 255, 255, 30));
		Quad(x0, y0, x1, y1, Rgba(12, 14, 20, kPanel));
		Quad(x0, y0, x1, y0 + 5.0f * ui, gold);
		Quad(L, y0 + headH - 12.0f * ui, R, y0 + headH - 11.0f * ui, Rgba(255, 255, 255, 34));
		for (int i = 0; i < n; i++) {
			const PlayersGame::Listed& r = s_listRows[i];
			const float top = y0 + headH + i * rowH, mid = top + rowH * 0.5f;
			if (i == s_listPick) {
				Quad(x0 + 16.0f * ui, top + 4.0f * ui, x1 - 16.0f * ui, top + rowH - 4.0f * ui, Rgba(255, 255, 255, 24));
				Quad(x0 + 16.0f * ui, top + 4.0f * ui, x0 + 20.0f * ui, top + rowH - 4.0f * ui, gold);
			}
			Diamond(L + 7.0f * ui, mid, 7.0f * ui, SponsorColor(r.who.sponsor));
			Arrow(R - 14.0f * ui, mid, 13.0f * ui, 3.14159265 - r.bearing, Rgba(235, 238, 245, 235));
		}
		Flush(dev, NULL);
		if (!s_fontTex) {
			return;
		}
		char count[32];
		Str::Format(count, sizeof(count), "%d HERE", n);
		Text("PLAYERS", L, y0 + 34.0f * ui, 30.0f * ui, white, -1, 0.06f, false);
		Text(count, R, y0 + 44.0f * ui, 15.0f * ui, dim, 1, 0.16f, false);
		if (!n) {
			Text("No other players here", L, y0 + headH + 22.0f * ui, 25.0f * ui, dim, -1, 0.0f, false);
		}
		for (int i = 0; i < n; i++) {
			const PlayersGame::Listed& r = s_listRows[i];
			const float top = y0 + headH + i * rowH;
			const bool picked = i == s_listPick;
			float px = 27.0f * ui;
			const float room = R - L - 150.0f * ui;
			const float w = TextWidth(r.who.name, px);
			px = w > room ? px * room / w : px;
			Text(r.who.name, L + 26.0f * ui, top + 12.0f * ui, px, picked ? white : Rgba(226, 230, 238, 220), -1, 0.0f, false);
			std::string about = Upper(Players::SponsorName(r.who.sponsor));
			if (r.who.sponsor != Players::kUnsponsored && r.who.board[0]) {
				about += std::string("  ") + r.who.board;
			}
			Text(about, L + 26.0f * ui, top + 46.0f * ui, 15.0f * ui, dim, -1, 0.1f, false);
			char away[24];
			Str::Format(away, sizeof(away), "%.0f m", r.away * 0.0254f);
			Text(away, R - 40.0f * ui, top + 26.0f * ui, 23.0f * ui, picked ? white : Rgba(226, 230, 238, 220), 1, 0.0f, false);
		}
		static const char* const kPad[][2] = { { "D-pad up / down", "Choose" }, { "Right", "View card" }, { "Left", "Close" } };
		static const char* const kKeys[][2] = { { "Up / Down", "Choose" }, { "Right", "View card" }, { "Left", "Close" } };
		ButtonLine(s_keyboardLast ? kKeys : kPad, 3, (x0 + x1) * 0.5f, y1 + 18.0f * ui, 21.0f * ui);
		Flush(dev, s_fontTex);
	}

	static void __cdecl DrawFrameEnd() {
		if (!s_ready) {
			return;
		}
		const bool toast = !s_toast.empty() && GetTickCount() - s_toastAt < kToastMs;
		if (!s_open && !s_cardOpen && !s_listOpen && s_charge <= 0.35 && !toast) {
			return;
		}
		IDirect3DDevice9* dev = *kDevice;
		if (!dev) {
			return;
		}
		IDirect3DSurface9* rt = NULL;
		D3DSURFACE_DESC desc;
		if (FAILED(dev->GetRenderTarget(0, &rt)) || !rt) {
			return;
		}
		const bool described = SUCCEEDED(rt->GetDesc(&desc));
		rt->Release();
		if (!described || desc.Width == 0 || desc.Height == 0) {
			return;
		}
		s_screenW = (int)desc.Width;
		s_screenH = (int)desc.Height;
		s_ui = s_screenH / 1080.0;
		IDirect3DStateBlock9* saved = NULL;
		if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &saved)) || !saved) {
			return;
		}
		MakeTextures(dev);
		SetStates(dev, desc.Width, desc.Height);
		s_tris.clear();
		if (s_open) {
			DrawMap(dev);
		}
		else if (s_cardOpen) {
			DrawCard(dev);
		}
		else if (s_listOpen) {
			DrawList(dev);
		}
		else {
			DrawOver(dev);
		}
		s_tris.clear();
		saved->Apply();
		saved->Release();
	}

	// In the place of "mov eax, [device]" before the game's EndScene: draws, then does that
	static __declspec(naked) void FrameEndStub() {
		__asm {
			pushad
			call DrawFrameEnd
			popad
			mov eax, dword ptr ds:[0x007CEC6C]
			ret
		}
	}

	/******************************************************************/
	/*  Putting it in                                                 */
	/******************************************************************/

	void Configure(const char* configFile) {
		const bool on = GetPrivateProfileIntA(FLICKIT_SECTION, "Map", 1, configFile) != 0;
		s_button = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "MapButton", SDL_CONTROLLER_BUTTON_BACK, configFile);
		const int hold = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "MapHoldMs", 350, configFile);
		s_gate.SetHold((uint32_t)(hold < 0 ? 0 : hold > 3000 ? 3000 : hold));
		s_key = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "MapKey", SDL_SCANCODE_M, configFile);
		s_listButton = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "PlayersButton", SDL_CONTROLLER_BUTTON_DPAD_DOWN, configFile);
		s_listKey = (int)GetPrivateProfileIntA(FLICKIT_SECTION, "PlayersKey", SDL_SCANCODE_P, configFile);
		if (s_open) {
			Close(true, false);
		}
		if (s_cardOpen) {
			const Dpad none = { false, false, false, false };
			s_cardFromList = false;
			CloseCard(false, none);
		}
		if (s_listOpen) {
			CloseList();
		}
		s_cardAsked = false;
		s_ready = false;
		if (!on) {
			Log::TypedLog(CHN_SDL, "Map: off ([Flickit] Map=0)\n");
			return;
		}
		if (!s_checked) {
			s_checked = true;
			s_codeOk = Game::Ready();
			if (!s_codeOk) {
				Game::Note("Map: the game's code is not what was expected; the map is off\n");
			}
			for (size_t i = 0; i < sizeof(kExpect) / sizeof(kExpect[0]); i++) {
				if (!Game::Matches(kExpect[i])) {
					Game::Note("Map: the game's code at %08X (%s) is not what was expected; the map is off\n", kExpect[i].address, kExpect[i].what);
					s_codeOk = false;
				}
			}
		}
		if (!s_codeOk) {
			return;
		}
		if (!FlickitGame::Installed()) {
			Log::TypedLog(CHN_SDL, "Map: off; it needs Flickit's hooks in the game ([Flickit] Enabled=1 and GameHooks=1)\n");
			return;
		}
		if (s_file.empty()) {
			std::string why;
			if (!ReadWhole(Path("flickit_map.bin").c_str(), &s_file) || s_file.empty()) {
				s_file.clear();
				Log::TypedLog(CHN_SDL, "Map: off; there is no flickit_map.bin next to the game\n");
				return;
			}
			if (!WorldMap::ParseMap(&s_file[0], s_file.size(), &s_map, &why)) {
				s_file.clear();
				Log::TypedLog(CHN_SDL, "Map: off; flickit_map.bin: %s\n", why.c_str());
				return;
			}
			Log::TypedLog(CHN_SDL, "Map: read flickit_map.bin, a picture of %.0f..%.0f across and %.0f..%.0f down, for the level %08X\n",
				s_map.x0, s_map.x1, s_map.z0, s_map.z1, s_map.level);
		}
		ReadSpots();
		if (!s_patched) {
			patchCall((void*)kFrameEnd, (void*)FrameEndStub);
			s_patched = true;
		}
		s_ready = true;
		Log::TypedLog(CHN_SDL, "Map: on; press controller button %d, or key %d (where the camera is on that button too: held for %d ms, a tap is still the camera's)\n",
			s_button, s_key, hold);
	}
}
