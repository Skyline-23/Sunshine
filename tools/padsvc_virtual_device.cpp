/**
 * @file tools/padsvc_virtual_device.cpp
 * @brief Null virtual DualSense device backend for the pad service.
 */

// standard includes
#include <iostream>
#include <memory>

// local includes
#include "padsvc_virtual_device.h"

namespace padsvc {
  namespace protocol = platf::pad_service_protocol;

  class null_virtual_device_backend_t final: public virtual_device_backend_t {
  public:
    int create(const protocol::create_dualsense_device_t &packet) override {
      std::cout << "padsvc/device: create slot=" << packet.header.global_index
                << " client=" << static_cast<int>(packet.client_relative_index) << "\n";
      return 0;
    }

    void destroy(std::int32_t global_index) override {
      std::cout << "padsvc/device: destroy slot=" << global_index << "\n";
    }

    void update_state(const protocol::update_state_t &packet) override {
      std::cout << "padsvc/device: state slot=" << packet.header.global_index << "\n";
    }

    void update_touch(const protocol::update_touch_t &packet) override {
      std::cout << "padsvc/device: touch slot=" << packet.header.global_index << "\n";
    }

    void update_motion(const protocol::update_motion_t &packet) override {
      std::cout << "padsvc/device: motion slot=" << packet.header.global_index << "\n";
    }

    void update_battery(const protocol::update_battery_t &packet) override {
      std::cout << "padsvc/device: battery slot=" << packet.header.global_index << "\n";
    }
  };

  std::unique_ptr<virtual_device_backend_t> make_virtual_device_backend() {
    return std::make_unique<null_virtual_device_backend_t>();
  }
}  // namespace padsvc
