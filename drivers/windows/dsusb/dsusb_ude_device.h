/**
 * @file drivers/windows/dsusb/dsusb_ude_device.h
 * @brief UDE-backed virtual DualSense device interface for the DSUSB bus driver.
 */
#pragma once

#include <ntddk.h>

#include "dsusb_bus_ioctl.h"

typedef struct _DSUSB_UDE_SLOT {
  BOOLEAN Created;
} DSUSB_UDE_SLOT, *PDSUSB_UDE_SLOT;

NTSTATUS DsUsbUdeCreate(PDSUSB_UDE_SLOT slot, PDSUSB_CREATE_DEVICE_PACKET packet);
VOID DsUsbUdeDestroy(PDSUSB_UDE_SLOT slot, LONG global_index);
NTSTATUS DsUsbUdeUpdateState(PDSUSB_UDE_SLOT slot, PDSUSB_UPDATE_STATE_PACKET packet);
NTSTATUS DsUsbUdeUpdateTouch(PDSUSB_UDE_SLOT slot, PDSUSB_UPDATE_TOUCH_PACKET packet);
NTSTATUS DsUsbUdeUpdateMotion(PDSUSB_UDE_SLOT slot, PDSUSB_UPDATE_MOTION_PACKET packet);
NTSTATUS DsUsbUdeUpdateBattery(PDSUSB_UDE_SLOT slot, PDSUSB_UPDATE_BATTERY_PACKET packet);
