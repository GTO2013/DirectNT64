// Gerätetreiber zum direkten Zugriff auf Systemressourcen
// unter Windows NT 4.0, c't 1/97 Matthias Witthopf, Andreas Stiller

#include <ntddk.h>
#include "directnt.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include<Ntstrsafe.h>

#define DIRECTNT_DEVICE_NAME L"\\Device\\DirectNT"
#define DOS_DEVICE_NAME      L"\\DosDevices\\Dev_DirectNT"
#define IOPM_SIZE 0x2000

typedef UCHAR IOPMTYP[IOPM_SIZE];
typedef struct {
	ULONG Dummy;
	IOPMTYP iopm;
} TLocalDevInfo, * PLocalDevInfo;

extern "C" void SetIOPermissionMap(int);
extern "C" void EachProcessorDpc(KDPC*, PVOID, PVOID, PVOID);

//64 Bit Code from: https://www-user.tu-chemnitz.de/~heha/viewzip.cgi/hs/giveio64.zip/
static void EachProcessor(void(*Callback)(void*), void* Arg) {
	ULONG i;
	volatile KAFFINITY a;
	a = KeQueryActiveProcessors();
	if (a == 1) Callback(Arg);			// Einzelprozessorsystem (Abk.)
	else {
		KAFFINITY m;
		KDPC* Dpc, * pDpc;
		Dpc = (KDPC*)ExAllocatePoolWithTag(NonPagedPool, sizeof(KDPC) * MAXIMUM_PROCESSORS, 'tplE');
		if (!Dpc) return;
		for (i = 0, m = 1, pDpc = Dpc; a >= m && i < MAXIMUM_PROCESSORS; i++, m <<= 1, pDpc++) if (a & m) {
			if (i == KeGetCurrentProcessorNumber()) EachProcessorDpc(NULL, Callback, Arg, (void*)& a);
			else {
				KeInitializeDpc(pDpc, EachProcessorDpc, Callback);
				KeSetTargetProcessorDpc(pDpc, (char)i);
				KeInsertQueueDpc(pDpc, Arg, (void*)& a);
			}
		}
		while (a);
		ExFreePoolWithTag(Dpc, 'tplE');
	}
}

static void GiveIO(int OnFlag) {
	EachProcessor((void(*)(void*))SetIOPermissionMap, (void*)(__int64)OnFlag);
}

static NTSTATUS OnCreate(PDEVICE_OBJECT o, PIRP I) {
	I->IoStatus.Information = 0;
	I->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(I, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS DirectNT_CreateDevice(IN  PWSTR PrototypeName, IN  DEVICE_TYPE DeviceType, IN  PDRIVER_OBJECT DriverObject, OUT PDEVICE_OBJECT* ppDevObj)
{
	UNICODE_STRING NtDeviceName;
	UNICODE_STRING Win32DeviceName;
	RtlInitUnicodeString(&NtDeviceName, PrototypeName);
	NTSTATUS Status = IoCreateDevice(DriverObject, sizeof(TLocalDevInfo),
		&NtDeviceName, DeviceType, 0, FALSE, ppDevObj);
	if (!NT_SUCCESS(Status)) return Status;
	RtlZeroMemory((*ppDevObj)->DeviceExtension, sizeof(TLocalDevInfo));
	RtlInitUnicodeString(&Win32DeviceName, DOS_DEVICE_NAME);
	Status = IoCreateSymbolicLink(&Win32DeviceName, &NtDeviceName);
	if (!NT_SUCCESS(Status))
		IoDeleteDevice(*ppDevObj);
	return Status;
}

static NTSTATUS OnPower(PDEVICE_OBJECT o, PIRP I) {
	IO_STACK_LOCATION* irpStack;
	PoStartNextPowerIrp(I);
	irpStack = IoGetCurrentIrpStackLocation(I);
	if (irpStack->MinorFunction == IRP_MN_SET_POWER
		&& irpStack->Parameters.Power.Type == SystemPowerState
		&& irpStack->Parameters.Power.State.SystemState == PowerDeviceD0) {
		GiveIO(TRUE);	
	}
	return STATUS_SUCCESS;
}

NTSTATUS DirectNT_Control(IN PLocalDevInfo pLDI, IN PIRP pIrp, IN PIO_STACK_LOCATION IrpStack, IN ULONG IoctlCode)
{
	PDirectNTInfo InBuf = (PDirectNTInfo)pIrp->AssociatedIrp.SystemBuffer;
	ULONG OpCode = InBuf->OpCode;
	void* OutBuf = (void*)pIrp->AssociatedIrp.SystemBuffer;

	switch (OpCode) {

	case OP_Check:
		*(PULONG)OutBuf = 0x12345678;
		pIrp->IoStatus.Information = sizeof(ULONG);
		return STATUS_SUCCESS;

	case OP_GiveIO:
		GiveIO(1);
		pIrp->IoStatus.Information = 0;
		return STATUS_SUCCESS;
	}

	return STATUS_INVALID_PARAMETER;
}

NTSTATUS DirectNT_Dispatch(IN PDEVICE_OBJECT pDO, IN PIRP pIrp)
{
	pIrp->IoStatus.Information = 0;  // Anzahl Rückgabe-Bytes

	PLocalDevInfo      pLDI = (PLocalDevInfo)pDO->DeviceExtension;
	PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	NTSTATUS           Status = STATUS_NOT_IMPLEMENTED;

	switch (pIrpStack->MajorFunction) {
	case IRP_MJ_CREATE:
	case IRP_MJ_CLOSE:
		Status = STATUS_SUCCESS;
		break;
	case IRP_MJ_DEVICE_CONTROL:
		switch (pIrpStack->Parameters.DeviceIoControl.IoControlCode) {
		case IOCTL_DIRECTNT_CONTROL:
			Status = DirectNT_Control(pLDI, pIrp, pIrpStack, pIrpStack->Parameters.DeviceIoControl.IoControlCode);
			break;
		}
		break;
	}
	pIrp->IoStatus.Status = Status;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return Status;
}

VOID DirectNT_Unload(PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING Win32DeviceName;
	RtlInitUnicodeString(&Win32DeviceName, DOS_DEVICE_NAME);
	IoDeleteSymbolicLink(&Win32DeviceName);

	GiveIO(0);
	IoDeleteDevice(DriverObject->DeviceObject);
}

extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath)
{
	DriverObject->MajorFunction[IRP_MJ_CREATE] = OnCreate;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DirectNT_Dispatch;
	DriverObject->DriverUnload = DirectNT_Unload;
	DriverObject->MajorFunction[IRP_MJ_POWER] = OnPower;

	PDEVICE_OBJECT DeviceObject;
	return DirectNT_CreateDevice(DIRECTNT_DEVICE_NAME, DIRECTNT_TYPE, DriverObject, &DeviceObject);
}

