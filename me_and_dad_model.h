#pragma once

#include <initializer_list>
#include <box2d/box2d.h>
#include "bagel.h"

/**
 * @file me_and_dad_model.h
 * @brief Game model for "Me and Dad" - components, entity factories and systems.
 */

namespace me_and_dad
{
	using bagel::ent_type;

	/**
	 * @brief Facing direction for an entity in 4-way movement.
	 */
	enum class FacingDirection {
		Left,
		Right,
		Up,
		Down
	};

	/**
	 * @brief Animation / behavior state of an entity.
	 *
	 * The presence of @ref Punching, @ref IFrames or @ref FlashEffect
	 * components also encodes transient state and may override this value
	 * when picking the sprite.
	 */
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

	/**
	 * @brief Identifies which AI behavior an enemy uses.
	 */
	enum class AiKind {
		None,
		ChasePlayer,
		Patrol
	};

	/**
	 * @brief Simple 2D vector used for positions, sizes and velocities.
	 */
	struct Vec2 {
		float x = 0;
		float y = 0;
	};

	/* ------------------------------------------------------------------ */
	/*  Persistent components                                             */
	/* ------------------------------------------------------------------ */

	/**
	 * @brief World-space position and scale of an entity.
	 */
	struct Transform {
		Vec2 position = {};
		Vec2 scale = {1, 1};
	};

	/**
	 * @brief Movement velocity (pixels/frame) and the max speed allowed.
	 */
	struct Velocity {
		Vec2 value = {};
		float maxSpeed = 0;
	};

	/**
	 * @brief Sprite reference used by the render system.
	 */
	struct Renderable {
		const char* spriteName = nullptr;
		int spriteIndex = -1;
		bool visible = true;
	};

	/**
	 * @brief AABB collider plus optional Box2D body for physics-driven entities.
	 */
	struct Collider {
		Vec2 offset = {};
		Vec2 size = {};
		bool solid = true;
		b2BodyId body = b2_nullBodyId;
	};

	/**
	 * @brief Health points (current and maximum).
	 */
	struct Health {
		int current = 5;
		int maximum = 5;
	};

	/**
	 * @brief Damage amount this entity inflicts on a successful hit.
	 */
	struct Damage {
		int amount = 0;
	};

	/**
	 * @brief Facing direction of an entity. Used by render flip and punch hitbox.
	 */
	struct Direction {
		FacingDirection facing = FacingDirection::Right;
	};

	/**
	 * @brief Current animation / behavior state. See @ref EntityState.
	 */
	struct State {
		EntityState current = EntityState::Idle;
	};

	/**
	 * @brief Display name assigned to an enemy when it is created.
	 */
	struct EnemyName {
		const char* value = nullptr;
	};

	/**
	 * @brief Jump ability data. Unused in the current scope but kept for future levels.
	 */
	struct Jump {
		bool canJump = false;
		bool isJumping = false;
		float force = 0;
	};

	/**
	 * @brief AI behavior selector + detection range.
	 */
	struct AI {
		AiKind kind = AiKind::None;
		float detectionRange = 0;
	};

	/**
	 * @brief Patrol route followed by enemies.
	 */
	struct Path {
		Vec2 points[8] = {};
		int pointCount = 0;
		int currentPoint = 0;
		bool loop = true;
	};

	/**
	 * @brief Marks an entity the player can interact with.
	 */
	struct Interactable {
		bool canInteract = false;
		int interactionType = -1;
	};

	/**
	 * @brief Marks an entity that can be picked up and thrown.
	 */
	struct Throwable {
		bool canBeThrown = true;
		float throwForce = 0;
	};

	/**
	 * @brief Sound reference. Sound is exempt from the assignment, kept for parity.
	 */
	struct Sound {
		const char* soundName = nullptr;
		int soundId = -1;
	};

	/**
	 * @brief Combo counter. Kept for future polish; not used in the current scope.
	 */
	struct Combo {
		int hitCount = 0;
		float timeLeft = 0;
	};

	/**
	 * @brief Current level number (lives on a single level-tracker entity).
	 */
	struct Level {
		int number = -1;
	};

	/**
	 * @brief Per-player gameplay statistics.
	 */
	struct Stats {
		int score = 0;
		int enemiesDefeated = 0;
		int objectsThrown = 0;
	};

	/**
	 * @brief Generic lifetime timer (frames). Entity is destroyed at zero.
	 */
	struct Lifetime {
		int framesLeft = 0;
	};

	/* ------------------------------------------------------------------ */
	/*  New components for Ex 3 gameplay                                  */
	/* ------------------------------------------------------------------ */

	/**
	 * @brief What an entity *wants* to do this frame.
	 *
	 * Filled by @ref InputSystem for the player and @ref AISystem for enemies.
	 * Consumed by @ref MovementSystem and @ref CombatSystem.
	 */
	struct Intent {
		bool up = false;
		bool down = false;
		bool left = false;
		bool right = false;
		bool punch = false;
	};

	/**
	 * @brief Keyboard scancodes that drive the player's @ref Intent.
	 *
	 * Stored as int to avoid leaking SDL types into the model header.
	 * @ref InputSystem casts to SDL_Scancode at use site.
	 */
	struct Keys {
		int up = 0;
		int down = 0;
		int left = 0;
		int right = 0;
		int punch = 0;
	};

	/**
	 * @brief Transient. Present only while the entity is mid-punch.
	 * Removed by @ref LifetimeSystem when @c framesLeft reaches zero.
	 */
	struct Punching {
		int framesLeft = 0;
	};

	/**
	 * @brief Transient. Present only while the entity is invulnerable after being hit.
	 * Removed by @ref LifetimeSystem when @c framesLeft reaches zero.
	 */
	struct IFrames {
		int framesLeft = 0;
	};

	/**
	 * @brief Transient. Present only on a short-lived "punch impact" flash entity.
	 * Entire entity is destroyed when @c framesLeft reaches zero.
	 */
	struct FlashEffect {
		int framesLeft = 0;
		bool enemyPunch = false;
	};

	/* ------------------------------------------------------------------ */
	/*  Tag components (no data, just a bit in the entity mask)           */
	/* ------------------------------------------------------------------ */

	/** @brief Marker: this entity reads keyboard input. */
	struct InputControlled {};
	/** @brief Marker: this entity is the player. */
	struct PlayerTag {};
	/** @brief Marker: this entity is an enemy. */
	struct EnemyTag {};
	/** @brief Marker: this enemy can punch back (used to differentiate Level 2 enemies). */
	struct SpecialEnemyTag {};
	/** @brief Marker: this entity is a static, non-moving object. */
	struct StaticObjectTag {};

	/* ------------------------------------------------------------------ */
	/*  Entity factory functions                                          */
	/* ------------------------------------------------------------------ */

	/**
	 * @brief Create the player entity.
	 * @param position Initial world position.
	 * @return The entity id of the newly created player.
	 */
	ent_type createPlayer(Vec2 position = {0, 0});

	/**
	 * @brief Create a normal (Level 1) enemy that patrols between waypoints.
	 * @param patrolPoints Ordered waypoint list. First point is the spawn point.
	 * @return The entity id.
	 */
	ent_type createNormalEnemy(std::initializer_list<Vec2> patrolPoints, const char* name);

	/**
	 * @brief Create a special (Level 2) enemy that patrols and can punch back.
	 * @param patrolPoints Ordered waypoint list. First point is the spawn point.
	 * @return The entity id.
	 */
	ent_type createSpecialEnemy(std::initializer_list<Vec2> patrolPoints, const char* name);

	/**
	 * @brief Create a static decoration entity (e.g. a tree, trash can).
	 * @param position Initial world position.
	 * @param spriteName Sprite key for the renderer.
	 * @param colliderSize AABB collider size in pixels.
	 * @param solid Whether the collider blocks movement.
	 * @return The entity id.
	 */
	ent_type createStaticObject(
		Vec2 position = {0, 0},
		const char* spriteName = nullptr,
		Vec2 colliderSize = {0, 0},
		bool solid = true);

	/**
	 * @brief Create a short-lived "punch impact" flash entity.
	 * @param position Where the flash is drawn.
	 * @return The entity id.
	 */
	ent_type createFlashEntity(Vec2 position = {0, 0}, bool enemyPunch = false);

	/* ------------------------------------------------------------------ */
	/*  Systems                                                           */
	/* ------------------------------------------------------------------ */

	/** @brief Reads the keyboard and writes player @ref Intent. */
	struct InputSystem { static void update(); };

	/** @brief Decides enemy behavior and writes enemy @ref Intent. */
	struct AISystem { static void update(); };

	/** @brief Translates @ref Intent into Box2D velocity on the entity's body. */
	struct MovementSystem { static void update(); };

	/** @brief Steps the Box2D world and copies physics transforms back to @ref Transform. */
	struct PhysicsSystem { static void update(); };

	/** @brief Detects punch overlaps and applies damage + spawns flash effects. */
	struct CombatSystem { static void update(); };

	/** @brief Decrements transient timers (@ref Punching, @ref IFrames, @ref FlashEffect, @ref Lifetime). */
	struct LifetimeSystem { static void update(); };

	/** @brief Destroys entities whose health has reached zero. */
	struct CollisionSystem { static void update(); };

	/** @brief Checks for level-completion and advances to the next level. */
	struct LevelSystem { static void update(); };

	/** @brief Renders entities to the screen. */
	struct RenderSystem { static void update(); };

	/** @brief Draws the HUD (player + enemy health bars). */
	struct HudSystem { static void update(); };
}

/* ---------------------------------------------------------------------- */
/*  Storage specializations                                               */
/*  - Tagged storage for marker components (no data)                      */
/*  - Packed storage for components present on only a few entities        */
/*  - Sparse storage (default) for components most entities have          */
/* ---------------------------------------------------------------------- */

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

template <> struct bagel::Storage<me_and_dad::StaticObjectTag> final : NoInstance {
	using type = TaggedStorage<me_and_dad::StaticObjectTag>;
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

template <> struct bagel::Storage<me_and_dad::EnemyName> final : NoInstance {
	using type = PackedStorage<me_and_dad::EnemyName>;
};

template <> struct bagel::Storage<me_and_dad::Intent> final : NoInstance {
	using type = PackedStorage<me_and_dad::Intent>;
};

template <> struct bagel::Storage<me_and_dad::Keys> final : NoInstance {
	using type = PackedStorage<me_and_dad::Keys>;
};

template <> struct bagel::Storage<me_and_dad::Punching> final : NoInstance {
	using type = PackedStorage<me_and_dad::Punching>;
};

template <> struct bagel::Storage<me_and_dad::IFrames> final : NoInstance {
	using type = PackedStorage<me_and_dad::IFrames>;
};

template <> struct bagel::Storage<me_and_dad::FlashEffect> final : NoInstance {
	using type = PackedStorage<me_and_dad::FlashEffect>;
};

template <> struct bagel::Storage<me_and_dad::Lifetime> final : NoInstance {
	using type = PackedStorage<me_and_dad::Lifetime>;
};
