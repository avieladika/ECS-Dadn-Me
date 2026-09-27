# Dad 'n Me — ECS Game Project

An academic C++ game project using an Entity Component System, SDL3 rendering, and Box2D physics integration.

## Highlights

- Components for transforms, movement, rendering, health, collision, and combat state.
- Systems and entity factories separated from the application loop.
- SDL3 and SDL3_image for the graphical application.
- Box2D integration for physics bodies.
- Documented adaptations of the course-provided Bagel ECS framework.

## Build

Requires CMake 3.27+, a C++20 toolchain, and the platform prerequisites for the bundled SDL dependencies.

```sh
cmake -S . -B build
cmake --build build --config Release
```

Run the resulting `ECS_Dadn_Me` executable from its output directory. CMake copies `res/` next to the executable; keep those assets alongside it. Executable locations vary by generator, for example `build/` or `build/Release/`.

## Architecture

- `Game.cpp` / `Game.h`: application loop and gameplay integration.
- `me_and_dad_model.*`: components, systems, and entity factories.
- `bagel.h`: course-provided ECS framework with local adaptations.
- `res/`: game assets.
- `lib/`: bundled third-party libraries.

See [BAGEL_CHANGES.md](BAGEL_CHANGES.md) for the portability, component registration, and cleanup changes.

## Credits and scope

Collaborative academic project, building on the [Bagel course framework](https://github.com/moshesu/bagel26), SDL, SDL_image, and Box2D. Third-party code retains its own notices and licenses. The game is a learning project; some model components are reserved for future gameplay.
