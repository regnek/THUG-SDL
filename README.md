
# THUG SDL

A mod to improve the gameplay experience of the PC version of Tony Hawk's Underground. It adds stability and usability fixes, utilizes the SDL2 game library and offers an open base for script-mod development.

## How to install

Download the latest version from the relase tab and extract the files from the zip-archive into the game directory.<br>
 - Default game directory: `C:\Program Files (x86)\Activision\Tony Hawk's Underground\Game`

Run `thugsdlpatcher.exe` in the Game directory where THUG.exe should already be. It will generate THUG-SDL.exe which will be used to start the game from now on. This only needs to be done once.<br>There is also a brief [video](https://youtu.be/717bshkWhMw) showcasing the process.<br><br>*Important: This only works for the No-CD THUG.exe, which is approximately 2.6 MB in size.*

## Features

### General
- Unofficial port of [partymod-thaw](https://github.com/PARTYMANX/partymod-thaw)
- Replaced input system entirely with new, modern system using the SDL2 library
- Improved window handling allowing for custom resolutions and configurable windowing (4k resolution, (borderless) windowed mode)
- Minimal launcher to configure game and mod settings
- In game mod settings menu
  - Saves to and loads from thugsdl.ini automatically
- Stability fixes
- Added pause buffer for speedrunning (menu to game transition with pause button held)
- Option to disable all script mods (speedrunner mode)
- Modloader for script mods: [Section Modloader](#modloader)
- Supported languages: English, French, German
- Increased speed when exiting the game
- Console window with adjustable log level
- Option to print the console output to a file (overwrite / append)
- Adjustable button prompts: PC, Ps2, Xbox, Ngc
- Restored the onscreen keyboard
- Intro movie skip
- Option to store the window position
- Exception Handler to generate crash reports (may not work on Windows 11)
- Enable leftover levels: School, Philadelphia, Downhill Jam

### Graphics
- Increased default clipping distance for large levels
- Higher quality shadows
- Option to toggle blur effects
- Option to toggle the fullscreen gamma correction which makes the game looks too dark
- FOV and aspect ratio settings

### Gameplay
- Land pivots (including the ability to buttslap or air manual after)
- Toggle for boardscuffs
- Toggle for skater voices during bails
- Removed player limits for created parks
- Buttslap counter
- Fixed double tap grind balance reset
- Option to enable free cam from the "select" button (only during observe mode in net games)
- Create-A-Skater: Increased 3 axes scaling
- Create-A-Skater: Numbers on color and scaling sliders
- Create-A-Skater: Increased color ranges
- Create-A-Skater: Board scaling
- Always enable the ability to edit tricks during games
- Unbind tricks in Edit Trick menu
- Raised level ceiling and acid drop height

### Controls
- Native gamepad support
- Restore original menu navigation for Xbox and Ps2 layouts
- Support for Ps2 controls on gamepad and keyboard
- Fix incorrect button prompts in textboxes and helper texts
- Adjustable keys for Rail Drop Downs including directional DDs
- Adjustable keys for Caveman
- Adjustable keys for ladder/rail grab
- Input setting for THPS4 Boostplants (Wallieplants)
- Input setting for wallplants
- Option to invert the x and/or y camera axis
- Option to disable the x and/or y camera axis
- Flickit: Skate-style right stick controls: [Section Flickit](#flickit)

### Multiplayer
- Openspy support
- Replace GameSpy with OpenSpy in texts and images
- Multiplayer Observe menu 
  - Choose next / previous on ollie / grab or arrow keys
  - Warp to players during freeskate
  - Toggle hud
  - Possible to start observe during games and rejoin freeskate after a round ended
- Menu to load online settings
- Longer text input in quick chat
- No chat reset after net games started / ended
- Removed short freeze time before level changes
- Option for chat size scaling
- Restart game option added to the ranking screen and pause menu


## MODLOADER
This allows you to load user-defined `.pre` and `.qb` files from a separate folder. 
Activate "Additional Mods" in the launcher's General Page and set your folder path (example: `data/pre/mymod`).

An example environment to easily develop script mods can be found [here](https://github.com/atljp/thps-modding-resources/tree/main/Scripts/THUG/THUG-SDL_DEV_ENV).<br>
<br>**THUG-SDL uses its own script mods by default (thugsdl.pre). Select "Disable Script Mods" in the launcher or your changes could get overwritten!**<br><br>


## FLICKIT
Flickit puts the board on the right stick, the way the Skate games do. The stick is translated into the game's own Ollie / Flip / Grab / Grind buttons, so every trick, score and goal works as before. The face buttons keep working next to it, with one exception: while the board rolls on the ground the Flip button and the Ollie button push (see Rolling below).

| Input | What happens |
| --- | --- |
| Right stick down | Crouch (Ollie is held) |
| Let the stick go, or flick it away | Ollie |
| Down, flick up-right | Kickflip (Left + Flip) |
| Down, flick up-left | Heelflip (Right + Flip) |
| Down, drag to the right | Pop Shove-It (Down + Flip) |
| Down, drag to the left | The trick on Up + Flip (Impossible by default, put FS Shove-It there for the Skate layout) |
| Down-left, flick up-right | Varial Kickflip (DownLeft + Flip) |
| Down-right, flick up-left | Varial Heelflip (DownRight + Flip) |
| Down-right, flick up | Hardflip (UpLeft + Flip) |
| Down-left, flick up | Inward Heelflip (UpRight + Flip) |
| Left trigger / right trigger | Grab with that hand (Left + Grab / Right + Grab), held as long as the trigger is |
| Nothing | Grind: rails and ledges catch the board by themselves, and the right stick says which grind it is (see Grinds below) |
| Left bumper / right bumper | Spin left + nollie / spin right + revert and switch, both together: caveman |
| Flip button or Ollie button, on the ground | Push. A tap is a small push, a quarter of a second a full one, and a button that stays down keeps pushing (see Rolling below) |
| Movement stick left / right | Steers the way Skate's board does: gently near the centre, tightly at a walk, widely at speed (see Rolling below) |
| Movement stick left / right, in the air | Turns the skater the way Skate does: by how far the stick is pushed, and faster the harder it was thrown over as the board left the ground (see Spinning below) |
| Right stick click | Flickit on / off, for looking around. On foot and in a car it stands down by itself |

The tricks named above are the default trick slots. Flickit presses the slot, so whatever is assigned to it in the Edit Tricks menu is what comes out. Manuals are unchanged (up, down on the movement stick).

### Grinds
There is no grind button to hold: a rail or ledge under the board catches it. Which grind it is depends on how the board leans when it touches down, and in the air the board is leaned with the right stick, the way Skate does it. Two things count: where the stick was pushed first, and where it is at touchdown. "Toward" is the side the rail is on.

| Right stick in the air | Grind |
| --- | --- |
| At rest | 50-50, or a boardslide / lipslide if the board lands across the rail |
| Up / down | Nosegrind / 5-0 |
| Up and toward the rail / up and away from it | Crooked / Overcrook |
| Down and away from the rail | Salad (down and toward stays a 5-0) |
| To a side | Boardslide or lipslide, whichever fits the way the skater faces |
| Up, then rolled toward the rail / away from it | Noseslide / Nosebluntslide |
| Down, then rolled toward the rail / away from it | Tailslide / Bluntslide |
| Toward the rail, then rolled up / away, then rolled up | Feeble / Smith |
| Toward the rail, then rolled down / away, then rolled down | Crooked / Overcrook (Skate has the willy and the overwilly here, which THUG does not know) |

On the rail:
- Stick down is the 5-0, up the nosegrind, let go the 50-50. The game takes such a switch once a grind has settled, about three quarters of a second in. Only these three switch; any other grind stays what it is.
- A flick pops off the rail, with the flip the flick asks for. Letting the stick go does not pop. A flick is a fast move: taking the stick from one end to the other slowly switches the grind instead.
- The right bumper drops off the rail (THUG SDL's drop-down key).
- The board does not drift off balance. `GrindBalance=1` gives the balance meter its job back.
- The board loses speed, and steps off when it has none left (see below).

In the air a flick across the stick is still a flip trick. Rolling the stick round to a side leans the board; it is a shove-it only when the roll starts on the ground or on a rail.

The Grind button itself works as it always did. While it is held the game picks the grind from the direction buttons, and lip tricks and wallrides stay on it. Coming out of a ramp nothing is caught without it, so the coping is left alone.

Off the board Flickit stands down: the right stick is the camera again and the triggers are the game's. Both bumpers get the skater back on, as they got him off.

### How far a rail reaches
THUG takes a rail when the skater's path comes within 40 units of it while he travels along it (20 when he crosses it). That is a metre at his size. It measures in every direction, so a rail overhead or well below counts too, and it puts the board on the rail in that same frame. With a Grind button to time, that is a reach the player asks for. With rails that catch by themselves it is a magnet: any rail near the flight takes the board.

Skate has nothing like it. Its helper looks eight ticks ahead for a part of the board that is about to pass a rail at less than 17 to 24 cm, and then slides the board toward the rail by at most 3 cm a tick and 20 cm in all. THUG's skater is one point, the middle of his board, so Flickit uses the test Skate has for the middle of the board, which is the same whichever way the board is turned. A rail that catches by itself reaches:

- 19 units to either side, level. That is Skate's 24 cm.
- 16 units down: the board may be that far above the rail. And 8 units up, for a board that comes in just under it. The board has to come down on the rail; a rail overhead or far below is out of reach.
- 16 units beyond its end.

Those are the half axes of one rounded shape around the rail, so a board that is both beside the rail and above it has to be nearer on both counts. No point of that shape is as far as 20 units from the rail, so the reach of a rail now lies wholly inside the reach THUG gives it, whichever way the board comes. A rail the board has crossed, or flown off the end of, is let go after 8 units. Nothing pulls the board back.

Where two rails are near, the one under the board is taken. THUG prefers the one that runs the way the skater travels, which at the corner of a ledge is often the wrong edge.

`GrindReach` scales all of it, in percent. At 100 the distances are Skate's at the scale the drag uses, 80 units to the metre: THUG's skater is about twice as fast for his size, so this gives the player the same time to line up a rail as in Skate. At 50 they are Skate's at the skater's own size, 40 units to the metre: the pull is as short on screen as in Skate, and there is half the time to line up. 200 is close to THUG's own reach along a rail. 0 hands the search back to the game.

Holding the Grind button gives the game's own reach for that jump, and the coping of a ramp is on the button as before. With the console on, every rail says how far it had to reach for the board, and a jump that ends on the ground next to a rail says by how much it missed.

### Speed on a rail
In THUG a grind lasts as long as the rail does. With Flickit the board loses speed on a rail the way it does in Skate, and steps off by itself when the speed is gone.

- What it slides on counts. Metal rails, wires, glass and plastic slide best. Ledges of concrete and wood drag about twice as hard, and brick, rough stone, logs and rope a third more than a ledge.
- How it slides counts. Least on the trucks (50-50, 5-0, nosegrind). On a metal rail a boardslide or lipslide drags a tenth more, a nose- or tailslide a fifth more, and the grinds with truck and deck both on the rail (crooked, overcrook, smith, feeble, salad) two fifths more. On a ledge the steps are bigger: a third more, half more, and twice as much.
- The drag grows over the first four seconds of a grind, from five eighths of its full strength to all of it.
- In numbers, for a 50-50 on a level rail: metal takes 63 units of speed per second at first and 100 from four seconds on, a ledge 109 and 173. From a fast roll (700) that is about eight seconds and 2900 units of metal rail or five seconds and 1800 units of ledge; a smith on a ledge is over in under three seconds. `GrindDrag` scales all of it: 50 is half, 200 double, 0 none.
- Slopes are the game's as before: downhill the board picks up speed, uphill it loses more.
- After at least a second on the rail, a board that has all but stopped steps off: THUG's own skate-in, the one on the right bumper. Where there is ground next to the rail the skater rolls onto it, where there is none he hops off and drops. He goes to the side the board came from; hold left or right to pick the side yourself. On a rail that goes uphill the board steps off at the point where THUG would let it slide back down. `GrindExit=0` leaves the board on the rail.
- THUG pushes the board every time it lands on a rail, by 150 units of speed, which is a third of a kick. Skate does not, and with a drag on the rail that push would be a way to get the speed back by hopping. So while the drag is on, next to nothing is left of it. `GrindBoost=1` brings it back.

The numbers are Skate 3's, put into THUG's units at 80 units of speed for one metre per second. That makes a grind last as long as it would in Skate when both skaters come in at their own rolling speed. THUG's skater is about twice as fast as Skate's, so measured in rail the grinds go about twice as far; `GrindDrag=200` gives roughly Skate's distances instead.

### Jumps
THUG's skater jumps high and comes down fast. A tap of Ollie throws him up at 350 units a second, a held one at 414 to 450 depending on his ollie stat, and gravity in the air is 1350 units/s². Off flat ground that is 45 to 75 units, which is 0.6 to 1 times his own height, in 0.52 to 0.67 seconds. Skate's rider rises 0.38 to 0.76 m, a fifth to two fifths of his height, and takes 0.56 to 0.79 seconds over it, under real gravity. For his size THUG's skater jumps two and a half times as high and still lands sooner, because his gravity is about three times real.

With Flickit both numbers are Skate's, at the scale the rails use (80 units to the metre):

- `AirGravity=100`: gravity in the air is Skate's, 784 units/s², which is 58 percent of THUG's.
- `JumpPop=100`: every jump leaves the ground at 69 percent of THUG's speed. THUG's best ollie then rises as far as Skate's full pop, 61 units, and lasts as long, 0.79 seconds. A tap rises 37 units in 0.61 seconds.

Together they make every jump four fifths as high as it was and a fifth longer: an ollie, a boneless, a pop out of a grind. What leaves a ramp is another matter. The speed up a quarter pipe is not the pop's, so with Skate's gravity an air goes 1.7 times as high and lasts 1.7 times as long. That is what real gravity does at THUG's speeds, and it is the first thing to turn back if it is too much: `AirGravity=0` is THUG's own gravity. Skate's pop alone would then leave a jump under half as high as THUG's, so turn the pop up with it: `JumpPop=131` keeps the jumps four fifths as high as THUG's and makes them a tenth shorter.

Both are percent of Skate's, so 50 is half of Skate's gravity or pop and 200 double. 0 leaves the game's own number alone. THUG's levels were built for THUG's jump: where a ledge or a gap is out of reach now, a boneless still clears more than THUG's plain ollie did.

### Rolling
THUG's skater keeps himself going. Whenever he is slower than his kick speed (394 to 496 units a second standing and 532 to 675 crouched, by his speed stat) he kicks, at 629 to 1200 units/s², and what holds him back is a drag that grows with the square of his speed: 96 units/s² at 400, 294 at 700. He turns at 1.8 radians a second for as long as left or right is held, all or nothing, at any speed. Skate's board does nothing by itself: it is pushed, it rolls out, and it goes where its trucks point as far as its wheels grip. With Flickit the board is Skate's, at the scale the rails and the jumps use (80 units to the metre).

Pushing, `Push=100`:

- The Flip button pushes: it is THUG's own button for a kick. So does the Ollie button, which has nothing else to do on the ground while Ollie is on the stick; Skate pushes on two buttons as well. The Flip button pushes whenever it is down while the board rolls on the ground, the Ollie button when its press begins there (held from the air into a landing it is still THUG's crouch). A press that pushes is not passed on to the game, not as Ollie and not as Flip: the game would take the Flip button for the start of a flip trick as well. In the air, on a rail, in a manual, during a revert and off the board both buttons are what they always were. `PushOnOllie=0` leaves the Ollie button alone.
- How long the button is down says how strong the push is. A tap adds 60 units of speed (Skate: 0.75 m/s), a tenth of a second 150, 0.15 s 215, 0.2 s 250, and a quarter of a second or more adds 320 (4 m/s). What counts is how long it has been down when it is let go, or when the foot has pushed, whichever comes first. A button that stays down pushes again and again with all there is.
- Pushing gets the board to 680 units a second and no further (Skate's 8.5 m/s, which is about the best THUG's own kick does). From 664 on no push starts. Holding the button from a standstill is 320, 640, 680.
- One push is one play of the game's push animation, and the speed comes in while the foot of that animation pushes. Skate has no clock for this and no easing curve: its board is taken up to the speed its push animation itself moves at, for as long as that animation says the foot is on the ground. Flickit does the same with THUG's animation. It reads from the game which animation the skater is in and how far into it, and the board has as much of the push as the foot has of its stroke: nothing before the foot starts back along the ground, half when it is half way, all of it when it has arrived. The board is taken up to that speed whatever the slope does meanwhile, and by no more in one tick than Skate allows (0.6 m/s near a standstill, 0.5 at full speed). Down ends a push.
- Where the foot pushes is not written down anywhere in THUG, so the mod looks. While a push animation plays it watches where the game puts the skater's two feet, and when the animation has run through it knows which foot went down to the ground and how it went back. In THUG's push animations the foot comes down beside the board, slides back along the ground for a twelfth of a second, and goes on back as it rises high behind the skater: all of that, about a quarter of a second from 7–12% to 31–34% of the animation, is the stroke, and the speed of a push comes in as the foot goes back, most of it while it is low. Every push after that goes by what was seen in the push animation watched last. (A foot that stood where it was put would have the speed come in over the time it is on the ground.) The first push of a session, and any animation the feet could not be made out in, goes by `PushFootDown` and `PushFootUp` instead: 8 and 32, the stroke as it was watched in the game. What was seen goes to the log, once for every animation, and the first time an animation has been watched whole, every look at the two toes with it.
- No animation, no push. The game's scripts decide when the push is played: a push asks them, as a rule they start it on the next frame, and while it plays they wait for it to end. So with the button held one push follows the other at the pace of the animation, and a second tap during a push is one more push after it. After a landing, after skating off the end of a rail and after a bump into a wall, THUG's scripts wait for that animation to end before they look for a push; Skate's skater does not wait, so a push cuts the animation short. It starts as soon as the scripts are past the few frames after a landing in which a revert or a manual can still begin: a sixth of a second, a twelfth after a vert air. Where the scripts are busy with anything else, a button that stays down gets its push when they are done, and a tap gives up after three quarters of a second. One safeguard: should the game be told to play the push three times running without the mod seeing a push animation on the skater, pushes go by a clock from then on (0.8 s each, from the moment the game is told) until one is seen, and the log says so.
- THUG's scripts look at Left and Right before they look for a push, and show the turn. Skate pushes in a turn like anywhere else, so with `PushOverTurn=1` the scripts do not get to see Left and Right while a push is asking for its animation and they are in their loop for the ground, which is where they look for a push: they play the push, and the board turns all the same. `PushOverTurn=0` is THUG's order, the turn first: then there is no push while the stick is held to one side.
- What is not covered: a skater whose push style is mongo gets THUG's looping mongo push. Every round of it is a push while the button is down, and it runs on after that until another animation takes over (THUG does the same with its "auto kick" off). And a crouching skater has a push of his own in THUG that the scripts play below 600 units a second only, so between 600 and 664 he is not pushed.
- `Push` is percent of Skate's: at 50 a push adds half as much and pushing stops at 340. 0 is THUG's own kick.
- While the push is Flickit's the game's "auto kick" is held off for the skater, so that he kicks only when asked. The game's own setting comes back when Flickit is switched off.

Rolling out, `Coast=100`:

- On its wheels the board loses 10 units of speed a second at a crawl and 35 to 40 from 400 up (Skate: 0.13 to 0.5 m/s²), and more above 650: 48 at 900, 100 at 1100. THUG's own drag takes 96 at 400 and 294 at 700 from a standing skater, a fifth of that from a crouching one.
- From 640 units a second the board rolls for half a minute and about 7000 units of ground; after nine seconds it still has half its speed.
- Under 10 units a second the game stops the board and stands the skater up.
- In a bail, in a revert, in a manual, and wherever the game's scripts put the brakes on, the friction is THUG's as before. Slopes are the game's as well.
- `Coast` is percent of Skate's. 0 is THUG's own friction. With the push on that is none at all (with "auto kick" off THUG lets the board roll forever), so the two belong together.

Steering, `Steering=100`:

- The stick turns the trucks, and the board follows them. Near its centre the stick does little: half the stick is a fifth of the angle. The d-pad and the keys count as the stick at its end.
- At a walk the board turns tightly: 2.8 radians a second at 100 units/s and 4.2 at 150, with the stick at its end. From there on the wheels' grip is the limit, 0.8 g to the side, and the faster the board the wider the turn: 3.1 at 200, 2.1 at 300, 1.3 at 500, 0.9 at 680, 0.7 at 900. The two games are level at about 350 units a second. At 680 the tightest circle is 740 units in radius, where THUG's is 380.
- The trucks follow the stick in about a fifth of a second. While a foot is on the ground they turn less, by a fifth at the end of one push.
- Standing still the board turns on the spot as it does in THUG, at 1.8 radians a second with the stick at its end.
- Down with left or right is THUG's own sharp turn as before.
- `Steering` is the grip in percent of Skate's: 200 turns at speed about as THUG does and leaves the rest as it is. 0 is THUG's own turning.

THUG's levels were built for a skater who keeps his own speed and turns on a coin. Timed goals take pushing now, and a tight park is tighter. The three settings are separate so that each can be turned back by itself.

### Spinning
THUG turns the skater in the air at 6.85 to 7.75 radians a second (by his spin stat) for as long as left or right is held, once it has been held for a tenth of a second: all or nothing, the same at the start of an air and at its end. In Skate the stick turns him, by how far it is pushed and by how hard it was thrown there. `Spin=100`:

- With the stick at its end after a full flick the skater turns at 3.2 radians a second as the board leaves the ground, 7.9 a quarter of a second later, 5.7 after 0.9 s and 3.7 from two seconds on (Skate 3's curve). With the stick halfway it is half of that.
- A stick that is simply held gets two fifths of that rate. The rest is for the flick: how far the stick moved outward within a 60th of a second, the best of the last quarter of a second before the board leaves the ground and the first 0.14 s after it. Thrown to its end in one frame it is the whole rate, pushed over in a twentieth of a second 0.7 of it, in a tenth 0.55. A flick earlier than that (up to half a second before the jump) or later in the air counts for less.
- What it comes to, in degrees. In an air of 0.56 s, Skate's quick pop: 94 with the stick held, 161 with it pushed over in a twentieth of a second, 220 with it thrown. In 0.79 s, its full pop: 127, 217 and 301. THUG's own turning makes 181 to 204 and 271 to 306 of the same two airs. In an air of two seconds: 260 held and 635 thrown, where THUG turns 746 to 844. So a 180 on the flat takes a flick, and a 360 takes something to jump off.
- Let the stick go and the turning runs out by itself, 4 percent less each tick: thrown and let go after a fifth of a second, the full pop still comes to 206 degrees.
- The rate follows the stick by at most 1 rad/s a tick at first and 0.15 after a second in the air, and comes down by at most 0.2 a tick. (Skate's own turning to one side starts a tick late, 7 degrees in a full pop; here both sides are the quicker one.)
- The d-pad counts as the stick thrown to its end.
- THUG's two spin buttons turn the skater as they always did (with `TriggerGrabs=1` they are on the bumpers), and so does THUG's turn back toward the way down on a ramp; Skate has one of its own there, which is not copied. Where THUG's scripts forbid turning during a trick, the stick does not turn him either. The count of the turning for the name of the trick is the game's.
- `Spin` is percent of Skate's rate: 150 turns half again as fast. 0 is THUG's own turning in the air.
- With the console on, every air says how hard the flick was and how far the stick asked the skater to turn.

Flip tricks are not touched. In both games how long a flip takes is in its animation: THUG plays it 1.16 to 1.3 times as fast as it was made, by the skater's flip stat, and bails a landing that comes more than 25 frames before its end; Skate blends the flip's animation by the height of the pop. Neither game's animations were measured, and how the speed of the flick enters Skate's was not worked out.

### What the mod changes in the game
For the grinds the mod looks at the skater's state and changes five places in the game's rail code: the test for the Grind button before a rail is caught, the search for a rail that follows it, the pick of the grind, the balance of a grind (the drag is applied in the same place, once per frame of the game's rail physics), and the speed the game adds when the board lands on a rail. For the jumps it stands in for the game's own answer to "what is the gravity in the air", and scales the speed of a jump at the point where the game has worked it out. For rolling it stands in for four things in the game's physics of the ground (the test for braking, the test for a kick, the friction and the turning) and for three commands of the game's scripts (CanKick in the nollie loop, Held when it is asked about the Flip button, and AnimFinished when the scripts wait for the end of a landing, a skate-off or a bump while a push asks for its animation), it holds the game's "auto kick" setting off for the skater, and with `PushOverTurn=1` it keeps Left and Right from the game's scripts while a push asks for its animation and they are in their loop for the ground (the game's physics see both as they are). For these it reads which script the skater's is in, and which one called it. For the push it also reads two things without changing them: which animation the skater is in and how far into it, and where his two feet are, through the game's own function for the place of a bone. For spinning it stands around the game's turning in the air, which for that one call sees Left or Right held the way the stick's turning goes, and answers for the skater's rate when the game asks for it there. The code at every address it uses is compared with what is expected there first. If anything differs nothing is touched and grinds work as in earlier versions, with the Grind button held for a moment after each pop. `GameHooks=0` does the same on purpose. All of it applies to the first player's skater, while he is on his board and Flickit is on.

The settings live in the `[Flickit]` section of `thugsdl.ini`. Thresholds are percent of full stick deflection, times are milliseconds. Press F9 in game to re-read the section without restarting.

| Key | Default | Meaning |
| --- | --- | --- |
| Enabled | 1 | Master switch |
| KickflipSide | 0 | 0: kickflip is the flick to the upper right. 1: mirror every flick left / right |
| TriggerGrabs | 1 | Triggers grab, bumpers take over nollie / revert / caveman. 0 leaves triggers and bumpers as bound |
| AutoGrind | 1 | Hold Grind after every pop. Only where rails do not catch by themselves (`GameHooks=0` or `GrindCatch=0`) |
| AutoGrindDelay, AutoGrindHold | 200, 1500 | When the hold starts after the pop and how long it lasts |
| Nollie, NollieTaps | 0, 2 | Experimental: stick up, flick down = nollie. NollieTaps is how many Nollie presses reach nollie stance |
| ToggleButton | 8 | SDL button for on / off (8 = right stick click, 99 = none) |
| ReloadKey | 66 | SDL scancode that re-reads this section (66 = F9, 0 = none) |
| Debug | 1 | Log every recognized flick. Needs the console switched on (`Console=1` under `[Logger]`); with `WriteFile=1` as well the same lines go to `debug.txt`. `WriteFile=1` alone logs nothing |
| Deadzone | 25 | Below this the stick counts as centered |
| Engage, Release | 60, 45 | A crouch starts above Engage and pops below Release |
| Flick, Snap | 50, 75 | How far the flick has to reach, and how far for it to be accepted at once |
| FlickWindow | 200 | How long after the pop the flick may arrive |
| Settle | 50 | How long a drag to the side has to rest to count |
| Dwell | 60 | How long the stick has to rest in down-left / down-right to make it the start |
| AfterPop, DirLead, Pulse | 10, 15, 30 | Timing of the synthetic direction + button press |
| Patterns | 1 | Decide tricks from a stick pattern file when one is found (see below). 0: always use the built-in directions |
| PatternFile | skater.pat | Name of the pattern file, looked for in the folder `thugsdl.ini` is in |
| MissTicks | 10 | How many 60 Hz ticks the stick may spend between two points of a pattern before the pattern is dropped |
| GameHooks | 1 | The mod may read the skater's state and change the game's code (see What the mod changes in the game). 0: none of the settings below do anything |
| GrindCatch | 1 | Rails catch without the Grind button |
| GrindPick | 1 | The lean of the board picks the grind. 0: the game's direction buttons do |
| GrindSwitch | 1 | On a rail, stick down / up / let go switches between 5-0, nosegrind and 50-50 |
| GrindBalance | 0 | 0: the board does not drift on a rail. 1: the game's balance |
| GrindMirror | 0 | 1: swap "toward the rail" and "away from it", in case the grinds come out mirrored |
| PauseOffBoard | 1 | Flickit stands down on foot and in a car |
| Tilt | 50 | How far the stick has to be pushed to lean the board, and to ask for a switch on a rail |
| TiltSettle | 150 | A stick that is still where a flick ended leans the board only after this long |
| SwitchHold | 250 | How long the stick has to rest on a rail before the grind switches |
| RailTap | 50 | How long Ollie is pressed for the pop off a rail. Longer pops higher, up to about 200 |
| FlickSpeed | 100 | On a rail and in the air, how fast the stick has to travel to be a flick: percent of full deflection per 100 ms. 0: any speed |
| GrindReach | 100 | How far a rail that catches by itself reaches for the board, in percent of Skate's reach (see How far a rail reaches). 0: the game's own reach, 20 to 40 units all around. Up to 400 |
| GrindDrag | 100 | How much speed the board loses on a rail, in percent of Skate's drag (see Speed on a rail). 0: none, as in THUG. Up to 1000 |
| GrindExit | 1 | A board that has run out of speed on a rail steps off by itself. 0: it stays on |
| GrindBoost | 0 | 1: keep the speed THUG adds when the board lands on a rail. Without the drag (`GrindDrag=0`) it is always kept |
| AirGravity | 100 | Gravity in the air, in percent of Skate's (see Jumps). 0: the game's own, which is 172. 25 to 400 |
| JumpPop | 100 | The speed of every jump, in percent of Skate's pop (see Jumps). 0: the game's own, which is 146. 25 to 400 |
| Push | 100 | The push, in percent of Skate's: what one push adds and how fast pushing gets (see Rolling). 0: the game's own kick. 25 to 400 |
| PushOnOllie | 1 | The Ollie button pushes too while the board rolls on the ground. 0: only the Flip button does |
| PushOverTurn | 1 | The game shows a push in a turn as well. 0: in a turn it shows the turn and not the push, as THUG does |
| PushMeasure | 1 | The skater's feet are watched in the game's push animations, and the speed of a push comes in while the foot pushes (see Rolling). 0: the next two settings say where that is |
| PushFootDown, PushFootUp | 8, 32 | Where in a push animation the foot pushes, in percent of the animation's length: for the first push of a session, and for an animation the feet could not be made out in |
| Coast | 100 | What the board loses while it rolls, in percent of Skate's (see Rolling). 0: the game's own friction. Up to 1000 |
| Steering | 100 | The grip of the wheels in a turn, in percent of Skate's (see Rolling). 0: the game's own turning. 25 to 400 |
| Spin | 100 | How fast the stick turns the skater in the air, in percent of Skate's (see Spinning). 0: the game's own turning in the air. 25 to 400 |

### Stick patterns
Out of the box Flickit sorts a flick by where it starts and ends, in eight directions (the table above). If a pattern file is present, it uses that instead: each trick is then a short list of stick positions that have to be passed in order, each with a tolerance radius. This is how the Skate games define their gestures, and the file format is theirs, so if you own Skate 3 you can copy its `skater.pat` next to `thugsdl.ini` and flick the tricks the way that game expects them. No pattern file comes with THUG SDL; without one nothing changes.

```
global_tolerance_dist 0.4        tolerance for the patterns that follow
pattern Kickflip                 the name says which trick it is
    tolerance_dist 0.4           tolerance of this pattern
    coord 0.0 0.7                a point: x (right is positive), y (down is positive), the rim is 1.0
    clock 2 1.0                  a point by clock face: hour (12 = up, 3 = right, 0 = center) and radius
```

- A pattern starts when the stick is within tolerance of its first point and is complete when the last point is reached. Time spent resting on the first point does not count.
- If several patterns are complete at once the one with the most points wins, then the most accurate and fastest. A complete pattern waits up to two ticks for a longer one that has only its last point to go.
- Names: `Ollie`, `Kickflip`, `Heelflip`, `PopShuvit`, `FSPopShuvit`, `VarialKickflip`, `VarialHeelflip`, `Hardflip`, `InwardHeelflip` press the slots of the table above. `360PopShuvit`, `FS360PopShuvit`, `360Flip`, `Laserflip`, `360Hardflip` and `360InwardHeelflip` press the slot of their base trick twice, which is how THUG turns a Varial Kickflip into a 360 Flip. `Nollie` and names starting with `N_` are followed only with `Nollie=1`. Other names are ignored.
- The crouch and the pop work as before: the stick in the lower half crouches, leaving it pops, and a pop without a completed pattern is a plain ollie. On a rail and in the air nothing is held, so there only a completed pattern counts, and on a rail the patterns that start at the top count too.
- With `Debug=1` the log shows the pattern that was recognized and how fast it was drawn (speed 1 = fast, 0 = slow; measured, not used yet).

The recognizer has offline tests that need neither the game nor Windows:
```
g++ -std=c++11 -I src tests/flickit_test.cpp src/flickit.cpp -o flickit_test && ./flickit_test
g++ -std=c++11 -I src tests/flickit_pattern_test.cpp src/flickit.cpp -o flickit_pattern_test && ./flickit_pattern_test
g++ -std=c++11 -I src tests/flickit_grind_test.cpp src/flickit.cpp -o flickit_grind_test && ./flickit_grind_test
g++ -std=c++11 -I src tests/flickit_physics_test.cpp src/flickit.cpp -o flickit_physics_test && ./flickit_physics_test
```
The second one also takes a pattern file: `./flickit_pattern_test skater.pat` draws every pattern in it and reports what was recognized. The third one is run from the top of the repository: it checks the grinds the mod asks the game for against the game's own grind table in `scripts/qb/source/scripts/game/skater/grindlist.q`, the drag on a rail against the times and distances a grind lasts in Skate, the reach of a rail against the distance to it worked out the slow way and against THUG's own reach, and the search for a rail against trying every piece of rail one by one. The fourth is run from there too: it checks THUG's jump, kick and turning numbers in the code against the game's own `physics.q`, what gravity and pop come to against Skate's jump, the pushes against Skate's curves with a stand-in for the game's scripts and its push animation, the watching of the feet against made-up animations, the rolling out and the steering against Skate's curves, all at several frame rates, and the spinning tick for tick against Skate's rules written down a second time.

## TROUBLESHOOTING

### Issues with write permissions

The game settings are stored in `thugsdl.ini` which needs write permission when you change and save settings. <br>

If the directory your game is installed to is protected, e.g. Program Files (x86), the ini file needs to be located in  
`C:\Users\<name>\AppData\Local\VirtualStore`. <br>The game will use the VirtualStore location to load and save settings.

### Gamepad bindings and controls

The control settings in the launcher expect the default gamepad binds. When you change to a non-default setup and want to change the controls, you have to account for the bind changes.<br>
Example 1:
- Default Ps2 layout: Spin left on L1, Spin right on R1. Nollie on L2, Switch/Revert on R2
- - In your custom binds you want to swap L1 <-> L2 and R1 <-> R2. This can be done directly in the "Gamepad" tab in the launcher
- - When you want to set the Caveman key to be on the L1 key, you would have to select L2 in the "Controls" tab in the launcher

Example 2:
- Default Xbox layout: Spin left and Nollie on L2, Spin Right and Revert on R2, Caveman on either L1 or R1
- - In your custom binds you want Spin left and Nollie on L1, Spin right and revert on L2, Caveman on R1 and Dropdown on R2.
- - In the "Gamepad" tab in the launcher, set Spin left on L1, Spin right on L2, Caveman on R2
- - In the "Controls" tab in the launcher, set Caveman to R1 and Drop down to L1 (you moved the default bind from L1 to R2 so it will apply R2). The controls tab only knows about the default binds.

## BUILDING FROM SOURCE

### Prerequisites
The build requires the following tools:
- [CMake](https://cmake.org/)
- [Visual Studio](https://visualstudio.microsoft.com/)
- [Recommended: git](https://git-scm.com/download/win)
- [vcpkg](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-cmd)
- [SDL2](https://www.libsdl.org/)

First download and install Visual Studio and CMake like any other GUI application.<br>
Install git via the Installer from the website or directly in Powershell:
- `winget install --id Git.Git -e --source winget`

Then install vcpkg via git as described in the Microsoft documentation:
- `git clone https://github.com/microsoft/vcpkg.git`
- Run the bootstrap script `cd vcpkg && .\bootstrap-vcpkg.bat`

Install SDL2 via vcpkg: `.\vcpkg.exe install SDL2:x86-windows`, for x64: `.\vcpkg.exe install SDL2`

### Generating the project file
Create a THUG-SDL/build directory. Then build the VS Solution with CMake from that directory via Powershell:
```
cmake .. -A win32 -DCMAKE_TOOLCHAIN_FILE=C:/[vcpkg directory]/scripts/buildsystems/vcpkg.cmake
```
Set the optimization for the DLL to O0 (disable optimization) because MSVC seems to break certain functions when optimization is enabled.
<br><br>
The `.sln` file is in the `build` directory. Make sure to select win32 and Release as the project's build options in Visual Studio. Build the actual DLL with `Ctrl+Shift+B`.
<br><br>
When building the launcher, make sure to set the SubSystem to Windows. In Visual Studio go to the project properties, Linker, System.

## CREDITS

- PARTYMANX: Original development of Partymod. Thanks for the help and patience!
- %.gone.: Clownjob'd mod for THUG which provided useful insights
- Zedek the Plague Doctor ™: LazyStruct and LazyArray classes, Logger and ExceptionHandler and lots of script-related helper code
- Drake9X: Scaling values in C-A-S scale sliders
- Hardronix: DirectX9 and general code support
- Edem: Gameplay and script support
- Thanks to everyone who tested this mod during development!
