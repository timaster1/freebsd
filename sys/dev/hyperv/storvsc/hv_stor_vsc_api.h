#ifndef __HV_STORVSC_API_H__
#define __HV_STORVSC_API_H__
/*
 * File referenced in MS Hyper-v code, but missing.
 */

#include <hv_osd.h>
#include <hv_vmbus_api.h>
#include "hv_vstorage.h"
#include <hv_list.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>

#define MAX_MULTIPAGE_BUFFER_PACKET (4096)
#define STORVSC_RINGBUFFER_SIZE		(20*PAGE_SIZE)
#define STORVSC_MAX_LUNS_PER_TARGET	(64)
#define STORVSC_MAX_IO_REQUESTS		(STORVSC_MAX_LUNS_PER_TARGET * 2)
#define BLKVSC_MAX_IDE_DISKS_PER_TARGET	(2)
#define BLKVSC_MAX_IO_REQUESTS		STORVSC_MAX_IO_REQUESTS
#define STORVSC_MAX_TARGETS		(1)

struct storvsc_softc;

enum storvsc_request_type {
	WRITE_TYPE,
	READ_TYPE,
	UNKNOWN_TYPE
};

typedef struct storvsc_request_s  STORVSC_REQUEST;
struct storvsc_request_s {
	LIST_ENTRY ListEntry;
	void *Extension;
	ULONG Host;
	UCHAR TargetId;
	UCHAR PathId;
	UCHAR LunId;
	UCHAR Bus;
	UCHAR CdbLen;
	UCHAR Cdb[CDB16GENERIC_LENGTH];
	enum storvsc_request_type Type;
	MULTIPAGE_BUFFER DataBuffer;
	void (*OnIOCompletion)(	STORVSC_REQUEST *request);
	UCHAR Status;
	UCHAR SenseBufferSize;
	PVOID SenseBuffer;
	union ccb *Ccb;
	struct storvsc_softc *Softc;
	ULONG BytesXfer;
};
typedef struct storvsc_driver_object_s {
	DRIVER_OBJECT Base;
	UINT32 RingBufferSize;
	UINT32 RequestExtSize;
	UINT32 MaxOutstandingRequestsPerChannel;
       int (*OnHostReset)(DEVICE_OBJECT *Device);
      int (*OnIORequest)(DEVICE_OBJECT *Device, STORVSC_REQUEST *Request);
} STORVSC_DRIVER_OBJECT;

typedef struct storvsc_device_info STORVSC_DEVICE_INFO;
struct storvsc_device_info {
	ULONG PortNumber;
	UCHAR PathId;
	UCHAR TargetId;
};

int StorVscInitialize( DRIVER_OBJECT *Driver);
int BlkVscInitialize( DRIVER_OBJECT *Driver);

#endif /* __HV_STORVSC_API_H__ */
