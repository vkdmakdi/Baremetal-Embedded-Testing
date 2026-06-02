#include "stm32g4xx.h"
#include <stdint.h>

static void USART2_WriteChar(char c)
{
    while(!(USART2->ISR & USART_ISR_TXE_TXFNF));
    USART2->TDR = c;
}

static void USART2_WriteString(const char *str)
{
    while(*str)
    {
        USART2_WriteChar(*str++);
    }
}

static void Timer2_Init_Synchronized(void)
{
    // enable tim2
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

    //prescare 1 tick, 1ms
    TIM2->PSC = 16000 - 1;

    // arr for 1ms
    TIM2->ARR = 1000 - 1;

    //enable cen
    TIM2->CR1 |= TIM_CR1_CEN;
}

static void USART2_Init(void)
{
	RCC->AHB2ENR   |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR1  |= RCC_APB1ENR1_USART2EN;

    GPIOA->MODER  &= ~(3UL << (2 * 2));
    GPIOA->MODER  |=  (2UL << (2 * 2));

    GPIOA->AFR[0] &= ~(0xFU << (4 * 2));
    GPIOA->AFR[0] |=  (0x7U << (4 * 2));

    USART2->BRR = 139;

    USART2->CR1 |= USART_CR1_TE;
    USART2->CR1 |= USART_CR1_UE;
}

int main(void)
{
    SystemInit();
    USART2_Init();
    Timer2_Init_Synchronized();

    while(1)
    {
        // flag trips every 1 second
        while (!(TIM2->SR & TIM_SR_UIF));

        // clear the flag
        TIM2->SR &= ~TIM_SR_UIF;

        // transmit message
        USART2_WriteString("Hello\r\n");
    }
}
