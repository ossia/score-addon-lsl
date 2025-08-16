#include "LSLSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>


// Sensor config serialization
template <>
void DataStreamReader::read(const Protocols::LSLSensorConfig& n)
{
  m_stream << n.streamName << n.streamType << n.sourceId << n.sampleRate << n.dataType << n.channelNames;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::LSLSensorConfig& n)
{
  m_stream >> n.streamName >> n.streamType >> n.sourceId >> n.sampleRate >> n.dataType >> n.channelNames;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::LSLSensorConfig& n)
{
  obj["StreamName"] = n.streamName;
  obj["StreamType"] = n.streamType;
  obj["SourceId"] = n.sourceId;
  obj["SampleRate"] = n.sampleRate;
  obj["DataType"] = n.dataType;
  obj["ChannelNames"] = n.channelNames;
}

template <>
void JSONWriter::write(Protocols::LSLSensorConfig& n)
{
  if (auto it = obj.tryGet("StreamName"))
    n.streamName <<= *it;
  if (auto it = obj.tryGet("StreamType"))
    n.streamType <<= *it;
  if (auto it = obj.tryGet("SourceId"))
    n.sourceId <<= *it;
  if (auto it = obj.tryGet("SampleRate"))
    n.sampleRate <<= *it;
  if (auto it = obj.tryGet("DataType"))
    n.dataType <<= *it;
  if (auto it = obj.tryGet("ChannelNames"))
    n.channelNames <<= *it;
}

// Main settings serialization
template <>
void DataStreamReader::read(const Protocols::LSLSpecificSettings& n)
{
  m_stream << n.streamTypeFilter << n.subscribedStreams << n.outboundSensors;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::LSLSpecificSettings& n)
{
  m_stream >> n.streamTypeFilter >> n.subscribedStreams >> n.outboundSensors;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::LSLSpecificSettings& n)
{
  obj["StreamTypeFilter"] = n.streamTypeFilter;
  obj["SubscribedStreams"] = n.subscribedStreams;
  obj["OutboundSensors"] = n.outboundSensors;
}

template <>
void JSONWriter::write(Protocols::LSLSpecificSettings& n)
{
  if (auto it = obj.tryGet("StreamTypeFilter"))
    n.streamTypeFilter <<= *it;
  if (auto it = obj.tryGet("SubscribedStreams"))
    n.subscribedStreams <<= *it;
  if (auto it = obj.tryGet("OutboundSensors"))
    n.outboundSensors <<= *it;
}