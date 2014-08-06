#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <Bluetoothleapis.h>

namespace btle {

GUID BTH_LE_UUID_TO_GUID(const BTH_LE_UUID& bth_le_uuid) {
  if (bth_le_uuid.IsShortUuid) {
    GUID result = BTH_LE_ATT_BLUETOOTH_BASE_GUID;
    result.Data1 += bth_le_uuid.Value.ShortUuid;
    return result;
  }
  else {
    return bth_le_uuid.Value.LongUuid;
  }
}

BTH_LE_UUID TO_BTH_LE_UUID(USHORT short_uuid) {
  BTH_LE_UUID result = {0};
  result.IsShortUuid = true;
  result.Value.ShortUuid = short_uuid;
  return result;
}

BTH_LE_UUID TO_BTH_LE_UUID(const UUID& long_uuid) {
  BTH_LE_UUID result = {0};
  result.IsShortUuid = false;
  result.Value.LongUuid = long_uuid;
  return result;
}

std::string BTH_LE_GATT_DESCRIPTOR_TYPE_TO_STRING(BTH_LE_GATT_DESCRIPTOR_TYPE descriptor_type) {
  switch(descriptor_type) {
  case CharacteristicExtendedProperties: return "CharacteristicExtendedProperties";
  case CharacteristicUserDescription: return "CharacteristicUserDescription";
  case ClientCharacteristicConfiguration: return "ClientCharacteristicConfiguration";
  case ServerCharacteristicConfiguration: return "ServerCharacteristicConfiguration";
  case CharacteristicFormat: return "CharacteristicFormat";
  case CharacteristicAggregateFormat: return "CharacteristicAggregateFormat";
  case CustomDescriptor: return "CustomDescriptor";
  default:
    return std::string("<unknown>");
  }
}

std::string GUID_TO_STRING(const GUID& uuid) {
  std::ostringstream stringStream;
  stringStream << std::hex;
  stringStream << std::setfill('0');
  stringStream << std::setw(8) << uuid.Data1;
  stringStream << "-";
  stringStream << std::setw(4) << uuid.Data2;
  stringStream << "-";
  stringStream << std::setw(4) << uuid.Data3;
  stringStream << "-";
  stringStream << std::setw(2);
  for(int i = 0; i < sizeof(uuid.Data4); i++) {
    if (i == 2)
      stringStream << "-";
    stringStream << static_cast<int>(uuid.Data4[i]);
  }
  return stringStream.str();
}

std::string BTH_LE_UUID_TO_STRING(const BTH_LE_UUID& uuid) {
  if (uuid.IsShortUuid) {
    std::ostringstream stringStream;
    stringStream << std::hex;
    stringStream << std::setfill('0');
    stringStream << "0x" << static_cast<int>(uuid.Value.ShortUuid);
    return stringStream.str();
  }
  else {
    return GUID_TO_STRING(uuid.Value.LongUuid);
  }

}

std::string BOOLEAN_TO_STRING(BOOLEAN value) {
  return value ? std::string("true") : std::string("false");
}

std::string CHARACTERISTIC_UUID_TO_STRING(const BTH_LE_UUID& uuid) {
  if (uuid.IsShortUuid) {
    switch(uuid.Value.ShortUuid) {
#define DEFINE_CHARACTERISTIC(id, name) case id: { \
      std::ostringstream stream; \
      stream << BTH_LE_UUID_TO_STRING(uuid) << " ['" << #name << "']"; \
      return stream.str(); \
    }
#include "btle_characteristics.h"
#undef DEFINE_CHARACTERISTIC
    }
  } else {
#define DEFINE_CHARACTERISTIC_LONG(uuid1, uuid2, uuid3, uuid_b1, uuid_b2, uuid_b3, uuid_b4, uuid_b5, uuid_b6, uuid_b7, uuid_b8, name) \
      { \
        UUID long_uuid = {uuid1, uuid2, uuid3, uuid_b1, uuid_b2, uuid_b3, uuid_b4, uuid_b5, uuid_b6, uuid_b7, uuid_b8}; \
        if (uuid.Value.LongUuid == long_uuid) { \
          std::ostringstream stream; \
          stream << BTH_LE_UUID_TO_STRING(uuid) << " ['" << #name << "']"; \
          return stream.str(); \
        } \
      }
#include "btle_characteristics_long.h"
#undef DEFINE_CHARACTERISTIC_LONG
  }
  return BTH_LE_UUID_TO_STRING(uuid);
}

std::string SERVICE_UUID_TO_STRING(const BTH_LE_UUID& uuid) {
  if (uuid.IsShortUuid) {
    switch(uuid.Value.ShortUuid) {
#define DEFINE_SERVICE(id, name) case id: { \
      std::ostringstream stream; \
      stream << BTH_LE_UUID_TO_STRING(uuid) << " ['" << #name << "']"; \
      return stream.str(); \
    }
#include "btle_services.h"
#undef DEFINE_SERVICE
    }
  } else {
#define DEFINE_SERVICE_LONG(uuid1, uuid2, uuid3, uuid_b1, uuid_b2, uuid_b3, uuid_b4, uuid_b5, uuid_b6, uuid_b7, uuid_b8, name) \
      { \
        UUID long_uuid = {uuid1, uuid2, uuid3, uuid_b1, uuid_b2, uuid_b3, uuid_b4, uuid_b5, uuid_b6, uuid_b7, uuid_b8}; \
        if (uuid.Value.LongUuid == long_uuid) { \
          std::ostringstream stream; \
          stream << BTH_LE_UUID_TO_STRING(uuid) << " ['" << #name << "']"; \
          return stream.str(); \
        } \
      }
#include "btle_services_long.h"
#undef DEFINE_SERVICE_LONG
  }
  return BTH_LE_UUID_TO_STRING(uuid);
}

std::string DESCRIPTOR_UUID_TO_STRING(const BTH_LE_UUID& uuid) {
  if (uuid.IsShortUuid) {
    switch(uuid.Value.ShortUuid) {
#define DEFINE_DESCRIPTOR(id, name) case id: { \
      std::ostringstream stream; \
      stream << BTH_LE_UUID_TO_STRING(uuid) << " ['" << #name << "']"; \
      return stream.str(); \
    }
#include "btle_descriptors.h"
#undef DEFINE_DESCRIPTOR
    }
  }
  return BTH_LE_UUID_TO_STRING(uuid);
}

}  // namespace btle

class scoped_hdevinfo {
public:
  scoped_hdevinfo() : handle_(INVALID_HANDLE_VALUE) {
  }

  explicit scoped_hdevinfo(HDEVINFO handle) : handle_(handle) {
  }

  ~scoped_hdevinfo() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      SetupDiDestroyDeviceInfoList(handle_);
    }
  }

  void Close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      SetupDiDestroyDeviceInfoList(handle_);
      handle_ = INVALID_HANDLE_VALUE;
    }
  }

  HRESULT OpenBluetoothLeDevices() {
    GUID BluetoothClassGUID = GUID_BLUETOOTHLE_DEVICE_INTERFACE;
    HDEVINFO result = SetupDiGetClassDevs(
      &BluetoothClassGUID,
      NULL,
      NULL,
      DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (result == INVALID_HANDLE_VALUE) {
      return HRESULT_FROM_WIN32(GetLastError());
    }

    Close();
    handle_ = result;
    return S_OK;
  }

  HRESULT OpenBluetoothLeService(const GUID& service_guid) {
    HDEVINFO result = SetupDiGetClassDevs(
      &service_guid,
      NULL,
      NULL,
      //DIGCF_PRESENT | DIGCF_ALLCLASSES | DIGCF_DEVICEINTERFACE));
      DIGCF_PRESENT| DIGCF_DEVICEINTERFACE);

    if (result == INVALID_HANDLE_VALUE) {
      return HRESULT_FROM_WIN32(GetLastError());
    }

    Close();
    handle_ = result;
    return S_OK;
  }

  HDEVINFO get() const { return handle_; }

private:
  scoped_hdevinfo(const scoped_hdevinfo& other);
  const scoped_hdevinfo& operator=(const scoped_hdevinfo& other);

  HDEVINFO handle_;
};

inline
bool NoDataResult(HRESULT hr, int length) {
  if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
    return true;
    
  if (SUCCEEDED(hr) && length == 0)
    return true;

  return false;
}
