#pragma once

#include <lsl_cpp.h>
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

}
