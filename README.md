# stm32-adc-dma-uart-logger

> 4-channel ADC data acquisition on STM32G4 using DMA circular buffer and TIM1 triggering, streamed over UART with a Python packet decoder.

![MCU](https://img.shields.io/badge/MCU-STM32G431-blue)
![Language](https://img.shields.io/badge/lang-C%20%2F%20Python-green)
![Bare Metal](https://img.shields.io/badge/bare%20metal-yes-orange)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

---

## Table of Contents

- [Overview](#overview)
- [How It Works](#how-it-works)
- [Packet Structure](#packet-structure)
- [Hardware](#hardware)
- [Clock Configuration](#clock-configuration)
- [Getting Started](#getting-started)
- [Python Decoder](#python-decoder)
- [File Structure](#file-structure)
- [Known Gotchas](#known-gotchas)
- [License](#license)

---

## Overview

This project implements bare-metal ADC data acquisition on the STM32G431 running at 170 MHz. The ADC samples a single analog input at 100 Hz, triggered by TIM1. DMA moves each conversion result into a circular ring buffer without CPU involvement. When half or all of the buffer fills, an interrupt flags the main loop to package and transmit the samples over USART2 as a compact binary packet.

A Python script on the host side listens on the serial port, re-syncs automatically on packet boundaries, verifies the XOR checksum, and prints all 4 channels continuously.

---

## How It Works

```
  PA0 (analog)
       │
       ▼
  ┌─────────┐   every 10ms   ┌─────────┐
  │  TIM1   │───── TRGO ────▶│  ADC1   │
  └─────────┘                └────┬────┘
                                  │ conversion result
                                  ▼
                            ┌──────────┐
                            │  DMA1    │  circular, 16-bit
                            │  Ch1     │  8-element ring
                            └────┬─────┘
                      HTIF │     │ TCIF
                    ┌───────┘     └───────┐
                    ▼                     ▼
             [0..3] ready          [4..7] ready
                    │                     │
                    └──────────┬──────────┘
                               ▼
                         Send_Packet()
                               │
                               ▼
                        ┌────────────┐
                        │  USART2    │  115200 baud
                        └─────┬──────┘
                               │
                               ▼
                        ┌────────────┐
                        │ decoder.py │  Python host
                        └────────────┘
```

**Step by step:**

1. **TIM1** is configured with PSC=169 and ARR=10000, producing an Update Event every 10 ms at 170 MHz. Master Mode is set to TRGO on Update Event.
2. **ADC1** is configured in hardware-triggered mode with EXTSEL=9 (TIM1_TRGO on STM32G4), rising edge detect. Each TRGO pulse starts one conversion on Channel 1 (PA0).
3. **DMA1 Channel 1** is routed from ADC1 via DMAMUX (slot 5). It runs in circular mode writing 16-bit results into `adc_ring_buffer[8]`.
4. At the **half-transfer** (elements 0–3 filled) and **transfer-complete** (elements 4–7 filled), the DMA IRQ sets a flag.
5. The **main loop** polls those flags and calls `Send_Packet_From_Buffer()` with a pointer to the ready half.
6. `Send_Packet_From_Buffer()` builds a 10-byte framed packet and sends it byte-by-byte over USART2.

---

## Packet Structure

Each transmission is exactly 10 bytes:

```
 Byte  0     1     2     3     4     5     6     7     8     9
      ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
      │  #  │ L0  │ H0  │ L1  │ H1  │ L2  │ H2  │ L3  │ H3  │ XOR │
      └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
        ^     └──── CH0 ────┘  └──── CH1 ────┘  └──── CH2 ────┘  ^
      Sync       uint16 LE         uint16 LE         uint16 LE   Checksum
      0x23                                                    XOR of bytes 1–8
```

- **Sync byte**: always `0x23` (`#`) — used to find packet boundaries
- **Samples**: 4 × uint16, little-endian (low byte first, then high byte)
- **Checksum**: XOR of all 8 payload bytes (bytes 1 through 8)
- **Total size**: 10 bytes per packet, 2 packets per DMA cycle = 20 bytes per 8 samples

---

## Hardware

| Pin | Function |
|-----|----------|
| PA0 | ADC1 Channel 1 — analog input (0 to 3.3 V) |
| PA2 | USART2 TX — connect to USB-UART adapter RX |
| GND | Common ground with USB-UART adapter |

| Peripheral | Configuration |
|------------|---------------|
| ADC1 | 12-bit, single channel, hardware triggered, DMA circular |
| TIM1 | PSC=169, ARR=10000, TRGO on Update Event → 100 Hz |
| DMA1 Ch1 | Circular, 16-bit peripheral and memory, HTIE + TCIE |
| USART2 | 115200 baud, TX only, BRR=16 (from PLLP clock) |
| DMAMUX1 Ch0 | Request input = 5 (ADC1) routed to DMA1 Ch1 |

---

## Clock Configuration

The system runs on HSI (16 MHz) through the PLL at full 170 MHz:

```
HSI (16 MHz)
  └── PLL (PLLM=4, PLLN=85, PLLR=2)
        ├── SYSCLK  = 170 MHz
        ├── HCLK    = 170 MHz
        ├── APB1    = 170 MHz  (TIM1, USART2)
        └── PLLP    = 42.5 MHz (ADC kernel clock)
                       └── ADC12SEL = 1 (PLLP) in RCC->CCIPR
```

> **Important:** On STM32G4, `ADC12SEL` must be set to `1` (PLLP). Value `3` is reserved and leaves the ADC with no clock — see [Known Gotchas](#known-gotchas).

---

## Getting Started

### Requirements

- STM32G431 board (e.g. NUCLEO-G431RB)
- ST-Link programmer
- USB-to-UART adapter connected to PA2
- Python 3.x with `pyserial`

### 1. Build and Flash

Open the project in **STM32CubeIDE**, build, and flash via ST-Link.

Alternatively with `arm-none-eabi-gcc` and OpenOCD:
```bash
make
openocd -f interface/stlink.cfg -f target/stm32g4x.cfg \
        -c "program build/output.elf verify reset exit"
```

### 2. Verify Firmware

Open Docklight or any serial terminal at **115200 baud**. On boot you should see:
```
ABC
```
These three bytes are sent at startup as an alive check. If you see them, clocking, GPIO, and USART are all working correctly.

### 3. Run the Decoder

```bash
pip install pyserial
python decoder.py
```

---

## Python Decoder

`decoder.py` runs on the host and decodes the binary packet stream:

```python
import serial
import struct

SYNC_BYTE    = ord('#')
PAYLOAD_SIZE = 8
PACKET_SIZE  = 1 + PAYLOAD_SIZE + 1  # 10 bytes

def compute_checksum(payload):
    checksum = 0
    for b in payload:
        checksum ^= b
    return checksum

def decode_packets(port, baud=115200):
    with serial.Serial(port, baud, timeout=2) as ser:
        print(f"Listening on {port} at {baud} baud...\n")
        while True:
            byte = ser.read(1)
            if not byte:
                continue
            if byte[0] != SYNC_BYTE:
                print(f"[SYNC LOST] Got 0x{byte[0]:02X}, re-syncing...")
                continue
            rest = ser.read(PAYLOAD_SIZE + 1)
            if len(rest) < PAYLOAD_SIZE + 1:
                print("[ERROR] Incomplete packet, skipping...")
                continue
            payload           = rest[:PAYLOAD_SIZE]
            received_checksum = rest[PAYLOAD_SIZE]
            expected_checksum = compute_checksum(payload)
            if received_checksum != expected_checksum:
                print(f"[CHECKSUM FAIL] Expected 0x{expected_checksum:02X}, "
                      f"got 0x{received_checksum:02X}")
                continue
            samples = struct.unpack('<4H', payload)
            print(f"CH0: {samples[0]:5d}  CH1: {samples[1]:5d}  "
                  f"CH2: {samples[2]:5d}  CH3: {samples[3]:5d}")

if __name__ == "__main__":
    decode_packets(port="COM3", baud=115200)  # Change port as needed
```

**Change the port** at the bottom to match your system:

| OS      | Example                   |
|---------|---------------------------|
| Windows | `COM3`, `COM4`, ...       |
| Linux   | `/dev/ttyUSB0`            |
| macOS   | `/dev/tty.usbmodem...`    |

**Example output:**
```
Listening on COM3 at 115200 baud...

CH0:  2048  CH1:  3210  CH2:  1987  CH3:  4001
CH0:  2051  CH1:  3208  CH2:  1990  CH3:  4003
CH0:  2047  CH1:  3211  CH2:  1985  CH3:  4005
```

---

## File Structure

```
stm32-adc-dma-uart-logger/
├── Core/
│   ├── Src/
│   │   └── main.c          # All firmware: ADC, DMA, TIM1, USART2, packet TX
│   └── Inc/
│       └── main.h          # Pin definitions, error handler declaration
├── Drivers/                # STM32 HAL (CubeIDE generated)
├── decoder.py              # Python host-side packet decoder
└── README.md
```

---

## Known Gotchas

These are real bugs discovered during development — not obvious from the datasheet.

### 1. Wrong ADC clock source stalls everything silently

```c
// WRONG — 0b11 is reserved on STM32G4, ADC receives no kernel clock
RCC->CCIPR |= (3U << RCC_CCIPR_ADC12SEL_Pos);

// CORRECT — 0b01 selects PLLP as the ADC kernel clock
RCC->CCIPR |= (1U << RCC_CCIPR_ADC12SEL_Pos);
```

**Symptom:** Firmware boots, `ABC` prints over UART, but the main loop hangs forever. No DMA interrupts ever fire because the ADC never completes a conversion. The `ADCAL` and `ADEN` startup sequence uses a separate internal path and succeeds even without the kernel clock — which makes this extremely hard to diagnose.

### 2. DMAMUX clock enable is separate from DMA clock enable

```c
// WRONG — DMAMUX peripheral has no clock, request routing is silently broken
RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

// CORRECT
RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN;
```

**Symptom:** Same as above — DMA never triggers even after fixing the ADC clock. On STM32G4, DMAMUX1 is a separate peripheral with its own clock gate. Writing to `DMAMUX1_Channel0->CCR` without the clock enabled does nothing.

### 3. EXTSEL value is not portable across STM32 families

```c
// On STM32G4 specifically, EXTSEL = 9 → TIM1_TRGO (regular channel group)
// This mapping differs on F4, L4, and other families — always check your RM
ADC1->CFGR |= (9U << ADC_CFGR_EXTSEL_Pos);
```

Always verify against **RM0440 Table 163** for STM32G4. Copying EXTSEL values from other projects targeting different STM32 families will silently select the wrong trigger source.

---

## License

MIT — do whatever you want with it.
