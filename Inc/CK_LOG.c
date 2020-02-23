
#include "CK_SPI_DMA.h"
#include "CK_MICROCARD.h"

#include "CK_LOG.h"

#define BUFFER_SIZE		512

typedef enum{
	BUFFER1,
	BUFFER2
}buffers_e;

typedef struct{

	buffers_e last_buffer;

	uint8_t log_buffer_1[BUFFER_SIZE];
	uint8_t log_buffer_2[BUFFER_SIZE];

	uint16_t buffer_index;

}log_parameters_t;

log_parameters_t flightLog = {
	.last_buffer = BUFFER2,

	.buffer_index = 0
};

/*
 * While buffer 1 is being transfer by dma (200 microseconds it takes)
 * a few loop passes and those data needed to be
 * recorded so dual buffering is used.
 */

void CK_LOG_Update(void){

	static int counter = 0;

	if(flightLog.last_buffer == BUFFER2){

	    if(flightLog.buffer_index == BUFFER_SIZE){

	    	CK_SPI_DMA_SetBuffer(flightLog.log_buffer_1, flightLog.buffer_index);

	    	flightLog.buffer_index = 0;
	    	flightLog.last_buffer = BUFFER1;

	    	CK_MICROCARD_TransferReady();
	    	counter++;
	    }
	    else{

	    	// Put 32bytes of data to buffer1

	        for(int i = 0; i < 32; i++){
	    		flightLog.log_buffer_1[flightLog.buffer_index++] = counter;
	    	}
	    }
	}
	else if(flightLog.last_buffer == BUFFER1){

	    if(flightLog.buffer_index == BUFFER_SIZE){

	    	CK_SPI_DMA_SetBuffer(flightLog.log_buffer_2, flightLog.buffer_index);

	    	flightLog.buffer_index = 0;
	    	flightLog.last_buffer = BUFFER2;

	    	CK_MICROCARD_TransferReady();
	    	counter++;
	    }
	    else{

	    	// Put 32bytes of data to buffer2

	        for(int i = 0; i < 32; i++){
	    		flightLog.log_buffer_2[flightLog.buffer_index++] = counter;
	    	}
	    }
	}

}







