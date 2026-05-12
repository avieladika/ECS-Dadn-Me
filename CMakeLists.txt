cmake_minimum_required(VERSION 3.27)
project(ECS_Dadn_Me)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ECS_Dadn_Me
        main.cpp
        bagel.h
        me_and_dad_model.h
        me_and_dad_model.cpp
        tests.cpp
)

target_compile_definitions(ECS_Dadn_Me PUBLIC
        ME_AND_DAD_RES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/res/"
)
