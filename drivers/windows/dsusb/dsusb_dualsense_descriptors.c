/**
 * @file drivers/windows/dsusb/dsusb_dualsense_descriptors.c
 * @brief Static USB/HID descriptor data for the virtual DualSense device skeleton.
 */

#include "dsusb_dualsense_descriptors.h"

//
// This is a development skeleton descriptor set. It is intentionally minimal and
// does not yet attempt to mirror the full retail DualSense composite descriptor.
// The purpose is to provide a stable descriptor boundary for the future UDE path.
//

static const UCHAR g_dsusb_device_descriptor[] = {
  0x12,       // bLength
  0x01,       // bDescriptorType = Device
  0x00, 0x02, // bcdUSB = 2.00
  0x00,       // bDeviceClass
  0x00,       // bDeviceSubClass
  0x00,       // bDeviceProtocol
  0x40,       // bMaxPacketSize0
  0x4C, 0x05, // idVendor = 0x054C
  0xE6, 0x0C, // idProduct = 0x0CE6
  0x11, 0x81, // bcdDevice = 0x8111
  0x01,       // iManufacturer
  0x02,       // iProduct
  0x03,       // iSerialNumber
  0x01,       // bNumConfigurations
};

static const UCHAR g_dsusb_configuration_descriptor[] = {
  0x09,       // bLength
  0x02,       // bDescriptorType = Configuration
  0x22, 0x00, // wTotalLength
  0x01,       // bNumInterfaces
  0x01,       // bConfigurationValue
  0x00,       // iConfiguration
  0x80,       // bmAttributes
  0xFA,       // bMaxPower

  0x09,       // bLength
  0x04,       // bDescriptorType = Interface
  0x00,       // bInterfaceNumber
  0x00,       // bAlternateSetting
  0x02,       // bNumEndpoints
  0x03,       // bInterfaceClass = HID
  0x00,       // bInterfaceSubClass
  0x00,       // bInterfaceProtocol
  0x00,       // iInterface

  0x09,       // bLength
  0x21,       // bDescriptorType = HID
  0x11, 0x01, // bcdHID
  0x00,       // bCountryCode
  0x01,       // bNumDescriptors
  0x22,       // bDescriptorType = Report
  0x34, 0x00, // wDescriptorLength

  0x07,       // bLength
  0x05,       // bDescriptorType = Endpoint
  0x81,       // bEndpointAddress = IN 1
  0x03,       // bmAttributes = Interrupt
  0x40, 0x00, // wMaxPacketSize
  0x01,       // bInterval

  0x07,       // bLength
  0x05,       // bDescriptorType = Endpoint
  0x02,       // bEndpointAddress = OUT 2
  0x03,       // bmAttributes = Interrupt
  0x40, 0x00, // wMaxPacketSize
  0x01,       // bInterval
};

static const UCHAR g_dsusb_hid_report_descriptor[] = {
  0x05, 0x01,       // Usage Page (Generic Desktop)
  0x09, 0x05,       // Usage (Game Pad)
  0xA1, 0x01,       // Collection (Application)
  0x85, 0x01,       //   Report ID (1)
  0x09, 0x30,       //   Usage (X)
  0x09, 0x31,       //   Usage (Y)
  0x09, 0x32,       //   Usage (Z)
  0x09, 0x35,       //   Usage (Rz)
  0x15, 0x00,       //   Logical Minimum (0)
  0x26, 0xFF, 0x00, //   Logical Maximum (255)
  0x75, 0x08,       //   Report Size (8)
  0x95, 0x04,       //   Report Count (4)
  0x81, 0x02,       //   Input (Data,Var,Abs)
  0x05, 0x09,       //   Usage Page (Button)
  0x19, 0x01,       //   Usage Minimum (1)
  0x29, 0x10,       //   Usage Maximum (16)
  0x15, 0x00,       //   Logical Minimum (0)
  0x25, 0x01,       //   Logical Maximum (1)
  0x75, 0x01,       //   Report Size (1)
  0x95, 0x10,       //   Report Count (16)
  0x81, 0x02,       //   Input (Data,Var,Abs)
  0x85, 0x02,       //   Report ID (2)
  0x09, 0x21,       //   Usage (Set Effect Report)
  0x15, 0x00,       //   Logical Minimum (0)
  0x26, 0xFF, 0x00, //   Logical Maximum (255)
  0x75, 0x08,       //   Report Size (8)
  0x95, 0x20,       //   Report Count (32)
  0x91, 0x02,       //   Output (Data,Var,Abs)
  0xC0,             // End Collection
};

static DSUSB_DESCRIPTOR_BLOB g_device_blob = {g_dsusb_device_descriptor, sizeof(g_dsusb_device_descriptor)};
static DSUSB_DESCRIPTOR_BLOB g_configuration_blob = {g_dsusb_configuration_descriptor, sizeof(g_dsusb_configuration_descriptor)};
static DSUSB_DESCRIPTOR_BLOB g_hid_report_blob = {g_dsusb_hid_report_descriptor, sizeof(g_dsusb_hid_report_descriptor)};

PDSUSB_DESCRIPTOR_BLOB DsUsbGetDeviceDescriptor(VOID) {
  return &g_device_blob;
}

PDSUSB_DESCRIPTOR_BLOB DsUsbGetConfigurationDescriptor(VOID) {
  return &g_configuration_blob;
}

PDSUSB_DESCRIPTOR_BLOB DsUsbGetHidReportDescriptor(VOID) {
  return &g_hid_report_blob;
}
