# SimpleMiner
A 3D voxel world generator based off of Minecraft, created with my custom game engine.
-----------------------------------------------------------------------------------------------
![SimpleMiner Banner](https://github.com/jswilkinSMU/SimpleMiner/blob/main/SimpleMinerHeroImg.png)

### How to Use:
	Keyboard Controls: 
		- WASD for movement
		- Hold shift for speed up when in spectator
		- LMB digs block
		- RMB places block
		- 0-9 selects respective inventory slot
		- Scrolling mouse wheel changes inventory slot.
		- X to empty currently selected inventory slot.
		- Z to empty all inventory slots.

	- General Controls: 
		- P to Pause 
		- T to enter slowmo 
		- O for a single unpaused update.
		- L to toggle lightning effects.
		- C changes camera mode.
		- V changes player physics mode.
		- K to toggle lighting color.
		- R to lock/unlock raycast.
		- Hold Y to accelerate world time by 50.
		- Hit the ESC key return to Attract mode and to quit the game.
		- Hit spacebar to exit Attract Mode and enter play mode.

	- Debugging controls:
		- Hit F2 to debug draw chunk bounds with index and vertex count.
		- Hit F3 to toggle job debug text.
		- Hit F4 to toggle player collision debug raycast arrows.
		- Hit the F8 key to reset the game.

### Features:
	- Voxel World Generation:
		- Infinite world going from player/camera position with an activation range.
		- Leaving activation range causes old chunks to deactivate.
		- Using data driven block definitions.
		- Block size is 3 bytes, holding data for type, light influence data, and bitflags.
		- Multithreaded with jobs for saving, loading, and chunk generation.
		- Saving and Loading occurs whenever a chunk is made dirty.

	- Noise
		- Caching noise into maps.
		- Density noise for shaping.
		- Continentalness using spline curves for height offsets and squashing values.
		- Biomes using temperature and humidity bands.
		- Tree stamps so that trees are built at game startup.
		- Noise caves
			- Cheese
			- Spaghetti
			- Rare large rooms
		- Ore vein clusters using 3D perlin noise.

	- Lighting and World Shader
		- Light propagation
		- World Constants holds camera position, sky color, outdoor and indoor lighting colors, fog near and far distances.
		- Indoor and Outdoor lighting values
			- Glowstone holds an indoor lighting value of 15.
		- Day to Night cycle
		- Fog using near and far distances.

	- Player Character:
		- Drawn out to resemble minecraft character.
		- Collision against blocks using multiple voxel raycast vs blocks.
		- 3 different physics modes: Walking, Flying, and NoClip
		- Animated movement
		- Blending between states

	- Inventory System:
		- Hotbar with 10 slots
		- Highlighted current selected slot
		- Mouse scrolling and number keypresses
		- Block icons
		- Max stack of 64 and text indicating current stack
		- X keypress to empty current slot and Z keypress to empty all slots.
		- Digging obtains a block and adds to slot if block doesn't already have a stack that isn't full.
		- Placing removes a block from current stack.
	
### Build and Use:

	1. Download and Extract the zip folder.
	2. Open the Run folder.
	3. Double-click SimpleMiner_Release_x64.exe to start the program.
