/**
 * @file src/platform/windows/gamepad_backend_factory.cpp
 * @brief Factory for selecting the active Windows gamepad backend.
 */
#define WINVER 0x0A00

// platform includes
#include <Windows.h>

// local includes
#include "gamepad_backend.h"
#include "src/config.h"

namespace platf {
  using namespace std::literals;

  std::unique_ptr<windows_gamepad_backend_t> make_gamepad_backend() {
    if (config::input.gamepad == "dualsense_usb"sv) {
      return make_dualsense_usb_gamepad_backend();
    }

    return make_vigem_gamepad_backend();
  }
}  // namespace platf
