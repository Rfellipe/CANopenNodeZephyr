/*
 * Copyright (c) 2019 Vestas Wind Systems A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_MODULES_CANOPENNODE_CO_DRIVER_H
#define ZEPHYR_MODULES_CANOPENNODE_CO_DRIVER_H

/*
 * Zephyr RTOS CAN driver interface and configuration for CANopenNode
 * CANopen protocol stack.
 *
 * See CANopenNode/301/CO_driver.h for API description.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/device.h>
#include <zephyr/toolchain.h>
#include <zephyr/dsp/types.h> /* float32_t, float64_t */

/* Use static variables instead of calloc() */
#define CO_USE_GLOBALS

/* Use SDO buffer size from Kconfig */
#define CO_SDO_BUFFER_SIZE CONFIG_CANOPENNODE_SDO_BUFFER_SIZE

/* Use trace buffer size from Kconfig */
#define CO_TRACE_BUFFER_SIZE_FIXED CONFIG_CANOPENNODE_TRACE_BUFFER_SIZE

#ifdef CONFIG_CANOPENNODE_LEDS
#define CO_USE_LEDS 1
#endif

#ifdef CONFIG_CANOPENNODE_CIA402
#define CO_CONFIG_CIA402 CO_CONFIG_CIA402_ENABLE
#endif

#ifdef CONFIG_LITTLE_ENDIAN
#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x) (x)
#define CO_SWAP_32(x) (x)
#define CO_SWAP_64(x) (x)
#else
#define CO_BIG_ENDIAN
#endif

typedef bool          bool_t;
typedef char          char_t;
typedef unsigned char oChar_t;
typedef unsigned char domain_t;

BUILD_ASSERT(sizeof(float32_t) >= 4);
BUILD_ASSERT(sizeof(float64_t) >= 8);

typedef struct canopen_rx_msg {
	uint8_t data[8];
	uint16_t ident;
	uint8_t DLC;
} CO_CANrxMsg_t;

#define CO_CANrxMsg_readIdent(msg) (((const CO_CANrxMsg_t *)(msg))->ident)
#define CO_CANrxMsg_readDLC(msg)   (((const CO_CANrxMsg_t *)(msg))->DLC)
#define CO_CANrxMsg_readData(msg)  (((const CO_CANrxMsg_t *)(msg))->data)

typedef void (*CO_CANrxBufferCallback_t)(void *object,
					 void *message);

typedef struct canopen_rx {
	int filter_id;
	void *object;
	CO_CANrxBufferCallback_t CANrx_callback;
	uint16_t ident;
	uint16_t mask;
#ifdef CONFIG_CAN_ACCEPT_RTR
	bool rtr;
#endif /* CONFIG_CAN_ACCEPT_RTR */
} CO_CANrx_t;

typedef struct canopen_tx {
	uint8_t data[8];
	uint16_t ident;
	uint8_t DLC;
	bool_t rtr : 1;
	volatile bool_t bufferFull : 1;
	volatile bool_t syncFlag : 1;
} CO_CANtx_t;

typedef struct canopen_module {
	void *CANptr;
	CO_CANrx_t *rxArray;
	uint16_t rxSize;
	CO_CANtx_t *txArray;
	uint16_t txSize;
	uint16_t CANerrorStatus;
	uint32_t errOld;
	void *em;
	volatile bool_t CANnormal : 1;
	volatile bool_t useCANrxFilters : 1;
	volatile bool_t bufferInhibitFlag : 1;
	volatile bool_t firstCANtxMessage : 1;
	volatile uint16_t CANtxCount;
} CO_CANmodule_t;

typedef struct {
	void *addr;
	size_t len;
	uint8_t subIndexOD;
	uint8_t attr;
	void *addrNV;
} CO_storage_entry_t;

void canopen_send_lock(void);
void canopen_send_unlock(void);
#define CO_LOCK_CAN_SEND(CAN_MODULE)   canopen_send_lock()
#define CO_UNLOCK_CAN_SEND(CAN_MODULE) canopen_send_unlock()

void canopen_emcy_lock(void);
void canopen_emcy_unlock(void);
#define CO_LOCK_EMCY(CAN_MODULE)   canopen_emcy_lock()
#define CO_UNLOCK_EMCY(CAN_MODULE) canopen_emcy_unlock()

void canopen_od_lock(void);
void canopen_od_unlock(void);
#define CO_LOCK_OD(CAN_MODULE)   canopen_od_lock()
#define CO_UNLOCK_OD(CAN_MODULE) canopen_od_unlock()

/*
 * CANopenNode RX callbacks run in interrupt context, no memory
 * barrier needed.
 */
#define CO_MemoryBarrier()
#define CO_FLAG_READ(rxNew) ((rxNew) != NULL)
#define CO_FLAG_SET(rxNew) { CO_MemoryBarrier(); rxNew = (void *)1L; }
#define CO_FLAG_CLEAR(rxNew) { CO_MemoryBarrier(); rxNew = NULL; }

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_MODULES_CANOPENNODE_CO_DRIVER_H */
