#pragma once
#include <score/tools/Metadata.hpp>
#include <QStringList>
#include <QMetaType>
#include <verdigris>

namespace Protocols
{

// Structure for sensor configuration
struct LSLSensorConfig
{
  QString streamName;
  QString streamType{"Measurement"};
  QString sourceId{"ossia_score"};
  double sampleRate{100.0};
  QString dataType{"float"}; // "float", "int", "string"
  std::vector<std::string> channelNames; // Simple list of channel names
};

struct LSLSpecificSettings
{
  std::string streamTypeFilter;                 // Empty means all types
  std::vector<std::string> subscribedStreams;   // UIDs of streams to subscribe to
  std::vector<LSLSensorConfig> outboundSensors; // Configured output sensors
};

}

Q_DECLARE_METATYPE(Protocols::LSLSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::LSLSpecificSettings)


Q_DECLARE_METATYPE(Protocols::LSLSensorConfig)
W_REGISTER_ARGTYPE(Protocols::LSLSensorConfig)
