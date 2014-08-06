#include <string>
#include <vector>

#include <bluetoothapis.h>
#include <bluetoothleapis.h>

#include "base.h"

namespace btle {

inline
bool operator==(const BTH_LE_UUID& x, const BTH_LE_UUID& y) {
  if (x.IsShortUuid) {
    return y.IsShortUuid && x.Value.ShortUuid == y.Value.ShortUuid;
  } else {
    return !y.IsShortUuid && x.Value.LongUuid == y.Value.LongUuid;
  }
}

struct DeviceInfo {
  DeviceInfo() {
    address.ullLong = BLUETOOTH_NULL_ADDRESS;
  }

  std::wstring path;
  std::string id;
  std::string friendly_name;
  BLUETOOTH_ADDRESS address;
};

class Device;
class Service;
class Characteristic;
class CharacteristicValue;
class Descriptor;
class DescriptorValue;

class Characteristic : public RefCounted<Characteristic> {
public:
  explicit Characteristic(const BTH_LE_GATT_CHARACTERISTIC& characteristic) : characteristic_(characteristic) {
  }

  const BTH_LE_GATT_CHARACTERISTIC& info() const { return characteristic_; }
  BTH_LE_GATT_CHARACTERISTIC& info() { return characteristic_; }

  const scoped_refptr<CharacteristicValue>& value() const { return value_; }
  void set_value(const scoped_refptr<CharacteristicValue>& value) { value_ = value; }

  const std::vector<scoped_refptr<Descriptor>>& descriptors() const { return descriptors_; }
  std::vector<scoped_refptr<Descriptor>>& descriptors() { return descriptors_; }

private:
  BTH_LE_GATT_CHARACTERISTIC characteristic_;
  scoped_refptr<CharacteristicValue> value_;
  std::vector<scoped_refptr<Descriptor>> descriptors_;
};

class Service : public RefCounted<Service> {
public:
  explicit Service(const BTH_LE_GATT_SERVICE& service) : service_(service) {
  }

  const BTH_LE_GATT_SERVICE& info() const { return service_; }
  BTH_LE_GATT_SERVICE& info() { return service_; }

  const std::vector<scoped_refptr<Characteristic>>& characteristics() const { return characteristics_; }
  std::vector<scoped_refptr<Characteristic>>& characteristics() { return characteristics_; }


  scoped_refptr<Characteristic> FindCharacteristic(const BTH_LE_UUID& uuid) {
    for(std::vector<scoped_refptr<Characteristic>>::const_iterator it = characteristics_.begin(); it != characteristics_.end(); it++) {
      if ((*it)->info().CharacteristicUuid == uuid)
        return (*it);
    }
    return scoped_refptr<Characteristic>();
  }

private:
  BTH_LE_GATT_SERVICE service_;
  std::vector<scoped_refptr<Characteristic>> characteristics_;
};

class Device : public RefCounted<Device> {
public:
  explicit Device(const DeviceInfo& device) : device_(device) {
  }

  const DeviceInfo& info() const { return device_; }

  const std::vector<scoped_refptr<Service>>& services() const { return services_; }
  std::vector<scoped_refptr<Service>>& services() { return services_; }

  scoped_refptr<Service> FindService(const BTH_LE_UUID& uuid) {
    for(std::vector<scoped_refptr<Service>>::const_iterator it = services_.begin(); it != services_.end(); it++) {
      if ((*it)->info().ServiceUuid == uuid)
        return (*it);
    }
    return scoped_refptr<Service>();
  }

private:
  DeviceInfo device_;
  std::vector<scoped_refptr<Service>> services_;
};

class CharacteristicValue : public RefCounted<CharacteristicValue> {
public:
  explicit CharacteristicValue() {
  }
  explicit CharacteristicValue(scoped_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE>& value) : value_(value.Pass()) {
  }

  const BTH_LE_GATT_CHARACTERISTIC_VALUE& info() const { return *value_.get(); }
  BTH_LE_GATT_CHARACTERISTIC_VALUE& info() { return *value_.get(); }

  void SetByte(UINT value)  {
    SetData(&value, sizeof(UINT8));
  }

  void SetData(UINT* data, size_t size) {
    size_t required_length = size + offsetof(BTH_LE_GATT_CHARACTERISTIC_VALUE, Data);

    BTH_LE_GATT_CHARACTERISTIC_VALUE* gatt_value = reinterpret_cast<BTH_LE_GATT_CHARACTERISTIC_VALUE*>(new UINT8[required_length]);
    gatt_value->DataSize = size;
    memcpy(gatt_value->Data, data, size);
    value_.set(gatt_value);
  }

private:
  scoped_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE> value_;
};

class Descriptor : public RefCounted<Descriptor> {
public:
  explicit Descriptor(const BTH_LE_GATT_DESCRIPTOR& descriptor) : descriptor_(descriptor) {
  }

  const BTH_LE_GATT_DESCRIPTOR& info() const { return descriptor_; }
  BTH_LE_GATT_DESCRIPTOR& info() { return descriptor_; }

  const scoped_refptr<DescriptorValue>& value() const { return value_; }
  void set_value(const scoped_refptr<DescriptorValue>& value) { value_ = value; }

private:
  BTH_LE_GATT_DESCRIPTOR descriptor_;
  scoped_refptr<DescriptorValue> value_;
};

class DescriptorValue : public RefCounted<DescriptorValue> {
public:
  explicit DescriptorValue(scoped_ptr<BTH_LE_GATT_DESCRIPTOR_VALUE>& value) : value_(value.Pass()) {
  }

  const BTH_LE_GATT_DESCRIPTOR_VALUE& info() const { return *value_.get(); }
  BTH_LE_GATT_DESCRIPTOR_VALUE& info() { return *value_.get(); }

private:
  scoped_ptr<BTH_LE_GATT_DESCRIPTOR_VALUE> value_;
};


}
