# Fixes Applied to `bagel.h`

This document lists every modification made to the course-provided `bagel.h` so
that the engine compiles and runs correctly on **MSVC (Windows)** and across
**multiple translation units**. None of these are design changes — they're
portability and correctness fixes.

The original `bagel.h` was written assuming a single `.cpp` file using GCC/Clang
(as in the Pong example). Once the project grew to multiple `.cpp` files
compiled by MSVC, several latent issues surfaced.

---

## Fix 1 — Missing standard headers

**Symptom:** Compile errors:
- `error C2039: 'max': is not a member of 'std'`
- `error C3861: 'malloc': identifier not found`
- `'std::countr_zero' not found` (after Fix 3)

**Cause:** The original file commented out `<cstdlib>` and didn't include
`<algorithm>` or `<bit>`. GCC/Clang's standard library often pulls these in
transitively; MSVC does not.

**Change:** Added explicit includes at the top of the file.

```cpp
#include <algorithm>   // std::max
#include <bit>         // std::countr_zero
#include <cstdlib>     // malloc, free, realloc
#include <cstdint>
#include <type_traits>
```

---

## Fix 2 — `__attribute__((used))` is GCC/Clang only

**Symptom:** MSVC parse errors at every line that used the attribute:
- `error C2059: syntax error: '('`
- `error C4430: missing type specifier - int assumed`

**Cause:** `__attribute__((used))` is a GCC/Clang extension. MSVC doesn't
recognize it and tries to interpret the token sequence as a function
declaration, which fails.

**Change:** Wrapped the attribute in a cross-compiler `BAGEL_USED` macro that
expands to nothing on MSVC.

```cpp
#if defined(__GNUC__) || defined(__clang__)
#  define BAGEL_USED __attribute__((used))
#else
#  define BAGEL_USED
#endif
```

All four uses (`SparseStorage`, `TaggedStorage`, `PackedStorage`,
`StackStorage`) updated to use `BAGEL_USED` instead.

---

## Fix 3 — `__builtin_ctz` is GCC/Clang only

**Symptom:** `error C3861: '__builtin_ctz': identifier not found`

**Cause:** `__builtin_ctz` (count trailing zeros) is a GCC/Clang intrinsic.
MSVC has `_BitScanForward` but it's an entirely different API.

**Change:** Replaced with the C++20 standard library equivalent, which works
on every compiler.

```cpp
// Before
int ctz() const { return _mask ? __builtin_ctz(_mask) : -1; }

// After
int ctz() const { return _mask ? std::countr_zero(_mask) : -1; }
```

---

## Fix 4 — `World::delComponent` signature mismatch

**Symptom:** Calling `Entity::del<T>()` would not compile (the body
`World::delComponent<T>(_ent)` passes 1 argument, but the function required 2).

**Cause:** The original signature was inconsistent with how it was being
called and with the `Storage::del(ent)` method it forwards to:

```cpp
// Before — required two arguments but Entity::del<T>() and
//          PackedStorage::del() both work with just one
template <class T>
static void delComponent(ent_type ent, const T& comp) {
    _masks[ent.id].clear(Component<T>::Bit);
    Storage<T>::type::del(ent, comp);
}
```

**Change:** Removed the unused second parameter so that the signature matches
`Entity::del<T>()` and `Storage<T>::type::del(ent_type)`.

```cpp
template <class T>
static void delComponent(ent_type ent) {
    _masks[ent.id].clear(Component<T>::Bit);
    Storage<T>::type::del(ent);
}
```

This is required by `LifetimeSystem` to remove transient components such as
`Punching` and `IFrames` when their timer expires.

---

## Fix 5 — `MaxComponents = 6` is too small for this project

**Symptom:** Game crashed at runtime with access violations. Components past
index 7 had their bits truncated to 0 (because `1 << 8` doesn't fit in
`uint_fast8_t`), causing mask collisions and reads from unmapped storage
slots.

**Cause:** The original value of `6` matches the Pong example, which only
defines six component types. "Me and Dad" uses about 19 component types
(Transform, Renderable, Collider, PlayerTag, InputControlled, Velocity,
Health, Damage, Direction, State, Intent, Keys, EnemyTag, SpecialEnemyTag,
AI, FlashEffect, Punching, IFrames, StaticObjectTag).

**Change:** Increased the parameter to 32. This selects `uint_fast32_t` as
the mask type, giving 32 distinct component bits.

```cpp
// Before
constexpr int MaxComponents = 6;

// After
constexpr int MaxComponents = 32;
```

The `mask_type` deduction in `bagel.h` already supports values up to 64,
so this is a parameter tweak, not a structural change.

---

## Fix 6 — `compCounter` had internal (per-TU) linkage

**Symptom:** With the game spread across two source files (`Game.cpp` and
`me_and_dad_model.cpp`), different `Component<T>::Index` values collided.
For example, both `Component<Transform>` and `Component<Punching>` could end
up with index `0` and bit `1`, so the player's mask incorrectly tested
positive for `Punching` and crashed in `LifetimeSystem` when accessing
non-existent component data.

**Cause:** `static inline int compCounter = -1;` at namespace scope has
**internal linkage** — each translation unit gets its own copy starting at
`-1`. `inline` alone (without `static`) would give external linkage (one
shared copy across the program).

The Pong example never hit this because it uses bagel from a single `.cpp`
file, so only one counter existed.

**Change:** Removed `static` to give the variable external linkage.

```cpp
// Before — one counter per .cpp file
static inline int compCounter = -1;

// After — one counter shared across the whole program
inline int compCounter = -1;
```

This guarantees every distinct `Component<T>` gets a globally unique index.

---

## Summary

| # | Fix | File location |
|---|---|---|
| 1 | Added `<algorithm>`, `<bit>`, `<cstdlib>` includes | Top of file |
| 2 | `__attribute__((used))` → `BAGEL_USED` macro | 4 storage classes |
| 3 | `__builtin_ctz` → `std::countr_zero` | `Mask::ctz()` |
| 4 | `delComponent` takes one argument, not two | `World::delComponent` |
| 5 | `MaxComponents = 6` → `32` | Parameters block |
| 6 | `static inline int compCounter` → `inline int compCounter` | Namespace scope |

None of these changes modify the engine's behavior or API beyond what's
required to compile under MSVC and to work across multiple translation units.
