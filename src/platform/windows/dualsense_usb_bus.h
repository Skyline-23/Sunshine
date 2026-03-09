/**
 * @file src/platform/windows/dualsense_usb_bus.h
 * @brief Shared control contract for the Windows DualSense USB bus device.
 */
#pragma once

// platform includes
#include <Windows.h>

// standard includes
#include <cstdint>

namespace platf::dualsense_usb_bus {
  constexpr std::uint32_t version = 1;
  constexpr auto device_path = LR"(\\.\SunshineDualSenseBus)";

  constexpr auto ioctl_base = 0x900;

  constexpr auto ioctl_get_version = CTL_CODE(FILE_DEVICE_UNKNOWN, ioctl_base + 0x00, METHOD_BUFFERED, FILE_ANY_ACCESS);
  constexpr auto ioctl_create_device = CTL_CODE(FILE_DEVICE_UNKNOWN, ioctl_base + 0x01, METHOD_BUFFERED, FILE_ANY_ACCESS);
  constexpr auto ioctl_destroy_device = CTL_CODE(FILE_DEVICE_UNKNOWN, ioctl_base + 0x02, METHOD_BUFFERED, FILE_ANY_ACCESS);
  constexpr auto ioctl_update_state = CTL_CODE(FILE_DEVICE_UNKNOWN, ioctl_base + 0x03, METHOD_BUFFERED, FILE_ANY_ACCESS);
  constexpr auto ioctl_update_touch = CTL_CODE(FILE_DEVICE_UNKNOWN, ioctl_base + 0x04, METHOD_BUFFERED, FILE_ANY_ACCESS);
  constexpr auto ioctl_update_motion = CTL_CODE(FILE_DEVICE_UNKNOWN, ioctl_base + 0x05, METHOD_BUFFERED, FILE_ANY_ACCESS);
  constexpr auto ioctl_update_battery = CTL_CODE(FILE_DEVICE_UNKNOWN, ioctl_base + 0x06, METHOD_BUFFERED, FILE_ANY_ACCESS);

  struct version_t {
    std::uint32_t version;
  };

  struct create_device_t {
    std::uint32_t version;
    std::int32_t global_index;
    std::uint8_t client_relative_index;
    std::uint8_t type;
    std::uint16_t capabilities;
    std::uint32_t supported_buttons;
  };

  struct destroy_device_t {
    std::uint32_t version;
    std::int32_t global_index;
  };

  struct update_state_t {
    std::uint32_t version;
    std::int32_t global_index;
    std::uint32_t button_flags;
    std::uint8_t lt;
    std::uint8_t rt;
    std::int16_t ls_x;
    std::int16_t ls_y;
    std::int16_t rs_x;
    std::int16_t rs_y;
  };

  struct update_touch_t {
    std::uint32_t version;
    std::int32_t global_index;
    std::uint8_t event_type;
    std::uint8_t reserved[3];
    std::uint32_t pointer_id;
    float x;
    float y;
    float pressure;
  };

  struct update_motion_t {
    std::uint32_t version;
    std::int32_t global_index;
    std::uint8_t motion_type;
    std::uint8_t reserved[3];
    float x;
    float y;
    float z;
  };

  struct update_battery_t {
    std::uint32_t version;
    std::int32_t global_index;
    std::uint8_t state;
    std::uint8_t percentage;
    std::uint8_t reserved[2];
  };
}  // namespace platf::dualsense_usb_bus
