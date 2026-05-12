cmake_minimum_required(VERSION 3.27)
project(ECS_Dadn_Me)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
        main.cpp
        bagel.h
        me_and_dad_model.h
        me_and_dad_model.cpp
        Game.h
        Game.cpp
        tests.cpp
)

# --- SDL3 (static) ---
set(SDL_STATIC ON)
set(SDL_SHARED OFF)
add_subdirectory(lib/SDL)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3-static)

# --- SDL3_image (static) ---
# Disable image formats we don't need to avoid heavyweight deps
# (dav1d needs NASM, libjxl needs many sub-submodules, etc.)
set(BUILD_SHARED_LIBS OFF)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL  OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF  OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
add_subdirectory(lib/SDL_image)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3_image-static)

# --- Box2D ---
set(BOX2D_SAMPLES OFF)
set(BOX2D_BENCHMARKS OFF)
set(BOX2D_DOCS OFF)
set(BOX2D_PROFILE OFF)
set(BOX2D_VALIDATE OFF)
set(BOX2D_UNIT_TESTS OFF)
add_subdirectory(lib/box2d)
target_link_libraries(${PROJECT_NAME} PUBLIC box2d)

# --- Copy res/ next to the executable after every build ---
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${PROJECT_SOURCE_DIR}/res"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res")
