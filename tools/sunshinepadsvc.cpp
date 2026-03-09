/**
 * @file tools/sunshinepadsvc.cpp
 * @brief Skeleton server for the Windows pad service named pipe endpoint.
 */
#define WIN32_LEAN_AND_MEAN

// platform includes
#include <Windows.h>

// standard includes
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// local includes
#include "third-party/moonlight-common-c/src/Limelight.h"
#include "padsvc_virtual_device.h"
#include "src/platform/windows/pad_service_protocol.h"

namespace {
  using namespace platf::pad_service_protocol;

  struct dualsense_slot_t {
    bool allocated {};
    std::uint8_t client_relative_index {};
    std::uint8_t type {};
    std::uint16_t capabilities {};
    std::uint32_t supported_buttons {};
  };

  std::atomic_bool running {true};
  std::mutex slot_mutex;
  std::array<dualsense_slot_t, 16> slots {};
  std::mutex feedback_mutex;
  HANDLE feedback_pipe_handle = INVALID_HANDLE_VALUE;
  auto device_backend = padsvc::make_virtual_device_backend();

  bool write_exact(HANDLE pipe, const void *buffer, DWORD size) {
    const auto *bytes = static_cast<const std::uint8_t *>(buffer);
    DWORD total_written = 0;

    while (total_written < size) {
      DWORD bytes_written = 0;
      if (!WriteFile(pipe, bytes + total_written, size - total_written, &bytes_written, nullptr) || bytes_written == 0) {
        return false;
      }
      total_written += bytes_written;
    }

    return true;
  }

  BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT || ctrl_type == CTRL_CLOSE_EVENT) {
      running = false;
      return TRUE;
    }

    return FALSE;
  }

  HANDLE create_pipe(const wchar_t *pipe_name, DWORD open_mode) {
    return CreateNamedPipeW(
      pipe_name,
      open_mode,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
      1,
      64 * 1024,
      64 * 1024,
      0,
      nullptr
    );
  }

  bool connect_pipe(HANDLE pipe) {
    if (ConnectNamedPipe(pipe, nullptr)) {
      return true;
    }

    auto error = GetLastError();
    return error == ERROR_PIPE_CONNECTED;
  }

  bool read_exact(HANDLE pipe, void *buffer, DWORD size) {
    auto *bytes = static_cast<std::uint8_t *>(buffer);
    DWORD total_read = 0;

    while (total_read < size) {
      DWORD bytes_read = 0;
      if (!ReadFile(pipe, bytes + total_read, size - total_read, &bytes_read, nullptr) || bytes_read == 0) {
        return false;
      }
      total_read += bytes_read;
    }

    return true;
  }

  template<typename T>
  void emit_feedback(const T &packet) {
    std::scoped_lock lock(feedback_mutex);
    if (feedback_pipe_handle == INVALID_HANDLE_VALUE) {
      return;
    }

    if (!write_exact(feedback_pipe_handle, &packet.header, sizeof(packet.header))) {
      return;
    }

    constexpr auto payload_size = sizeof(T) - sizeof(feedback_header_t);
    if constexpr (payload_size > 0) {
      (void) write_exact(
        feedback_pipe_handle,
        reinterpret_cast<const std::uint8_t *>(&packet) + sizeof(packet.header),
        payload_size
      );
    }
  }

  void emit_motion_request(std::int32_t global_index, std::uint8_t client_relative_index, std::uint8_t motion_type, std::uint16_t report_rate) {
    motion_report_rate_t packet {
      {
        version,
        static_cast<std::uint16_t>(feedback_type_e::motion_report_rate),
        static_cast<std::uint16_t>(sizeof(motion_report_rate_t) - sizeof(feedback_header_t)),
        global_index,
        client_relative_index,
        {0, 0, 0},
      },
      report_rate,
      motion_type,
      0,
    };
    emit_feedback(packet);
  }

  template<typename T>
  bool decode_packet(const command_header_t &header, const std::vector<std::uint8_t> &payload, T &packet) {
    const auto expected_payload_size = sizeof(T) - sizeof(command_header_t);
    if (payload.size() != expected_payload_size) {
      return false;
    }

    std::memcpy(&packet.header, &header, sizeof(header));
    if (expected_payload_size > 0) {
      std::memcpy(reinterpret_cast<std::uint8_t *>(&packet) + sizeof(header), payload.data(), expected_payload_size);
    }
    return true;
  }

  int handle_create(const create_dualsense_device_t &packet) {
    if (packet.header.global_index < 0 || packet.header.global_index >= static_cast<int>(slots.size())) {
      return -1;
    }

    if (device_backend->create(packet)) {
      std::cout << "padsvc: create_dualsense_device failed for slot=" << packet.header.global_index << "\n";
      return -1;
    }

    std::scoped_lock lock(slot_mutex);
    auto &slot = slots[packet.header.global_index];
    slot.allocated = true;
    slot.client_relative_index = packet.client_relative_index;
    slot.type = packet.type;
    slot.capabilities = packet.capabilities;
    slot.supported_buttons = packet.supported_buttons;

    std::cout << "padsvc: create_dualsense_device slot=" << packet.header.global_index
              << " client=" << static_cast<int>(packet.client_relative_index)
              << " caps=0x" << std::hex << packet.capabilities
              << " buttons=0x" << packet.supported_buttons << std::dec << "\n";

    if (packet.capabilities & LI_CCAP_ACCEL) {
      emit_motion_request(packet.header.global_index, packet.client_relative_index, LI_MOTION_TYPE_ACCEL, 100);
    }
    if (packet.capabilities & LI_CCAP_GYRO) {
      emit_motion_request(packet.header.global_index, packet.client_relative_index, LI_MOTION_TYPE_GYRO, 100);
    }

    return 0;
  }

  int handle_destroy(const destroy_dualsense_device_t &packet) {
    if (packet.header.global_index < 0 || packet.header.global_index >= static_cast<int>(slots.size())) {
      return -1;
    }

    std::scoped_lock lock(slot_mutex);
    slots[packet.header.global_index] = {};

    std::cout << "padsvc: destroy_dualsense_device slot=" << packet.header.global_index << "\n";
    device_backend->destroy(packet.header.global_index);
    return 0;
  }

  int handle_state(const update_state_t &packet) {
    std::cout << "padsvc: update_state slot=" << packet.header.global_index
              << " buttons=0x" << std::hex << packet.button_flags << std::dec << "\n";
    device_backend->update_state(packet);
    return 0;
  }

  int handle_touch(const update_touch_t &packet) {
    std::cout << "padsvc: update_touch slot=" << packet.header.global_index
              << " event=" << static_cast<int>(packet.event_type)
              << " pointer=" << packet.pointer_id << "\n";
    device_backend->update_touch(packet);
    return 0;
  }

  int handle_motion(const update_motion_t &packet) {
    std::cout << "padsvc: update_motion slot=" << packet.header.global_index
              << " type=" << static_cast<int>(packet.motion_type) << "\n";
    device_backend->update_motion(packet);
    return 0;
  }

  int handle_battery(const update_battery_t &packet) {
    std::cout << "padsvc: update_battery slot=" << packet.header.global_index
              << " state=" << static_cast<int>(packet.state)
              << " percentage=" << static_cast<int>(packet.percentage) << "\n";
    device_backend->update_battery(packet);
    return 0;
  }

  int handle_command(const command_header_t &header, const std::vector<std::uint8_t> &payload) {
    switch (static_cast<command_e>(header.command)) {
      case command_e::query_status:
        return 0;
      case command_e::create_dualsense_device:
        {
          create_dualsense_device_t packet {};
          if (decode_packet(header, payload, packet)) {
            return handle_create(packet);
          }
        }
        break;
      case command_e::destroy_dualsense_device:
        {
          destroy_dualsense_device_t packet {};
          if (decode_packet(header, payload, packet)) {
            return handle_destroy(packet);
          }
        }
        break;
      case command_e::update_state:
        {
          update_state_t packet {};
          if (decode_packet(header, payload, packet)) {
            return handle_state(packet);
          }
        }
        break;
      case command_e::update_touch:
        {
          update_touch_t packet {};
          if (decode_packet(header, payload, packet)) {
            return handle_touch(packet);
          }
        }
        break;
      case command_e::update_motion:
        {
          update_motion_t packet {};
          if (decode_packet(header, payload, packet)) {
            return handle_motion(packet);
          }
        }
        break;
      case command_e::update_battery:
        {
          update_battery_t packet {};
          if (decode_packet(header, payload, packet)) {
            return handle_battery(packet);
          }
        }
        break;
    }

    return -1;
  }
}  // namespace

int main() {
  SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

  auto backend_status = device_backend->status();
  std::cout << "padsvc: device backend available=" << (backend_status.available ? "true" : "false");
  if (!backend_status.reason.empty()) {
    std::cout << " reason=" << backend_status.reason;
  }
  std::cout << "\n";

  const auto command_pipe = create_pipe(named_pipe_path, PIPE_ACCESS_DUPLEX);
  const auto feedback_pipe = create_pipe(named_pipe_feedback_path, PIPE_ACCESS_OUTBOUND);

  if (command_pipe == INVALID_HANDLE_VALUE || feedback_pipe == INVALID_HANDLE_VALUE) {
    std::cerr << "padsvc: failed to create named pipes\n";
    return 1;
  }

  std::cout << "padsvc: waiting for command pipe connection\n";
  if (!connect_pipe(command_pipe)) {
    std::cerr << "padsvc: failed to connect command pipe\n";
    CloseHandle(command_pipe);
    CloseHandle(feedback_pipe);
    return 1;
  }

  std::cout << "padsvc: command pipe connected\n";
  std::thread feedback_accept_thread([feedback_pipe] {
    if (connect_pipe(feedback_pipe)) {
      std::scoped_lock lock(feedback_mutex);
      feedback_pipe_handle = feedback_pipe;
      std::cout << "padsvc: feedback pipe connected\n";
    }
  });

  while (running) {
    command_header_t header {};
    if (!read_exact(command_pipe, &header, sizeof(header))) {
      break;
    }

    std::vector<std::uint8_t> payload(header.payload_size);
    if (!payload.empty() && !read_exact(command_pipe, payload.data(), static_cast<DWORD>(payload.size()))) {
      break;
    }

    if (header.version != version) {
      std::cerr << "padsvc: unsupported protocol version " << header.version << "\n";
      continue;
    }

    auto status = handle_command(header, payload);

    if (static_cast<command_e>(header.command) == command_e::query_status) {
      auto current_status = device_backend->status();
      query_status_response_t response {
        {
          version,
          header.command,
          0,
          header.global_index,
          status,
        },
        static_cast<std::uint8_t>(current_status.available ? 1 : 0),
        static_cast<std::uint8_t>(current_status.installed ? 1 : 0),
        static_cast<std::uint8_t>(current_status.service_running ? 1 : 0),
        0,
        current_status.driver_version,
      };
      (void) write_exact(command_pipe, &response, sizeof(response));
      continue;
    }

    command_response_t response {
      version,
      header.command,
      0,
      header.global_index,
      status,
    };
    (void) write_exact(command_pipe, &response, sizeof(response));
  }

  CloseHandle(command_pipe);
  {
    std::scoped_lock lock(feedback_mutex);
    if (feedback_pipe_handle == INVALID_HANDLE_VALUE) {
      CloseHandle(feedback_pipe);
    }
  }
  if (feedback_accept_thread.joinable()) {
    feedback_accept_thread.join();
  }
  {
    std::scoped_lock lock(feedback_mutex);
    if (feedback_pipe_handle != INVALID_HANDLE_VALUE) {
      CloseHandle(feedback_pipe_handle);
      feedback_pipe_handle = INVALID_HANDLE_VALUE;
    }
  }
  return 0;
}
