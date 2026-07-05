cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0091 NEW) # allow MSVC CRT selection via flag

project(EVA)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(EVA
	"EVA/App.cpp"
	"EVA/Arena.cpp"
	"EVA/Assets/Asset.cpp"
	"EVA/Camera.cpp"
	"EVA/Console.cpp"
	"EVA/CSG.cpp"
	"EVA/Editor.cpp"
	"EVA/Entities.cpp"
	"EVA/ECharacter.cpp"
	"EVA/Font.cpp"
	"EVA/Game.cpp"
	"EVA/GameClient.cpp"
	"EVA/GameServer.cpp"
	"EVA/GLTF.cpp"
	"EVA/Hashing.cpp"
	"EVA/Input.cpp"
	"EVA/Library.cpp"
	"EVA/MainMenu.cpp"
	"EVA/Math.cpp"
	"EVA/Net.cpp"
	"EVA/Platform.cpp"
	"EVA/Renderer/GL.cpp"
	"EVA/Renderer/Renderer.cpp"
	"EVA/UI_Core.cpp"
	"EVA/UI_Utils.cpp"
	"EVA/UI_Widgets.cpp"
	"EVA/Utils.cpp"
	"EVA/EVA.natvis"
)


set_property(TARGET EVA PROPERTY CXX_STANDARD 20)

target_include_directories(EVA PRIVATE "${CMAKE_SOURCE_DIR}")

#
# SDL
#

 set(SDL3_MAINPROJECT OFF)

set(SDL_GPU          OFF CACHE BOOL "" FORCE)
set(SDL_RENDER       OFF CACHE BOOL "" FORCE)
set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
add_subdirectory("Vendor/SDL-release-3.4.10")
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

#
# Tracy
#
target_sources(EVA PRIVATE "Vendor/tracy-0.13.1/public/TracyClient.cpp")
target_include_directories(EVA PRIVATE "Vendor/tracy-0.13.1/public")
# add_definitions(-DTRACY_ENABLE)

#
# Box3D
#
add_subdirectory(Vendor/box3d)
target_link_libraries(EVA box3d)