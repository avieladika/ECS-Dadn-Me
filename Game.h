#pragma once

#include <SDL3/SDL.h>
#include <box2d/box2d.h>

/**
 * @brief Top-level "Me and Dad" game class. Owns SDL and Box2D resources
 *        and runs the game loop that drives all ECS systems.
 */

namespace me_and_dad
{
	/**
	 * @brief Orchestrates the game: SDL window/renderer, Box2D world, level
	 *        spawning, and the main per-frame loop.
	 */
	class Game
	{
	public:
		/**
		 * @brief Initialize SDL, the renderer, load textures and spawn Level 1.
		 * Check @ref valid() before calling @ref run().
		 */
		Game();

		/// @brief Destroy SDL/Box2D resources.
		~Game();
		Game(const Game&) = delete;
		Game& operator=(const Game&) = delete;

		/// @brief Run the main loop until the user quits.
		void run();

		/// @brief @c true if initialization succeeded.
		bool valid() const { return _valid; }

		// --- Game tuning constants ---------------------------------------
		static constexpr int   WIN_W = 800;                // Window width in pixels.
		static constexpr int   WIN_H = 600;                // Window height in pixels.
		static constexpr int   FPS = 60;                   // Target frames per second.
		static constexpr int   GAME_FRAME_MS = 1000 / FPS; // ms per frame budget (~16ms at 60fps).

	private:
		// --- Helper methods used by the constructor and run loop ---------

		/// @brief Load every PNG/JPG used by the game.
		bool loadTextures();

		/// @brief Spawn the entities that make up a given level.
		/// @param level 1 = passive enemies, 2 = enemies that punch back.
		void spawnLevel(int level);

		/// @brief Destroy all gameplay entities (player + enemies + flashes).
		/// Called between level transitions and on game-over.
		void clearAllEntities();

		/// @brief Reset the game back to level 1 after the end screen.
		void restart();

		/// @brief Handle Play Again / Exit input while the end screen is visible.
		void handleEndScreenEvent(const SDL_Event& ev, bool& quit);

		/// @brief Draw the win/loss overlay and menu buttons.
		void renderEndScreen();

		// --- Member variables -------------------------------------------
		bool          _valid = false;                 // Initialization succeeded?
		SDL_Window*   _win = nullptr;                 // The OS window (owned).
		SDL_Renderer* _ren = nullptr;                 // The 2D renderer (owned).
		b2WorldId     _world = b2_nullWorldId;        // Box2D physics world handle.
	};
}
