/**
 * @file src/platform/windows/pad_service_client_stub.cpp
 * @brief Stub implementation of the Windows pad service client boundary.
 */
#define WINVER 0x0A00

// platform includes
#include <Windows.h>

// standard includes
#include <memory>

// local includes
#include "pad_service_client.h"
#include "src/logging.h"

namespace platf {
  using namespace std::literals;

  class stub_pad_service_client_t final: public pad_service_client_t {
  public:
    int init() override {
      BOOST_LOG(warning) << "Pad service client is not implemented yet for DualSense USB"sv;
      return -1;
    }

    pad_service_status_t status() const override {
      return {
        false,
        false,
        false,
        "gamepads.dualsense-usb-not-available"
      };
    }

    int create_dualsense_device(const gamepad_id_t &, const gamepad_arrival_t &) override {
      return -1;
    }

    void destroy_dualsense_device(int) override {}
    void update_state(int, const gamepad_state_t &) override {}
    void update_touch(int, const gamepad_touch_t &) override {}
    void update_motion(int, const gamepad_motion_t &) override {}
    void update_battery(int, const gamepad_battery_t &) override {}
    void set_feedback_sink(int, gamepad_feedback_sink_t) override {}
  };

  std::unique_ptr<pad_service_client_t> make_pad_service_client() {
    return std::make_unique<stub_pad_service_client_t>();
  }
}  // namespace platf
