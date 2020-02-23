
#include "stm32f4xx.h"

#include "USBD_CDC/CK_USBD_INTERFACE.h"
#include "CK_SPI_DMA.h"

#include "CK_SYSTEM.h"
#include "CK_TIME_HAL.h"
#include "CK_MICROCARD.h"
#include "CK_LOG.h"
#include "CK_GPIO.h"


uint8_t data[512];

int main(void){

    CK_SYSTEM_SetSystemClock(SYSTEM_CLK_168MHz); // ALWAYS FIRST

    HAL_Init();

	CK_GPIO_ClockEnable(GPIOC);

    CK_GPIO_Init(GPIOC, 7, CK_GPIO_OUTPUT, CK_GPIO_NOAF, CK_GPIO_PUSHPULL, CK_GPIO_MEDIUM, CK_GPIO_NOPUPD);

    CK_GPIO_ClearPin(GPIOC, 7); // Set pin to low.

    //CK_USBD_Init();

    CK_MICROCARD_Init(SPI_DMA_INTERRUPT_MULTIBLOCK);

    while(1){

    	// TO DO:
    	// 50Mhz method implemented but couldn't activated it yet.

    	/*
    	 *  SPI DMA INTERRUPT SINGLE MODE: 1 sector transfer is 200 microsec
    	 *  and after one transfer for the next transfer, busy time is around 1.2 millisec.
    	 *
    	 *  Single write is not meaningful for data burst since everytime microcard
    	 *  needs to open and close itself which creates long busy delays after transfer.
    	 */

    	/*
    	 *  SPI DMA INTERRUPT MULTI MODE: 1 sector transfer is 200 microsec
    	 *  No loop delay busy time is 6 microsec.
    	 *  125microsec delay busy time is 250 microsec.
    	 *  250microsec delay busy time is 550 microsec.
    	 *  Still better than single mode.
    	 *
    	 */

    	/*
    	 * IMPORTANT NOTE:
    	 *  Now microcard library reliably working with dual buffer
    	 *  However:
    	 *
    	 *  1. Once the transfer is started, the card is clocked so
    	 *  starts recording with a little delay that is why there is difference between
    	 *  first sector and second one. If i write consequtive numbers to sectors
    	 *  first is 0 the second is like 3 or 10 etc.(depending on card's speed)
    	 *  but rest is fine. So instead of correcting it with providing dummy clocks
    	 *  this can be considered as it actually starts recording a little after.
    	 *
    	 *  2. When N number of sectors are written in multi mode, if the code stops sending spi clock
    	 *  immediately then microcard just buffered received data inside not placed it to sector yet so
    	 *  last 40-50 etc sector will not be written. This can be considered as record finished a little early.
    	 *  To correct it i provided dummy clocks with extra spi transfer loop but can be ignored.
    	 *
    	 *  As a result it works fine the 2 are not problem just little details.
    	 *
    	 *
    	 */

    	CK_LOG_Update();

        CK_MICROCARD_Update();

        CK_TIME_DelayMicroSec(125);

    }

}
