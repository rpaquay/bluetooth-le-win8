// BluetoothLowEnergyNativeApp.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <setupapi.h>
#include <devguid.h>
#include <devpkey.h>

#include "base.h"
#include "btle.h"
#include "btle_helpers.h"
#include "btle_services_def.h"
#include "btle_characteristics_def.h"

struct DevPropertyKey {
  DEVPROPKEY key;
  const char* name;
};

struct DevPropertyKey DevPropertyKeys[] {

#undef DEFINE_DEVPROPKEY
#define DEFINE_DEVPROPKEY(name, uuid1, uuid2, uuid3, uuid_b1, uuid_b2, uuid_b3, uuid_b4, uuid_b5, uuid_b6, uuid_b7, uuid_b8, pid) \
  { \
    { /*fmtid*/{uuid1, uuid2, uuid3, uuid_b1, uuid_b2, uuid_b3, uuid_b4, uuid_b5, uuid_b6, uuid_b7, uuid_b8 } , /* pid */ pid}, \
    /* name */ #name \
  },
#include "devpropkeys.h"
#undef DEFINE_DEVPROPKEY
};

std::string DEVPROPKEY_TO_STRING(const DEVPROPKEY& key) {
  for(size_t index = 0; index < sizeof(DevPropertyKeys) / sizeof(DevPropertyKey); ++index) {
    const DevPropertyKey& local_key = DevPropertyKeys[index];
    if (local_key.key == key) {
      return local_key.name;
    }
  }
  std::ostringstream stream;
  stream << "guid=" << btle::GUID_TO_STRING(key.fmtid) << ", pid=" << key.pid;
  return stream.str();
}

std::string BLUETOOTH_ADDRESS_TO_STRING(const BLUETOOTH_ADDRESS& btha) {
  char buffer[6 * 2 + 1];
  sprintf_s(buffer, "%02X%02X%02X%02X%02X%02X",
      btha.rgBytes[5],
      btha.rgBytes[4],
      btha.rgBytes[3],
      btha.rgBytes[2],
      btha.rgBytes[1],
      btha.rgBytes[0]);
  return std::string(buffer);
}

bool STRING_TO_BLUETOOTH_ADDRESS(std::string value, BLUETOOTH_ADDRESS* btha, std::string* error) {
  if (value.length() != 6 * 2) {
    *error = "Bluetooth address length is incorrect.";
    return false;
  }

  int buffer[6];
  int result = sscanf_s(value.c_str(), "%02X%02X%02X%02X%02X%02X",
      &buffer[5],
      &buffer[4],
      &buffer[3],
      &buffer[2],
      &buffer[1],
      &buffer[0]);
  if (result != 6) {
    *error = "Bluetooth address contains invalid characters.";
    return false;
  }

  ZeroMemory(btha, sizeof(*btha));
  btha->rgBytes[0] = buffer[0];
  btha->rgBytes[1] = buffer[1];
  btha->rgBytes[2] = buffer[2];
  btha->rgBytes[3] = buffer[3];
  btha->rgBytes[4] = buffer[4];
  btha->rgBytes[5] = buffer[5];
  return true;
}

bool CheckInsufficientBuffer(bool success, std::string function_name, std::string* error) {
  if (success) {
    std::ostringstream string_stream;
    string_stream << "Unexpected successfull call to " << function_name;
    *error = string_stream.str();
    return false;
  }

  DWORD last_error = GetLastError();
  if (last_error != ERROR_INSUFFICIENT_BUFFER) {
    std::ostringstream string_stream;
    string_stream << "Unexpected error from call to " << function_name << ", hr=" << HRESULT_FROM_WIN32(last_error);
    *error = string_stream.str();
    return false;
  }

  return true;
}

bool CheckSuccessulHResult(HRESULT hr, size_t actual_length, size_t expected_length, std::string function_name, std::string* error) {
  if (FAILED(hr)) {
    std::ostringstream string_stream;
    string_stream << "Error calling " << function_name << ", hr=" << hr;
    *error = string_stream.str();
    return false;
  }
  if (actual_length != expected_length) {
    std::ostringstream string_stream;
    string_stream << "Returned length does not match required length when calling " << function_name << "";
    *error = string_stream.str();
    return false;
  }

  return true;
}

bool CheckSuccessulResult(bool success, size_t actual_length, size_t expected_length, std::string function_name, std::string* error) {
  HRESULT hr = S_OK;
  if (!success)
    hr = HRESULT_FROM_WIN32(GetLastError());
  return CheckSuccessulHResult(hr, actual_length, expected_length, function_name, error);
}

//////////////////////////////////////////////////////////////////////////////
// Represents a registry property value
//
class DeviceRegistryProperty : public RefCounted<DeviceRegistryProperty> {
public:
  DeviceRegistryProperty(DWORD property_type, scoped_array<UINT8>& value, size_t value_size)
      : property_type_(property_type), value_(value.Pass()), value_size_(value_size) {
  }

  bool AsString(std::string* value, std::string* error) {
    if (property_type_ != REG_SZ) {
      *error = "Property is not a string";
      return false;
    }

    std::wstring wvalue(reinterpret_cast<WCHAR*>(value_.get()));
    *value = to_std_string(wvalue);
    return true;
  }

private:
  DWORD property_type_;
  scoped_array<UINT8> value_;
  size_t value_size_;
};

//////////////////////////////////////////////////////////////////////////////
// Represents a single DEVPROPKEY instance.
//
struct DevicePropertyKey {
  DevicePropertyKey(DEVPROPKEY k) : key(k) {}

  DEVPROPKEY key;

  std::string ToString() {
    return DEVPROPKEY_TO_STRING(key);
  }
};

//////////////////////////////////////////////////////////////////////////////
// Represents a collection of DEVPROPKEY instances.
//
class DevicePropertyKeys : public RefCounted<DevicePropertyKeys> {
public:
  DevicePropertyKeys(scoped_array<DEVPROPKEY>& keys, size_t keys_count)
      : keys_(keys.Pass()), keys_count_(keys_count) {
  }

  DevicePropertyKey get(int index) {
    return DevicePropertyKey(keys_.get()[index]);
  }

  size_t count() { return keys_count_; }

private:
  scoped_array<DEVPROPKEY> keys_;
  size_t keys_count_;
};

//////////////////////////////////////////////////////////////////////////////
// Represents a DEVPROPTYPE value.
//
class DevicePropertyValue : public RefCounted<DevicePropertyValue> {
public:
  DevicePropertyValue(DEVPROPTYPE value_type, scoped_array<UINT8>& value, size_t value_size)
      : value_type_(value_type), value_(value.Pass()), value_size_(value_size) {
  }

  std::string ToString() {
    std::ostringstream stream;
    switch(value_type_) {
    case DEVPROP_TYPE_EMPTY:
      stream << "(empty)";
      break;
    case DEVPROP_TYPE_GUID:
      stream << btle::GUID_TO_STRING(*reinterpret_cast<GUID*>(value_.get())) << " (guid)";
      break;
    case DEVPROP_TYPE_BOOLEAN:
      stream << btle::BOOLEAN_TO_STRING(*reinterpret_cast<BOOLEAN*>(value_.get())) << " (bool)";
      break;
    case DEVPROP_TYPE_STRING:
      stream << to_std_string(std::wstring(reinterpret_cast<WCHAR*>(value_.get()))) << " (wstring)";
      break;
    default:
      stream << " (prop type=" << value_type_ << ")";
    }
    return stream.str();
  }

private:
  DEVPROPTYPE value_type_;
  scoped_array<UINT8> value_;
  size_t value_size_;
};


enum DeviceInfoResult {
  kOk,
  kError,
  kNoMoreDevices
};

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectDeviceProperty(HDEVINFO device_info_handle, SP_DEVINFO_DATA& device_info_data, const DEVPROPKEY& key, scoped_refptr<DevicePropertyValue>* value, std::string* error) {
  DWORD required_length;
  DEVPROPTYPE prop_type;
  BOOL success = SetupDiGetDeviceProperty(
    device_info_handle,
    &device_info_data,
    &key,
    &prop_type,
    NULL,
    0,
    &required_length,
    0);
  if (!CheckInsufficientBuffer(success, "SetupDiGetDeviceProperty", error))
    return false;

  scoped_array<UINT8> prop_value(new UINT8[required_length]);
  DWORD actual_length = required_length;
  success = SetupDiGetDeviceProperty(
    device_info_handle,
    &device_info_data,
    &key,
    &prop_type,
    prop_value.get(),
    actual_length,
    &required_length,
    0);
  if (!CheckSuccessulResult(success, actual_length, required_length, "SetupDiGetDeviceProperty", error))
    return false;

  (*value) = scoped_refptr<DevicePropertyValue>(new DevicePropertyValue(prop_type, prop_value, actual_length));
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectDevicePropertyKeys(HDEVINFO device_info_handle, SP_DEVINFO_DATA& device_info_data, scoped_refptr<DevicePropertyKeys>* keys, std::string* error) {
  DWORD required_count;
  BOOL success = SetupDiGetDevicePropertyKeys(
    device_info_handle,
    &device_info_data,
    NULL,
    0,
    &required_count,
    0);
  if (!CheckInsufficientBuffer(success, "SetupDiGetDevicePropertyKeys", error))
    return false;
  
  scoped_array<DEVPROPKEY> prop_keys(new DEVPROPKEY[required_count]);
  DWORD actual_count = required_count;
  success = SetupDiGetDevicePropertyKeys(
    device_info_handle,
    &device_info_data,
    prop_keys.get(),
    actual_count,
    &required_count,
    0);
  if (!CheckSuccessulResult(success, actual_count, required_count, "SetupDiGetDevicePropertyKeys", error))
    return false;

  (*keys) = scoped_refptr<DevicePropertyKeys>(new DevicePropertyKeys(prop_keys, actual_count));
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectDeviceRegistryProperty(HDEVINFO device_info_handle, SP_DEVINFO_DATA& device_info_data, DWORD property, scoped_refptr<DeviceRegistryProperty>* value, std::string* error) {
  ULONG required_length = 0;
  BOOL success = SetupDiGetDeviceRegistryProperty(
      device_info_handle,
      &device_info_data,
      property,
      NULL,
      NULL,
      0, 
      &required_length);
  if (!CheckInsufficientBuffer(success, "SetupDiGetDeviceRegistryProperty", error))
    return false;

  scoped_array<UINT8> property_value(new UINT8[required_length]);
  ULONG actual_length = required_length;
  DWORD property_type;
  success = SetupDiGetDeviceRegistryProperty(
      device_info_handle,
      &device_info_data,
      property,
      &property_type,
      property_value.get(),
      actual_length, 
      &required_length);
  if (!CheckSuccessulResult(success, actual_length, required_length, "SetupDiGetDeviceRegistryProperty", error))
    return false;

  (*value) = scoped_refptr<DeviceRegistryProperty>(new DeviceRegistryProperty(property_type, property_value, actual_length));
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectDeviceFriendlyName(HDEVINFO device_info_handle, SP_DEVINFO_DATA& device_info_data, btle::DeviceInfo* device_info, std::string* error) {
  scoped_refptr<DeviceRegistryProperty> property;
  if (!CollectDeviceRegistryProperty(device_info_handle, device_info_data, SPDRP_FRIENDLYNAME, &property, error)) {
    return false;
  }

  if (!property->AsString(&device_info->friendly_name, error)) {
    return false;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectDeviceBluetoothAddress(HDEVINFO device_info_handle, SP_DEVINFO_DATA& device_info_data, btle::DeviceInfo* device_info, std::string* error) {
  size_t start = device_info->id.find("_");
  if (start == std::string::npos) {
    *error = "Device instance ID value does not seem to contain a Bluetooth Adpater address.";
    return false;
  }
  size_t end = device_info->id.find("\\", start);
  if (end == std::string::npos) {
    *error = "Device instance ID value does not seem to contain a Bluetooth Adpater address.";
    return false;
  }

  start++;
  std::string address = device_info->id.substr(start, end - start);
  if (!STRING_TO_BLUETOOTH_ADDRESS(address, &device_info->address, error))
    return false;

  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectDeviceInstanceId(HDEVINFO device_info_handle, SP_DEVINFO_DATA& device_info_data, btle::DeviceInfo* device_info, std::string* error) {
  ULONG required_length = 0;
  BOOL success = SetupDiGetDeviceInstanceId(
      device_info_handle,
      &device_info_data,
      NULL,
      0, 
      &required_length);
  if (!CheckInsufficientBuffer(success, "SetupDiGetDeviceInstanceId", error))
    return false;

  scoped_array<WCHAR> instance_id(new WCHAR[required_length]);
  ULONG actual_length = required_length;
  success = SetupDiGetDeviceInstanceId(
      device_info_handle,
      &device_info_data,
      instance_id.get(),
      actual_length, 
      &required_length);
  if (!CheckSuccessulResult(success, actual_length, required_length, "SetupDiGetDeviceInstanceId", error))
    return false;

  device_info->id = to_std_string(std::wstring(instance_id.get()));
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool DisplayProperty(HDEVINFO device_info_handle, SP_DEVICE_INTERFACE_DATA& device_interface_data, DEVPROPKEY& key, std::string* error) {
  DWORD required_length;
  DEVPROPTYPE prop_type;
  BOOL success = SetupDiGetDeviceInterfaceProperty(
    device_info_handle,
    &device_interface_data,
    &key,
    &prop_type,
    NULL,
    0,
    &required_length,
    0);
  if (!CheckInsufficientBuffer(success, "SetupDiGetDeviceInterfaceProperty", error))
    return false;

  scoped_array<UINT8> prop_value(new UINT8[required_length]);
  DWORD actual_length = required_length;
  success = SetupDiGetDeviceInterfaceProperty(
    device_info_handle,
    &device_interface_data,
    &key,
    &prop_type,
    prop_value.get(),
    actual_length,
    &required_length,
    0);
  if (!CheckSuccessulResult(success, actual_length, required_length, "SetupDiGetDeviceInterfaceProperty", error))
    return false;

  scoped_refptr<DevicePropertyValue> value(new DevicePropertyValue(prop_type, prop_value, actual_length));
  std::cout << "  Key: " << DevicePropertyKey(key).ToString() << " - Value: " << value->ToString() << "\n";
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool DisplayDeviceInterfaceProperties(HDEVINFO device_info_handle, SP_DEVICE_INTERFACE_DATA& device_interface_data, std::string* error) {
  DWORD required_count;
  BOOL success = SetupDiGetDeviceInterfacePropertyKeys(
    device_info_handle,
    &device_interface_data,
    NULL,
    0,
    &required_count,
    0);
  if (!CheckInsufficientBuffer(success, "SetupDiGetDeviceInterfacePropertyKeys", error))
    return false;
  
  scoped_array<DEVPROPKEY> prop_keys(new DEVPROPKEY[required_count]);
  DWORD actual_count = required_count;
  success = SetupDiGetDeviceInterfacePropertyKeys(
    device_info_handle,
    &device_interface_data,
    prop_keys.get(),
    actual_count,
    &required_count,
    0);
  if (!CheckSuccessulResult(success, actual_count, required_count, "SetupDiGetDeviceInterfacePropertyKeys", error))
    return false;

  for(DWORD i = 0; i < actual_count; i++) {
    if (!DisplayProperty(device_info_handle, device_interface_data, prop_keys.get()[i], error)) {
      return false;
    }
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool DisplayDeviceProperties(HDEVINFO device_info_handle, SP_DEVINFO_DATA& device_info_data, std::string* error) {
  scoped_refptr<DevicePropertyKeys> keys;
  if (!CollectDevicePropertyKeys(device_info_handle, device_info_data, &keys, error)) {
    return false;
  }

  for(DWORD i = 0; i < keys->count(); ++i) {
    scoped_refptr<DevicePropertyValue> value;
    if (!CollectDeviceProperty(device_info_handle, device_info_data, keys->get(i).key, &value, error)) {
      return false;
    }

    std::cout << "  Key: " << keys->get(i).ToString() << " - Value: " << value->ToString() << "\n";

  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
DeviceInfoResult TryCollectServicePath(HDEVINFO device_info_handle, int device_index, const GUID& service_guid, std::wstring* path, std::string* error) {
  // Enumerate device of LE_DEVICE interface class
  GUID temp_guid = service_guid;
  SP_DEVICE_INTERFACE_DATA device_interface_data = {0};
  device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
  BOOL success = SetupDiEnumDeviceInterfaces(
      device_info_handle, 
      NULL,
      (LPGUID)&temp_guid,
      device_index,
      &device_interface_data);
  if (!success) {
    DWORD last_error = GetLastError();
    if (last_error == ERROR_NO_MORE_ITEMS) {
      return kNoMoreDevices;
    }
    else {
      std::ostringstream string_stream;
      string_stream << "Error enumerating device interfaces: " << last_error;
      *error = string_stream.str();
      return kError;
    }
  }

  // Retrieve required # of bytes for interface details
  ULONG required_length = 0;
  success = SetupDiGetDeviceInterfaceDetail(
      device_info_handle,
      &device_interface_data,
      NULL,
      0, 
      &required_length, 
      NULL);
  if (!CheckInsufficientBuffer(success, "SetupDiGetDeviceInterfaceDetail", error))
    return kError;
  
  scoped_array<UINT8> interface_data(new UINT8[required_length]);
  RtlZeroMemory(interface_data.get(), required_length);

  PSP_DEVICE_INTERFACE_DETAIL_DATA device_interface_detail_data = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(interface_data.get());
  device_interface_detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

  ULONG predicted_length = required_length;
  success = SetupDiGetDeviceInterfaceDetail(
      device_info_handle,
      &device_interface_data,
      device_interface_detail_data,
      predicted_length,
      &required_length, 
      NULL);
  if (!CheckSuccessulResult(success, predicted_length, required_length, "SetupDiGetDeviceInterfaceDetail", error))
    return kError;

  *path = std::wstring(device_interface_detail_data->DevicePath);
  return kOk;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool TryGetDeviceServicePath(scoped_refptr<btle::Device> device, const BTH_LE_UUID& service_uuid, std::wstring* path, std::string* error) {
  GUID long_uuid = btle::BTH_LE_UUID_TO_GUID(service_uuid);
  scoped_hdevinfo dev_info_handle;
  HRESULT hr = dev_info_handle.OpenBluetoothLeService(long_uuid);
  if (FAILED(hr)) {
    std::ostringstream string_stream;
    string_stream << "Error enumerating service from GUID: HRESULT=" << hr;
    *error = string_stream.str();
    return false;
  }

  std::vector<std::wstring> paths;
  for(int i = 0; ; i++) {
    std::wstring service_path;
    std::string device_info_error;
    DeviceInfoResult result = TryCollectServicePath(dev_info_handle.get(), i, long_uuid, &service_path, &device_info_error);
    if (result == kNoMoreDevices) {
      break;
    }
    else if (result == kError) {
      *error = device_info_error;
      return false;
    }
    else {
      std::string device_address = to_lower_string(BLUETOOTH_ADDRESS_TO_STRING(device->info().address));
      std::string path = to_lower_string(to_std_string(service_path));
      if (path.find(device_address) != std::string::npos) {
        paths.push_back(service_path);
      }
    }
  }

  if (paths.size() >= 2) {
    std::ostringstream string_stream;
    string_stream << "There is more than one service for the given device. How can this be?";
    *error = string_stream.str();
    return false;
  }

  if (paths.size() == 0) {
    (*path) = L"";
    return true;
  }

  (*path) = paths[0];
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectCharacteristicDescriptorValueWorker(HANDLE service_handle, scoped_refptr<btle::Device> device, scoped_refptr<btle::Service> service, scoped_refptr<btle::Characteristic> characteristic, scoped_refptr<btle::Descriptor> descriptor, std::string* error) {
  USHORT required_length;
  HRESULT hr = BluetoothGATTGetDescriptorValue(
      service_handle,
      &descriptor->info(),
      0,
      NULL,
      &required_length,
      BLUETOOTH_GATT_FLAG_NONE);
  if (NoDataResult(hr, required_length)) {
    return true;
  }

  if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) {
    std::ostringstream string_stream;
    string_stream << "Error getting descriptor value";
    *error = string_stream.str();
    return false;
  }

  scoped_ptr<BTH_LE_GATT_DESCRIPTOR_VALUE> value(reinterpret_cast<BTH_LE_GATT_DESCRIPTOR_VALUE*>(new UINT8[required_length]));
  RtlZeroMemory(value.get(), required_length);
  value.get()->DataSize = required_length;

  ULONG actual_length = required_length;
  hr = BluetoothGATTGetDescriptorValue(
      service_handle,
      &descriptor->info(),
      actual_length,
      value.get(),
      &required_length,
      BLUETOOTH_GATT_FLAG_NONE);
  if (!CheckSuccessulHResult(hr, actual_length, required_length, "BluetoothGATTGetDescriptorValue", error))
    return false;

  scoped_refptr<btle::DescriptorValue> descriptor_value(new btle::DescriptorValue(value));
  descriptor->set_value(descriptor_value);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectCharacteristicDescriptorValue(scoped_refptr<btle::Device> device, scoped_refptr<btle::Service> service, scoped_refptr<btle::Characteristic> characteristic, scoped_refptr<btle::Descriptor> descriptor, std::string* error) {
  std::wstring path;
  if (!TryGetDeviceServicePath(device, service->info().ServiceUuid, &path, error))
    return false;

  if (path.empty())
    return true;

  HANDLE service_handle = CreateFile(path.c_str(), /*GENERIC_WRITE | */GENERIC_READ, NULL, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (service_handle == INVALID_HANDLE_VALUE) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    std::ostringstream string_stream;
    string_stream << "Error opening device '" << to_std_string(path) << "': hr=" << hr << ".";
    *error = string_stream.str();
    return false;
  }

  scoped_handle<HANDLE> handle(service_handle);
  if (!CollectCharacteristicDescriptorValueWorker(service_handle, device, service, characteristic, descriptor, error)) {
    return false;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectCharacteristicDescriptors(HANDLE device_handle, scoped_refptr<btle::Device> device, scoped_refptr<btle::Service> service, scoped_refptr<btle::Characteristic> characteristic, std::string* error) {
  USHORT required_count;
  HRESULT hr = BluetoothGATTGetDescriptors(
      device_handle,
      &characteristic->info(),
      0,
      NULL,
      &required_count,
      BLUETOOTH_GATT_FLAG_NONE);
  if (NoDataResult(hr, required_count)) {
    return true;
  }

  if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) {
    std::ostringstream string_stream;
    string_stream << "Error getting descriptors";
    *error = string_stream.str();
    return false;
  }

  scoped_array<BTH_LE_GATT_DESCRIPTOR> descriptors(new BTH_LE_GATT_DESCRIPTOR[required_count]);
  USHORT actual_count = required_count;
  hr = BluetoothGATTGetDescriptors(
      device_handle,
      &characteristic->info(),
      actual_count,
      descriptors.get(),
      &required_count,
      BLUETOOTH_GATT_FLAG_NONE);
  if (!CheckSuccessulHResult(hr, actual_count, required_count, "BluetoothGATTGetDescriptors", error))
    return false;

  for(int i = 0; i < actual_count; i++) {
    BTH_LE_GATT_DESCRIPTOR& descriptor(descriptors.get()[i]);
    scoped_refptr<btle::Descriptor> descriptor_ptr(new btle::Descriptor(descriptor));
    characteristic->descriptors().push_back(descriptor_ptr);
    if (!CollectCharacteristicDescriptorValue(device, service, characteristic, descriptor_ptr, error))
      return false;
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
DeviceInfoResult CollectBluetoothLowEnergyDevice(HDEVINFO device_info_handle, int device_index, btle::DeviceInfo* device_info, std::string* error) {
  GUID BluetoothInterfaceGUID = GUID_BLUETOOTHLE_DEVICE_INTERFACE;

  // Enumerate device of LE_DEVICE interface class
  SP_DEVICE_INTERFACE_DATA device_interface_data = {0};
  device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
  BOOL success = SetupDiEnumDeviceInterfaces(
      device_info_handle, 
      NULL,
      (LPGUID)&BluetoothInterfaceGUID,
      (DWORD)device_index,
      &device_interface_data);
  if (!success) {
    DWORD last_error = GetLastError();
    if (last_error == ERROR_NO_MORE_ITEMS) {
      return kNoMoreDevices;
    }
    else {
      std::ostringstream string_stream;
      string_stream << "Error enumerating device interfaces: " << last_error;
      *error = string_stream.str();
      return kError;
    }
  }

  // Retrieve required # of bytes for interface details
  ULONG required_length = 0;
  success = SetupDiGetDeviceInterfaceDetail(
      device_info_handle,
      &device_interface_data,
      NULL,
      0, 
      &required_length, 
      NULL);
  if (!CheckInsufficientBuffer(success, "SetupDiGetDeviceInterfaceDetail", error))
    return kError;

  scoped_array<UINT8> interface_data(new UINT8[required_length]);
  RtlZeroMemory(interface_data.get(), required_length);

  PSP_DEVICE_INTERFACE_DETAIL_DATA device_interface_detail_data = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(interface_data.get());
  device_interface_detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

  SP_DEVINFO_DATA device_info_data = {0};
  device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  ULONG actual_length = required_length;
  success = SetupDiGetDeviceInterfaceDetail(
      device_info_handle,
      &device_interface_data,
      device_interface_detail_data,
      actual_length,
      &required_length, 
      &device_info_data);
  if (!CheckSuccessulResult(success, actual_length, required_length, "SetupDiGetDeviceInterfaceDetail", error))
    return kError;

  device_info->path = std::wstring(device_interface_detail_data->DevicePath);

  if (!CollectDeviceInstanceId(device_info_handle, device_info_data, device_info, error)) {
    return kError;
  }
  if (!CollectDeviceFriendlyName(device_info_handle, device_info_data, device_info, error)) {
    return kError;
  }
  if (!CollectDeviceBluetoothAddress(device_info_handle, device_info_data, device_info, error)) {
    return kError;
  }

  std::cout << "Device interface properties: " << to_std_string(device_info->path) << "\n";
  DisplayDeviceInterfaceProperties(device_info_handle, device_interface_data, error);

  std::cout << "Device properties: " << to_std_string(device_info->path) << "\n";
  DisplayDeviceProperties(device_info_handle, device_info_data, error);

  return kOk;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool ReadServiceCharacteristicValue(HANDLE service_handle, scoped_refptr<btle::Characteristic> characteristic, scoped_refptr<btle::CharacteristicValue>* characteristic_value, std::string* error) {
  ULONG flags = BLUETOOTH_GATT_FLAG_FORCE_READ_FROM_DEVICE;

  USHORT required_length;
  HRESULT hr = BluetoothGATTGetCharacteristicValue(
      service_handle,
      &characteristic->info(),
      0,
      NULL,
      &required_length,
      flags);
  if (NoDataResult(hr, required_length)) {
    return true;
  }

  if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) {
    std::ostringstream string_stream;
    string_stream << "Error getting characteristic value";
    *error = string_stream.str();
    return false;
  }

  scoped_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE> value(reinterpret_cast<BTH_LE_GATT_CHARACTERISTIC_VALUE*>(new UINT8[required_length]));
  RtlZeroMemory(value.get(), required_length);
  value.get()->DataSize = required_length;

  USHORT actual_length = required_length;
  hr = BluetoothGATTGetCharacteristicValue(
      service_handle,
      &characteristic->info(),
      actual_length,
      value.get(),
      &required_length,
      flags);
  if (!CheckSuccessulHResult(hr, actual_length, required_length, "BluetoothGATTGetCharacteristicValue", error))
    return false;

  (*characteristic_value) = scoped_refptr<btle::CharacteristicValue>(new btle::CharacteristicValue(value));
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectCharacteristicValueWorker(HANDLE service_handle, scoped_refptr<btle::Characteristic> characteristic, std::string* error) {
  USHORT required_length;
  HRESULT hr = BluetoothGATTGetCharacteristicValue(
      service_handle,
      &characteristic->info(),
      0,
      NULL,
      &required_length,
      BLUETOOTH_GATT_FLAG_NONE);
  if (NoDataResult(hr, required_length)) {
    return true;
  }

  if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) {
    std::ostringstream string_stream;
    string_stream << "Error getting characteristic value";
    *error = string_stream.str();
    return false;
  }

  scoped_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE> value(reinterpret_cast<BTH_LE_GATT_CHARACTERISTIC_VALUE*>(new UINT8[required_length]));
  RtlZeroMemory(value.get(), required_length);
  value.get()->DataSize = required_length;

  USHORT actual_length = required_length;
  hr = BluetoothGATTGetCharacteristicValue(
      service_handle,
      &characteristic->info(),
      actual_length,
      value.get(),
      &required_length,
      BLUETOOTH_GATT_FLAG_NONE);
  if (!CheckSuccessulHResult(hr, actual_length, required_length, "BluetoothGATTGetCharacteristicValue", error))
    return false;

  scoped_refptr<btle::CharacteristicValue> characteristic_value(new btle::CharacteristicValue(value));
  characteristic->set_value(characteristic_value);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool WriteServiceCharacteristicValue(HANDLE service_handle, scoped_refptr<btle::Characteristic> characteristic, scoped_refptr<btle::CharacteristicValue> value, std::string* error) {
  //BTH_LE_GATT_RELIABLE_WRITE_CONTEXT context;
  //HRESULT hr = BluetoothGATTBeginReliableWrite(
  //    service_handle,
  //    &context,
  //    BLUETOOTH_GATT_FLAG_NONE);
  //if (FAILED(hr)) {
  //  std::ostringstream string_stream;
  //  string_stream << "Error calling BluetoothGATTBeginReliableWrite: hr=" <<  hr;
  //  *error = string_stream.str();
  //  return false;
  //}

  HRESULT hr = BluetoothGATTSetCharacteristicValue(
      service_handle,
      &characteristic->info(),
      &value->info(),
      NULL,
      BLUETOOTH_GATT_FLAG_NONE);
  if (FAILED(hr)) {
    std::ostringstream string_stream;
    string_stream << "Error calling BluetoothGATTSetCharacteristicValue: hr=" <<  hr;
    *error = string_stream.str();
    return false;
  }

  //hr = BluetoothGATTEndReliableWrite(
  //    service_handle,
  //    context,
  //    BLUETOOTH_GATT_FLAG_NONE);
  //if (FAILED(hr)) {
  //  std::ostringstream string_stream;
  //  string_stream << "Error calling BluetoothGATTEndReliableWrite: hr=" <<  hr;
  //  *error = string_stream.str();
  //  return false;
  //}
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool OpenDeviceService(scoped_refptr<btle::Device> device, const BTH_LE_UUID& service_uuid, bool read_write, scoped_handle<HANDLE>* handle, std::string* error) {
  std::wstring path;
  if (!TryGetDeviceServicePath(device, service_uuid, &path, error))
    return false;

  if (path.empty())
    return true;

  DWORD desired_access = (read_write ? GENERIC_WRITE | GENERIC_READ : GENERIC_READ);
  HANDLE service_handle = CreateFile(path.c_str(),desired_access, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (service_handle == INVALID_HANDLE_VALUE) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    std::ostringstream string_stream;
    string_stream << "Error opening device '" << to_std_string(path) << "': hr=" << hr << ".";
    *error = string_stream.str();
    return false;
  }

  (*handle).set(service_handle);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectCharacteristicValue(scoped_refptr<btle::Device> device, scoped_refptr<btle::Service> service, scoped_refptr<btle::Characteristic> characteristic, std::string* error) {
  scoped_handle<HANDLE> service_handle;
  if (!OpenDeviceService(device, service->info().ServiceUuid, false, &service_handle, error))
    return false;

  if (!CollectCharacteristicValueWorker(service_handle.get(), characteristic, error)) {
    return false;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectServiceCharacteristics(HANDLE device_handle, scoped_refptr<btle::Device> device, scoped_refptr<btle::Service> service, std::string* error) {
  USHORT required_count;
  HRESULT hr = BluetoothGATTGetCharacteristics(device_handle, &service->info(), 0, NULL, &required_count, BLUETOOTH_GATT_FLAG_NONE);
  if (NoDataResult(hr, required_count)) {
    return true;
  }

  if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) {
    std::ostringstream string_stream;
    string_stream << "Error getting characteristics";
    *error = string_stream.str();
    return false;
  }

  scoped_array<BTH_LE_GATT_CHARACTERISTIC> gatt_characteristics(new BTH_LE_GATT_CHARACTERISTIC[required_count]);
  USHORT actual_count = required_count;
  hr = BluetoothGATTGetCharacteristics(device_handle, &service->info(), actual_count, gatt_characteristics.get(), &required_count, BLUETOOTH_GATT_FLAG_NONE);
  if (!CheckSuccessulHResult(hr, actual_count, required_count, "BluetoothGATTGetCharacteristics", error))
    return false;

  for(int i = 0; i < actual_count; i++) {
    BTH_LE_GATT_CHARACTERISTIC& gatt_characteristic(gatt_characteristics.get()[i]);
    scoped_refptr<btle::Characteristic> characteristic(new btle::Characteristic(gatt_characteristic));
    service->characteristics().push_back(characteristic);
    if (characteristic->info().IsReadable) {
      if (!CollectCharacteristicValue(device, service, characteristic, error)) {
        return false;
      }
    }

    if (!CollectCharacteristicDescriptors(device_handle, device, service,  characteristic, error)) {
      return false;
    }
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectDeviceService(HANDLE device_handle, scoped_refptr<btle::Device> device, scoped_refptr<btle::Service> service, std::string* error) {
  if (!CollectServiceCharacteristics(device_handle, device, service, error)) {
    return false;
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectDeviceServices(scoped_refptr<btle::Device> device, std::string* error) {
  std::wstring path = device->info().path;

  HANDLE device_handle = CreateFile(path.c_str(), GENERIC_WRITE | GENERIC_READ, NULL, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (device_handle == INVALID_HANDLE_VALUE) {
    DWORD last_error = GetLastError();
    std::ostringstream string_stream;
    string_stream << "Error opening device: " << last_error;
    *error = string_stream.str();
    return false;
  }

  scoped_handle<HANDLE> handle(device_handle);
  USHORT required_count;
  HRESULT hr = BluetoothGATTGetServices(handle.get(), 0, NULL, &required_count, BLUETOOTH_GATT_FLAG_NONE);
  if (NoDataResult(hr, required_count)) {
    return true;
  }

  if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) {
    std::ostringstream string_stream;
    string_stream << "Unexpected return value from BluetoothGATTGetServices: " << hr;
    *error = string_stream.str();
    return false;
  }

  scoped_array<BTH_LE_GATT_SERVICE> services(new BTH_LE_GATT_SERVICE[required_count]);
  USHORT actual_count = required_count;
  hr = BluetoothGATTGetServices(handle.get(), actual_count, services.get(), &required_count, BLUETOOTH_GATT_FLAG_NONE);
  if (!CheckSuccessulHResult(hr, actual_count, required_count, "BluetoothGATTGetServices", error))
    return false;

  for(int i = 0; i < actual_count; i++) {
    BTH_LE_GATT_SERVICE& service(services.get()[i]);
    scoped_refptr<btle::Service> service_ptr(new btle::Service(service));
    device->services().push_back(service_ptr);

    if (!CollectDeviceService(handle.get(), device, service_ptr, error)) {
      return false;
    }
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//
bool CollectBluetoothLowEnergyDevices(std::vector<scoped_refptr<btle::Device>>* devices, std::string* error) {
  // Open an enumerator for all present BTLE devices.
  scoped_hdevinfo dev_info_handle;
  HRESULT hr = dev_info_handle.OpenBluetoothLeDevices();

  if (FAILED(hr)) {
    std::ostringstream string_stream;
    string_stream << "Error enumerating device: HRESULT=" << hr;
    *error = string_stream.str();
    return false;
  }

  for(int i = 0; ; i++) {
    btle::DeviceInfo device_info;
    std::string device_info_error;
    DeviceInfoResult result = CollectBluetoothLowEnergyDevice(dev_info_handle.get(), i, &device_info, &device_info_error);
    
    if (result == kNoMoreDevices) {
      return true;
    }
    
    if (result == kError) {
      *error = device_info_error;
      return false;
    }

    devices->push_back(scoped_refptr<btle::Device>(new btle::Device(device_info)));
  }
}

bool CollectGattDevices(std::vector<scoped_refptr<btle::Device>>* devices, std::string* error) {
  if (!CollectBluetoothLowEnergyDevices(devices, error)) {
    return false;
  }

  for(std::vector<scoped_refptr<btle::Device>>::iterator it = devices->begin(); it != devices->end(); ++it) {
    if (!CollectDeviceServices(*it, error)) {
      return false;
    }
  }
  return true;
}

void DisplayGattDevices(const std::vector<scoped_refptr<btle::Device>>& devices) {
  std::string indent;
  for(std::vector<scoped_refptr<btle::Device>>::const_iterator it = devices.begin();
      it != devices.end();
      ++it) {
    std::cout << "Device:\n";
    indent = "  ";
    std::cout << indent << "Name:" << (*it)->info().friendly_name << "\n";
    std::cout << indent << "Address:" << BLUETOOTH_ADDRESS_TO_STRING((*it)->info().address) << "\n";
    std::cout << indent << "Path:" << to_std_string((*it)->info().path) << "\n";
    std::cout << indent << "Id:" << (*it)->info().id << "\n";

    for(std::vector<scoped_refptr<btle::Service>>::iterator service = (*it)->services().begin();
        service != (*it)->services().end();
        ++service) {

      indent = "  ";
      std::cout << indent << "Service:\n";
      indent = "    ";
      std::cout << indent << "AttributeHandle:" << (*service)->info().AttributeHandle << "\n";
      std::cout << indent << "ServiceUuid:" << btle::SERVICE_UUID_TO_STRING((*service)->info().ServiceUuid).c_str() << "\n";

      for(std::vector<scoped_refptr<btle::Characteristic>>::iterator characteristic = (*service)->characteristics().begin();
          characteristic != (*service)->characteristics().end();
          ++characteristic) {

        indent = "    ";
        std::cout << indent << "Characteristic:\n";
        indent = "      ";
        std::cout << indent << "AttributeHandle:" << (*characteristic)->info().AttributeHandle << "\n";
        std::cout << indent << "CharacteristicUuid:" << btle::CHARACTERISTIC_UUID_TO_STRING((*characteristic)->info().CharacteristicUuid).c_str() << "\n";
        std::cout << indent << "CharacteristicValueHandle:" << (*characteristic)->info().CharacteristicValueHandle << "\n";
        std::cout << indent << "HasExtendedProperties:" << btle::BOOLEAN_TO_STRING((*characteristic)->info().HasExtendedProperties) << "\n";
        std::cout << indent << "IsBroadcastable:" << btle::BOOLEAN_TO_STRING((*characteristic)->info().IsBroadcastable) << "\n";
        std::cout << indent << "IsIndicatable:" << btle::BOOLEAN_TO_STRING((*characteristic)->info().IsIndicatable) << "\n";
        std::cout << indent << "IsNotifiable:" << btle::BOOLEAN_TO_STRING((*characteristic)->info().IsNotifiable) << "\n";
        std::cout << indent << "IsReadable:" << btle::BOOLEAN_TO_STRING((*characteristic)->info().IsReadable) << "\n";
        std::cout << indent << "IsSignedWritable:" << btle::BOOLEAN_TO_STRING((*characteristic)->info().IsSignedWritable) << "\n";
        std::cout << indent << "IsWritable:" << btle::BOOLEAN_TO_STRING((*characteristic)->info().IsWritable) << "\n";
        std::cout << indent << "IsWritableWithoutResponse:" << btle::BOOLEAN_TO_STRING((*characteristic)->info().IsWritableWithoutResponse) << "\n";
        std::cout << indent << "ServiceHandle:" << (*characteristic)->info().ServiceHandle << "\n";
        std::cout << indent << "Value:\n";
        indent = "        ";
        if ((*characteristic)->value()) {
          std::cout << indent << "DataSize:" << (*characteristic)->value()->info().DataSize << "\n";
          std::stringstream stream;
          stream << std::hex << std::setfill('0') ;
          for (ULONG i = 0; i < (*characteristic)->value()->info().DataSize; i++) {
            stream << std::setw(2) << (int)(*characteristic)->value()->info().Data[i];
          }
          std::cout << indent << "Data:" << stream.str() << "\n";
        }
        else {
          std::cout << indent << "(none)" << "\n";
        }

        for(std::vector<scoped_refptr<btle::Descriptor>>::iterator descriptor = (*characteristic)->descriptors().begin();
            descriptor != (*characteristic)->descriptors().end();
            ++descriptor) {

          indent = "      ";
          std::cout << indent << "Descriptor:\n";
          indent = "        ";
          std::cout << indent << "AttributeHandle:" << (*descriptor)->info().AttributeHandle << "\n";
          std::cout << indent << "CharacteristicHandle:" << (*descriptor)->info().CharacteristicHandle << "\n";
          std::cout << indent << "DescriptorType:" << btle::BTH_LE_GATT_DESCRIPTOR_TYPE_TO_STRING((*descriptor)->info().DescriptorType) << "\n";
          std::cout << indent << "DescriptorUuid:" << btle::DESCRIPTOR_UUID_TO_STRING((*descriptor)->info().DescriptorUuid) << "\n";
          std::cout << indent << "ServiceHandle:" << (*descriptor)->info().ServiceHandle << "\n";
          std::cout << indent << "Value:\n";
          indent = "          ";
          if ((*descriptor)->value()) {
            std::cout << indent << "DescriptorType:" << btle::BTH_LE_GATT_DESCRIPTOR_TYPE_TO_STRING((*descriptor)->value()->info().DescriptorType) << "\n";
            std::cout << indent << "DescriptorUuid:" << btle::DESCRIPTOR_UUID_TO_STRING((*descriptor)->value()->info().DescriptorUuid) << "\n";
            std::cout << indent << "DataSize:" << (*descriptor)->value()->info().DataSize << "\n";
            std::stringstream stream;
            stream << std::hex << std::setfill('0') ;
            for (ULONG i = 0; i < (*descriptor)->value()->info().DataSize; i++) {
              stream << std::setw(2) << (int)(*descriptor)->value()->info().Data[i];
            }
            if ((*descriptor)->value()->info().DescriptorType == CharacteristicUserDescription) {
              stream << " (\"" << to_std_string(std::wstring(reinterpret_cast<wchar_t*>(&(*descriptor)->value()->info().Data), (*descriptor)->value()->info().DataSize / sizeof(wchar_t))) << "\")";
            }
            std::cout << indent << "Data:" << stream.str() << "\n";

            std::cout << indent << "CharacteristicExtendedProperties:\n";
            indent = "            ";
            std::cout << indent << "IsAuxiliariesWritable:" << btle::BOOLEAN_TO_STRING((*descriptor)->value()->info().CharacteristicExtendedProperties.IsAuxiliariesWritable) << "\n";
            std::cout << indent << "IsReliableWriteEnabled:" << btle::BOOLEAN_TO_STRING((*descriptor)->value()->info().CharacteristicExtendedProperties.IsReliableWriteEnabled) << "\n";
            indent = "          ";

            std::cout << indent << "CharacteristicFormat:\n";
            indent = "            ";
            std::cout << indent << "Description:" << btle::BTH_LE_UUID_TO_STRING((*descriptor)->value()->info().CharacteristicFormat.Description) << "\n";
            std::cout << indent << "Exponent:" << (int)(*descriptor)->value()->info().CharacteristicFormat.Exponent << "\n";
            std::cout << indent << "Format:" << (int)(*descriptor)->value()->info().CharacteristicFormat.Format << "\n";
            std::cout << indent << "NameSpace:" << (int)(*descriptor)->value()->info().CharacteristicFormat.NameSpace << "\n";
            std::cout << indent << "Unit:" << btle::BTH_LE_UUID_TO_STRING((*descriptor)->value()->info().CharacteristicFormat.Unit) << "\n";
            indent = "          ";

            std::cout << indent << "ClientCharacteristicConfiguration:\n";
            indent = "            ";
            std::cout << indent << "IsSubscribeToIndication:" << btle::BOOLEAN_TO_STRING((*descriptor)->value()->info().ClientCharacteristicConfiguration.IsSubscribeToIndication) << "\n";
            std::cout << indent << "IsSubscribeToNotification:" << btle::BOOLEAN_TO_STRING((*descriptor)->value()->info().ClientCharacteristicConfiguration.IsSubscribeToNotification) << "\n";
            indent = "            ";

            std::cout << indent << "ServerCharacteristicConfiguration:\n";
            indent = "              ";
            std::cout << indent << "IsBroadcast:" << btle::BOOLEAN_TO_STRING((*descriptor)->value()->info().ServerCharacteristicConfiguration.IsBroadcast) << "\n";
          }
          else {
            std::cout << indent << "(none)" << "\n";
          }
        }
      }
    }
  }
}

namespace ti_sensor_tag {

VOID Callback(
    _In_ BTH_LE_GATT_EVENT_TYPE EventType,
    _In_ PVOID EventOutParameter,
    _In_opt_ PVOID Context
) {
}

bool RegisterEvent(HANDLE service_handle, scoped_refptr<btle::Characteristic> characteristic, std::string* error) {
 BLUETOOTH_GATT_VALUE_CHANGED_EVENT_REGISTRATION* reg = new  BLUETOOTH_GATT_VALUE_CHANGED_EVENT_REGISTRATION();
 reg->NumCharacteristics = 0x1;
 reg->Characteristics[0] = characteristic->info();
 BLUETOOTH_GATT_EVENT_HANDLE  event_handle;
 HRESULT hr = BluetoothGATTRegisterEvent(
    service_handle,
    CharacteristicValueChangedEvent,
    reg,
    Callback,
    NULL,
    &event_handle,
    BLUETOOTH_GATT_FLAG_NONE);
  if (FAILED(hr)) {
    std::ostringstream string_stream;
    string_stream << "Error calling BluetoothGATTSetCharacteristicValue: hr=" <<  hr;
    *error = string_stream.str();
    return false;
  }

  return true;
}

double GetAmbientTempToDouble(scoped_refptr<btle::CharacteristicValue> value) {
  UINT8 low = value->info().Data[2];
  INT8 high = value->info().Data[3];

  int raw_value = low + (high << 8);
  return (double)raw_value / (double)128.0;
}

double GetObjectTempToDouble(scoped_refptr<btle::CharacteristicValue> value) {
  UINT8 low = value->info().Data[0];
  INT8 high = value->info().Data[1];

  int raw_value = low + (high << 8);
  double ambient = GetAmbientTempToDouble(value);


  double Vobj2 = (double)raw_value;
  Vobj2 *= 0.00000015625;

  double Tdie2 = ambient + 273.15;

  const double S0 = 6.4E-14;            // Calibration factor
  const double a1 = 1.75E-3;
  const double a2 = -1.678E-5;
  const double b0 = -2.94E-5;
  const double b1 = -5.7E-7;
  const double b2 = 4.63E-9;
  const double c2 = 13.4;
  const double Tref = 298.15;

  double S = S0*(1+a1*(Tdie2 - Tref)+a2*pow((Tdie2 - Tref),2));
  double Vos = b0 + b1*(Tdie2 - Tref) + b2*pow((Tdie2 - Tref),2);
  double fObj = (Vobj2 - Vos) + c2*pow((Vobj2 - Vos),2);
  double tObj = pow(pow(Tdie2,4) + (fObj/S),.25);
  tObj = (tObj - 273.15);

  return tObj;
}

void MonitorTemp(scoped_refptr<btle::Device> device) {
  BTH_LE_UUID service_uuid = btle::TO_BTH_LE_UUID(btle::IR_Temperature_Service);
  BTH_LE_UUID temp_config_characteristic_uuid = btle::TO_BTH_LE_UUID(btle::IR_Temperature_Config);
  BTH_LE_UUID temp_data_characteristic_uuid = btle::TO_BTH_LE_UUID(btle::IR_Temperature_Data);

  scoped_refptr<btle::Service> service = device->FindService(service_uuid);
  if (!service) {
    std::cout << "Can't find service " << btle::SERVICE_UUID_TO_STRING(service_uuid) << "\n";
    return;
  }

  scoped_handle<HANDLE> service_handle;
  std::string error;
  if (!OpenDeviceService(device, service->info().ServiceUuid, true/*read_write*/, &service_handle, &error)) {
    std::cout << error << "\n";
    return;
  }

  scoped_refptr<btle::Characteristic> temp_config_characteristic = service->FindCharacteristic(temp_config_characteristic_uuid);
  if (!temp_config_characteristic) {
    std::cout << "Can't find characteristic " << btle::CHARACTERISTIC_UUID_TO_STRING(temp_config_characteristic_uuid) << "\n";
    return;
  }

  // Write "0x01" to start temperature measurements
  scoped_refptr<btle::CharacteristicValue> value(new btle::CharacteristicValue());
  value->SetByte(0x01);
  if (!WriteServiceCharacteristicValue(service_handle.get(), temp_config_characteristic, value, &error)) {
    std::cout << error << "\n";
    return;
  }

  // Read "Tempp Data" values
  scoped_refptr<btle::Characteristic> temp_data_characteristic = service->FindCharacteristic(temp_data_characteristic_uuid);
  if (!temp_data_characteristic) {
    std::cout << "Can't find characteristic " << btle::CHARACTERISTIC_UUID_TO_STRING(temp_data_characteristic_uuid) << "\n";
    return;
  }

  //if (!RegisterEvent(service_handle.get(), temp_data_characteristic, &error)) {
  //  std::cout << error << "\n";
  //  return;
  //}

  service_handle.set(NULL);

  for(int i = 0; i < 200; i++) {
    scoped_handle<HANDLE> service_handle2;
    std::string error;
    if (!OpenDeviceService(device, service->info().ServiceUuid, false/*read_write*/, &service_handle2, &error)) {
      std::cout << error << "\n";
      return;
    }

    scoped_refptr<btle::CharacteristicValue> cur_value(new btle::CharacteristicValue());
    if (!ReadServiceCharacteristicValue(service_handle2.get(), temp_data_characteristic, &cur_value, &error)) {
      std::cout << error << "\n";
      return;
    }
    
    std::cout << "Ambient Temp: " << GetAmbientTempToDouble(cur_value) << " C, Object Temp:" << GetObjectTempToDouble(cur_value) << " C" << "\n";
    //cout << "Temp data" << GetObjectToDouble(cur_value) << "\n";
    Sleep(500);
  }
  //btle::
}

void MonitorTemp(const std::vector<scoped_refptr<btle::Device>>& devices) {
  for(std::vector<scoped_refptr<btle::Device>>::const_iterator it = devices.begin();
      it != devices.end();
      ++it) {
    if ((*it)->info().friendly_name == "TI BLE Sensor Tag") {
      MonitorTemp((*it));
    }
  }
}

}  // ti_sensor_tag

int _tmain(int argc, _TCHAR* argv[]) {
  std::vector<scoped_refptr<btle::Device>> devices;
  std::string error;
  if (!CollectGattDevices(&devices, &error)) {
    printf("Error: %s\n", error.c_str());
    return -1;
  }

  DisplayGattDevices(devices);

  // TI Sensor Tag IR
  ti_sensor_tag::MonitorTemp(devices);

  return 0;
}
