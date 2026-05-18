#include "Game.h"

/**
 * @brief Entry point for the "Me and Dad" demo. Creates the Game object and
 *        starts the main loop if initialization succeeded.
 */


int main()
{
	// Step 1: construct the Game object on the stack.
	// The Game constructor (in Game.cpp) initializes SDL, opens the window,
	// creates the renderer, loads sprites, builds the Box2D physics world, and
	// spawns the initial ECS entities. If any of those fail, the constructor
	// records the failure internally and Game::valid() will return false.
	me_and_dad::Game game;

	// Step 2: only enter the main loop if initialization succeeded.
	// This prevents running the game with a half-built renderer/world, which
	// would crash on the first frame. If valid() is false, we just exit cleanly.
	if (game.valid()) {
		// Step 3: hand control to the Game. run() blocks until the player quits
		// (window close, ESC, etc.), then returns so we can shut down.
		game.run();
	}

	// Step 4: return 0 to signal a clean exit to the OS. Game's destructor
	// runs here (RAII) - it tears down SDL, Box2D and frees all resources.
	return 0;
}
