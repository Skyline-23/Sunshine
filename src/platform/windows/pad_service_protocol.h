/**
 * @file src/platform/windows/pad_service_protocol.h
 * @brief Wire protocol definitions for the Windows pad service boundary.
 */
#pragma once

// standard includes
#include <array>
#include <cstdint>

namespace platf::pad_service_protocol {
  constexpr std::uint32_t version = 1;
  constexpr auto named_pipe_path = LR"(\\.\pipe\SunshinePadService)";

  enum class command_e: std::uint16_t {
    create_dualsense_device = 1,
    destroy_dualsense_device = 2,
    update_state = 3,
    update_touch = 4,
    update_motion = 5,
    update_battery = 6,
  };

  enum class feedback_type_e: std::uint16_t {
    rumble = 1,
    rumble_triggers = 2,
    motion_report_rate = 3,
    rgb_led = 4,
    adaptive_triggers = 5,
  };

  struct command_header_t {
    std::uint32_t version;
    std::uint16_t command;
    std::uint16_t payload_size;
    std::int32_t global_index;
  };

  struct feedback_header_t {
    std::uint32_t version;
    std::uint16_t type;
    std::uint16_t payload_size;
    std::int32_t global_index;
    std::uint8_t client_relative_index;
    std::uint8_t reserved[3];
  };

  struct create_dualsense_device_t {
    command_header_t header;
    std::uint8_t client_relative_index;
    std::uint8_t type;
    std::uint16_t capabilities;
    std::uint32_t supported_buttons;
  };

  struct destroy_dualsense_device_t {
    command_header_t header;
  };

  struct update_state_t {
    command_header_t header;
    std::uint32_t button_flags;
    std::uint8_t lt;
    std::uint8_t rt;
    std::int16_t ls_x;
    std::int16_t ls_y;
    std::int16_t rs_x;
    std::int16_t rs_y;
  };

  struct update_touch_t {
    command_header_t header;
    std::uint8_t event_type;
    std::uint8_t reserved[3];
    std::uint32_t pointer_id;
    float x;
    float y;
    float pressure;
  };

  struct update_motion_t {
    command_header_t header;
    std::uint8_t motion_type;
    std::uint8_t reserved[3];
    float x;
    float y;
    float z;
  };

  struct update_battery_t {
    command_header_t header;
    std::uint8_t state;
    std::uint8_t percentage;
    std::uint8_t reserved[2];
  };

  struct rumble_t {
    feedback_header_t header;
    std::uint16_t lowfreq;
    std::uint16_t highfreq;
  };

  struct rumble_triggers_t {
    feedback_header_t header;
    std::uint16_t left;
    std::uint16_t right;
  };

  struct motion_report_rate_t {
    feedback_header_t header;
    std::uint16_t report_rate;
    std::uint8_t motion_type;
    std::uint8_t reserved;
  };

  struct rgb_led_t {
    feedback_header_t header;
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t reserved;
  };

  struct adaptive_triggers_t {
    feedback_header_t header;
    std::uint8_t event_flags;
    std::uint8_t type_left;
    std::uint8_t type_right;
    std::uint8_t reserved;
    std::array<std::uint8_t, 10> left;
    std::array<std::uint8_t, 10> right;
  };
}  // namespace platf::pad_service_protocol
