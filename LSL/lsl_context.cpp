#include "lsl_context.hpp"
#include "lsl_protocol.hpp"

#include <ossia/network/value/value.hpp>
#include <ossia/network/domain/domain.hpp>

#include <boost/algorithm/string.hpp>

namespace lsl_protocol
{

ossia::val_type lsl_format_to_ossia_type(lsl::channel_format_t fmt)
{
  switch(fmt)
  {
    case lsl::cf_float32:
    case lsl::cf_double64:
      return ossia::val_type::FLOAT;
    case lsl::cf_int8:
    case lsl::cf_int16:
    case lsl::cf_int32:
    case lsl::cf_int64:
      return ossia::val_type::INT;
    case lsl::cf_string:
      return ossia::val_type::STRING;
    default:
      return ossia::val_type::FLOAT;
  }
}

ossia::domain get_domain_for_lsl_format(lsl::channel_format_t fmt)
{
  switch(fmt)
  {
    case lsl::cf_int8:
      return ossia::make_domain(-128, 127);
    case lsl::cf_int16:
      return ossia::make_domain(-32768, 32767);
    default:
      return {};
  }
}

lsl_context::lsl_context()
    : m_streams_buffer{{}}
{
  // Start discovery thread
  m_discovery_thread = std::thread(&lsl_context::discovery_thread, this);
}

lsl_context::~lsl_context()
{
  m_running = false;
  
  if (m_discovery_thread.joinable())
    m_discovery_thread.join();
}

lsl_stream_map lsl_context::get_current_streams()
{
  m_streams_buffer.consume(m_previous_streams_consumer_thread);
  return m_previous_streams_consumer_thread;
}

void lsl_context::register_stream_callback(stream_callback cb)
{
  std::lock_guard<std::mutex> lock(m_callbacks_mutex);
  m_callbacks.push_back(cb);
}

void lsl_context::unregister_stream_callback(stream_callback cb)
{
  std::lock_guard<std::mutex> lock(m_callbacks_mutex);
  // Note: std::function doesn't support == operator, so we can't directly remove
  // In practice, we might need a more sophisticated callback management system
  m_callbacks.clear();
}

void lsl_context::discovery_thread()
{

  while(m_running)
  {
    try
    {
      // Discover all streams
      std::vector<lsl::stream_info> streams = lsl::resolve_streams(2.0);

      lsl_stream_map new_streams;

      for(auto& info : streams)
      {
        lsl_stream_data stream;
        stream.uid = info.uid();
        stream.name = info.name();
        stream.type = info.type();
        stream.channel_count = info.channel_count();
        stream.nominal_srate = info.nominal_srate();
        stream.channel_format = info.channel_format();
        
        // Extract metadata
        stream.source_id = info.source_id();
        stream.hostname = info.hostname();

        // Get manufacturer info if available
        lsl::xml_element desc = info.desc();
        if(!desc.first_child().empty())
        {
          lsl::xml_element manufacturer = desc.child("manufacturer");
          if(!manufacturer.empty())
            stream.manufacturer = manufacturer.child_value();
            
          lsl::xml_element model = desc.child("model");
          if(!model.empty())
            stream.model = model.child_value();
            
          lsl::xml_element serial = desc.child("serial_number");
          if(!serial.empty())
            stream.serial_number = serial.child_value();
        }
        
        // Parse channel information
        stream.channels = parse_channel_info(info);

        new_streams[stream.uid] = stream;
      }

      // Update the buffer
      update_streams_in_buffer(new_streams);
    }
    catch (const std::exception& e)
    {
    }

    // Sleep for discovery interval
    for (int i = 0; i < m_discovery_interval.count() * 10 && m_running; ++i)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

void lsl_context::update_streams_in_buffer(lsl_stream_map new_streams)
{
  if(m_previous_streams_producer_thread != new_streams)
  {
    m_previous_streams_producer_thread = new_streams;

    m_streams_buffer.produce(new_streams);

    std::vector<stream_callback> callbacks_copy;
    {
      std::lock_guard<std::mutex> lock(m_callbacks_mutex);
      callbacks_copy = m_callbacks;
    }

    for(const auto& cb : callbacks_copy)
    {
      if (cb)
        cb();
    }
  }
}

std::vector<lsl_channel_info> lsl_context::parse_channel_info(lsl::stream_info& info)
{
  std::vector<lsl_channel_info> channels;
  channels.reserve(info.channel_count());
  
  lsl::xml_element desc = info.desc();
  lsl::xml_element channels_elem = desc.child("channels");

  if(!channels_elem.empty())
  {
    // Parse detailed channel information from XML
    int ch_idx = 0;
    for(lsl::xml_element ch = channels_elem.child("channel"); !ch.empty();
        ch = ch.next_sibling("channel"), ++ch_idx)
    {
      lsl_channel_info ch_info;
      
      // Get channel name
      lsl::xml_element label = ch.child("label");
      if(!label.empty())
        ch_info.name = label.child_value();
      else
        ch_info.name = "ch" + std::to_string(ch_idx + 1);
      
      // Get unit
      lsl::xml_element unit = ch.child("unit");
      if(!unit.empty())
        ch_info.unit = unit.child_value();
      
      // Get type (for validation)
      lsl::xml_element type = ch.child("type");
      std::string type_str = !type.empty() ? type.child_value() : "";

      // Set format and ossia type
      ch_info.lsl_format = info.channel_format();
      ch_info.ossia_type = lsl_format_to_ossia_type(info.channel_format());
      
      // Parse range if available
      lsl::xml_element range = ch.child("range");
      if(!range.empty())
      {
        lsl::xml_element minimum = range.child("minimum");
        lsl::xml_element maximum = range.child("maximum");

        if(!minimum.empty() && !maximum.empty())
        {
          try
          {
            double min_val = std::stod(minimum.child_value());
            double max_val = std::stod(maximum.child_value());
            ch_info.domain = ossia::make_domain(min_val, max_val);
          }
          catch (...)
          {
            // Ignore parsing errors
          }
        }
      }
      
      channels.push_back(ch_info);
    }
  }
  
  // If no channel info in XML, create default channels
  if (channels.empty())
  {
    for (int i = 0; i < info.channel_count(); ++i)
    {
      lsl_channel_info ch_info;
      ch_info.name = "ch" + std::to_string(i + 1);
      ch_info.lsl_format = info.channel_format();
      ch_info.ossia_type = lsl_format_to_ossia_type(info.channel_format());
      ch_info.domain = get_domain_for_lsl_format(info.channel_format());
      channels.push_back(ch_info);
    }
  }
  
  return channels;
}
}
