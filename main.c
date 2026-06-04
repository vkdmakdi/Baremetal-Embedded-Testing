#include "main.h"

#define PACKET_SYNC         '#'
#define PAYLOAD_SIZE        8
#define PACKET_SIZE         (1 + PAYLOAD_SIZE + 1)

uint16_t adc_ring_buffer[8];
uint8_t tx_buffer[PACKET_SIZE];

// Global trackers for background processing
volatile uint8_t half_buffer_ready = 0;
volatile uint8_t full_buffer_ready = 0;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void USART2_WriteChar(char c);
static void suwi(void);
static void adc_calibrate(void);
static void Send_Packet_From_Buffer(uint16_t *src_buffer);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  suwi();

  // Start the hardware engines
  ADC1->CR |= ADC_CR_ADSTART;
  TIM1->CR1 |= TIM_CR1_CEN;

  // Alive check bytes
  USART2_WriteChar('A');
  USART2_WriteChar('B');
  USART2_WriteChar('C');

  while (1)
  {
      if (half_buffer_ready)
      {
          half_buffer_ready = 0;
          Send_Packet_From_Buffer(&adc_ring_buffer[0]); // First 4 elements
      }

      if (full_buffer_ready)
      {
          full_buffer_ready = 0;
          Send_Packet_From_Buffer(&adc_ring_buffer[4]); // Last 4 elements
      }
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
}

static void USART2_WriteChar(char c)
{
    while(!(USART2->ISR & USART_ISR_TXE_TXFNF));
    USART2->TDR = c;
}

static void adc_calibrate(void)
{
    ADC1->CR &= ~ADC_CR_DEEPPWD;
    ADC1->CR |= ADC_CR_ADVREGEN;
    for (volatile int i = 0; i < 3000; i++) { }

    ADC1->CR |= ADC_CR_ADCAL;
    while (ADC1->CR & ADC_CR_ADCAL);
}

static void suwi(void)
{
    RCC->PLLCFGR |= (8U << RCC_PLLCFGR_PLLPDIV_Pos) | (RCC_PLLCFGR_PLLPEN);

    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN;
    RCC->AHB2ENR  |= RCC_AHB2ENR_ADC12EN;
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
    RCC->APB2ENR  |= RCC_APB2ENR_TIM1EN;

    RCC->CCIPR &= ~(RCC_CCIPR_ADC12SEL);
    RCC->CCIPR |=  (1U << RCC_CCIPR_ADC12SEL_Pos);

    GPIOA->MODER  &= ~(3UL << (2 * 2));
    GPIOA->MODER  |=  (2UL << (2 * 2));
    GPIOA->AFR[0] &= ~(0xFU << (4 * 2));
    GPIOA->AFR[0] |=  (0x7U << (4 * 2));

    USART2->BRR = 16;
    USART2->CR1 |= USART_CR1_TE;
    USART2->CR1 |= USART_CR1_UE;

    // Clear and set PA0 to Analog Mode
    GPIOA->MODER &= ~(3UL << (2 * 0));
    GPIOA->MODER |=  (3UL << (2 * 0));

    // Clear DMA flag status bits
    DMA1->IFCR = DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1;

    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    DMA1_Channel1->CPAR  = (uint32_t)(&(ADC1->DR));
    DMA1_Channel1->CMAR  = (uint32_t)(adc_ring_buffer);
    DMA1_Channel1->CNDTR = 8;

    DMA1_Channel1->CCR = 0;
    DMA1_Channel1->CCR |= (1U << DMA_CCR_PSIZE_Pos); // 16-bit peripheral
    DMA1_Channel1->CCR |= (1U << DMA_CCR_MSIZE_Pos); // 16-bit memory
    DMA1_Channel1->CCR |= DMA_CCR_MINC;             // Memory increment

    // Enable circular mode along with Half-Transfer and Transfer-Complete Interrupts
    DMA1_Channel1->CCR |= DMA_CCR_CIRC | DMA_CCR_TCIE | DMA_CCR_HTIE;

    DMAMUX1_Channel0->CCR = 5; // Route ADC1 to DMA1 Channel 1

    // Configure DMA interrupts inside the core NVIC
    NVIC_SetPriority(DMA1_Channel1_IRQn, 1);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    // Bulletproof Timer Configuration using Master Mode Update (TRGO)
    TIM1->PSC = 169;
    TIM1->ARR = 10000; // Trigger every 10ms

    // Configure Master Mode Selection (MMS) to send TRGO pulse on Update Event
    TIM1->CR2 &= ~TIM_CR2_MMS;
    TIM1->CR2 |= (2U << TIM_CR2_MMS_Pos); // 2 = TRGO on Update Event
    TIM1->EGR |= TIM_EGR_UG;              // Force a register update

    // Run calibration before enabling the converter macro block
    adc_calibrate();

    // Enable the ADC and block until internal analog stability flag triggers
    ADC1->CR |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY)) { }
    ADC1->ISR |= ADC_ISR_ADRDY;

    // Apply runtime tracking configurations (Triggers and Peripheral DMA mode)
    ADC1->CFGR &= ~(ADC_CFGR_EXTEN | ADC_CFGR_EXTSEL | ADC_CFGR_CONT);

    // On STM32G4, EXTSEL = 9 selects TIM1_TRGO for regular channel group
    ADC1->CFGR |= (9U << ADC_CFGR_EXTSEL_Pos);
    ADC1->CFGR |= (1U << ADC_CFGR_EXTEN_Pos);  // Detect on rising edge
    ADC1->CFGR |= ADC_CFGR_DMAEN | ADC_CFGR_DMACFG; // Continuous Circular DMA mode

    ADC1->SQR1 = 0;
    ADC1->SQR1 |= (1U << ADC_SQR1_SQ1_Pos); // Map sequence to Channel 1

    // Activate the DMA channel stream layer
    DMA1_Channel1->CCR |= DMA_CCR_EN;
}

void DMA1_Channel1_IRQHandler(void)
{
    // Half Transfer Interrupt Handling (Elements 0 through 3 Ready)
    if (DMA1->ISR & DMA_ISR_HTIF1)
    {
        DMA1->IFCR = DMA_IFCR_CHTIF1;
        half_buffer_ready = 1;
    }

    // Transfer Complete Interrupt Handling (Elements 4 through 7 Ready)
    if (DMA1->ISR & DMA_ISR_TCIF1)
    {
        DMA1->IFCR = DMA_IFCR_CTCIF1;
        full_buffer_ready = 1;
    }
}

static void Send_Packet_From_Buffer(uint16_t *src_buffer)
{
    tx_buffer[0] = PACKET_SYNC;
    uint8_t checksum = 0;
    uint8_t byte_idx = 1;

    for (uint8_t i = 0; i < 4; i++)
    {
        uint16_t sample = src_buffer[i];
        uint8_t low_byte  = (uint8_t)(sample & 0xFF);
        uint8_t high_byte = (uint8_t)((sample >> 8) & 0xFF);

        tx_buffer[byte_idx++] = low_byte;
        tx_buffer[byte_idx++] = high_byte;

        checksum ^= low_byte;
        checksum ^= high_byte;
    }

    tx_buffer[PACKET_SIZE - 1] = checksum;

    for (uint8_t i = 0; i < PACKET_SIZE; i++)
    {
        USART2_WriteChar((char)tx_buffer[i]);
    }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
