cmake_minimum_required(VERSION 3.30)

project(EVA)
add_executable (EVA
	"EVA/Main.cpp"
	"EVA/Utils.cpp"
	"EVA/GL.cpp"
	"EVA/GLTF.cpp"
)


set_property(TARGET EVA PROPERTY CXX_STANDARD 20)

target_include_directories(EVA PRIVATE "${CMAKE_SOURCE_DIR}")

#
# SDL
#

add_subdirectory("Vendor/SDL-release-3.4.10")
set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
target_link_libraries(EVA SDL3-static)

#
# glad
#

target_sources(EVA PRIVATE "Vendor/glad/src/glad.c")
target_include_directories(EVA PRIVATE "Vendor/glad/include")


#
# cglm
#

target_include_directories(EVA PRIVATE "Vendor/cglm-0.9.6/include")