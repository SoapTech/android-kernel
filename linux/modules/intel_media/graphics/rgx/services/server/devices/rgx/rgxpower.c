/*************************************************************************/ /*!
@File
@Title          Device specific power routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Device specific functions
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

#include <stddef.h>

#include "rgxpower.h"
#include "rgx_fwif_km.h"
#include "rgxutils.h"
#include "rgxfwutils.h"
#include "pdump_km.h"
#include "rgxdefs_km.h"
#include "pvrsrv.h"
#include "pvr_debug.h"
#include "osfunc.h"
#include "rgxdebug.h"
#include "rgx_meta.h"


/*!
*******************************************************************************

 @Function	RGXEnableClocks

 @Description Enable RGX Clocks

 @Input psDevInfo - device info structure

 @Return   IMG_VOID

******************************************************************************/
static IMG_VOID RGXEnableClocks(PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGX clock: use default (automatic clock gating)");
}


/*!
*******************************************************************************

 @Function	RGXInitSLC

 @Description Initialise RGX SLC

 @Input psDevInfo - device info structure

 @Return   IMG_VOID

******************************************************************************/
static IMG_VOID RGXInitSLC(PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	IMG_UINT32	ui32Reg;
	IMG_UINT32	ui32RegVal;

#if defined(FIX_HW_BRN_36492)
	/* Because the WA for this BRN forbids using SLC reset, need to inval it instead */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "Invalidate the SLC");
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, 
			RGX_CR_SLC_CTRL_FLUSH_INVAL, RGX_CR_SLC_CTRL_FLUSH_INVAL_ALL_EN);
	PDUMPREG32(RGX_PDUMPREG_NAME, 
			RGX_CR_SLC_CTRL_FLUSH_INVAL, RGX_CR_SLC_CTRL_FLUSH_INVAL_ALL_EN, 
			PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

	/* poll for completion */
	PVRSRVPollForValueKM((IMG_UINT32 *)((IMG_UINT8*)psDevInfo->pvRegsBaseKM + RGX_CR_SLC_STATUS0),
							 0x0,
							 RGX_CR_SLC_STATUS0_INVAL_PENDING_EN);

	PDUMPREGPOL(RGX_PDUMPREG_NAME,
				RGX_CR_SLC_STATUS0,
				0x0,
				RGX_CR_SLC_STATUS0_INVAL_PENDING_EN,
				PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS,
				PDUMP_POLL_OPERATOR_EQUAL);
#endif
	 
	/*
	 * SLC Bypass control
	 */
	ui32Reg = RGX_CR_SLC_CTRL_BYPASS;
	ui32RegVal = 0x0;

	if (!PVRSRVSystemSnoopingOfCPUCache() && !PVRSRVSystemSnoopingOfDeviceCache())
	{
		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "System has NO cache snooping");
	}
	else
	{
		if (PVRSRVSystemSnoopingOfCPUCache())
		{
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "System has CPU cache snooping");
		}
		if (PVRSRVSystemSnoopingOfDeviceCache())
		{
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "System has DEVICE cache snooping");
		}
	}

	/* Bypass SLC for textures if the SLC size is less than 128kB */
#if (RGX_FEATURE_SLC_SIZE_IN_BYTES < (128*1024))
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "Bypass SLC for TPU");
	ui32RegVal |= RGX_CR_SLC_CTRL_BYPASS_REQ_TPU_EN;
#endif

	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, ui32Reg, ui32RegVal);
	PDUMPREG32(RGX_PDUMPREG_NAME, ui32Reg, ui32RegVal, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

	/*
	 * SLC Bypass control
	 */
	ui32Reg = RGX_CR_SLC_CTRL_MISC;
	ui32RegVal = RGX_CR_SLC_CTRL_MISC_ADDR_DECODE_MODE_PVR_HASH1;

	/* Bypass burst combiner if SLC line size is smaller than 1024 bits */
#if (RGX_FEATURE_SLC_CACHE_LINE_SIZE_BITS < 1024)
	ui32RegVal |= RGX_CR_SLC_CTRL_MISC_BYPASS_BURST_COMBINER_EN;
#endif
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, ui32Reg, ui32RegVal);
	PDUMPREG32(RGX_PDUMPREG_NAME, ui32Reg, ui32RegVal, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
}


/*!
*******************************************************************************

 @Function	RGXInitBIF

 @Description Initialise RGX BIF

 @Input psDevInfo - device info structure

 @Return   IMG_VOID

******************************************************************************/
static IMG_VOID RGXInitBIF(PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	PVRSRV_ERROR	eError;
	IMG_DEV_PHYADDR sPCAddr;

	/*
		Acquire the address of the Kernel Page Catalogue.
	*/
	eError = MMU_AcquireBaseAddr(psDevInfo->psKernelMMUCtx, &sPCAddr);
	PVR_ASSERT(eError == PVRSRV_OK);

	/*
		Write the kernel catalogue base.
	*/
	/* FIXME: There should be a RGX_CR macro for getting the address of the cat base */
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, psDevInfo->ui32KernelCatBase, sPCAddr.uiAddr);

	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGX firmware MMU Page Catalogue");
	MMU_PDumpWritePageCatBase(psDevInfo->psKernelMMUCtx,
							  RGX_PDUMPREG_NAME,
							  psDevInfo->ui32KernelCatBase,
							  PDUMP_FLAGS_POWERTRANS);
	/*
	 * Trusted META boot
	 */
#if defined(SUPPORT_TRUSTED_DEVICE)
	#if TRUSTED_DEVICE_DEFAULT_ENABLED == 1
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Trusted Device enabled");
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_BIF_TRUST, RGX_CR_BIF_TRUST_ENABLE_EN);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_BIF_TRUST, RGX_CR_BIF_TRUST_ENABLE_EN, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_SYS_BUS_SECURE, RGX_CR_SYS_BUS_SECURE_ENABLE_EN);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_SYS_BUS_SECURE, RGX_CR_SYS_BUS_SECURE_ENABLE_EN, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	#else /* ! defined(TRUSTED_DEVICE_DEFAULT_ENABLED) */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Trusted Device disabled");
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_BIF_TRUST, 0);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_BIF_TRUST, 0, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_SYS_BUS_SECURE, 0);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_SYS_BUS_SECURE, 0, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	#endif /* TRUSTED_DEVICE_DEFAULT_ENABLED */
#endif

}

#if defined(RGX_FEATURE_AXI_ACELITE)
/*!
*******************************************************************************

 @Function	RGXAXIACELiteInit

 @Description Initialise AXI-ACE Lite interface

 @Input psDevInfo - device info structure

 @Return   IMG_VOID

******************************************************************************/
static IMG_VOID RGXAXIACELiteInit(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	IMG_UINT32 ui32RegAddr;
	IMG_UINT64 ui64RegVal;

	ui32RegAddr = RGX_CR_AXI_ACE_LITE_CONFIGURATION;

	/* Setup AXI-ACE config. Set everything to outer cache */
	ui64RegVal =   (3U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_NON_SNOOPING_SHIFT) |
				   (3U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_NON_SNOOPING_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_CACHE_MAINTENANCE_SHIFT)  |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_COHERENT_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_COHERENT_SHIFT) |
				   (((IMG_UINT64) 1) << RGX_CR_AXI_ACE_LITE_CONFIGURATION_DISABLE_COHERENT_WRITELINEUNIQUE_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWCACHE_COHERENT_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_COHERENT_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_CACHE_MAINTENANCE_SHIFT);

	OSWriteHWReg64(psDevInfo->pvRegsBaseKM,
				   ui32RegAddr,
				   ui64RegVal);
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "Init AXI-ACE interface");
	PDUMPREG64(RGX_PDUMPREG_NAME, ui32RegAddr, ui64RegVal, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
}
#endif

/*!
*******************************************************************************

 @Function	RGXStart

 @Description

 (client invoked) chip-reset and initialisation

 @Input psDevInfo - device info structure

 @Return   PVRSRV_ERROR

******************************************************************************/
static PVRSRV_ERROR RGXStart(PVRSRV_RGXDEV_INFO	*psDevInfo, PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR	eError = PVRSRV_OK;
	RGXFWIF_INIT	*psRGXFWInit;

	/* Set RGX in soft-reset */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: soft reset everything");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_MASKFULL);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_MASKFULL, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

	/* Take Rascal and Dust out of reset */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Rascal and Dust out of reset");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_MASKFULL ^ RGX_CR_SOFT_RESET_RASCALDUSTS_EN);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_MASKFULL ^ RGX_CR_SOFT_RESET_RASCALDUSTS_EN, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

	/* Read soft-reset to fence previos write in order to clear the SOCIF pipeline */
	(IMG_VOID) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	PDUMPREGREAD64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

	/* Take everything out of reset but META */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Take everything out of reset but META");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_GARTEN_EN);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_GARTEN_EN, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

	/*
	 * Enable clocks.
	 */
	RGXEnableClocks(psDevInfo);

	/*
	 * Initialise SLC.
	 */
	RGXInitSLC(psDevInfo);

#if !defined(SUPPORT_META_SLAVE_BOOT)
	/* Configure META to Master boot */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: META Master boot");
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_META_BOOT, RGX_CR_META_BOOT_MODE_EN);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_META_BOOT, RGX_CR_META_BOOT_MODE_EN, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
#endif

	/* Set Garten IDLE to META idle and Set the Garten Wrapper BIF Fence address */
	{
		IMG_UINT64 ui32BIFFenceAddr = RGXFW_BOOTLDR_DEVV_ADDR | RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_IDLE_CTRL_META;

		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Configure META wrapper");
		OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_MTS_GARTEN_WRAPPER_CONFIG, ui32BIFFenceAddr);
		PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_MTS_GARTEN_WRAPPER_CONFIG, ui32BIFFenceAddr, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	}

#if defined(RGX_FEATURE_AXI_ACELITE)
	/*
		We must init the AXI-ACE interface before 1st BIF transaction
	*/
	RGXAXIACELiteInit(psDevInfo);
#endif

	/*
	 * Initialise BIF.
	 */
	RGXInitBIF(psDevInfo);

	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Take META out of reset");
	/* need to wait for at least 16 cycles before taking meta out of reset ... */
	PVRSRVSystemWaitCycles(psDevConfig, 32);
	PDUMPIDLWITHFLAGS(32, PDUMP_FLAGS_POWERTRANS);
	
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, 0x0);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, 0x0, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

	(IMG_VOID) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	PDUMPREGREAD64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	
	/* ... and afterwards */
	PVRSRVSystemWaitCycles(psDevConfig, 32);
	PDUMPIDLWITHFLAGS(32, PDUMP_FLAGS_POWERTRANS);

	/*
	 * Start the firmware.
	 */
#if defined(SUPPORT_META_SLAVE_BOOT)
	RGXStartFirmware(psDevInfo);
#else
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS|PDUMP_FLAGS_CONTINUOUS, "RGXStart: RGX Firmware Master boot Start");
#endif
	
	OSMemoryBarrier();

	/* Check whether the FW has started by polling on bFirmwareStarted flag */
	eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc,
									  (IMG_VOID **)&psRGXFWInit);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXStart: Failed to acquire kernel fw if ctl (%u)", eError));
		return eError;
	}

	if (PVRSRVPollForValueKM((IMG_UINT32 *)&psRGXFWInit->bFirmwareStarted,
							 IMG_TRUE,
							 0xFFFFFFFF) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXStart: Polling for 'FW started' flag failed."));
		eError = PVRSRV_ERROR_TIMEOUT;
		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);
		return eError;
	}

	/* Initialize the first entry in DVFS history with current OS time and snapshot of CR timer */
	psDevInfo->psGpuDVFSHistory->asCB[0].ui32OSTimeStamp = OSClockms();
	psDevInfo->psGpuDVFSHistory->asCB[0].ui64CRTimerStamp = psRGXFWInit->ui64CRTimerInitSnapshot;

	/* This place should contain PDUMP POL for bFirmwareStarted, once objanal supports it */

	DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);

	return eError;
}


/*!
*******************************************************************************

 @Function	RGXStop

 @Description Stop RGX in preparation for power down

 @Input psDevInfo - RGX device info

 @Return   PVRSRV_ERROR

******************************************************************************/
static PVRSRV_ERROR RGXStop(PVRSRV_RGXDEV_INFO	*psDevInfo)

{
	PVRSRV_ERROR		eError; 


	eError = RGXRunScript(psDevInfo, psDevInfo->sScripts.asDeinitCommands, RGX_MAX_DEINIT_COMMANDS, PDUMP_FLAGS_LASTFRAME | PDUMP_FLAGS_POWERTRANS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXStop: RGXRunScript failed (%d)", eError));
		return eError;
	}


	return PVRSRV_OK;
}

/*
	RGXPrePowerState
*/
PVRSRV_ERROR RGXPrePowerState (IMG_HANDLE				hDevHandle,
							   PVRSRV_DEV_POWER_STATE	eNewPowerState,
							   PVRSRV_DEV_POWER_STATE	eCurrentPowerState,
							   IMG_BOOL					bForced)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if ((eNewPowerState != eCurrentPowerState) &&
		(eNewPowerState != PVRSRV_DEV_POWER_STATE_ON))
	{
		PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
		PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
		RGXFWIF_KCCB_CMD	sPowCmd;
		RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;
		IMG_UINT32			ui32DM;

		/* Send the Power off request to the FW */
		sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
		sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_OFF_REQ;
		sPowCmd.uCmdData.sPowData.uPoweReqData.bForced = bForced;

		SyncPrimSet(psDevInfo->psPowSyncPrim, 0);

		/* Send one pow command to each DM to make sure we flush all the DMs pipelines */
		for (ui32DM = 0; ui32DM < RGXFWIF_DM_MAX; ui32DM++)
		{
			eError = RGXSendCommandRaw(psDevInfo,
					ui32DM,
					&sPowCmd,
					sizeof(sPowCmd),
					PDUMP_FLAGS_POWERTRANS);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: Failed to send Power off request for DM%d", ui32DM));
				return eError;
			}
		}

		/* Wait for the firmware to complete processing. It cannot use PVRSRVWaitForValueKM as it relies 
		   on the EventObject which is signalled in this MISR */
		eError = PVRSRVPollForValueKM(psDevInfo->psPowSyncPrim->pui32LinAddr, 0x1, 0xFFFFFFFF);

		/* Check the Power state after the answer */
		if (eError == PVRSRV_OK)	
		{
			/* Finally, de-initialise some registers. */
			if (psFWTraceBuf->ePowState == RGXFWIF_POW_OFF)
			{
				eError = RGXStop(psDevInfo);
				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: RGXStop failed (%s)", PVRSRVGetErrorStringKM(eError)));
					eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_FAILURE;
				}
			}
			else
			{
				/* the sync was updated bu the pow state isn't off -> the FW denied the transition */
				eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED;
			}
		}
		else if (eError == PVRSRV_ERROR_TIMEOUT)
		{
			/* timeout waiting for the FW to ack the request: return timeout */
			PVR_DPF((PVR_DBG_WARNING,"RGXPrePowerState: Timeout waiting for powoff ack from the FW"));
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: Error waiting for powoff ack from the FW (%s)", PVRSRVGetErrorStringKM(eError)));
			eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_FAILURE;
		}

	}

	return eError;
}


/*
	RGXPostPowerState
*/
PVRSRV_ERROR RGXPostPowerState (IMG_HANDLE				hDevHandle,
								PVRSRV_DEV_POWER_STATE	eNewPowerState,
								PVRSRV_DEV_POWER_STATE	eCurrentPowerState,
								IMG_BOOL				bForced)
{
	if ((eNewPowerState != eCurrentPowerState) &&
		(eCurrentPowerState != PVRSRV_DEV_POWER_STATE_ON))
	{
		PVRSRV_ERROR		 eError;
		PVRSRV_DEVICE_NODE	 *psDeviceNode = hDevHandle;
		PVRSRV_RGXDEV_INFO	 *psDevInfo = psDeviceNode->pvDevice;
		PVRSRV_DEVICE_CONFIG *psDevConfig = psDeviceNode->psDevConfig;
		RGX_DATA			 *psRGXData = (RGX_DATA*)psDevConfig->hDevData;

		if (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF)
		{
			/*
				Coming up from off, re-initialise RGX.
			*/

			/* Reset DVFS history */
			psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId = 0;
			psDevInfo->psGpuDVFSHistory->asCB[0].ui64CRTimerStamp = 0;
			psDevInfo->psGpuDVFSHistory->asCB[0].ui32OSTimeStamp = 0;
			psDevInfo->psGpuDVFSHistory->asCB[0].ui32DVFSClock = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;
			psDevInfo->ui32GpuUtilTransitionsCountSample = 0;

			/* Reset FW CB of GPU state transitions history */
			psDevInfo->psRGXFWIfGpuUtilFWCb->ui32LastGpuUtilState = RGXFWIF_GPU_UTIL_FWCB_STATE_RESERVED;
			psDevInfo->psRGXFWIfGpuUtilFWCb->ui32WriteOffset = 0;
			psDevInfo->psRGXFWIfGpuUtilFWCb->ui32CurrentDVFSId = 0;
			psDevInfo->psRGXFWIfGpuUtilFWCb->aui64CB[0] = 
				RGXFWIF_GPU_UTIL_FWCB_STATE_RESERVED << RGXFWIF_GPU_UTIL_FWCB_STATE_SHIFT;
			psDevInfo->psRGXFWIfGpuUtilFWCb->aui64CB[RGXFWIF_GPU_UTIL_FWCB_SIZE-1] = 
				RGXFWIF_GPU_UTIL_FWCB_STATE_RESERVED << RGXFWIF_GPU_UTIL_FWCB_STATE_SHIFT;
			psDevInfo->psRGXFWIfGpuUtilFWCb->ui32GpuUtilTransitionsCount = 0;
			psDevInfo->psRGXFWIfGpuUtilFWCb->ui32GpuUtilRendersCount = 0;

			/*
				Run the RGX init script.
			*/
			eError = RGXStart(psDevInfo, psDevConfig);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXPostPowerState: RGXStart failed"));
				return eError;
			}
		}
	}

	PDUMPCOMMENT("RGXPostPowerState: Current state: %d, New state: %d", eCurrentPowerState, eNewPowerState);

	return PVRSRV_OK;
}


/*
	RGXPreClockSpeedChange
*/
PVRSRV_ERROR RGXPreClockSpeedChange (IMG_HANDLE				hDevHandle,
									 IMG_BOOL				bIdleDevice,
									 PVRSRV_DEV_POWER_STATE	eCurrentPowerState)
{
	PVRSRV_ERROR		eError = PVRSRV_OK;
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGX_DATA			*psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;
	RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

	PVR_UNREFERENCED_PARAMETER(psRGXData);

	PVR_DPF((PVR_DBG_MESSAGE,"RGXPreClockSpeedChange: RGX clock speed was %uHz",
			psRGXData->psRGXTimingInfo->ui32CoreClockSpeed));

    if ((eCurrentPowerState != PVRSRV_DEV_POWER_STATE_OFF) 
		&& (psFWTraceBuf->ePowState != RGXFWIF_POW_OFF) 
		&& bIdleDevice)
	{
		RGXFWIF_KCCB_CMD	sPowCmd;

		/* Send the IDLE request to the FW */
		sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
		sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_FORCED_IDLE_REQ;
		sPowCmd.uCmdData.sPowData.uPoweReqData.bCancelForcedIdle = IMG_FALSE;

		SyncPrimSet(psDevInfo->psPowSyncPrim, 0);

		/* Send one forced IDLE command to GP */
		eError = RGXSendCommandRaw(psDevInfo,
				RGXFWIF_DM_GP,
				&sPowCmd,
				sizeof(sPowCmd),
				PDUMP_FLAGS_POWERTRANS);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXPreClockSpeedChange: Failed to send IDLE request for DM%d", RGXFWIF_DM_GP));
			return eError;
		}

		/* Wait for the firmware to complete processing. */
		eError = PVRSRVPollForValueKM(psDevInfo->psPowSyncPrim->pui32LinAddr, 0x1, 0xFFFFFFFF);

		/* Check the Power state after the answer */
		if (eError == PVRSRV_OK)	
		{
			if (psFWTraceBuf->ePowState != RGXFWIF_POW_FORCED_IDLE)
			{
				/* the sync was updated but the pow state isn't idle -> the FW denied the transition */
				eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED;
			}
			else
			{
				RGXFWIF_GPU_DVFS_HIST_ENTRY *psCBEntry;

				/* Advance DVFS history ID */
				psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId++;
				if (psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId >= RGXFWIF_GPU_DVFS_HIST_SIZE)
				{
					psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId = 0;
				}

				/* Update DVFS history ID that is used by the FW to populate state changes CB */
				psDevInfo->psRGXFWIfGpuUtilFWCb->ui32CurrentDVFSId = psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId;
				/* Populate DVFS history entry */
				psCBEntry = &psDevInfo->psGpuDVFSHistory->asCB[psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId];
				psCBEntry->ui64CRTimerStamp = psFWTraceBuf->ui64CRTimerSnapshot;
				psCBEntry->ui32OSTimeStamp = OSClockms();
				psCBEntry->ui32DVFSClock = 0;
			}
		}
		else if (eError == PVRSRV_ERROR_TIMEOUT)
		{
			/* timeout waiting for the FW to ack the request: return timeout */
			PVR_DPF((PVR_DBG_ERROR,"RGXPreClockSpeedChange: Timeout waiting for idle ack from the FW"));
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXPreClockSpeedChange: Error waiting for idle ack from the FW (%s)", PVRSRVGetErrorStringKM(eError)));
			eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_FAILURE;
		}
	}

	return eError;
}


/*
	RGXPostClockSpeedChange
*/
PVRSRV_ERROR RGXPostClockSpeedChange (IMG_HANDLE				hDevHandle,
									  IMG_BOOL					bIdleDevice,
									  PVRSRV_DEV_POWER_STATE	eCurrentPowerState)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGX_DATA			*psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;
	PVRSRV_ERROR		eError = PVRSRV_OK;
	RGXFWIF_KCCB_CMD 	sCOREClkSpeedChangeCmd;
	RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

    if ((eCurrentPowerState != PVRSRV_DEV_POWER_STATE_OFF) 
		&& (psFWTraceBuf->ePowState == RGXFWIF_POW_FORCED_IDLE) 
		&& bIdleDevice)
	{
		RGXFWIF_KCCB_CMD	sPowCmd;

		/* Send the IDLE request to the FW */
		sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
		sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_FORCED_IDLE_REQ;
		sPowCmd.uCmdData.sPowData.uPoweReqData.bCancelForcedIdle = IMG_TRUE;

		SyncPrimSet(psDevInfo->psPowSyncPrim, 0);

		/* Send one forced IDLE command to GP */
		eError = RGXSendCommandRaw(psDevInfo,
				RGXFWIF_DM_GP,
				&sPowCmd,
				sizeof(sPowCmd),
				PDUMP_FLAGS_POWERTRANS);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXPostClockSpeedChange: Failed to send Cancel IDLE request for DM%d", RGXFWIF_DM_GP));
			return eError;
		}
	}

    if (eCurrentPowerState != PVRSRV_DEV_POWER_STATE_OFF) 
	{
		sCOREClkSpeedChangeCmd.eCmdType = RGXFWIF_KCCB_CMD_CORECLKSPEEDCHANGE;
		sCOREClkSpeedChangeCmd.uCmdData.sCORECLKSPEEDCHANGEData.ui32NewClockSpeed = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;
		/* Store new DVFS freq into DVFS history entry */
		psDevInfo->psGpuDVFSHistory->asCB[psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId].ui32DVFSClock = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;

		PDUMPCOMMENT("Scheduling CORE clock speed change command");
		eError = RGXSendCommandRaw(psDeviceNode->pvDevice,
											RGXFWIF_DM_GP,
											&sCOREClkSpeedChangeCmd,
											sizeof(sCOREClkSpeedChangeCmd),
											IMG_TRUE);
	
		if (eError != PVRSRV_OK)
		{
			PDUMPCOMMENT("Scheduling CORE clock speed change command failed");
			PVR_DPF((PVR_DBG_ERROR, "RGXPostClockSpeedChange: Scheduling KCCB command failed. Error:%u", eError));
			return eError;
		}
 
		PVR_DPF((PVR_DBG_MESSAGE,"RGXPostClockSpeedChange: RGX clock speed changed to %uHz",
				psRGXData->psRGXTimingInfo->ui32CoreClockSpeed));
	}

	return eError;
}


/*!
******************************************************************************

 @Function	RGXDustCountChange

 @Description

	Does change of number of DUSTs

 @Input	   hDevHandle : RGX Device Node
 @Input	   ui32NumberOfDusts : Number of DUSTs to make transition to

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXDustCountChange(IMG_HANDLE				hDevHandle,
								IMG_UINT32				ui32NumberOfDusts)
{

	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_ERROR		eError;
	RGXFWIF_KCCB_CMD 	sDustCountChange;
	IMG_UINT32			ui32MaxAvailableDusts = RGX_BVNC_KM_N / 2;

	if ((ui32NumberOfDusts == 0) || (ui32NumberOfDusts > ui32MaxAvailableDusts))
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		PVR_DPF((PVR_DBG_ERROR, 
				"RGXDustCountChange: Invalid number of DUSTs (%u) while expecting value within <1,%u>. Error:%u", 
				ui32NumberOfDusts,
				ui32MaxAvailableDusts,
				eError));
		return eError;
	}

	sDustCountChange.eCmdType = RGXFWIF_KCCB_CMD_POW;
	sDustCountChange.uCmdData.sPowData.ePowType = RGXFWIF_POW_NUMDUST_CHANGE;
	sDustCountChange.uCmdData.sPowData.uPoweReqData.ui32NumOfDusts = ui32NumberOfDusts;

	PDUMPCOMMENT("Scheduling command to change Dust Count to %u", ui32NumberOfDusts);
	eError = RGXScheduleCommand(psDeviceNode->pvDevice,
				RGXFWIF_DM_GP,
				&sDustCountChange,
				sizeof(sDustCountChange),
				IMG_TRUE);
	
	if (eError != PVRSRV_OK)
	{
		PDUMPCOMMENT("Scheduling command to change Dust Count failed. Error:%u", eError);
		PVR_DPF((PVR_DBG_ERROR, "RGXDustCountChange: Scheduling KCCB to change Dust Count failed. Error:%u", eError));
		return eError;
	}

	return PVRSRV_OK;
}

/*
	RGXActivePowerRequest
*/
PVRSRV_ERROR RGXActivePowerRequest(IMG_HANDLE hDevHandle)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;

	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_TRACEBUF *psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

	PDUMPPOWCMDSTART();

	OSAcquireBridgeLock();
	OSSetKeepPVRLock();

	/* Powerlock to avoid further requests from racing with the FW hand-shake from now on
	   (previous kicks to this point are detected by the FW) */
	eError = PVRSRVPowerLock();
	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXActivePowerRequest: Failed to acquire PowerLock (device index: %d, error: %s)", 
					psDeviceNode->sDevId.ui32DeviceIndex,
					PVRSRVGetErrorStringKM(eError)));
		goto _RGXActivePowerRequest_PowerLock_failed;
	}

	/* Check again for IDLE once we have the power lock */
	if (psFWTraceBuf->ePowState == RGXFWIF_POW_IDLE)
	{

		psDevInfo->ui32ActivePMReqTotal++;

		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXActivePowerRequest: FW set APM, handshake to power off");

		eError = 
			PVRSRVSetDevicePowerStateKM(psDeviceNode->sDevId.ui32DeviceIndex,
					PVRSRV_DEV_POWER_STATE_OFF,
					IMG_FALSE); /* forced */

		if (eError == PVRSRV_OK)
		{
			psDevInfo->ui32ActivePMReqOk++;
		}
		else if (eError == PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED)
		{
			psDevInfo->ui32ActivePMReqDenied++;
		}

	}

	PVRSRVPowerUnlock();

_RGXActivePowerRequest_PowerLock_failed:
	OSSetReleasePVRLock();
	OSReleaseBridgeLock();
	
	PDUMPPOWCMDEND();

	return eError;

}


/******************************************************************************
 End of file (rgxpower.c)
******************************************************************************/
