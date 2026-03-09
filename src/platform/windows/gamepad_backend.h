/**
 * @file src/platform/windows/gamepad_backend.h
 * @brief Declarations for Windows virtual gamepad backends.
 */
#pragma once

// standard includes
#include <memory>

// local includes
#include "src/platform/common.h"

namespace platf {
  class windows_gamepad_backend_t {
  public:
    virtual ~windows_gamepad_backend_t() = default;

    virtual int init() = 0;
    virtual int alloc_gamepad(const gamepad_id_t &id, const gamepad_arrival_t &metadata, feedback_queue_t feedback_queue) = 0;
    virtual void free_gamepad(int nr) = 0;
    virtual void update_gamepad(int nr, const gamepad_state_t &gamepad_state) = 0;
    virtual void touch_gamepad(const gamepad_touch_t &touch) = 0;
    virtual void motion_gamepad(const gamepad_motion_t &motion) = 0;
    virtual void battery_gamepad(const gamepad_battery_t &battery) = 0;
    virtual const std::vector<supported_gamepad_t> &supported_gamepads() const = 0;
  };

  std::unique_ptr<windows_gamepad_backend_t> make_dualsense_usb_gamepad_backend();
  std::unique_ptr<windows_gamepad_backend_t> make_gamepad_backend();
  std::unique_ptr<windows_gamepad_backend_t> make_vigem_gamepad_backend();
}  // namespace platf
