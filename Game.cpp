#include "Game.h"
#include "me_and_dad_model.h"
#include "bagel.h"

#include <SDL3_image/SDL_image.h>
#include <iostream>
#include <cmath>
#include <cstdlib>

/**
 * @file Game.cpp
 * @brief Implementation of the Game class plus every ECS system update.
 *
 * Systems are defined here (rather than next to the factories) because they
 * need access to the SDL renderer / textures and the Box2D world. These are
 * kept as file-scope statics so each system can read them without touching
 * the Game class internals.
 */

namespace me_and_dad
{
	namespace
	{
		/* ----------------------------------------------------------- */
		/*  Resources shared between Game and the systems              */
		/* ----------------------------------------------------------- */
		SDL_Renderer* g_ren = nullptr;
		SDL_Texture*  g_texPlayer = nullptr;
		SDL_Texture*  g_texEnemies = nullptr;
		SDL_Texture*  g_texBackground = nullptr;
		SDL_Texture*  g_texTree = nullptr;

		int g_currentLevel = 1;
		bool g_gameOver = false;

		/// @brief Map a Renderable sprite name to the loaded SDL texture.
		SDL_Texture* texFor(const char* name) {
			if (!name) return nullptr;
			if (SDL_strcmp(name, "kid") == 0)            return g_texPlayer;
			if (SDL_strcmp(name, "normal_enemy") == 0)   return g_texEnemies;
			if (SDL_strcmp(name, "special_enemy") == 0)  return g_texEnemies;
			if (SDL_strcmp(name, "background") == 0)     return g_texBackground;
			if (SDL_strcmp(name, "tree") == 0)           return g_texTree;
			return nullptr;
		}

		/// @brief Iterate to find the player's entity id, if it exists.
		bagel::ent_type findPlayer() {
			static const bagel::Mask m = bagel::MaskBuilder().set<PlayerTag>().build();
			for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
				if (e.test(m)) return e.entity();
			}
			return bagel::ent_type{-1};
		}

		/// @brief Count how many enemies still exist.
		int countEnemies() {
			static const bagel::Mask m = bagel::MaskBuilder().set<EnemyTag>().build();
			int n = 0;
			for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
				if (e.test(m)) ++n;
			}
			return n;
		}
	}

	/* --------------------------------------------------------------- */
	/*  Game class                                                     */
	/* --------------------------------------------------------------- */

	Game::Game()
	{
		if (!SDL_Init(SDL_INIT_VIDEO)) {
			std::cout << "SDL_Init failed: " << SDL_GetError() << std::endl;
			return;
		}
		if (!SDL_CreateWindowAndRenderer(
				"Me and Dad", WIN_W, WIN_H, 0, &_win, &_ren)) {
			std::cout << "Create window failed: " << SDL_GetError() << std::endl;
			return;
		}
		g_ren = _ren;

		if (!loadTextures()) return;

		b2WorldDef def = b2DefaultWorldDef();
		def.gravity = {0, 0};
		_world = b2CreateWorld(&def);
		if (!b2World_IsValid(_world)) {
			std::cout << "Failed to create Box2D world" << std::endl;
			return;
		}

		SDL_srand(static_cast<Uint64>(SDL_GetTicks()));
		SDL_SetRenderDrawColor(_ren, 0, 0, 0, 255);

		spawnLevel(1);

		_valid = true;
	}

	Game::~Game()
	{
		if (b2World_IsValid(_world)) b2DestroyWorld(_world);
		if (g_texPlayer)     SDL_DestroyTexture(g_texPlayer);
		if (g_texEnemies)    SDL_DestroyTexture(g_texEnemies);
		if (g_texBackground) SDL_DestroyTexture(g_texBackground);
		if (g_texTree)       SDL_DestroyTexture(g_texTree);
		if (_ren)            SDL_DestroyRenderer(_ren);
		if (_win)            SDL_DestroyWindow(_win);
		SDL_Quit();
	}

	bool Game::loadTextures()
	{
		auto load = [this](const char* path) -> SDL_Texture* {
			SDL_Surface* surf = IMG_Load(path);
			if (!surf) {
				std::cout << "Failed to load " << path << ": "
				          << SDL_GetError() << std::endl;
				return nullptr;
			}
			SDL_Texture* tex = SDL_CreateTextureFromSurface(_ren, surf);
			SDL_DestroySurface(surf);
			return tex;
		};
		g_texPlayer     = load("res/player.png");
		g_texEnemies    = load("res/enemies.jpg");
		g_texBackground = load("res/background.jpg");
		g_texTree       = load("res/tree_object.jpg");
		return g_texPlayer && g_texEnemies && g_texBackground && g_texTree;
	}

	void Game::spawnLevel(int level)
	{
		g_currentLevel = level;
		createPlayer({100, WIN_H / 2.f});

		if (level == 1) {
			createNormalEnemy({500, 200});
			createNormalEnemy({600, 400});
			createNormalEnemy({700, 300});
		} else {
			createSpecialEnemy({500, 200});
			createSpecialEnemy({600, 400});
			createSpecialEnemy({700, 300});
			createSpecialEnemy({400, 450});
		}
	}

	void Game::clearAllEntities()
	{
		static const bagel::Mask m = bagel::MaskBuilder().set<Transform>().build();
		for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
			if (e.test(m)) e.destroy();
		}
	}

	void Game::run()
	{
		Uint64 start = SDL_GetTicks();
		bool quit = false;

		while (!quit) {
			InputSystem::update();
			AISystem::update();
			MovementSystem::update();
			PhysicsSystem::update();
			CombatSystem::update();
			LifetimeSystem::update();
			CollisionSystem::update();
			LevelSystem::update();

			SDL_RenderClear(_ren);
			RenderSystem::update();
			HudSystem::update();
			SDL_RenderPresent(_ren);

			const Uint64 end = SDL_GetTicks();
			if (end - start < GAME_FRAME_MS) {
				SDL_Delay(static_cast<Uint32>(GAME_FRAME_MS - (end - start)));
			}
			start += GAME_FRAME_MS;

			SDL_Event ev;
			while (SDL_PollEvent(&ev)) {
				if (ev.type == SDL_EVENT_QUIT ||
				    (ev.type == SDL_EVENT_KEY_DOWN &&
				     ev.key.scancode == SDL_SCANCODE_ESCAPE)) {
					quit = true;
				}
			}
		}
	}

	/* --------------------------------------------------------------- */
	/*  Systems                                                        */
	/* --------------------------------------------------------------- */

	void InputSystem::update()
	{
		static const bagel::Mask m = bagel::MaskBuilder()
			.set<Keys>().set<Intent>().build();

		SDL_PumpEvents();
		const bool* keys = SDL_GetKeyboardState(nullptr);

		for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
			if (e.test(m)) {
				const auto& k = e.get<Keys>();
				auto& i = e.get<Intent>();
				i.up    = keys[k.up];
				i.down  = keys[k.down];
				i.left  = keys[k.left];
				i.right = keys[k.right];
				i.punch = keys[k.punch];
			}
		}
	}

	void AISystem::update()
	{
		const auto pid = findPlayer();
		if (pid.id < 0) return;
		bagel::Entity player{pid};
		const auto& playerT = player.get<Transform>();

		static const bagel::Mask m = bagel::MaskBuilder()
			.set<EnemyTag>().set<AI>().set<Intent>().set<Transform>().build();

		for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
			if (!e.test(m)) continue;

			auto& i = e.get<Intent>();
			const auto& t = e.get<Transform>();
			const auto& a = e.get<AI>();

			i = {};
			if (a.kind != AiKind::ChasePlayer) continue;

			const float dx = playerT.position.x - t.position.x;
			const float dy = playerT.position.y - t.position.y;
			const float dist = std::sqrt(dx*dx + dy*dy);

			if (dist > 50) {
				if (dx < -2) i.left  = true;
				if (dx >  2) i.right = true;
				if (dy < -2) i.up    = true;
				if (dy >  2) i.down  = true;
			}

			// Level-2 enemies (CanPunchBack) attack when close.
			if (e.has<SpecialEnemyTag>() && dist < 60 && !e.has<Punching>()) {
				i.punch = true;
			}
		}
	}

	void MovementSystem::update()
	{
		static const bagel::Mask m = bagel::MaskBuilder()
			.set<Intent>().set<Velocity>().set<Transform>().build();

		constexpr float HALF_W = 32.f;
		constexpr float HALF_H = 48.f;

		for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
			if (!e.test(m)) continue;

			const auto& i = e.get<Intent>();
			auto& v = e.get<Velocity>();
			auto& t = e.get<Transform>();

			float vx = 0, vy = 0;
			if (i.left)  vx -= v.maxSpeed;
			if (i.right) vx += v.maxSpeed;
			if (i.up)    vy -= v.maxSpeed;
			if (i.down)  vy += v.maxSpeed;

			v.value = {vx, vy};
			t.position.x += vx;
			t.position.y += vy;

			if (t.position.x < HALF_W) t.position.x = HALF_W;
			if (t.position.x > Game::WIN_W - HALF_W) t.position.x = Game::WIN_W - HALF_W;
			if (t.position.y < HALF_H) t.position.y = HALF_H;
			if (t.position.y > Game::WIN_H - HALF_H) t.position.y = Game::WIN_H - HALF_H;

			if (e.has<Direction>()) {
				auto& d = e.get<Direction>();
				if (vx < 0)      d.facing = FacingDirection::Left;
				else if (vx > 0) d.facing = FacingDirection::Right;
			}
		}
	}

	void PhysicsSystem::update()
	{
		// Reserved for Phase 2: step Box2D world and copy transforms back.
	}

	void CombatSystem::update()
	{
		static const bagel::Mask attackerMask = bagel::MaskBuilder()
			.set<Intent>().set<Direction>().set<Transform>().build();
		static const bagel::Mask targetMask = bagel::MaskBuilder()
			.set<Health>().set<Transform>().build();

		constexpr float PUNCH_REACH = 50.f;
		constexpr float PUNCH_HALF  = 30.f;

		for (bagel::Entity attacker = bagel::Entity::first(); !attacker.eof(); attacker.next()) {
			if (!attacker.test(attackerMask)) continue;
			const auto& i = attacker.get<Intent>();
			if (!i.punch || attacker.has<Punching>()) continue;

			attacker.add(Punching{8});

			const auto& at = attacker.get<Transform>();
			const auto& ad = attacker.get<Direction>();
			float hbX = at.position.x;
			if (ad.facing == FacingDirection::Left)  hbX -= PUNCH_REACH;
			if (ad.facing == FacingDirection::Right) hbX += PUNCH_REACH;
			const float hbY = at.position.y;

			createFlashEntity({hbX, hbY});

			const bool attackerIsPlayer = attacker.has<PlayerTag>();

			for (bagel::Entity target = bagel::Entity::first(); !target.eof(); target.next()) {
				if (target.entity().id == attacker.entity().id) continue;
				if (!target.test(targetMask)) continue;
				if (target.has<IFrames>()) continue;

				// Player damages enemies; enemies damage the player.
				if (attackerIsPlayer && !target.has<EnemyTag>()) continue;
				if (!attackerIsPlayer && !target.has<PlayerTag>()) continue;

				const auto& tt = target.get<Transform>();
				if (std::abs(tt.position.x - hbX) < PUNCH_HALF + 24 &&
				    std::abs(tt.position.y - hbY) < PUNCH_HALF + 32) {
					target.get<Health>().current -= 1;
					target.add(IFrames{30});
				}
			}
		}
	}

	void LifetimeSystem::update()
	{
		// FlashEffect: destroy entity when timer reaches zero.
		{
			static const bagel::Mask m = bagel::MaskBuilder().set<FlashEffect>().build();
			for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
				if (e.test(m)) {
					auto& f = e.get<FlashEffect>();
					if (--f.framesLeft <= 0) e.destroy();
				}
			}
		}
		// Punching: remove component when timer reaches zero.
		{
			static const bagel::Mask m = bagel::MaskBuilder().set<Punching>().build();
			for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
				if (e.test(m)) {
					auto& p = e.get<Punching>();
					if (--p.framesLeft <= 0) e.del<Punching>();
				}
			}
		}
		// IFrames: remove component when timer reaches zero.
		{
			static const bagel::Mask m = bagel::MaskBuilder().set<IFrames>().build();
			for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
				if (e.test(m)) {
					auto& fr = e.get<IFrames>();
					if (--fr.framesLeft <= 0) e.del<IFrames>();
				}
			}
		}
	}

	void CollisionSystem::update()
	{
		// Death: any entity whose Health reached zero is destroyed.
		static const bagel::Mask m = bagel::MaskBuilder().set<Health>().build();
		for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
			if (e.test(m) && e.get<Health>().current <= 0) {
				e.destroy();
			}
		}
	}

	void LevelSystem::update()
	{
		if (g_gameOver) return;

		// Player dead -> game over.
		if (findPlayer().id < 0) {
			g_gameOver = true;
			std::cout << "Game Over!" << std::endl;
			return;
		}

		// All enemies cleared -> advance.
		if (countEnemies() == 0) {
			if (g_currentLevel == 1) {
				std::cout << "Level 1 cleared - advancing to Level 2" << std::endl;
				// Spawn a new wave of stronger enemies. Don't despawn the player.
				createSpecialEnemy({500, 200});
				createSpecialEnemy({600, 400});
				createSpecialEnemy({700, 300});
				createSpecialEnemy({400, 450});
				g_currentLevel = 2;
			} else {
				std::cout << "You win!" << std::endl;
				g_gameOver = true;
			}
		}
	}

	void RenderSystem::update()
	{
		if (g_texBackground) {
			SDL_FRect bg = {0, 0, (float)Game::WIN_W, (float)Game::WIN_H};
			SDL_RenderTexture(g_ren, g_texBackground, nullptr, &bg);
		}

		static const bagel::Mask m = bagel::MaskBuilder()
			.set<Transform>().set<Renderable>().build();

		for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
			if (!e.test(m)) continue;
			const auto& t = e.get<Transform>();
			const auto& r = e.get<Renderable>();
			if (!r.visible) continue;

			// Flash entity: draw a yellow square as the punch impact.
			if (e.has<FlashEffect>()) {
				SDL_FRect dest = {t.position.x - 18, t.position.y - 18, 36, 36};
				SDL_SetRenderDrawColor(g_ren, 255, 230, 0, 255);
				SDL_RenderFillRect(g_ren, &dest);
				SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
				continue;
			}

			SDL_Texture* tex = texFor(r.spriteName);
			SDL_FRect dest;
			if (e.has<Collider>()) {
				const auto& c = e.get<Collider>();
				dest = {
					t.position.x - c.size.x/2,
					t.position.y - c.size.y/2,
					c.size.x, c.size.y
				};
			} else {
				dest = {t.position.x - 16, t.position.y - 16, 32, 32};
			}

			SDL_FlipMode flip = SDL_FLIP_NONE;
			if (e.has<Direction>() &&
			    e.get<Direction>().facing == FacingDirection::Left) {
				flip = SDL_FLIP_HORIZONTAL;
			}

			if (tex) {
				// Flicker when invulnerable.
				const bool dim = e.has<IFrames>() &&
				                 (e.get<IFrames>().framesLeft / 4) % 2 == 0;
				if (dim) SDL_SetTextureAlphaMod(tex, 110);
				SDL_RenderTextureRotated(g_ren, tex, nullptr, &dest, 0, nullptr, flip);
				if (dim) SDL_SetTextureAlphaMod(tex, 255);
			} else {
				// Fallback: colored rectangle if a texture is missing.
				if (e.has<PlayerTag>())     SDL_SetRenderDrawColor(g_ren, 100, 100, 255, 255);
				else if (e.has<EnemyTag>()) SDL_SetRenderDrawColor(g_ren, 255, 100, 100, 255);
				else                         SDL_SetRenderDrawColor(g_ren, 200, 200, 200, 255);
				SDL_RenderFillRect(g_ren, &dest);
				SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
			}
		}
	}

	void HudSystem::update()
	{
		const auto pid = findPlayer();
		if (pid.id < 0) return;
		bagel::Entity player{pid};
		if (!player.has<Health>()) return;

		const auto& h = player.get<Health>();
		SDL_SetRenderDrawColor(g_ren, 220, 30, 30, 255);
		for (int i = 0; i < h.current; ++i) {
			SDL_FRect r = {10 + i * 24.f, 10, 20, 20};
			SDL_RenderFillRect(g_ren, &r);
		}
		SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
	}
}
