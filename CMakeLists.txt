cmake_minimum_required(VERSION 3.30)
cmake_policy(SET CMP0091 NEW) # allow MSVC CRT selection via flag

project(EVA)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(EVA
	"EVA/App.cpp"
	"EVA/Core/Arena.cpp"
	"EVA/Core/Result.cpp"
	"EVA/Core/Serialization.cpp"
	"EVA/Core/String.cpp"
	"EVA/Core/Utils.cpp"
	"EVA/Core/Allocator.cpp"
	"EVA/Core/Base64.cpp"
	"EVA/Assets/Asset.cpp"
	"EVA/Assets/Shader.cpp"
	"EVA/Assets/Sprite.cpp"
	"EVA/Assets/Texture.cpp"
	"EVA/Assets/Model.cpp"
	"EVA/Assets/GLTF.cpp"
	"EVA/Assets/MeshAsset.cpp"
	"EVA/Console.cpp"
	"EVA/CSG.cpp"
	"EVA/Core/FS.cpp"
	"EVA/Core/Win32.cpp"
	"EVA/Core/Hashing.cpp"
	"EVA/Editor/Editor.cpp"
	"EVA/Editor/SelectTool.cpp"
	"EVA/Editor/EntityTool.cpp"
	"EVA/Editor/BrushTool.cpp"
	"EVA/GameModes/EditorGameMode.cpp"
	"EVA/Entities/Entities.cpp"
	"EVA/Entities/ECharacter.cpp"
	"EVA/Entities/ECamera.cpp"
	"EVA/Font.cpp"
	"EVA/Game.cpp"
	"EVA/GameModes/GameMode.cpp"
	"EVA/GameModes/BasePlayableGameMode.cpp"
	"EVA/GameClient.cpp"
	"EVA/GameServer.cpp"
	"EVA/Input.cpp"
	"EVA/Library.cpp"
	"EVA/Math.cpp"
	"EVA/Net.cpp"
	"EVA/Platform.cpp"
	"EVA/GFX/GPUDevice.cpp"
	"EVA/GFX/Mesh.cpp"
	"EVA/GFX/GPUDevice_Vulkan.cpp"
	"EVA/GFX/Material.cpp"
	"EVA/GFX/Renderer.cpp"
	"Vendor/volk/volk.c"
	"EVA/UI_Core.cpp"
	"EVA/UI_Utils.cpp"
	"EVA/UI_Widgets.cpp"
	"EVA/EVA.gen.cpp"
	"EVA/Async/JobSystem.cpp"
	"EVA/EVA.natvis"
)

set_property(TARGET EVA PROPERTY CXX_STANDARD 20)
target_include_directories(EVA PRIVATE "${CMAKE_SOURCE_DIR}")

find_package(Vulkan REQUIRED)
target_link_libraries(EVA Vulkan::Headers)
target_include_directories(EVA PRIVATE "Vendor/volk")

if (NOT MSVC)
	target_compile_options(EVA PRIVATE
		-Wno-nullability-completeness
		-Wno-deprecated-declarations
		-Wno-deprecated-enum-compare
	)
endif()

target_compile_definitions(EVA PRIVATE "-D_CRT_SECURE_NO_WARNINGS")

#
# SDL
#
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
add_definitions(-DTRACY_ENABLE)

#
# Box3D
#
add_subdirectory(Vendor/box3d)
target_link_libraries(EVA box3d)

#
# EVAGEN — reflection code generator
#
add_custom_target(EVAGEN_generate ALL
	COMMAND "${CMAKE_SOURCE_DIR}/EVAGEN/evagen.exe"
	BYPRODUCTS "${CMAKE_SOURCE_DIR}/EVA/EVA.gen.cpp"
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	COMMENT "EVAGEN: generating reflection data"
	VERBATIM
)
add_dependencies(EVA EVAGEN_generate)
