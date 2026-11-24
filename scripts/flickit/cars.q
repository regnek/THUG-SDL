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
	// the car the game's way for a car of the level, but standing: its model's own nodes (the gap triggers on
	// its roof look there: without them the game looked in the level's nodes, found none and crashed) and the
	// rails on its roof, as the game gives a car of the level (its car factory, 0x00506099)
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
	// the way the car faced, and on the list of cars to take
	Flickit_CarParked
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
