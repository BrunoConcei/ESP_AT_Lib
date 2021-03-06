/**
 * \file            esp_threads.h
 * \brief           OS threads implementations
 */

/*
 * Copyright (c) 2017, Tilen MAJERLE
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *  * Neither the name of the author nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * \author          Tilen MAJERLE <tilen@majerle.eu>
 */
#define ESP_INTERNAL
#include "esp_threads.h"
#include "esp_parser.h"
#include "esp_sys.h"
#include "esp_int.h"
#include "esp.h"
#include "esp_mem.h"

/**
 * \brief           User input thread to process inputs packets from API functions
 */
void
esp_thread_producer(void* const arg) {
    esp_t* e = arg;                             /* Thread argument is main structure */
    esp_msg_t* msg;                             /* Message structure */
    espr_t res;
    uint32_t time;
    
    esp_sys_protect();                          /* Protect system */
    while (1) {
        esp_sys_unprotect();                    /* Unprotect system */
        time = esp_sys_mbox_get(&esp.mbox_producer, (void **)&msg, 0);  /* Get message from queue */
        esp_sys_protect();                      /* Protect system */
        
        if (time == ESP_SYS_TIMEOUT) {          /* Check valid message */
            continue;
        }
        
        /**
         * Try to call function to process this message
         * Usually it should be function to transmit data to AT port
         */
        esp.msg = msg;
        if (msg->fn) {                          /* Check for callback processing function */
            esp_sys_sem_wait(&e->sem_sync, 0);  /* Lock semaphore, should be unlocked before! */
            res = msg->fn(msg);                 /* Process this message */
            if (res == espOK) {                 /* We have valid data and data were sent */
                esp.cmd = msg->cmd;             /* Save command type */
                esp_sys_unprotect();            /* Release protection */
                time = esp_sys_sem_wait(&e->sem_sync, 0);   /* Wait for synchronization semaphore */
                esp_sys_sem_release(&e->sem_sync);  /* Release protection and start over later */
                esp_sys_protect();              /* Protect system again */
            }
        } else {
            res = espERR;                       /* Simply set error message */
        }
        
        /**
         * In case message is blocking,
         * release semaphore that we finished with processing
         * otherwise directly free memory of message structure
         */
        if (msg->block_time) {
            esp_sys_sem_release(&msg->sem);     /* Release semaphore */
        } else {
            ESP_MSG_VAR_FREE(msg);              /* Release message structure */
        }
        esp.msg = NULL;
    }
}

/**
 * \brief           Thread for processing received data from device
 */
void
esp_thread_consumer(void* const arg) {
    esp_msg_t* msg;
    uint32_t time;
    
    esp_sys_protect();                          /* Protect system */
    while (1) {
        esp_sys_unprotect();                    /* Unprotect system */
        time = esp_sys_mbox_get(&esp.mbox_consumer, (void **)&msg, 10);  /* Get message from queue */
        esp_sys_protect();                      /* Protect system */
        
        //TODO: Handle NULL message
        
        espi_process();                         /* Process input data */
        //espi_process_pt();                      /* Process protothreads */
    }
}
