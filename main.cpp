#include "Game.h"

/**
 * @file main.cpp
 * @brief Entry point for the "Me and Dad" demo. Creates the Game object and
 *        starts the main loop if initialization succeeded.
 */


int main()
{

	me_and_dad::Game game;
	if (game.valid()) {
		game.run();
	}
	return 0;
}
