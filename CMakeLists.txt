cmake_minimum_required(VERSION 3.20)
project(EVA LANGUAGES CXX)

# Instruments everything for fuzzing (see EVA/Script/Fuzz/README.md): coverage for libFuzzer, AddressSanitizer and
# UndefinedBehaviorSanitizer. Clang only. Set before any target is added, so all of them get the flags.
option(EVA_FUZZ "Build the fuzz targets, with every library instrumented" OFF)
if(EVA_FUZZ)
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		message(FATAL_ERROR "EVA_FUZZ needs clang")
	endif()
	# -UNDEBUG keeps asserts on, so each is another check the fuzzers can trip.
	add_compile_options(-fsanitize=address,undefined,fuzzer-no-link -fno-sanitize-recover=all -fno-omit-frame-pointer -UNDEBUG)
	add_link_options(-fsanitize=address,undefined)
	add_compile_definitions(EVA_FUZZ EVA_ARENA_REDZONE=16)
	if(WIN32)
		# LLVM's prebuilt libFuzzer comes with an older MSVC STL, and where the linker picks its copy of an STL function
		# the container annotations disagree with ours, giving false container-overflow reports. Overflows past an
		# allocation are still caught without them.
		add_compile_definitions(_DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION)
	endif()
endif()

# Instruments everything for source-based coverage, for the CoverageReport target. Clang only.
option(EVA_COVERAGE "Build with coverage instrumentation" OFF)
if(EVA_COVERAGE)
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		message(FATAL_ERROR "EVA_COVERAGE needs clang")
	endif()
	add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
	add_link_options(-fprofile-instr-generate)
endif()

option(EVA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

function(eva_init_target target)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        # Designated initializers leaving out fields with defaults are intended, hence no -Wmissing-field-initializers.
        target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:
            -Wall -Wextra -Wshadow -Wimplicit-fallthrough -Wsign-compare -Wshorten-64-to-32
            -Wno-missing-field-initializers $<$<BOOL:${EVA_WARNINGS_AS_ERRORS}>:-Werror>>)
    endif()
endfunction()

add_library(Core STATIC
	EVA/Core/Common.hpp
	EVA/Core/Arena.hpp
	EVA/Core/Arena.cpp
	EVA/Core/Atom.hpp
	EVA/Core/Atom.cpp
	EVA/Core/Error.hpp
	EVA/Core/Panic.hpp
	EVA/Core/Panic.cpp
	EVA/Core/StringBuilder.hpp
	EVA/Core/StringBuilder.cpp
	EVA/Core/VirtualMemory.hpp
	EVA/Core/VirtualMemory.cpp
)
eva_init_target(Core)

if(WIN32)
	target_compile_definitions(Core PUBLIC EVA_WIN32)
	target_link_libraries(Core PRIVATE advapi32) # large page privilege
endif()

if(ANDROID)
	target_compile_definitions(Core PUBLIC EVA_ANDROID)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	enable_language(OBJCXX)
	target_compile_definitions(Core PUBLIC EVA_MACOS)
endif()

add_library(PAL STATIC
	EVA/PAL/PAL.hpp
	EVA/PAL/PAL.cpp
)
eva_init_target(PAL)
target_link_libraries(PAL PUBLIC Core)

if(WIN32)
	target_sources(PAL PRIVATE EVA/PAL/PAL_Win32.cpp)
	target_link_libraries(PAL PRIVATE user32)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_sources(PAL PRIVATE EVA/PAL/PAL_MacOS.mm)
	set_target_properties(PAL PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
	set_source_files_properties(EVA/PAL/PAL_MacOS.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
	find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
	target_link_libraries(PAL PRIVATE ${APPKIT_FRAMEWORK})
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

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_sources(GPU PRIVATE EVA/GPU/GPU_Metal.hpp EVA/GPU/GPU_Metal.cpp)
	target_compile_definitions(GPU PUBLIC EVA_METAL)
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

add_library(Script STATIC
	EVA/Script/Script.hpp
	EVA/Script/Script.cpp
	EVA/Script/Script_Dump.cpp
	EVA/Script/Script_Lexer.cpp
	EVA/Script/Script_Parser.cpp
	EVA/Script/Script_Resolver.cpp
	EVA/Script/Script_EvaluateConstant.cpp
	EVA/Script/Script_Typer.cpp
)
eva_init_target(Script)
target_link_libraries(Script PUBLIC Core)

if(ANDROID)
	find_package(game-activity REQUIRED CONFIG)
	add_library(eva SHARED EVA/PAL/PAL_Android.cpp EVA/TestApp/Main.cpp)
	eva_init_target(eva)
	target_link_libraries(eva PRIVATE GPU Script game-activity::game-activity_static android log)
	target_link_options(eva PRIVATE "LINKER:-u,Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")
else()
	add_executable(TestApp EVA/PAL/PAL_Desktop.cpp EVA/TestApp/Main.cpp)
	eva_init_target(TestApp)
	target_link_libraries(TestApp PRIVATE GPU Script)
	if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		set_target_properties(TestApp PROPERTIES
			MACOSX_BUNDLE TRUE
			MACOSX_BUNDLE_GUI_IDENTIFIER "com.ghostdisk.eva.testapp"
			MACOSX_BUNDLE_BUNDLE_NAME "EVA Test App"
			MACOSX_BUNDLE_BUNDLE_VERSION "1"
			MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
		)
	endif()
endif()

option(EVA_BUILD_TESTS "Build the test executables" ON)

if(EVA_BUILD_TESTS AND NOT ANDROID)
	enable_testing()

	add_library(Test STATIC
		EVA/Test/Test.hpp
		EVA/Test/Test.cpp
	)
	eva_init_target(Test)
	target_link_libraries(Test PUBLIC Core)

	# Builds EVA/<module>/Test/Test_*.cpp into a <module>Tests executable linked against the module, registered with CTest.
	# Test files are compiled into the executable rather than a static library, which would drop their registrations.
	function(eva_add_tests module)
		file(GLOB test_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/EVA/${module}/Test/Test_*.cpp")
		add_executable(${module}Tests EVA/Test/TestMain.cpp ${test_sources})
		eva_init_target(${module}Tests)
		target_link_libraries(${module}Tests PRIVATE Test ${module})
		add_test(NAME ${module} COMMAND ${module}Tests)
	endfunction()

	eva_add_tests(Core)
	eva_add_tests(Script)
endif()

# Fuzz targets, EVA/Script/Fuzz/Fuzz_<name>.cpp, each defining LLVMFuzzerTestOneInput (see EVA/Script/Fuzz/README.md).
# With EVA_FUZZ they're linked with libFuzzer. Otherwise with ReplayMain.cpp, which runs them over files, so the seeds
# and regressions are CTest tests in every build.
if(EVA_FUZZ OR (EVA_BUILD_TESTS AND NOT ANDROID))
	set(fuzz_dir "${PROJECT_SOURCE_DIR}/EVA/Script/Fuzz")
	add_library(ScriptFuzz STATIC ${fuzz_dir}/FuzzCommon.hpp ${fuzz_dir}/FuzzCommon.cpp)
	eva_init_target(ScriptFuzz)
	target_link_libraries(ScriptFuzz PUBLIC Script)

	if(EVA_FUZZ AND WIN32)
		execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir OUTPUT_VARIABLE clang_resource_dir
			OUTPUT_STRIP_TRAILING_WHITESPACE)
		set(asan_dll "${clang_resource_dir}/lib/windows/clang_rt.asan_dynamic-x86_64.dll")
	endif()

	function(eva_add_fuzzer name)
		if(EVA_FUZZ)
			add_executable(Fuzz${name} ${fuzz_dir}/Fuzz_${name}.cpp)
			target_link_options(Fuzz${name} PRIVATE -fsanitize=fuzzer)
			if(asan_dll)
				# The ASan runtime is a DLL on Windows, needed next to the executable.
				add_custom_command(TARGET Fuzz${name} POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_if_different ${asan_dll} $<TARGET_FILE_DIR:Fuzz${name}>)
			endif()
		else()
			add_executable(Fuzz${name} ${fuzz_dir}/Fuzz_${name}.cpp ${fuzz_dir}/ReplayMain.cpp)
			set(inputs)
			foreach(inputs_dir Seeds Regressions)
				if(EXISTS "${fuzz_dir}/${inputs_dir}/${name}")
					list(APPEND inputs "${fuzz_dir}/${inputs_dir}/${name}")
				endif()
			endforeach()
			if(inputs)
				add_test(NAME Fuzz${name} COMMAND Fuzz${name} ${inputs})
			endif()
		endif()
		eva_init_target(Fuzz${name})
		if(WIN32)
			target_compile_definitions(Fuzz${name} PRIVATE _CRT_SECURE_NO_WARNINGS) # fopen, getenv
		endif()
		target_link_libraries(Fuzz${name} PRIVATE ScriptFuzz)
	endfunction()

	eva_add_fuzzer(Script)
	eva_add_fuzzer(ScriptGrammar)

	# Replays the seeds, regressions and fuzzing corpora through the fuzz targets and reports which code they reach:
	# a summary per file, and an HTML report in coverage/html.
	if(EVA_COVERAGE AND NOT EVA_FUZZ)
		set(EVA_FUZZ_CORPUS "${PROJECT_SOURCE_DIR}/Build/Fuzz/Corpus" CACHE PATH "The corpora to measure coverage of")
		get_filename_component(llvm_dir ${CMAKE_CXX_COMPILER} DIRECTORY)
		find_program(LLVM_PROFDATA llvm-profdata HINTS ${llvm_dir} REQUIRED)
		find_program(LLVM_COV llvm-cov HINTS ${llvm_dir} REQUIRED)

		set(coverage_dir "${CMAKE_BINARY_DIR}/coverage")
		set(coverage_commands)
		foreach(name Script ScriptGrammar)
			set(inputs "${EVA_FUZZ_CORPUS}/${name}")
			foreach(inputs_dir Seeds Regressions)
				if(EXISTS "${fuzz_dir}/${inputs_dir}/${name}")
					list(APPEND inputs "${fuzz_dir}/${inputs_dir}/${name}")
				endif()
			endforeach()
			list(APPEND coverage_commands
				COMMAND ${CMAKE_COMMAND} -E make_directory "${EVA_FUZZ_CORPUS}/${name}"
				COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${coverage_dir}/${name}.profraw" $<TARGET_FILE:Fuzz${name}> --quiet ${inputs})
		endforeach()

		# Only the code under test, not the fuzz harness, tests or libraries.
		set(coverage_filter "${PROJECT_SOURCE_DIR}/EVA/Core" "${PROJECT_SOURCE_DIR}/EVA/Script" "-ignore-filename-regex=[/\\\\](Fuzz|Test)[/\\\\]")
		add_custom_target(CoverageReport
			COMMAND ${CMAKE_COMMAND} -E rm -rf "${coverage_dir}"
			${coverage_commands}
			COMMAND ${LLVM_PROFDATA} merge -sparse "${coverage_dir}/Script.profraw" "${coverage_dir}/ScriptGrammar.profraw" -o "${coverage_dir}/merged.profdata"
			COMMAND ${LLVM_COV} report $<TARGET_FILE:FuzzScript> -object $<TARGET_FILE:FuzzScriptGrammar> "-instr-profile=${coverage_dir}/merged.profdata" ${coverage_filter}
			COMMAND ${LLVM_COV} show $<TARGET_FILE:FuzzScript> -object $<TARGET_FILE:FuzzScriptGrammar> "-instr-profile=${coverage_dir}/merged.profdata" -format=html "-output-dir=${coverage_dir}/html" -show-line-counts-or-regions ${coverage_filter}
			COMMAND ${CMAKE_COMMAND} -E echo "Report: ${coverage_dir}/html/index.html"
			DEPENDS FuzzScript FuzzScriptGrammar
			USES_TERMINAL
			VERBATIM)
	endif()
endif()

# clang-tidy over our sources with the checks in .clang-tidy. A step per file, so they run in parallel, rerun when the
# file, any header or the checks change.
get_filename_component(llvm_dir ${CMAKE_CXX_COMPILER} DIRECTORY)
find_program(CLANG_TIDY clang-tidy HINTS ${llvm_dir})
if(CLANG_TIDY AND CMAKE_EXPORT_COMPILE_COMMANDS)
	get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
	file(GLOB_RECURSE tidy_headers CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/EVA/*.hpp")
	set(tidy_files)
	set(tidy_stamps)
	foreach(target ${targets})
		get_target_property(sources ${target} SOURCES)
		if(NOT sources)
			continue()
		endif()
		foreach(source ${sources})
			get_filename_component(path ${source} ABSOLUTE BASE_DIR ${PROJECT_SOURCE_DIR})
			if(NOT path MATCHES "\.cpp$" OR path MATCHES "/Vendor/" OR path IN_LIST tidy_files)
				continue()
			endif()
			list(APPEND tidy_files ${path})
			file(RELATIVE_PATH name ${PROJECT_SOURCE_DIR} ${path})
			set(stamp "${CMAKE_BINARY_DIR}/tidy/${name}.stamp")
			add_custom_command(OUTPUT ${stamp}
				COMMAND ${CLANG_TIDY} -p ${CMAKE_BINARY_DIR} --quiet ${path}
				COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
				DEPENDS ${path} ${tidy_headers} "${PROJECT_SOURCE_DIR}/.clang-tidy"
				COMMENT "clang-tidy ${name}"
				VERBATIM)
			list(APPEND tidy_stamps ${stamp})
		endforeach()
	endforeach()
	add_custom_target(Tidy DEPENDS ${tidy_stamps})
endif()
