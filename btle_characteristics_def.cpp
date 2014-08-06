#include "stdafx.h"

#include "btle_characteristics_def.h"

namespace btle {
#define DEFINE_CHARACTERISTIC(id, name) \
  USHORT name = id;
#include "btle_characteristics.h"
#undef DEFINE_CHARACTERISTIC

#define DEFINE_CHARACTERISTIC_LONG(uuid1, uuid2, uuid3, uuid_b1, uuid_b2, uuid_b3, uuid_b4, uuid_b5, uuid_b6, uuid_b7, uuid_b8, name) \
  UUID name = { uuid1, uuid2, uuid3, { uuid_b1, uuid_b2, uuid_b3, uuid_b4, uuid_b5, uuid_b6, uuid_b7, uuid_b8 } };
#include "btle_characteristics_long.h"
#undef DEFINE_CHARACTERISTIC_LONG
}
