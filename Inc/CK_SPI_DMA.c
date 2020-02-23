
#include "CK_SPI_DMA.h"
#include "string.h"			// memcpy

#define DMA_BUFFER_SIZE         512
#define RCC_DMA1EN              1u<<21 // DMA1 Clock enable
#define RCC_DMA2EN              1u<<22 // DMA2 Clock enable

#define DMA_STREAM0_CLEAR       0x3D<<0
#define DMA_STREAM1_CLEAR       0x3D<<6
#define DMA_STREAM2_CLEAR       0x3D<<16
#define DMA_STREAM3_CLEAR       0x3D<<22

#define DMA_STREAM4_CLEAR       0x3D<<0
#define DMA_STREAM5_CLEAR       0x3D<<6
#define DMA_STREAM6_CLEAR       0x3D<<16
#define DMA_STREAM7_CLEAR       0x3D<<22

#define DMA_STREAM4_TCIF        1u<<5

#define DMA_CR_Enable           1u<<0

DMA_Stream_TypeDef* dma_stream_n;
DMA_TypeDef* dma_n;

uint8_t dma_tx_buffer[DMA_BUFFER_SIZE];

uint16_t dmaBufferIndex;

/*
 * 1. CK_SPI_DMA_Init use it once to configure spi dma
 * Then before each transfer
 *
 * 2. CK_SPI_DMA_SetBuffer to setup tranfer buffer and its size
 * 3. CK_SPI_DMA_Enable
 * 4. CK_SPI_EnableDMA
 * 5. When finished it will go to interrupt.
 */

void CK_SPI_DMA_Init(SPI_TypeDef* spi){

	// SPI2 uses DMA1 request
	if(spi == SPI2){
		RCC->AHB1ENR |= RCC_DMA1EN; // DMA1 Clock enable
		dma_stream_n = DMA1_Stream4;
		dma_n = DMA1;

		CK_SPI_DMA_ClearFlag(dma_n);

		dma_stream_n->PAR   = (uint32_t)(&spi->DR);     // Set Peripheral's data transfer address

        dma_stream_n->CR   |= (1u<<4) | (1u<<6) | (1<<10) | (2u<<16) | (0u<<25); // TCIE, Memory to Peripheral, Memory increment, High priority, channel 0

        NVIC_EnableIRQ(DMA1_Stream4_IRQn); // Enable interrupt

	}

}

// This will be called before starting the transfer by enabling dma.
void CK_SPI_DMA_SetBuffer(uint8_t* dma_buffer, uint32_t transferSize){

	dma_stream_n->M0AR  = (uint32_t)dma_buffer;  // Set data buffer address
	dma_stream_n->NDTR  = transferSize;          // Set number of data to transfer

}

void CK_SPI_DMA_ClearFlag(DMA_TypeDef* dma){

    dma->HIFCR = DMA_STREAM4_CLEAR;
}

void CK_SPI_DMA_Enable(DMA_Stream_TypeDef* dma){

    dma->CR |= DMA_CR_Enable; //Enable
}

void CK_SPI_DMA_Disable(DMA_Stream_TypeDef* dma){

    dma->CR &= ~DMA_CR_Enable; //Disable
}

uint8_t CK_SPI_DMA_IsTransferComplete(DMA_TypeDef* dma){

    if(dma->HISR & DMA_STREAM4_TCIF){
        return 1; // Completed
    }
    return 0;
}

uint16_t CK_SPI_DMA_NumberOfDataLeft(DMA_Stream_TypeDef* dma_stream){

    return dma_stream->NDTR;
}















