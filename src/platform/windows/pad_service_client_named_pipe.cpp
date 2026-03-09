/**
 * @file src/platform/windows/pad_service_client_named_pipe.cpp
 * @brief Named-pipe transport skeleton for the Windows pad service client boundary.
 */
#define WINVER 0x0A00

// platform includes
#include <Windows.h>

// standard includes
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

// local includes
#include "pad_service_client.h"
#include "pad_service_protocol.h"
#include "src/logging.h"

namespace platf {
  using namespace std::literals;

  class named_pipe_pad_service_client_t final: public pad_service_client_t {
  public:
    int init() override {
      status_cache = probe_status();
      if (status_cache.available) {
        maybe_start_feedback_pump();
      }
      return 0;
    }

    ~named_pipe_pad_service_client_t() override {
      shutdown_requested = true;

      if (feedback_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(feedback_pipe);
        feedback_pipe = INVALID_HANDLE_VALUE;
      }

      if (feedback_thread.joinable()) {
        feedback_thread.join();
      }
    }

    pad_service_status_t status() const override {
      return status_cache;
    }

    int create_dualsense_device(const gamepad_id_t &id, const gamepad_arrival_t &metadata) override {
      pad_service_protocol::create_dualsense_device_t command {
        {
          pad_service_protocol::version,
          static_cast<std::uint16_t>(pad_service_protocol::command_e::create_dualsense_device),
          static_cast<std::uint16_t>(sizeof(pad_service_protocol::create_dualsense_device_t) - sizeof(pad_service_protocol::command_header_t)),
          id.globalIndex,
        },
        id.clientRelativeIndex,
        metadata.type,
        metadata.capabilities,
        metadata.supportedButtons,
      };
      return send_command(command);
    }

    void destroy_dualsense_device(int global_index) override {
      pad_service_protocol::destroy_dualsense_device_t command {
        {
          pad_service_protocol::version,
          static_cast<std::uint16_t>(pad_service_protocol::command_e::destroy_dualsense_device),
          static_cast<std::uint16_t>(sizeof(pad_service_protocol::destroy_dualsense_device_t) - sizeof(pad_service_protocol::command_header_t)),
          global_index,
        }
      };
      (void) send_command(command);
    }

    void update_state(int global_index, const gamepad_state_t &gamepad_state) override {
      pad_service_protocol::update_state_t command {
        {
          pad_service_protocol::version,
          static_cast<std::uint16_t>(pad_service_protocol::command_e::update_state),
          static_cast<std::uint16_t>(sizeof(pad_service_protocol::update_state_t) - sizeof(pad_service_protocol::command_header_t)),
          global_index,
        },
        gamepad_state.buttonFlags,
        gamepad_state.lt,
        gamepad_state.rt,
        gamepad_state.lsX,
        gamepad_state.lsY,
        gamepad_state.rsX,
        gamepad_state.rsY,
      };
      (void) send_command(command);
    }

    void update_touch(int global_index, const gamepad_touch_t &touch) override {
      pad_service_protocol::update_touch_t command {
        {
          pad_service_protocol::version,
          static_cast<std::uint16_t>(pad_service_protocol::command_e::update_touch),
          static_cast<std::uint16_t>(sizeof(pad_service_protocol::update_touch_t) - sizeof(pad_service_protocol::command_header_t)),
          global_index,
        },
        touch.eventType,
        {0, 0, 0},
        touch.pointerId,
        touch.x,
        touch.y,
        touch.pressure,
      };
      (void) send_command(command);
    }

    void update_motion(int global_index, const gamepad_motion_t &motion) override {
      pad_service_protocol::update_motion_t command {
        {
          pad_service_protocol::version,
          static_cast<std::uint16_t>(pad_service_protocol::command_e::update_motion),
          static_cast<std::uint16_t>(sizeof(pad_service_protocol::update_motion_t) - sizeof(pad_service_protocol::command_header_t)),
          global_index,
        },
        motion.motionType,
        {0, 0, 0},
        motion.x,
        motion.y,
        motion.z,
      };
      (void) send_command(command);
    }

    void update_battery(int global_index, const gamepad_battery_t &battery) override {
      pad_service_protocol::update_battery_t command {
        {
          pad_service_protocol::version,
          static_cast<std::uint16_t>(pad_service_protocol::command_e::update_battery),
          static_cast<std::uint16_t>(sizeof(pad_service_protocol::update_battery_t) - sizeof(pad_service_protocol::command_header_t)),
          global_index,
        },
        battery.state,
        battery.percentage,
        {0, 0},
      };
      (void) send_command(command);
    }

    void set_feedback_sink(int global_index, gamepad_feedback_sink_t sink) override {
      if (global_index < 0 || global_index >= feedback_sinks.size()) {
        return;
      }

      feedback_sinks[global_index] = std::move(sink);
    }

  private:
    template<typename T>
    bool decode_feedback_packet(const pad_service_protocol::feedback_header_t &header, const std::vector<std::uint8_t> &payload, T &packet) {
      const auto expected_payload_size = sizeof(T) - sizeof(pad_service_protocol::feedback_header_t);
      if (payload.size() != expected_payload_size) {
        return false;
      }

      std::memcpy(&packet.header, &header, sizeof(header));
      if (expected_payload_size > 0) {
        std::memcpy(reinterpret_cast<std::uint8_t *>(&packet) + sizeof(header), payload.data(), expected_payload_size);
      }
      return true;
    }

    template<typename T>
    int send_command(const T &command) {
      if (!status_cache.available) {
        return -1;
      }

      auto pipe = open_pipe();
      if (pipe == INVALID_HANDLE_VALUE) {
        status_cache = probe_status();
        return -1;
      }

      DWORD bytes_written = 0;
      const auto *data = reinterpret_cast<const char *>(&command);
      const auto command_size = static_cast<DWORD>(sizeof(T));
      const auto write_ok = WriteFile(pipe, data, command_size, &bytes_written, nullptr);
      CloseHandle(pipe);

      if (!write_ok || bytes_written != command_size) {
        BOOST_LOG(warning) << "Failed to send command to SunshinePadService named pipe"sv;
        status_cache.available = false;
        status_cache.service_running = false;
        if (status_cache.reason.empty()) {
          status_cache.reason = "gamepads.dualsense-usb-not-available";
        }
        return -1;
      }

      maybe_start_feedback_pump();
      return 0;
    }

    HANDLE open_pipe() const {
      return CreateFileW(
        pad_service_protocol::named_pipe_path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
      );
    }

    HANDLE open_feedback_pipe() const {
      return CreateFileW(
        pad_service_protocol::named_pipe_feedback_path,
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
      );
    }

    void maybe_start_feedback_pump() {
      if (feedback_thread.joinable() || shutdown_requested) {
        return;
      }

      auto pipe = open_feedback_pipe();
      if (pipe == INVALID_HANDLE_VALUE) {
        return;
      }

      feedback_pipe = pipe;
      feedback_thread = std::thread([this] {
        feedback_loop();
      });
    }

    void feedback_loop() {
      while (!shutdown_requested) {
        pad_service_protocol::feedback_header_t header {};
        DWORD bytes_read = 0;
        if (!ReadFile(feedback_pipe, &header, sizeof(header), &bytes_read, nullptr) || bytes_read != sizeof(header)) {
          break;
        }

        std::vector<std::uint8_t> payload(header.payload_size);
        if (!payload.empty()) {
          bytes_read = 0;
          if (!ReadFile(feedback_pipe, payload.data(), static_cast<DWORD>(payload.size()), &bytes_read, nullptr) || bytes_read != payload.size()) {
            break;
          }
        }

        dispatch_feedback(header, payload);
      }

      if (feedback_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(feedback_pipe);
        feedback_pipe = INVALID_HANDLE_VALUE;
      }
    }

    void dispatch_feedback(const pad_service_protocol::feedback_header_t &header, const std::vector<std::uint8_t> &payload) {
      switch (static_cast<pad_service_protocol::feedback_type_e>(header.type)) {
        case pad_service_protocol::feedback_type_e::rumble:
          {
            pad_service_protocol::rumble_t packet {};
            if (decode_feedback_packet(header, payload, packet)) {
              emit_feedback(header.global_index, gamepad_feedback_msg_t::make_rumble(header.client_relative_index, packet.lowfreq, packet.highfreq));
            }
          }
          break;
        case pad_service_protocol::feedback_type_e::rumble_triggers:
          {
            pad_service_protocol::rumble_triggers_t packet {};
            if (decode_feedback_packet(header, payload, packet)) {
              emit_feedback(header.global_index, gamepad_feedback_msg_t::make_rumble_triggers(header.client_relative_index, packet.left, packet.right));
            }
          }
          break;
        case pad_service_protocol::feedback_type_e::motion_report_rate:
          {
            pad_service_protocol::motion_report_rate_t packet {};
            if (decode_feedback_packet(header, payload, packet)) {
              emit_feedback(header.global_index, gamepad_feedback_msg_t::make_motion_event_state(header.client_relative_index, packet.motion_type, packet.report_rate));
            }
          }
          break;
        case pad_service_protocol::feedback_type_e::rgb_led:
          {
            pad_service_protocol::rgb_led_t packet {};
            if (decode_feedback_packet(header, payload, packet)) {
              emit_feedback(header.global_index, gamepad_feedback_msg_t::make_rgb_led(header.client_relative_index, packet.r, packet.g, packet.b));
            }
          }
          break;
        case pad_service_protocol::feedback_type_e::adaptive_triggers:
          {
            pad_service_protocol::adaptive_triggers_t packet {};
            if (decode_feedback_packet(header, payload, packet)) {
              emit_feedback(header.global_index, gamepad_feedback_msg_t::make_adaptive_triggers(header.client_relative_index, packet.event_flags, packet.type_left, packet.type_right, packet.left, packet.right));
            }
          }
          break;
      }
    }

    void emit_feedback(int global_index, gamepad_feedback_msg_t msg) {
      if (global_index < 0 || global_index >= feedback_sinks.size()) {
        return;
      }

      auto &sink = feedback_sinks[global_index];
      if (sink) {
        sink(std::move(msg));
      }
    }

    pad_service_status_t probe_status() const {
      if (WaitNamedPipeW(pad_service_protocol::named_pipe_path, 0)) {
        return {
          true,
          true,
          true,
          ""
        };
      }

      auto error = GetLastError();
      if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
        return {
          false,
          false,
          false,
          "gamepads.dualsense-usb-not-available"
        };
      }

      if (error == ERROR_SEM_TIMEOUT || error == ERROR_PIPE_BUSY) {
        return {
          false,
          true,
          true,
          "gamepads.dualsense-usb-busy"
        };
      }

      return {
        false,
        true,
        false,
        "gamepads.dualsense-usb-not-available"
      };
    }

    mutable pad_service_status_t status_cache {
      false,
      false,
      false,
      "gamepads.dualsense-usb-not-available"
    };
    std::array<gamepad_feedback_sink_t, MAX_GAMEPADS> feedback_sinks {};
    std::atomic_bool shutdown_requested {false};
    HANDLE feedback_pipe {INVALID_HANDLE_VALUE};
    std::thread feedback_thread;
  };

  std::unique_ptr<pad_service_client_t> make_pad_service_client() {
    return std::make_unique<named_pipe_pad_service_client_t>();
  }
}  // namespace platf
