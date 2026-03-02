/// @file dds_includes.h
/// @brief Macro guard header — undefines conflicting Windows/third-party macros
///        before including the CycloneDDS C++ API.
///
/// @details Several headers (Windows SDK, COM, and some third-party libraries)
/// define macros whose names collide with CycloneDDS-CXX identifiers:
///
/// | Macro            | Typical source              |
/// |------------------|-----------------------------|
/// | `dds`            | Some COM/MIDL generated code |
/// | `core`           | Third-party platform headers |
/// | `topic`          | Windows SDK (objbase.h area) |
/// | `TopicInstance`  | Various                      |
/// | `InstanceHandle` | Various                      |
/// | `interface`      | Windows SDK (`rpc.h`)        |
/// | `min`, `max`     | Windows SDK (`windef.h`)     |
///
/// Include **this header** instead of `<dds/dds.hpp>` anywhere in the project.
///
/// @warning Do not include `<Windows.h>` or any header that re-defines these
///          macros *after* including this file, or DDS compilation will fail.

#pragma once

// Undefine problematic Windows or third-party macros that can corrupt DDS headers.
#ifdef dds
#undef dds
#endif
#ifdef core
#undef core
#endif
#ifdef topic
#undef topic
#endif
#ifdef TopicInstance
#undef TopicInstance
#endif
#ifdef InstanceHandle
#undef InstanceHandle
#endif
#ifdef interface
#undef interface
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <dds/dds.hpp>
