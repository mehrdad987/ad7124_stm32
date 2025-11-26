# AD7124 STM32 Driver / Examples

## Overview

This repository provides an implementation for interfacing the AD7124 24-bit Σ-Δ ADC with an STM32 microcontroller using SPI and HAL (Hardware Abstraction Layer). It includes example applications demonstrating how to configure the ADC, perform conversions, and read data, as well as low-level driver code for register access, calibration, and error checking.  

## Features

- Full register-level control of AD7124: read/write all registers, including calibration.  
- Supports configuration of channels, gain, filter, reference source, and operational modes.  
- Example code demonstrating single and continuous conversion modes.  
- Designed for STM32 HAL: compatible with various STM32 MCU series.  
- Easy integration into STM32CubeIDE or other build environments.  

## Repository Structure

/ (root)
├── example/ # Example applications / demo code
│ ├── main.c # Example main loop using AD7124 driver
│ └── stm32yyxx_it.c # Interrupt handler example (if using DOUT/RDY)
├── lib/ # Driver library
│ ├── ad7124.h # Public header(s)
│ ├── ad7124.c # Driver implementation
│ └── ad7124_config.c # Configuration / platform-specific settings
└── README.md # This file

pgsql
Copy code

## Getting Started

### Prerequisites

- STM32 development environment (e.g. STM32CubeIDE)  
- A target STM32 board or MCU with SPI and GPIO support  
- AD7124 connected via SPI, with optional /CS pin and /DOUT (RDY) interrupt pin  

### Basic Steps

1. **Configure SPI** — set up SPI (e.g. full-duplex master, 8-bit data, MSB first, CPOL/CPHA as required by AD7124).  
2. **Configure /CS and /DOUT (RDY)** — if using /CS, assign a GPIO; if using DRDY, set up external interrupt on falling edge.  
3. **Include driver in project** — add `ad7124.h`, `ad7124.c`, and `ad7124_config.c` to your sources, and configure include paths.  
4. **Initialize and configure AD7124** — call initialization/config routines in your setup, set up desired channels, reference, gain, etc.  
5. **Read data** — trigger conversions and read samples. Use interrupt or polling depending on configuration.  

## Example

See `example/main.c` for a minimal demonstration. The code shows how to initialize the ADC, configure a channel, and read conversion results. You may also adapt the interrupt-driven example in `stm32yyxx_it.c` for DRDY-based sampling.  

## License

This project is released under the **MIT License**.  

## Acknowledgments & References

- The AD7124 is a high-precision, low-noise 24-bit analog-to-digital converter with configurable channels, gain, filters, and reference sources. :contentReference[oaicite:2]{index=2}  
- The design and structure take inspiration from existing no-OS or HAL-based examples of AD7124 integration. :contentReference[oaicite:3]{index=3}  

I
