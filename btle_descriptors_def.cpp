#include "stdafx.h"

#include "btle_descriptors_def.h"

namespace btle {
#define DEFINE_DESCRIPTOR(id, name) \
  USHORT name = id;
#include "btle_descriptors.h"
#undef DEFINE_DESCRIPTOR
}
