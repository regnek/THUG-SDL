// Flickit's players: others skating in the same world, for the online mode to come. For now they are fake:
// runs he records himself (F6, [Flickit] RunKey), each played back by a skater of the game's with a made-up
// name, sponsors, story and stats. Skate up to one and Circle shows his card.
//
// src/Flickit/players_game.cpp records the runs, moves the players and plays their animations; these scripts make
// them and put the words on the screen the game's own way. Compiled into src/Flickit/flickit_players_qb.h by
// scripts/flickit/qbc.py; the game reads it once, in the first level.

// A player, made: one of the level's people on a board (Ped_Skateboard_a, profile_ped.q: his head, body and legs
// picked by the game from the level's own), with the skater's skeleton and animations, made the way the goal editor
// makes its people (goal_editor.q: its components, the model from a profile, Obj_SetPosition), but for its motion
// component (the DLL moves him; setdisplaymatrix draws him turned the way the DLL turns him), idling until the DLL
// takes him on. src/Flickit/players_game.cpp spawns it, so that it runs with the game's other scripts and not in the middle
// of the skater's update. From the DLL: id (a name of his own, another each time he is made), label (the name of
// his label's screen element), name, pos.
SCRIPT Flickit_PlayerSpawn
	SpawnScript Flickit_PlayerMake params = { <...> }
ENDSCRIPT

SCRIPT Flickit_PlayerMake
	CreateCompositeObject {
		components = [
			{ component = suspend NeverSuspend }
			{ component = skeleton skeletonName = THPS5_human }
			{ component = setdisplaymatrix }
			{ component = model }
			{ component = animation AnimName = animload_THPS5_human }
		]
		params = { name = <id> pos = <pos> }
	}
	<id> : Obj_SetPosition Position = <pos>
	<id> : Obj_InitModelFromProfile struct = Ped_Skateboard_a use_asset_manager = 1 texDictOffset = 0
	<id> : Obj_PlayAnim Anim = Idle Cycle
	// his name over his head, moved with him by the DLL (Flickit_PlayerLabelAt)
	IF ObjectExists id = <label>
		DestroyScreenElement id = <label>
	ENDIF
	SetScreenElementLock id = root_window OFF
	CreateScreenElement {
		type = textElement
		parent = root_window
		id = <label>
		font = small
		text = <name>
		scale = 0.85
		pos3D = <pos>
		rgba = [ 128 128 128 108 ]
		z_priority = -10
	}
ENDSCRIPT

// A player gone, and his label
SCRIPT Flickit_PlayerKill
	IF ObjectExists id = <id>
		<id> : Die
	ENDIF
	IF ObjectExists id = <label>
		DestroyScreenElement id = <label>
	ENDIF
ENDSCRIPT

// His label where he is (pos3D: the game puts it on the screen there), or shown or not: far players have none
SCRIPT Flickit_PlayerLabelAt
	IF ObjectExists id = <label>
		SetScreenElementProps id = <label> pos3D = <pos>
	ENDIF
ENDSCRIPT

SCRIPT Flickit_PlayerLabelShow
	IF ObjectExists id = <label>
		IF GotParam show
			SetScreenElementProps id = <label> unhide
		ELSE
			SetScreenElementProps id = <label> hide
		ENDIF
	ENDIF
ENDSCRIPT

// The name of the player Circle is for, lit; without picked, as the others
SCRIPT Flickit_PlayerLabelPick
	IF ObjectExists id = <label>
		IF GotParam picked
			SetScreenElementProps id = <label> rgba = [ 128 100 30 128 ]
		ELSE
			SetScreenElementProps id = <label> rgba = [ 128 128 128 108 ]
		ENDIF
	ENDIF
ENDSCRIPT

// The prompt while a player is in reach (src/Flickit/players_game.cpp says when, and for whom), in the box of the
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
