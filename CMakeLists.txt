cmake_minimum_required(VERSION 3.24)
project(omp_desktop VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Configurable paths to sibling Etch and Inlay repositories (falls back to GitHub FetchContent in CI/standalone builds)
set(OMPE_ETCH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Etch" CACHE PATH "Path to Etch source directory")
set(OMPE_INLAY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Inlay" CACHE PATH "Path to Inlay source directory")
set(OMPE_ETCH_GIT_TAG "main" CACHE STRING "Etch Git ref when using FetchContent")
set(OMPE_INLAY_GIT_TAG "main" CACHE STRING "Inlay Git ref when using FetchContent")
set(OMPE_GITHUB_REPO "senran-N/OMP-E" CACHE STRING "GitHub owner/repo for release update checks")
option(OMPE_BUILD_TESTS "Build OMP-E unit and headless visual tests" ON)
option(OMPE_COMPILE_SHADERS "Compile custom HLSL surface shaders via DXC/shadercross" ON)
option(OMPE_RELEASE_BUILD "Omit developer source-tree asset fallback paths in release binaries" OFF)

# Suppress subproject examples/tests/install so configure and build stay lean
set(ETCH_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ETCH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ETCH_INSTALL OFF CACHE BOOL "" FORCE)
set(INLAY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(INLAY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INLAY_INSTALL OFF CACHE BOOL "" FORCE)
set(INLAY_ENABLE_SCRIPT ON CACHE BOOL "" FORCE)

include(FetchContent)

if(NOT TARGET Etch::etch)
    if(EXISTS "${OMPE_ETCH_DIR}/CMakeLists.txt")
        add_subdirectory("${OMPE_ETCH_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/etch_build" EXCLUDE_FROM_ALL)
    else()
        message(STATUS "Sibling Etch not found at '${OMPE_ETCH_DIR}'; fetching senran-N/Etch (${OMPE_ETCH_GIT_TAG})...")
        FetchContent_Declare(
            etch
            GIT_REPOSITORY https://github.com/senran-N/Etch.git
            GIT_TAG "${OMPE_ETCH_GIT_TAG}"
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(etch)
        set(OMPE_ETCH_DIR "${etch_SOURCE_DIR}")
    endif()
endif()

if(NOT TARGET Inlay::inlay)
    if(EXISTS "${OMPE_INLAY_DIR}/CMakeLists.txt")
        add_subdirectory("${OMPE_INLAY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/inlay_build" EXCLUDE_FROM_ALL)
    else()
        message(STATUS "Sibling Inlay not found at '${OMPE_INLAY_DIR}'; fetching senran-N/Inlay (${OMPE_INLAY_GIT_TAG})...")
        FetchContent_Declare(
            inlay
            GIT_REPOSITORY https://github.com/senran-N/Inlay.git
            GIT_TAG "${OMPE_INLAY_GIT_TAG}"
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(inlay)
        set(OMPE_INLAY_DIR "${inlay_SOURCE_DIR}")
    endif()
endif()

# Include Etch shader compiler helper
if(EXISTS "${OMPE_ETCH_DIR}/cmake/EtchShaderTools.cmake")
    include("${OMPE_ETCH_DIR}/cmake/EtchShaderTools.cmake")
endif()

# Core logic library (headless-testable without GPU)
set(OMPE_CORE_SOURCES
    src/core/json.cpp
    src/core/base64.cpp
    src/core/ansi.cpp
    src/core/i18n.cpp
    src/core/platform.cpp
    src/core/updater.cpp
    src/omp/protocol.cpp
    src/omp/client.cpp
    src/omp/config_store.cpp
    src/omp/mock_replay.cpp
)

if(WIN32)
    list(APPEND OMPE_CORE_SOURCES src/core/process_win.cpp)
else()
    list(APPEND OMPE_CORE_SOURCES src/core/process_posix.cpp)
endif()

add_library(ompe_core STATIC ${OMPE_CORE_SOURCES})
target_include_directories(ompe_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ompe_core PUBLIC Inlay::inlay)
target_compile_definitions(ompe_core PUBLIC
    OMPE_VERSION_STRING="${PROJECT_VERSION}"
    OMPE_GITHUB_REPO="${OMPE_GITHUB_REPO}"
)

if(MSVC)
    target_compile_options(ompe_core PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_compile_definitions(ompe_core PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(ompe_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(ompe_core PRIVATE shell32 ole32 winhttp bcrypt)
elseif(APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(ompe_core PRIVATE Threads::Threads "-framework Foundation" "-framework AppKit")
else()
    find_package(Threads REQUIRED)
    target_link_libraries(ompe_core PRIVATE Threads::Threads)
endif()

# Desktop application executable
set(OMPE_APP_SOURCES
    src/main.cpp
    src/theme/palette.cpp
    src/theme/icons.cpp
    src/theme/particles.cpp
    src/ui/app.cpp
    src/ui/titlebar.cpp
    src/ui/sidebar.cpp
    src/ui/chat_view.cpp
    src/ui/composer.cpp
    src/ui/overlays.cpp
    src/ui/inspector.cpp
    src/ui/settings_view.cpp
    src/ui/about_modal.cpp
)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/omp_desktop.rc")
    list(APPEND OMPE_APP_SOURCES resources/omp_desktop.rc)
endif()

add_executable(omp_desktop ${OMPE_APP_SOURCES})
target_link_libraries(omp_desktop PRIVATE ompe_core Inlay::inlay)
target_include_directories(omp_desktop PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(NOT OMPE_RELEASE_BUILD)
    target_compile_definitions(omp_desktop PRIVATE OMPE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
endif()

if(WIN32)
    set_target_properties(omp_desktop PROPERTIES WIN32_EXECUTABLE ON)
    if(MSVC)
        target_link_options(omp_desktop PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()

if(MSVC)
    target_compile_options(omp_desktop PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_compile_definitions(omp_desktop PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(omp_desktop PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Compile bespoke ambient architectural shader (DXIL + SPIR-V + MSL)
if(OMPE_COMPILE_SHADERS AND COMMAND etch_compile_hlsl_surface)
    if(EXISTS "${OMPE_ETCH_DIR}/.ci/shadercross/sdk/SDL3_shadercross-3.0.0-windows-VC-x64/bin/shadercross.exe" AND NOT ETCH_SHADERCROSS_EXE)
        set(ETCH_SHADERCROSS_EXE "${OMPE_ETCH_DIR}/.ci/shadercross/sdk/SDL3_shadercross-3.0.0-windows-VC-x64/bin/shadercross.exe" CACHE FILEPATH "" FORCE)
    endif()
    etch_compile_hlsl_surface(
        TARGET omp_desktop
        SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/omp_ambient.hlsl"
        NAME omp_ambient
    )
    set_source_files_properties(src/ui/app.cpp PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/omp_ambient_shader/omp_ambient_dxil.h")
    target_compile_definitions(omp_desktop PRIVATE OMPE_HAS_AMBIENT_SHADER=1)
    if(DEFINED _etch_spv_probe_res AND _etch_spv_probe_res EQUAL 0)
        target_compile_definitions(omp_desktop PRIVATE OMPE_HAS_AMBIENT_SPV=1)
        if(ETCH_SHADERCROSS_EXE)
            target_compile_definitions(omp_desktop PRIVATE OMPE_HAS_AMBIENT_MSL=1)
        endif()
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts")
    add_custom_command(TARGET omp_desktop POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:omp_desktop>/assets/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
            "$<TARGET_FILE_DIR:omp_desktop>/assets/fonts"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales")
    add_custom_command(TARGET omp_desktop POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:omp_desktop>/assets/locales"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales"
            "$<TARGET_FILE_DIR:omp_desktop>/assets/locales"
    )
endif()

if(WIN32)
    add_custom_command(TARGET omp_desktop POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:omp_desktop>
            $<TARGET_FILE_DIR:omp_desktop>
        COMMAND_EXPAND_LISTS
    )
endif()

if(OMPE_BUILD_TESTS)
    enable_testing()
    add_executable(ompe_core_tests tests/core_tests.cpp)
    target_link_libraries(ompe_core_tests PRIVATE ompe_core)
    if(MSVC)
        target_compile_options(ompe_core_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    else()
        target_compile_options(ompe_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(WIN32)
        add_custom_command(TARGET ompe_core_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:ompe_core_tests>
                $<TARGET_FILE_DIR:ompe_core_tests>
            COMMAND_EXPAND_LISTS
        )
    endif()
    add_test(NAME ompe_core_unit COMMAND ompe_core_tests)
    set_tests_properties(ompe_core_unit PROPERTIES TIMEOUT 15)
endif()
