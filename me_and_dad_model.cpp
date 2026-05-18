#include "me_and_dad_model.h"

#include <SDL3/SDL_scancode.h>

/**
 * @file me_and_dad_model.cpp
 * @brief Implementation of entity factories.
 *
 * Factories use @c bagel::World::addComponent (rather than calling
 * @c Storage::add directly) so that each entity's mask bit is set.
 * Without this the systems' @c MaskBuilder filters would reject every
 * entity at runtime.
 */

namespace me_and_dad
{
	namespace
	{
		/**
		 * @brief Create an entity with the three components every visible thing needs.
		 * @param position World position.
		 * @param spriteName Sprite key for the renderer.
		 * @param colliderSize AABB collider size.
		 * @return The new entity id.
		 */
		ent_type createBaseEntity(Vec2 position, const char* spriteName, Vec2 colliderSize)
		{
			// Step 1: ask the ECS World for a fresh, unused entity id.
			// At this point the entity has no components attached - it is just an id.
			ent_type entity = bagel::World::createEntity();

			// Step 2: attach a Transform so the entity has a place in the world.
			// Position comes from the caller; scale is 1:1 (no stretching).
			bagel::World::addComponent<Transform>(entity, {{position.x, position.y}, {1, 1}});

			// Step 3: attach a Renderable so the RenderSystem knows what sprite to draw.
			// spriteIndex = -1 means "use the whole sprite, no animation frame"; visible = true.
			bagel::World::addComponent<Renderable>(entity, {spriteName, -1, true});

			// Step 4: attach a Collider so physics / hit detection can see this entity.
			// Offset {0,0} means the collider is centered on the Transform. We start with
			// b2_nullBodyId because the Box2D body is created later by PhysicsSystem.
			bagel::World::addComponent<Collider>(entity, {{0, 0}, colliderSize, true, b2_nullBodyId});

			// Step 5: hand the configured entity id back to the specific factory
			// (player / enemy / static object) that will add the rest of its components.
			return entity;
		}

		/**
		 * @brief Build a patrol Path from a small list of waypoints.
		 *
		 * Path stores points in a fixed-size array (max 8) to keep the
		 * component POD-friendly. We copy in the caller's points, clamp to
		 * the capacity, and reset the patrol cursor.
		 */
		Path buildPath(std::initializer_list<Vec2> points)
		{
			// Step 1: zero-initialize the Path so every field has a defined value
			// (pointCount = 0, currentPoint = 0, loop = false, points all zero).
			Path path = {};

			// Step 2: copy the caller's waypoints into the fixed-size array.
			// We stop at 8 because Path::points has a capacity of 8 - extra
			// points are silently dropped rather than overflowing the buffer.
			for (const auto& point : points) {
				if (path.pointCount < 8) {
					path.points[path.pointCount++] = point;
				}
			}

			// Step 3: start the patrol from the first waypoint and tell the
			// AI to loop back to the start once it reaches the end.
			path.currentPoint = 0;
			path.loop = true;
			return path;
		}

	}

	ent_type createPlayer(Vec2 position)
	{
		// Step 1: build the shared visual/physical skeleton - Transform, Renderable, Collider.
		// "kid" is the sprite key registered in the asset map; {42, 64} is the collider box size.
		ent_type entity = createBaseEntity(position, "kid", {42, 64});

		// Step 2: identity tags. PlayerTag lets systems filter "this is the player",
		// and InputControlled tells the InputSystem to read keyboard for this entity.
		bagel::World::addComponent<PlayerTag>(entity, {});
		bagel::World::addComponent<InputControlled>(entity, {});

		// Step 3: motion + combat stats.
		// Velocity starts at zero; 6 is the max speed cap MovementSystem will enforce.
		// Health starts full (5/5). Damage = 1 means each punch removes one HP from the target.
		bagel::World::addComponent<Velocity>(entity, {{0, 0}, 6});
		bagel::World::addComponent<Health>(entity, {5, 5});
		bagel::World::addComponent<Damage>(entity, {1});

		// Step 4: facing + animation state. Player spawns facing right and idle;
		// these are updated by InputSystem (facing) and CombatSystem (state).
		bagel::World::addComponent<Direction>(entity, {FacingDirection::Right});
		bagel::World::addComponent<State>(entity, {EntityState::Idle});

		// Step 5: Intent is the "what does this entity want to do this frame" buffer.
		// InputSystem writes into it from the keyboard; MovementSystem/CombatSystem read it.
		bagel::World::addComponent<Intent>(entity, {});

		// Step 6: bind keyboard scancodes to actions. We store SDL scancodes as ints
		// (see Keys struct) so the model header stays free of SDL dependencies.
		bagel::World::addComponent<Keys>(entity, {
			SDL_SCANCODE_UP,
			SDL_SCANCODE_DOWN,
			SDL_SCANCODE_LEFT,
			SDL_SCANCODE_RIGHT,
			SDL_SCANCODE_SPACE
		});

		return entity;

	}

	ent_type createNormalEnemy(std::initializer_list<Vec2> patrolPoints, const char* name)
	{
		// Step 1: convert the waypoint list into a Path component, then use the
		// first waypoint as the spawn position (fallback to origin if the list is empty).
		const Path path = buildPath(patrolPoints);
		const Vec2 spawn = path.pointCount > 0 ? path.points[0] : Vec2{0, 0};

		// Step 2: build the base entity (Transform/Renderable/Collider) using the
		// "normal_enemy" sprite and a 44x66 collider box.
		ent_type entity = createBaseEntity(spawn, "normal_enemy", {44, 66});

		// Step 3: mark this entity as an enemy so CombatSystem and AISystem pick it up.
		bagel::World::addComponent<EnemyTag>(entity, {});

		// Step 4: combat stats - slower than the player (maxSpeed = 1.5), only 2 HP, 1 damage per hit.
		bagel::World::addComponent<Velocity>(entity, {{0, 0}, 1.5f});
		bagel::World::addComponent<Health>(entity, {2, 2});
		bagel::World::addComponent<Damage>(entity, {1});

		// Step 5: visual / behavioral state. Enemies start facing left and idle;
		// AISystem will switch them into Walking when patrolling.
		bagel::World::addComponent<Direction>(entity, {FacingDirection::Left});
		bagel::World::addComponent<State>(entity, {EntityState::Idle});

		// Step 6: HUD-facing name (shown on the enemy health bar) and the per-frame Intent buffer
		// that AISystem fills in instead of the keyboard.
		bagel::World::addComponent<EnemyName>(entity, {name});
		bagel::World::addComponent<Intent>(entity, {});

		// Step 7: AI configuration. AiKind::Patrol tells AISystem to follow Path waypoints;
		// detectionRange = 0 because this enemy never chases the player, only patrols.
		bagel::World::addComponent<AI>(entity, {AiKind::Patrol, 0});

		// Step 8: attach the waypoint Path the AI will walk along.
		bagel::World::addComponent<Path>(entity, path);

		return entity;
	}

	ent_type createSpecialEnemy(std::initializer_list<Vec2> patrolPoints, const char* name)
	{
		// Step 1: same waypoint -> Path conversion + spawn pick as the normal enemy.
		const Path path = buildPath(patrolPoints);
		const Vec2 spawn = path.pointCount > 0 ? path.points[0] : Vec2{0, 0};

		// Step 2: base entity uses the "special_enemy" sprite and a slightly larger
		// collider (52x74) because special enemies are physically bigger on screen.
		ent_type entity = createBaseEntity(spawn, "special_enemy", {52, 74});

		// Step 3: tag this entity as BOTH an enemy AND a "special" enemy.
		// SpecialEnemyTag is what CombatSystem checks to allow this enemy to punch back.
		bagel::World::addComponent<EnemyTag>(entity, {});
		bagel::World::addComponent<SpecialEnemyTag>(entity, {});

		// Step 4: combat stats - faster than a normal enemy (maxSpeed = 2.0), same HP and damage.
		bagel::World::addComponent<Velocity>(entity, {{0, 0}, 2.f});
		bagel::World::addComponent<Health>(entity, {2, 2});
		bagel::World::addComponent<Damage>(entity, {1});

		// Step 5: identical facing/state/name/intent setup as the normal enemy.
		bagel::World::addComponent<Direction>(entity, {FacingDirection::Left});
		bagel::World::addComponent<State>(entity, {EntityState::Idle});
		bagel::World::addComponent<EnemyName>(entity, {name});
		bagel::World::addComponent<Intent>(entity, {});

		// Step 6: AI + Path. Same patrol behavior; the punch-back ability comes purely
		// from the SpecialEnemyTag above, not from a different AI kind.
		bagel::World::addComponent<AI>(entity, {AiKind::Patrol, 0});
		bagel::World::addComponent<Path>(entity, path);

		return entity;
	}

	ent_type createStaticObject(Vec2 position, const char* spriteName, Vec2 colliderSize, bool solid)
	{
		// Step 1: build the base entity with the caller-supplied sprite + collider size.
		// Static objects only need Transform / Renderable / Collider - no Velocity, no AI.
		ent_type entity = createBaseEntity(position, spriteName, colliderSize);

		// Step 2: mark this entity as static. PhysicsSystem uses this tag to create
		// a Box2D static body (zero mass, never moves) instead of a dynamic body.
		bagel::World::addComponent<StaticObjectTag>(entity, {});

		// Step 3: override the Collider's "solid" flag in-place. createBaseEntity
		// always sets solid = true, but some decorations (e.g. visual-only props)
		// should pass through, so we patch the stored Collider directly here.
		bagel::Storage<Collider>::type::get(entity).solid = solid;

		return entity;
	}

	ent_type createFlashEntity(Vec2 position, bool enemyPunch)
	{
		// Step 1: allocate a brand-new entity. We don't use createBaseEntity here
		// because the flash has no Collider - it's a pure visual effect.
		ent_type entity = bagel::World::createEntity();

		// Step 2: place the flash sprite at the impact position, scale 1:1.
		bagel::World::addComponent<Transform>(entity, {{position.x, position.y}, {1, 1}});

		// Step 3: render with the "flash" sprite. spriteIndex = -1 means draw the
		// full sprite, visible = true so it's drawn this frame.
		bagel::World::addComponent<Renderable>(entity, {"flash", -1, true});

		// Step 4: attach the FlashEffect timer. framesLeft = 6 means the flash lives
		// for 6 frames before LifetimeSystem destroys the entity. enemyPunch tells
		// the renderer/audio whether this was an enemy hitting the player (different tint/sound).
		bagel::World::addComponent<FlashEffect>(entity, {6, enemyPunch});

		return entity;
	}

	// System implementations live in Game.cpp where they have access to
	// the SDL renderer and Box2D world via file-scope statics.
}
