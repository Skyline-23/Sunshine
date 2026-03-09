/**
 * @file src/platform/windows/pad_service_client.h
 * @brief Declarations for the Windows pad service client boundary.
 */
#pragma once

// standard includes
#include <functional>
#include <memory>
#include <string>

// local includes
#include "src/platform/common.h"

namespace platf {
  struct pad_service_status_t {
    bool available {};
    bool installed {};
    bool service_running {};
    std::string version;
    std::string reason;
  };

  using gamepad_feedback_sink_t = std::function<void(gamepad_feedback_msg_t)>;

  class pad_service_client_t {
  public:
    virtual ~pad_service_client_t() = default;

    virtual int init() = 0;
    virtual pad_service_status_t status() const = 0;
    virtual int create_dualsense_device(const gamepad_id_t &id, const gamepad_arrival_t &metadata) = 0;
    virtual void destroy_dualsense_device(int global_index) = 0;
    virtual void update_state(int global_index, const gamepad_state_t &gamepad_state) = 0;
    virtual void update_touch(int global_index, const gamepad_touch_t &touch) = 0;
    virtual void update_motion(int global_index, const gamepad_motion_t &motion) = 0;
    virtual void update_battery(int global_index, const gamepad_battery_t &battery) = 0;
    virtual void set_feedback_sink(int global_index, gamepad_feedback_sink_t sink) = 0;
  };

  std::unique_ptr<pad_service_client_t> make_pad_service_client();
}  // namespace platf
