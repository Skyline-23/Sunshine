/**
 * @file drivers/windows/dsusb/dsusb_ude_device.c
 * @brief Stub UDE-backed virtual DualSense device implementation for the DSUSB bus driver.
 */

#include "dsusb_ude_device.h"
#include "dsusb_dualsense_descriptors.h"

NTSTATUS DsUsbUdeCreate(PDSUSB_UDE_SLOT slot, PDSUSB_CREATE_DEVICE_PACKET packet) {
  PDSUSB_DESCRIPTOR_BLOB device_descriptor = DsUsbGetDeviceDescriptor();
  PDSUSB_DESCRIPTOR_BLOB configuration_descriptor = DsUsbGetConfigurationDescriptor();
  PDSUSB_DESCRIPTOR_BLOB hid_report_descriptor = DsUsbGetHidReportDescriptor();

  slot->Created = TRUE;
  slot->ClientRelativeIndex = packet->ClientRelativeIndex;
  slot->Type = packet->Type;
  slot->Capabilities = packet->Capabilities;
  slot->SupportedButtons = packet->SupportedButtons;
  slot->DeviceDescriptorLength = device_descriptor->Length;
  slot->ConfigurationDescriptorLength = configuration_descriptor->Length;
  slot->HidReportDescriptorLength = hid_report_descriptor->Length;
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
