/**
 * @file src/platform/windows/gamepad_backend_dualsense_usb.cpp
 * @brief Placeholder for the Windows Virtual USB DualSense backend.
 */
#define WINVER 0x0A00

// platform includes
#include <Windows.h>

// standard includes
#include <memory>

// local includes
#include "gamepad_backend.h"
#include "src/logging.h"

namespace platf {
  using namespace std::literals;

  class dualsense_usb_backend_t final: public windows_gamepad_backend_t {
  public:
    int init() override {
      BOOST_LOG(warning) << "DualSense USB backend is not implemented yet on this branch stage"sv;
      return -1;
    }

    int alloc_gamepad(const gamepad_id_t &, const gamepad_arrival_t &, feedback_queue_t) override {
      return -1;
    }

    void free_gamepad(int) override {}
    void update_gamepad(int, const gamepad_state_t &) override {}
    void touch_gamepad(const gamepad_touch_t &) override {}
    void motion_gamepad(const gamepad_motion_t &) override {}
    void battery_gamepad(const gamepad_battery_t &) override {}

    const std::vector<supported_gamepad_t> &supported_gamepads() const override {
      static const std::vector gps {
        supported_gamepad_t {"auto", true, ""},
        supported_gamepad_t {"x360", false, "gamepads.vigem-not-available"},
        supported_gamepad_t {"ds4", false, "gamepads.vigem-not-available"},
        supported_gamepad_t {"dualsense_usb", false, "gamepads.dualsense-usb-not-available"}
      };

      return gps;
    }
  };

  std::unique_ptr<windows_gamepad_backend_t> make_dualsense_usb_gamepad_backend() {
    return std::make_unique<dualsense_usb_backend_t>();
  }
}  // namespace platf
