#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

#include <Explorer/DefaultProtocolFactory.hpp>

namespace Protocols
{
class LSLProtocolFactory final : public Protocols::DefaultProtocolFactory
{
  SCORE_CONCRETE("faf31a91-532b-48e0-ab9d-20232c9469e5")

public:
  QString prettyName() const noexcept override;
  QString category() const noexcept override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;

  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data,
      const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};
}
