#ifndef __FIFO_MONITOR_H_INCLUDED__
#define __FIFO_MONITOR_H_INCLUDED__

/*
 * This file is included on every cell {SP,ISP,host} and on every system
 * that uses the input system device(s). It defines the API to DLI bridge
 *
 * System and cell specific interfaces and inline code are included
 * conditionally through Makefile path settings.
 *
 *  - .        system and cell agnostic interfaces, constants and identifiers
 *	- public:  system agnostic, cell specific interfaces
 *	- private: system dependent, cell specific interfaces & inline implementations
 *	- global:  system specific constants and identifiers
 *	- local:   system and cell specific constants and identifiers
 */

#include "storage_class.h"

#include "system_local.h"
#include "fifo_monitor_local.h"

#ifndef __INLINE_FIFO_MONITOR__
#define STORAGE_CLASS_FIFO_MONITOR_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_FIFO_MONITOR_C 
#include "fifo_monitor_public.h"
#else  /* __INLINE_FIFO_MONITOR__ */
#define STORAGE_CLASS_FIFO_MONITOR_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_FIFO_MONITOR_C STORAGE_CLASS_INLINE
#include "fifo_monitor_private.h"
#endif /* __INLINE_FIFO_MONITOR__ */

#endif /* __FIFO_MONITOR_H_INCLUDED__ */
