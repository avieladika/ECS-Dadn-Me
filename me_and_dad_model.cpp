#include "me_and_dad_model.h"

namespace me_and_dad
{
	namespace
	{
		ent_type createBaseEntity(Vec2 position, const char* spriteName, Vec2 colliderSize)
		{
			ent_type entity = bagel::World::createEntity();

			bagel::Storage<Transform>::type::add(entity, {{position.x, position.y}, {1, 1}});
			bagel::Storage<Renderable>::type::add(entity, {spriteName, -1, true});
			bagel::Storage<Collider>::type::add(entity, {{0, 0}, colliderSize, true});

			return entity;
		}

		Vec2 directionToVelocity(FacingDirection direction, float speed)
		{
			switch (direction) {
			case FacingDirection::Left:
				return {-speed, 0};
			case FacingDirection::Right:
				return {speed, 0};
			case FacingDirection::Up:
				return {0, -speed};
			case FacingDirection::Down:
				return {0, speed};
			}

			return {0, 0};
		}
	}

	ent_type createPlayer(Vec2 position)
	{
		ent_type entity = createBaseEntity(position, "kid", {42, 64});

		bagel::Storage<PlayerTag>::type::add(entity, {});
		bagel::Storage<InputControlled>::type::add(entity, {});
		bagel::Storage<Velocity>::type::add(entity, {{0, 0}, 6});
		bagel::Storage<Health>::type::add(entity, {100, 100});
		bagel::Storage<Damage>::type::add(entity, {12});
		bagel::Storage<Direction>::type::add(entity, {FacingDirection::Right});
		bagel::Storage<State>::type::add(entity, {EntityState::Idle});
		bagel::Storage<Jump>::type::add(entity, {true, false, 12});
		bagel::Storage<PowerMode>::type::add(entity, {false, 0});
		bagel::Storage<Combo>::type::add(entity, {0, 0});
		bagel::Storage<Level>::type::add(entity, {-1});
		bagel::Storage<Stats>::type::add(entity, {0, 0, 0});
		bagel::Storage<Sound>::type::add(entity, {nullptr, -1});

		return entity;
	}

	ent_type createNormalEnemy(Vec2 position)
	{
		ent_type entity = createBaseEntity(position, "normal_enemy", {44, 66});

		bagel::Storage<EnemyTag>::type::add(entity, {});
		bagel::Storage<Velocity>::type::add(entity, {{0, 0}, 3});
		bagel::Storage<Health>::type::add(entity, {100, 100});
		bagel::Storage<Damage>::type::add(entity, {8});
		bagel::Storage<Direction>::type::add(entity, {FacingDirection::Left});
		bagel::Storage<State>::type::add(entity, {EntityState::Idle});
		bagel::Storage<AI>::type::add(entity, {AiKind::Patrol, 180});
		bagel::Storage<Path>::type::add(entity, {{{position.x - 80, position.y}, {position.x + 80, position.y}}, 2, 0, true});
		bagel::Storage<Sound>::type::add(entity, {nullptr, -1});

		return entity;
	}

	ent_type createSpecialEnemy(Vec2 position)
	{
		ent_type entity = createBaseEntity(position, "special_enemy", {52, 74});

		bagel::Storage<EnemyTag>::type::add(entity, {});
		bagel::Storage<SpecialEnemyTag>::type::add(entity, {});
		bagel::Storage<Velocity>::type::add(entity, {{0, 0}, 4});
		bagel::Storage<Health>::type::add(entity, {100, 100});
		bagel::Storage<Damage>::type::add(entity, {16});
		bagel::Storage<Direction>::type::add(entity, {FacingDirection::Left});
		bagel::Storage<State>::type::add(entity, {EntityState::Idle});
		bagel::Storage<AI>::type::add(entity, {AiKind::SpecialEnemy, 260});
		bagel::Storage<Path>::type::add(entity, {{{position.x - 120, position.y}, {position.x + 120, position.y}}, 2, 0, true});
		bagel::Storage<PowerMode>::type::add(entity, {false, 0});
		bagel::Storage<Sound>::type::add(entity, {nullptr, -1});

		return entity;
	}

	ent_type createThrowableRock(Vec2 position)
	{
		ent_type entity = createBaseEntity(position, "rock", {24, 24});

		bagel::Storage<StaticObjectTag>::type::add(entity, {});
		bagel::Storage<Throwable>::type::add(entity, {true, 10});
		bagel::Storage<Velocity>::type::add(entity, {{0, 0}, 12});
		bagel::Storage<Damage>::type::add(entity, {20});
		bagel::Storage<State>::type::add(entity, {EntityState::Idle});
		bagel::Storage<Interactable>::type::add(entity, {true, 1});
		bagel::Storage<Sound>::type::add(entity, {nullptr, -1});

		return entity;
	}

	ent_type createStaticObject(Vec2 position, const char* spriteName, Vec2 colliderSize, bool solid)
	{
		ent_type entity = createBaseEntity(position, spriteName, colliderSize);

		bagel::Storage<StaticObjectTag>::type::add(entity, {});
		bagel::Storage<Collider>::type::get(entity).solid = solid;

		return entity;
	}

	ent_type createBullet(Vec2 position, FacingDirection direction)
	{
		ent_type entity = createBaseEntity(position, "bullet", {12, 12});

		bagel::Storage<ProjectileTag>::type::add(entity, {});
		bagel::Storage<Velocity>::type::add(entity, {directionToVelocity(direction, 14), 14});
		bagel::Storage<Damage>::type::add(entity, {18});
		bagel::Storage<Direction>::type::add(entity, {direction});
		bagel::Storage<State>::type::add(entity, {EntityState::Idle});
		bagel::Storage<Lifetime>::type::add(entity, {3});
		bagel::Storage<Sound>::type::add(entity, {nullptr, -1});

		return entity;
	}

	ent_type createBoard(int levelNumber)
	{
		ent_type entity = createBaseEntity({0, 0}, "board", {800, 600});

		bagel::Storage<BoardTag>::type::add(entity, {});
		bagel::Storage<Level>::type::add(entity, {levelNumber});
		bagel::Storage<Collider>::type::get(entity).solid = false;

		return entity;
	}

	ent_type createLegendBoard()
	{
		ent_type entity = createBaseEntity({0, 0}, "legend_board", {220, 120});

		bagel::Storage<LegendBoardTag>::type::add(entity, {});
		bagel::Storage<Collider>::type::get(entity).solid = false;

		return entity;
	}

	ent_type createBomb(Vec2 position)
	{
		ent_type entity = createBaseEntity(position, "bomb", {30, 30});

		bagel::Storage<BombTag>::type::add(entity, {});
		bagel::Storage<Velocity>::type::add(entity, {{0, 0}, 8});
		bagel::Storage<Damage>::type::add(entity, {45});
		bagel::Storage<State>::type::add(entity, {EntityState::Idle});
		bagel::Storage<Lifetime>::type::add(entity, {5});
		bagel::Storage<Sound>::type::add(entity, {nullptr, -1});

		return entity;
	}

	void InputSystem::update() {}
	void MovementSystem::update() {}
	void PhysicsSystem::update() {}
	void CollisionSystem::update() {}
	void CombatSystem::update() {}
	void AISystem::update() {}
	void AbilitySystem::update() {}
	void RenderSystem::update() {}
	void SoundSystem::update() {}
	void LifetimeSystem::update() {}
}
