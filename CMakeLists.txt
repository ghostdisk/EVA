cmake_minimum_required(VERSION 3.30)

project(EVA)
add_executable (EVA
	"EVA/Main.cpp"
	"EVA/Utils.cpp"
	"EVA/GL.cpp"
	"EVA/GLTF.cpp"
	"EVA/IO.cpp"
	"EVA/Camera.cpp"
	"EVA/UI.cpp"
	"EVA/Draw.cpp"
	"EVA/Hashing.cpp"
	"EVA/Arena.cpp"
	"EVA/Renderer.cpp"
	"EVA/Game.cpp"
	"EVA/ClientGame.cpp"
	"EVA/ServerGame.cpp"
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

#
# FreeType
#

add_subdirectory("Vendor/freetype-2.14.3")
target_link_libraries(EVA freetype)


#
# ENet
#

add_subdirectory("Vendor/enet-1.3.18")
target_include_directories(EVA PRIVATE "Vendor/enet-1.3.18/include")
target_link_libraries(EVA enet "Ws2_32.lib")