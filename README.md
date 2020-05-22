# STM32F4_MICROCARD_FATFS_SPI_DMA

Lightweight Microsoft File Allocation Table implementation on STM32F405 MCU along with data logging.

FAT libraries for microcontrollers are too detailed and not easy to add to the code. Also it is not allowing user to get into the low levels to achieve higher throughput. Because of this reasons and also it is good to know what is going on down there,
I implemented FAT File System according to the Microsoft's File Allocation Table Specification document.

1. CK_MICROCARD.c/h initializes the microsdcard for v2.0 compatible cards(previous version is really old and not necessary) then reads the sd cards sectors that hold information about formating of the card and existed files.

2. CK_MICROCARD.c/h works with 3 possible spi configurations which are polling, interrupt and dma(direct memory access).
I implemented this files for fligh logging so timing is critical. Polling takes 100s of microseconds to millisecond so it is not usable for me. DMA with interrupt takes only 25-40 microseconds. 

For logging purposes, create big empty file such as 100Mbyte txt file and copy it to card. Then provide this empty files name 
to variable "filename_to_look" in CK_MICROCARD.c file. I use HxD Text Editor to check results.

Example screenshots:


![Untitled](https://user-images.githubusercontent.com/61315249/75179499-44673680-574b-11ea-8933-973b9b6e3e1f.png)


![Untitled1](https://user-images.githubusercontent.com/61315249/75179506-46c99080-574b-11ea-991d-293b3b22a21f.png)
