
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
Flickit puts the board on the right stick, the way the Skate games do. It sits in the input layer only: the stick is translated into the game's own Ollie / Flip / Grab / Grind buttons, so every trick, score and goal works as before. The face buttons keep working next to it.

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

For all this the mod looks at the skater's state and changes four places in the game's rail code: the test for the Grind button before a rail is caught, the pick of the grind, the balance of a grind (the drag is applied in the same place, once per frame of the game's rail physics), and the speed the game adds when the board lands on a rail. The code at every address it uses is compared with what is expected there first. If anything differs nothing is touched and grinds work as in earlier versions, with the Grind button held for a moment after each pop. `GameHooks=0` does the same on purpose. All of it applies to the first player's skater.

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
| Debug | 1 | Log every recognized flick (needs the console or the log file switched on) |
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
| GameHooks | 1 | The mod may read the skater's state and change the game's rail code (see Grinds). 0: none of the settings below do anything |
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
| GrindDrag | 100 | How much speed the board loses on a rail, in percent of Skate's drag (see Speed on a rail). 0: none, as in THUG. Up to 1000 |
| GrindExit | 1 | A board that has run out of speed on a rail steps off by itself. 0: it stays on |
| GrindBoost | 0 | 1: keep the speed THUG adds when the board lands on a rail. Without the drag (`GrindDrag=0`) it is always kept |

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
```
The second one also takes a pattern file: `./flickit_pattern_test skater.pat` draws every pattern in it and reports what was recognized. The third one is run from the top of the repository: it checks the grinds the mod asks the game for against the game's own grind table in `scripts/qb/source/scripts/game/skater/grindlist.q`, and the drag on a rail against the times and distances a grind lasts in Skate.

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
