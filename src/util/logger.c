/*
 * logger.c
 *
 *  Created on: Sep 19, 2016
 *      Author: Myant
 */
#include "logger.h"

#if ENABLE_LOGGER == 0

void log_init(LogLevel level, LogOutput out){}
void log_debug(char *msg, ...){}
void log_info(char *msg, ...){}
void log_warn(char *msg, ...){}
void log_error(char *msg, ...){}

#elif ENABLE_LOGGER == 1
#define ENABLE_SEGGER_RTT   1   //messages are sent to segger real time terminal

#include "task.h"
#include <appconfig.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "tskctrl.h"
#if (ENABLE_SEGGER_RTT == 1)
#include "SEGGER_RTT.h"
#define SEGGER_LOG_TERMINAL     0
#endif

static int8_t get_free_buffer(void);
static void set_time_stamp(void);

//number of messages to be stored in RAM before printing
#define NMSGS       50
//number of characters that can be printed
#define LENBUFFER       172
#define LENLABEL        8
#define LENTIMESTAMP    15  //10 chars time stamp + \r\n + []
#define LENMSG          (LENBUFFER-(LENLABEL+LENTIMESTAMP))
#define FREE_BUFFER     0x00
#define BUFFER_FULL     -1
//Level labels
#define LBL_DEBUG       "[DEBUG] "
#define LBL_INFO        "[ INFO] "
#define LBL_WARN        "[ WARN] "
#define LBL_ERROR       "[ERROR] "
//Level labels used when the buffer is full
#define LBL_DEBUG_FULL   "[DFULL] "
#define LBL_INFO_FULL    "[IFULL] "
#define LBL_WARN_FULL    "[WFULL] "
#define LBL_ERROR_FULL   "[EFULL] "

#define MAX_COUNTER ENABLE_SEGGER_RTT

//buffer to hold string messages
static char buffer[NMSGS][LENBUFFER] = {{0}};
/* counters used to track messages that were sent and need to be deleted
 * when a counter is zero, the message can be deleted from the buffer */
static uint8_t txcounter[NMSGS] = {0};
//buffer to hold time stamp
static char timestamp[LENTIMESTAMP] = {0};
static bool out_segger_rtt = false;     //serial real time terminal
static uint8_t loglevel = 0;
static uint8_t ix_head = 0;
static uint8_t ix_tail = 0;
static uint32_t lentstamp = 0;

#define copy2buffer(lbl, msg, ixbuffer)                 \
    uint8_t i;                                          \
    char *buffptr, *tag, *time;                         \
    int32_t len;                                        \
    buffptr = &buffer[ixbuffer][0];                     \
    txcounter[ixbuffer] = MAX_COUNTER;                  \
    time = timestamp;                                   \
    for(i = 0; i < lentstamp; i++, time++, buffptr++)   \
        *buffptr = *time;                               \
    tag = lbl;                                          \
    for(i = 0; i < LENLABEL; i++, tag++, buffptr++)     \
        *buffptr = *tag;                                \
    va_list args;                                       \
    va_start(args, msg);                                \
    len = vsnprintf(buffptr, LENMSG, msg, args);        \
    va_end(args);                                       \
    len += LENLABEL+lentstamp+2;                        \
    if(len <= 0)                                        \
        return;                                         \
    if(len > LENMSG-1)                                  \
        len = LENMSG;                                   \

static void set_time_stamp(void)
{
    TickType_t tick = xTaskGetTickCountFromISR();
    lentstamp = snprintf(timestamp, LENTIMESTAMP, "\r\n[%ld]", tick);
    if(lentstamp >= LENTIMESTAMP)
        lentstamp = LENTIMESTAMP;
}

static void flush_output(uint8_t ix, uint16_t len)
{
    uint8_t *buff = (uint8_t*)&buffer[ix][0];
#if (ENABLE_SEGGER_RTT == 1)
    if(out_segger_rtt)
    {
        SEGGER_RTT_SetTerminal(SEGGER_LOG_TERMINAL);
        //SEGGER_RTT_WriteString(SEGGER_LOG_TERMINAL, (char*)buff);
        SEGGER_RTT_Write(SEGGER_LOG_TERMINAL, (char*)buff, len+2);
        txcounter[ix]--;
    }
#endif //ENABLE_SEGGER_RTT
}

static int8_t get_free_buffer(void)
{
    uint8_t ct = 0;
    //check if tail position can be freed
    if(txcounter[ix_tail] == 0)
    {
        buffer[ix_tail][0] = FREE_BUFFER;
        ix_tail++;
        if(ix_tail >= NMSGS)
            ix_tail = 0;
    }
    while((buffer[ix_head][0] != FREE_BUFFER) && (ct < NMSGS))
    {
        ix_head++;
        if(ix_head >= NMSGS)
            ix_head = 0;
        ct++;
    }
    if(buffer[ix_head][0] == FREE_BUFFER)
        return ix_head;
    return BUFFER_FULL;
}


void log_init(LogLevel level, LogOutput out)
{
    loglevel = level;
    out_segger_rtt = (out & LOG_SEGGER_RTT);

    //initialize circular buffer pointers
    ix_head = ix_tail = 0;
}

void log_debug(char *msg, ...)
{
    int8_t freepos;

    if(loglevel <= LEVEL_DEBUG)
    {
        set_time_stamp();
        freepos = get_free_buffer();
        if(BUFFER_FULL == freepos)
        {
            copy2buffer(LBL_DEBUG_FULL, msg, NMSGS-1);
            flush_output(NMSGS-1, len);
        }
        else
        {
            copy2buffer(LBL_DEBUG, msg, freepos);
            flush_output(freepos, len);
        }
    }
}

void log_info(char *msg, ...)
{
    int8_t freepos;
    if(loglevel <= LEVEL_INFO)
    {
        set_time_stamp();
        freepos = get_free_buffer();
        if(BUFFER_FULL == freepos)
        {
            copy2buffer(LBL_INFO_FULL, msg, NMSGS-1);
            flush_output(NMSGS-1, len);
        }
        else
        {
            copy2buffer(LBL_INFO, msg, freepos);
            flush_output(freepos, len);
        }
    }
}

void log_warn(char *msg, ...)
{
    int8_t freepos;
    if(loglevel <= LEVEL_WARN)
    {
        set_time_stamp();
        freepos = get_free_buffer();
        if(BUFFER_FULL == freepos)
        {
            copy2buffer(LBL_WARN_FULL, msg, NMSGS-1);
            flush_output(NMSGS-1, len);
        }
        else
        {
            copy2buffer(LBL_WARN, msg, freepos);
            flush_output(freepos, len);
        }
    }
}

void log_error(char *msg, ...)
{
    int8_t freepos;
    if(loglevel <= LEVEL_ERROR)
    {
        set_time_stamp();
        freepos = get_free_buffer();
        if(BUFFER_FULL == freepos)
        {
            copy2buffer(LBL_ERROR_FULL, msg, NMSGS-1);
            flush_output(NMSGS-1, len);
        }
        else
        {
            copy2buffer(LBL_ERROR, msg, freepos);
            flush_output(freepos, len);
        }
    }
}

#endif //ENABLE_LOGGER
