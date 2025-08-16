#include "lsl_protocol.hpp"

#include "lsl_context.hpp"

#include <ossia/network/base/parameter_data.hpp>
#include <ossia/network/common/value_bounding.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/value/format_value.hpp>

#include <boost/algorithm/string.hpp>

#include <iomanip>
#include <sstream>

namespace lsl_protocol
{

lsl_protocol::lsl_protocol(std::shared_ptr<lsl_context> lsl)
    : protocol_base{flags{}}
    , m_context{lsl}
{
}

lsl_protocol::~lsl_protocol()
{
  stop();
}

bool lsl_protocol::push(const ossia::net::parameter_base& param, const ossia::value& v)
{
  // Find which outlet this parameter belongs to
  std::lock_guard<std::mutex> lock(m_outlets_mutex);

  for (auto& [uid, outlet] : m_active_outlets)
  {
    auto it = std::find(
        outlet.parameters.begin(), outlet.parameters.end(), &param);
    if (it != outlet.parameters.end())
    {
      size_t channel_index = std::distance(outlet.parameters.begin(), it);

      // Update the stored value for this channel
      if (channel_index < outlet.current_values.size())
      {
        outlet.current_values[channel_index] = v;
        
        // Send the complete current measurement using the existing typed push
        if (outlet.outlet)
        {
          try
          {
            push_typed_sample(uid, outlet.current_values);
            return true;
          }
          catch (const std::exception& e)
          {
            ossia::logger().error("LSL push error: {}", e.what());
          }
        }
      }
    }
  }

  return false;
}

bool lsl_protocol::push_raw(const ossia::net::full_parameter_data& data)
{
  // Forward to regular push
  return false; // FIXME push(data.address, data.value);
}

bool lsl_protocol::pull(ossia::net::parameter_base& param)
{
  // LSL is push-based, pulling will return the last known value
  // The actual values are updated in the streaming thread
  return true;
}

bool lsl_protocol::observe(ossia::net::parameter_base& param, bool enable)
{
  // LSL parameters are always observed when streaming is enabled
  return true;
}

bool lsl_protocol::update(ossia::net::node_base& node_base)
{
  // Nothing to update - LSL is push-based
  return true;
}

void lsl_protocol::set_device(ossia::net::device_base& dev)
{
  m_device = &dev;
}

void lsl_protocol::start_discovery()
{
  if (!m_context)
  {
    ossia::logger().error("No LSL context available");
    return;
  }

  // Start the streaming thread
  m_running = true;
  m_streaming_thread = std::thread(&lsl_protocol::streaming_thread_function, this);
}

void lsl_protocol::stop()
{
  // Stop the streaming thread
  m_running = false;
  if (m_streaming_thread.joinable())
  {
    m_streaming_thread.join();
  }
  
  // Clean up all inlets
  {
    std::lock_guard<std::mutex> lock(m_inlets_mutex);
    for (auto& [uid, inlet] : m_active_inlets)
    {
      if(inlet.sensor)
        this->m_device->get_root_node().remove_child(*inlet.sensor);
    }
    m_active_inlets.clear();
  }
  
  // Clean up all outlets
  {
    std::lock_guard<std::mutex> lock(m_outlets_mutex);
    m_active_outlets.clear();
  }
}

bool lsl_protocol::subscribe_to_stream(const std::string& stream_uid)
{
  std::lock_guard<std::mutex> lock(m_inlets_mutex);
  
  // Check if already subscribed
  if (m_active_inlets.find(stream_uid) != m_active_inlets.end())
  {
    ossia::logger().warn("Already subscribed to stream: {}", stream_uid);
    return true;
  }
  
  // Get stream info from context
  auto streams = m_context->get_current_streams();
  auto it = streams.find(stream_uid);
  if (it == streams.end())
  {
    ossia::logger().error("Stream not found: {}", stream_uid);
    return false;
  }
  
  const auto& stream_info = it->second;
  
  try
  {
    // Create inlet
    inlet_data inlet;
    inlet.stream_info = stream_info;
    
    // Resolve the stream by UID
    std::vector<lsl::stream_info> results = lsl::resolve_stream("uid", stream_uid, 1, 2.0);
    if (results.empty())
    {
      ossia::logger().error("Failed to resolve stream: {}", stream_uid);
      return false;
    }

    inlet.inlet = std::make_unique<lsl::stream_inlet>(results[0]);
    inlet.last_samples.resize(stream_info.channel_count);
    inlet.last_update = std::chrono::steady_clock::now();
    
    // Create node hierarchy
    m_active_inlets[stream_uid] = std::move(inlet);
    create_node_hierarchy_for_stream(stream_info);
    return true;
  }
  catch (const std::exception& e)
  {
    ossia::logger().error("Failed to subscribe to stream {}: {}", 
                                   stream_uid, e.what());
    return false;
  }
}

std::string lsl_protocol::create_outlet(
    const lsl::stream_info& info,
    const std::vector<lsl_channel_info>& channel_info)
{
  std::lock_guard<std::mutex> lock(m_outlets_mutex);
  
  try
  {
    // Create outlet
    outlet_data outlet_data;
    outlet_data.outlet = std::make_unique<lsl::stream_outlet>(info);
    
    // Get the UID
    std::string uid = info.uid();
    
    if (channel_info.empty())
    {
      // Generate channel info for outlet (we create our own structure)
      outlet_data.format = info.channel_format();
      outlet_data.channel_info.reserve(info.channel_count());
      outlet_data.current_values.resize(info.channel_count(), ossia::value{0.0f});

      for (int i = 0; i < info.channel_count(); ++i)
      {
        lsl_channel_info ch_info;
        ch_info.name
            = "ch" + std::to_string(i + 1); // Default naming for outlets
        ch_info.lsl_format = info.channel_format();
        ch_info.ossia_type = lsl_format_to_ossia_type(info.channel_format());
        outlet_data.channel_info.push_back(ch_info);
      }
    }
    else
    {
      // Use provided channel info
      outlet_data.format = info.channel_format();
      outlet_data.channel_info = channel_info;
      outlet_data.current_values.resize(channel_info.size(), ossia::value{0.0f});
    }
    
    // Create node hierarchy for the outlet
    if (m_device)
    {
      auto& root = m_device->get_root_node();
      auto outlet_node = root.create_child("outlet_" + uid);
      ossia::net::set_description(*outlet_node, info.name());

      // Create parameters for each channel
      outlet_data.parameters.reserve(outlet_data.channel_info.size());
      
      for (size_t i = 0; i < outlet_data.channel_info.size(); ++i)
      {
        const auto& ch_info = outlet_data.channel_info[i];
        auto param_node = outlet_node->create_child(ch_info.name);
        auto param = param_node->create_parameter(ch_info.ossia_type);
        
        if (!ch_info.unit.empty())
          param->set_unit(ossia::parse_pretty_unit(ch_info.unit));
          
        if (ch_info.domain != ossia::domain{})
          param->set_domain(ch_info.domain);
          
        param->set_access(ossia::access_mode::SET);
        
        outlet_data.parameters.push_back(param);
      }
    }
    
    m_active_outlets[uid] = std::move(outlet_data);
    
    ossia::logger().info("Created outlet: {} ({})", info.name(), uid);
    return uid;
  }
  catch (const std::exception& e)
  {
    ossia::logger().error("Failed to create outlet: {}", e.what());
    return "";
  }
}

void lsl_protocol::destroy_outlet(const std::string& outlet_uid)
{
  std::lock_guard<std::mutex> lock(m_outlets_mutex);
  
  auto it = m_active_outlets.find(outlet_uid);
  if (it != m_active_outlets.end())
  {
    // Remove node hierarchy
    if (m_device)
    {
      auto& root = m_device->get_root_node();
      auto outlet_node = root.find_child("outlet_" + outlet_uid);
      if (outlet_node)
      {
        root.remove_child(*outlet_node);
      }
    }

    m_active_outlets.erase(it);
  }
}

void lsl_protocol::push_typed_sample(const std::string& outlet_uid, 
                                    const std::vector<ossia::value>& values)
{
  auto it = m_active_outlets.find(outlet_uid);
  if (it == m_active_outlets.end() || !it->second.outlet)
    return;
    
  auto& outlet = it->second;
  
  if (values.size() != outlet.channel_info.size())
  {
    ossia::logger().error(
        "Sample size mismatch: expected {}, got {}", 
        outlet.channel_info.size(), values.size());
    return;
  }
  
  try
  {
    switch (outlet.format)
    {
      case lsl::cf_float32:
      {
        thread_local std::vector<float> sample;
        sample.clear();
        sample.reserve(values.size());
        for (const auto& v : values)
          sample.push_back(ossia::convert<float>(v));
        outlet.outlet->push_sample(sample);
        break;
      }
      case lsl::cf_double64:
      {
        thread_local std::vector<double> sample;
        sample.clear();
        sample.reserve(values.size());
        for (const auto& v : values)
          sample.push_back(ossia::convert<double>(v));
        outlet.outlet->push_sample(sample);
        break;
      }
      case lsl::cf_int32:
      {
        thread_local std::vector<int32_t> sample;
        sample.clear();
        sample.reserve(values.size());
        for (const auto& v : values)
          sample.push_back(ossia::convert<int>(v));
        outlet.outlet->push_sample(sample);
        break;
      }
      case lsl::cf_int16:
      {
        thread_local std::vector<int16_t> sample;
        sample.clear();
        sample.reserve(values.size());
        for (const auto& v : values)
          sample.push_back(static_cast<int16_t>(ossia::convert<int>(v)));
        outlet.outlet->push_sample(sample);
        break;
      }
      case lsl::cf_string:
      {
        thread_local std::vector<std::string> sample;
        sample.clear();
        sample.reserve(values.size());
        for (const auto& v : values)
        {
          if (v.get_type() == ossia::val_type::STRING)
            sample.push_back(*v.target<std::string>());
          else
            sample.push_back(ossia::convert<std::string>(v));
        }
        outlet.outlet->push_sample(sample);
        break;
      }
      default:
        ossia::logger().warn(
            "Unsupported LSL channel format: {}",
            static_cast<int>(outlet.format));
    }
  }
  catch (const std::exception& e)
  {
    ossia::logger().error("Failed to push sample: {}", e.what());
  }
}

void lsl_protocol::streaming_thread_function()
{
  while (m_running)
  {
    if (m_streaming_enabled)
    {
      std::lock_guard<std::mutex> lock(m_inlets_mutex);
      
      for (auto& [uid, inlet] : m_active_inlets)
      {
        process_inlet_samples(inlet);
      }
    }

    // Small sleep to prevent busy waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

void lsl_protocol::process_inlet_samples(inlet_data& inlet)
{
  if (!inlet.inlet || inlet.parameters.empty())
    return;

  try
  {
    switch(inlet.stream_info.channel_format)
    {
      case lsl::cf_float32: {
        std::vector<float> sample(inlet.stream_info.channel_count);
        if(inlet.inlet->pull_sample(sample, 0.0) != 0.0)
        {
          for(size_t i = 0; i < sample.size() && i < inlet.parameters.size(); ++i)
          {
            if(inlet.parameters[i])
            {
              inlet.parameters[i]->push_value(sample[i]);
              inlet.last_samples[i] = sample[i];
            }
          }
          inlet.last_update = std::chrono::steady_clock::now();
        }
        break;
      }
      case lsl::cf_double64: {
        std::vector<double> sample(inlet.stream_info.channel_count);
        if(inlet.inlet->pull_sample(sample, 0.0) != 0.0)
        {
          for(size_t i = 0; i < sample.size() && i < inlet.parameters.size(); ++i)
          {
            if(inlet.parameters[i])
            {
              inlet.parameters[i]->push_value(sample[i]);
              inlet.last_samples[i] = (float)sample[i];
            }
          }
          inlet.last_update = std::chrono::steady_clock::now();
        }
        break;
      }
      case lsl::cf_int32: {
        std::vector<int32_t> sample(inlet.stream_info.channel_count);
        if(inlet.inlet->pull_sample(sample, 0.0) != 0.0)
        {
          for(size_t i = 0; i < sample.size() && i < inlet.parameters.size(); ++i)
          {
            if(inlet.parameters[i])
            {
              inlet.parameters[i]->push_value(sample[i]);
              inlet.last_samples[i] = sample[i];
            }
          }
          inlet.last_update = std::chrono::steady_clock::now();
        }
        break;
      }
      case lsl::cf_int16: {
        std::vector<int16_t> sample(inlet.stream_info.channel_count);
        if(inlet.inlet->pull_sample(sample, 0.0) != 0.0)
        {
          for(size_t i = 0; i < sample.size() && i < inlet.parameters.size(); ++i)
          {
            if(inlet.parameters[i])
            {
              inlet.parameters[i]->push_value(static_cast<int>(sample[i]));
              inlet.last_samples[i] = static_cast<int>(sample[i]);
            }
          }
          inlet.last_update = std::chrono::steady_clock::now();
        }
        break;
      }
      case lsl::cf_string: {
        std::vector<std::string> sample(inlet.stream_info.channel_count);
        if(inlet.inlet->pull_sample(sample, 0.0) != 0.0)
        {
          for(size_t i = 0; i < sample.size() && i < inlet.parameters.size(); ++i)
          {
            if(inlet.parameters[i])
            {
              inlet.parameters[i]->push_value(sample[i]);
              inlet.last_samples[i] = sample[i];
            }
          }
          inlet.last_update = std::chrono::steady_clock::now();
        }
        break;
      }
      default:
        break;
    }
  }
  catch (const std::exception& e)
  {
    ossia::logger().error("Error processing samples for stream {}: {}", 
                                   inlet.stream_info.uid, e.what());
  }
}

void lsl_protocol::create_node_hierarchy_for_stream(const lsl_stream_data& stream)
{
  if (!m_device)
    return;
    
  auto& root = m_device->get_root_node();
  
  // Create stream node
  auto name = stream.name.empty() ? "stream" : stream.name;
  auto stream_node = root.create_child(name);
  ossia::net::set_description(*stream_node, stream.uid);

  // Get the inlet data
  auto& inlet = m_active_inlets[stream.uid];
  inlet.sensor = stream_node;
  inlet.parameters.reserve(stream.channels.size());

  // Create parameter nodes for each channel
  for (const auto& channel : stream.channels)
  {
    auto param_node = stream_node->create_child(channel.name);
    auto param = param_node->create_parameter(channel.ossia_type);
    
    if (!channel.unit.empty())
      param->set_unit(ossia::parse_pretty_unit(channel.unit));
      
    if (channel.domain != ossia::domain{})
      param->set_domain(channel.domain);
      
    param->set_access(ossia::access_mode::GET);

    inlet.parameters.push_back(param);
  }
}

ossia::val_type lsl_protocol::lsl_format_to_ossia_type(lsl::channel_format_t fmt) const
{
  switch (fmt)
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

ossia::domain lsl_protocol::get_domain_for_lsl_format(lsl::channel_format_t fmt) const
{
  switch (fmt)
  {
    case lsl::cf_int8:
      return ossia::make_domain(-128, 127);
    case lsl::cf_int16:
      return ossia::make_domain(-32768, 32767);
    default:
      return {};
  }
}

}
