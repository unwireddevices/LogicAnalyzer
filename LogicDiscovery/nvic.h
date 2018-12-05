/*
 * nvic.h
 *
 *  Created on: 09.12.2009
 *      Author: Flexz
 */

#ifndef NVIC_H_
#define NVIC_H_

//#include "stm32f10x.h"
//#include "peripheral.h"
#include <misc.h>

#define AIRCR_VECTKEY_MASK		((uint32_t)0x05FA0000)
#define AIRCR_SYSRESETREQ		((uint32_t)0x00000004)

typedef void (*InterruptHandler)(void);

//���������� ������������: ���������, ����������, ���������
class InterruptController
{
public:
	//�������� ����������
	static void EnableChannel(IRQn_Type irqChannel)
	{
	    /* Enable the Selected IRQ Channels --------------------------------------*/
	    NVIC->ISER[irqChannel >> 0x05] =
	      (uint32_t)0x01 << (irqChannel & (uint8_t)0x1F);
	}
	//�������� ���������� � ������ ���������
	static void EnableChannel(IRQn_Type irqChannel, uint8_t priority, uint8_t subpriority)
	{
		SetChannelPriority(irqChannel, priority, subpriority);
	    EnableChannel(irqChannel);
	}
	//�������� ����������, ������ ��������� � ������������� ���������
	static void EnableChannel(IRQn_Type irqChannel, uint8_t priority, uint8_t subpriority, InterruptHandler handler)
	{
		DisableChannel(irqChannel);
		SetHandler(irqChannel, handler);
		EnableChannel(irqChannel, priority, subpriority);
	}
	//��������� ����������
	static void DisableChannel(IRQn_Type irqChannel)
	{
	    NVIC->ICER[irqChannel >> 0x05] =
	      (uint32_t)0x01 << (irqChannel & (uint8_t)0x1F);
	}
	static void SetChannelPriority(IRQn_Type irqChannel, uint8_t priority, uint8_t subpriority)
	{
	#ifdef STM32L1XX
		NVIC->IP[(uint32_t)(irqChannel)] = ((priority << (8 - __NVIC_PRIO_BITS)) & 0xff);
	#else
		uint32_t tmppriority = 0x00, tmppre = 0x00, tmpsub = 0x0F;
		/* Compute the Corresponding IRQ Priority --------------------------------*/
		tmppriority = (0x700 - ((SCB->AIRCR) & (uint32_t)0x700))>> 0x08;
		tmppre = (0x4 - tmppriority);
		tmpsub = tmpsub >> tmppriority;

		tmppriority = (uint32_t)priority << tmppre;
		tmppriority |=  subpriority & tmpsub;
		tmppriority = tmppriority << 0x04;
		NVIC->IP[irqChannel] = tmppriority;
	#endif
	}
	//
	static void PriorityGroupConfig(uint32_t NVIC_PriorityGroup)
	{
		SCB->AIRCR = AIRCR_VECTKEY_MASK | NVIC_PriorityGroup;
	}
	static void SystemReset()
	{
		SCB->AIRCR = AIRCR_VECTKEY_MASK | AIRCR_SYSRESETREQ;
	}
	//����������� ������� ���������� � RAM. ������� ������� ����������� ��������
	//��� ������������� ����������.
	//������� �������� ���������� ���������� �� ���� � ����������.
	static void RemapToRam();
	//������������� ����������
	static bool SetHandler(IRQn_Type channel, InterruptHandler handler);
	//static bool SetHandler(IRQn_Type channel, InterruptParametericHandler handler, void * parameter);
};

#endif /* NVIC_H_ */
