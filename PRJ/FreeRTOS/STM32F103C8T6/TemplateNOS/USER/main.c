/****************************************Copyright (c)****************************************************
** Created by:			yusengo@163.com
** Created date:		2018-04-25
** Version:					v1.0    
** Descriptions:		STM32F103C8T6 无 OS 工程模板
** CopyRight:				炬远智能科技（上海）有限公司
**********************************************************************************************************/

#include "stm32f10x.h"
#include "delay.h"

GPIO_InitTypeDef GPIO_InitStructure;

int main(void)
{
	SystemInit();
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOA,GPIO_Pin_8);
	
	while (1)
	{
		GPIO_SetBits(GPIOA,GPIO_Pin_8);
		Delay_ms(1000);
		GPIO_ResetBits(GPIOA,GPIO_Pin_8);
		Delay_ms(1000);
	}
}

/*********************************END FILE************************************/
