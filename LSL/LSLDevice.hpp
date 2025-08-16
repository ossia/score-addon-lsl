#pragma once
#include <Device/Protocol/DeviceInterface.hpp>

namespace Protocols
{
class LSLDevice final : public Device::OwningDeviceInterface
{
public:
  LSLDevice(const Device::DeviceSettings& settings);
  ~LSLDevice();

  bool reconnect() override;
  void disconnect() override;
};
}