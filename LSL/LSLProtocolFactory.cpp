#include "LSLProtocolFactory.hpp"
#include "LSLDevice.hpp"
#include "LSLProtocolSettingsWidget.hpp"
#include "LSLSpecificSettings.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <QObject>

namespace Protocols
{

QString LSLProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("LSL");
}

QString LSLProtocolFactory::category() const noexcept
{
  return StandardCategories::osc;
}

Device::DeviceInterface* LSLProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new LSLDevice{settings};
}

const Device::DeviceSettings& LSLProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = static_concreteKey();
    s.name = "LSL";
    LSLSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* LSLProtocolFactory::makeSettingsWidget()
{
  return new LSLProtocolSettingsWidget;
}

QVariant LSLProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<LSLSpecificSettings>(visitor);
}

void LSLProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<LSLSpecificSettings>(data, visitor);
}

bool LSLProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  qDebug("bah?");
  return true;
}
}
