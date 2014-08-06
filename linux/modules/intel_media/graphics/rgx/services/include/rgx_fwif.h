/*************************************************************************/ /*!
@File			rgx_fwif.h
@Title          RGX firmware interface structures
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX firmware interface structures used by srvinit and server
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

#if !defined (__RGX_FWIF_H__)
#define __RGX_FWIF_H__

#include "rgx_meta.h"
#include "rgx_fwif_shared.h"

#include "pvr_tlcommon.h"
#include "rgx_hwperf.h"

/*************************************************************************/ /*!
 Logging type
*/ /**************************************************************************/
#define RGXFWIF_LOG_TYPE_NONE			 0x00000000
#define RGXFWIF_LOG_TYPE_TRACE			 0x00000001
#define RGXFWIF_LOG_TYPE_GROUP_MAIN		 0x00000002
#define RGXFWIF_LOG_TYPE_GROUP_MTS		 0x00000004
#define RGXFWIF_LOG_TYPE_GROUP_CLEANUP	 0x00000008
#define RGXFWIF_LOG_TYPE_GROUP_CSW		 0x00000010
#define RGXFWIF_LOG_TYPE_GROUP_BIF		 0x00000020
#define RGXFWIF_LOG_TYPE_GROUP_PM		 0x00000040
#define RGXFWIF_LOG_TYPE_GROUP_RTD		 0x00000080
#define RGXFWIF_LOG_TYPE_GROUP_SPM		 0x00000100
#define RGXFWIF_LOG_TYPE_GROUP_POW		 0x00000200
#define RGXFWIF_LOG_TYPE_GROUP_HWR		 0x00000400
#define RGXFWIF_LOG_TYPE_GROUP_DEBUG	 0x00000800
#define RGXFWIF_LOG_TYPE_GROUP_MASK		 0x00000FFE
#define RGXFWIF_LOG_TYPE_MASK			 0x00000FFF

#define RGXFWIF_LOG_ENABLED_GROUPS_LIST(types)	(((types) & RGXFWIF_LOG_TYPE_GROUP_MAIN)	?("main ")		:("")),		\
												(((types) & RGXFWIF_LOG_TYPE_GROUP_MTS)		?("mts ")		:("")),		\
												(((types) & RGXFWIF_LOG_TYPE_GROUP_CLEANUP)	?("cleanup ")	:("")),		\
												(((types) & RGXFWIF_LOG_TYPE_GROUP_CSW)		?("csw ")		:("")),		\
												(((types) & RGXFWIF_LOG_TYPE_GROUP_BIF)		?("bif ")		:("")),		\
												(((types) & RGXFWIF_LOG_TYPE_GROUP_PM)		?("pm ")		:("")),		\
												(((types) & RGXFWIF_LOG_TYPE_GROUP_RTD)		?("rtd ")		:("")),		\
												(((types) & RGXFWIF_LOG_TYPE_GROUP_SPM)		?("spm ")		:("")),		\
												(((types) & RGXFWIF_LOG_TYPE_GROUP_POW)		?("pow ")		:("")),		\
												(((types) & RGXFWIF_LOG_TYPE_GROUP_HWR)		?("hwr ")		:("")),     \
												(((types) & RGXFWIF_LOG_TYPE_GROUP_DEBUG)	?("debug ")		:(""))

/*! Logging function */
typedef IMG_VOID (*PFN_RGXFW_LOG) (const IMG_CHAR* pszFmt, ...);

/*!
 ******************************************************************************
 * HWPERF
 *****************************************************************************/
/* Size of the Firmware L1 HWPERF buffer in bytes (128KB). Accessed by the
 * Firmware and host driver. */
#define RGXFW_HWPERF_L1_SIZE_MIN		(0x04000)
#define RGXFW_HWPERF_L1_SIZE_DEFAULT    (0x20000)
#define RGXFW_HWPERF_L1_PADDING_DEFAULT (RGX_HWPERF_V2_MAX_PACKET_SIZE)

/*!
 ******************************************************************************
 * Trace Buffer
 *****************************************************************************/

/*! Number of elements on each line when dumping the trace buffer */
#define RGXFW_TRACE_BUFFER_LINESIZE	(30)

/*! Total size of RGXFWIF_TRACEBUF dword (needs to be a multiple of RGXFW_TRACE_BUFFER_LINESIZE) */
#define RGXFW_TRACE_BUFFER_SIZE		(400*RGXFW_TRACE_BUFFER_LINESIZE)
#define RGXFW_TRACE_BUFFER_ASSERT_SIZE 200
#define RGXFW_THREAD_NUM 2

#define RGXFW_POLL_TYPE_SET 0x80000000

typedef struct _RGXFWIF_ASSERTBUF_
{
	IMG_CHAR	szPath[RGXFW_TRACE_BUFFER_ASSERT_SIZE];
	IMG_CHAR	szInfo[RGXFW_TRACE_BUFFER_ASSERT_SIZE];
	IMG_UINT32	ui32LineNum;
}RGXFWIF_ASSERTBUF;

typedef struct _RGXFWIF_TRACEBUF_SPACE_
{
	IMG_UINT32			ui32TracePointer;
	IMG_UINT32			aui32TraceBuffer[RGXFW_TRACE_BUFFER_SIZE];
	RGXFWIF_ASSERTBUF	sAssertBuf;
} RGXFWIF_TRACEBUF_SPACE;

#define RGXFWIF_POW_STATES \
  X(RGXFWIF_POW_OFF)			/* idle and handshaked with the host (ready to full power down) */ \
  X(RGXFWIF_POW_ON)				/* running HW mds */ \
  X(RGXFWIF_POW_FORCED_IDLE)	/* forced idle */ \
  X(RGXFWIF_POW_IDLE)			/* idle waiting for host handshake */

typedef enum _RGXFWIF_POW_STATE_
{
#define X(NAME) NAME,
	RGXFWIF_POW_STATES
#undef X
} RGXFWIF_POW_STATE;

/* Firmware HWR states */
#define RGXFWIF_HWR_HARDWARE_OK		(0x1 << 0)	/*!< Tells if the HW state is ok or locked up */
#define RGXFWIF_HWR_FREELIST_OK		(0x1 << 1)	/*!< Tells if the freelists are ok or being reconstructed */
#define RGXFWIF_HWR_ANALYSIS_DONE	(0x1 << 2)	/*!< Tells if the analysis of a GPU lockup has already been performed */
#define RGXFWIF_HWR_GENERAL_LOCKUP	(0x1 << 3)	/*!< Tells if a DM unrelated lockup has been detected */
typedef IMG_UINT32 RGXFWIF_HWR_STATEFLAGS;

/* Firmware per-DM HWR states */
#define RGXFWIF_DM_STATE_WORKING 					(0x00)		/*!< DM is working if all flags are cleared */
#define RGXFWIF_DM_STATE_READY_FOR_HWR 				(0x1 << 0)	/*!< DM is idle and ready for HWR */
#define RGXFWIF_DM_STATE_NEEDS_FL_RECONSTRUCTION	(0x1 << 1)	/*!< DM need FL reconstruction before resuming processing */
#define RGXFWIF_DM_STATE_NEEDS_SKIP					(0x1 << 2)	/*!< DM need to skip to next cmd before resuming processing */
#define RGXFWIF_DM_STATE_NEEDS_PR_CLEANUP			(0x1 << 3)	/*!< DM need partial render cleanup before resuming processing */
#define RGXFWIF_DM_STATE_NEEDS_TRACE_CLEAR			(0x1 << 4)	/*!< DM need to increment Recovery Count once fully recovered */
typedef IMG_UINT32 RGXFWIF_HWR_RECOVERYFLAGS;

typedef struct _RGXFWIF_TRACEBUF_
{
	IMG_UINT32				ui32LogType;
	RGXFWIF_POW_STATE		ePowState;
	IMG_UINT64				RGXFW_ALIGN ui64CRTimerSnapshot;
	RGXFWIF_TRACEBUF_SPACE	sTraceBuf[RGXFW_THREAD_NUM];

	IMG_UINT16				aui16HwrDmLockedUpCount[RGXFWIF_DM_MAX];
	IMG_UINT16				aui16HwrDmRecoveredCount[RGXFWIF_DM_MAX];
	RGXFWIF_DEV_VIRTADDR	apsHwrDmFWCommonContext[RGXFWIF_DM_MAX];

	IMG_UINT32				aui32CrPollAddr[RGXFW_THREAD_NUM];
	IMG_UINT32				aui32CrPollValue[RGXFW_THREAD_NUM];

	RGXFWIF_HWR_STATEFLAGS		ui32HWRStateFlags;
	RGXFWIF_HWR_RECOVERYFLAGS	aui32HWRRecoveryFlags[RGXFWIF_HWDM_MAX];

	volatile IMG_UINT32		ui32HWPerfRIdx;
	volatile IMG_UINT32		ui32HWPerfWIdx;
	volatile IMG_UINT32		ui32HWPerfWrapCount;
	IMG_UINT32				ui32HWPerfSize; /* constant after setup, needed in FW */
	
	IMG_UINT32				ui32InterruptCount;
} RGXFWIF_TRACEBUF;

/*!
 ******************************************************************************
 * GPU Utilization FW CB
 *****************************************************************************/
#define RGXFWIF_GPU_UTIL_FWCB_SIZE_WIDTH		(8)
#define RGXFWIF_GPU_UTIL_FWCB_SIZE				(1 << RGXFWIF_GPU_UTIL_FWCB_SIZE_WIDTH)
#define RGXFWIF_GPU_UTIL_FWCB_MASK				(RGXFWIF_GPU_UTIL_FWCB_SIZE - 1)
#define RGXFWIF_GPU_UTIL_FWCB_STATE_RESERVED	IMG_UINT64_C(0x0)
#define RGXFWIF_GPU_UTIL_FWCB_STATE_IDLE		IMG_UINT64_C(0x1)
#define RGXFWIF_GPU_UTIL_FWCB_STATE_ACTIVE		IMG_UINT64_C(0x2)
#define RGXFWIF_GPU_UTIL_FWCB_STATE_BLOCKED		IMG_UINT64_C(0x3)
#define RGXFWIF_GPU_UTIL_FWCB_STATE_MASK		IMG_UINT64_C(0xC000000000000000)
#define RGXFWIF_GPU_UTIL_FWCB_STATE_SHIFT		(62)
#define RGXFWIF_GPU_UTIL_FWCB_ID_MASK			IMG_UINT64_C(0x3FFF000000000000)
#define RGXFWIF_GPU_UTIL_FWCB_ID_SHIFT			(48)
#define RGXFWIF_GPU_UTIL_FWCB_TIMER_MASK		IMG_UINT64_C(0x0000FFFFFFFFFFFF)
#define RGXFWIF_GPU_UTIL_FWCB_TIMER_SHIFT		(0)

#define RGXFWIF_GPU_UTIL_FWCB_ENTRY_STATE(entry)	(((entry)&RGXFWIF_GPU_UTIL_FWCB_STATE_MASK)>>RGXFWIF_GPU_UTIL_FWCB_STATE_SHIFT)
#define RGXFWIF_GPU_UTIL_FWCB_ENTRY_ID(entry)		(((entry)&RGXFWIF_GPU_UTIL_FWCB_ID_MASK)>>RGXFWIF_GPU_UTIL_FWCB_ID_SHIFT)
#define RGXFWIF_GPU_UTIL_FWCB_ENTRY_TIMER(entry)	(((entry)&RGXFWIF_GPU_UTIL_FWCB_TIMER_MASK)>>RGXFWIF_GPU_UTIL_FWCB_TIMER_SHIFT)

#define RGXFWIF_GPU_UTIL_FWCB_ENTRY_ADD(cb, crtimer, state) do {														\
		/* Combine all the informations about current state transition into a single 64-bit word */						\
		(cb)->aui64CB[(cb)->ui32WriteOffset & RGXFWIF_GPU_UTIL_FWCB_MASK] =												\
			(((IMG_UINT64)(crtimer) << RGXFWIF_GPU_UTIL_FWCB_TIMER_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_TIMER_MASK) |			\
			(((IMG_UINT64)(state) << RGXFWIF_GPU_UTIL_FWCB_STATE_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_STATE_MASK) |			\
			(((IMG_UINT64)(cb)->ui32CurrentDVFSId << RGXFWIF_GPU_UTIL_FWCB_ID_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_ID_MASK);	\
		/* Advance the CB write offset */																				\
		(cb)->ui32WriteOffset++;																						\
		/* Cache current transition in cached memory */																	\
		(cb)->ui32LastGpuUtilState = (state);																			\
		/* Notify the host about a new GPU state transition */															\
		(cb)->ui32GpuUtilTransitionsCount++;																			\
		/* Reset render counter */																						\
		(cb)->ui32GpuUtilRendersCount = 0;																				\
	} while (0)

typedef IMG_UINT64 RGXFWIF_GPU_UTIL_FWCB_ENTRY;

typedef struct _RGXFWIF_GPU_UTIL_FWCB_
{
	IMG_UINT32	ui32WriteOffset;
	IMG_UINT32	ui32LastGpuUtilState;
	IMG_UINT32	ui32CurrentDVFSId;
	IMG_UINT32	ui32GpuUtilTransitionsCount;
	IMG_UINT32	ui32GpuUtilRendersCount;
	RGXFWIF_GPU_UTIL_FWCB_ENTRY	RGXFW_ALIGN aui64CB[RGXFWIF_GPU_UTIL_FWCB_SIZE];
} RGXFWIF_GPU_UTIL_FWCB;

typedef struct _RGX_HWRINFO_
{
	IMG_UINT32	ui32FrameNum;
	IMG_UINT32	ui32PID;
	IMG_UINT32	ui32ActiveHWRTData;
	IMG_UINT32	ui32HWRNumber;
} RGX_HWRINFO;

#define RGXFWIF_MAX_HWINFO 10
typedef struct _RGXFWIF_HWRINFOBUF_
{
	RGX_HWRINFO sHWRInfo[RGXFWIF_DM_MAX][RGXFWIF_MAX_HWINFO];
	IMG_UINT32	ui32WriteIndex[RGXFWIF_DM_MAX];
} RGXFWIF_HWRINFOBUF;

/*! RGX firmware Init Config Data */
#define RGXFWIF_INICFG_CTXSWITCH_TA_EN		(0x1 << 0)
#define RGXFWIF_INICFG_CTXSWITCH_3D_EN		(0x1 << 1)
#define RGXFWIF_INICFG_CTXSWITCH_CDM_EN		(0x1 << 2)
#define RGXFWIF_INICFG_CTXSWITCH_MODE_RAND	(0x1 << 3)
#define RGXFWIF_INICFG_CTXSWITCH_SRESET_EN	(0x1 << 4)
#define RGXFWIF_INICFG_2ND_THREAD_EN		(0x1 << 5)
#define RGXFWIF_INICFG_POW_RASCALDUST		(0x1 << 6)
#define RGXFWIF_INICFG_HWPERF_EN			(0x1 << 7)
#define RGXFWIF_INICFG_HWR_EN				(0x1 << 8)
#define RGXFWIF_INICFG_CHECK_MLIST_EN		(0x1 << 9)
#define RGXFWIF_INICFG_DISABLE_CLKGATING_EN (0x1 << 10)
#define RGXFWIF_INICFG_ALL					(0x000007FFU)
#define RGXFWIF_SRVCFG_DISABLE_PDP_EN 		(0x1 << 31)
#define RGXFWIF_SRVCFG_ALL					(0x80000000U)

#define RGXFWIF_INICFG_CTXSWITCH_DM_ALL		(RGXFWIF_INICFG_CTXSWITCH_TA_EN | \
											 RGXFWIF_INICFG_CTXSWITCH_3D_EN | \
											 RGXFWIF_INICFG_CTXSWITCH_CDM_EN)

#define RGXFWIF_INICFG_CTXSWITCH_CLRMSK		~(RGXFWIF_INICFG_CTXSWITCH_DM_ALL | \
											 RGXFWIF_INICFG_CTXSWITCH_MODE_RAND | \
											 RGXFWIF_INICFG_CTXSWITCH_SRESET_EN)

typedef enum
{
	RGX_ACTIVEPM_FORCE_OFF = 0,
	RGX_ACTIVEPM_FORCE_ON = 1,
	RGX_ACTIVEPM_DEFAULT = 3
} RGX_ACTIVEPM_CONF;

/*!
 ******************************************************************************
 * Querying DM state
 *****************************************************************************/

typedef enum _RGXFWIF_DM_STATE_
{
	RGXFWIF_DM_STATE_NORMAL			= 0,
	RGXFWIF_DM_STATE_LOCKEDUP		= 1,

} RGXFWIF_DM_STATE;

#endif /*  __RGX_FWIF_H__ */

/******************************************************************************
 End of file (rgx_fwif.h)
******************************************************************************/

