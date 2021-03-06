/*************************************************************************/ /*!
@File
@Title          RGX device node header file
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX device node
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if !defined(__RGXDEVICE_H__)
#define __RGXDEVICE_H__

#include "img_types.h"
#include "pvrsrv_device_types.h"
#include "mmu_common.h"
#include "rgx_fwif_km.h"
#include "rgx_fwif.h"
#include "rgxscript.h"
#include "cache_external.h"
#include "device.h"


/*!
 ******************************************************************************
 * Device state flags
 *****************************************************************************/
#define RGXKM_DEVICE_STATE_ZERO_FREELIST		(0x1 << 0)		/*!< Zeroing the physical pages of reconstructed freelists */

#define RGXFWIF_GPU_STATS_WINDOW_SIZE_US					1000000
#define RGXFWIF_GPU_STATS_MAX_VALUE_OF_STATE				10000
#define RGXFWIF_GPU_STATS_NUMBER_OF_RENDERS_BETWEEN_RECALC	10

typedef struct _PVRSRV_STUB_PBDESC_ PVRSRV_STUB_PBDESC;

typedef struct _PVRSRV_RGXDEV_INFO_
{
	PVRSRV_DEVICE_TYPE		eDeviceType;
	PVRSRV_DEVICE_CLASS		eDeviceClass;
	PVRSRV_DEVICE_NODE		*psDeviceNode;

	IMG_UINT8				ui8VersionMajor;
	IMG_UINT8				ui8VersionMinor;
	IMG_UINT32				ui32CoreConfig;
	IMG_UINT32				ui32CoreFlags;

	IMG_BOOL                bFirmwareInitialised;
	IMG_BOOL				bPDPEnabled;

	/* Kernel mode linear address of device registers */
	IMG_PVOID				pvRegsBaseKM;

	/* FIXME: The alloc for this should go through OSAllocMem in future */
	IMG_HANDLE				hRegMapping;

	/* System physical address of device registers*/
	IMG_CPU_PHYADDR			sRegsPhysBase;
	/*  Register region size in bytes */
	IMG_UINT32				ui32RegSize;

	PVRSRV_STUB_PBDESC		*psStubPBDescListKM;


	/* Firmware memory context info */
	DEVMEM_CONTEXT			*psKernelDevmemCtx;
	DEVMEM_HEAP				*psFirmwareHeap;
	MMU_CONTEXT				*psKernelMMUCtx;
	IMG_UINT32				ui32KernelCatBase;

	IMG_VOID				*pvDeviceMemoryHeap;
	
	/* Kernel CCBs */
	DEVMEM_MEMDESC			*apsKernelCCBCtlMemDesc[RGXFWIF_DM_MAX];	/*!< memdesc for kernel CCB control */
	RGXFWIF_CCB_CTL			*apsKernelCCBCtl[RGXFWIF_DM_MAX];			/*!< kernel CCB control kernel mapping */
	DEVMEM_MEMDESC			*apsKernelCCBMemDesc[RGXFWIF_DM_MAX];		/*!< memdesc for kernel CCB */
	IMG_UINT8				*apsKernelCCB[RGXFWIF_DM_MAX];				/*!< kernel CCB kernel mapping */

	/* Firmware CCBs */
	DEVMEM_MEMDESC			*apsFirmwareCCBCtlMemDesc[RGXFWIF_DM_MAX];	/*!< memdesc for Firmware CCB control */
	RGXFWIF_CCB_CTL			*apsFirmwareCCBCtl[RGXFWIF_DM_MAX];			/*!< kernel CCB control Firmware mapping */
	DEVMEM_MEMDESC			*apsFirmwareCCBMemDesc[RGXFWIF_DM_MAX];		/*!< memdesc for Firmware CCB */
	IMG_UINT8				*apsFirmwareCCB[RGXFWIF_DM_MAX];				/*!< kernel CCB Firmware mapping */

	/*
		if we don't preallocate the pagetables we must 
		insert newly allocated page tables dynamically 
	*/
	IMG_VOID				*pvMMUContextList;

	IMG_UINT32				ui32ClkGateStatusReg;
	IMG_UINT32				ui32ClkGateStatusMask;
	RGX_SCRIPTS				sScripts;

	DEVMEM_MEMDESC			*psRGXFWCodeMemDesc;
	DEVMEM_EXPORTCOOKIE		sRGXFWCodeExportCookie;

	DEVMEM_MEMDESC			*psRGXFWDataMemDesc;
	DEVMEM_EXPORTCOOKIE		sRGXFWDataExportCookie;

	DEVMEM_MEMDESC			*psRGXFWIfTraceBufCtlMemDesc;
	RGXFWIF_TRACEBUF		*psRGXFWIfTraceBuf;

	DEVMEM_MEMDESC			*psRGXFWIfHWRInfoBufCtlMemDesc;
	RGXFWIF_HWRINFOBUF		*psRGXFWIfHWRInfoBuf;

	DEVMEM_MEMDESC			*psRGXFWIfGpuUtilFWCbCtlMemDesc;
	RGXFWIF_GPU_UTIL_FWCB	*psRGXFWIfGpuUtilFWCb;

	DEVMEM_MEMDESC			*psRGXFWIfHWPerfBufCtlMemDesc;
	IMG_BYTE				*psRGXFWIfHWPerfBuf;
	IMG_UINT32				ui32RGXFWIfHWPerfBufSize; /* in bytes */
	
	DEVMEM_MEMDESC			*psRGXFWIfInitMemDesc;

#if defined(RGXFW_ALIGNCHECKS)
	DEVMEM_MEMDESC			*psRGXFWAlignChecksMemDesc;	
#endif

	DEVMEM_MEMDESC			*psRGXFWSigTAChecksMemDesc;	
	IMG_UINT32				ui32SigTAChecksSize;

	DEVMEM_MEMDESC			*psRGXFWSig3DChecksMemDesc;	
	IMG_UINT32				ui32Sig3DChecksSize;

	IMG_VOID				*pvLISRData;
	IMG_VOID				*pvMISRData;
	
	DEVMEM_MEMDESC			*psRGXBIFFaultAddressMemDesc;

#if defined(FIX_HW_BRN_37200)
	DEVMEM_MEMDESC			*psRGXFWHWBRN37200MemDesc;
#endif

#if defined (PDUMP)
	IMG_BOOL				abDumpedKCCBCtlAlready[RGXFWIF_DM_MAX];
	
#endif	

	/*! Handles to the lock and stream objects used to transport
	 * HWPerf data to user side clients. See RGXHWPerfInit() RGXHWPerfDeinit().
	 * Set during initialisation if the application hint turns bit 7
	 * 'Enable HWPerf' on in the ConfigFlags sent to the FW. FW stores this
	 * bit in the RGXFW_CTL.ui32StateFlags member. They may also get
	 * set by the API RGXCtrlHWPerf(). Thus these members may be 0 if HWPerf is
	 * not enabled as these members are created and destroyed on demand.
	 */
	POS_LOCK 				hLockHWPerfStream;
	IMG_HANDLE				hHWPerfStream;

	/* If we do 10 deferred memory allocations per second, then the ID would warp around after 13 years */
	IMG_UINT32				ui32ZSBufferCurrID;	/*!< ID assigned to the next deferred devmem allocation */
	IMG_UINT32				ui32FreelistCurrID;	/*!< ID assigned to the next freelist */

	POS_LOCK 				hLockZSBuffer;		/*!< Lock to protect simultaneous access to ZSBuffers */
	DLLIST_NODE				sZSBufferHead;		/*!< List of on-demand ZSBuffers */
	POS_LOCK 				hLockFreeList;		/*!< Lock to protect simultaneous access to Freelists */
	DLLIST_NODE				sFreeListHead;		/*!< List of growable Freelists */
	PSYNC_PRIM_CONTEXT		hSyncPrimContext;
	PVRSRV_CLIENT_SYNC_PRIM *psPowSyncPrim;

	IMG_VOID				(*pfnActivePowerCheck) (PVRSRV_DEVICE_NODE *psDeviceNode);
	IMG_UINT32				ui32ActivePMReqOk;
	IMG_UINT32				ui32ActivePMReqDenied;
	IMG_UINT32				ui32ActivePMReqTotal;
	
	IMG_HANDLE				hProcessQueuesMISR;

	IMG_UINT32 				ui32DeviceFlags;	/*!< Flags to track general device state  */

	/* Poll data for detecting firmware fatal errors */
	IMG_UINT32  aui32CrLastPollAddr[RGXFW_THREAD_NUM];
	IMG_UINT32  ui32KCCBLastROff[RGXFWIF_DM_MAX];
	IMG_UINT32  ui32LastGEOTimeouts;

	/* GPU DVFS History and GPU Utilization stats */
	RGXFWIF_GPU_DVFS_HIST	*psGpuDVFSHistory;
	IMG_VOID				(*pfnUpdateGpuUtilStats) (PVRSRV_DEVICE_NODE *psDeviceNode);
	IMG_UINT32				ui32GpuUtilTransitionsCountSample;
	IMG_UINT32				ui32GpuStatActive;	/* GPU active  ratio expressed in 0,01% units */
	IMG_UINT32				ui32GpuStatBlocked; /* GPU blocked ratio expressed in 0,01% units */
	IMG_UINT32				ui32GpuStatIdle;    /* GPU idle    ratio expressed in 0,01% units */

} PVRSRV_RGXDEV_INFO;



typedef struct _RGX_TIMING_INFORMATION_
{
	IMG_UINT32			ui32CoreClockSpeed;
	IMG_BOOL			bEnableActivePM;
	IMG_BOOL			bEnableRDPowIsland;
	IMG_UINT32			ui32ActivePMLatencyms;
} RGX_TIMING_INFORMATION;

typedef struct _RGX_DATA_
{
	/*! Timing information */
	RGX_TIMING_INFORMATION	*psRGXTimingInfo;
	IMG_BOOL bHasTDMetaCodePhysHeap;
	IMG_UINT32 uiTDMetaCodePhysHeapID;
} RGX_DATA;


/*
	RGX PDUMP register bank name (prefix)
*/
#define RGX_PDUMPREG_NAME		"RGXREG"

#endif /* __RGXDEVICE_H__ */
