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

