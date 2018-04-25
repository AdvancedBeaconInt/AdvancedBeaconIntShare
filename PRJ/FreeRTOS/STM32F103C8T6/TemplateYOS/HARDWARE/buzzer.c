#include "buzzer.h"

//PA8
void BuzzerInit(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	GPIO_ResetBits(GPIOA,GPIO_Pin_8);
}

void BuzzerCtrl(unsigned char ctrl)
{
	if(ctrl == ON)
		GPIO_SetBits(GPIOA,GPIO_Pin_8);
	else
		GPIO_ResetBits(GPIOA,GPIO_Pin_8);
}
