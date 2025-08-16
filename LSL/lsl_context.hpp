#pragma once
#include <ossia/detail/logger.hpp>
#include <ossia/detail/triple_buffer.hpp>
#include <ossia/network/context.hpp>

#include <lsl_cpp.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace lsl_protocol
{

struct lsl_channel_info;
struct lsl_stream_data;

using lsl_stream_map = std::unordered_map<std::string, lsl_stream_data>;

class lsl_context
{
public:
  lsl_context();
  ~lsl_context();

  // Get current streams (thread-safe)
  lsl_stream_map get_current_streams();

  // Register callback for stream changes
  using stream_callback = std::function<void()>;
  void register_stream_callback(stream_callback cb);
  void unregister_stream_callback(stream_callback cb);

private:
  void discovery_thread();
  void update_streams_in_buffer(lsl_stream_map new_streams);
  std::vector<lsl_channel_info> parse_channel_info(lsl::stream_info& info);

  mutable ossia::triple_buffer<lsl_stream_map> m_streams_buffer;
  
  std::thread m_discovery_thread;
  std::atomic<bool> m_running{true};
  
  std::vector<stream_callback> m_callbacks;
  std::mutex m_callbacks_mutex;

  std::chrono::seconds m_discovery_interval{2};

  lsl_stream_map m_previous_streams_producer_thread;
  lsl_stream_map m_previous_streams_consumer_thread;
};

}
