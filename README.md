# STM32 High-Speed ADC Data Acquisition & Streaming Pipeline

An optimized, low-overhead data acquisition application for STM32 microcontrollers (STM32G4 series). This project demonstrates how to bypass heavy HAL functions to build a deterministic hardware pipeline using **Timers, ADC, and DMA**, streaming framed data packets over **UART** with minimal CPU intervention.

---

## System Architecture

The project configures the MCU to run at its boosted maximum performance frequency (**170 MHz** via the PLL).

### Key Peripherals & Configurations
* **System Clock:** 170 MHz system clock derived from the internal HSI oscillator, boosted via Voltage Scale 1 Boost mode.
* **TIM1 (Timer 1):** Configured with a prescaler of 169 to drop the clock down to 1 MHz. It handles precise timing intervals to trigger the ADC.
* **ADC1 (Analog-to-Digital Converter):** Operating in hardware-triggered mode (tied to TIM1). It captures analog signals without polling delays.
* **DMA1 & DMAMUX1:** Transfers raw 16-bit ADC samples directly into a circular ring buffer (`adc_ring_buffer`) in SRAM.
* **USART2 (UART):** Configured at a register level for raw, high-speed character transmission.

---

## Packet Protocol Format

Data is transmitted in structured **10-byte packets** to ensure data integrity and ease of parsing on the receiving end (e.g., a Python script or serial plotter). 

Each transmission cycle takes the 8 collected samples, splits them into two blocks of 4 samples, and packages them as follows:

| Byte Index | Field | Description |
| :--- | :--- | :--- |
| `0` | **Sync Byte** | Always `#` (`0x23`) to mark the frame start |
| `1 - 2` | **Sample 1** | 16-bit ADC value (Low Byte, High Byte) |
| `3 - 4` | **Sample 2** | 16-bit ADC value (Low Byte, High Byte) |
| `5 - 6` | **Sample 3** | 16-bit ADC value (Low Byte, High Byte) |
| `7 - 8` | **Sample 4** | 16-bit ADC value (Low Byte, High Byte) |
| `9` | **Checksum** | 8-bit XOR of all payload bytes (Bytes 1-8) |

---

## Deep Dive: Code Implementation

### The `suwi()` Initialization
Instead of relying on bloated abstract drivers, the `suwi()` function handles bare-metal register manipulation to link peripherals together:
* Enables clocks for `DMA1`, `ADC12`, `GPIOA`, `USART2`, and `TIM1`.
* Routes `ADC1` to the DMA pipeline using `DMAMUX1`.
* Calibrates and enables the ADC internal voltage regulator for stable, precise readings.

### The Main Loop Pipeline
1.  **DMA Reset:** Clears transfer flags and arms DMA Channel 1 for exactly 8 transfers.
2.  **Hardware Start:** Starts `ADC1` and enables `TIM1`.
3.  **Zero-CPU Wait:** The MCU blocks on a hardware flag `while (!(DMA1->ISR & DMA_ISR_TCIF1))` waiting for the DMA to finish filling the buffer.
4.  **Packetization:** Breaks the 8-sample ring buffer into two distinct 4-sample payloads, calculates the XOR checksum, and pushes them out via `USART2`.
5.  **Pacing Delay:** Features a lightweight spin-loop delay before restarting the acquisition cycle.

---

## Getting Started

### Prerequisites
* **Toolchain:** STM32CubeIDE or `arm-none-eabi-gcc` with `make`.
* **Hardware:** An STM32G4 series development board (e.g., NUCLEO-G474RE or similar) with `PA2` configured for UART TX and the target analog source connected to `PA0` (ADC1_IN1).

### Build & Flash
1. Clone this repository to your local machine.
2. Open the project inside **STM32CubeIDE**.
3. Hit `Build` (Hammer Icon) to compile the binary.
4. Connect your Nucleo board and hit `Run` / `Debug` to flash the firmware.

---

## Verifying Output
Connect a USB-to-UART converter to the target MCU's `USART2` pins (typically routed automatically to the ST-Link Virtual COM port on Nucleo boards) and open a serial terminal at your designated baud rate. You should see a steady stream of binary packets starting with `#`.
