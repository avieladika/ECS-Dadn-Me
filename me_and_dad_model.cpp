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
			SDL_SCANCODE_W,
			SDL_SCANCODE_S,
			SDL_SCANCODE_A,
			SDL_SCANCODE_D,
			SDL_SCANCODE_SPACE
		});

		return entity;
	}

	ent_type createNormalEnemy(Vec2 position)
	{
		ent_type entity = createBaseEntity(position, "normal_enemy", {44, 66});

		bagel::World::addComponent<EnemyTag>(entity, {});
		bagel::World::addComponent<Velocity>(entity, {{0, 0}, 3});
		bagel::World::addComponent<Health>(entity, {2, 2});
		bagel::World::addComponent<Damage>(entity, {1});
		bagel::World::addComponent<Direction>(entity, {FacingDirection::Left});
		bagel::World::addComponent<State>(entity, {EntityState::Idle});
		bagel::World::addComponent<Intent>(entity, {});
		bagel::World::addComponent<AI>(entity, {AiKind::ChasePlayer, 180});

		return entity;
	}

	ent_type createSpecialEnemy(Vec2 position)
	{
		ent_type entity = createBaseEntity(position, "special_enemy", {52, 74});

		bagel::World::addComponent<EnemyTag>(entity, {});
		bagel::World::addComponent<SpecialEnemyTag>(entity, {});
		bagel::World::addComponent<Velocity>(entity, {{0, 0}, 4});
		bagel::World::addComponent<Health>(entity, {2, 2});
		bagel::World::addComponent<Damage>(entity, {1});
		bagel::World::addComponent<Direction>(entity, {FacingDirection::Left});
		bagel::World::addComponent<State>(entity, {EntityState::Idle});
		bagel::World::addComponent<Intent>(entity, {});
		bagel::World::addComponent<AI>(entity, {AiKind::ChasePlayer, 260});

		return entity;
	}

	ent_type createStaticObject(Vec2 position, const char* spriteName, Vec2 colliderSize, bool solid)
	{
		ent_type entity = createBaseEntity(position, spriteName, colliderSize);

		bagel::World::addComponent<StaticObjectTag>(entity, {});
		bagel::Storage<Collider>::type::get(entity).solid = solid;

		return entity;
	}

	ent_type createFlashEntity(Vec2 position)
	{
		ent_type entity = bagel::World::createEntity();

		bagel::World::addComponent<Transform>(entity, {{position.x, position.y}, {1, 1}});
		bagel::World::addComponent<Renderable>(entity, {"flash", -1, true});
		bagel::World::addComponent<FlashEffect>(entity, {6});

		return entity;
	}

	// System implementations live in Game.cpp where they have access to
	// the SDL renderer and Box2D world via file-scope statics.
}
