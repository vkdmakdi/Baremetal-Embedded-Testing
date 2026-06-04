# Project ZeroWait

A high-efficiency, non-blocking analog data acquisition system for STM32 microcontrollers utilizing hardware-triggered Injected ADC channels and asynchronous UART transmission.

## Overview

In traditional embedded applications, sampling analog signals frequently locks the CPU in polling loops (`while` checks on flags) or stalls the system with software delays. Project ZeroWait eliminates this inefficiency. 

By configuring Timer 1 (TIM1) to generate a Master Trigger Output (TRGO) on compare match, ADC1 is driven entirely by hardware. The application utilizes the ADC's **Injected Channel Group** to automatically sample and store four consecutive data points into dedicated internal hardware registers without requiring DMA or CPU intervention. 

When the conversion sequence finishes, an Injected End of Sequence (JEOS) interrupt wakes the processor just long enough to copy the data and schedule a structured packet transmission over USART2.

## Features

* **Zero CPU Polling**: No `while(!(ADC->ISR & ...))` blocking loops during the sampling window.
* **Hardware-Timed Pacing**: Sampling intervals are governed precisely by the TIM1 time base, removing artificial software delay loops.
* **Injected Sequence Routing**: Leverages the high-priority injected channel queue (`JSQR`) to capture 4 samples sequentially into dedicated hardware registers (`JDR1` through `JDR4`).
* **Asynchronous Execution Architecture**: The main program loop remains entirely non-blocking, making it trivial to integrate this driver into real-time operating systems (RTOS) or complex state machines.
* **Structured Data Packetization**: Implements a lightweight data link protocol equipped with a sync character alignment byte and an XOR checksum for transmission integrity validation.

## Hardware Configuration

### Clock Tree
* System Clock: 170 MHz via High-Speed Internal (HSI) oscillator and Phase-Locked Loop (PLL) configuration.
* ADC Clock Source: Synchronous AHB clock divided by 4.

### Peripherals
* **TIM1**: Configured with a Prescaler of 169 and an Auto-Reload Register (ARR) value of 10000 to establish the time-base trigger. Master Mode Selection (MMS) is set to Output Compare 1 Reference (`OC1REF`) to drive the TRGO line.
* **ADC1**: Configured for Injected Conversion. Sequence length is set to 4 channels (`JL = 3`), triggered on the rising edge of `TIM1_TRGO`. Channel 1 is mapped to all 4 sequence slots.
* **USART2**: Initialized for transmitter-only operation to stream the packed data bytes to a host system.

## Data Packet Protocol

Data is transmitted through USART2 in fixed-size frames consisting of 10 bytes:

| Byte Index | Field | Description |
|---|---|---|
| 0 | Packet Sync | ASCII character '#' (0x23) |
| 1 - 2 | Sample 0 | Injected Channel Data 1 (Low Byte, High Byte) |
| 3 - 4 | Sample 1 | Injected Channel Data 2 (Low Byte, High Byte) |
| 5 - 6 | Sample 2 | Injected Channel Data 3 (Low Byte, High Byte) |
| 7 - 8 | Sample 3 | Injected Channel Data 4 (Low Byte, High Byte) |
| 9 | Checksum | Byte-wise XOR combination of bytes 1 through 8 |

## Project Structure

* `main.c`: Contains peripheral initialization (`suwi`), the asynchronous background execution loop, the ADC interrupt handler (`ADC1_2_IRQHandler`), and the serialization packet engine.
* `main.h`: Core configuration constants, macros, and function prototypes.

## Getting Started

### Prerequisites
* Toolchain: GNU Arm Embedded Toolchain (GCC) or STM32CubeIDE.
* Hardware: Compatible STM32 microcontroller supporting basic register map layout for advanced timers and injected ADC functionality.

### Compilation and Flashing
1. Import the source files into your preferred STM32 development environment.
2. Ensure that your startup file routes the `ADC1_2_IRQHandler` vector properly to the handler defined in `main.c`.
3. Compile the project using the appropriate optimization flags (`-O2` or `-Os` recommended).
4. Flash the binary to your target microcontroller using an ST-LINK or equivalent programmer.
5. Connect a logic analyzer or a serial terminal program (configured to match the hardware baud rate settings) to the USART2 TX pin to observe the continuous data frame streaming.
