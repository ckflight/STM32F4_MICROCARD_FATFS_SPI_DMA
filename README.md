# STM32F4_MICROCARD_FAT32

Lightweight Microsoft File Allocation Table implementation on STM32F405 MCU.

FAT libraries for microcontrollers are too detailed and not easy to add to code. 
I implemented FAT according to Microsoft File Allocation Table Specification.pdf file.

1. CK_MICROCARD.c/h file initializes the microsdcard for v2.0 compatible cards(previous version is really old and not necessary) then reads the sd cards sectors that hold information about formating of the card.

2. CK_MICROCARD.c/h works with 3 possible spi configurations which are polling, interrupt and dma(direct memory access).
I implemented this files for fligh logging so timing is critical. Polling takes 100s of microseconds to millisecond so it is not usable for me. DMA with interrupt takes only 25-40 microseconds. 

