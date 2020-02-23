

#ifndef CK_MICROCARD_H_
#define CK_MICROCARD_H_

#include "stm32f4xx.h"

typedef enum{
	SPI_POLLING_SINGLEBLOCK,
	SPI_POLLING_MULTIBLOCK,

	SPI_DMA_POLLING_SINGLEBLOCK,
	SPI_DMA_POLLING_MULTIBLOCK,

	SPI_DMA_INTERRUPT_SINGLEBLOCK,
	SPI_DMA_INTERRUPT_MULTIBLOCK

}microcard_transfer_modes_e;

void CK_MICROCARD_Init(microcard_transfer_modes_e mode);

void CK_MICROCARD_InitCard(void);

void CK_MICROCARD_AccessCardDetails(void);

void CK_MICROCARD_SPIFullSpeed(void);

void CK_MICROCARD_Update(void);

void CK_MICROCARD_ReadData(uint32_t sector, uint32_t length, uint8_t* buffer);

void CK_MICROCARD_WriteData(uint32_t sector);

void CK_MICROCARD_TransferReady(void);

uint8_t CK_MICROCARD_SendAppCommand(uint8_t cmd, uint32_t arg);

uint8_t CK_MICROCARD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc);

uint8_t CK_MICROCARD_WaitForResponse(int bytesToWait);

int CK_MICROCARD_WaitForIdle(int bytesToWait);

void CK_MICROCARD_DeselectCard(void);

void CK_MICROCARD_SelectCard(void);

uint8_t CK_MICROCARD_WaitCardBusy(void);

uint8_t CK_MICROCARD_CheckIsCardBusy(void);

uint8_t CK_MICROCARD_MultiWrite_CheckIsCardBusy(void);

void CK_MICROCARD_WaitTransferComplete(void);

uint16_t CK_MICROCARD_NumberOfDataLeft(void);

uint16_t CK_MICROCARD_GetStartByteOfFile(uint8_t* buffer, uint8_t* filename_to_look);

#endif /* CK_MICROCARD_H_ */
