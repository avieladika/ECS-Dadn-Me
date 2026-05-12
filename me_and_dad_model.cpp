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
			ent_type entity = bagel::World::createEntity();

			bagel::World::addComponent<Transform>(entity, {{position.x, position.y}, {1, 1}});
			bagel::World::addComponent<Renderable>(entity, {spriteName, -1, true});
			bagel::World::addComponent<Collider>(entity, {{0, 0}, colliderSize, true, b2_nullBodyId});

			return entity;
		}

		Path buildPath(std::initializer_list<Vec2> points)
		{
			Path path = {};
			for (const auto& point : points) {
				if (path.pointCount < 8) {
					path.points[path.pointCount++] = point;
				}
			}
			path.currentPoint = 0;
			path.loop = true;
			return path;
		}

	}

	ent_type createPlayer(Vec2 position)
	{
		ent_type entity = createBaseEntity(position, "kid", {42, 64});

		bagel::World::addComponent<PlayerTag>(entity, {});
		bagel::World::addComponent<InputControlled>(entity, {});
		bagel::World::addComponent<Velocity>(entity, {{0, 0}, 6});
		bagel::World::addComponent<Health>(entity, {5, 5});
		bagel::World::addComponent<Damage>(entity, {1});
		bagel::World::addComponent<Direction>(entity, {FacingDirection::Right});
		bagel::World::addComponent<State>(entity, {EntityState::Idle});
		bagel::World::addComponent<Intent>(entity, {});
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
		const Path path = buildPath(patrolPoints);
		const Vec2 spawn = path.pointCount > 0 ? path.points[0] : Vec2{0, 0};
		ent_type entity = createBaseEntity(spawn, "normal_enemy", {44, 66});

		bagel::World::addComponent<EnemyTag>(entity, {});
		bagel::World::addComponent<Velocity>(entity, {{0, 0}, 1.5f});
		bagel::World::addComponent<Health>(entity, {2, 2});
		bagel::World::addComponent<Damage>(entity, {1});
		bagel::World::addComponent<Direction>(entity, {FacingDirection::Left});
		bagel::World::addComponent<State>(entity, {EntityState::Idle});
		bagel::World::addComponent<EnemyName>(entity, {name});
		bagel::World::addComponent<Intent>(entity, {});
		bagel::World::addComponent<AI>(entity, {AiKind::Patrol, 0});
		bagel::World::addComponent<Path>(entity, path);

		return entity;
	}

	ent_type createSpecialEnemy(std::initializer_list<Vec2> patrolPoints, const char* name)
	{
		const Path path = buildPath(patrolPoints);
		const Vec2 spawn = path.pointCount > 0 ? path.points[0] : Vec2{0, 0};
		ent_type entity = createBaseEntity(spawn, "special_enemy", {52, 74});

		bagel::World::addComponent<EnemyTag>(entity, {});
		bagel::World::addComponent<SpecialEnemyTag>(entity, {});
		bagel::World::addComponent<Velocity>(entity, {{0, 0}, 2.f});
		bagel::World::addComponent<Health>(entity, {2, 2});
		bagel::World::addComponent<Damage>(entity, {1});
		bagel::World::addComponent<Direction>(entity, {FacingDirection::Left});
		bagel::World::addComponent<State>(entity, {EntityState::Idle});
		bagel::World::addComponent<EnemyName>(entity, {name});
		bagel::World::addComponent<Intent>(entity, {});
		bagel::World::addComponent<AI>(entity, {AiKind::Patrol, 0});
		bagel::World::addComponent<Path>(entity, path);

		return entity;
	}

	ent_type createStaticObject(Vec2 position, const char* spriteName, Vec2 colliderSize, bool solid)
	{
		ent_type entity = createBaseEntity(position, spriteName, colliderSize);

		bagel::World::addComponent<StaticObjectTag>(entity, {});
		bagel::Storage<Collider>::type::get(entity).solid = solid;

		return entity;
	}

	ent_type createFlashEntity(Vec2 position, bool enemyPunch)
	{
		ent_type entity = bagel::World::createEntity();

		bagel::World::addComponent<Transform>(entity, {{position.x, position.y}, {1, 1}});
		bagel::World::addComponent<Renderable>(entity, {"flash", -1, true});
		bagel::World::addComponent<FlashEffect>(entity, {6, enemyPunch});

		return entity;
	}

	// System implementations live in Game.cpp where they have access to
	// the SDL renderer and Box2D world via file-scope statics.
}
