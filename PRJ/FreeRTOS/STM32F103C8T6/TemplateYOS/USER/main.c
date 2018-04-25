/****************************************Copyright (c)****************************************************
** Created by:			yusengo@163.com
** Created date:		2018-04-25
** Version:					v1.0    
** Descriptions:		STM32F103C8T6 FreeRTOS 工程
** CopyRight:				炬远智能科技（上海）有限公司
**********************************************************************************************************/

#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"

#include "buzzer.h"
#include "doorctrl.h"

#define START_TASK_PRIO		1
#define START_STACK_SIZE	128
TaskHandle_t StartTask_Handler;
void StartTask(void *pvParameters);

#define BUZZER_TASK_PRIO	2
#define BUZZER_STACK_SIZE	64
TaskHandle_t BuzzerTask_Handler;
void BuzzerTask(void *pvParameters);

#define DOORCTRL_TASK_PRIO		2
#define DOORCTRL_STACK_SIZE	64
TaskHandle_t DoorCtrlTask_Handler;
void DoorCtrlTask(void *pvParameters);

#define UART_TASK_PRIO		1
#define UART_STACK_SIZE	128
TaskHandle_t UartTask_Handler;
void UartTask(void *pvParameters);

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	delay_init();
	uart_init(115200);
	BuzzerInit();
	DoorCtrlInit();
	
	xTaskCreate((TaskFunction_t)StartTask,
							(const char*)"startTask",
							(uint16_t)START_STACK_SIZE,
							(void*)NULL,
							(UBaseType_t)START_TASK_PRIO,
							(TaskHandle_t*)&StartTask_Handler);
	vTaskStartScheduler();
}

void StartTask(void* pvParameters)
{
	taskENTER_CRITICAL();
	
	xTaskCreate((TaskFunction_t)BuzzerTask,
							(const char*)"BuzzerTask",
							(uint16_t)BUZZER_STACK_SIZE,
							(void*)NULL,
							(UBaseType_t)BUZZER_TASK_PRIO,
							(TaskHandle_t*)&BuzzerTask_Handler);
	
	xTaskCreate((TaskFunction_t)DoorCtrlTask,
							(const char*)"DoorCtrlTask",
							(uint16_t)DOORCTRL_STACK_SIZE,
							(void*)NULL,
							(UBaseType_t)DOORCTRL_TASK_PRIO,
							(TaskHandle_t*)&DoorCtrlTask_Handler);		
							
	xTaskCreate((TaskFunction_t)UartTask,
							(const char*)"UartTask",
							(uint16_t)UART_STACK_SIZE,
							(void*)NULL,
							(UBaseType_t)UART_TASK_PRIO,
							(TaskHandle_t*)&UartTask_Handler);	
							
	vTaskDelete(StartTask_Handler);
							
	taskEXIT_CRITICAL();
}

void BuzzerTask(void* pvParameters)
{
	while(1)
	{
		BuzzerCtrl(ON);
		vTaskDelay(1000);
		BuzzerCtrl(OFF);
		vTaskDelay(1000);
	}
}

void DoorCtrlTask(void* pvParameters)
{
	while(1)
	{
		DoorCtrl(ON);
		vTaskDelay(5000);
		DoorCtrl(OFF);
		vTaskDelay(5000);
	}
}

void UartTask(void* pvParameters)
{
	while(1)
	{
		printf("123456saa");
		vTaskDelay(2000);
	}
}
/*********************************END FILE************************************/
