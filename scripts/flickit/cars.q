// Flickit's cars: any car in the level can be taken, a moving one too, by walking or skating up to it and
// pressing Circle (the button the game gets into its own drivable cars with), and it stays where he gets out,
// for two minutes (CarParkedTime): then it goes back, and the car it was is at the start of its way again.
//
// src/drive_game.cpp finds the car in reach, starts Flickit_CarSteal, and answers the Flickit_Car... commands
// below (the car's place and speed, the place to get out at). These scripts do the rest the game's own way:
// the car is the game's own drivable car (PlayerVehicle, player_controlled_vehicle.q) with the model of the
// car taken, and the way in and out is the game's (Switch_OnGroundAI, ToggleUserControlledVehicleMode).
//
// Compiled into src/flickit_cars_qb.h by scripts/flickit/qbc.py; the game reads it once, in the first level.

// The setup of the game's drivable car that suits each model, and for a model that has no setup of its own
// the size of its body and wheels (measured from the model: the base and the roof of the body for the
// colliders, as the game's own setups have them, and the radius of the wheels). The rest is the setup's.
Flickit_Car_Default = { setup = PoliceCarSetup }
Flickit_Car_veh_custom = { setup = CustomCarSetup }
Flickit_Car_veh_policecar_fl = { setup = PoliceCarSetup }
Flickit_Car_veh_sdkart_security = { setup = SecurityCartSetup }
Flickit_Car_veh_sdkart_gardens = { setup = GardnersCartSetup }
Flickit_Car_veh_transam = { setup = TransCarSetup }
Flickit_Car_veh_chevy_impala = { setup = ImpalaCarSetup }
Flickit_Car_veh_camino = { setup = ElCaminoCarSetup }
Flickit_Car_veh_limo = { setup = LimoCarSetup }
Flickit_Car_veh_minibaja = { setup = MiniBajaCarSetup }
Flickit_Car_veh_lada = { setup = LadaCarSetup }
Flickit_Car_veh_policecar_nj = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(36, 26, 137) VECTOR(-36, 26, 137) VECTOR(36, 26, -75) ]
		[ VECTOR(26, 55, 57) VECTOR(-26, 55, 57) VECTOR(26, 55, -12) ]
	]
	all_wheels = { radius = 13.5 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_policecar_ny = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(36, 26, 149) VECTOR(-36, 26, 149) VECTOR(36, 26, -63) ]
		[ VECTOR(26, 55, 69) VECTOR(-26, 55, 69) VECTOR(26, 55, 0) ]
	]
	all_wheels = { radius = 13.5 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_cab_ny = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(36, 24, 149) VECTOR(-36, 24, 149) VECTOR(36, 24, -63) ]
		[ VECTOR(26, 51, 69) VECTOR(-26, 51, 69) VECTOR(26, 51, 0) ]
	]
	all_wheels = { radius = 13.5 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_taurustype = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(38, 28, 160) VECTOR(-38, 28, 160) VECTOR(38, 28, -45) ]
		[ VECTOR(28, 58, 83) VECTOR(-28, 58, 83) VECTOR(28, 58, 16) ]
	]
	all_wheels = { radius = 14.2 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_neontype = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(35, 26, 148) VECTOR(-35, 26, 148) VECTOR(35, 26, -33) ]
		[ VECTOR(26, 52, 80) VECTOR(-26, 52, 80) VECTOR(26, 52, 20) ]
	]
	all_wheels = { radius = 14.2 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_smallsportscar = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(32, 23, 122) VECTOR(-32, 23, 122) VECTOR(32, 23, -38) ]
		[ VECTOR(23, 46, 62) VECTOR(-23, 46, 62) VECTOR(23, 46, 9) ]
	]
	all_wheels = { radius = 12.5 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_minivan = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(42, 32, 147) VECTOR(-42, 32, 147) VECTOR(42, 32, -29) ]
		[ VECTOR(31, 66, 81) VECTOR(-31, 66, 81) VECTOR(31, 66, 23) ]
	]
	all_wheels = { radius = 12.1 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_suv_thug = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(37, 33, 146) VECTOR(-37, 33, 146) VECTOR(37, 33, -43) ]
		[ VECTOR(27, 60, 75) VECTOR(-27, 60, 75) VECTOR(27, 60, 13) ]
	]
	all_wheels = { radius = 15.6 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_suv_drg = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(48, 33, 163) VECTOR(-48, 33, 163) VECTOR(48, 33, -48) ]
		[ VECTOR(35, 71, 83) VECTOR(-35, 71, 83) VECTOR(35, 71, 14) ]
	]
	all_wheels = { radius = 19 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_suv_musk = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(48, 33, 163) VECTOR(-48, 33, 163) VECTOR(48, 33, -48) ]
		[ VECTOR(35, 71, 83) VECTOR(-35, 71, 83) VECTOR(35, 71, 14) ]
	]
	all_wheels = { radius = 19 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_pickup = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(52, 38, 186) VECTOR(-52, 38, 186) VECTOR(52, 38, -61) ]
		[ VECTOR(39, 69, 93) VECTOR(-39, 69, 93) VECTOR(39, 69, 12) ]
	]
	all_wheels = { radius = 18.1 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_dcshoetruck = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(50, 38, 205) VECTOR(-50, 38, 205) VECTOR(50, 38, -64) ]
		[ VECTOR(47, 142, 138) VECTOR(-47, 142, 138) VECTOR(47, 142, -53) ]
	]
	all_wheels = { radius = 19.8 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}
Flickit_Car_veh_slamcitytruck = {
	setup = PoliceCarSetup
	colliders = [
		[ VECTOR(50, 36, 205) VECTOR(-50, 36, 205) VECTOR(50, 36, -64) ]
		[ VECTOR(47, 140, 138) VECTOR(-47, 140, 138) VECTOR(47, 140, -53) ]
	]
	all_wheels = { radius = 19.8 moment = 3570 spring_rate = 200 damping_rate = 28 static_friction = 4.0 min_static_grip_velocity = 8 max_static_grip_velocity = 12 min_dynamic_grip_velocity = 30 handbrake_torque = 3000 brake_torque = 2000 }
}

// He takes the car in reach. From src/drive_game.cpp: car (its id), model, setup (the setup of the game's
// drivable car it gets), tune (the Flickit_Car_ structure for its model).
SCRIPT Flickit_CarSteal
	IF Skater : Driving
		RETURN
	ENDIF
	// where the car is and how fast it goes, now (and false if it has gone)
	IF NOT Flickit_CarTake car = <car>
		RETURN
	ENDIF
	// a car of the level's own to drive ("Press ... to drive") leaves its box on the screen otherwise, and so
	// would Flickit's own
	IF ObjectExists id = goal_start_dialog
		speech_box_exit anchor_id = goal_start_dialog
	ENDIF
	Flickit_CarPromptOff
	IF Skater : Skitching
		Skater : StopSkitch
		Skater : SetState Ground
	ENDIF
	<car> : Die
	MakeSkaterGoto Switch_OnGroundAI params = { NewScript = TransAm VehicleSetup = ( <setup> + <tune> + { model = <model> } ) Exitable }
	IF NOT ObjectExists id = PlayerVehicle
		RETURN
	ENDIF
	// where the car was, the way it faced; the camera higher and farther back for a big car (the game's own is
	// for a car of the usual size, and a box truck hides the road ahead)
	IF Flickit_CarPlace
		PlayerVehicleCamera : VehicleCamera_Reset offset_height = <camera_height> offset_distance = <camera_distance>
	ELSE
		PlayerVehicleCamera : VehicleCamera_Reset
	ENDIF
	SetActiveCamera id = PlayerVehicleCamera
	IF ( <speed> > 5 )
		PlayerVehicle : Vehicle_Kick linear = ( VECTOR(0, 0, 1) * <speed> ) angular = VECTOR(0, 0, 0)
	ENDIF
	KillSpawnedScript name = Flickit_CarWaitForExit
	SpawnScript Flickit_CarWaitForExit
	create_panel_block id = flickit_car_help text = "\\b3 = Accelerate\\n\\b1 = Brake/Reverse\\n\\bf = Handbrake\\nHold \\b0 = Stop and get out" rgba = [ 128 128 128 100 ] time = 5000
ENDSCRIPT

// The game's own prompt for a car to drive, as car_scripts.q shows it for a car of the level's own ("Press
// Circle to drive."), while a car is in reach (src/drive_game.cpp says when). Only the box: the DLL reads
// Circle itself. Not over a goal's or the level's own car's box.
SCRIPT Flickit_CarPrompt
	IF ObjectExists id = flickit_car_prompt
		RETURN
	ENDIF
	IF ObjectExists id = goal_start_dialog
		RETURN
	ENDIF
	create_speech_box {
		anchor_id = flickit_car_prompt
		text = "Press \\m5 to drive."
		no_pad_choose
		no_pad_start
		bg_rgba = [ 100 100 100 128 ]
		text_rgba = [ 128 128 128 128 ]
		font = small
		z_priority = 5
	}
ENDSCRIPT

// The prompt away (not speech_box_exit: that holds the jump button back for a moment)
SCRIPT Flickit_CarPromptOff
	IF ObjectExists id = flickit_car_prompt
		DestroyScreenElement id = flickit_car_prompt
	ENDIF
ENDSCRIPT

// Holding Triangle stops the car and gets him out once it is slow (the game's own), but not with a press
// he was already holding when he got in: that has to be let go first.
SCRIPT Flickit_CarWaitForExit
	Wait 0.5 Seconds
	BEGIN
		IF Flickit_CarButtonUp
			BREAK
		ENDIF
		Wait 1 GameFrame
	REPEAT
	WaitForEvent Type = ExitVehicleRequest
	Goto Flickit_CarExit
ENDSCRIPT

SCRIPT Flickit_CarExit
	IF NOT Skater : Driving
		RETURN
	ENDIF
	IF ObjectExists id = flickit_car_help
		DestroyScreenElement id = flickit_car_help
	ENDIF
	// what the car is and where it stands, the place beside it he gets out at, and the name it is left under
	IF NOT Flickit_CarLeave
		RETURN
	ENDIF
	IF ObjectExists id = <parked>
		<parked> : Die
	ENDIF
	// out of the car the game's own way (the way a goal's car is left), on his board
	MakeSkaterGoto Switch_OnGroundAI params = { NewScript = OnGroundAi }
	// him beside the car through the game's own restart, standing, as he is after a goal's car
	Flickit_CarOut
	MakeSkaterGoto HandBrake
	Flickit_CarMake parked = <parked> model = <model> pos = <pos>
	// the way the car faced, and on the list of cars to take
	Flickit_CarParked
ENDSCRIPT

// A car standing on its own (a car he left, a car knocked off its way): the game's way for a car of the level,
// but standing: its model's own nodes (the gap triggers on its roof look there: without them the game looked in
// the level's nodes, found none and crashed) and the rails on its roof, as the game gives a car of the level
// (its car factory, 0x00506099). Where it stands and how it is turned are src/drive_game.cpp's.
SCRIPT Flickit_CarMake
	CreateCompositeObject {
		components = [
			{ component = staticvehicle }
			{ component = skeleton skeletonName = car }
			{ component = setdisplaymatrix }
			{ component = model model = <model> UseModelLights }
			{ component = collision CollisionMode = geometry }
			{ component = nodearray model = <model> }
			{ component = railmanager }
			{ component = modellightupdate }
		]
		params = { name = <parked> pos = <pos> }
	}
ENDSCRIPT

// A car of the level knocked off its way in a crash (src/drive_game.cpp: car, parked, model, pos): it goes, and
// a car of the same model stands in its place under a name of its own, to tumble on from there as the hit
// left it (the DLL moves it), and to be taken like a car he left
SCRIPT Flickit_CarKnocked
	IF ObjectExists id = <car>
		<car> : Die
	ENDIF
	IF ObjectExists id = <parked>
		<parked> : Die
	ENDIF
	Flickit_CarMake parked = <parked> model = <model> pos = <pos>
ENDSCRIPT

// The damage a car shows (src/drive_game.cpp picks them): smoke from its engine, heavy smoke, fire, and the
// smoke of a car burnt out. Particles of the game's own kinds, with its own textures that are always there
// (startup.q), seen from far off. fx: the name of the effect (a new one each time: the game keeps particles by
// name, and one made under the name of one that died in the same frame left the dead one behind), car: what it
// is on, parts: one of the structures below, offset: where on the car (from its model's origin, along its left,
// up and the way it faces), pos: that place now. Each structure has LocalSpace and UseStartPosition, as the
// game's own fireball: its places are from the effect's object, which the lock moves with the car (without
// them the game takes them as places in the world, at its middle).
SCRIPT Flickit_CarFx
	IF ObjectExists id = <fx>
		<fx> : Die
	ENDIF
	IF NOT ObjectExists id = <car>
		RETURN
	ENDIF
	CreateCompositeObject {
		Components = skater_particle_composite_structure
		params = { name = <fx> pos = <pos> LocalSpace UseStartPosition NeverSuspend <parts> }
	}
	<fx> : Obj_LockToObject id = <car> <offset>
ENDSCRIPT

SCRIPT Flickit_CarFxOff
	IF ObjectExists id = <fx>
		<fx> : Die
	ENDIF
ENDSCRIPT

Flickit_FxSmoke = {
	type = NEWFLAT
	LocalSpace
	UseStartPosition
	texture = dt_generic_particle01
	blendmode = blend
	FixedAlpha = 128
	AlphaCutoff = 1
	MaxStreams = 2
	SuspendDistance = 0
	lod_dist1 = 3000
	lod_dist2 = 3500
	UseMidPoint
	UseColorMidTime
	StartPosition = VECTOR(0, 0, 0)
	BoxDimsStart = VECTOR(30, 6, 30)
	MidPosition = VECTOR(0, 45, 0)
	BoxDimsMid = VECTOR(30, 20, 30)
	EndPosition = VECTOR(0, 110, 0)
	BoxDimsEnd = VECTOR(60, 30, 60)
	EmitRate = 20
	Lifetime = 1.6
	MidPointPCT = 40
	StartRadius = 8
	MidRadius = 18
	EndRadius = 32
	StartRadiusSpread = 2
	MidRadiusSpread = 4
	EndRadiusSpread = 6
	StartRGB = [ 110 , 110 , 110 ]
	StartAlpha = 90
	ColorMidTime = 40
	MidRGB = [ 95 , 95 , 95 ]
	MidAlpha = 70
	EndRGB = [ 85 , 85 , 85 ]
	EndAlpha = 0
}
Flickit_FxSmokeHeavy = {
	type = NEWFLAT
	LocalSpace
	UseStartPosition
	texture = dt_generic_particle01
	blendmode = blend
	FixedAlpha = 128
	AlphaCutoff = 1
	MaxStreams = 2
	SuspendDistance = 0
	lod_dist1 = 3500
	lod_dist2 = 4000
	UseMidPoint
	UseColorMidTime
	StartPosition = VECTOR(0, 0, 0)
	BoxDimsStart = VECTOR(40, 8, 40)
	MidPosition = VECTOR(0, 70, 0)
	BoxDimsMid = VECTOR(50, 30, 50)
	EndPosition = VECTOR(0, 190, 0)
	BoxDimsEnd = VECTOR(90, 50, 90)
	EmitRate = 40
	Lifetime = 2.2
	MidPointPCT = 40
	StartRadius = 12
	MidRadius = 30
	EndRadius = 55
	StartRadiusSpread = 3
	MidRadiusSpread = 6
	EndRadiusSpread = 10
	StartRGB = [ 45 , 43 , 40 ]
	StartAlpha = 140
	ColorMidTime = 40
	MidRGB = [ 35 , 33 , 30 ]
	MidAlpha = 110
	EndRGB = [ 30 , 30 , 30 ]
	EndAlpha = 0
}
Flickit_FxSmokeBurnt = {
	type = NEWFLAT
	LocalSpace
	UseStartPosition
	texture = dt_generic_particle01
	blendmode = blend
	FixedAlpha = 128
	AlphaCutoff = 1
	MaxStreams = 2
	SuspendDistance = 0
	lod_dist1 = 3000
	lod_dist2 = 3500
	UseMidPoint
	UseColorMidTime
	StartPosition = VECTOR(0, 0, 0)
	BoxDimsStart = VECTOR(60, 10, 90)
	MidPosition = VECTOR(0, 60, 0)
	BoxDimsMid = VECTOR(50, 30, 50)
	EndPosition = VECTOR(0, 160, 0)
	BoxDimsEnd = VECTOR(80, 40, 80)
	EmitRate = 10
	Lifetime = 2.5
	MidPointPCT = 40
	StartRadius = 10
	MidRadius = 24
	EndRadius = 44
	StartRadiusSpread = 3
	MidRadiusSpread = 5
	EndRadiusSpread = 8
	StartRGB = [ 35 , 33 , 30 ]
	StartAlpha = 90
	ColorMidTime = 40
	MidRGB = [ 30 , 30 , 30 ]
	MidAlpha = 65
	EndRGB = [ 30 , 30 , 30 ]
	EndAlpha = 0
}
Flickit_FxFire = {
	type = NEWFLAT
	LocalSpace
	UseStartPosition
	texture = dt_nj_flame02
	blendmode = add
	FixedAlpha = 128
	AlphaCutoff = 0
	MaxStreams = 2
	SuspendDistance = 0
	lod_dist1 = 3500
	lod_dist2 = 4000
	UseMidPoint
	UseColorMidTime
	StartPosition = VECTOR(0, 0, 0)
	BoxDimsStart = VECTOR(50, 10, 40)
	MidPosition = VECTOR(0, 30, 0)
	BoxDimsMid = VECTOR(40, 15, 35)
	EndPosition = VECTOR(0, 75, 0)
	BoxDimsEnd = VECTOR(20, 20, 20)
	EmitRate = 70
	Lifetime = 0.5
	MidPointPCT = 45
	StartRadius = 14
	MidRadius = 20
	EndRadius = 8
	StartRadiusSpread = 3
	MidRadiusSpread = 4
	EndRadiusSpread = 3
	StartRGB = [ 255 , 150 , 60 ]
	StartAlpha = 110
	ColorMidTime = 45
	MidRGB = [ 230 , 80 , 20 ]
	MidAlpha = 90
	EndRGB = [ 80 , 20 , 0 ]
	EndAlpha = 0
}
Flickit_FxBoomFire = {
	type = NEWFLAT
	LocalSpace
	UseStartPosition
	texture = dt_nj_flame02
	blendmode = add
	FixedAlpha = 128
	AlphaCutoff = 0
	MaxStreams = 2
	SuspendDistance = 0
	lod_dist1 = 4000
	lod_dist2 = 4500
	UseMidPoint
	UseColorMidTime
	StartPosition = VECTOR(0, 0, 0)
	BoxDimsStart = VECTOR(80, 30, 120)
	MidPosition = VECTOR(0, 90, 0)
	BoxDimsMid = VECTOR(160, 80, 160)
	EndPosition = VECTOR(0, 210, 0)
	BoxDimsEnd = VECTOR(240, 100, 240)
	EmitRate = 140
	Lifetime = 0.6
	MidPointPCT = 40
	StartRadius = 40
	MidRadius = 140
	EndRadius = 40
	StartRadiusSpread = 10
	MidRadiusSpread = 20
	EndRadiusSpread = 10
	StartRGB = [ 255 , 160 , 70 ]
	StartAlpha = 128
	ColorMidTime = 40
	MidRGB = [ 200 , 70 , 10 ]
	MidAlpha = 110
	EndRGB = [ 40 , 10 , 0 ]
	EndAlpha = 0
}
Flickit_FxBoomSmoke = {
	type = NEWFLAT
	LocalSpace
	UseStartPosition
	texture = dt_generic_particle01
	blendmode = blend
	FixedAlpha = 128
	AlphaCutoff = 1
	MaxStreams = 2
	SuspendDistance = 0
	lod_dist1 = 4000
	lod_dist2 = 4500
	UseMidPoint
	UseColorMidTime
	StartPosition = VECTOR(0, 40, 0)
	BoxDimsStart = VECTOR(120, 40, 160)
	MidPosition = VECTOR(0, 160, 0)
	BoxDimsMid = VECTOR(200, 80, 200)
	EndPosition = VECTOR(0, 340, 0)
	BoxDimsEnd = VECTOR(300, 120, 300)
	EmitRate = 60
	Lifetime = 1.8
	MidPointPCT = 40
	StartRadius = 40
	MidRadius = 90
	EndRadius = 140
	StartRadiusSpread = 10
	MidRadiusSpread = 20
	EndRadiusSpread = 30
	StartRGB = [ 35 , 30 , 28 ]
	StartAlpha = 110
	ColorMidTime = 40
	MidRGB = [ 30 , 28 , 26 ]
	MidAlpha = 80
	EndRGB = [ 30 , 30 , 30 ]
	EndAlpha = 0
}

// A car blowing up at pos: the fireball and its smoke, gone by themselves (as the game's own fireball,
// skutils.q), and the game's fireball sound, as loud as src/drive_game.cpp says for how far he is (vol)
SCRIPT Flickit_CarBoom
	CreateCompositeObject {
		Components = explosion_composite_structure
		params = { pos = <pos> NoName SystemLifetime = 700 LocalSpace UseStartPosition NeverSuspend Flickit_FxBoomFire }
	}
	CreateCompositeObject {
		Components = explosion_composite_structure
		params = { pos = <pos> NoName SystemLifetime = 1800 LocalSpace UseStartPosition NeverSuspend Flickit_FxBoomSmoke }
	}
	PlaySound FlamingFireBall01 vol = <vol> pitch = 55
ENDSCRIPT

// A car burnt out: dark (its model's own lights; the game's light from the ground under it only changes how
// bright they are)
SCRIPT Flickit_CarBurnt
	IF ObjectExists id = <car>
		<car> : Obj_SetLightAmbientColor r = 24 g = 21 b = 18
		<car> : Obj_SetLightDiffuseColor index = 0 r = 18 g = 15 b = 12
		<car> : Obj_SetLightDiffuseColor index = 1 r = 12 g = 10 b = 8
	ENDIF
ENDSCRIPT

// The sound of a crash (the bang of metal his bail into a car makes), as loud and as high as the DLL says
SCRIPT Flickit_CarHitSound
	PlaySound BonkMetal_11 vol = <vol> pitch = <pitch>
ENDSCRIPT

// His car caught fire: it blows up in a few seconds
SCRIPT Flickit_CarOnFire
	IF ObjectExists id = flickit_car_help
		DestroyScreenElement id = flickit_car_help
	ENDIF
	create_panel_block id = flickit_car_help text = "Your car is on fire!\\nHold \\b0 to get out" rgba = [ 128 60 40 110 ] time = 4500
	PlaySound FlamingFireBall01 vol = 120 pitch = 110
ENDSCRIPT

// His car blew up with him in it: out the way he gets out (spawned, as the game's own exit is), then knocked
// down
SCRIPT Flickit_CarBlowOut
	KillSpawnedScript name = Flickit_CarWaitForExit
	KillSpawnedScript name = Flickit_CarBlownOut
	SpawnScript Flickit_CarBlownOut
ENDSCRIPT

SCRIPT Flickit_CarBlownOut
	Flickit_CarExit
	MakeSkaterGoto PedKnockDown params = { Jump }
ENDSCRIPT

// A car blew up near him on foot: knocked down (the game's own for a person who walks into him)
SCRIPT Flickit_CarBlastHim
	IF NOT Skater : Driving
		MakeSkaterGoto PedKnockDown params = { Jump }
	ENDIF
ENDSCRIPT

// A car of the level, back at the start of its way (when he is far from there)
SCRIPT Flickit_CarBack
	IF NOT IsAlive name = <car>
		Create name = <car>
	ENDIF
ENDSCRIPT

// A car he left goes back (src/drive_game.cpp says when: it has stood long enough, and he is far from it and
// from the start of the car's way): it goes, and the car of the level it was (car, if it is to come back) is
// at the start of its way again, in the same frame, so that the level has the cars it had
SCRIPT Flickit_CarGoesBack
	IF ObjectExists id = <parked>
		<parked> : Die
	ENDIF
	IF GotParam car
		Flickit_CarBack car = <car>
	ENDIF
ENDSCRIPT

// Someone run over by a car. src/drive_game.cpp finds them in the way of his car (or of a car thrown about),
// throws them and lands them (peds.h); these are the game's own falling down and lying there, as
// Ped_FallDownState has them (sk4_PedCollision.q), but they do not get up. Run on them: their own script
// becomes Flickit_PedDown.
SCRIPT Flickit_PedHit
	Obj_SwitchScript Flickit_PedDown
ENDSCRIPT

SCRIPT Flickit_PedDown
	Obj_GetId
	TerminateObjectsScripts id = <objId> script_name = ped_walker_hit_dead_end use_proper_version
	TerminateObjectsScripts id = <objId> script_name = ped_adjust_speed_to_match_anim use_proper_version
	TerminateObjectsScripts id = <objId> script_name = ped_standing_look_at_skater use_proper_version
	TerminateObjectsScripts id = <objId> script_name = ped_skater_start_moving use_proper_version
	TerminateObjectsScripts id = <objId> script_name = ped_standing_idle use_proper_version
	// the ped logic does nothing in this state, so they are where the DLL puts them
	Ped_SetLogicState fall
	Ped_StopMoving
	// and nothing of the skater near them gets them up again
	Obj_ClearExceptions
	Temp_Ped_Fall_Sound
	ped_play_falldown_anim
	BEGIN
		ped_play_layidle_anim
		Wait 1 GameFrame
	REPEAT
ENDSCRIPT

// They hit the ground (on them). From the DLL: vol.
SCRIPT Flickit_PedLand
	Obj_PlaySound BailBodyFall01 vol = <vol>
ENDSCRIPT

// Back on their way, once he is far (src/drive_game.cpp says when): they go, and a moment later they are made
// again from their node (ped), at the start of their way
SCRIPT Flickit_PedGone
	IF IsAlive name = <ped>
		Kill name = <ped>
	ENDIF
ENDSCRIPT

SCRIPT Flickit_PedBack
	IF NOT IsAlive name = <ped>
		Create name = <ped>
	ENDIF
ENDSCRIPT
