# DSUSB Driver Notes

## Purpose

`dsusb` is the Windows-side control and virtual device path for host-side Virtual USB DualSense emulation.

The repository now contains:

- a user-mode/backend path in Sunshine that selects `dualsense_usb`
- a named-pipe pad service client
- a `sunshinepadsvc` server skeleton
- a DSUSB bus driver skeleton that accepts IOCTLs
- a split between bus control and UDE-backed device creation
- a minimal DualSense descriptor skeleton

## Files

- `dsusb_bus_ioctl.h`
  C-compatible IOCTL contract used by the bus driver
- `dsusb_bus_driver.c`
  control device skeleton that exposes `\\.\SunshineDualSenseBus`
- `dsusb_ude_device.h`
  internal interface for the UDE-backed virtual device implementation
- `dsusb_ude_device.c`
  current UDE stub layer
- `dsusb_dualsense_descriptors.h`
  descriptor declarations
- `dsusb_dualsense_descriptors.c`
  minimal USB/HID descriptor skeleton
- `dsusb_bus.inf`
  starter INF package for installing the bus driver

## What Works Today

- Sunshine can select a `dualsense_usb` backend.
- Sunshine can auto-launch `sunshinepadsvc.exe` when the pad service pipe is absent.
- `sunshinepadsvc` accepts create/destroy/update commands over named pipes.
- `sunshinepadsvc` forwards those commands into the DSUSB bus contract.
- The DSUSB bus driver skeleton accepts IOCTLs and stores per-slot state.
- The UDE layer is split out behind `DsUsbUde*` functions.
- A descriptor skeleton exists for the future virtual device path.

## What Is Still Missing

### 1. Real UDE Device Creation

`dsusb_ude_device.c` currently stores metadata only. It does not create an actual UDE root, USB device, interface, or endpoints.

Needed work:

- create a per-slot UDE object model
- register the USB device with UDE
- bind the descriptor set to the device
- expose interrupt IN and OUT endpoints
- manage per-slot lifecycle and teardown

### 2. Real DualSense HID Report Model

The HID descriptor is intentionally minimal.

Needed work:

- replace the skeleton report descriptor with a report layout that matches the intended DualSense emulation target
- define stable input report structures
- define output report structures for:
  - rumble
  - RGB LED
  - adaptive trigger effects
- map Sunshine state into those reports

### 3. Output Report Path

The current driver does not yet capture host output reports and push them back to `sunshinepadsvc`.

Needed work:

- collect output reports from the HID OUT endpoint
- translate them into the service feedback protocol
- forward them through the feedback named pipe
- validate the end-to-end path:
  `game -> driver -> sunshinepadsvc -> Sunshine -> Shadow`

### 4. Driver Packaging

The INF and batch scripts are a starting point only.

Needed work:

- sign the driver package
- package `dsusb_bus_driver.sys` and `dsusb_bus.inf` with the Windows build artifacts
- decide whether installation should be driven by:
  - `pnputil`
  - a setup script
  - the existing `sunshine-setup.ps1`
  - a future Windows installer step

### 5. Build Integration

The repository includes driver source but does not yet build it.

Needed work:

- add a WDK-capable build path
- decide whether the driver builds through:
  - a Visual Studio / WDK project
  - MSBuild invoked from CI
  - a separate driver-only workflow
- document local build requirements

## Suggested Next Order

1. Implement real UDE per-slot device creation in `dsusb_ude_device.c`
2. Replace the HID descriptor skeleton with a proper DualSense report model
3. Add OUT report capture and feedback forwarding
4. Add Windows-side build and packaging integration
5. Validate with `Shadow` end-to-end

## Practical Testing Checklist

When the real UDE path is implemented, validate in this order:

1. `sunshinepadsvc.exe` starts and creates both named pipes
2. `dualsense_usb` backend reports available in `/api/gamepads/backends`
3. `sunshinepadsvc` can create slot 0 successfully
4. `SunshineDualSenseBus` accepts IOCTL create/update/destroy calls
5. Windows enumerates a new USB/HID game controller
6. Button and stick input reaches the game
7. Motion and touch reports flow correctly
8. Host output reports reach Shadow as:
   - rumble
   - RGB LED
   - adaptive triggers

