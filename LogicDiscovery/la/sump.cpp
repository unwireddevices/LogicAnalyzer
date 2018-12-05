/*
 * sump.cpp
 *
 *  Created on: 11.11.2012
 *      Author: user
 */

#include "sump.h"
#include "ramblocks.h"
#include "la_sampling.h"
#include "usbd_cdc_core.h"
#include "nvic.h"

typedef void (*SumpByteTXFunction)(uint8_t data);
typedef void (*SumpBufferTXFunction)(uint8_t *data, int count);

static SumpByteTXFunction byteTXFunc = NULL;
static SumpBufferTXFunction bufferTXFunc = NULL;

static char metaData[] _AHBRAM
     = {SUMP_META_NAME, 'L', 'o', 'g', 'i', 'c', ' ', 'a', 'n', 'a', 'l','y', 'z', 'e', 'r', 0,
		SUMP_META_FPGA_VERSION, 'N', 'o', 'F', 'P', 'G', 'A', ' ', ':', '(', 0,
		SUMP_META_CPU_VERSION, 'V', 'e', 'r', 'y', ' ','b' ,'e', 't', 'a', 0,
		SUMP_META_SAMPLE_RATE, BYTE4(maxSampleRate), BYTE3(maxSampleRate), BYTE2(maxSampleRate), BYTE1(maxSampleRate),
		SUMP_META_SAMPLE_RAM, 0, 0, BYTE2(maxSampleMemory), BYTE1(maxSampleMemory), //24*1024 b
		SUMP_META_PROBES_B, 8,
		SUMP_META_PROTOCOL_B, 2,
		SUMP_META_END
};

static void SamplingComplete();
static void DemoUSARTIrq();

void SumpSetTXFunctions(SumpByteTXFunction byteTX, SumpBufferTXFunction bufferTX)
{
	byteTXFunc = byteTX;
	bufferTXFunc = bufferTX;
}

void SetupDemoTimer()
{
	USART_InitTypeDef USART_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;// | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	RCC_APB1PeriphClockCmd(RCC_APB1ENR_USART2EN, ENABLE);
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init(USART2, &USART_InitStructure);
	USART_Cmd(USART2, ENABLE);
	//USART2->CR1 = USART_cr1
	//USART2->BRR = 100;

	RCC_APB1PeriphClockCmd(RCC_APB1ENR_TIM2EN, ENABLE);
	TIM2->CR1 = TIM_CR1_URS;
	TIM2->ARR = 19999;
	TIM2->PSC = 2;
	TIM2->CR2 = 0;
	TIM2->DIER = TIM_DIER_UIE;
	TIM2->CCMR1 = TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
	TIM2->CCR1 = 100;
	TIM2->EGR = TIM_EGR_UG;
	TIM2->CCER = TIM_CCER_CC1E;
	TIM2->SR &= ~TIM_SR_UIF;
	InterruptController::EnableChannel(TIM2_IRQn, 2, 0, DemoUSARTIrq);
	TIM2->CR1 = TIM_CR1_URS | TIM_CR1_CEN;

	/* Connect TIM3 pins to AF2 */
	//GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_TIM2);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
	//GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);
}

static uint8_t num = 0;
static void DemoUSARTIrq()
{
	TIM2->SR &= ~TIM_SR_UIF;
	if(USART2->SR & USART_SR_TXE)
		USART2->DR = num++;
}

int SumpIsShortCommand(uint8_t command)
{
	switch(command)
	{
	case SUMP_CMD_RESET:
	case SUMP_CMD_RUN:
	case SUMP_CMD_ID:
	case SUMP_CMD_META:
		return true;
	default:
		return false;
	}
}

uint32_t CalcLocalDivider(uint32_t divider, const uint32_t localFrequency, const uint32_t sumpFrequency)
{
	return localFrequency / (sumpFrequency / (divider + 1)) - 1;
}

//extern CDC_IF_Prop_TypeDef VCP_fops;
//static char text[40];
//static int samplingReadCount = 1024;
//static int samplingDelayCount = 0;
static uint32_t divider = 1;
static int startup = 1;
int SumpProcessRequest(uint8_t *buffer, int len)
{
	int result = 0;
	switch(buffer[0])
	{
	case SUMP_CMD_RESET://reset
		if(startup)
		{
			SetupDemoTimer();
			startup = 0;
		}
		//SamplingSetupTimer(16);
		sampler.Stop();
	    result = 1;
	  break;
	case SUMP_CMD_RUN://run
		SamplingClearBuffer();
		sampler.Start();
		sampler.Arm(SamplingComplete);
		result = 1;
	  break;
	case SUMP_CMD_ID://ID
		//APP_FOPS.pIf_DataTx((uint8_t*)"1ALS", 4);
		bufferTXFunc((uint8_t*)"1ALS", 4);
		result = 1;
	  break;
	case SUMP_CMD_META://Query metas
		//APP_FOPS.pIf_DataTx((uint8_t*)metaData, sizeof(metaData));
		bufferTXFunc((uint8_t*)metaData, sizeof(metaData));
		result = 1;
	  break;
	case SUMP_CMD_SET_SAMPLE_RATE:
		if(len == 5)
		{
			//div120 = 120MHz / (100MHz / (div100 + 1)) - 1;
			divider = *((uint32_t*)(buffer+1));
			//if maximum samplerate is 20MHz => 100/20 = 5, 5 - 1 = 4
			if(divider != 0)
			{
				RCC_ClocksTypeDef clocks;
				RCC_GetClocksFreq(&clocks);
				divider = CalcLocalDivider(divider, clocks.SYSCLK_Frequency, SUMP_ORIGINAL_FREQ);
			}
			if(divider == 0)
			{
				divider = 1;
			}
			sampler.SetSamplingPeriod(divider);
			result = 1;
			//GUI_Text(0, 14, (uint8_t*)text, White, Black);
		}
		break;
	case SUMP_CMD_SET_COUNTS:
		if(len == 5)
		{
			uint16_t readCount  = 1 + *((uint16_t*)(buffer+1));
			uint16_t delayCount = *((uint16_t*)(buffer+3));
			sampler.SetBufferSize(4*readCount);
			sampler.SetDelayCount(4*delayCount);
			result = 1;
			//GUI_Text(100, 14, (uint8_t*)text, White, Black);
		}
		break;
	case SUMP_CMD_SET_BT0_MASK:
		if(len == 5)
		{
			sampler.SetTriggerMask(*(uint32_t*)(buffer+1));
			result = 1;
		}
		break;
	case SUMP_CMD_SET_BT0_VALUE:
		if(len == 5)
		{
			sampler.SetTriggerValue(*(uint32_t*)(buffer+1));
			result = 1;
		}
		break;
	case SUMP_CMD_SET_FLAGS:
		if(len == 5)
		{
			sampler.SetFlags(*(uint16_t*)(buffer+1));
			result = 1;
		}
		break;
	}
	return result;
}

void SamplingComplete()
{
	uint32_t i;
	__disable_irq();
	if(sampler.GetBytesPerTransfer() == 1)
	{
		uint8_t* ptr = sampler.GetBufferTail() - 1;//sampler.GetBytesPerTransfer();
		//SUMP requests samples to be sent in reverse order: newest items first
		for(i = 0; i < sampler.GetBufferTailSize(); i++)
		{
			byteTXFunc(*ptr);//VCP_ByteTx(*ptr);
			ptr--;
		}
		ptr = sampler.GetBuffer() + sampler.GetBufferSize() - 1;
		for(; i < sampler.GetBufferSize(); i++)
		{
			byteTXFunc(*ptr);//VCP_ByteTx(*ptr);
			ptr--;
		}
	}
	else if(sampler.GetBytesPerTransfer() == 2)
	{
		uint8_t *ptr = sampler.GetBufferTail() - sampler.GetBytesPerTransfer();
		for(i = 0; i < sampler.GetBufferTailSize(); i += sampler.GetBytesPerTransfer())
		{
			byteTXFunc(ptr[0]);
			byteTXFunc(ptr[1]);
			ptr -= sampler.GetBytesPerTransfer();
		}
		ptr = sampler.GetBuffer() + sampler.GetBufferSize() - sampler.GetBytesPerTransfer();
		for(; i < sampler.GetBufferSize(); i += sampler.GetBytesPerTransfer())
		{
			byteTXFunc(ptr[0]);
			byteTXFunc(ptr[1]);
			ptr -= sampler.GetBytesPerTransfer();
		}
	}
	__enable_irq();
}
