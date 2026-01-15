/*
 * wdt.h
 *
 *  Created on: Jun 20, 2018
 *      Author: Myant
 */

#ifndef SRC_WDT_H_
#define SRC_WDT_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "appconfig.h"
#include "tskctrl.h"

typedef enum
{
    WDT_IRQ_RTC1 = 0,
}wdt_irq_id;

typedef enum
{
    WDT_E_INVALID = 0,
    WDT_E_PERIODIC,
    WDT_E_BLOCKING,
    WDT_E_EVT_BASED,
    WDT_E_IRQ,
}wdt_type_t;

typedef struct
{
    TaskId id;
    bool enabled;
    wdt_type_t type;
    uint32_t timeout;
    evt_app_id event_mask;
}wdt_config_t;

typedef const void *wdt_entry;

void wdt_init(void);
void wdt_start(void);
wdt_entry wdt_config_entry(wdt_config_t *config, TickType_t wait_ms);
void wdt_trigger_event(evt_app_id evt);
void wdt_unblock_task(wdt_entry entry);
void wdt_feed(wdt_entry entry);
void wdt_enable(wdt_entry entry, bool en);
void wdt_set_type(wdt_entry entry, wdt_type_t type);
void wdt_set_timeout(wdt_entry entry, uint32_t timeout);
void wdt_set_event_mask(wdt_entry entry, evt_app_id mask);
void wdt_ignore_feed(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_WDT_H_ */
