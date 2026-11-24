// Flickit's day and night. src/daynight_game.cpp keeps the clock and works out the look of the hour
// (src/daynight.h); these scripts give it to the level the way the game's own time of day does (timeofday.q:
// script_change_tod, dynamic_tod and set_tod_from_params; Levels.q: set_level_lights, without its fog).
//
// Compiled into src/flickit_daynight_qb.h by scripts/flickit/qbc.py; the game reads it once, in the first level.

// The look of the hour, once a second. From the DLL: scene (1: the colours too), color and sky (the level's
// geometry and its sky, red + green * 256 + blue * 65536, 128 each as the level is), and the lights the skater,
// the cars and the people are lit by (set_level_lights' own parameters).
SCRIPT Flickit_DayNightLook
	// the game's own time of day, going from one look to another over a few seconds (the story's chapters
	// start one), would undo this one
	KillSpawnedScript name = dynamic_tod
	IF ( <scene> = 1 )
		// the sectors of the light groups the game's own time of day colours (outdoor, every sector that has
		// no other, with the sky; nolevellights), and indoor ones as they are (the game's own neutral). Sectors
		// of a light group of their own (a lit window) are as the level has them.
		SetSceneColor color = <color> sky = <sky> lightgroup = outdoor
		SetSceneColor color = <color> lightgroup = nolevellights
		SetSceneColor color = 8421505 lightgroup = indoor
	ENDIF
	SetLightAmbientColor r = <ambient_red> g = <ambient_green> b = <ambient_blue>
	SetLightDirection index = 0 heading = <heading_0> pitch = <pitch_0>
	SetLightDiffuseColor index = 0 r = <red_0> g = <green_0> b = <blue_0>
	SetLightDirection index = 1 heading = <heading_1> pitch = <pitch_1>
	SetLightDiffuseColor index = 1 r = <red_1> g = <green_1> b = <blue_1>
	SetDynamicLightModulationFactor ambient value = <ambient_mod_factor>
	SetDynamicLightModulationFactor directional = 0 value = <mod_factor_0>
	SetDynamicLightModulationFactor directional = 1 value = <mod_factor_1>
ENDSCRIPT

// The level's own night, on or off: its lights for the night (TRG_levellight...) and the evening
// (TRG_Evening_levellight...), its lit windows and lamps (NightOn...; DefaultOn... by day), and its sounds for the
// night (TRG_SFX_TrigBox_Night_...; TRG_SFX_TrigBox_Day_... by day), as script_set_level_tod has them. The game
// has one FakeLights at a time (a second one ends the first), so the two go one after the other. From the DLL:
// night, evening (1 on, 0 off), time (the frames the lights take to come on or go off, one after another: 0 at
// once), gap (time + 1).
SCRIPT Flickit_DayNightLights
	KillSpawnedScript name = Flickit_DayNightLightsGo
	SpawnScript Flickit_DayNightLightsGo params = { <...> }
ENDSCRIPT

SCRIPT Flickit_DayNightLightsGo
	IF ( <night> = 1 )
		FakeLights percent = 100 time = <time> prefix = TRG_levellight
		Kill prefix = "DefaultOn"
		Create prefix = "NightOn"
		Kill prefix = "TRG_SFX_TrigBox_Day_"
		Kill prefix = "TRG_SFX_TrigBox_Night_"
		Create prefix = "TRG_SFX_TrigBox_Night_"
	ELSE
		FakeLights percent = 0 time = <time> prefix = TRG_levellight
		Kill prefix = "NightOn"
		Create prefix = "DefaultOn"
		Kill prefix = "TRG_SFX_TrigBox_Day_"
		Kill prefix = "TRG_SFX_TrigBox_Night_"
		Create prefix = "TRG_SFX_TrigBox_Day_"
	ENDIF
	Wait <gap> GameFrames
	IF ( <evening> = 1 )
		FakeLights percent = 100 time = <time> prefix = TRG_Evening_levellight
	ELSE
		FakeLights percent = 0 time = <time> prefix = TRG_Evening_levellight
	ENDIF
ENDSCRIPT
