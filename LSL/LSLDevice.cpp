#include "LSLDevice.hpp"

#include "LSLSpecificSettings.hpp"
#include "lsl_context.hpp"
#include "lsl_protocol.hpp"

#include <ossia/detail/unique_instance.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <QDebug>

namespace Protocols
{

LSLDevice::LSLDevice(const Device::DeviceSettings& settings)
    : OwningDeviceInterface{settings}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

LSLDevice::~LSLDevice()
{
}

bool LSLDevice::reconnect()
{
  disconnect();

  try
  {
    // Get LSL-specific settings
    const auto& lsl_settings = settings().deviceSpecificSettings.value<LSLSpecificSettings>();

    // Create the ossia device

    static const auto context = ossia::unique_instance<lsl_protocol::lsl_context>();
    auto protocol = std::make_unique<lsl_protocol::lsl_protocol>(context);

    // Configure the protocol
    if(!lsl_settings.streamTypeFilter.empty())
    {
      protocol->set_stream_type_filter(lsl_settings.streamTypeFilter);
    }

    // Create ossia device
    m_dev = std::make_unique<ossia::net::generic_device>(
        std::move(protocol), settings().name.toStdString());

    // Get the protocol back
    auto* lsl_proto = static_cast<lsl_protocol::lsl_protocol*>(&m_dev->get_protocol());

    // Create outlets from configuration
    for (const auto& sensorConfig : lsl_settings.outboundSensors)
    {
      // Determine LSL format from data type
      lsl::channel_format_t format = lsl::cf_float32;
      ossia::val_type ossia_type = ossia::val_type::FLOAT;
      
      if (sensorConfig.dataType == "int")
      {
        format = lsl::cf_int32;
        ossia_type = ossia::val_type::INT;
      }
      else if (sensorConfig.dataType == "string")
      {
        format = lsl::cf_string;
        ossia_type = ossia::val_type::STRING;
      }
      
      // Extract stream info
      lsl::stream_info streamInfo(
          sensorConfig.streamName.toStdString(), 
          sensorConfig.streamType.toStdString(),
          static_cast<int>(sensorConfig.channelNames.size()), 
          sensorConfig.sampleRate,
          format,
          sensorConfig.sourceId.toStdString());

      // Convert channel info
      std::vector<lsl_protocol::lsl_channel_info> channelInfo;
      for (const auto& channelName : sensorConfig.channelNames)
      {
        lsl_protocol::lsl_channel_info info;
        info.name = channelName;
        info.lsl_format = format;
        info.ossia_type = ossia_type;
        channelInfo.push_back(info);
      }

      // Create the outlet
      lsl_proto->create_outlet(streamInfo, channelInfo);
    }

    // Start discovery
    lsl_proto->start_discovery();

    // Subscribe to configured streams
    for (const auto& uid : lsl_settings.subscribedStreams)
    {
      lsl_proto->subscribe_to_stream(uid);
    }

    deviceChanged(nullptr, m_dev.get());
    return true;
  }
  catch (const std::exception& e)
  {
    qDebug() << "LSL Device connection error:" << e.what();
    return false;
  }
}

void LSLDevice::disconnect()
{
  if (m_dev)
  {
    auto* proto = static_cast<lsl_protocol::lsl_protocol*>(&m_dev->get_protocol());
    proto->stop();
  }

  OwningDeviceInterface::disconnect();
}

}
