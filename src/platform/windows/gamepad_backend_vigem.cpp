/**
 * @file src/platform/windows/gamepad_backend_vigem.cpp
 * @brief Legacy ViGEm-backed Windows gamepad backend implementation.
 */
#define WINVER 0x0A00

// platform includes
#include <Windows.h>

// standard includes
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <thread>
#include <vector>

// lib includes
#include <ViGEm/Client.h>

// local includes
#include "gamepad_backend.h"
#include "misc.h"
#include "src/config.h"
#include "src/globals.h"
#include "src/logging.h"

namespace platf {
  using namespace std::literals;

  using client_t = util::safe_ptr<_VIGEM_CLIENT_T, vigem_free>;
  using target_t = util::safe_ptr<_VIGEM_TARGET_T, vigem_target_free>;

  struct gamepad_context_t {
    target_t gp;
    feedback_queue_t feedback_queue;

    union {
      XUSB_REPORT x360;
      DS4_REPORT_EX ds4;
    } report;

    std::map<uint32_t, uint8_t> pointer_id_map;
    uint8_t available_pointers;
    uint8_t client_relative_index;

    thread_pool_util::ThreadPool::task_id_t repeat_task {};
    std::chrono::steady_clock::time_point last_report_ts;

    gamepad_feedback_msg_t last_rumble;
    gamepad_feedback_msg_t last_rgb_led;
  };

  constexpr float EARTH_G = 9.80665f;

#define MPS2_TO_DS4_ACCEL(x) (int32_t) (((x) / EARTH_G) * 8192)
#define DPS_TO_DS4_GYRO(x) (int32_t) ((x) * (1024 / 64))
#define APPLY_CALIBRATION(val, bias, scale) (int32_t) (((float) (val) + (bias)) / (scale))

  constexpr DS4_TOUCH ds4_touch_unused = {
    .bPacketCounter = 0,
    .bIsUpTrackingNum1 = 0x80,
    .bTouchData1 = {0x00, 0x00, 0x00},
    .bIsUpTrackingNum2 = 0x80,
    .bTouchData2 = {0x00, 0x00, 0x00},
  };

  constexpr DS4_REPORT_EX ds4_report_init_ex = {
    {{.bThumbLX = 0x80,
      .bThumbLY = 0x80,
      .bThumbRX = 0x80,
      .bThumbRY = 0x80,
      .wButtons = DS4_BUTTON_DPAD_NONE,
      .bSpecial = 0,
      .bTriggerL = 0,
      .bTriggerR = 0,
      .wTimestamp = 0,
      .bBatteryLvl = 0xFF,
      .wGyroX = 0,
      .wGyroY = 0,
      .wGyroZ = 0,
      .wAccelX = 0,
      .wAccelY = 0,
      .wAccelZ = 0,
      ._bUnknown1 = {0x00, 0x00, 0x00, 0x00, 0x00},
      .bBatteryLvlSpecial = 0x1A,
      ._bUnknown2 = {0x00, 0x00},
      .bTouchPacketsN = 1,
      .sCurrentTouch = ds4_touch_unused,
      .sPreviousTouch = {ds4_touch_unused, ds4_touch_unused}}}
  };

  class vigem_backend_t final: public windows_gamepad_backend_t {
  public:
    friend void CALLBACK x360_notify(
      client_t::pointer client,
      target_t::pointer target,
      std::uint8_t largeMotor,
      std::uint8_t smallMotor,
      std::uint8_t led_number,
      void *userdata
    );
    friend void CALLBACK ds4_notify(
      client_t::pointer client,
      target_t::pointer target,
      std::uint8_t largeMotor,
      std::uint8_t smallMotor,
      DS4_LIGHTBAR_COLOR led_color,
      void *userdata
    );

    int init() override;
    int alloc_gamepad(const gamepad_id_t &id, const gamepad_arrival_t &metadata, feedback_queue_t feedback_queue) override;
    void free_gamepad(int nr) override;
    void update_gamepad(int nr, const gamepad_state_t &gamepad_state) override;
    void touch_gamepad(const gamepad_touch_t &touch) override;
    void motion_gamepad(const gamepad_motion_t &motion) override;
    void battery_gamepad(const gamepad_battery_t &battery) override;
    const std::vector<supported_gamepad_t> &supported_gamepads() const override;
    ~vigem_backend_t() override;

  private:
    int alloc_gamepad_internal(const gamepad_id_t &id, feedback_queue_t &feedback_queue, VIGEM_TARGET_TYPE gp_type);
    void free_target(int nr);
    void rumble(target_t::pointer target, std::uint8_t largeMotor, std::uint8_t smallMotor);
    void set_rgb_led(target_t::pointer target, std::uint8_t r, std::uint8_t g, std::uint8_t b);
    void ds4_update_ts_and_send(int nr);

    std::vector<gamepad_context_t> gamepads;
    client_t client;
  };

  static void ds4_update_motion(gamepad_context_t &gamepad, uint8_t motion_type, float x, float y, float z) {
    auto &report = gamepad.report.ds4.Report;

    int32_t intX;
    int32_t intY;
    int32_t intZ;

    switch (motion_type) {
      case LI_MOTION_TYPE_ACCEL:
        intX = MPS2_TO_DS4_ACCEL(x);
        intY = MPS2_TO_DS4_ACCEL(y);
        intZ = MPS2_TO_DS4_ACCEL(z);
        intX = APPLY_CALIBRATION(intX, -297, 1.010796f);
        intY = APPLY_CALIBRATION(intY, -42, 1.014614f);
        intZ = APPLY_CALIBRATION(intZ, -512, 1.024768f);
        break;
      case LI_MOTION_TYPE_GYRO:
        intX = DPS_TO_DS4_GYRO(x);
        intY = DPS_TO_DS4_GYRO(y);
        intZ = DPS_TO_DS4_GYRO(z);
        intX = APPLY_CALIBRATION(intX, 1, 0.977596f);
        intY = APPLY_CALIBRATION(intY, 0, 0.972370f);
        intZ = APPLY_CALIBRATION(intZ, 0, 0.971550f);
        break;
      default:
        return;
    }

    intX = std::clamp(intX, INT16_MIN, INT16_MAX);
    intY = std::clamp(intY, INT16_MIN, INT16_MAX);
    intZ = std::clamp(intZ, INT16_MIN, INT16_MAX);

    switch (motion_type) {
      case LI_MOTION_TYPE_ACCEL:
        report.wAccelX = (int16_t) intX;
        report.wAccelY = (int16_t) intY;
        report.wAccelZ = (int16_t) intZ;
        break;
      case LI_MOTION_TYPE_GYRO:
        report.wGyroX = (int16_t) intX;
        report.wGyroY = (int16_t) intY;
        report.wGyroZ = (int16_t) intZ;
        break;
      default:
        return;
    }
  }

  static XUSB_BUTTON x360_buttons(const gamepad_state_t &gamepad_state) {
    int buttons {};

    auto flags = gamepad_state.buttonFlags;
    if (flags & DPAD_UP) buttons |= XUSB_GAMEPAD_DPAD_UP;
    if (flags & DPAD_DOWN) buttons |= XUSB_GAMEPAD_DPAD_DOWN;
    if (flags & DPAD_LEFT) buttons |= XUSB_GAMEPAD_DPAD_LEFT;
    if (flags & DPAD_RIGHT) buttons |= XUSB_GAMEPAD_DPAD_RIGHT;
    if (flags & START) buttons |= XUSB_GAMEPAD_START;
    if (flags & BACK) buttons |= XUSB_GAMEPAD_BACK;
    if (flags & LEFT_STICK) buttons |= XUSB_GAMEPAD_LEFT_THUMB;
    if (flags & RIGHT_STICK) buttons |= XUSB_GAMEPAD_RIGHT_THUMB;
    if (flags & LEFT_BUTTON) buttons |= XUSB_GAMEPAD_LEFT_SHOULDER;
    if (flags & RIGHT_BUTTON) buttons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
    if (flags & (HOME | MISC_BUTTON)) buttons |= XUSB_GAMEPAD_GUIDE;
    if (flags & A) buttons |= XUSB_GAMEPAD_A;
    if (flags & B) buttons |= XUSB_GAMEPAD_B;
    if (flags & X) buttons |= XUSB_GAMEPAD_X;
    if (flags & Y) buttons |= XUSB_GAMEPAD_Y;

    return (XUSB_BUTTON) buttons;
  }

  static void x360_update_state(gamepad_context_t &gamepad, const gamepad_state_t &gamepad_state) {
    auto &report = gamepad.report.x360;
    report.wButtons = x360_buttons(gamepad_state);
    report.bLeftTrigger = gamepad_state.lt;
    report.bRightTrigger = gamepad_state.rt;
    report.sThumbLX = gamepad_state.lsX;
    report.sThumbLY = gamepad_state.lsY;
    report.sThumbRX = gamepad_state.rsX;
    report.sThumbRY = gamepad_state.rsY;
  }

  static DS4_DPAD_DIRECTIONS ds4_dpad(const gamepad_state_t &gamepad_state) {
    auto flags = gamepad_state.buttonFlags;
    if (flags & DPAD_UP) {
      if (flags & DPAD_RIGHT) return DS4_BUTTON_DPAD_NORTHEAST;
      if (flags & DPAD_LEFT) return DS4_BUTTON_DPAD_NORTHWEST;
      return DS4_BUTTON_DPAD_NORTH;
    } else if (flags & DPAD_DOWN) {
      if (flags & DPAD_RIGHT) return DS4_BUTTON_DPAD_SOUTHEAST;
      if (flags & DPAD_LEFT) return DS4_BUTTON_DPAD_SOUTHWEST;
      return DS4_BUTTON_DPAD_SOUTH;
    } else if (flags & DPAD_RIGHT) {
      return DS4_BUTTON_DPAD_EAST;
    } else if (flags & DPAD_LEFT) {
      return DS4_BUTTON_DPAD_WEST;
    }
    return DS4_BUTTON_DPAD_NONE;
  }

  static DS4_BUTTONS ds4_buttons(const gamepad_state_t &gamepad_state) {
    int buttons {};
    auto flags = gamepad_state.buttonFlags;
    if (flags & LEFT_STICK) buttons |= DS4_BUTTON_THUMB_LEFT;
    if (flags & RIGHT_STICK) buttons |= DS4_BUTTON_THUMB_RIGHT;
    if (flags & LEFT_BUTTON) buttons |= DS4_BUTTON_SHOULDER_LEFT;
    if (flags & RIGHT_BUTTON) buttons |= DS4_BUTTON_SHOULDER_RIGHT;
    if (flags & START) buttons |= DS4_BUTTON_OPTIONS;
    if (flags & BACK) buttons |= DS4_BUTTON_SHARE;
    if (flags & A) buttons |= DS4_BUTTON_CROSS;
    if (flags & B) buttons |= DS4_BUTTON_CIRCLE;
    if (flags & X) buttons |= DS4_BUTTON_SQUARE;
    if (flags & Y) buttons |= DS4_BUTTON_TRIANGLE;
    if (gamepad_state.lt > 0) buttons |= DS4_BUTTON_TRIGGER_LEFT;
    if (gamepad_state.rt > 0) buttons |= DS4_BUTTON_TRIGGER_RIGHT;
    return (DS4_BUTTONS) buttons;
  }

  static DS4_SPECIAL_BUTTONS ds4_special_buttons(const gamepad_state_t &gamepad_state) {
    int buttons {};
    if (gamepad_state.buttonFlags & HOME) {
      buttons |= DS4_SPECIAL_BUTTON_PS;
    }
    if (gamepad_state.buttonFlags & (TOUCHPAD_BUTTON | MISC_BUTTON)) {
      buttons |= DS4_SPECIAL_BUTTON_TOUCHPAD;
    }
    if (config::input.gamepad == "ds4"sv && config::input.ds4_back_as_touchpad_click && (gamepad_state.buttonFlags & BACK)) {
      buttons |= DS4_SPECIAL_BUTTON_TOUCHPAD;
    }
    return (DS4_SPECIAL_BUTTONS) buttons;
  }

  static std::uint8_t to_ds4_triggerX(std::int16_t v) {
    return (v + std::numeric_limits<std::uint16_t>::max() / 2 + 1) / 257;
  }

  static std::uint8_t to_ds4_triggerY(std::int16_t v) {
    auto new_v = -((std::numeric_limits<std::uint16_t>::max() / 2 + v - 1)) / 257;
    return new_v == 0 ? 0xFF : (std::uint8_t) new_v;
  }

  static void ds4_update_state(gamepad_context_t &gamepad, const gamepad_state_t &gamepad_state) {
    auto &report = gamepad.report.ds4.Report;
    report.wButtons = static_cast<uint16_t>(ds4_buttons(gamepad_state)) | static_cast<uint16_t>(ds4_dpad(gamepad_state));
    report.bSpecial = ds4_special_buttons(gamepad_state);
    report.bTriggerL = gamepad_state.lt;
    report.bTriggerR = gamepad_state.rt;
    report.bThumbLX = to_ds4_triggerX(gamepad_state.lsX);
    report.bThumbLY = to_ds4_triggerY(gamepad_state.lsY);
    report.bThumbRX = to_ds4_triggerX(gamepad_state.rsX);
    report.bThumbRY = to_ds4_triggerY(gamepad_state.rsY);
  }

  int vigem_backend_t::init() {
    client_t probe_client {vigem_alloc()};
    VIGEM_ERROR status = vigem_connect(probe_client.get());
    if (!VIGEM_SUCCESS(status)) {
      BOOST_LOG(fatal) << "ViGEmBus is not installed or running. You must install ViGEmBus for gamepad support!"sv;
    } else {
      vigem_disconnect(probe_client.get());
    }

    gamepads.resize(MAX_GAMEPADS);
    return 0;
  }

  int vigem_backend_t::alloc_gamepad_internal(const gamepad_id_t &id, feedback_queue_t &feedback_queue, VIGEM_TARGET_TYPE gp_type) {
    auto &gamepad = gamepads[id.globalIndex];
    assert(!gamepad.gp);

    gamepad.client_relative_index = id.clientRelativeIndex;
    gamepad.last_report_ts = std::chrono::steady_clock::now();

    if (!client) {
      BOOST_LOG(debug) << "Connecting to ViGEmBus driver"sv;
      client.reset(vigem_alloc());

      auto status = vigem_connect(client.get());
      if (!VIGEM_SUCCESS(status)) {
        BOOST_LOG(warning) << "Couldn't setup connection to ViGEm for gamepad support ["sv << util::hex(status).to_string_view() << ']';
        client.reset();
        return -1;
      }
    }

    if (gp_type == Xbox360Wired) {
      gamepad.gp.reset(vigem_target_x360_alloc());
      XUSB_REPORT_INIT(&gamepad.report.x360);
    } else {
      gamepad.gp.reset(vigem_target_ds4_alloc());
      gamepad.report.ds4 = ds4_report_init_ex;
      ds4_update_motion(gamepad, LI_MOTION_TYPE_ACCEL, 0.0f, EARTH_G, 0.0f);
      ds4_update_motion(gamepad, LI_MOTION_TYPE_GYRO, 0.0f, 0.0f, 0.0f);
      feedback_queue->raise(gamepad_feedback_msg_t::make_motion_event_state(gamepad.client_relative_index, LI_MOTION_TYPE_ACCEL, 100));
      feedback_queue->raise(gamepad_feedback_msg_t::make_motion_event_state(gamepad.client_relative_index, LI_MOTION_TYPE_GYRO, 100));
      gamepad.available_pointers = 0x3;
    }

    auto status = vigem_target_add(client.get(), gamepad.gp.get());
    if (!VIGEM_SUCCESS(status)) {
      BOOST_LOG(error) << "Couldn't add Gamepad to ViGEm connection ["sv << util::hex(status).to_string_view() << ']';
      return -1;
    }

    gamepad.feedback_queue = std::move(feedback_queue);
    if (gp_type == Xbox360Wired) {
      status = vigem_target_x360_register_notification(client.get(), gamepad.gp.get(), x360_notify, this);
    } else {
      status = vigem_target_ds4_register_notification(client.get(), gamepad.gp.get(), ds4_notify, this);
    }

    if (!VIGEM_SUCCESS(status)) {
      BOOST_LOG(warning) << "Couldn't register notifications for rumble support ["sv << util::hex(status).to_string_view() << ']';
    }

    return 0;
  }

  int vigem_backend_t::alloc_gamepad(const gamepad_id_t &id, const gamepad_arrival_t &metadata, feedback_queue_t feedback_queue) {
    VIGEM_TARGET_TYPE selectedGamepadType;

    if (config::input.gamepad == "x360"sv) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be Xbox 360 controller (manual selection)"sv;
      selectedGamepadType = Xbox360Wired;
    } else if (config::input.gamepad == "ds4"sv) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be DualShock 4 controller (manual selection)"sv;
      selectedGamepadType = DualShock4Wired;
    } else if (metadata.type == LI_CTYPE_PS) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be DualShock 4 controller (auto-selected by client-reported type)"sv;
      selectedGamepadType = DualShock4Wired;
    } else if (metadata.type == LI_CTYPE_XBOX) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be Xbox 360 controller (auto-selected by client-reported type)"sv;
      selectedGamepadType = Xbox360Wired;
    } else if (config::input.motion_as_ds4 && (metadata.capabilities & (LI_CCAP_ACCEL | LI_CCAP_GYRO))) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be DualShock 4 controller (auto-selected by motion sensor presence)"sv;
      selectedGamepadType = DualShock4Wired;
    } else if (config::input.touchpad_as_ds4 && (metadata.capabilities & LI_CCAP_TOUCHPAD)) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be DualShock 4 controller (auto-selected by touchpad presence)"sv;
      selectedGamepadType = DualShock4Wired;
    } else {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be Xbox 360 controller (default)"sv;
      selectedGamepadType = Xbox360Wired;
    }

    if (selectedGamepadType == Xbox360Wired) {
      if (metadata.capabilities & (LI_CCAP_ACCEL | LI_CCAP_GYRO)) {
        BOOST_LOG(warning) << "Gamepad " << id.globalIndex << " has motion sensors, but they are not usable when emulating an Xbox 360 controller"sv;
      }
      if (metadata.capabilities & LI_CCAP_TOUCHPAD) {
        BOOST_LOG(warning) << "Gamepad " << id.globalIndex << " has a touchpad, but it is not usable when emulating an Xbox 360 controller"sv;
      }
      if (metadata.capabilities & LI_CCAP_RGB_LED) {
        BOOST_LOG(warning) << "Gamepad " << id.globalIndex << " has an RGB LED, but it is not usable when emulating an Xbox 360 controller"sv;
      }
    } else if (selectedGamepadType == DualShock4Wired) {
      if (!(metadata.capabilities & (LI_CCAP_ACCEL | LI_CCAP_GYRO))) {
        BOOST_LOG(warning) << "Gamepad " << id.globalIndex << " is emulating a DualShock 4 controller, but the client gamepad doesn't have motion sensors active"sv;
      }
      if (!(metadata.capabilities & LI_CCAP_TOUCHPAD)) {
        BOOST_LOG(warning) << "Gamepad " << id.globalIndex << " is emulating a DualShock 4 controller, but the client gamepad doesn't have a touchpad"sv;
      }
    }

    return alloc_gamepad_internal(id, feedback_queue, selectedGamepadType);
  }

  const std::vector<supported_gamepad_t> &vigem_backend_t::supported_gamepads() const {
    static const std::vector gps {
      supported_gamepad_t {"auto", true, ""},
      supported_gamepad_t {"x360", true, ""},
      supported_gamepad_t {"ds4", true, ""}
    };

    return gps;
  }

  void vigem_backend_t::free_target(int nr) {
    auto &gamepad = gamepads[nr];

    if (gamepad.repeat_task) {
      task_pool.cancel(gamepad.repeat_task);
      gamepad.repeat_task = nullptr;
    }

    if (gamepad.gp && vigem_target_is_attached(gamepad.gp.get())) {
      auto status = vigem_target_remove(client.get(), gamepad.gp.get());
      if (!VIGEM_SUCCESS(status)) {
        BOOST_LOG(warning) << "Couldn't detach gamepad from ViGEm ["sv << util::hex(status).to_string_view() << ']';
      }
    }

    gamepad.gp.reset();

    bool disconnect = true;
    for (auto &attached_gamepad : gamepads) {
      if (attached_gamepad.gp && vigem_target_is_attached(attached_gamepad.gp.get())) {
        disconnect = false;
        break;
      }
    }
    if (disconnect) {
      BOOST_LOG(debug) << "Disconnecting from ViGEmBus driver"sv;
      vigem_disconnect(client.get());
      client.reset();
    }
  }

  void vigem_backend_t::free_gamepad(int nr) {
    free_target(nr);
  }

  void vigem_backend_t::rumble(target_t::pointer target, std::uint8_t largeMotor, std::uint8_t smallMotor) {
    for (auto &gamepad : gamepads) {
      if (gamepad.gp.get() == target) {
        uint16_t normalizedLargeMotor = largeMotor << 8;
        uint16_t normalizedSmallMotor = smallMotor << 8;

        if (normalizedSmallMotor != gamepad.last_rumble.data.rumble.highfreq ||
            normalizedLargeMotor != gamepad.last_rumble.data.rumble.lowfreq) {
          auto msg = gamepad_feedback_msg_t::make_rumble(gamepad.client_relative_index, normalizedLargeMotor, normalizedSmallMotor);
          gamepad.feedback_queue->raise(msg);
          gamepad.last_rumble = msg;
        }
        return;
      }
    }
  }

  void vigem_backend_t::set_rgb_led(target_t::pointer target, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    for (auto &gamepad : gamepads) {
      if (gamepad.gp.get() == target) {
        if (r != gamepad.last_rgb_led.data.rgb_led.r ||
            g != gamepad.last_rgb_led.data.rgb_led.g ||
            b != gamepad.last_rgb_led.data.rgb_led.b) {
          auto msg = gamepad_feedback_msg_t::make_rgb_led(gamepad.client_relative_index, r, g, b);
          gamepad.feedback_queue->raise(msg);
          gamepad.last_rgb_led = msg;
        }
        return;
      }
    }
  }

  vigem_backend_t::~vigem_backend_t() {
    if (client) {
      for (auto &gamepad : gamepads) {
        if (gamepad.gp && vigem_target_is_attached(gamepad.gp.get())) {
          auto status = vigem_target_remove(client.get(), gamepad.gp.get());
          if (!VIGEM_SUCCESS(status)) {
            BOOST_LOG(warning) << "Couldn't detach gamepad from ViGEm ["sv << util::hex(status).to_string_view() << ']';
          }
        }
      }

      vigem_disconnect(client.get());
    }
  }

  void CALLBACK x360_notify(
    client_t::pointer client,
    target_t::pointer target,
    std::uint8_t largeMotor,
    std::uint8_t smallMotor,
    std::uint8_t /* led_number */,
    void *userdata
  ) {
    BOOST_LOG(debug)
      << "largeMotor: "sv << (int) largeMotor << std::endl
      << "smallMotor: "sv << (int) smallMotor;

    task_pool.push(&vigem_backend_t::rumble, (vigem_backend_t *) userdata, target, largeMotor, smallMotor);
  }

  void CALLBACK ds4_notify(
    client_t::pointer client,
    target_t::pointer target,
    std::uint8_t largeMotor,
    std::uint8_t smallMotor,
    DS4_LIGHTBAR_COLOR led_color,
    void *userdata
  ) {
    BOOST_LOG(debug)
      << "largeMotor: "sv << (int) largeMotor << std::endl
      << "smallMotor: "sv << (int) smallMotor << std::endl
      << "LED: "sv << util::hex(led_color.Red).to_string_view() << ' '
      << util::hex(led_color.Green).to_string_view() << ' '
      << util::hex(led_color.Blue).to_string_view() << std::endl;

    task_pool.push(&vigem_backend_t::rumble, (vigem_backend_t *) userdata, target, largeMotor, smallMotor);
    task_pool.push(&vigem_backend_t::set_rgb_led, (vigem_backend_t *) userdata, target, led_color.Red, led_color.Green, led_color.Blue);
  }

  void vigem_backend_t::ds4_update_ts_and_send(int nr) {
    auto &gamepad = gamepads[nr];

    if (gamepad.repeat_task) {
      task_pool.cancel(gamepad.repeat_task);
      gamepad.repeat_task = nullptr;
    }

    if (gamepad.gp && vigem_target_is_attached(gamepad.gp.get())) {
      auto now = std::chrono::steady_clock::now();
      auto delta_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - gamepad.last_report_ts);
      gamepad.report.ds4.Report.wTimestamp += (uint16_t) (delta_ns.count() / 5333);

      auto status = vigem_target_ds4_update_ex(client.get(), gamepad.gp.get(), gamepad.report.ds4);
      if (!VIGEM_SUCCESS(status)) {
        BOOST_LOG(warning) << "Couldn't send gamepad input to ViGEm ["sv << util::hex(status).to_string_view() << ']';
        return;
      }

      gamepad.last_report_ts = now;
      gamepad.repeat_task = task_pool.pushDelayed(&vigem_backend_t::ds4_update_ts_and_send, 100ms, this, nr).task_id;
    }
  }

  void vigem_backend_t::update_gamepad(int nr, const gamepad_state_t &gamepad_state) {
    auto &gamepad = gamepads[nr];
    if (!gamepad.gp) {
      return;
    }

    if (vigem_target_get_type(gamepad.gp.get()) == Xbox360Wired) {
      x360_update_state(gamepad, gamepad_state);
      auto status = vigem_target_x360_update(client.get(), gamepad.gp.get(), gamepad.report.x360);
      if (!VIGEM_SUCCESS(status)) {
        BOOST_LOG(warning) << "Couldn't send gamepad input to ViGEm ["sv << util::hex(status).to_string_view() << ']';
      }
    } else {
      ds4_update_state(gamepad, gamepad_state);
      ds4_update_ts_and_send(nr);
    }
  }

  void vigem_backend_t::touch_gamepad(const gamepad_touch_t &touch) {
    auto &gamepad = gamepads[touch.id.globalIndex];
    if (!gamepad.gp) {
      return;
    }
    if (vigem_target_get_type(gamepad.gp.get()) != DualShock4Wired) {
      return;
    }

    auto &report = gamepad.report.ds4.Report;

    uint8_t pointerIndex;
    if (touch.eventType == LI_TOUCH_EVENT_DOWN) {
      if (gamepad.available_pointers & 0x1) {
        gamepad.pointer_id_map[touch.pointerId] = pointerIndex = 0;
        gamepad.available_pointers &= ~(1 << pointerIndex);
        report.sCurrentTouch.bIsUpTrackingNum1 &= ~0x80;
        report.sCurrentTouch.bIsUpTrackingNum1++;
      } else if (gamepad.available_pointers & 0x2) {
        gamepad.pointer_id_map[touch.pointerId] = pointerIndex = 1;
        gamepad.available_pointers &= ~(1 << pointerIndex);
        report.sCurrentTouch.bIsUpTrackingNum2 &= ~0x80;
        report.sCurrentTouch.bIsUpTrackingNum2++;
      } else {
        BOOST_LOG(warning) << "No more free pointer indices! Did the client miss an touch up event?"sv;
        return;
      }
    } else if (touch.eventType == LI_TOUCH_EVENT_CANCEL_ALL) {
      report.sCurrentTouch.bIsUpTrackingNum1 |= 0x80;
      report.sCurrentTouch.bIsUpTrackingNum2 |= 0x80;
      gamepad.pointer_id_map.clear();
      gamepad.available_pointers = 0x3;
    } else {
      auto i = gamepad.pointer_id_map.find(touch.pointerId);
      if (i == gamepad.pointer_id_map.end()) {
        BOOST_LOG(warning) << "Pointer ID not found! Did the client miss a touch down event?"sv;
        return;
      }

      pointerIndex = (*i).second;
      if (touch.eventType == LI_TOUCH_EVENT_UP || touch.eventType == LI_TOUCH_EVENT_CANCEL) {
        gamepad.pointer_id_map.erase(i);
        if (pointerIndex == 0) {
          report.sCurrentTouch.bIsUpTrackingNum1 |= 0x80;
        } else {
          report.sCurrentTouch.bIsUpTrackingNum2 |= 0x80;
        }
        gamepad.available_pointers |= (1 << pointerIndex);
      } else if (touch.eventType != LI_TOUCH_EVENT_MOVE) {
        BOOST_LOG(warning) << "Unsupported touch event for gamepad: "sv << (uint32_t) touch.eventType;
        return;
      }
    }

    uint16_t x = touch.x * 1920;
    uint16_t y = touch.y * 943;
    uint8_t touchData[] = {
      (uint8_t) (x & 0xFF),
      (uint8_t) ((x >> 8 & 0x0F) | (y & 0x0F) << 4),
      (uint8_t) (y >> 4 & 0xFF)
    };

    report.sCurrentTouch.bPacketCounter++;
    if (touch.eventType != LI_TOUCH_EVENT_CANCEL_ALL) {
      if (pointerIndex == 0) {
        memcpy(report.sCurrentTouch.bTouchData1, touchData, sizeof(touchData));
      } else {
        memcpy(report.sCurrentTouch.bTouchData2, touchData, sizeof(touchData));
      }
    }

    ds4_update_ts_and_send(touch.id.globalIndex);
  }

  void vigem_backend_t::motion_gamepad(const gamepad_motion_t &motion) {
    auto &gamepad = gamepads[motion.id.globalIndex];
    if (!gamepad.gp) {
      return;
    }
    if (vigem_target_get_type(gamepad.gp.get()) != DualShock4Wired) {
      return;
    }

    ds4_update_motion(gamepad, motion.motionType, motion.x, motion.y, motion.z);
    ds4_update_ts_and_send(motion.id.globalIndex);
  }

  void vigem_backend_t::battery_gamepad(const gamepad_battery_t &battery) {
    auto &gamepad = gamepads[battery.id.globalIndex];
    if (!gamepad.gp) {
      return;
    }
    if (vigem_target_get_type(gamepad.gp.get()) != DualShock4Wired) {
      return;
    }

    auto &report = gamepad.report.ds4.Report;
    switch (battery.state) {
      case LI_BATTERY_STATE_CHARGING:
      case LI_BATTERY_STATE_DISCHARGING:
        if (battery.state == LI_BATTERY_STATE_CHARGING) {
          report.bBatteryLvlSpecial |= 0x10;
        } else {
          report.bBatteryLvlSpecial &= ~0x10;
        }
        if ((report.bBatteryLvlSpecial & 0xF) > 0xA) {
          report.bBatteryLvlSpecial = (report.bBatteryLvlSpecial & ~0xF) | 0x5;
        }
        break;
      case LI_BATTERY_STATE_FULL:
        report.bBatteryLvlSpecial = 0x1B;
        report.bBatteryLvl = 0xFF;
        break;
      case LI_BATTERY_STATE_NOT_PRESENT:
      case LI_BATTERY_STATE_NOT_CHARGING:
        report.bBatteryLvlSpecial = 0x1F;
        break;
      default:
        break;
    }

    if (battery.percentage != LI_BATTERY_PERCENTAGE_UNKNOWN) {
      report.bBatteryLvl = battery.percentage * 255 / 100;
      if ((report.bBatteryLvlSpecial & 0x10) && (report.bBatteryLvlSpecial & 0xF) <= 0xA) {
        report.bBatteryLvlSpecial = (report.bBatteryLvlSpecial & ~0xF) | ((battery.percentage + 5) / 10);
      }
    }

    ds4_update_ts_and_send(battery.id.globalIndex);
  }

  std::unique_ptr<windows_gamepad_backend_t> make_gamepad_backend() {
    return std::make_unique<vigem_backend_t>();
  }
}  // namespace platf
