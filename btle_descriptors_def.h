#pragma once

#include <bthledef.h>

namespace btle {
#define DEFINE_DESCRIPTOR(id, name) \
  extern USHORT name;
#include "btle_descriptors.h"
#undef DEFINE_DESCRIPTOR
}
