#pragma once

#include "bagel.h"

// Game model for "Me and Dad".
namespace me_and_dad
{
	using bagel::ent_type;

	enum class FacingDirection {
		Left,
		Right,
		Up,
		Down
	};

	enum class EntityState {
		Idle,
		Walking,
		Jumping,
		Attacking,
		Hit,
		Thrown,
		Exploding,
		Dead
	};

	enum class AiKind {
		None,
		Patrol,
		ChasePlayer,
		SpecialEnemy
	};

	struct Vec2 {
		float x = 0;
		float y = 0;
	};

	// stores entity position
	struct Transform {
		Vec2 position = {};
		Vec2 scale = {1, 1};
	};

	// stores movement speed and direction
	struct Velocity {
		Vec2 value = {};
		float maxSpeed = 0;
	};

	// stores how the entity is drawn
	struct Renderable {
		const char* spriteName = nullptr;
		int spriteIndex = -1;
		bool visible = true;
	};

	// stores collision size
	struct Collider {
		Vec2 offset = {};
		Vec2 size = {};
		bool solid = true;
	};

	// stores current and max HP
	struct Health {
		int current = 100;
		int maximum = 100;
	};

	// stores attack damage
	struct Damage {
		int amount = 0;
	};

	// stores facing direction
	struct Direction {
		FacingDirection facing = FacingDirection::Right;
	};

	// stores current action/state
	struct State {
		EntityState current = EntityState::Idle;
	};

	// stores jump ability data
	struct Jump {
		bool canJump = false;
		bool isJumping = false;
		float force = 0;
	};

	// stores enemy behavior type
	struct AI {
		AiKind kind = AiKind::None;
		float detectionRange = 0;
	};

	// stores patrol route points
	struct Path {
		Vec2 points[8] = {};
		int pointCount = 0;
		int currentPoint = 0;
		bool loop = true;
	};

	// marks objects the player can interact with
	struct Interactable {
		bool canInteract = false;
		int interactionType = -1;
	};

	// marks objects that can be thrown
	struct Throwable {
		bool canBeThrown = true;
		float throwForce = 0;
	};

	// stores sound information
	struct Sound {
		const char* soundName = nullptr;
		int soundId = -1;
	};

	// stores superpower mode state
	struct PowerMode {
		bool active = false;
		float timeLeft = 0;
	};

	// stores combo state
	struct Combo {
		int hitCount = 0;
		float timeLeft = 0;
	};

	// stores current level number
	struct Level {
		int number = -1;
	};

	// stores player statistics
	struct Stats {
		int score = 0;
		int enemiesDefeated = 0;
		int objectsThrown = 0;
	};

	// stores time until entity disappears
	struct Lifetime {
		float timeLeft = 0;
	};

	// marks keyboard-controlled entity
	struct InputControlled {};
	// marks the player
	struct PlayerTag {};
	// marks an enemy
	struct EnemyTag {};
	// marks a special enemy
	struct SpecialEnemyTag {};
	// marks a projectile
	struct ProjectileTag {};
	// marks a static object
	struct StaticObjectTag {};
	// marks the board
	struct BoardTag {};
	// marks the legend board
	struct LegendBoardTag {};
	// marks a bomb
	struct BombTag {};

	// Entity factory functions.
	// Each function creates an entity and adds the matching components with bagel::Storage::add.
	ent_type createPlayer(Vec2 position = {0, 0});
	ent_type createNormalEnemy(Vec2 position = {0, 0});
	ent_type createSpecialEnemy(Vec2 position = {0, 0});
	ent_type createThrowableRock(Vec2 position = {0, 0});
	ent_type createStaticObject(
		Vec2 position = {0, 0},
		const char* spriteName = nullptr,
		Vec2 colliderSize = {0, 0},
		bool solid = true);
	ent_type createBullet(Vec2 position = {0, 0}, FacingDirection direction = FacingDirection::Right);
	ent_type createBoard(int levelNumber = -1);
	ent_type createLegendBoard();
	ent_type createBomb(Vec2 position = {0, 0});

	// Systems: empty update functions for now, as required by the assignment.
	// The behavior will be implemented later in the cpp file.
	struct InputSystem {
		static void update();
	};

	struct MovementSystem {
		static void update();
	};

	struct PhysicsSystem {
		static void update();
	};

	struct CollisionSystem {
		static void update();
	};

	struct CombatSystem {
		static void update();
	};

	struct AISystem {
		static void update();
	};

	struct AbilitySystem {
		static void update();
	};

	struct RenderSystem {
		static void update();
	};

	struct SoundSystem {
		static void update();
	};

	struct LifetimeSystem {
		static void update();
	};
}

template <> struct bagel::Storage<me_and_dad::InputControlled> final : NoInstance {
	using type = TaggedStorage<me_and_dad::InputControlled>;
};

template <> struct bagel::Storage<me_and_dad::PlayerTag> final : NoInstance {
	using type = TaggedStorage<me_and_dad::PlayerTag>;
};

template <> struct bagel::Storage<me_and_dad::EnemyTag> final : NoInstance {
	using type = TaggedStorage<me_and_dad::EnemyTag>;
};

template <> struct bagel::Storage<me_and_dad::SpecialEnemyTag> final : NoInstance {
	using type = TaggedStorage<me_and_dad::SpecialEnemyTag>;
};

template <> struct bagel::Storage<me_and_dad::ProjectileTag> final : NoInstance {
	using type = TaggedStorage<me_and_dad::ProjectileTag>;
};

template <> struct bagel::Storage<me_and_dad::StaticObjectTag> final : NoInstance {
	using type = TaggedStorage<me_and_dad::StaticObjectTag>;
};

template <> struct bagel::Storage<me_and_dad::BoardTag> final : NoInstance {
	using type = TaggedStorage<me_and_dad::BoardTag>;
};

template <> struct bagel::Storage<me_and_dad::LegendBoardTag> final : NoInstance {
	using type = TaggedStorage<me_and_dad::LegendBoardTag>;
};

template <> struct bagel::Storage<me_and_dad::BombTag> final : NoInstance {
	using type = TaggedStorage<me_and_dad::BombTag>;
};

template <> struct bagel::Storage<me_and_dad::Transform> final : NoInstance {
	using type = PackedStorage<me_and_dad::Transform>;
};

template <> struct bagel::Storage<me_and_dad::Velocity> final : NoInstance {
	using type = PackedStorage<me_and_dad::Velocity>;
};

template <> struct bagel::Storage<me_and_dad::Renderable> final : NoInstance {
	using type = PackedStorage<me_and_dad::Renderable>;
};

template <> struct bagel::Storage<me_and_dad::Collider> final : NoInstance {
	using type = PackedStorage<me_and_dad::Collider>;
};

template <> struct bagel::Storage<me_and_dad::Direction> final : NoInstance {
	using type = PackedStorage<me_and_dad::Direction>;
};

template <> struct bagel::Storage<me_and_dad::State> final : NoInstance {
	using type = PackedStorage<me_and_dad::State>;
};

template <> struct bagel::Storage<me_and_dad::Lifetime> final : NoInstance {
	using type = PackedStorage<me_and_dad::Lifetime>;
};
