cmake_minimum_required(VERSION 3.31)
project(dds-boilerplate)

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

set(IDL_LIB cyclonedds_lib)
set(IDL_FILES
        "./idl/Value.idl"
)
idlcxx_generate(TARGET ${IDL_LIB} FILES ${IDL_FILES} WARNINGS no-implicit-extensibility)

set(EXE_NAME dds-boilerplate)
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
target_link_libraries(${EXE_NAME}
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
