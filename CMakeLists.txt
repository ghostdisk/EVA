cmake_minimum_required(VERSION 3.20)
project(EVA LANGUAGES CXX)

function(eva_init_target target)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}")
endfunction()

add_library(Core STATIC
	EVA/Core/Common.hpp
	EVA/Core/Common.cpp
)
eva_init_target(Core)

if(WIN32)
	target_compile_definitions(Core PUBLIC EVA_WIN32)
endif()

if(ANDROID)
	target_compile_definitions(Core PUBLIC EVA_ANDROID)
endif()

add_library(OS STATIC
	EVA/OS/OS.hpp
	EVA/OS/OS.cpp
)
eva_init_target(OS)
target_link_libraries(OS PUBLIC Core)

add_library(PAL STATIC
	EVA/PAL/PAL.hpp
	EVA/PAL/PAL.cpp
)
eva_init_target(PAL)
target_link_libraries(PAL PUBLIC OS)

if(WIN32)
	target_sources(PAL PRIVATE EVA/PAL/PAL_Win32.cpp)
	target_link_libraries(PAL PRIVATE user32)
endif()

add_library(GPU STATIC
	EVA/GPU/GPU.hpp
	EVA/GPU/GPU.cpp
)

eva_init_target(GPU)
target_link_libraries(GPU PUBLIC PAL)

if(WIN32)
	target_sources(GPU PRIVATE EVA/GPU/GPU_D3D11.cpp)
	target_link_libraries(GPU PRIVATE d3d11 dxgi)
endif()

find_package(Vulkan QUIET)
if(Vulkan_FOUND)
	enable_language(C)
	target_sources(GPU PRIVATE
		EVA/GPU/GPU_Vulkan.hpp
		EVA/GPU/GPU_Vulkan.cpp
		Vendor/volk/volk.c
	)
	target_include_directories(GPU PRIVATE "${PROJECT_SOURCE_DIR}/Vendor/volk")
	if(TARGET Vulkan::Headers)
		target_link_libraries(GPU PRIVATE Vulkan::Headers)
	else()
		target_include_directories(GPU PRIVATE ${Vulkan_INCLUDE_DIRS})
	endif()
	target_compile_definitions(GPU PUBLIC EVA_VULKAN)
	if(WIN32)
		target_compile_definitions(GPU PRIVATE VK_USE_PLATFORM_WIN32_KHR)
	endif()
	if(ANDROID)
		target_compile_definitions(GPU PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
	endif()
endif()

if(ANDROID)
	find_package(game-activity REQUIRED CONFIG)
	add_library(eva SHARED EVA/PAL/PAL_Android.cpp EVA/TestApp/Main.cpp)
	eva_init_target(eva)
	target_link_libraries(eva PRIVATE GPU game-activity::game-activity_static android log)
	target_link_options(eva PRIVATE "LINKER:-u,Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")
else()
	add_executable(TestApp EVA/PAL/PAL_Desktop.cpp EVA/TestApp/Main.cpp)
	eva_init_target(TestApp)
	target_link_libraries(TestApp PRIVATE GPU)
endif()
