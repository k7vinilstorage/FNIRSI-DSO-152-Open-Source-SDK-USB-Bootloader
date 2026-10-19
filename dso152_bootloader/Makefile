CC = /home/joao/projects/TCC-2/SDK/Lyra-sdk/prebuilts/gcc/linux-x86/arm/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux/bin/arm-none-eabi-gcc
OBJCOPY = /home/joao/projects/TCC-2/SDK/Lyra-sdk/prebuilts/gcc/linux-x86/arm/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux/bin/arm-none-eabi-objcopy
SIZE = /home/joao/projects/TCC-2/SDK/Lyra-sdk/prebuilts/gcc/linux-x86/arm/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux/bin/arm-none-eabi-size

CFLAGS = -mcpu=cortex-m3 -mthumb -Os -Wall -Wextra \
         -ffunction-sections -fdata-sections -flto \
         -DSTM32F103xB -DUSE_HAL_DRIVER -DUSE_FULL_LL_DRIVER

INCLUDES = -IInc \
           -IDrivers/STM32F1xx_HAL_Driver/Inc \
           -IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy \
           -IDrivers/CMSIS/Device/ST/STM32F1xx/Include \
           -IDrivers/CMSIS/Include \
           -IMiddlewares/ST/STM32_USB_Device_Library/Core/Inc \
           -IMiddlewares/ST/STM32_USB_Device_Library/Class/MSC/Inc

LDFLAGS = -mcpu=cortex-m3 -mthumb -TCubeIDE/STM32F103C8TX_FLASH.ld \
          -Wl,--gc-sections -flto -specs=nano.specs -specs=nosys.specs \
          -Wl,-Map=dso152_bootloader.map

SRCS = Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd_ex.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_gpio.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_rcc.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_usb.c \
       Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_utils.c \
       Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Src/usbd_msc.c \
       Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Src/usbd_msc_bot.c \
       Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Src/usbd_msc_data.c \
       Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Src/usbd_msc_scsi.c \
       Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c \
       Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c \
       Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c \
       Src/fat32.c \
       Src/ihex_parser.c \
       Src/main.c \
       Src/stm32f1xx_hal_msp.c \
       Src/stm32f1xx_it.c \
       Src/system_stm32f1xx.c \
       Src/usb_device.c \
       Src/usbd_conf.c \
       Src/usbd_desc.c \
       Src/usbd_storage_if.c \
       Src/st7789_boot.c \
       Src/font8x8.c

ASRCS = CubeIDE/Startup/startup_stm32f103c8tx.s

OBJS = $(SRCS:.c=.o) $(ASRCS:.s=.o)

all: dso152_bootloader.bin

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

dso152_bootloader.elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

dso152_bootloader.bin: dso152_bootloader.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) dso152_bootloader.elf dso152_bootloader.bin dso152_bootloader.map
