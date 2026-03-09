/**
 * @file drivers/windows/dsusb/dsusb_ude_device.c
 * @brief Stub UDE-backed virtual DualSense device implementation for the DSUSB bus driver.
 */

#include "dsusb_ude_device.h"

NTSTATUS DsUsbUdeCreate(PDSUSB_UDE_SLOT slot, PDSUSB_CREATE_DEVICE_PACKET packet) {
  UNREFERENCED_PARAMETER(packet);
  slot->Created = TRUE;
  return STATUS_SUCCESS;
}

VOID DsUsbUdeDestroy(PDSUSB_UDE_SLOT slot, LONG global_index) {
  UNREFERENCED_PARAMETER(global_index);
  slot->Created = FALSE;
}

NTSTATUS DsUsbUdeUpdateState(PDSUSB_UDE_SLOT slot, PDSUSB_UPDATE_STATE_PACKET packet) {
  UNREFERENCED_PARAMETER(packet);
  if (!slot->Created) {
    return STATUS_DEVICE_DOES_NOT_EXIST;
  }
  return STATUS_SUCCESS;
}

NTSTATUS DsUsbUdeUpdateTouch(PDSUSB_UDE_SLOT slot, PDSUSB_UPDATE_TOUCH_PACKET packet) {
  UNREFERENCED_PARAMETER(packet);
  if (!slot->Created) {
    return STATUS_DEVICE_DOES_NOT_EXIST;
  }
  return STATUS_SUCCESS;
}

NTSTATUS DsUsbUdeUpdateMotion(PDSUSB_UDE_SLOT slot, PDSUSB_UPDATE_MOTION_PACKET packet) {
  UNREFERENCED_PARAMETER(packet);
  if (!slot->Created) {
    return STATUS_DEVICE_DOES_NOT_EXIST;
  }
  return STATUS_SUCCESS;
}

NTSTATUS DsUsbUdeUpdateBattery(PDSUSB_UDE_SLOT slot, PDSUSB_UPDATE_BATTERY_PACKET packet) {
  UNREFERENCED_PARAMETER(packet);
  if (!slot->Created) {
    return STATUS_DEVICE_DOES_NOT_EXIST;
  }
  return STATUS_SUCCESS;
}
