#pragma once

#include <bthledef.h>

namespace btle {
#define DEFINE_SERVICE(id, name) \
  extern USHORT name;
#include "btle_services.h"
#undef DEFINE_SERVICE

#define DEFINE_SERVICE_LONG(uuid1, uuid2, uuid3, uuid_b1, uuid_b2, uuid_b3, uuid_b4, uuid_b5, uuid_b6, uuid_b7, uuid_b8, name) \
  extern UUID name;
#include "btle_services_long.h"
#undef DEFINE_SERVICE_LONG
}
