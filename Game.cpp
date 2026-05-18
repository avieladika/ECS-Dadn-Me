#include "Game.h"
#include "me_and_dad_model.h"
#include "bagel.h"

#include <SDL3_image/SDL_image.h>
#include <iostream>
#include <cmath>
#include <cstdlib>

/**
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
                // These are file-scope (anonymous namespace) so they are private
                // to Game.cpp. Systems update() functions need them but should not
                // be coupled to the Game class, so we expose them as globals here.
                // "g_" prefix marks them as globals; all initialized to nullptr/
                // safe defaults so a partially-built game shuts down cleanly.
                SDL_Renderer* g_ren = nullptr;            // Borrowed from Game (not owned here).
                SDL_Texture*  g_texPlayer = nullptr;      // "kid" sprite.
                SDL_Texture*  g_texEnemies = nullptr;     // Normal + special enemy sprite sheet.
                SDL_Texture*  g_texBackground = nullptr;  // Full-screen background image.

                // Gameplay state shared by Game and the LevelSystem. Reset by restart().
                int g_currentLevel = 1;   // 1 = passive enemies, 2 = punch-back enemies.
                bool g_gameOver = false;  // True once the player wins or loses.

                // Three top-level screens the game can be in. We use enum class
                // (not plain enum) so the values are scoped and don't leak into
                // the surrounding namespace.
                enum class ScreenMode {
                    Playing,           // Normal gameplay - all systems update.
                    LevelTransition,   // Flashing "LEVEL 2" overlay between levels.
                    EndScreen          // Win/loss screen with Play Again / Exit buttons.
                };

                // Why the end screen is showing - drives the title text ("YOU WIN!" vs "GAME OVER").
                enum class EndReason {
                    Won,
                    Lost
                };

                // Current state of the screen-mode state machine.
                ScreenMode g_screenMode = ScreenMode::Playing;
                EndReason g_endReason = EndReason::Lost;
                // Which level we are about to enter (during LevelTransition).
                int g_pendingLevel = 0;
                // Wall-clock (SDL_GetTicks) timestamp when transition began.
                Uint64 g_levelTransitionStartedAt = 0;
                // Small text buffer + timestamp for the "<name> - died" HUD line.
                // Fixed-size char array (no std::string) keeps this struct POD-like.
                char g_deathNotice[64] = "";
                Uint64 g_deathNoticeStartedAt = 0;

                // End-screen button hitboxes. Defined here so both renderEndScreen
                // (which draws them) and handleEndScreenEvent (which tests clicks
                // against them) see the exact same rectangle.
                constexpr SDL_FRect PLAY_AGAIN_BUTTON = {270.f, 335.f, 260.f, 54.f};
                constexpr SDL_FRect EXIT_BUTTON = {270.f, 405.f, 260.f, 54.f};
                // How long the "LEVEL 2" overlay flashes, and how long a death
                // notice stays on screen. Both in milliseconds.
                constexpr Uint64 LEVEL_TRANSITION_MS = 2000;
                constexpr Uint64 DEATH_NOTICE_MS = 2500;
                // One name per enemy, used so the death notice can show
                // "Daniel - died" instead of just "enemy died".
                constexpr const char* ENEMY_NAMES[7] = {
                    "Adam", "Ben", "Daniel", "Ethan", "Gabriel", "Henry", "Isaac"
                };

                /// @brief Map a Renderable sprite name to the loaded SDL texture.
                // Components store the sprite as a const char* key; this function
                // resolves that key into the actual SDL_Texture* loaded at startup.
                // Using strings instead of texture pointers in the component keeps
                // the model layer free of SDL dependencies.
                SDL_Texture* texFor(const char* name) {
                    if (!name) return nullptr;
                    if (SDL_strcmp(name, "kid") == 0)            return g_texPlayer;
                    if (SDL_strcmp(name, "normal_enemy") == 0)   return g_texEnemies;
                    if (SDL_strcmp(name, "special_enemy") == 0)  return g_texEnemies;
                    if (SDL_strcmp(name, "background") == 0)     return g_texBackground;
                    return nullptr;
                }

                /// @brief Iterate to find the player's entity id, if it exists.
                bagel::ent_type findPlayer() {
                    // Step 1: build (once) a Mask that means "has PlayerTag".
                    // static + const means we pay the cost only on the first call.
                    static const bagel::Mask m = bagel::MaskBuilder().set<PlayerTag>().build();
                    // Step 2: walk every alive entity. test(m) is a fast bitwise check.
                    for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
                        if (e.test(m)) return e.entity();
                    }
                    // Step 3: no player exists (dead). Return -1 so callers can check.
                    return bagel::ent_type{-1};
                }

                /// @brief Count how many enemies still exist.
                int countEnemies() {
                    // Same pattern as findPlayer, but counts instead of returning the first match.
                    // LevelSystem uses this to detect "all enemies dead -> advance level".
                    static const bagel::Mask m = bagel::MaskBuilder().set<EnemyTag>().build();
                    int n = 0;
                    for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
                        if (e.test(m)) ++n;
                    }
                    return n;
                }

                // Classic AABB point-in-rectangle test. Used to detect mouse clicks
                // on the end-screen buttons.
                bool pointInRect(float x, float y, const SDL_FRect& rect) {
                    return x >= rect.x && x <= rect.x + rect.w &&
                           y >= rect.y && y <= rect.y + rect.h;
                }

                // Draw text horizontally centered on `centerX`, at vertical `y`.
                // SDL's debug font has a known character width; we multiply by
                // string length to compute total width and shift left by half.
                // SetRenderScale lets us draw the debug font at any size (the font
                // itself is fixed-pixel, so we scale the renderer instead).
                void drawCenteredDebugText(const char* text, float centerX, float y, float scale) {
                    const float charW = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
                    const float x = centerX - (SDL_strlen(text) * charW) / 2.f;
                    SDL_SetRenderScale(g_ren, scale, scale);
                    // Coordinates are pre-scale, so we divide by scale before passing in.
                    SDL_RenderDebugText(g_ren, x / scale, y / scale, text);
                    // Restore the default 1:1 scale so later draw calls aren't affected.
                    SDL_SetRenderScale(g_ren, 1.f, 1.f);
                }

                // Same as above but without centering - text is drawn left-aligned at (x, y).
                void drawDebugText(const char* text, float x, float y, float scale) {
                    SDL_SetRenderScale(g_ren, scale, scale);
                    SDL_RenderDebugText(g_ren, x / scale, y / scale, text);
                    SDL_SetRenderScale(g_ren, 1.f, 1.f);
                }

                // Draw one end-screen button: filled light background, dark border,
                // centered label text. Uses the shared `drawCenteredDebugText` helper.
                void drawButton(const SDL_FRect& rect, const char* label) {
                    SDL_SetRenderDrawColor(g_ren, 245, 245, 245, 255); // Off-white fill.
                    SDL_RenderFillRect(g_ren, &rect);
                    SDL_SetRenderDrawColor(g_ren, 20, 25, 30, 255);    // Near-black border.
                    SDL_RenderRect(g_ren, &rect);
                    drawCenteredDebugText(label, rect.x + rect.w / 2.f, rect.y + 18.f, 2.f);
                }

                // Switch the screen-mode FSM into LevelTransition and remember
                // which level we are transitioning to + when it started.
                void startLevelTransition(int level) {
                    g_pendingLevel = level;
                    g_levelTransitionStartedAt = SDL_GetTicks();
                    g_screenMode = ScreenMode::LevelTransition;
                }

                void renderLevelTransition() {
                    // Step 1: compute elapsed time since transition began, then
                    // use the elapsed/250 trick to make the text blink every 250ms
                    // (visible / hidden / visible / hidden ...).
                    const Uint64 elapsed = SDL_GetTicks() - g_levelTransitionStartedAt;
                    const bool visible = (elapsed / 250) % 2 == 0;
                    if (!visible) return;

                    // Step 2: build "LEVEL N" into a stack-allocated buffer.
                    char text[32];
                    SDL_snprintf(text, sizeof(text), "LEVEL %d", g_pendingLevel);

                    // Step 3: dim the game behind the text with a 50% black overlay.
                    // BLENDMODE_BLEND enables alpha compositing for this rect.
                    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 130);
                    SDL_FRect overlay = {0, 0, static_cast<float>(Game::WIN_W), static_cast<float>(Game::WIN_H)};
                    SDL_RenderFillRect(g_ren, &overlay);
                    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_NONE);

                    // Step 4: draw the big white "LEVEL N" text centered, then
                    // restore the default black draw color.
                    SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
                    drawCenteredDebugText(text, Game::WIN_W / 2.f, 255.f, 6.f);
                    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
                }

                // Format "<name> - died" into the HUD buffer and record the timestamp.
                // HudSystem reads these and renders the line for DEATH_NOTICE_MS ms.
                void showEnemyDeathNotice(const char* name) {
                    if (!name) return;
                    SDL_snprintf(g_deathNotice, sizeof(g_deathNotice), "%s - died", name);
                    g_deathNoticeStartedAt = SDL_GetTicks();
                }

                // Spawn the three Level 1 enemies, each with a hand-picked
                // patrol path. The first waypoint becomes the spawn position.
                // Names come from ENEMY_NAMES for the death-notice HUD.
                void spawnLevel1Enemies() {
                    // Enemy 1: patrols the top half of the screen in a rectangle.
                    createNormalEnemy({
                            Vec2{50, 80}, Vec2{750, 80}, Vec2{750, 220}, Vec2{50, 220}
                    }, ENEMY_NAMES[0]);
                    // Enemy 2: zig-zags down the right side of the map.
                    createNormalEnemy({
                            Vec2{700, 80}, Vec2{600, 250}, Vec2{700, 400}, Vec2{600, 520}
                    }, ENEMY_NAMES[1]);
                    // Enemy 3: 5-point patrol across the bottom of the screen.
                    createNormalEnemy({
                            Vec2{50, 420}, Vec2{200, 520}, Vec2{400, 400},
                            Vec2{600, 520}, Vec2{750, 420}
                    }, ENEMY_NAMES[2]);
                }

                // Same idea, but Level 2 uses createSpecialEnemy (faster + can punch back).
                void spawnLevel2Enemies() {
                    createSpecialEnemy({
                            Vec2{400, 120}, Vec2{680, 300}, Vec2{400, 480}, Vec2{120, 300}
                    }, ENEMY_NAMES[3]);
                    createSpecialEnemy({
                            Vec2{100, 80}, Vec2{100, 520}, Vec2{320, 520}, Vec2{320, 80}
                    }, ENEMY_NAMES[4]);
                    createSpecialEnemy({
                            Vec2{50, 100}, Vec2{400, 250}, Vec2{750, 100},
                            Vec2{750, 450}, Vec2{400, 520}, Vec2{50, 450}
                    }, ENEMY_NAMES[5]);
                    createSpecialEnemy({
                            Vec2{550, 120}, Vec2{750, 300}, Vec2{550, 500}
                    }, ENEMY_NAMES[6]);
                }
            }

            /* --------------------------------------------------------------- */
            /*  Game class                                                     */
            /* --------------------------------------------------------------- */

            Game::Game()
            {
                // Step 1: initialize the SDL video subsystem. If this fails we
                // print the error and return early - _valid stays false so main()
                // will skip the run loop.
                if (!SDL_Init(SDL_INIT_VIDEO)) {
                    std::cout << "SDL_Init failed: " << SDL_GetError() << std::endl;
                    return;
                }

                // Step 2: ask SDL to create a window and a renderer in one call.
                // Both pointers are written back into our member variables.
                if (!SDL_CreateWindowAndRenderer(
                        "Me and Dad", WIN_W, WIN_H, 0, &_win, &_ren)) {
                    std::cout << "Create window failed: " << SDL_GetError() << std::endl;
                    return;
                }
                // Cache the renderer in the file-scope global so systems can use it
                // without going through the Game instance.
                g_ren = _ren;

                // Step 3: load every PNG/JPG into SDL_Texture objects. Bails out
                // if any required sprite is missing.
                if (!loadTextures()) return;

                // Step 4: build the Box2D world. Gravity is {0, 0} because this is
                // a top-down game (no falling). b2DefaultWorldDef gives sensible
                // defaults for everything else (iterations, allow-sleep, etc.).
                b2WorldDef def = b2DefaultWorldDef();
                def.gravity = {0, 0};
                _world = b2CreateWorld(&def);
                if (!b2World_IsValid(_world)) {
                    std::cout << "Failed to create Box2D world" << std::endl;
                    return;
                }

                // Step 5: seed SDL's RNG with the current tick count so any
                // randomness (currently unused) is fresh per launch.
                SDL_srand(static_cast<Uint64>(SDL_GetTicks()));
                // Default draw color is black; SDL clears to this between frames.
                SDL_SetRenderDrawColor(_ren, 0, 0, 0, 255);

                // Step 6: populate the world with Level 1 entities.
                spawnLevel(1);

                // Step 7: declare success. Only after this point is the Game ready to run().
                _valid = true;
            }

            Game::~Game()
            {
                // Tear everything down in reverse order of creation. Each guard
                // checks validity first so a partially-built Game (constructor
                // bailed out early) still destructs safely without crashing.
                if (b2World_IsValid(_world)) b2DestroyWorld(_world);
                if (g_texPlayer)     SDL_DestroyTexture(g_texPlayer);
                if (g_texEnemies)    SDL_DestroyTexture(g_texEnemies);
                if (g_texBackground) SDL_DestroyTexture(g_texBackground);
                if (_ren)            SDL_DestroyRenderer(_ren);
                if (_win)            SDL_DestroyWindow(_win);
                // Shuts down all SDL subsystems initialized with SDL_Init.
                SDL_Quit();
            }

            bool Game::loadTextures()
            {
                // Step 1: define a small lambda that loads one image file.
                // It returns a SDL_Texture* or nullptr on failure. We capture
                // [this] so the lambda can use `_ren` (our renderer).
                auto load = [this](const char* path) -> SDL_Texture* {
                    // 1a: IMG_Load decodes the file (PNG/JPG) into a CPU-side SDL_Surface.
                    SDL_Surface* surf = IMG_Load(path);
                    if (!surf) {
                        std::cout << "Failed to load " << path << ": "
                                  << SDL_GetError() << std::endl;
                        return nullptr;
                    }
                    // 1b: Upload the surface pixels to GPU memory as a texture.
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(_ren, surf);
                    // 1c: We no longer need the CPU-side surface; free it now.
                    // The texture keeps its own GPU copy.
                    SDL_DestroySurface(surf);
                    return tex;
                };

                // Step 2: load every sprite. Paths are relative to the working directory.
                g_texPlayer     = load("res/player.png");
                g_texEnemies    = load("res/enemies.jpg");
                g_texBackground = load("res/background.jpg");

                // Step 3: succeed only if every texture loaded. Short-circuit && stops
                // at the first nullptr.
                return g_texPlayer && g_texEnemies && g_texBackground;
            }

            void Game::spawnLevel(int level)
            {
                // Step 1: remember which level we are in - LevelSystem reads this.
                g_currentLevel = level;
                // Step 2: spawn the player on the left side of the screen, vertically centered.
                createPlayer({100, WIN_H / 2.f});

                // Step 3: spawn the level-specific enemies. Level 1 = passive patrollers,
                // Level 2 = special enemies that can punch back.
                if (level == 1) {
                    spawnLevel1Enemies();
                } else {
                    spawnLevel2Enemies();
                }
            }

            void Game::clearAllEntities()
            {
                // Build a mask that matches any entity that has a Transform.
                // Every gameplay entity (player, enemy, flash, static object) has one,
                // so this effectively means "destroy every gameplay entity".
                static const bagel::Mask m = bagel::MaskBuilder().set<Transform>().build();
                for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
                    if (e.test(m)) e.destroy();
                }
            }

            void Game::restart()
            {
                // Step 1: nuke the world - remove player, enemies, flashes, etc.
                clearAllEntities();
                // Step 2: reset every piece of game state to its level-1 defaults.
                g_currentLevel = 1;
                g_gameOver = false;
                g_endReason = EndReason::Lost;
                g_pendingLevel = 0;
                g_levelTransitionStartedAt = 0;
                g_deathNotice[0] = '\0';        // Empty string = HUD line hidden.
                g_deathNoticeStartedAt = 0;
                g_screenMode = ScreenMode::Playing;
                // Step 3: re-spawn Level 1.
                spawnLevel(1);
            }

            void Game::handleEndScreenEvent(const SDL_Event& ev, bool& quit)
            {
                // Branch 1: keyboard shortcuts. R = play again, Q or ESC = quit.
                // Returning here means a keypress consumes the event without also
                // being treated as a mouse click.
                if (ev.type == SDL_EVENT_KEY_DOWN) {
                    if (ev.key.scancode == SDL_SCANCODE_R) {
                        restart();
                    } else if (ev.key.scancode == SDL_SCANCODE_Q ||
                               ev.key.scancode == SDL_SCANCODE_ESCAPE) {
                        quit = true;
                    }
                    return;
                }

                // Branch 2: left mouse button. Hit-test the click position against
                // each button rectangle; the first match wins.
                if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT) {
                    if (pointInRect(ev.button.x, ev.button.y, PLAY_AGAIN_BUTTON)) {
                        restart();
                    } else if (pointInRect(ev.button.x, ev.button.y, EXIT_BUTTON)) {
                        quit = true;
                    }
                }
            }

            void Game::renderEndScreen()
            {
                // Step 1: dim the game underneath with a 73% black overlay
                // (alpha = 185 of 255). BLENDMODE_BLEND turns on alpha compositing.
                SDL_SetRenderDrawBlendMode(_ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(_ren, 0, 0, 0, 185);
                SDL_FRect overlay = {0, 0, static_cast<float>(WIN_W), static_cast<float>(WIN_H)};
                SDL_RenderFillRect(_ren, &overlay);
                SDL_SetRenderDrawBlendMode(_ren, SDL_BLENDMODE_NONE);

                // Step 2: pick the title based on win/loss and draw it big & white,
                // centered roughly one third down the screen.
                SDL_SetRenderDrawColor(_ren, 255, 255, 255, 255);
                drawCenteredDebugText(
                        g_endReason == EndReason::Won ? "YOU WIN!" : "GAME OVER",
                        WIN_W / 2.f,
                        185.f,
                        5.f);
                // Step 3: subtitle hints at keyboard shortcuts, even though the
                // buttons below are clickable too.
                drawCenteredDebugText("R: PLAY AGAIN    Q/ESC: EXIT", WIN_W / 2.f, 270.f, 2.f);

                // Step 4: draw the two interactive buttons. Their rects match the
                // hit-test rects in handleEndScreenEvent (both reference the constexpr).
                drawButton(PLAY_AGAIN_BUTTON, "PLAY AGAIN");
                drawButton(EXIT_BUTTON, "EXIT");

                // Step 5: restore the default black draw color for the next frame.
                SDL_SetRenderDrawColor(_ren, 0, 0, 0, 255);
            }

            void Game::run()
            {
                // Frame timer reference + main-loop quit flag.
                Uint64 start = SDL_GetTicks();
                bool quit = false;

                // The main game loop. Each iteration is one frame, capped to ~16ms (60 FPS).
                while (!quit) {
                    /* ---------- Phase 1: process OS events ---------- */
                    // SDL_PollEvent drains the OS event queue, one event at a time.
                    SDL_Event ev;
                    while (SDL_PollEvent(&ev)) {
                        if (ev.type == SDL_EVENT_QUIT) {
                            // The user clicked the window's close button.
                            quit = true;
                        } else if (g_screenMode == ScreenMode::EndScreen) {
                            // On the win/loss screen, route input to the end-screen handler.
                            handleEndScreenEvent(ev, quit);
                        } else if (ev.type == SDL_EVENT_KEY_DOWN &&
                                   ev.key.scancode == SDL_SCANCODE_ESCAPE) {
                            // ESC during gameplay quits immediately.
                            quit = true;
                        }
                    }

                    /* ---------- Phase 2: run ECS systems (gameplay only) ----- */
                    // The order matters: input -> AI -> movement -> physics ->
                    // combat -> lifetimes -> deaths -> level checks.
                    if (g_screenMode == ScreenMode::Playing) {
                        InputSystem::update();      // Keyboard -> player Intent.
                        AISystem::update();         // Patrol logic -> enemy Intent.
                        MovementSystem::update();   // Intent -> Velocity -> Transform.
                        PhysicsSystem::update();    // Reserved (Box2D step).
                        CombatSystem::update();     // Punch + damage.
                        LifetimeSystem::update();   // Tick down timers (Punching, IFrames, FlashEffect).
                        CollisionSystem::update();  // Destroy entities with 0 HP.
                        LevelSystem::update();      // Check for win/loss + advance level.
                    }

                    /* ---------- Phase 3: advance the level-transition timer -- */
                    // When 2 seconds have passed since transition started, spawn
                    // the next level's enemies (player still exists from before)
                    // and switch back to Playing.
                    if (g_screenMode == ScreenMode::LevelTransition &&
                        SDL_GetTicks() - g_levelTransitionStartedAt >= LEVEL_TRANSITION_MS) {
                        if (g_pendingLevel == 2) {
                            spawnLevel2Enemies();
                            g_currentLevel = 2;
                        }
                        g_pendingLevel = 0;
                        g_levelTransitionStartedAt = 0;
                        g_screenMode = ScreenMode::Playing;
                    }

                    /* ---------- Phase 4: render the frame -------------------- */
                    SDL_RenderClear(_ren);            // Wipe the framebuffer to black.
                    RenderSystem::update();           // Draw background + every entity.
                    HudSystem::update();              // Draw player HP + death notice.
                    if (g_screenMode == ScreenMode::LevelTransition) {
                        renderLevelTransition();      // Flashing "LEVEL 2" overlay.
                    }
                    if (g_screenMode == ScreenMode::EndScreen) {
                        renderEndScreen();            // Win/loss UI.
                    }
                    SDL_RenderPresent(_ren);          // Swap back buffer to screen.

                    /* ---------- Phase 5: cap framerate to 60 FPS ------------- */
                    // If we finished faster than the frame budget, sleep the rest.
                    // start += GAME_FRAME_MS (instead of resetting to now) keeps
                    // long-term timing accurate even if one frame ran a bit late.
                    const Uint64 end = SDL_GetTicks();
                    if (end - start < GAME_FRAME_MS) {
                        SDL_Delay(static_cast<Uint32>(GAME_FRAME_MS - (end - start)));
                    }
                    start += GAME_FRAME_MS;
                }
            }

            /* --------------------------------------------------------------- */
            /*  Systems                                                        */
            /* --------------------------------------------------------------- */

            void InputSystem::update()
            {
                // Step 1: build the "wants keyboard input" filter mask. Cached
                // in a static so the MaskBuilder runs only on the first call.
                static const bagel::Mask m = bagel::MaskBuilder()
                        .set<Keys>().set<Intent>().build();

                // Step 2: refresh SDL's internal keyboard state and grab a
                // pointer to the per-scancode pressed/released table.
                SDL_PumpEvents();
                const bool* keys = SDL_GetKeyboardState(nullptr);

                // Step 3: for every entity that has Keys + Intent (the player),
                // copy the live key states into its Intent flags. MovementSystem
                // and CombatSystem read these flags this same frame.
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
                // Step 1: look up the player once per frame. If the player is dead
                // (entity destroyed), hasPlayer = false and special enemies skip the
                // punch-back check below.
                const auto pid = findPlayer();
                const bool hasPlayer = pid.id >= 0;
                Vec2 playerPos = {};
                if (hasPlayer) {
                    bagel::Entity player{pid};
                    playerPos = player.get<Transform>().position;
                }

                // Step 2: mask matches "enemy that wants to patrol" - everything we need.
                static const bagel::Mask m = bagel::MaskBuilder()
                        .set<EnemyTag>().set<AI>().set<Intent>().set<Transform>().set<Path>().build();

                for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
                    if (!e.test(m)) continue;

                    auto& i = e.get<Intent>();
                    const auto& t = e.get<Transform>();
                    const auto& a = e.get<AI>();

                    // Step 3: clear last frame's intent. We rebuild it from scratch
                    // each frame based on path/player state.
                    i = {};
                    if (a.kind != AiKind::Patrol) continue;

                    auto& path = e.get<Path>();
                    if (path.pointCount <= 0) continue;

                    // Step 4: compute vector from us to the next waypoint.
                    const Vec2 target = path.points[path.currentPoint];
                    const float dx = target.x - t.position.x;
                    const float dy = target.y - t.position.y;

                    // Step 5: if we are within ~5 pixels of the waypoint, advance
                    // to the next one. Looping paths wrap back to index 0;
                    // non-looping paths clamp to the last point.
                    if (std::abs(dx) < 5 && std::abs(dy) < 5) {
                        ++path.currentPoint;
                        if (path.currentPoint >= path.pointCount) {
                            path.currentPoint = path.loop ? 0 : path.pointCount - 1;
                        }
                    } else {
                        // Step 6: still walking toward the waypoint. Set Intent
                        // flags based on which side of us the waypoint is. The
                        // ±2 deadzone prevents jittering when nearly aligned.
                        if (dx < -2) i.left  = true;
                        if (dx >  2) i.right = true;
                        if (dy < -2) i.up    = true;
                        if (dy >  2) i.down  = true;
                    }

                    // Step 7: special enemies (Level 2) punch back when the player
                    // is within 60 px AND they are not already mid-punch.
                    // Euclidean distance via Pythagoras.
                    const float playerDx = playerPos.x - t.position.x;
                    const float playerDy = playerPos.y - t.position.y;
                    const float playerDist = std::sqrt(playerDx * playerDx + playerDy * playerDy);
                    if (hasPlayer && e.has<SpecialEnemyTag>() && playerDist < 60 && !e.has<Punching>()) {
                        i.punch = true;
                    }
                }
            }

            void MovementSystem::update()
            {
                // Step 1: filter mask + half-size constants used for screen clamping.
                // HALF_W / HALF_H are the player/enemy sprite half-sizes so the
                // entity's center stays inside the visible area.
                static const bagel::Mask m = bagel::MaskBuilder()
                        .set<Intent>().set<Velocity>().set<Transform>().build();

                constexpr float HALF_W = 32.f;
                constexpr float HALF_H = 48.f;

                for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
                    if (!e.test(m)) continue;

                    const auto& i = e.get<Intent>();
                    auto& v = e.get<Velocity>();
                    auto& t = e.get<Transform>();

                    // Step 2: convert four boolean Intent flags into a direction vector.
                    // Up = -Y, down = +Y (SDL screen-space). Holding two keys gives
                    // a diagonal like (1, 1); we normalize later.
                    float dirX = 0, dirY = 0;
                    if (i.left)  dirX -= 1.f;
                    if (i.right) dirX += 1.f;
                    if (i.up)    dirY -= 1.f;
                    if (i.down)  dirY += 1.f;

                    // Step 3 (enemies only): boid-style separation. Each nearby enemy
                    // pushes us slightly in the opposite direction so they don't pile up.
                    if (e.has<EnemyTag>()) {
                        constexpr float SEPARATION_RADIUS = 58.f;   // Personal-space bubble.
                        constexpr float SEPARATION_FORCE = 1.25f;   // How strongly to push away.

                        static const bagel::Mask enemyMask = bagel::MaskBuilder()
                                .set<EnemyTag>().set<Transform>().build();

                        for (bagel::Entity other = bagel::Entity::first(); !other.eof(); other.next()) {
                            // Skip self and non-enemies. Using id comparison because
                            // bagel::Entity wraps an integer handle.
                            if (other.entity().id == e.entity().id || !other.test(enemyMask)) continue;

                            // Vector from `other` to `us` and squared distance.
                            // distSq is compared without sqrt for performance.
                            const auto& ot = other.get<Transform>();
                            const float awayX = t.position.x - ot.position.x;
                            const float awayY = t.position.y - ot.position.y;
                            const float distSq = awayX * awayX + awayY * awayY;
                            if (distSq <= 0.001f || distSq >= SEPARATION_RADIUS * SEPARATION_RADIUS) continue;

                            // Closer = stronger push (strength ranges from 0 to 1).
                            // Divide by `dist` to get a unit-length direction.
                            const float dist = std::sqrt(distSq);
                            const float strength = (SEPARATION_RADIUS - dist) / SEPARATION_RADIUS;
                            dirX += (awayX / dist) * strength * SEPARATION_FORCE;
                            dirY += (awayY / dist) * strength * SEPARATION_FORCE;
                        }
                    }

                    // Step 4: normalize the combined direction vector so diagonals
                    // are not faster than orthogonals. lenSq guards against div-by-zero.
                    const float lenSq = dirX * dirX + dirY * dirY;
                    if (lenSq > 0.001f) {
                        const float invLen = 1.f / std::sqrt(lenSq);
                        dirX *= invLen;
                        dirY *= invLen;
                    }

                    // Step 5: smooth acceleration toward the target velocity.
                    // Each frame the current velocity moves 22% of the way to the
                    // target - this gives a feel of inertia instead of snapping.
                    const float targetVx = dirX * v.maxSpeed;
                    const float targetVy = dirY * v.maxSpeed;
                    constexpr float ACCEL = 0.22f;

                    v.value.x += (targetVx - v.value.x) * ACCEL;
                    v.value.y += (targetVy - v.value.y) * ACCEL;

                    // Step 6: snap tiny velocities to zero so the entity actually
                    // stops instead of drifting forever from float noise.
                    if (std::abs(v.value.x) < 0.05f) v.value.x = 0;
                    if (std::abs(v.value.y) < 0.05f) v.value.y = 0;

                    // Step 7: integrate position from velocity. Since this is a
                    // 60 FPS game we just add velocity directly (units are px/frame).
                    t.position.x += v.value.x;
                    t.position.y += v.value.y;

                    // Step 8: clamp position to the screen so entities can't walk off.
                    if (t.position.x < HALF_W) t.position.x = HALF_W;
                    if (t.position.x > Game::WIN_W - HALF_W) t.position.x = Game::WIN_W - HALF_W;
                    if (t.position.y < HALF_H) t.position.y = HALF_H;
                    if (t.position.y > Game::WIN_H - HALF_H) t.position.y = Game::WIN_H - HALF_H;

                    // Step 9: update facing for sprite flip + punch hitbox direction.
                    // Horizontal motion wins over vertical so the sprite-flip looks
                    // natural when both axes are pressed.
                    if (e.has<Direction>()) {
                        auto& d = e.get<Direction>();
                        if (dirX < -0.1f)      d.facing = FacingDirection::Left;
                        else if (dirX > 0.1f)  d.facing = FacingDirection::Right;
                        else if (dirY < -0.1f) d.facing = FacingDirection::Up;
                        else if (dirY > 0.1f)  d.facing = FacingDirection::Down;
                    }
                }
            }

            void PhysicsSystem::update()
            {
                // Reserved for Phase 2: step Box2D world and copy transforms back.
                // Currently MovementSystem integrates position directly, so this
                // is a no-op placeholder kept here so run()'s system order matches
                // the documented architecture.
            }

            void CombatSystem::update()
            {
                // Step 1: two masks - one matches anything that can punch (has Intent
                // + a Direction + a Transform), the other matches anything that can
                // BE punched (has Health + Transform).
                static const bagel::Mask attackerMask = bagel::MaskBuilder()
                        .set<Intent>().set<Direction>().set<Transform>().build();
                static const bagel::Mask targetMask = bagel::MaskBuilder()
                        .set<Health>().set<Transform>().build();

                constexpr float PUNCH_REACH = 50.f; // How far in front of us the hitbox sits.
                constexpr float PUNCH_HALF  = 30.f; // Hitbox half-width / height.

                // Outer loop: walk every potential attacker.
                for (bagel::Entity attacker = bagel::Entity::first(); !attacker.eof(); attacker.next()) {
                    if (!attacker.test(attackerMask)) continue;
                    const auto& i = attacker.get<Intent>();
                    // Step 2: only enter punch logic if Intent.punch is set AND we
                    // are not already mid-punch (Punching component is the cooldown).
                    if (!i.punch || attacker.has<Punching>()) continue;

                    // Step 3: start the punch. Attach a Punching component with 8
                    // frames remaining - LifetimeSystem will tick it down and remove
                    // it when it hits zero, freeing the attacker to punch again.
                    attacker.add(Punching{8});

                    // Step 4: compute the hitbox center by offsetting the attacker's
                    // position by PUNCH_REACH in the direction they are facing.
                    const auto& at = attacker.get<Transform>();
                    const auto& ad = attacker.get<Direction>();
                    float hbX = at.position.x;
                    float hbY = at.position.y;
                    switch (ad.facing) {
                        case FacingDirection::Left:
                            hbX -= PUNCH_REACH;
                            break;
                        case FacingDirection::Right:
                            hbX += PUNCH_REACH;
                            break;
                        case FacingDirection::Up:
                            hbY -= PUNCH_REACH;
                            break;
                        case FacingDirection::Down:
                            hbY += PUNCH_REACH;
                            break;
                    }

                    // Step 5: spawn the visual flash at the hitbox center. The
                    // enemyPunch flag tints it blue (vs yellow for the player).
                    const bool attackerIsPlayer = attacker.has<PlayerTag>();
                    createFlashEntity({hbX, hbY}, !attackerIsPlayer);

                    // Step 6: inner loop - find all valid victims of this punch.
                    for (bagel::Entity target = bagel::Entity::first(); !target.eof(); target.next()) {
                        if (target.entity().id == attacker.entity().id) continue; // No self-hit.
                        if (!target.test(targetMask)) continue;                    // Must have Health.
                        if (target.has<IFrames>()) continue;                       // Just got hit recently.

                        // Step 7: faction check. The player only hurts enemies, enemies
                        // only hurt the player. No friendly fire between enemies.
                        if (attackerIsPlayer && !target.has<EnemyTag>()) continue;
                        if (!attackerIsPlayer && !target.has<PlayerTag>()) continue;

                        // Step 8: AABB overlap test between the punch hitbox and the
                        // target's center. The +24 / +32 padding approximates the
                        // target's half-size so the punch feels generous.
                        const auto& tt = target.get<Transform>();
                        if (std::abs(tt.position.x - hbX) < PUNCH_HALF + 24 &&
                            std::abs(tt.position.y - hbY) < PUNCH_HALF + 32) {
                            // Hit! Subtract 1 HP and grant 30 frames of invulnerability
                            // (~0.5s at 60 FPS) so a single punch doesn't drain all health.
                            target.get<Health>().current -= 1;
                            target.add(IFrames{30});
                        }
                    }
                }
            }

            void LifetimeSystem::update()
            {
                // This system handles three different transient components.
                // Each block has the SAME shape: walk entities, tick timer down,
                // act when it reaches zero. The key difference is what "expire"
                // means - destroy the whole entity vs remove just the component.

                // Block 1 - FlashEffect: the entire entity is the punch flash
                // visual. When the timer hits zero, destroy the entity so it
                // disappears from the world.
                {
                    static const bagel::Mask m = bagel::MaskBuilder().set<FlashEffect>().build();
                    for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
                        if (e.test(m)) {
                            auto& f = e.get<FlashEffect>();
                            if (--f.framesLeft <= 0) e.destroy();
                        }
                    }
                }
                // Block 2 - Punching: the cooldown component attached to the
                // attacker during a punch. When it expires we only remove that
                // ONE component; the attacker (player/enemy) keeps existing.
                {
                    static const bagel::Mask m = bagel::MaskBuilder().set<Punching>().build();
                    for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
                        if (e.test(m)) {
                            auto& p = e.get<Punching>();
                            if (--p.framesLeft <= 0) e.del<Punching>();
                        }
                    }
                }
                // Block 3 - IFrames: the "I just got hit, leave me alone" marker.
                // Same pattern - remove the single component, keep the entity.
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
                // Walk every entity with Health. If health dropped to zero,
                // destroy the entity. For named enemies, surface a death notice
                // in the HUD ("Daniel - died") before destruction so the player
                // sees who they killed.
                static const bagel::Mask m = bagel::MaskBuilder().set<Health>().build();
                for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
                    if (e.test(m) && e.get<Health>().current <= 0) {
                        if (e.has<EnemyTag>() && e.has<EnemyName>()) {
                            showEnemyDeathNotice(e.get<EnemyName>().value);
                        }
                        e.destroy();
                    }
                }
            }

            void LevelSystem::update()
            {
                // Step 1: if the game is already over, skip - the end screen is
                // showing and we should not re-trigger win/loss every frame.
                if (g_gameOver) return;

                // Step 2: loss condition. If findPlayer returns id < 0 the player
                // entity was destroyed by CollisionSystem. Switch to the end screen.
                if (findPlayer().id < 0) {
                    g_gameOver = true;
                    g_endReason = EndReason::Lost;
                    g_screenMode = ScreenMode::EndScreen;
                    std::cout << "Game Over!" << std::endl;
                    return;
                }

                // Step 3: progress condition. When all enemies are dead:
                //   - Level 1: start the 2-second "LEVEL 2" transition (which
                //     will spawnLevel2Enemies in run()).
                //   - Level 2: the game is won. Show the end screen.
                if (countEnemies() == 0) {
                    if (g_currentLevel == 1) {
                        std::cout << "Level 1 cleared - advancing to Level 2" << std::endl;
                        startLevelTransition(2);
                    } else {
                        std::cout << "You win!" << std::endl;
                        g_gameOver = true;
                        g_endReason = EndReason::Won;
                        g_screenMode = ScreenMode::EndScreen;
                    }
                }
            }

            void RenderSystem::update()
            {
                // Step 1: draw the background first, full screen. Everything else
                // is drawn on top of it, no z-sorting needed for this small game.
                if (g_texBackground) {
                    SDL_FRect bg = {0, 0, (float)Game::WIN_W, (float)Game::WIN_H};
                    SDL_RenderTexture(g_ren, g_texBackground, nullptr, &bg);
                }

                // Step 2: filter mask - any entity with Transform + Renderable.
                static const bagel::Mask m = bagel::MaskBuilder()
                        .set<Transform>().set<Renderable>().build();

                for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next()) {
                    if (!e.test(m)) continue;
                    const auto& t = e.get<Transform>();
                    const auto& r = e.get<Renderable>();
                    if (!r.visible) continue; // Honor the "hidden" flag.

                    // Step 3: flash entities are rendered as a solid color square
                    // (not a sprite). enemyPunch flag chooses blue vs yellow.
                    // `continue` skips the sprite branch below.
                    if (e.has<FlashEffect>()) {
                        SDL_FRect dest = {t.position.x - 18, t.position.y - 18, 36, 36};
                        const auto& flash = e.get<FlashEffect>();
                        if (flash.enemyPunch) {
                            SDL_SetRenderDrawColor(g_ren, 30, 130, 255, 255);   // Blue.
                        } else {
                            SDL_SetRenderDrawColor(g_ren, 255, 230, 0, 255);    // Yellow.
                        }
                        SDL_RenderFillRect(g_ren, &dest);
                        SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);            // Restore black.
                        continue;
                    }

                    // Step 4: resolve the sprite name to its actual texture pointer.
                    SDL_Texture* tex = texFor(r.spriteName);

                    // Step 5: build the destination rect. If the entity has a
                    // Collider we use its size (sprite drawn at collider size,
                    // centered on the position). Fallback: a 32x32 box.
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

                    // Step 6: pick horizontal flip based on facing so the sprite
                    // visually mirrors when walking left. Up / Down / Right use
                    // the un-flipped sprite.
                    SDL_FlipMode flip = SDL_FLIP_NONE;
                    if (e.has<Direction>() &&
                        e.get<Direction>().facing == FacingDirection::Left) {
                        flip = SDL_FLIP_HORIZONTAL;
                    }

                    if (tex) {
                        // Step 7: i-frame flicker. Toggle visible/dim every 4 frames
                        // while invulnerable so the player can see they got hit.
                        const bool dim = e.has<IFrames>() &&
                                         (e.get<IFrames>().framesLeft / 4) % 2 == 0;
                        if (dim) SDL_SetTextureAlphaMod(tex, 110);
                        // RenderTextureRotated supports flipping; rotation is 0 here.
                        SDL_RenderTextureRotated(g_ren, tex, nullptr, &dest, 0, nullptr, flip);
                        if (dim) SDL_SetTextureAlphaMod(tex, 255); // Restore full alpha.
                    } else {
                        // Step 8: graceful fallback when a texture is missing -
                        // draw a colored rectangle so we can still tell what
                        // the entity was supposed to be.
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
                // Step 1: find the player. If the player is dead (or has no Health
                // for some reason), there is no HUD to draw.
                const auto pid = findPlayer();
                if (pid.id < 0) return;
                bagel::Entity player{pid};
                if (!player.has<Health>()) return;

                // Step 2: draw one red 20x20 square per remaining HP, lined up in
                // the top-left corner. 24 px stride = 20 px square + 4 px gap.
                const auto& h = player.get<Health>();
                SDL_SetRenderDrawColor(g_ren, 220, 30, 30, 255);
                for (int i = 0; i < h.current; ++i) {
                    SDL_FRect r = {10 + i * 24.f, 10, 20, 20};
                    SDL_RenderFillRect(g_ren, &r);
                }

                // Step 3: if a death notice is queued and its 2.5-second window
                // is still active, draw it in the top-right corner.
                // The text width is computed so we can right-align it (anchor at
                // WIN_W minus width minus 16 px padding).
                if (g_deathNotice[0] != '\0' &&
                    SDL_GetTicks() - g_deathNoticeStartedAt < DEATH_NOTICE_MS) {
                    constexpr float scale = 2.f;
                    const float width = SDL_strlen(g_deathNotice) *
                                        SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
                    SDL_SetRenderDrawColor(g_ren, 255, 255, 255, 255);
                    drawDebugText(g_deathNotice, Game::WIN_W - width - 16.f, 14.f, scale);
                }

                // Step 4: restore the default black draw color for the next frame.
                SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
            }
        }
