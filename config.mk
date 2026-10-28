# Compile options

VERBOSE=n
OPT=g
USE_NANO=y
SEMIHOST=n
USE_FPU=y

# Libraries

USE_SAPI=y
USE_SAPI_INTERRUPTS=n

USE_LPCUSBLIB=n
USE_FATFS=n

USE_FREERTOS=n
FREERTOS_HEAP_TYPE=5

# Auto remoto: las interrupciones por pin las maneja gpio_irq.c.
# Sin esto, el driver de HC-SR04 de la sAPI define GPIO0/1/2_IRQHandler
# y el enlazador da "multiple definition".
DEFINES+=OVERRIDE_SAPI_HCSR04_GPIO_IRQ
