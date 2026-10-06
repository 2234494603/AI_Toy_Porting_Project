# CMake build

This project can be built with CMake and the `arm-none-eabi-gcc` toolchain.
The source tree is organized by module under `Project/Cold`, with CMake files
in `Project/Cmake` and generated files in `Project/Build`.

## Requirements

- CMake 3.20 or newer
- A CMake build backend, such as `make` for the included `Unix Makefiles` preset
- GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, `arm-none-eabi-size`)
- STM32CubeF1 package, such as `STM32Cube_FW_F1_V1.8.6`

Set `STM32CUBE_F1_PATH` to the STM32CubeF1 package root. The path must contain `Drivers/STM32F1xx_HAL_Driver` and `Drivers/CMSIS`.

## Build

```sh
export STM32CUBE_F1_PATH=/path/to/STM32Cube_FW_F1_V1.8.6
cd Project/Cmake
cmake --preset arm-gcc
cmake --build --preset arm-gcc
```

The build outputs are generated in `Project/Build/`:

- `UART_LED.elf`
- `UART_LED.hex`
- `UART_LED.bin`
- `UART_LED.map`

You can also pass the Cube path directly:

```sh
cmake -S Project/Cmake -B Project/Build \
  -DCMAKE_TOOLCHAIN_FILE=Project/Cmake/arm-none-eabi-gcc.cmake \
  -DSTM32CUBE_F1_PATH=/path/to/STM32Cube_FW_F1_V1.8.6 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build Project/Build
```
