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
#include <vector>

// local includes
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

  void handle_create(const create_dualsense_device_t &packet) {
    if (packet.header.global_index < 0 || packet.header.global_index >= static_cast<int>(slots.size())) {
      return;
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
  }

  void handle_destroy(const destroy_dualsense_device_t &packet) {
    if (packet.header.global_index < 0 || packet.header.global_index >= static_cast<int>(slots.size())) {
      return;
    }

    std::scoped_lock lock(slot_mutex);
    slots[packet.header.global_index] = {};

    std::cout << "padsvc: destroy_dualsense_device slot=" << packet.header.global_index << "\n";
  }

  void handle_state(const update_state_t &packet) {
    std::cout << "padsvc: update_state slot=" << packet.header.global_index
              << " buttons=0x" << std::hex << packet.button_flags << std::dec << "\n";
  }

  void handle_touch(const update_touch_t &packet) {
    std::cout << "padsvc: update_touch slot=" << packet.header.global_index
              << " event=" << static_cast<int>(packet.event_type)
              << " pointer=" << packet.pointer_id << "\n";
  }

  void handle_motion(const update_motion_t &packet) {
    std::cout << "padsvc: update_motion slot=" << packet.header.global_index
              << " type=" << static_cast<int>(packet.motion_type) << "\n";
  }

  void handle_battery(const update_battery_t &packet) {
    std::cout << "padsvc: update_battery slot=" << packet.header.global_index
              << " state=" << static_cast<int>(packet.state)
              << " percentage=" << static_cast<int>(packet.percentage) << "\n";
  }

  void handle_command(const command_header_t &header, const std::vector<std::uint8_t> &payload) {
    switch (static_cast<command_e>(header.command)) {
      case command_e::create_dualsense_device:
        {
          create_dualsense_device_t packet {};
          if (decode_packet(header, payload, packet)) {
            handle_create(packet);
          }
        }
        break;
      case command_e::destroy_dualsense_device:
        {
          destroy_dualsense_device_t packet {};
          if (decode_packet(header, payload, packet)) {
            handle_destroy(packet);
          }
        }
        break;
      case command_e::update_state:
        {
          update_state_t packet {};
          if (decode_packet(header, payload, packet)) {
            handle_state(packet);
          }
        }
        break;
      case command_e::update_touch:
        {
          update_touch_t packet {};
          if (decode_packet(header, payload, packet)) {
            handle_touch(packet);
          }
        }
        break;
      case command_e::update_motion:
        {
          update_motion_t packet {};
          if (decode_packet(header, payload, packet)) {
            handle_motion(packet);
          }
        }
        break;
      case command_e::update_battery:
        {
          update_battery_t packet {};
          if (decode_packet(header, payload, packet)) {
            handle_battery(packet);
          }
        }
        break;
    }
  }
}  // namespace

int main() {
  SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

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
  std::cout << "padsvc: feedback pipe created (connection not active in skeleton)\n";

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

    handle_command(header, payload);
  }

  CloseHandle(command_pipe);
  CloseHandle(feedback_pipe);
  return 0;
}
