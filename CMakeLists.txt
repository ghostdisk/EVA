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

add_executable(TestApp
	EVA/TestApp/Main.cpp
)
eva_init_target(TestApp)

target_link_libraries(TestApp PRIVATE GPU)
