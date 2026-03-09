/**
 * @file drivers/windows/dsusb/dsusb_bus_driver.c
 * @brief Stub control driver for the Sunshine DualSense USB bus.
 */

#include "dsusb_bus_ioctl.h"
#include "dsusb_ude_device.h"

#define DSUSB_DEVICE_NAME L"\\Device\\SunshineDualSenseBus"
#define DSUSB_DOS_DEVICE_NAME L"\\DosDevices\\SunshineDualSenseBus"
#define DSUSB_MAX_SLOTS 16

typedef struct _DSUSB_SLOT {
  BOOLEAN Allocated;
  UCHAR ClientRelativeIndex;
  UCHAR Type;
  USHORT Capabilities;
  ULONG SupportedButtons;
  DSUSB_UDE_SLOT UdeSlot;
  DSUSB_UPDATE_STATE_PACKET State;
  DSUSB_UPDATE_TOUCH_PACKET Touch;
  DSUSB_UPDATE_MOTION_PACKET Motion;
  DSUSB_UPDATE_BATTERY_PACKET Battery;
} DSUSB_SLOT, *PDSUSB_SLOT;

typedef struct _DSUSB_DEVICE_EXTENSION {
  FAST_MUTEX Lock;
  DSUSB_SLOT Slots[DSUSB_MAX_SLOTS];
} DSUSB_DEVICE_EXTENSION, *PDSUSB_DEVICE_EXTENSION;

DRIVER_UNLOAD DsUsbUnload;
DRIVER_DISPATCH DsUsbCreateClose;
DRIVER_DISPATCH DsUsbDeviceControl;

static NTSTATUS DsUsbValidateSlotIndex(LONG slot_index) {
  if (slot_index < 0 || slot_index >= DSUSB_MAX_SLOTS) {
    return STATUS_INVALID_PARAMETER;
  }

  return STATUS_SUCCESS;
}

static VOID DsUsbCompleteIrp(PIRP irp, NTSTATUS status, ULONG_PTR information) {
  irp->IoStatus.Status = status;
  irp->IoStatus.Information = information;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
}

static NTSTATUS DsUsbHandleGetVersion(PVOID system_buffer, ULONG in_length, ULONG out_length, ULONG_PTR *information) {
  UNREFERENCED_PARAMETER(in_length);

  if (system_buffer == NULL || out_length < sizeof(DSUSB_VERSION_PACKET)) {
    return STATUS_BUFFER_TOO_SMALL;
  }

  ((PDSUSB_VERSION_PACKET) system_buffer)->Version = DSUSB_BUS_VERSION;
  *information = sizeof(DSUSB_VERSION_PACKET);
  return STATUS_SUCCESS;
}

static NTSTATUS DsUsbHandleCreateDevice(PDSUSB_DEVICE_EXTENSION extension, PDSUSB_CREATE_DEVICE_PACKET packet) {
  NTSTATUS status = DsUsbValidateSlotIndex(packet->GlobalIndex);
  if (!NT_SUCCESS(status)) {
    return status;
  }

  ExAcquireFastMutex(&extension->Lock);
  extension->Slots[packet->GlobalIndex].Allocated = TRUE;
  extension->Slots[packet->GlobalIndex].ClientRelativeIndex = packet->ClientRelativeIndex;
  extension->Slots[packet->GlobalIndex].Type = packet->Type;
  extension->Slots[packet->GlobalIndex].Capabilities = packet->Capabilities;
  extension->Slots[packet->GlobalIndex].SupportedButtons = packet->SupportedButtons;
  RtlZeroMemory(&extension->Slots[packet->GlobalIndex].State, sizeof(DSUSB_UPDATE_STATE_PACKET));
  RtlZeroMemory(&extension->Slots[packet->GlobalIndex].Touch, sizeof(DSUSB_UPDATE_TOUCH_PACKET));
  RtlZeroMemory(&extension->Slots[packet->GlobalIndex].Motion, sizeof(DSUSB_UPDATE_MOTION_PACKET));
  RtlZeroMemory(&extension->Slots[packet->GlobalIndex].Battery, sizeof(DSUSB_UPDATE_BATTERY_PACKET));
  RtlZeroMemory(&extension->Slots[packet->GlobalIndex].UdeSlot, sizeof(DSUSB_UDE_SLOT));
  status = DsUsbUdeCreate(&extension->Slots[packet->GlobalIndex].UdeSlot, packet);
  if (!NT_SUCCESS(status)) {
    RtlZeroMemory(&extension->Slots[packet->GlobalIndex], sizeof(DSUSB_SLOT));
  }
  ExReleaseFastMutex(&extension->Lock);

  return status;
}

static NTSTATUS DsUsbHandleDestroyDevice(PDSUSB_DEVICE_EXTENSION extension, PDSUSB_DESTROY_DEVICE_PACKET packet) {
  NTSTATUS status = DsUsbValidateSlotIndex(packet->GlobalIndex);
  if (!NT_SUCCESS(status)) {
    return status;
  }

  ExAcquireFastMutex(&extension->Lock);
  DsUsbUdeDestroy(&extension->Slots[packet->GlobalIndex].UdeSlot, packet->GlobalIndex);
  RtlZeroMemory(&extension->Slots[packet->GlobalIndex], sizeof(DSUSB_SLOT));
  ExReleaseFastMutex(&extension->Lock);

  return STATUS_SUCCESS;
}

static NTSTATUS DsUsbHandleUpdateState(PDSUSB_DEVICE_EXTENSION extension, PDSUSB_UPDATE_STATE_PACKET packet) {
  NTSTATUS status = DsUsbValidateSlotIndex(packet->GlobalIndex);
  if (!NT_SUCCESS(status)) {
    return status;
  }

  ExAcquireFastMutex(&extension->Lock);
  if (!extension->Slots[packet->GlobalIndex].Allocated) {
    ExReleaseFastMutex(&extension->Lock);
    return STATUS_DEVICE_DOES_NOT_EXIST;
  }
  extension->Slots[packet->GlobalIndex].State = *packet;
  status = DsUsbUdeUpdateState(&extension->Slots[packet->GlobalIndex].UdeSlot, packet);
  ExReleaseFastMutex(&extension->Lock);
  return status;
}

static NTSTATUS DsUsbHandleUpdateTouch(PDSUSB_DEVICE_EXTENSION extension, PDSUSB_UPDATE_TOUCH_PACKET packet) {
  NTSTATUS status = DsUsbValidateSlotIndex(packet->GlobalIndex);
  if (!NT_SUCCESS(status)) {
    return status;
  }

  ExAcquireFastMutex(&extension->Lock);
  if (!extension->Slots[packet->GlobalIndex].Allocated) {
    ExReleaseFastMutex(&extension->Lock);
    return STATUS_DEVICE_DOES_NOT_EXIST;
  }
  extension->Slots[packet->GlobalIndex].Touch = *packet;
  status = DsUsbUdeUpdateTouch(&extension->Slots[packet->GlobalIndex].UdeSlot, packet);
  ExReleaseFastMutex(&extension->Lock);
  return status;
}

static NTSTATUS DsUsbHandleUpdateMotion(PDSUSB_DEVICE_EXTENSION extension, PDSUSB_UPDATE_MOTION_PACKET packet) {
  NTSTATUS status = DsUsbValidateSlotIndex(packet->GlobalIndex);
  if (!NT_SUCCESS(status)) {
    return status;
  }

  ExAcquireFastMutex(&extension->Lock);
  if (!extension->Slots[packet->GlobalIndex].Allocated) {
    ExReleaseFastMutex(&extension->Lock);
    return STATUS_DEVICE_DOES_NOT_EXIST;
  }
  extension->Slots[packet->GlobalIndex].Motion = *packet;
  status = DsUsbUdeUpdateMotion(&extension->Slots[packet->GlobalIndex].UdeSlot, packet);
  ExReleaseFastMutex(&extension->Lock);
  return status;
}

static NTSTATUS DsUsbHandleUpdateBattery(PDSUSB_DEVICE_EXTENSION extension, PDSUSB_UPDATE_BATTERY_PACKET packet) {
  NTSTATUS status = DsUsbValidateSlotIndex(packet->GlobalIndex);
  if (!NT_SUCCESS(status)) {
    return status;
  }

  ExAcquireFastMutex(&extension->Lock);
  if (!extension->Slots[packet->GlobalIndex].Allocated) {
    ExReleaseFastMutex(&extension->Lock);
    return STATUS_DEVICE_DOES_NOT_EXIST;
  }
  extension->Slots[packet->GlobalIndex].Battery = *packet;
  status = DsUsbUdeUpdateBattery(&extension->Slots[packet->GlobalIndex].UdeSlot, packet);
  ExReleaseFastMutex(&extension->Lock);
  return status;
}

VOID DsUsbUnload(PDRIVER_OBJECT driver_object) {
  UNICODE_STRING dos_device_name;

  RtlInitUnicodeString(&dos_device_name, DSUSB_DOS_DEVICE_NAME);
  IoDeleteSymbolicLink(&dos_device_name);

  if (driver_object->DeviceObject != NULL) {
    IoDeleteDevice(driver_object->DeviceObject);
  }
}

NTSTATUS DsUsbCreateClose(PDEVICE_OBJECT device_object, PIRP irp) {
  UNREFERENCED_PARAMETER(device_object);
  DsUsbCompleteIrp(irp, STATUS_SUCCESS, 0);
  return STATUS_SUCCESS;
}

NTSTATUS DsUsbDeviceControl(PDEVICE_OBJECT device_object, PIRP irp) {
  PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
  PDSUSB_DEVICE_EXTENSION extension = (PDSUSB_DEVICE_EXTENSION) device_object->DeviceExtension;
  PVOID system_buffer = irp->AssociatedIrp.SystemBuffer;
  ULONG in_length = stack->Parameters.DeviceIoControl.InputBufferLength;
  ULONG out_length = stack->Parameters.DeviceIoControl.OutputBufferLength;
  ULONG_PTR information = 0;
  NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

  switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_DSUSB_GET_VERSION:
      status = DsUsbHandleGetVersion(system_buffer, in_length, out_length, &information);
      break;
    case IOCTL_DSUSB_CREATE_DEVICE:
      if (system_buffer != NULL && in_length >= sizeof(DSUSB_CREATE_DEVICE_PACKET)) {
        status = DsUsbHandleCreateDevice(extension, (PDSUSB_CREATE_DEVICE_PACKET) system_buffer);
      } else {
        status = STATUS_BUFFER_TOO_SMALL;
      }
      break;
    case IOCTL_DSUSB_DESTROY_DEVICE:
      if (system_buffer != NULL && in_length >= sizeof(DSUSB_DESTROY_DEVICE_PACKET)) {
        status = DsUsbHandleDestroyDevice(extension, (PDSUSB_DESTROY_DEVICE_PACKET) system_buffer);
      } else {
        status = STATUS_BUFFER_TOO_SMALL;
      }
      break;
    case IOCTL_DSUSB_UPDATE_STATE:
      if (system_buffer != NULL && in_length >= sizeof(DSUSB_UPDATE_STATE_PACKET)) {
        status = DsUsbHandleUpdateState(extension, (PDSUSB_UPDATE_STATE_PACKET) system_buffer);
      } else {
        status = STATUS_BUFFER_TOO_SMALL;
      }
      break;
    case IOCTL_DSUSB_UPDATE_TOUCH:
      if (system_buffer != NULL && in_length >= sizeof(DSUSB_UPDATE_TOUCH_PACKET)) {
        status = DsUsbHandleUpdateTouch(extension, (PDSUSB_UPDATE_TOUCH_PACKET) system_buffer);
      } else {
        status = STATUS_BUFFER_TOO_SMALL;
      }
      break;
    case IOCTL_DSUSB_UPDATE_MOTION:
      if (system_buffer != NULL && in_length >= sizeof(DSUSB_UPDATE_MOTION_PACKET)) {
        status = DsUsbHandleUpdateMotion(extension, (PDSUSB_UPDATE_MOTION_PACKET) system_buffer);
      } else {
        status = STATUS_BUFFER_TOO_SMALL;
      }
      break;
    case IOCTL_DSUSB_UPDATE_BATTERY:
      if (system_buffer != NULL && in_length >= sizeof(DSUSB_UPDATE_BATTERY_PACKET)) {
        status = DsUsbHandleUpdateBattery(extension, (PDSUSB_UPDATE_BATTERY_PACKET) system_buffer);
      } else {
        status = STATUS_BUFFER_TOO_SMALL;
      }
      break;
  }

  DsUsbCompleteIrp(irp, status, information);
  return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path) {
  UNREFERENCED_PARAMETER(registry_path);

  UNICODE_STRING device_name;
  UNICODE_STRING dos_device_name;
  PDEVICE_OBJECT device_object = NULL;
  NTSTATUS status;

  RtlInitUnicodeString(&device_name, DSUSB_DEVICE_NAME);
  status = IoCreateDevice(
    driver_object,
    sizeof(DSUSB_DEVICE_EXTENSION),
    &device_name,
    FILE_DEVICE_UNKNOWN,
    FILE_DEVICE_SECURE_OPEN,
    FALSE,
    &device_object
  );
  if (!NT_SUCCESS(status)) {
    return status;
  }

  RtlInitUnicodeString(&dos_device_name, DSUSB_DOS_DEVICE_NAME);
  status = IoCreateSymbolicLink(&dos_device_name, &device_name);
  if (!NT_SUCCESS(status)) {
    IoDeleteDevice(device_object);
    return status;
  }

  RtlZeroMemory(device_object->DeviceExtension, sizeof(DSUSB_DEVICE_EXTENSION));
  ExInitializeFastMutex(&((PDSUSB_DEVICE_EXTENSION) device_object->DeviceExtension)->Lock);

  driver_object->DriverUnload = DsUsbUnload;
  driver_object->MajorFunction[IRP_MJ_CREATE] = DsUsbCreateClose;
  driver_object->MajorFunction[IRP_MJ_CLOSE] = DsUsbCreateClose;
  driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DsUsbDeviceControl;

  device_object->Flags &= ~DO_DEVICE_INITIALIZING;
  return STATUS_SUCCESS;
}
