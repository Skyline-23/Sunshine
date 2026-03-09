/**
 * @file tools/padsvc_virtual_device.cpp
 * @brief Service-side backend that forwards DualSense commands to the DSUSB bus device.
 */
#define WIN32_LEAN_AND_MEAN

// platform includes
#include <Windows.h>

// standard includes
#include <iostream>
#include <memory>

// local includes
#include "padsvc_virtual_device.h"
#include "src/platform/windows/dualsense_usb_bus.h"

namespace padsvc {
  namespace bus = platf::dualsense_usb_bus;
  namespace protocol = platf::pad_service_protocol;
  constexpr auto driver_service_name = L"SunshineDualSenseBus";

  class dsusb_virtual_device_backend_t final: public virtual_device_backend_t {
  public:
    virtual_device_status_t status() const override {
      auto service_status = ensure_driver_service();
      HANDLE handle = open_bus();
      if (handle == INVALID_HANDLE_VALUE) {
        return {
          false,
          service_status.installed,
          service_status.service_running,
          service_status.reason
        };
      }

      bus::version_t version_payload {bus::version};
      DWORD bytes_returned = 0;
      const auto ok = DeviceIoControl(
        handle,
        bus::ioctl_get_version,
        &version_payload,
        sizeof(version_payload),
        &version_payload,
        sizeof(version_payload),
        &bytes_returned,
        nullptr
      );
      CloseHandle(handle);

      if (!ok) {
        return {
          false,
          service_status.installed,
          service_status.service_running,
          "gamepads.dualsense-usb-not-available"
        };
      }

      return {
        true,
        true,
        true,
        ""
      };
    }

    int create(const protocol::create_dualsense_device_t &packet) override {
      bus::create_device_t create_packet {
        bus::version,
        packet.header.global_index,
        packet.client_relative_index,
        packet.type,
        packet.capabilities,
        packet.supported_buttons,
      };
      return send_ioctl(bus::ioctl_create_device, create_packet);
    }

    void destroy(std::int32_t global_index) override {
      bus::destroy_device_t destroy_packet {
        bus::version,
        global_index,
      };
      (void) send_ioctl(bus::ioctl_destroy_device, destroy_packet);
    }

    void update_state(const protocol::update_state_t &packet) override {
      bus::update_state_t state_packet {
        bus::version,
        packet.header.global_index,
        packet.button_flags,
        packet.lt,
        packet.rt,
        packet.ls_x,
        packet.ls_y,
        packet.rs_x,
        packet.rs_y,
      };
      (void) send_ioctl(bus::ioctl_update_state, state_packet);
    }

    void update_touch(const protocol::update_touch_t &packet) override {
      bus::update_touch_t touch_packet {
        bus::version,
        packet.header.global_index,
        packet.event_type,
        {0, 0, 0},
        packet.pointer_id,
        packet.x,
        packet.y,
        packet.pressure,
      };
      (void) send_ioctl(bus::ioctl_update_touch, touch_packet);
    }

    void update_motion(const protocol::update_motion_t &packet) override {
      bus::update_motion_t motion_packet {
        bus::version,
        packet.header.global_index,
        packet.motion_type,
        {0, 0, 0},
        packet.x,
        packet.y,
        packet.z,
      };
      (void) send_ioctl(bus::ioctl_update_motion, motion_packet);
    }

    void update_battery(const protocol::update_battery_t &packet) override {
      bus::update_battery_t battery_packet {
        bus::version,
        packet.header.global_index,
        packet.state,
        packet.percentage,
        {0, 0},
      };
      (void) send_ioctl(bus::ioctl_update_battery, battery_packet);
    }

  private:
    virtual_device_status_t ensure_driver_service() const {
      SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
      if (scm == nullptr) {
        return {
          false,
          false,
          false,
          "gamepads.dualsense-usb-not-available"
        };
      }

      SC_HANDLE service = OpenServiceW(scm, driver_service_name, SERVICE_QUERY_STATUS | SERVICE_START);
      if (service == nullptr) {
        CloseServiceHandle(scm);
        return {
          false,
          false,
          false,
          "gamepads.dualsense-usb-not-available"
        };
      }

      SERVICE_STATUS_PROCESS status {};
      DWORD bytes_needed = 0;
      const auto queried = QueryServiceStatusEx(
        service,
        SC_STATUS_PROCESS_INFO,
        reinterpret_cast<LPBYTE>(&status),
        sizeof(status),
        &bytes_needed
      );

      if (!queried) {
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return {
          false,
          true,
          false,
          "gamepads.dualsense-usb-not-available"
        };
      }

      if (status.dwCurrentState != SERVICE_RUNNING) {
        StartServiceW(service, 0, nullptr);
        QueryServiceStatusEx(
          service,
          SC_STATUS_PROCESS_INFO,
          reinterpret_cast<LPBYTE>(&status),
          sizeof(status),
          &bytes_needed
        );
      }

      auto service_running = status.dwCurrentState == SERVICE_RUNNING;
      CloseServiceHandle(service);
      CloseServiceHandle(scm);

      return {
        false,
        true,
        service_running,
        service_running ? "" : "gamepads.dualsense-usb-not-available"
      };
    }

    HANDLE open_bus() const {
      return CreateFileW(
        bus::device_path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
      );
    }

    template<typename T>
    int send_ioctl(DWORD ioctl, T &payload) const {
      HANDLE handle = open_bus();
      if (handle == INVALID_HANDLE_VALUE) {
        std::cout << "padsvc/device: DSUSB bus unavailable for ioctl " << ioctl << "\n";
        return -1;
      }

      DWORD bytes_returned = 0;
      const auto ok = DeviceIoControl(
        handle,
        ioctl,
        &payload,
        sizeof(payload),
        &payload,
        sizeof(payload),
        &bytes_returned,
        nullptr
      );
      CloseHandle(handle);

      if (!ok) {
        std::cout << "padsvc/device: DeviceIoControl failed for ioctl " << ioctl << "\n";
        return -1;
      }

      return 0;
    }
  };

  std::unique_ptr<virtual_device_backend_t> make_virtual_device_backend() {
    return std::make_unique<dsusb_virtual_device_backend_t>();
  }
}  // namespace padsvc
