#include "stm32g4xx.h"
#include <stdint.h>

static void USART2_WriteChar(char c)
{
    // wait for transmit data reg empty 
    while(!(USART2->ISR & USART_ISR_TXE_TXFNF));
    USART2->TDR = c;
}

static void USART2_Init(void)
{
    // enable clk
    RCC->AHB2ENR   |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR1  |= RCC_APB1ENR1_USART2EN;

    // config pa2 to af7
    GPIOA->MODER  &= ~(3UL << (2 * 2)); // clear bit for pin 2
    GPIOA->MODER  |=  (2UL << (2 * 2)); // set af10

    GPIOA->AFR[0] &= ~(0xFU << (4 * 2)); // clear bit for pin 2
    GPIOA->AFR[0] |=  (0x7U << (4 * 2)); // set af7

    // BRR = 16,000,000 / 115,200 = 139 
    USART2->BRR = 139;

    // eable te and ue
    USART2->CR1 |= USART_CR1_TE;
    USART2->CR1 |= USART_CR1_UE;
}

int main(void)
{
    SystemInit();
    USART2_Init();

    while(1)
    {
        USART2_WriteChar('H');
        USART2_WriteChar('e');
        USART2_WriteChar('l');
        USART2_WriteChar('l');
        USART2_WriteChar('o');
        USART2_WriteChar('\r');
        USART2_WriteChar('\n');

        // delay loop
        for(volatile int i = 0; i < 100000; i++);
    }
}