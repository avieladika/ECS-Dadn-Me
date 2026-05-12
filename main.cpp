#include "Game.h"

/**
 * @file main.cpp
 * @brief Entry point for the "Me and Dad" demo. Creates the Game object and
 *        starts the main loop if initialization succeeded.
 */

void run_tests();

int main()
{
	run_tests();

	me_and_dad::Game game;
	if (game.valid()) {
		game.run();
	}
	return 0;
}
