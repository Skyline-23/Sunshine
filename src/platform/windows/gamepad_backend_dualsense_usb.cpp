/**
 * @file src/platform/windows/gamepad_backend_dualsense_usb.cpp
 * @brief Placeholder for the Windows Virtual USB DualSense backend.
 */
#define WINVER 0x0A00

// platform includes
#include <Windows.h>

// standard includes
#include <memory>
#include <vector>

// local includes
#include "gamepad_backend.h"
#include "pad_service_client.h"
#include "src/logging.h"

namespace platf {
  using namespace std::literals;

  struct dualsense_usb_device_context_t {
    bool allocated {};
    feedback_queue_t feedback_queue;
  };

  class dualsense_usb_backend_t final: public windows_gamepad_backend_t {
  public:
    int init() override {
      pad_service = make_pad_service_client();
      devices.resize(MAX_GAMEPADS);

      if (!pad_service) {
        status_cache = {
          false,
          false,
          false,
          "gamepads.dualsense-usb-not-available"
        };
      } else {
        (void) pad_service->init();
        status_cache = pad_service->status();
      }

      rebuild_supported_gamepads();
      return 0;
    }

    int alloc_gamepad(const gamepad_id_t &id, const gamepad_arrival_t &metadata, feedback_queue_t feedback_queue) override {
      if (!status_cache.available || !pad_service) {
        return -1;
      }

      auto &device = devices[id.globalIndex];
      if (device.allocated) {
        BOOST_LOG(warning) << "DualSense USB device already allocated for slot "sv << id.globalIndex;
        return -1;
      }

      if (pad_service->create_dualsense_device(id, metadata)) {
        return -1;
      }

      device.feedback_queue = std::move(feedback_queue);
      device.allocated = true;

      pad_service->set_feedback_sink(id.globalIndex, [queue = device.feedback_queue](gamepad_feedback_msg_t msg) {
        queue->raise(msg);
      });

      return 0;
    }

    void free_gamepad(int nr) override {
      if (!pad_service || nr < 0 || nr >= devices.size() || !devices[nr].allocated) {
        return;
      }

      pad_service->destroy_dualsense_device(nr);
      pad_service->set_feedback_sink(nr, {});
      devices[nr] = {};
    }

    void update_gamepad(int nr, const gamepad_state_t &gamepad_state) override {
      if (!pad_service || nr < 0 || nr >= devices.size() || !devices[nr].allocated) {
        return;
      }

      pad_service->update_state(nr, gamepad_state);
    }

    void touch_gamepad(const gamepad_touch_t &touch) override {
      if (!pad_service || touch.id.globalIndex < 0 || touch.id.globalIndex >= devices.size() || !devices[touch.id.globalIndex].allocated) {
        return;
      }

      pad_service->update_touch(touch.id.globalIndex, touch);
    }

    void motion_gamepad(const gamepad_motion_t &motion) override {
      if (!pad_service || motion.id.globalIndex < 0 || motion.id.globalIndex >= devices.size() || !devices[motion.id.globalIndex].allocated) {
        return;
      }

      pad_service->update_motion(motion.id.globalIndex, motion);
    }

    void battery_gamepad(const gamepad_battery_t &battery) override {
      if (!pad_service || battery.id.globalIndex < 0 || battery.id.globalIndex >= devices.size() || !devices[battery.id.globalIndex].allocated) {
        return;
      }

      pad_service->update_battery(battery.id.globalIndex, battery);
    }

    const std::vector<supported_gamepad_t> &supported_gamepads() const override {
      return supported_gamepads_cache;
    }

  private:
    void rebuild_supported_gamepads() {
      auto reason = status_cache.reason.empty() ? "gamepads.dualsense-usb-not-available" : status_cache.reason;
      supported_gamepads_cache = {
        supported_gamepad_t {"auto", true, ""},
        supported_gamepad_t {"x360", false, "gamepads.vigem-not-available"},
        supported_gamepad_t {"ds4", false, "gamepads.vigem-not-available"},
        supported_gamepad_t {"dualsense_usb", status_cache.available, status_cache.available ? "" : reason}
      };
    }

    std::unique_ptr<pad_service_client_t> pad_service;
    pad_service_status_t status_cache {};
    std::vector<dualsense_usb_device_context_t> devices;
    std::vector<supported_gamepad_t> supported_gamepads_cache;
  };

  std::unique_ptr<windows_gamepad_backend_t> make_dualsense_usb_gamepad_backend() {
    return std::make_unique<dualsense_usb_backend_t>();
  }
}  // namespace platf
