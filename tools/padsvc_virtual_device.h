/**
 * @file tools/padsvc_virtual_device.h
 * @brief Service-side virtual DualSense device backend interface.
 */
#pragma once

// standard includes
#include <memory>
#include <string>

// local includes
#include "src/platform/windows/pad_service_protocol.h"

namespace padsvc {
  struct virtual_device_status_t {
    bool available {};
    bool installed {};
    bool service_running {};
    std::string reason;
  };

  class virtual_device_backend_t {
  public:
    virtual ~virtual_device_backend_t() = default;

    virtual virtual_device_status_t status() const = 0;
    virtual int create(const platf::pad_service_protocol::create_dualsense_device_t &packet) = 0;
    virtual void destroy(std::int32_t global_index) = 0;
    virtual void update_state(const platf::pad_service_protocol::update_state_t &packet) = 0;
    virtual void update_touch(const platf::pad_service_protocol::update_touch_t &packet) = 0;
    virtual void update_motion(const platf::pad_service_protocol::update_motion_t &packet) = 0;
    virtual void update_battery(const platf::pad_service_protocol::update_battery_t &packet) = 0;
  };

  std::unique_ptr<virtual_device_backend_t> make_virtual_device_backend();
}  // namespace padsvc
