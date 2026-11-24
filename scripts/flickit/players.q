// Flickit's players: others skating in the same world, for the online mode to come. For now they are fake:
// runs he records himself (F6, [Flickit] RunKey), each played back by a skater of the game's with a made-up
// name, sponsors, story and stats. Skate up to one and Circle shows his card.
//
// src/players_game.cpp records the runs, moves the players and plays their animations; these scripts make
// them and put the words on the screen the game's own way. Compiled into src/flickit_players_qb.h by
// scripts/flickit/qbc.py; the game reads it once, in the first level.

// A player, made: a pedestrian of the level on a board (Ped_Skateboard_a, profile_ped.q: his head, body and
// legs picked by the game from the level's own), with the skater's skeleton and animations, the way the goal
// editor makes its people (goal_editor.q), and his name over his head the way a net game has a skater's (its
// FloatingLabel, which moves the label with him every frame and takes it away with him). From
// src/players_game.cpp: id, label (the name of the label's screen element), name, pos.
SCRIPT Flickit_PlayerMake
	IF ObjectExists id = <id>
		<id> : Die
	ENDIF
	// the label made here first, in Flickit's colours: the FloatingLabel only makes one (net.q's, yellow)
	// if there is none
	IF ObjectExists id = <label>
		DestroyScreenElement id = <label>
	ENDIF
	SetScreenElementLock id = root_window OFF
	CreateScreenElement {
		type = textElement
		parent = root_window
		id = <label>
		font = small
		text = ""
		scale = 0.85
		pos3D = <pos>
		rgba = [ 128 128 128 108 ]
		z_priority = -10
	}
	CreateCompositeObject {
		components = [
			{ component = suspend NeverSuspend }
			{ component = skeleton skeletonName = THPS5_human }
			{ component = model }
			{ component = animation AnimName = animload_THPS5_human }
			{ component = shadow ShadowType = simple }
			{ component = FloatingLabel string = <name> color = 0 y_offset = 108 screen_element_id = <label> }
		]
		params = { name = <id> pos = <pos> }
	}
	<id> : Obj_InitModelFromProfile struct = Ped_Skateboard_a use_asset_manager = 1 buildscript = create_ped_model_from_appearance
ENDSCRIPT

// A player gone (his label goes with him)
SCRIPT Flickit_PlayerKill
	IF ObjectExists id = <id>
		<id> : Die
	ENDIF
ENDSCRIPT

// His label shown or not: far players have none on the screen
SCRIPT Flickit_PlayerLabelShow
	IF ObjectExists id = <label>
		IF GotParam show
			SetScreenElementProps id = <label> unhide
		ELSE
			SetScreenElementProps id = <label> hide
		ENDIF
	ENDIF
ENDSCRIPT

// The prompt while a player is in reach (src/players_game.cpp says when, and for whom), in the box of the
// game's own prompts ("Press Circle to drive."). Only the box: the DLL reads Circle itself.
SCRIPT Flickit_PlayerPrompt
	Flickit_PlayerPromptOff
	IF ObjectExists id = goal_start_dialog
		RETURN
	ENDIF
	FormatText TextName = text "Press \\m5 to view %s." s = <name>
	create_speech_box {
		anchor_id = flickit_player_prompt
		text = <text>
		no_pad_choose
		no_pad_start
		bg_rgba = [ 100 100 100 128 ]
		text_rgba = [ 128 128 128 128 ]
		font = small
		z_priority = 5
	}
ENDSCRIPT

SCRIPT Flickit_PlayerPromptOff
	IF ObjectExists id = flickit_player_prompt
		DestroyScreenElement id = flickit_player_prompt
	ENDIF
ENDSCRIPT

// A line for a moment, the game's own way (the box the car's buttons are shown in)
SCRIPT Flickit_PlayersSay
	create_panel_block id = flickit_players_say text = <text> pos = PAIR(320, 92) dims = PAIR(300, 0) rgba = [ 128 128 128 100 ] time = <time>
ENDSCRIPT

// The card of a player as words, where Flickit's own card cannot be drawn (it takes the map's letters)
SCRIPT Flickit_PlayerCardWords
	create_panel_block id = flickit_players_say text = <text> pos = PAIR(320, 150) dims = PAIR(360, 0) rgba = [ 128 128 128 110 ] time = 7000
ENDSCRIPT

// While a run is recorded: REC and its time, at the top right
SCRIPT Flickit_RunRec
	IF ObjectExists id = flickit_run_rec
		SetScreenElementProps id = flickit_run_rec text = <text>
	ELSE
		SetScreenElementLock id = root_window OFF
		CreateScreenElement {
			type = textElement
			parent = root_window
			id = flickit_run_rec
			font = small
			text = <text>
			pos = PAIR(620, 22)
			just = [ right top ]
			rgba = [ 128 26 26 118 ]
			scale = 0.9
			z_priority = 6
		}
	ENDIF
ENDSCRIPT

SCRIPT Flickit_RunRecOff
	IF ObjectExists id = flickit_run_rec
		DestroyScreenElement id = flickit_run_rec
	ENDIF
ENDSCRIPT
