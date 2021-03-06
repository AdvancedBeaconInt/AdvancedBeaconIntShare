/*
 * FreeRTOS+UDP V1.0.4
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/* Standard includes. */
#include <stdint.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+UDP includes. */
#include "FreeRTOS_UDP_IP.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_Sockets.h"
#include "NetworkBufferManagement.h"
#include "lpc18xx_43xx_EMAC_LPCOpen.h"

/* Demo includes. */
#include "NetworkInterface.h"

/* Library includes. */
#include "board.h"

#if configMAC_INTERRUPT_PRIORITY > configMAC_INTERRUPT_PRIORITY
	#error configMAC_INTERRUPT_PRIORITY must be greater than or equal to configMAC_INTERRUPT_PRIORITY (higher numbers mean lower logical priority)
#endif

#ifndef configNUM_RX_ETHERNET_DMA_DESCRIPTORS
	#error configNUM_RX_ETHERNET_DMA_DESCRIPTORS must be defined in FreeRTOSConfig.h to set the number of RX DMA descriptors
#endif

#ifndef configNUM_TX_ETHERNET_DMA_DESCRIPTORS
	#error configNUM_TX_ETHERNET_DMA_DESCRIPTORS must be defined in FreeRTOSConfig.h to set the number of TX DMA descriptors
#endif

/* If a packet cannot be sent immediately then the task performing the send
operation will be held in the Blocked state (so other tasks can execute) for
niTX_BUFFER_FREE_WAIT ticks.  It will do this a maximum of niMAX_TX_ATTEMPTS
before giving up. */
#define niTX_BUFFER_FREE_WAIT	( ( TickType_t ) 2UL / portTICK_RATE_MS )
#define niMAX_TX_ATTEMPTS		( 5 )

/*-----------------------------------------------------------*/

/*
 * A deferred interrupt handler task that processes received frames.
 */
static void prvEMACDeferredInterruptHandlerTask( void *pvParameters );

/*-----------------------------------------------------------*/

/* The queue used to communicate Ethernet events to the IP task. */
extern xQueueHandle xNetworkEventQueue;

/* The semaphore used to wake the deferred interrupt handler task when an Rx
interrupt is received. */
xSemaphoreHandle xEMACRxEventSemaphore = NULL;

/*-----------------------------------------------------------*/

BaseType_t xNetworkInterfaceInitialise( void )
{
BaseType_t xReturn;
extern uint8_t ucMACAddress[ 6 ];

	xReturn = xEMACInit( ucMACAddress );

	if( xReturn == pdPASS )
	{
		/* Create the event semaphore if it has not already been created. */
		if( xEMACRxEventSemaphore == NULL )
		{
			vSemaphoreCreateBinary( xEMACRxEventSemaphore );
			#if ipconfigINCLUDE_EXAMPLE_FREERTOS_PLUS_TRACE_CALLS == 1
			{
				/* If the trace recorder code is included name the semaphore for
				viewing in FreeRTOS+Trace. */
				vTraceSetQueueName( xEMACRxEventSemaphore, "MAC_RX" );
			}
			#endif /*  ipconfigINCLUDE_EXAMPLE_FREERTOS_PLUS_TRACE_CALLS == 1 */

			configASSERT( xEMACRxEventSemaphore );

			/* The Rx deferred interrupt handler task is created at the highest
			possible priority to ensure the interrupt handler can return directly to
			it no matter which task was running when the interrupt occurred. */
			xTaskCreate( 	prvEMACDeferredInterruptHandlerTask,/* The function that implements the task. */
							"MACTsk",
							configMINIMAL_STACK_SIZE,	/* Stack allocated to the task (defined in words, not bytes). */
							NULL, 						/* The task parameter is not used. */
							configMAX_PRIORITIES - 1, 	/* The priority assigned to the task. */
							NULL );						/* The handle is not required, so NULL is passed. */
		}
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xNetworkInterfaceOutput( xNetworkBufferDescriptor_t * const pxNetworkBuffer )
{
BaseType_t xReturn = pdFAIL;
int32_t x;

	/* Attempt to obtain access to a Tx descriptor. */
	for( x = 0; x < niMAX_TX_ATTEMPTS; x++ )
	{
		if( xEMACIsTxDescriptorAvailable() == TRUE )
		{
			/* Assign the buffer being transmitted to the Tx descriptor. */
			vEMACAssignBufferToDescriptor( pxNetworkBuffer->pucEthernetBuffer );

			/* The EMAC now owns the buffer and will free it when it has been
			transmitted.  Set pucBuffer to NULL to ensure the buffer is not
			freed when the network buffer structure is returned to the pool
			of network buffers. */
			pxNetworkBuffer->pucEthernetBuffer = NULL;

			/* Initiate the Tx. */
			vEMACStartNextTransmission( pxNetworkBuffer->xDataLength );
			iptraceNETWORK_INTERFACE_TRANSMIT();

			/* The Tx has been initiated. */
			xReturn = pdPASS;

			break;
		}
		else
		{
			iptraceWAITING_FOR_TX_DMA_DESCRIPTOR();
			vTaskDelay( niTX_BUFFER_FREE_WAIT );
		}
	}

	/* Finished with the network buffer. */
	vNetworkBufferRelease( pxNetworkBuffer );

	return xReturn;
}
/*-----------------------------------------------------------*/

static void prvEMACDeferredInterruptHandlerTask( void *pvParameters )
{
xNetworkBufferDescriptor_t *pxNetworkBuffer;
xIPStackEvent_t xRxEvent = { eEthernetRxEvent, NULL };

	( void ) pvParameters;
	configASSERT( xEMACRxEventSemaphore );

	for( ;; )
	{
		/* Wait for the EMAC interrupt to indicate that another packet has been
		received.  The while() loop is only needed if INCLUDE_vTaskSuspend is
		set to 0 in FreeRTOSConfig.h.  If INCLUDE_vTaskSuspend is set to 1
		then portMAX_DELAY would be an indefinite block time and
		xSemaphoreTake() would only return when the semaphore was actually
		obtained. */
		while( xSemaphoreTake( xEMACRxEventSemaphore, portMAX_DELAY ) == pdFALSE );

		/* At least one packet has been received. */
		while( xEMACRxDataAvailable() != FALSE )
		{
			/* The buffer filled by the DMA is going to be passed into the IP
			stack.  Allocate another buffer for the DMA descriptor. */
			pxNetworkBuffer = pxNetworkBufferGet( ipTOTAL_ETHERNET_FRAME_SIZE, ( TickType_t ) 0 );

			if( pxNetworkBuffer != NULL )
			{
				/* Swap the buffer just allocated and referenced from the
				pxNetworkBuffer with the buffer that has already been filled by
				the DMA.  pxNetworkBuffer will then hold a reference to the
				buffer that already contains the data without any data having
				been copied between buffers. */
				vEMACSwapEmptyBufferForRxedData( pxNetworkBuffer );

				#if ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES == 1
				{
					if( pxNetworkBuffer->xDataLength > 0 )
					{
						/* If the frame would not be processed by the IP stack then
						don't even bother sending it to the IP stack. */
						if( eConsiderFrameForProcessing( pxNetworkBuffer->pucEthernetBuffer ) != eProcessBuffer )
						{
							pxNetworkBuffer->xDataLength = 0;
						}
					}
				}
				#endif

				if( pxNetworkBuffer->xDataLength > 0 )
				{
					/* Store a pointer to the network buffer structure in the
					padding	space that was left in front of the Ethernet frame.
					The pointer	is needed to ensure the network buffer structure
					can be located when it is time for it to be freed if the
					Ethernet frame gets	used as a zero copy buffer. */
					*( ( xNetworkBufferDescriptor_t ** ) ( ( pxNetworkBuffer->pucEthernetBuffer - ipBUFFER_PADDING ) ) ) = pxNetworkBuffer;

					/* Data was received and stored.  Send it to the IP task
					for processing. */
					xRxEvent.pvData = ( void * ) pxNetworkBuffer;
					if( xQueueSendToBack( xNetworkEventQueue, &xRxEvent, ( TickType_t ) 0 ) == pdFALSE )
					{
						/* The buffer could not be sent to the IP task so the
						buffer must be released. */
						vNetworkBufferRelease( pxNetworkBuffer );
						iptraceETHERNET_RX_EVENT_LOST();
					}
					else
					{
						iptraceNETWORK_INTERFACE_RECEIVE();
					}
				}
				else
				{
					/* The buffer does not contain any data so there is no
					point sending it to the IP task.  Just release it. */
					vNetworkBufferRelease( pxNetworkBuffer );
					iptraceETHERNET_RX_EVENT_LOST();
				}
			}
			else
			{
				iptraceETHERNET_RX_EVENT_LOST();
			}

			/* Release the descriptor. */
			vEMACReturnRxDescriptor();
		}
	}
}
/*-----------------------------------------------------------*/

