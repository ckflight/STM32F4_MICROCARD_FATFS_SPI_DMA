
#ifndef CK_SPI_DMA_H_
#define CK_SPI_DMA_H_

#include "stm32f4xx.h"

void CK_SPI_DMA_Init(SPI_TypeDef* spi);

void CK_SPI_DMA_SetBuffer(uint8_t* dma_buffer, uint32_t transferSize);

void CK_SPI_DMA_ClearFlag(DMA_TypeDef* dma);

void CK_SPI_DMA_Enable(DMA_Stream_TypeDef* dma);

void CK_SPI_DMA_Disable(DMA_Stream_TypeDef* dma);

uint8_t CK_SPI_DMA_IsTransferComplete(DMA_TypeDef* dma);

uint16_t CK_SPI_DMA_NumberOfDataLeft(DMA_Stream_TypeDef* dma_stream);

#endif /* CK_SPI_DMA_H_ */
