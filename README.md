# STM32 Temperature Monitor Bare-Metal

Dual LM35 temperature monitor on STM32F401RE using bare-metal register programming, with SSD1306 OLED output and Proteus simulation support.

## What this project does

- Reads 2 LM35 sensors on ADC channels:
	- LM35-1 on PA6 (ADC1 IN6)
	- LM35-2 on PA7 (ADC1 IN7)
- Uses TIM2 update event to trigger ADC conversion sequence every 1 second
- Converts ADC readings to Fahrenheit
- Displays both temperatures on SSD1306 OLED over I2C1 (PB8 SCL, PB9 SDA)
- Toggles LED on PA5 using TIM2 CH1 compare (heartbeat)

## Repository layout

- `_datasheets_/` : Reference PDFs used in development
- `stm32cube_project/smart_fan_v1_0/` : STM32CubeIDE project
- `proteus/` : Proteus simulation project
- `arduino_for_compare/arduino_for_compare.ino` : Arduino implementation for side-by-side comparison
- `learning_notes.pdf` : Personal study notes captured during development

## Prerequisites

1. STM32CubeIDE installed
2. Proteus installed
3. Git installed

## Clone

```bash
git clone https://github.com/Jkdxbns/stm32-temperature-monitor-bare-metal.git
cd stm32-temperature-monitor-bare-metal
```

## Run in STM32CubeIDE (build firmware)

1. Open STM32CubeIDE.
2. Import project:
	 - File -> Import -> Existing Projects into Workspace
	 - Select folder: `stm32cube_project/smart_fan_v1_0`
3. Build:
	 - Project -> Build Project
4. Build output is generated under:
	 - `stm32cube_project/smart_fan_v1_0/Debug/`
	 - Use `smart_fan_v1_0.elf` in Proteus as the program file.

## Build footprint (Debug)

Measured from STM32CubeIDE build output:

```text
arm-none-eabi-size smart_fan_v1_0.elf
	 text    data    bss    dec    hex    filename
	 3676       4    1580   5260   148c   smart_fan_v1_0.elf
```

- Flash (ROM) used: 3680 bytes (text + data)
- SRAM (RAM) used: 1584 bytes (data + bss)

These values are from Debug configuration and can change with compiler optimization level and feature additions.

## Arduino comparison build logs

For comparison, the Arduino sketch build reported:

```text
Sketch uses 14790 bytes (45%) of program storage space. Maximum is 32256 bytes.
Global variables use 364 bytes (17%) of dynamic memory, leaving 1684 bytes for local variables. Maximum is 2048 bytes.
```

## Bare-metal vs Arduino (quick comparison)

| Metric | STM32 bare-metal (this project) | Arduino sketch (comparison) |
|---|---:|---:|
| Program memory used | 3680 bytes (Flash: text + data) | 14790 bytes |
| Data RAM used | 1584 bytes (SRAM: data + bss) | 364 bytes globals |
| Peripheral control style | Direct register-level control | Framework/Arduino abstraction |
| ADC trigger model | Hardware timer-triggered ADC sequence | Typically software-managed in loop |
| I2C display driver flow | Manual start/address/data/stop handling | Library-level calls |

STM32 RAM figure includes linker-reserved stack + heap (1536 bytes) in addition to globals, so it is intentionally conservative for runtime safety.
Arduino IDE line reports global/static memory separately, so these two RAM numbers are not a direct apples-to-apples comparison.

This comparison shows why this implementation is a strong embedded-systems project: it demonstrates precise low-level control, timer-synchronized acquisition, and interrupt-driven data handling with significantly compact firmware size.

## Run in Proteus

1. Open `proteus/New Project.pdsprj`.
2. In schematic, open STM32F401RE device properties.
3. Set Program File to the built ELF path:
	 - `stm32cube_project/smart_fan_v1_0/Debug/smart_fan_v1_0.elf`
4. Start simulation.
5. Change LM35 source values in Proteus and observe OLED updates.

## Pin map

- PA5: LED output (TIM2 CH1 toggle)
- PA6: LM35-1 analog input (ADC1 IN6)
- PA7: LM35-2 analog input (ADC1 IN7)
- PB8: I2C1 SCL (OLED)
- PB9: I2C1 SDA (OLED)

## Firmware behavior details

- System clock: HSI 16 MHz
- TIM2:
	- PSC = 15999
	- ARR = 999
	- Update event period = 1 second
- ADC1:
	- Scan mode enabled
	- Sequence length = 2 conversions
	- External trigger = TIM2 TRGO (update event)
	- EOC interrupt enabled
- Temperature conversion:
	- LM35 scale = 10 mV per degC
	- Fahrenheit shown as integer on OLED

## Implementation highlights

- Bare-metal register-level firmware (no HAL peripheral drivers in the application flow)
- Timer-driven sampling architecture:
	- TIM2 update event is exported as TRGO
	- ADC conversion sequence starts from hardware trigger, not software polling
- Multi-channel ADC sequencing with interrupt handling:
	- ADC1 scans IN6 and IN7 in sequence
	- End-of-conversion interrupt captures both samples safely
- Low-level I2C master implementation:
	- Start/address/data/stop handling is implemented directly using I2C status flags
	- SSD1306 command/data framing is handled manually
- Display update efficiency:
	- OLED lines are refreshed only when temperature values change

This project was built from scratch as a personal bare-metal learning exercise and reflects practical understanding of clocks, GPIO alternate functions, timer-triggered ADC, interrupt-driven data capture, and I2C display control.

## Included references

The `_datasheets_/` directory includes:

- STM32F401 reference and programming manuals
- STM32 ADC mode notes
- SSD1306 datasheet
- I2C spec (UM10204)

Additional document in repo root:

- `learning_notes.pdf` : concise learning journey and implementation notes

## Troubleshooting

1. No OLED output
	 - Check OLED I2C address in simulation (expected 0x3C)
	 - Confirm PB8/PB9 wiring and pull-ups in Proteus
2. Temperatures do not change
	 - Verify LM35 model outputs connected to PA6/PA7
	 - Confirm ADC trigger source is TIM2 update
3. Proteus cannot run firmware
	 - Rebuild project in CubeIDE
	 - Re-select latest ELF path in Proteus MCU properties

## Notes

- The code is implemented in bare-metal style (direct register access) in `Core/Src/main.cpp`.
- Cube-generated files are kept for startup/system compatibility.
