/**
 * @file drivers/windows/dsusb/dsusb_dualsense_descriptors.h
 * @brief Static USB/HID descriptor data for the virtual DualSense device skeleton.
 */
#pragma once

#include <ntddk.h>

#define DSUSB_DUALSENSE_VENDOR_ID 0x054C
#define DSUSB_DUALSENSE_PRODUCT_ID 0x0CE6
#define DSUSB_DUALSENSE_BCD_DEVICE 0x8111

typedef struct _DSUSB_DESCRIPTOR_BLOB {
  const UCHAR *Buffer;
  ULONG Length;
} DSUSB_DESCRIPTOR_BLOB, *PDSUSB_DESCRIPTOR_BLOB;

PDSUSB_DESCRIPTOR_BLOB DsUsbGetDeviceDescriptor(VOID);
PDSUSB_DESCRIPTOR_BLOB DsUsbGetConfigurationDescriptor(VOID);
PDSUSB_DESCRIPTOR_BLOB DsUsbGetHidReportDescriptor(VOID);
