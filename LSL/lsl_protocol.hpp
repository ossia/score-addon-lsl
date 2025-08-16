#pragma once
#include <ossia/detail/logger.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/context_functions.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value.hpp>

#include <lsl_cpp.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

namespace lsl_protocol
{
class lsl_context;

// Channel information structure
struct lsl_channel_info
{
  std::string name;
  lsl::channel_format_t lsl_format{};
  ossia::val_type ossia_type{};
  ossia::domain domain;
  std::string unit;

  std::strong_ordering operator<=>(const lsl_channel_info&) const noexcept = default;
};

// Stream information structure
struct lsl_stream_data
{
  std::string uid;
  std::string name;
  std::string type;
  lsl::channel_format_t channel_format{};
  int channel_count = 0;
  double nominal_srate = 0.0;
  
  // Added metadata fields
  std::string source_id;
  std::string hostname;
  std::string manufacturer;
  std::string model;
  std::string serial_number;
  
  // Channel info
  std::vector<lsl_channel_info> channels;

  std::strong_ordering operator<=>(const lsl_stream_data&) const noexcept = default;
};


class lsl_protocol final : public ossia::net::protocol_base
{
public:
  explicit lsl_protocol(std::shared_ptr<lsl_context> lsl);
  ~lsl_protocol() override;

  bool pull(ossia::net::parameter_base&) override;
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override;
  bool push_raw(const ossia::net::full_parameter_data&) override;
  bool observe(ossia::net::parameter_base&, bool) override;
  bool update(ossia::net::node_base& node_base) override;

  void set_device(ossia::net::device_base& dev) override;

  // LSL-specific methods
  void start_discovery();
  void stop() override;

  // Stream management
  bool subscribe_to_stream(const std::string& stream_uid);
  void unsubscribe_from_stream(const std::string& stream_uid);
  std::vector<lsl_stream_data> get_available_streams() const;
  
  // Outlet management
  std::string create_outlet(
      const lsl::stream_info& info,
      const std::vector<lsl_channel_info>& channel_info = {});
  void destroy_outlet(const std::string& outlet_uid);
  void push_typed_sample(const std::string& outlet_uid, const std::vector<ossia::value>& values);

  // Enable/disable automatic streaming
  void set_streaming_enabled(bool enabled) { m_streaming_enabled = enabled; }
  bool is_streaming_enabled() const { return m_streaming_enabled; }

  // Configuration
  void set_stream_type_filter(const std::string& filter) { m_stream_type_filter = filter; }
  const std::string& get_stream_type_filter() const { return m_stream_type_filter; }


private:
  // Device reference
  ossia::net::device_base* m_device = nullptr;
  
  // Discovery
  std::shared_ptr<lsl_context> m_context;
  
  // Stream management
  struct inlet_data
  {
    std::unique_ptr<lsl::stream_inlet> inlet;
    lsl_stream_data stream_info;
    ossia::net::node_base* sensor{};
    std::vector<ossia::net::parameter_base*> parameters;
    std::vector<ossia::value> last_samples;
    std::chrono::steady_clock::time_point last_update;
  };
  
  std::unordered_map<std::string, inlet_data> m_active_inlets;
  std::mutex m_inlets_mutex;
  
  // Active outlets
  struct outlet_data
  {
    std::unique_ptr<lsl::stream_outlet> outlet;
    std::vector<ossia::net::parameter_base*> parameters;
    std::vector<lsl_channel_info> channel_info;
    lsl::channel_format_t format;
    std::vector<ossia::value> current_values; // Store complete measurement
  };
  
  std::unordered_map<std::string, outlet_data> m_active_outlets;
  std::mutex m_outlets_mutex;
  
  // Streaming thread
  std::thread m_streaming_thread;
  std::atomic<bool> m_streaming_enabled{true};
  std::atomic<bool> m_running{false};
  
  // Configuration
  std::string m_stream_type_filter; // Empty means all types
  
  // Helper methods
  void streaming_thread_function();
  void process_inlet_samples(inlet_data& inlet);
  void create_node_hierarchy_for_stream(const lsl_stream_data& stream);
  void remove_node_hierarchy_for_stream(const std::string& stream_uid);
  
  ossia::val_type lsl_format_to_ossia_type(lsl::channel_format_t fmt) const;
  ossia::domain get_domain_for_lsl_format(lsl::channel_format_t fmt) const;
  
  // Helper for finding node by stream UID
  ossia::net::node_base* find_stream_node(const std::string& stream_uid) const;
};

}
