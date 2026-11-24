
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
| Right stick down | Crouch (Ollie is held), as deep as the stick is pulled. A third of the way down is enough, as in Skate. How far down is how high the ollie goes |
| Flick it away from there | Ollie |
| Let it come back to the centre, or let go of it | He stands up again: no ollie |
| Down, flick up-left | Kickflip (Left + Flip), with the left foot forward: a regular skater in his own stance |
| Down, flick up-right | Heelflip (Right + Flip) |
| Down, drag to the left | Pop Shove-It (Down + Flip) |
| Down, drag to the right | The trick on Up + Flip (Impossible by default, put FS Shove-It there for the Skate layout) |
| Down-right, flick up-left | Varial Kickflip (DownLeft + Flip) |
| Down-left, flick up-right | Varial Heelflip (DownRight + Flip) |
| Down-left, flick up | Hardflip (UpLeft + Flip) |
| Down-right, flick up | Inward Heelflip (UpRight + Flip) |
| Any of these with the right foot forward | The same flick mirrored, left for right, as in Skate: a goofy skater in his own stance, or a regular one riding switch, has the kickflip down, flick up-right and the pop shove-it a drag to the right (see The flicks go by the feet below) |
| Right stick up | Crouch for a nollie: THUG's nollie stance, crouched as deep as the stick is pushed. Back at the centre without a flick he stands up again, out of nollie (see Nollie below) |
| Up, flick away from there | Nollie. Every flick above works from up as well, upside down, and does the nollie version of its trick: with the left foot forward up, flick down-left is the Nollie Kickflip, up, flick down-right the Nollie Heelflip, and so on |
| A flip trick, and the stick kept where the flick ended | One more rotation each time the game has one to give and there is air enough to land it: Double and Triple Kickflip, Heelflip and Impossible, 360 and 540 Shove-It (see Holding a flip below) |
| Left trigger / right trigger | Grab with that hand (Left + Grab / Right + Grab), held as long as the trigger is |
| Nothing | Grind: rails and ledges catch the board by themselves, and the right stick says which grind it is (see Grinds below) |
| Left bumper / right bumper | Spin left + nollie / spin right + revert and switch, both together: caveman |
| Flip button or Ollie button, on the ground | Push. A tap is a small push, a quarter of a second a full one, and a button that stays down keeps pushing (see Rolling below) |
| Movement stick left / right | Steers the way Skate's board does: gently near the centre, tightly at a walk, widely at speed (see Rolling below) |
| Movement stick left / right, in the air | Turns the skater the way Skate does: by how far the stick is pushed, and faster the harder it was thrown over as the board left the ground (see Spinning below) |
| Right stick click | Flickit on / off, for looking around. On foot and in a car it stands down by itself |

The tricks named above are the default trick slots. Flickit presses the slot, so whatever is assigned to it in the Edit Tricks menu is what comes out. Manuals are unchanged (up, down on the movement stick).

### The crouch, the pop and standing up
- As in Skate, a gesture does not need the stick at its end: a crouch starts a third of the way down (`Engage=35`), and a flick that gets 40 percent of the way out of it pops (`Flick=40`). Skate's own patterns start about that far down and end under half way out; drawn through these rules, 77 of the 78 in its `skater.pat` come out as themselves.
- The pop is a flick: the stick going out of the crouch and on, in any other direction. Easing the stick back to the centre, or letting go of it so that it springs back, is not one: Ollie goes up and the skater stands up again, without a jump. So does a stick left part way out for longer than `FlickWindow`. Skate needs the flick as well: its ollie is a pattern that ends half way up the stick.
- How far the stick is pulled down is how high the ollie goes, as in Skate: from Skate's quick pop at a third of the way (the rider rises 0.38 m) to its full pop at nine tenths (0.76 m), in between in proportion. The crouch gets deeper over a fifth of a second at most, so a flick straight out of a pull is a smaller pop; easing off before the flick makes it smaller too. THUG's own pop goes by how long the jump button was down instead; with Flickit that is set aside for a pop out of the stick's crouch.
- The crouch the skater shows goes as deep as the pop would: standing at a third of the way, THUG's full crouch all the way down. THUG has one crouch, and the player's skater goes into it at once: his animations never blend (THUG blends them for the people in its levels and for the trick editor's skater, which switch it on). While the stick crouches Flickit switches blending on for him, so that the animation he stood in is still there when THUG starts its crouch, and sets the mix of the two by the stick, as Skate blends its rider into the crouch by how far the stick is pulled. The crouch animations THUG plays meanwhile (looking around, the crouched turns, the bumps) still play, on the crouched side, and now blend into each other. Easing the stick off eases him up; standing up without a pop blends him back up over 0.3 s once THUG plays the animation he stands in; then blending goes back off. A crouch that starts as he lands from a short drop has nothing standing to blend with (THUG lands him in its crouch bump and goes on to its crouch idle): then Flickit starts THUG's rolling idle under the crouch, between two frames of the loop of the ground, and the stick's depth comes in over a fifth of a second. The pop starts the ollie's animation without a blend, as THUG always does. `CrouchPose=0` shows THUG's one crouch, whatever the depth. The log says when a crouch starts to follow the stick and from which animation he stood in, and when a crouch found nothing to blend with (with what the skater's animation had).

### Nollie
- The stick pushed up is the crouch for a nollie, the way down is the crouch for an ollie: a third of the way up is enough, how far up is how high it goes, and the skater crouches as deep as the stick is pushed. A flick out of it pops; back at the centre without a flick he stands up again.
- THUG rides nollie as a stance of its own, with its own animations: its Nollie button (L2 on its PlayStation pad, the left bumper here) goes round regular, pressure and nollie, and he keeps the stance until he has jumped and landed, or it is pressed again. Skate has no stance to switch to: the stick up puts the rider's weight on the nose, and back at the centre he rides as before. So here the stick does it. As the crouch up begins, Flickit takes the skater into THUG's nollie stance, and his crouch is THUG's nollie crouch, mixed with the way he stood by how far the stick is up. Standing up without a pop takes him out of nollie again. A crouch down takes him out of nollie as well, if the Nollie button had put him there: the stick decides.
- The flick from up is THUG's nollie, and every flip trick plays its nollie animation: the nollie kickflip, heelflip, shove-it and the rest. The landing takes him out of nollie, as THUG always does.
- The flicks are Skate's nollie flicks: the ones from down, upside down. With Skate's `skater.pat` (see Stick patterns) its nollie patterns are the ones that count from up: `Nollie`, `N_Kickflip`, `N_Heelflip`, `N_PopShuvit`, `N_FSPopShuvit`, `N_VarialKickflip`, `N_VarialHeelflip`, `N_Hardflip`, `N_InwardHeelflip`, and the 360s, which press their trick twice as from down. THUG has no nollie animation for the second press that makes a 360 Heelflip (Skate's laser flip), a 360 Hardflip or a 360 Inward Heelflip: from nollie those finish in their regular animation.
- Pressure, THUG's third stance, stays as the Nollie button left it for a crouch down: its pressure ollie and pressure flips work as before. A crouch up takes him out of it, into nollie.
- In the air the stick up leans the board, as before: up at touchdown is the nosegrind.
- How: THUG has a loop of its scripts for riding on the ground and another for riding nollie, and each sets the stance as it starts. Between two frames of one of them, where no script waits for anything, the skater's script goes to the other one. Anywhere else (a landing, a manual, a brake, a bump into a wall) Flickit waits until he is back in his loop. Should a crouch up end and leave him in nollie on the ground (the game not jumping for a pop), he comes out of nollie half a second later. The log says when the stick changes his stance.
- Without the game hooks (`GameHooks=0`, or a second player) Flickit presses the Nollie button instead, `NollieTaps` times as the crouch up begins; there every crouch ends in a pop, as in THUG, and the landing brings him back. `Nollie=0` leaves the stick up alone on the ground, and the Nollie button is the only way into nollie.

### The flicks go by the feet
- Skate's flicks go by the rider's feet: the kickflip goes toward his heels. A regular rider has his left foot forward, and his kickflip is down, flick up-left, his heelflip up-right. A goofy rider has his right foot forward, and every flick is mirrored: the kickflip up-right. Riding switch puts the other foot forward, and the flicks turn round with it: a regular rider's switch kickflip is up-right, a goofy rider's up-left. The varials, the shove-its and the nollie flicks swap sides the same way. Up and down stay as they are.
- Flickit reads which foot is forward from the way the game has the skater stand (its Flipped: a regular skater starts one way round and a goofy one the other, and riding switch turns it round), on every frame from the moment it first sees him, so a level that starts with either foot forward has the right flicks from the start. Skate's `skater.pat` is written for the right foot forward; with the left foot forward its patterns are mirrored.
- A flick keeps the side it began on: switching stance while the stick crouches (THUG's switch button, or a revert) changes the next flick, not the one under way.
- The log says which foot is forward and where the kickflip is, as soon as Flickit sees the skater and whenever it changes ("left foot forward (regular): the kickflip is the flick up-left").
- THUG itself keeps its trick buttons where they are whatever the feet: Left + Flip is the kickflip in every stance. `FlicksByFeet=0` keeps the flicks that way too: then `KickflipSide` alone says where the kickflip is (0 up-right, 1 up-left). With the feet, `KickflipSide=1` turns every flick the other way round. Without the game hooks the feet cannot be seen, and the flicks are those of the right foot forward.

### Holding a flip
In Skate a kickflip or a heelflip whose stick stays where the flick ended keeps turning: a second rotation if the rider has 0.53 s of air left, a third with 0.8 s, up to four. THUG has the same tricks with one rotation more on another press of the trick button while the trick starts (its extra tricks: Double Kickflip on a Kickflip, Triple on a Double, and so on), and Flickit presses for them while the stick is held: Double and Triple Kickflip, Heelflip and Impossible, 360 and 540 Shove-It and FS Shove-It, and the back foot doubles. Skate does this for its flips only; the shove-its are THUG's.
- The press for another rotation comes a little over a tenth of a second into the trick, if the stick is still within one direction of where the flick ended and out at least 30 percent. Let it go, or roll it on round, and the trick lands as it is.
- A rotation is only pressed for when there is air enough to land it: the game's own time left in the air, against how long the longer trick's animation runs before a landing no longer bails. The log says so either way ("the stick is held: one more rotation ..." or "... not air enough for ...").
- Tricks that turn into another trick on that press (a Varial Kickflip into a 360 Flip, a Hardflip into a 360 Hardflip) are not held: holding them changes nothing.

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

The board's own angle to the rail has its say too, as in Skate. THUG turns the board to whatever grind it is handed, so with the stick alone a board that comes in at 40 degrees with its nose over the rail would be swung straight for a 50-50. Instead, a board at an angle counts as the stick pushed the way its nose points, "toward" when the nose points onto the rail:

| The board at touchdown | Grind |
| --- | --- |
| Along the rail (under 16 degrees off it) | The stick alone, as in the table above |
| At an angle, stick at rest | Crooked when the nose points onto the rail, smith when the tail does: the end that points onto the rail is the one that gets there first |
| At an angle, a crooked or overcrook asked for | Crooked or overcrook, whichever is angled the way the board is |
| At an angle, a feeble, smith or salad asked for | Feeble when the nose points onto the rail; smith (or the salad) when the tail does |
| At an angle, up / down / a slide asked for | As asked: nosegrind, 5-0, the slides |
| Across the rail (THUG's own line, about 44 degrees off it) | The slides as asked; a grind on the trucks (nosegrind, 5-0, crooked and the rest) becomes a boardslide or lipslide, which THUG picks by the end that went over the rail |

Coming onto a rail backwards along it, THUG turns the skater round for its grinds, and there the stick alone picks. `GrindAngle` moves the line between along and at an angle (in degrees, 5 to 44); `GrindByBoard=0` leaves it all to the stick, as before.

On the rail:
- Stick down is the 5-0, up the nosegrind, let go the 50-50. The game takes such a switch once a grind has settled, about three quarters of a second in. Only these three switch; any other grind stays what it is.
- A flick pops off the rail, with the flip the flick asks for. Letting the stick go does not pop. A flick is a fast move: taking the stick from one end to the other slowly switches the grind instead.
- The right bumper drops off the rail (THUG SDL's drop-down key).
- The board does not drift off balance, so there is no balance meter on a rail either. `GrindBalance=1` gives the balance meter its job back, and with it its place on the screen. Manuals and lip tricks keep their meter, and their balance is THUG's.
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
- After at least a second on the rail, a board that has all but stopped comes off it the way it does in Skate: it is pushed off to one side and drops. Off a ledge it always goes over the edge, away from the top, so the skater lands on the ground and not on the ledge (Flickit looks for a surface right next to the rail with THUG's own test for a ledge). Off a rail it goes to the side the board came from, and with a surface on both sides as well. THUG's own way off a rail does the rest: it lifts the skater 5 units and puts him 5 units off to that side, and from there he drops with Skate's speed sideways, about 1 m/s (80 units/s on the trucks, 72 in a 5-0, 60 with truck and deck on the rail and across it, 83 in a nose- or tailslide, 26 on the trucks with a surface on both sides). Next to a vert ramp he skates in, as THUG does on its button. On a rail that goes uphill the board steps off at the point where THUG would let it slide back down. `GrindExit=0` leaves the board on the rail.
- THUG pushes the board every time it lands on a rail, by 150 units of speed, which is a third of a kick. Skate does not, and with a drag on the rail that push would be a way to get the speed back by hopping. So while the drag is on, next to nothing is left of it. `GrindBoost=1` brings it back.
- The salads slow down like the rest. THUG's scripts make the salad a grind you switch to from another grind with a double tap, and such a grind leaves the balance of the rail to the grind before it. Picked by the stick as the board lands there is no grind before it, so the game balanced nothing on the rail and the drag, which goes through that balance, never came. Flickit starts the balance for it the way the scripts start it for every other grind.

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
- No animation, no push. The game's scripts decide when the push is played: a push asks them, as a rule they start it on the next frame, and while it plays they wait for it to end. So with the button held one push follows the other at the pace of the animation, and a second tap during a push is one more push after it. After a landing, after skating off the end of a rail and after a bump into a wall, THUG's scripts wait for that animation to end before they look for a push; Skate's skater does not wait, so a push cuts the animation short. It starts as soon as the scripts are past the few frames after a landing in which a revert or a manual can still begin: a sixth of a second, a twelfth after a vert air. Where the scripts are busy with anything else, a button that stays down gets its push when they are done, and a tap gives up after three quarters of a second.
- What comes before a push follows Skate's order on the ground: the crouch for an ollie or a nollie (the wind-up on the stick) comes first, then the push, then the brake and the turns. THUG's scripts play a push and wait for it to end, so until now nothing else could start before the animation was over. Now a crouch takes over from a push once its foot has gone down and pushed, never more than about a third of a second into it, so the push's speed is kept; Skate's wind-up waits for the start of a push in the same way. Down takes over at once, to one side or not (THUG never kicks with Down held): it has stopped the push already. No push starts in a crouch, not even THUG's own push from a crouch: a button held through the crouch pushes once it is over, a tap gives up as above. One safeguard: should the game be told to play the push three times running without the mod seeing a push animation on the skater, pushes go by a clock from then on (0.8 s each, from the moment the game is told) until one is seen, and the log says so.
- THUG's scripts look at Left and Right before they look for a push, and show the turn. Skate pushes in a turn like anywhere else, so with `PushOverTurn=1` the scripts do not get to see Left and Right while a push is asking for its animation and they are in their loop for the ground, which is where they look for a push (not in a crouch, where no push is played): they play the push, and the board turns all the same. `PushOverTurn=0` is THUG's order, the turn first: then there is no push while the stick is held to one side.
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
- Down does nothing to the turn, as in Skate, where only how far the stick is pushed to the side counts. THUG reads Down from how far down the stick is, whatever it does to the side: from 40% of the way down. So a stick pushed all the way to one side and a quarter of the way round toward Down is Down to THUG as well. Its sharp turn (3.6 radians a second) used to take over there, and the trucks came back straight when the thumb came up again. Down still brakes where THUG brakes, and ends a push.
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

### Cars
Any car of the level can be taken, a moving one too, the way GTA has it: walk or skate up to a car and press Circle (B on an Xbox pad), the button the game gets into its own drivable cars with ("Press Circle to drive"). Only the Circle button itself does it: a grab the triggers make with `TriggerGrabs` takes no car. Near enough is `CarReach` units (45 by default) from the car's body, from any side.
- The car becomes the game's own drivable car (the one its goals use, `PlayerVehicle`) with the model of the car taken: it is put where that car was, facing its way, on the ground, and a car that was moving goes on at the speed it had. The physics are those of one of the game's own drivable cars: the one made for that model where there is one (the custom car, Tampa's police car, San Diego's security cart), otherwise the police car's, with colliders and wheels of the model's own size (measured from each model, in `scripts/flickit/cars.q`). The game's own buttons drive it; a panel shows them for five seconds.
- While a car is in reach, the game's own "Press Circle to drive." box shows, the one it shows by its own drivable cars. When another player is nearer than the car (see Other players), Circle is his.
- The camera is the game's own for its drivable cars (55 units up, 240 back), higher and farther back for a taller car and a little farther back for a longer one, so that the road ahead shows over a box truck (`Drive::CameraFor`; passed to the game's own `VehicleCamera_Reset`).
- The body is drawn where its model has it. The game's drivable car takes the place of the body from the model's hierarchy as (-x, z, -y) and draws the body there with the model's x as it is (`CVehicleComponent::Finalize`, 0x0045E516, and its drawing, 0x0045A6E9): the body's side-to-side place the wrong way round. THUG's own drivable cars have their bodies' places in the middle and it did not show; the NY police car's model has its body's 34 units to the side, and the body was drawn twice that far off its wheels. The DLL turns that x round once the car is made (the place is only drawn with: the colliders are the setup's).
- Holding Triangle stops it and gets him out once it is slow, as the game has it for its own cars. He stands beside the driver's door on his board (on the other side, behind or in front of it when there is a wall or no ground there; on the roof when there is room nowhere), and the car stays there, to be taken again. It stands on the ground under it, on the ground's slope: the driven car's place is its body up on its springs, so the DLL finds the ground under the car's middle and wheels (the skater's own line test) and puts the parked car's model there.
- A car he left is set up as the game sets up a car of the level, standing still: its model's own nodes (`nodearray`, from the model's `.qb`) and the rails on its roof (`railmanager`), which the game's car factory gives every car of the level (0x00506099). He can grind its roof, and its gap triggers work. Without its own nodes, a gap trigger on its roof (a cab's) made the game look for the trigger's node in the level's nodes (0x00456840), find none and read through nothing: the crash at 0x004110B3.
- The level keeps the cars it has: the car he drives, and then the car he left, is the one he took, not one more. A car he left stands `CarParkedTime` seconds (two minutes); then, once he is 2500 units from it and from the start of the car's way, it goes and in the same frame the car it was is at the start of its way again, driving its round, so that no car goes or comes before his eyes. A car taken again and left again counts its time from then, and is still the car it was. At most ten cars stand on their own at once (the cars he left and the cars knocked off their way in a crash, together); another takes the place of the one left longest ago, one 2500 units from him if there is one, and that one's car comes back at the start of its way when he is away from there. So does the car he drove when it goes some other way (a restart). Cars that are there from the level's start (`CreatedAtStart`) come back; a car a goal makes does not (its car he left just goes).
- The game's Vehicle nodes whose models are not made for the game's drivable car (buses, trams, the skitch node) are not taken. Nothing happens in net games.
- A trigger face whose node the game cannot find is left alone. When a car's feelers (or the skater) touch a trigger face, the game runs the TriggerScript of the face's node (0x00456840): it looks the node up in the nodes of the object the face is on, or in the level's, and reads the script from what it found without looking. A node missing from the level's nodes, or an object whose own nodes are an empty list, left it reading through nothing (the crash at 0x004110B3, driving into the barricades on Jersey's highway); an object whose own nodes do not name the face ran their first node's script instead. The DLL now starts that function itself: it finds the node the game's way and runs the game's function only when there is one, and writes down once which node was missing (in the log and in `flickit_watchdog.txt`, lines that start "Triggers:").

#### Crashes
The game's drivable car has a rigid body but goes through other cars, and the cars of the traffic follow their ways with no physics at all. So the DLL works the crashes out itself (`src/crash.h`, tested by `tests/crash_test.cpp`):
- Each car is a box along the ground: its rectangle turned with it (from its colliders, as for taking it), between a little under its wheels and over its roof. Two boxes that overlap touch along the axis they overlap least on (or up and down, when one has come down on the other's roof), and the touch is an impulse with a quarter of the speed into each other coming back, friction along it, and the turn it gives each about up.
- His car is the game's own body: the impulse goes into its momentum and its spin's momentum (`CVehicleComponent` +0x28 and +0x48, as `Vehicle_Kick` does), the game's own function makes its speeds from them (0x00457AD0), and it is woken. It turns about its place, as the game's body does.
- A car of the traffic cannot be pushed. A hit that would change its speed by 60 units a second or more (from his car, or from a car thrown at it) knocks it off its way: it dies, and a car of the same model stands in its place under one of the names of the cars standing on their own (`Flickit_CarKnocked`, the same object as a car he left), moving and turning as the hit left it. A softer hit only pushes the other car away from it, as a heavy car would.
- A car standing on its own (knocked off its way, or a car he left that is hit) is a box of the DLL's own: it falls, bounces off the ground at a fifth of its speed, slides with friction, on its wheels rolls along the way it faces and holds across, turns, rolls over, and comes to rest once it has been nearly still for half a second. The ground under it and the walls around it come from lines into the level from its middle (the feeler of the skater's physics, without the moving objects, as the game's own `MoveToNode` looks for the ground; the place hit and the face's normal are the feeler's own), one down and four along the ground, every frame it moves; the DLL puts its object where its box is. One with no ground under it for five seconds fell out of the level and goes.
- Cars that meet at 700 units a second or more (about 40 mph between them) throw the one hit up and roll it over the way it was pushed, more for more (`CarLaunch`, percent). His own car only from 1000, and half as much: the game's car rights itself in the air.
- Damage: each hit's change of speed past 100 units a second, a loose car's landing or wall past 300, and his own car's change of speed in a frame past 260 (a wall, a post: more than its driving does), times `CarDamage` percent, comes off a car's 1000. Under 600 it smokes, under 300 it smokes heavily, at nothing it catches fire, and five seconds later it blows up: the game's fireball and its sound, the car thrown up and burnt dark (its model's own lights), the cars within 600 units pushed away and damaged (one already damaged and near enough catches fire in its turn), the cars of the traffic there knocked off their way, and him on foot within 350 knocked down (the game's own `PedKnockDown`, as a person who walks into him). A car on fire, burnt out, on its roof or side, or still moving cannot be taken.
- His car keeps its damage while he drives it and when he leaves it; a car he takes that was damaged is as damaged. When it catches fire a panel says so (hold Triangle to stop and get out). If he is still in it when it blows up he is thrown out (the game's own way out, then knocked down) and the car he leaves is burnt out and thrown.
- The effects are particle systems of the game's own kind, made by Flickit's scripts and locked to the car (`Obj_LockToObject`, the game's own way for the skater's): smoke at the engine, heavy smoke, fire, the smoke of a car burnt out, with the textures the game always has loaded. Their places are relative to their object (`LocalSpace` and `UseStartPosition`, as the game's fireball has them; without them the game takes the places as places in the world). Each effect is made under a name never used before: the game keeps its particle systems in a table by name (`Nx::CParticleManager`, 0x006FF6E8) and takes one out by name only once its object is deleted after the frame it died in, so an effect made again under the same name in that frame left the dead one's entry behind, and the table's update later called it through freed memory (the crash at 0x00421BEA, its stack 4 off). The crash's sound is the bang of metal the game's car bail makes, as loud as the crash was hard and as near as it was.
- What the crashes do goes into `flickit_watchdog.txt` on lines that start "Crashes:". Nothing happens in net games. The code at every address it uses is compared first; if anything differs, the cars go through each other as before (and can still be taken).

How it is done: the steps are scripts of Flickit's own, in the game's script language (`scripts/flickit/cars.q`, compiled by `scripts/flickit/qbc.py` into `src/flickit_cars_qb.h`; the compiler gives the game's own compiled files for 174 of the sources in `scripts/qb/source`). The DLL gives them to the game once at start-up, through the game's own reading of a script file from memory, where THUG-SDL makes its own changes to the scripts. They do what the game's own scripts do for its drivable cars (`Switch_OnGroundAI`, `ToggleUserControlledVehicleMode`, `Vehicle_Kick`, `CreateCompositeObject`), and ask six commands of the DLL's for the rest: where the car is and how fast it goes (from where it was on the frames before), the car put at a place (the game's own `Vehicle_MoveToRestart`, given a node of the DLL's making), whether Triangle is up, the place to get out at (through the skater's own line test), the skater put there (the game's custom restart, as the map does), and the car left as it stood. Once a frame, after the game has read the skater's pad, the DLL looks at where each car of the level is: the nodes of class Vehicle with the car's skeleton, by name. Once a second it sends back a car he left that has stood long enough (`Flickit_CarGoesBack`: the parked car dies, and the game's own `Create` makes the car it was from its node) and brings back a car of the level that went for good (`Flickit_CarBack`); which cars are there from the level's start is the game's own `CStruct::ContainsFlag` on each node. The code at every address it uses is compared with what is expected there first; if anything differs, or the game did not take the scripts, there are no cars to take.

### Other players
The start of an online mode: other skaters in the same world, each with a card of who he is. For now they are fake, so that everything about seeing another player can be tried from one end: they skate runs you record yourself, with made-up names, sponsors, story and stats. What a real player's card shows will come over the network instead (his profile, his sponsors, his story).
- F6 (`RunKey`) starts the recording of a run and F6 again stops it; REC and its time show at the top right while it records. Up to 30 times a second, and on every frame an animation starts, it keeps where the skater is, the way he is turned as he is drawn (his display matrix, as a quaternion), and the animation he is in: which, where in it, from and to, how fast, how it loops, and whether it was mirrored when it started. Pauses do not count. It stops by itself after five minutes, when he gets in a car, or when the level changes; a run under 3 seconds is not kept. It is saved as `flickit_runs\run_<date>_<time>.run` next to the exe (56 bytes a frame; `src/players.h` reads and writes it).
- Each run of the level is a player: one of the level's own people on a board (the game's skateboarding pedestrian, `Ped_Skateboard_a`, whose head, body and legs the game picks from the level's own), with the skater's skeleton and animations, skating the run round and round. The last eight runs of the level have players, every time the level loads; a run comes in as soon as it is saved, at the place it started. His name is over his head out to 1500 units, in a text element of the game's that the DLL puts at his head every frame.
- His identity is made up from the run's file name, the same every time (`Players::FakeIdentity`): an online name, a sponsor level (Unsponsored, Am or Pro), the board company and the shoe company he rides for when he has a sponsor (the board companies the story lets you choose, the shoe companies of the game's pros that the game has a logo of, with DC Shoes for Axion, which it has none of), where his story is (three story levels for each sponsor level, in the story's order), how many goals he has, and his ten stats, 3 to 10 (a created skater starts at 3; an Am's are higher, a Pro's higher again).
- The player Circle is for is the one he is looking at: in front of the skater where the camera looks (40 degrees either side), out to `PlayerAimReach` (600 units, about 15 m), or anywhere right by him (`PlayerReach`, 150 units from the player's edge); the nearer and the straighter ahead the better, and the one picked stays while another is not clearly better. His name turns orange and the game's prompt box says "Press Circle to view (name).". Once he has gone past and nobody else is there he stays picked for `PlayerLockMs` (4 seconds), while he is within twice the reach, so a player going by fast can still be had. Circle opens his card when the skater is on the ground (in the air it is the game's own Circle). Circle is the button the cars use: with a car in reach too, the nearer of the two has it.
- The d-pad's down (`PlayersButton`; P on the keyboard, `PlayersKey`) opens the list of the players there: the nearest first, each with his sponsor level's colour, his name, how far he is in metres and an arrow for which way (up is where the camera looks). Up and down on the d-pad choose, right opens his card, and left on that card comes back to the list; left (or P) closes the list. Not in a car.
- The card is drawn over the game with the map's letters: his sponsor level as a badge, his name, the logos of his board and shoe companies, where his story is and his goals, his ten stats as bars, and below them what can be done with him. Play S.K.A.T.E., Race and Join party are there, marked "soon": they come with online play. Close closes it. The card and the list take only the d-pad (or the keyboard's arrows): up and down choose, right takes, left closes. Everything else on the controller stays the game's, so he skates on while one is up: the left stick steers, Flickit's right stick flicks, A and X push and the triggers grab. The game is not paused under them either: the other players skate on, as they would online. They sit at the right of the screen, out of the skater's way, and the world is seen through them (the game behind is a little dimmed, the card and the list are 78% solid). The d-pad is kept from the game while one is up, and after closing until it is let go (Flickit's own directions for its flip tricks still reach the game). The logos are the game's own pictures, the ones it puts on a shirt for each company (`cas_logos.q`): the DLL reads them from the game's `data\pre\skaterparts.pre` the first time a card needs them (`src/gamefiles.h` reads the archive and the picture: the game's LZSS, its 16-colour swizzled images); where it cannot, the company's name is there instead. It needs the map's file (`flickit_map.bin`) for its letters; without it the card shows as words in the game's message box.
- Nothing happens in net games.

How it is done: the players are scripts of Flickit's own (`scripts/flickit/players.q`, compiled into `src/flickit_players_qb.h`) that the DLL gives the game at start-up, as it does the car scripts. A player is made the way the goal editor makes its people (`CreateCompositeObject` with a skeleton, a model and the human animations, then `Obj_SetPosition` and `Obj_InitModelFromProfile`), but without its motion component (the DLL moves him) and with the one that draws an object turned the way its matrix is (`setdisplaymatrix`, the drivable car's); he idles (`Obj_PlayAnim Idle`) until the DLL takes him on. The DLL spawns that script, so that it runs with the game's other scripts and not in the middle of the skater's update, and gives every player it makes a name never used before (`Flickit_Player<n>_<count>`). It then looks for him by that name (`Obj::ResolveToObject`) for three seconds; if he is not there it makes him again under a new name, three times at most in a level. Once a frame, after the game has read the skater's pad, the DLL moves each player along his run (his place between two frames, his turn halfway between theirs), and starts his animation where the run started one, through the game's own start of an animation (what `PlayAnim` does once it has read its parameters), mirrored as it was: the game hands an object's flip (bit 29 of its animation component's flags) to each animation as it starts, so the bit is set first. The time in it is the run's on every frame. Which way the camera looks is the renderer's own camera, as it last made a view of the level (its eye, and its right from its view matrix, whichever way round the matrix is kept), only read; without it, the way the skater faces. What happens to each player goes into `flickit_watchdog.txt` (lines that start "Players:"): when he is made, when he is found or that he was not, and three seconds after: where he is, his animation and its time, how many animations were started on him and how many he has not, his object's flags, and whether his label is there. The code at every address it uses is compared with what is expected there first; if anything differs, or the game did not take the scripts, there are no players.

### Memory for big levels
THUG-SDL gives the game a main block of 360 MB instead of its own 45 MB, but the game is a 32-bit program: on a 64-bit Windows it gets 2 GB of address space, or 4 GB when the exe carries the large-address flag. New Jersey with 4x textures is about 75 MB of them, so an open world of several such levels needs the flag.

`.github/flickit/large-address` (in the build's zip as `large-address`) has a PowerShell script, and a batch file each to run it, that sets or clears that one bit on `THUG-SDL.exe` and keeps a copy of the exe the first time. Whether THUG copes with memory above 2 GB is not known yet, so there is a test: with `HighMemoryTest=1` in a `[Memory]` section of the ini, and the flag set, THUG-SDL reserves all the free address space below 2 GB while it loads, before the game asks for anything. Nothing is committed behind it, but everything the game allocates from then on lands above 2 GB, where code that treats an address as a signed number goes wrong at once. The log says how much address space there is and whether the exe has the flag, how much the test holds, where the game's main block ended up, and (on every F9) how much of the address space is in use.

THUG keeps its scripts and each level's node array (rails, objects, restart points) in a script heap of its own, 3.9 MB, with fixed pools of script objects inside it (components, structures, vectors, arrays, names, running scripts). A full pool crashes the game: its allocator hands back nothing and the caller writes through it. New Jersey alone fills about 80% of the vector, component and structure pools, and two levels joined into one need about one and a half times what they hold. With `BigScriptMemory=1` in `[Memory]` (the default) THUG-SDL makes the heap and each pool 8 times as big while it loads, before the game makes them: the sizes the commented THUGPLUS block in `config.cpp` lists, with the heap at 31.6 MB of the 360 MB main block. The same switch makes the pool that every hash table in the game takes its overflow items from 8 times as big (12500 to 100000 items of 12 bytes), which a joined New Jersey and Manhattan also runs out of. The sizes are compared with what is expected first; if any differs nothing is changed. `BigScriptMemory=0` leaves the game's sizes.

### The guard
A crash while driving (at `0x0041FBB3`, a read of `0x003B0042`) had the game's pointer to its object manager, at `0x0069B6C0`, at `0x003B003A`: something had written over it, and the score's next look-up of an object by its name went through it. Every such look-up does (`ObjectExists`, `Obj::ResolveToObject`); the game makes the manager once when it starts and ends it when it closes, and those are its only three writes to the pointer (it reads it in 51 places). What wrote over it is not in the crash.

So the DLL puts a write breakpoint of the processor's on that word in every thread of the game (`src/guard_game.cpp`): a thread of its own stops each thread in turn and sets its debug registers, once the manager is there, again for a thread that starts later, and every ten seconds. Its exception handler, first in line, sees the write as it happens. A write from one of the game's own three places is taken; any other write is written down (the instruction it came from, the registers, the code up to it, the returns of the calls on the stack, the script that ran, where the skater was) and the pointer goes back to what it was before the game reads it again. A test write at the start tells whether the breakpoint works on the system; once a frame the pointer is looked at as well, for a change the breakpoint did not see. The notes go to the log and to `flickit_watchdog.txt`. What it decides, and how it tells a return address on the stack, is in `src/guard.h` (tested by `tests/guard_test.cpp`); the CI log lists where the DLL's own functions are (from the linker's map), for a write that came from the DLL.

### What the mod changes in the game
For the grinds the mod looks at the skater's state and changes five places in the game's rail code: the test for the Grind button before a rail is caught, the search for a rail that follows it, the pick of the grind, the balance of a grind (the drag is applied in the same place, once per frame of the game's rail physics), and the speed the game adds when the board lands on a rail. For a salad, whose script starts no balance of its own, it starts the rail's balance with the game's own command for it (DoBalanceTrick), as the scripts do for the other grinds. When the speed is gone it looks for a surface on either side of the rail with the game's own test for a ledge, starts the game's own script for getting off a rail (SkateInOrBail) with the side and the step it picked, and gives the skater Skate's speed sideways once he is in the air. (THUG's button for that script always sends the skater to his right unless THUG SDL's directional drop-down is on; the button itself is left as it is.) For the jumps it stands in for the game's own answer to "what is the gravity in the air", and scales the speed of a jump at the point where the game has worked it out. For rolling it stands in for four things in the game's physics of the ground (the test for braking, the test for a kick, the friction and the turning) and for three commands of the game's scripts (CanKick in the nollie loop, Held when it is asked about the Flip button, and AnimFinished when the scripts wait for the end of a landing, a skate-off or a bump while a push asks for its animation) and for the wait of a script for the end of a push animation that gives way to a crouch or to Down, it holds the game's "auto kick" setting off for the skater, and with `PushOverTurn=1` it keeps Left and Right from the game's scripts while a push asks for its animation and they are in their loop for the ground (the game's physics see both as they are). For these it reads which script the skater's is in, and which one called it, and whether he crouches. For the push it also reads two things without changing them: which animation the skater is in and how far into it, and where his two feet are, through the game's own function for the place of a bone. For the crouch it sets, before the jump, how long the game thinks the jump button was down, scales the speed of the jump with the depth of the crouch, and takes a crouch back before the physics see the button go up when the stick came back without a flick. For a held flip trick it reads the tricks the trick in progress can turn into, the game's clock, the time left in the air and the length of an animation, all without changing them. For the depth of the crouch it switches the skater's animation blending on while the stick crouches (the switch Obj_EnableAnimBlending sets), and stands around two calls in the game's animation of an object, for the first player's skater only: where PlayAnim starts a blend (the animation he stood in stays in the blend), and where the pose is made from the blend (that animation gets the share the stick asks for). It finds the animation he stood in among the skater's blend channels and changes nothing else of them. For the flicks it reads which foot the skater has forward (the game's Flipped). For the nollie it moves the skater's script between the game's two loops for riding on the ground (OnGroundAi and OnGroundNollieAI, which set the stance as they start), between two frames of them, and for the stick's nollie it lets go of the game's pressure stance (one byte of the component that keeps the stances). For spinning it stands around the game's turning in the air, which for that one call sees Left or Right held the way the stick's turning goes, and answers for the skater's rate when the game asks for it there. The code at every address it uses is compared with what is expected there first. If anything differs nothing is touched and grinds work as in earlier versions, with the Grind button held for a moment after each pop. `GameHooks=0` does the same on purpose. All of it applies to the first player's skater, while he is on his board and Flickit is on.

The settings live in the `[Flickit]` section of `thugsdl.ini`. Thresholds are percent of full stick deflection, times are milliseconds. Press F9 in game to re-read the section without restarting.

| Key | Default | Meaning |
| --- | --- | --- |
| Enabled | 1 | Master switch |
| KickflipSide | 0 | 1: mirror every flick left / right, on top of the feet. With `FlicksByFeet=0`, 0 is the kickflip up-right and 1 up-left in every stance |
| FlicksByFeet | 1 | The flicks go by the foot the skater has forward, as in Skate: the kickflip toward his heels, up-left with the left foot forward (a regular skater in his own stance), up-right with the right foot forward, and riding switch turns them round (see The flicks go by the feet). 0: the same flicks whatever his feet |
| TriggerGrabs | 1 | Triggers grab, bumpers take over nollie / revert / caveman. 0 leaves triggers and bumpers as bound |
| AutoGrind | 1 | Hold Grind after every pop. Only where rails do not catch by themselves (`GameHooks=0` or `GrindCatch=0`) |
| AutoGrindDelay, AutoGrindHold | 200, 1500 | When the hold starts after the pop and how long it lasts |
| Nollie, NollieTaps | 1, 2 | The stick up crouches for a nollie, in THUG's nollie stance, and the flicks from there are the nollie and the nollie flips (see Nollie). 0: on the ground the stick up does nothing. NollieTaps: without the game hooks, how many presses of the Nollie button get from regular into nollie |
| ToggleButton | 8 | SDL button for on / off (8 = right stick click, 99 = none) |
| ReloadKey | 66 | SDL scancode that re-reads this section (66 = F9, 0 = none) |
| Debug | 1 | Log every recognized flick. Needs the console switched on (`Console=1` under `[Logger]`); with `WriteFile=1` as well the same lines go to `debug.txt`. `WriteFile=1` alone logs nothing |
| Deadzone | 25 | Below this the stick counts as centered |
| Engage, Release | 35, 30 | A crouch starts above Engage and is left below Release (Skate's gestures start about a third of the way down) |
| Flick, Snap | 40, 60 | How far the flick out of a crouch has to reach to pop, and how far for its direction to be taken at once |
| FlickWindow | 200 | How long after the pop the flick may arrive. Also how long the stick may stay part way out of a crouch before the skater stands up without a pop |
| Settle | 50 | How long a drag to the side has to rest to count. Also how long the stick has to rest at the centre after a crouch for him to stand up |
| Dwell | 60 | How long the stick has to rest in down-left / down-right to make it the start |
| AfterPop, DirLead, Pulse | 10, 15, 30 | Timing of the synthetic direction + button press |
| Patterns | 1 | Decide tricks from a stick pattern file when one is found (see below). 0: always use the built-in directions |
| PatternFile | skater.pat | Name of the pattern file, looked for in the folder `thugsdl.ini` is in |
| MissTicks | 10 | How many 60 Hz ticks the stick may spend between two points of a pattern before the pattern is dropped |
| GameHooks | 1 | The mod may read the skater's state and change the game's code (see What the mod changes in the game). 0: none of the settings below do anything |
| GrindCatch | 1 | Rails catch without the Grind button |
| GrindPick | 1 | The lean of the board picks the grind. 0: the game's direction buttons do |
| GrindSwitch | 1 | On a rail, stick down / up / let go switches between 5-0, nosegrind and 50-50 |
| GrindBalance | 0 | 0: the board does not drift on a rail, and no balance meter is shown there. 1: the game's balance and its meter |
| GrindMirror | 0 | 1: swap "toward the rail" and "away from it", in case the grinds come out mirrored |
| GrindByBoard | 1 | The board's angle to the rail at touchdown has its say in the grind (see Grinds). 0: the stick alone |
| GrindAngle | 16 | Degrees: a board closer than this to the line of the rail counts as along it, and the stick alone picks. 5 to 44 |
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
| CrouchPose | 1 | 1: the crouch the skater shows goes as deep as the stick is pulled (see The crouch, the pop and standing up). 0: THUG's one crouch |
| Push | 100 | The push, in percent of Skate's: what one push adds and how fast pushing gets (see Rolling). 0: the game's own kick. 25 to 400 |
| PushOnOllie | 1 | The Ollie button pushes too while the board rolls on the ground. 0: only the Flip button does |
| PushOverTurn | 1 | The game shows a push in a turn as well. 0: in a turn it shows the turn and not the push, as THUG does |
| PushMeasure | 1 | The skater's feet are watched in the game's push animations, and the speed of a push comes in while the foot pushes (see Rolling). 0: the next two settings say where that is |
| PushFootDown, PushFootUp | 8, 32 | Where in a push animation the foot pushes, in percent of the animation's length: for the first push of a session, and for an animation the feet could not be made out in |
| Coast | 100 | What the board loses while it rolls, in percent of Skate's (see Rolling). 0: the game's own friction. Up to 1000 |
| Steering | 100 | The grip of the wheels in a turn, in percent of Skate's (see Rolling). 0: the game's own turning. 25 to 400 |
| Spin | 100 | How fast the stick turns the skater in the air, in percent of Skate's (see Spinning). 0: the game's own turning in the air. 25 to 400 |
| Cars | 1 | Circle by a car takes it, and the car stays where he gets out (see Cars). 0: off |
| CarReach | 45 | How near a car's body he has to be to take it, in game units. 10 to 200 |
| CarParkedTime | 120 | Seconds a car he got out of stands before it goes back (and the car it was is at the start of its way again). 10 to 3600; 0: it stays |
| CarCrashes | 1 | The cars crash into each other (see Crashes). 0: they go through each other |
| CarDamage | 100 | How much a crash hurts a car, in percent. 0: no damage. Up to 500 |
| CarLaunch | 100 | How high a hard crash throws the car hit, in percent. 0: not at all (a car that blows up still jumps a little). Up to 300 |
| Guard | 1 | A write breakpoint on the game's pointer to its object manager: a write from anywhere else is written down and put back (see The guard). 0: off |
| FakePlayers | 1 | The runs you record are skated by fake players, each with a card (see Other players). 0: off |
| PlayerReach | 150 | How near another player, anywhere around him, Circle opens his card: game units from the player's edge. 20 to 400 |
| PlayerAimReach | 600 | How far in front of him, where the camera looks, Circle opens a player's card: game units. 100 to 3000 |
| PlayerLockMs | 4000 | How long the player picked stays picked once he has gone past, in ms. 0 to 15000 |
| PlayersButton | 12 | Controller button that opens the list of the players there (SDL numbering; 12 = the d-pad's down) |
| PlayersKey | 19 | SDL scancode that opens the list of the players there (19 = P) |
| RunKey | 63 | SDL scancode that starts and stops the recording of a run (63 = F6, 0 = none) |

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
- Names: `Ollie`, `Kickflip`, `Heelflip`, `PopShuvit`, `FSPopShuvit`, `VarialKickflip`, `VarialHeelflip`, `Hardflip`, `InwardHeelflip` press the slots of the table above. `360PopShuvit`, `FS360PopShuvit`, `360Flip`, `Laserflip`, `360Hardflip` and `360InwardHeelflip` press the slot of their base trick twice, which is how THUG turns a Varial Kickflip into a 360 Flip. `Nollie` and the names that start with `N_` are the same tricks from nollie, drawn from up (see Nollie); they count with `Nollie=1`, the default. Other names are ignored.
- The crouch and the pop work as before: the stick in the lower half crouches (in the upper half for a nollie), and the completed pattern is the pop. On a rail and in the air nothing is held, so there only a completed pattern counts, and on a rail the patterns that start at the top count too.
- Skate's patterns are written for a rider with his right foot forward. With the left foot forward every pattern is mirrored left / right, and `KickflipSide=1` mirrors them once more (see The flicks go by the feet).
- With `Debug=1` the log shows the pattern that was recognized and how fast it was drawn (speed 1 = fast, 0 = slow; measured, not used yet).

The recognizer has offline tests that need neither the game nor Windows:
```
g++ -std=c++11 -I src tests/flickit_test.cpp src/flickit.cpp -o flickit_test && ./flickit_test
g++ -std=c++11 -I src tests/flickit_pattern_test.cpp src/flickit.cpp -o flickit_pattern_test && ./flickit_pattern_test
g++ -std=c++11 -I src tests/flickit_grind_test.cpp src/flickit.cpp -o flickit_grind_test && ./flickit_grind_test
g++ -std=c++11 -I src tests/flickit_physics_test.cpp src/flickit.cpp -o flickit_physics_test && ./flickit_physics_test
g++ -std=c++11 -I src tests/flickit_stance_test.cpp src/flickit.cpp -o flickit_stance_test && ./flickit_stance_test
```
The second one also takes a pattern file: `./flickit_pattern_test skater.pat` draws every pattern in it, as it is (the right foot forward) and mirrored (the left), and reports what was recognized. The third one is run from the top of the repository: it checks the grinds the mod asks the game for against the game's own grind table in `scripts/qb/source/scripts/game/skater/grindlist.q`, which of the grinds started by name leave the balance of the rail to the mod (the salads, in `grindscripts.q`), the way off a rail against the game's scripts for it and against Skate's push worked out tick by tick, the drag on a rail against the times and distances a grind lasts in Skate, the reach of a rail against the distance to it worked out the slow way and against THUG's own reach, and the search for a rail against trying every piece of rail one by one. The fourth is run from there too: it checks THUG's jump, kick and turning numbers in the code against the game's own `physics.q`, what gravity and pop come to against Skate's jump, the pushes against Skate's curves with a stand-in for the game's scripts and its push animation, the watching of the feet against made-up animations, the rolling out and the steering against Skate's curves, all at several frame rates, and the spinning tick for tick against Skate's rules written down a second time. The fifth checks the stance the stick asks for and what the game's side does with it, and what that relies on in the game's scripts (the two loops, the landing, the nollie animations of the flip tricks); it is run from the top of the repository as well.

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
