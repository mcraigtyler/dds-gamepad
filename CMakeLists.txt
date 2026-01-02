cmake_minimum_required(VERSION 3.31)
project(dds-gamepad)

if(NOT WIN32)
    message(FATAL_ERROR "dds-gamepad is a Windows-only application")
endif()

if(MSVC)
    # MSVC 17.5 has a C++20 parsing issue in CycloneDDS-CXX headers.
    set(CMAKE_CXX_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD 20)
endif()

if(NOT TARGET CycloneDDS-CXX::ddscxx)
  find_package(CycloneDDS-CXX REQUIRED)
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    include(${CMAKE_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake)
endif()

find_package(yaml-cpp CONFIG REQUIRED)

include(FetchContent)

set(IDL_LIB cyclonedds_lib)
set(IDL_FILES
        "./idl/Common.idl"
        "./idl/Gamepad.idl"
        "./idl/Standard_types.idl"
        "./idl/Value.idl"
)
idlcxx_generate(TARGET ${IDL_LIB} FILES ${IDL_FILES} WARNINGS no-implicit-extensibility)

set(EXE_NAME dds-gamepad)
set(SERVICE_EXE_NAME dds-gamepad-service)
set(IDL_HEADERS
        ${CMAKE_CURRENT_BINARY_DIR}/Gamepad.hpp
)
set(EXE_HEADERS
        src/dds_includes.h
)
set(EXE_SOURCES
        src/main.cpp
    src/console/RxTable.cpp
    src/app/AppRunner.cpp
        src/config/ConfigLoader.cpp
        src/mapper/MappingEngine.cpp
)

add_executable(${EXE_NAME} ${EXE_HEADERS} ${IDL_HEADERS} ${EXE_SOURCES})

set(SERVICE_SOURCES
    src/service/ServiceMain.cpp
    src/console/RxTable.cpp
    src/app/AppRunner.cpp
    src/config/ConfigLoader.cpp
    src/mapper/MappingEngine.cpp
)

add_executable(${SERVICE_EXE_NAME} ${EXE_HEADERS} ${IDL_HEADERS} ${SERVICE_SOURCES})
target_link_libraries(${EXE_NAME}
    PRIVATE
        ${IDL_LIB}
        CycloneDDS::ddsc
        CycloneDDS-CXX::ddscxx
        yaml-cpp::yaml-cpp)

target_link_libraries(${SERVICE_EXE_NAME}
    PRIVATE
        ${IDL_LIB}
        CycloneDDS::ddsc
        CycloneDDS-CXX::ddscxx
        yaml-cpp::yaml-cpp)
if(WIN32)
    target_compile_definitions(${EXE_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
    target_compile_definitions(${SERVICE_EXE_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()
target_include_directories(${EXE_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        "${CMAKE_CURRENT_SOURCE_DIR}/install/cyclonedds/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/install/cyclonedds-cxx/include/ddscxx")

target_include_directories(${SERVICE_EXE_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        "${CMAKE_CURRENT_SOURCE_DIR}/install/cyclonedds/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/install/cyclonedds-cxx/include/ddscxx")

if(WIN32)
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

    target_link_libraries(${EXE_NAME} PRIVATE vigem_wrapper)
    target_link_libraries(${SERVICE_EXE_NAME} PRIVATE vigem_wrapper)

    add_executable(vigem_sanity src/vigem_sample.cpp)
    target_link_libraries(vigem_sanity PRIVATE vigem_wrapper)
    target_compile_definitions(vigem_sanity PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Embed runtime library search paths so installed binary finds CycloneDDS libs without LD_LIBRARY_PATH.
set_target_properties(${EXE_NAME} PROPERTIES
        BUILD_RPATH "$ORIGIN/../../cyclonedds/lib;$ORIGIN/../../cyclonedds-cxx/lib"
        INSTALL_RPATH "$ORIGIN/../../cyclonedds/lib;$ORIGIN/../../cyclonedds-cxx/lib"
)
set_target_properties(${SERVICE_EXE_NAME} PROPERTIES
    BUILD_RPATH "$ORIGIN/../../cyclonedds/lib;$ORIGIN/../../cyclonedds-cxx/lib"
    INSTALL_RPATH "$ORIGIN/../../cyclonedds/lib;$ORIGIN/../../cyclonedds-cxx/lib"
)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(INSTALL_DIR "" CACHE PATH "Installation directory")

if(NOT "${INSTALL_DIR}" STREQUAL "")
    install(TARGETS ${EXE_NAME} RUNTIME DESTINATION ${INSTALL_DIR}/bin)
    install(TARGETS ${SERVICE_EXE_NAME} RUNTIME DESTINATION ${INSTALL_DIR}/bin)
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
        # Also copy yaml-cpp runtime DLL next to the installed executable so
        # local runs don't fail with "yaml-cpp.dll was not found." Use the
        # target file generator expression to locate the imported target's
        # runtime file (DLL) provided by vcpkg or other package config.
        install(FILES
            "$<TARGET_FILE:yaml-cpp::yaml-cpp>"
            DESTINATION ${_install_bin}
        )
    endif()

    if(WIN32)
        # Include optional installer binaries from the repository external/ folder
        set(_external_dir "${CMAKE_CURRENT_SOURCE_DIR}/external")
        install(FILES
            "${_external_dir}/ViGEmBus_1.22.0_x64_x86_arm64.exe"
            "${_external_dir}/vc_redist.x64.exe"
            DESTINATION ${INSTALL_DIR}/installers
            OPTIONAL
        )
    endif()

    # Install IDL files
    install(DIRECTORY idl/
            DESTINATION ${INSTALL_DIR}/idl
            FILES_MATCHING PATTERN "*.idl")

        # Install top-level config files (YAML) used by the application
        install(DIRECTORY config/
            DESTINATION ${INSTALL_DIR}/config
            FILES_MATCHING PATTERN "*.yaml")

        # Service convention: config lives next to the service executable.
        install(DIRECTORY config/
            DESTINATION ${INSTALL_DIR}/bin/config
            FILES_MATCHING PATTERN "*.yaml")
endif()
