# Local divergences from upstream `bagel.h`

This document lists every line in our `bagel.h` that differs from upstream
([`bagel26@db3403f`](https://github.com/...) — 2026-05-16, "12/5 Lecture 7"),
and explains why.

The base of our file is the upstream verbatim. On top of that we apply five
small portability/correctness patches required to:
- compile under **MSVC** (the course author tests on GCC/Clang),
- work across **multiple translation units** (our project is split into
  `Game.cpp` and `me_and_dad_model.cpp`; the Pong example is a single `.cpp`),
- keep **`PackedStorage` cleanup** wired up under upstream's new
  `CallbackOnDelete` design.

> **Historical note.** Earlier versions of this document listed two additional
> fixes that have since been adopted upstream: `__builtin_ctz` → `std::countr_zero`
> in `Mask::ctz()`, and the single-argument `delComponent` signature. Both are
> now present in upstream and require no local patch.

---

## Fix 1 — Missing standard headers

**Symptom (MSVC):**
- `error C2039: 'max': is not a member of 'std'`
- `error C3861: 'malloc': identifier not found`
- `'std::countr_zero': identifier not found`

**Cause:** Upstream's `bagel.h` only includes `<cstdint>` and `<type_traits>`
(and leaves `<cstdlib>` commented out). GCC/Clang's standard library pulls the
rest in transitively; MSVC's stricter STL does not.

**Change:** Three explicit includes near the top of the file.

```cpp
#include <algorithm>   // std::max          (used by DynamicBag::ensure)
#include <bit>         // std::countr_zero  (used by Mask::ctz)
#include <cstdlib>     // malloc/free/realloc (used by DynamicBag)
```

---

## Fix 2 — `__attribute__((used))` is GCC/Clang-only

**Symptom (MSVC):** parse cascade at every storage class:
- `error C2059: syntax error: '('`
- `error C4430: missing type specifier - int assumed`

**Cause:** `__attribute__((used))` is a GCC/Clang extension. MSVC treats the
token sequence as a malformed declarator.

**Change:** A portable `BAGEL_USED` macro, used in place of the raw attribute
at the four `Register<T> _reg{...}` sites (`SparseStorage`, `TaggedStorage`,
`PackedStorage`, `StackStorage`).

```cpp
#if defined(__GNUC__) || defined(__clang__)
#  define BAGEL_USED __attribute__((used))
#else
#  define BAGEL_USED
#endif
```

> **Caveat:** On MSVC `BAGEL_USED` is empty — there is no portable equivalent
> of `__attribute__((used))`. That means we cannot guarantee the
> `Register<T> _reg{del}` static initializer fires on MSVC. See Fix 5 for the
> belt-and-suspenders workaround.

---

## Fix 3 — `MaxComponents = 32`

**Symptom:** Random crashes with access violations after about the 8th
component type was introduced. Components past index 7 had their bits
truncated to zero (because `1 << 8` doesn't fit in `uint_fast8_t`), causing
mask collisions and reads from unmapped storage slots.

**Cause:** Upstream's default `MaxComponents = 6` matches the Pong example
(6 component types). Our project defines about 19 (Transform, Renderable,
Collider, PlayerTag, InputControlled, Velocity, Health, Damage, Direction,
State, Intent, Keys, EnemyTag, SpecialEnemyTag, AI, FlashEffect, Punching,
IFrames, StaticObjectTag, Lifetime, EnemyName, …).

**Change:** Bump the parameter to 32. The existing `conditional_t` ladder
automatically promotes `mask_type` to `uint_fast32_t`.

```cpp
constexpr int MaxComponents = 32;   // was 6
```

---

## Fix 4 — `compCounter` needs external linkage

**Symptom:** With the game spread across two source files, different
`Component<T>::Index` values collided across translation units. For example,
both `Component<Transform>` and `Component<Punching>` could end up with
index 0 in their respective TUs. The player's mask would then test positive
for `Punching`, and `LifetimeSystem` would crash reading non-existent
`Punching` data.

**Cause:** `static inline int compCounter = -1;` at namespace scope has
**internal linkage** — each TU gets its own private copy starting at -1.
The `inline` keyword alone gives **external linkage** (one shared copy
across the whole program), which is what we need.

The Pong example never hit this because it uses bagel from a single `.cpp`
file.

**Change:** Drop the `static`.

```cpp
inline int compCounter = -1;        // was: static inline int compCounter = -1;
```

---

## Fix 5 — `CallbackOnDelete = true` *(new in this revision)*

**Symptom:** Without this flag, every call to `Entity::destroy()` would skip
`PackedStorage::del`, leaking the dense-array slot. The `_comps`, `_compToId`,
and `_idToComp` bags would grow forever, and after enough enemy spawns/kills
the storage would overflow or corrupt.

**Cause:** Upstream's revised `World::deleteEntity` gates the
component-deleter loop behind `if constexpr (CallbackOnDelete)`, and the
default upstream value is `false`. That's fine for the Pong example (which
uses `SparseStorage` only — `SparseStorage::del` is a no-op). It's not fine
for our project: `me_and_dad_model.h:398-448` specializes 12 components to
`PackedStorage` (Transform, Velocity, Renderable, Collider, Direction, State,
EnemyName, Intent, Keys, Punching, IFrames, FlashEffect, Lifetime), and
`PackedStorage::del` is the function that performs the swap-and-pop cleanup.

`Entity::destroy()` is called in three places in our game:

| Call site | When it fires |
|---|---|
| `Game.cpp:266` | Bulk cleanup of all entities matching a given mask |
| `Game.cpp:612` | A `FlashEffect` timer reaches zero |
| `Game.cpp:647` | An entity is flagged dead and removed |

**Change:** Flip the parameter to `true`.

```cpp
constexpr bool CallbackOnDelete = true;   // was false
```

**Companion change (in `World::addComponent`):** Upstream further simplified
`addComponent` to assume the `BAGEL_USED Register<T> _reg{del}` static
initializer is what registers the deleter. As noted under Fix 2, that
initializer is unreliable on MSVC. We keep the *explicit* call to
`registerDeleter<T>(Storage<T>::type::del)` at the top of `addComponent`,
which guarantees the deleter ends up in the table the first time we
`add<T>` any component of that type — regardless of compiler.

```cpp
template <class T>
static void addComponent(ent_type ent, const T& comp) {
    registerDeleter<T>(Storage<T>::type::del);    // local: MSVC safety net
    _masks[ent.id].set(Component<T>::Bit);
    Storage<T>::type::add(ent, comp);
}
```

---

## Summary

| # | Fix | File location | Reason |
|---|---|---|---|
| 1 | Add `<algorithm>`, `<bit>`, `<cstdlib>` | Top of file | MSVC stricter STL |
| 2 | `__attribute__((used))` → `BAGEL_USED` | 4 storage classes | MSVC parse |
| 3 | `MaxComponents = 32` (was 6) | Parameters block | 19+ component types |
| 4 | `inline int compCounter` (drop `static`) | Namespace scope | Cross-TU linkage |
| 5 | `CallbackOnDelete = true` (was false) | Parameters block | PackedStorage cleanup |
|   | + explicit `registerDeleter` in `addComponent` | `World` body | MSVC safety net |

None of these change the engine's API or behavior beyond what's strictly
required for our compiler and our project structure. The shape of the
public interface (`Entity::create/destroy/get/add/del/has/test`,
`MaskBuilder`, `Storage<T>` specialization, `ent_type`) is identical to
upstream.

## Upstream improvements adopted in this sync

For reference, the merge also pulled in these upstream improvements (no
local changes required — listed for context):

- New sizing constants `IdBagSize`, `InitialEntities`, `InitialPackedSize`
  replacing hardcoded `10` / `100` / `50` literals.
- `World::_deleters` moved into a function-local static for safer C++
  static-initialization order.
- `World::deleteEntity` loop gated behind `if constexpr (CallbackOnDelete)`
  (with `CallbackOnDelete = true` it compiles in for us).
- `PackedStorage::del` simplified — drops the `if (idx != lastIdx)` guard
  in favour of an unconditional swap-with-last.
- Doxygen comments on `World`, `createEntity`, `deleteEntity`.
- `public NoInstance` inheritance and `World` field-before-method layout
  (cosmetic).
