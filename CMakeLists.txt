cmake_minimum_required(VERSION 3.31)
project(dds-boilerplate)

if(MSVC)
    # MSVC 17.5 has a C++20 parsing issue in CycloneDDS-CXX headers.
    set(CMAKE_CXX_STANDARD 17)
    # Ensure all targets use the same MSVC runtime library to avoid Debug/Release CRT mismatches.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    set(CMAKE_CXX_STANDARD 20)
endif()

if(NOT TARGET CycloneDDS-CXX::ddscxx)
  find_package(CycloneDDS-CXX REQUIRED)
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    include(${CMAKE_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake)
endif()

set(IDL_LIB cyclonedds_lib)
set(IDL_FILES
        "./idl/Value.idl"
)
idlcxx_generate(TARGET ${IDL_LIB} FILES ${IDL_FILES} WARNINGS no-implicit-extensibility)

set(EXE_NAME dds-gamepad)
set(IDL_HEADERS
        ${CMAKE_CURRENT_BINARY_DIR}/Value.hpp
)
set(EXE_HEADERS
        src/dds_includes.h
)
set(EXE_SOURCES
        src/main.cpp
)

add_executable(${EXE_NAME} ${EXE_HEADERS} ${IDL_HEADERS} ${EXE_SOURCES})
target_link_libraries(${EXE_NAME} PRIVATE
        ${IDL_LIB}
        CycloneDDS::ddsc
        CycloneDDS-CXX::ddscxx)
if(WIN32)
    target_compile_definitions(${EXE_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()
target_include_directories(${EXE_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        "${CMAKE_CURRENT_SOURCE_DIR}/install/cyclonedds/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/install/cyclonedds-cxx/include/ddscxx")

if(WIN32)
    include(FetchContent)

    set(VIGEMCLIENT_SOURCE_DIR "" CACHE PATH "Optional local path to ViGEmClient source")
    if(VIGEMCLIENT_SOURCE_DIR)
        add_subdirectory(${VIGEMCLIENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/vigemclient)
    else()
        FetchContent_Declare(
                ViGEmClient
                GIT_REPOSITORY https://github.com/ViGEm/ViGEmClient.git
                GIT_TAG v1.21.222.0
        )
        FetchContent_MakeAvailable(ViGEmClient)
    endif()

    if(TARGET ViGEmClient::ViGEmClient)
        set(_vigemclient_target ViGEmClient::ViGEmClient)
    elseif(TARGET ViGEmClient)
        set(_vigemclient_target ViGEmClient)
    else()
        message(FATAL_ERROR "ViGEmClient target not found. Ensure the ViGEmClient source provides a CMake target.")
    endif()

    add_library(vigem_wrapper STATIC
            src/emulator/VigemClient.cpp
            src/emulator/VigemClient.h
    )
    target_include_directories(vigem_wrapper PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(vigem_wrapper PUBLIC ${_vigemclient_target})
    target_compile_definitions(vigem_wrapper PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(vigem_wrapper PRIVATE Setupapi)

    add_executable(vigem_sanity src/vigem_sample.cpp)
    target_link_libraries(vigem_sanity PRIVATE vigem_wrapper)
    target_compile_definitions(vigem_sanity PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

    target_link_libraries(${EXE_NAME} PRIVATE vigem_wrapper)
endif()

# Embed runtime library search paths so installed binary finds CycloneDDS libs without LD_LIBRARY_PATH.
set_target_properties(${EXE_NAME} PROPERTIES
        BUILD_RPATH "$ORIGIN/../../cyclonedds/lib;$ORIGIN/../../cyclonedds-cxx/lib"
        INSTALL_RPATH "$ORIGIN/../../cyclonedds/lib;$ORIGIN/../../cyclonedds-cxx/lib"
)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(INSTALL_DIR "" CACHE PATH "Installation directory")

if(NOT "${INSTALL_DIR}" STREQUAL "")
    install(TARGETS ${EXE_NAME} RUNTIME DESTINATION ${INSTALL_DIR}/bin)
    if(WIN32)
        install(TARGETS vigem_sanity RUNTIME DESTINATION ${INSTALL_DIR}/bin)
    endif()
    if(WIN32)
        # Copy runtime DLLs next to the installed executable for easy local runs.
        set(_install_bin "${INSTALL_DIR}/bin")
        set(_cyclonedds_bin "${CMAKE_CURRENT_SOURCE_DIR}/install/cyclonedds/bin")
        set(_cyclonedds_cxx_bin "${CMAKE_CURRENT_SOURCE_DIR}/install/cyclonedds-cxx/bin")

        install(FILES
            "${_cyclonedds_bin}/ddsc.dll"
            "${_cyclonedds_cxx_bin}/ddscxx.dll"
            DESTINATION ${_install_bin}
        )
    endif()

    # Install IDL files
    install(DIRECTORY idl/
            DESTINATION ${INSTALL_DIR}/idl
            FILES_MATCHING PATTERN "*.idl")
endif()
