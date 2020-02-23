
#include "USBD_CDC/CK_USBD_INTERFACE.h"

#include "CK_SYSTEM.h"
#include "CK_TIME_HAL.h"
#include "CK_GPIO.h"
#include "CK_SPI.h"
#include "CK_SPI_DMA.h"

#include "CK_MICROCARD.h"

#include "stdbool.h"

#define MICROCARD_DMA           DMA1
#define MICROCARD_DMA_STREAM    DMA1_Stream4
#define MICROCARD_SPI           SPI2
#define MICROCARD_CS_PORT       GPIOB
#define MICROCARD_CS_PIN        12

#define CMD0        0
#define CMD6        6
#define CMD8        8
#define CMD9        9
#define CMD12       12
#define CMD17       17
#define CMD18       18
#define CMD24       24
#define CMD25       25
#define CMD55       55
#define CMD58       58
#define ACMD41      41

#define R1_RESPONSE_READY				0x00 // Bit 0
#define R1_RESPONSE_IDLE				0x01 // Bit 0
#define R1_RESPONSE_ILLEGAL_CMD			0x04 // Bit 1

#define MICROCARD_V1		1
#define MICROCARD_V2		2

// Root directory attributes
#define DIR_ATTR_READ_ONLY		0x01
#define DIR_ATTR_HIDDEN			0x02
#define DIR_ATTR_SYSTEM			0x04
#define DIR_ATTR_VOLUME_ID		0x08
#define DIR_ATTR_DIRECTORY		0x10
#define DIR_ATTR_ARCHIVE		0x20

// 50MHz max clock speed
// (not working yet but needed as well)
//#define HIGH_SPEED_MODE

//#define DEBUG_TIMING

typedef struct{

	uint8_t init_retry;

	bool is_card_fast;

	bool is_Initialized;

	bool is_dma_ready;

	bool is_log_buffer_full;

	bool is_card_high_capacity;

	uint8_t card_version;

	uint8_t card_speed_clock;

	uint32_t START_SECTOR;

	uint32_t CURRENT_SECTOR;

	uint32_t SECTOR_OFFSET;

	bool is_multi_started;

	uint32_t multi_number_of_sector;

	uint32_t TIME_OUT;

	uint8_t card_bpb[512];

}microcard_parameters_t;

microcard_parameters_t card = {
	.init_retry 			= 10,

	.is_card_fast 			= false,
	.is_Initialized 		= false,
	.is_dma_ready 			= true,
	.is_log_buffer_full 	= false,
	.is_card_high_capacity 	= false,

	.card_version 			= 0,
	.card_speed_clock 		= 0,

	.START_SECTOR 			= 0,
	.CURRENT_SECTOR 		= 0,
	.SECTOR_OFFSET 			= 0,

	.is_multi_started 		= false,
	.multi_number_of_sector = 50000,

	.TIME_OUT 				= 100 // some part uses *10 time_out so 100msec is fine

};

typedef struct{

	uint16_t BPB_BytsPerSec; 	// Byte size of each sector (Offset 11, 2 bytes)
	uint8_t  BPB_SecPerCluster;	// Number of sector of a cluster (Offset 13, 1 byte)
	uint16_t BPB_RsvdSecCnt; 	// Number of reserved sectors (Offset 14, 2 bytes)
	uint8_t  BPB_NumFATs;		// Number of FATs (Offset 16, 1 byte)
	uint32_t BPB_TotSec32;		// Total number of sectors (Offset 32, 4 bytes)
	uint32_t BPB_FATSz32;		// Number of sectors used by FAT (Offset 36, 4 bytes)
	uint32_t BPB_RootClus;		// This is set to the cluster number of the first cluster of the root directory, this value should be 2 (Offset 44, 4 bytes)

	uint32_t firstRootDirectorSector;

}microcard_bpb_t;

// Each created file is listed in root directory sector(firstRootDirectorSector)
// with 32 bytes of information for each file
// find the name of the file and decode its 32byte information to fill below variables.
// Later i can create file at desired size as well.

typedef struct{

	uint8_t  DIR_Name[11]; 		// Name of the DRIVE (Offset 0, 11 bytes)
	uint8_t  DIR_Attr;			// Directory attributes (Offset 11, 1 byte)
	uint16_t DIR_LstAccDate;	// Last access date (Offset 18, 2 bytes)
	uint16_t DIR_FstClusHI;		// High bytes of first cluster. This where the log is started (Offset 20, 2 bytes
	uint16_t DIR_FstClusLo;		// High bytes of first cluster. This where the log is started (Offset 26, 2 bytes
	uint32_t DIR_FileSize;		// File size (Offset 28, 4 bytes)

	uint32_t fileFirstCluster;

	uint32_t firsSectorOfFile;

}microcard_rootdirectory_t;

microcard_bpb_t boot_sector;

microcard_rootdirectory_t log_file;

#if defined(DEBUG_TIMING)
uint32_t sector_update_time;
uint32_t sector_start_time;
uint32_t sector_results[1000]; // make size equal to .multi_number_of_sector
int sectorIndex = 0;

uint32_t busy_update_time;
uint32_t busy_start_time;
uint32_t busy_results[1000]; // make size equal to .multi_number_of_sector
int busyIndex = 0;
#endif

microcard_transfer_modes_e transfer_mode;

void CK_MICROCARD_Init(microcard_transfer_modes_e mode){

	transfer_mode = mode;

    //card.SECTOR_OFFSET = 2048; // Read data uses this set it first.
	//card.SECTOR_OFFSET = 8192; // Read data uses this set it first.

    CK_GPIO_ClockEnable(MICROCARD_CS_PORT);

    CK_GPIO_Init(MICROCARD_CS_PORT, MICROCARD_CS_PIN, CK_GPIO_OUTPUT, CK_GPIO_NOAF, CK_GPIO_PUSHPULL, CK_GPIO_VERYHIGH, CK_GPIO_NOPUPD);

    CK_GPIO_SetPin(MICROCARD_CS_PORT, MICROCARD_CS_PIN);

    if(!CK_SPI_CheckInitialized(MICROCARD_SPI)){
        CK_SPI_Init(MICROCARD_SPI);
    }

    CK_MICROCARD_InitCard();

}

void CK_MICROCARD_InitCard(void){

    uint8_t ocr_register[4];
    uint8_t csd_register[16];
    uint8_t highspeed_response_register[64]; UNUSED(highspeed_response_register);
    uint8_t resp = 0xFF;


    while(resp != R1_RESPONSE_IDLE && card.init_retry--){
    	CK_MICROCARD_SelectCard();
        resp = CK_MICROCARD_SendCmd(CMD0, 0, 0x95);
        CK_MICROCARD_DeselectCard();
    }

    if (resp == R1_RESPONSE_IDLE){

        CK_MICROCARD_SelectCard();

        // CMD8 is for initialization of version 2.0 compatible card.
        // CMD8 response is R3 = R1 + 4 byte OCR
        resp = CK_MICROCARD_SendCmd(CMD8, 0x000001AB, 0x95);

        // Read OCR register
        if(resp == R1_RESPONSE_IDLE){
            for(int i = 0; i < 4 ; i++){
            	ocr_register[i] = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
            }
            if(ocr_register[3] == 0xAB){
                card.card_version = MICROCARD_V2; // Version 2
            }
        }
        CK_MICROCARD_DeselectCard();

        if(card.card_version  == MICROCARD_V2){

            // It takes a few attempts to get 0 response

        	resp = 0xFF;
        	CK_TIME_SetTimeOut(card.TIME_OUT);
            while(resp != R1_RESPONSE_READY && CK_TIME_GetTimeOut()){
                CK_MICROCARD_SelectCard();
                resp = CK_MICROCARD_SendAppCommand(ACMD41, 1 << 30);
                CK_MICROCARD_DeselectCard();
            }

            // The Card is initialized
            if(resp == R1_RESPONSE_READY){
                CK_MICROCARD_SelectCard();
                resp = CK_MICROCARD_SendCmd(CMD58, 0, 0x95);// Read OCR Register

                for(int i = 0; i < 4 ; i++){
                	ocr_register[i] = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
                }

                CK_MICROCARD_DeselectCard();

                uint32_t ocr = (ocr_register[0] << 24) | (ocr_register[1] << 16) | (ocr_register[2] << 8) | (ocr_register[0]);
                if(resp == R1_RESPONSE_READY){

                	card.is_card_high_capacity = ((ocr & (1<<30)) != 0);

                    card.is_card_fast = true;
                }


				#if defined(HIGH_SPEED_MODE)
            	/*
            	 * HIGH SPEED MODE
            	 * For cards v1.10 SDHC CL10 and higher supports high speed mode
            	 * The maximum spi clock will be 50MHz rather than 25MHz
            	 *
            	 */
                CK_TIME_SetTimeOut(card.TIME_OUT);
            	CK_MICROCARD_SelectCard();

            	uint8_t resp = 1;
            	while(resp != 0 && CK_TIME_GetTimeOut()){
            		resp = CK_MICROCARD_SendCmd(CMD6, 0x80000001, 0);
            	}

            	// 512 bits (64 bytes) result will be readed.
            	for(int i = 0; i < 64; i++){
            		highspeed_response_register[i] = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
            	}

            	// At least 8 clock cycle is needed.
            	CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
            	CK_SPI_Transfer(MICROCARD_SPI, 0xFF);

            	CK_MICROCARD_DeselectCard();
				#endif

                /* MicroCard Specifications.pdf in my Design Notes
                 *
                 * CSD register gives every information about the card
                 *
                 * In Normal    Mode TRAN_SPEED(4th byte) is 0x32 which is 25MHz max freq.
                 * In HighSpeed Mode TRAN_SPEED(4th byte) is 0x5A which is 50MHz max freq.
                 *
                 * Switch Function command (CMD6), the Version 1.10 and higher memory card
                 * can be placed in High-Speed mode.
                 *
                 * */

                CK_MICROCARD_SelectCard();

                resp = CK_MICROCARD_SendCmd(CMD9, 0, 0);

                resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
                resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFE);

                for(int byte = 0; byte < 16 ; byte++){
                	csd_register[byte] = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
                    if(byte == 3){
                    	if(csd_register[byte] == 0x32){
                    		card.card_speed_clock = 25;
                    	}
                    	else if(csd_register[byte] == 0x5A){
                    		card.card_speed_clock = 50;
                    	}
                    }
                }
                CK_MICROCARD_DeselectCard();

                card.is_Initialized = true;
            }

        }
    }
    else{
    	card.is_Initialized = false;
    	//CK_USBD_StringPrintln("ERROR, MICROCARD NOT INITIALIZED.");
    	//CK_USBD_Transmit();
    }

    CK_MICROCARD_SPIFullSpeed();

    // Configure SPI_DMA
    CK_SPI_DMA_Init(MICROCARD_SPI);

    if(card.is_Initialized){
    	CK_MICROCARD_AccessCardDetails();
    }


}

void CK_MICROCARD_AccessCardDetails(void){

	// All details are in Microsoft File Allocation Table Specification.pdf
	// First sector has bpb information and from there,
	// number of the first sector of the root file is calculated.

	uint8_t buffer[512];

	// BPB sector 0 (this 0 is actually 0 + offset) so the offset must be found to operate correctly.
	// BPB first byte is 0xEB so read until reaching that byte to determine the offset of the card.
	// One card has 2048, other has 8192 so 10000 read should be fine.

	// This information can be store in flash as well.
	//int sector_offset = 0;
	int sector_offset = 2000; // No need to start searching from 0 since it is 2048 or around 8192
	while(sector_offset < 10000){
		CK_MICROCARD_ReadData(sector_offset, 1, buffer);
		if(buffer[0] != 0xEB && buffer[2] != 0x90){
			sector_offset++;
		}else{
			card.SECTOR_OFFSET = sector_offset;
			break;
		}
	}


	// Read the boot sector
	CK_MICROCARD_ReadData(0, 1, buffer); // Read sector 0, and only 1 sector

	/*
	uint16_t BPB_BytsPerSec; 	// Byte size of each sector (Offset 11 2 bytes)
	uint8_t  BPB_SecPerCluster;	// Number of sector of a cluster (Offset 13 1 byte)
	uint16_t BPB_RsvdSecCnt; 	// Number of reserved sectors (Offset 14 2 bytes)
	uint8_t  BPB_NumFATs;		// Number of FATs (Offset 16 1 byte)
	uint32_t BPB_TotSec32;		// Total number of sectors (Offset 32 4 bytes)
	uint32_t BPB_FATSz32;		// Number of sectors used by FAT (Offset 36 4 bytes)
	*/

	// Boot sector bytes are in little endian format meaning LSB first
	boot_sector.BPB_BytsPerSec 	  = (uint16_t)(buffer[12] << 8 | buffer[11]);

	boot_sector.BPB_SecPerCluster = (uint8_t)buffer[13];

	boot_sector.BPB_RsvdSecCnt    = (uint16_t)(buffer[15] << 8 | buffer[14]);

	boot_sector.BPB_NumFATs		  = (uint8_t)buffer[16];

	boot_sector.BPB_TotSec32	  = (uint32_t)(buffer[35] << 24 | buffer[34] << 16 | buffer[33] << 8 | buffer[32]);

	boot_sector.BPB_FATSz32		  = (uint32_t)(buffer[39] << 24 | buffer[38] << 16 | buffer[37] << 8 | buffer[36]);

	boot_sector.BPB_RootClus      = (uint32_t)(buffer[47] << 24 | buffer[46] << 16 | buffer[45] << 8 | buffer[44]);

	boot_sector.firstRootDirectorSector = boot_sector.BPB_RsvdSecCnt + (boot_sector.BPB_FATSz32 * boot_sector.BPB_NumFATs);

	// Read the first root directory sector where info of files are stored.
	CK_MICROCARD_ReadData(boot_sector.firstRootDirectorSector, 1, buffer); // Read sector firstRootDirectorSector, and only 1 sector

	/*
	uint8_t  DIR_Name[11]; 		// Name of the DRIVE (Offset 0, 11 bytes)
	uint8_t  DIR_Attr;			// Directory attributes (Offset 11, 1 byte)
	uint16_t DIR_LstAccDate;	// Last access date (Offset 18, 2 bytes)
	uint16_t DIR_FstClusHI;		// High bytes of first cluster. This where the log is started (Offset 20, 2 bytes
	uint16_t DIR_FstClusLo;		// High bytes of first cluster. This where the log is started (Offset 26, 2 bytes
	uint32_t DIR_FileSize;		// File size (Offset 28, 4 bytes)
	*/

	// Even if the name of file is flight_log.txt this is how it is stored
	uint8_t filename_to_look[11] = {'F','L','I','G','H','T','~','1','T','X','T'};
	uint16_t start_byte_of_file = CK_MICROCARD_GetStartByteOfFile(buffer, filename_to_look);

	for(int i = 0; i < 11; i++){
		log_file.DIR_Name[i] = buffer[start_byte_of_file + i];
	}

	log_file.DIR_Attr = buffer[start_byte_of_file + 11];

	log_file.DIR_LstAccDate = (uint16_t)(buffer[start_byte_of_file + 19] << 8 | buffer[start_byte_of_file + 18]);

	log_file.DIR_FstClusHI = (uint16_t)(buffer[start_byte_of_file + 21] << 8 | buffer[start_byte_of_file + 20]);

	log_file.DIR_FstClusLo = (uint16_t)(buffer[start_byte_of_file + 27] << 8 | buffer[start_byte_of_file + 26]);

	log_file.DIR_FileSize = (uint32_t)(buffer[start_byte_of_file + 31] << 24 | buffer[start_byte_of_file + 30] << 16 | buffer[start_byte_of_file + 29] << 8 | buffer[start_byte_of_file + 28]);

	log_file.fileFirstCluster = (uint32_t)(log_file.DIR_FstClusHI << 16 | log_file.DIR_FstClusLo);


	log_file.firsSectorOfFile = ((log_file.fileFirstCluster - boot_sector.BPB_RootClus) * boot_sector.BPB_SecPerCluster) + boot_sector.firstRootDirectorSector;

	// This part is for precaution
	// If for some reason above info are corrupted
	// set these parameters to some safe values.
	if(log_file.firsSectorOfFile == 0){
		 // I am setting to this value rather than 32832 to understand that this loop set it.
		log_file.firsSectorOfFile = 32900;
	}
	if(card.SECTOR_OFFSET == 0){
		card.SECTOR_OFFSET = 8192;
	}

    card.START_SECTOR = log_file.firsSectorOfFile + card.SECTOR_OFFSET;

}


void CK_MICROCARD_SPIFullSpeed(void){

    // Initialize spi if microcard is not activated.
    // Clock cannot be changed without activating spi.

    if(!CK_SPI_CheckInitialized(MICROCARD_SPI)){
        CK_SPI_Init(MICROCARD_SPI);
    }

    CK_SPI_WaitTransfer(MICROCARD_SPI);

    // When spi1 is used with 90Mhz max clock this will be set to divide 2
    if(card.card_speed_clock == 50 && card.is_Initialized){
    	CK_SPI_ChangeClock(MICROCARD_SPI, CK_SPIx_CR1_Fclk_Div2); // 45 / 2 = 22.5 MHz Max speed
    }

    // When spi1 is used with 90Mhz max clock this will be set to divide 4
    else if(card.card_speed_clock == 25 && card.is_Initialized){
    	CK_SPI_ChangeClock(MICROCARD_SPI, CK_SPIx_CR1_Fclk_Div2); // 45 / 2 = 22.5 MHz Max speed
    }

    // If card is not initialized
    else{
    	CK_SPI_ChangeClock(MICROCARD_SPI, CK_SPIx_CR1_Fclk_Div2); // 45 / 2 = 22.5 MHz Max speed
    }


}

void CK_MICROCARD_Update(void){

	uint8_t resp; UNUSED(resp);

    if(card.is_log_buffer_full && card.is_Initialized){

    	if(card.is_card_fast || card.is_card_high_capacity){

    		/* READ HERE:
             * First 2 sectors will be used to write number of sectors
             * used during log etc. so Python code can know how much to read
             */

    		switch(transfer_mode){

    		case SPI_DMA_INTERRUPT_MULTIBLOCK:

    			// TESTED WORKS. FASTEST OPTION

    			// Card will not be deselected in multi mode until all sectors are written.
    			// When all sectors are written counter in the interrupt handler will deselect spi.
    			// Use a fast dedicated spi only for microcard

    			resp = CK_MICROCARD_MultiWrite_CheckIsCardBusy();
				if(resp == 0xFF && card.is_dma_ready){

					#if defined(DEBUG_TIMING)
					sector_start_time = CK_TIME_GetMicroSec_DWT();

					busy_update_time = CK_TIME_GetMicroSec_DWT() - busy_start_time;
					busy_results[busyIndex++] = busy_update_time;
					#endif

					card.is_dma_ready = false;
					CK_MICROCARD_WriteData(card.START_SECTOR + card.CURRENT_SECTOR);
				}
				break;

    		case SPI_DMA_INTERRUPT_SINGLEBLOCK:

    			// TESTED WORKS.

    			// Card will be deselected after card is not busy.
				resp = CK_MICROCARD_CheckIsCardBusy();
				if(resp == 0xFF && card.is_dma_ready){

					#if defined(DEBUG_TIMING)
					sector_start_time = CK_TIME_GetMicroSec_DWT();

					busy_update_time = CK_TIME_GetMicroSec_DWT() - busy_start_time;
					busy_results[busyIndex++] = busy_update_time;
					#endif

					card.is_dma_ready = false;
					CK_MICROCARD_WriteData(card.START_SECTOR + card.CURRENT_SECTOR);

				}
				break;

    		case SPI_DMA_POLLING_SINGLEBLOCK:
    		case SPI_POLLING_SINGLEBLOCK:

    			// TESTED WORKS.
    			for(int num = 0; num < card.multi_number_of_sector; num++){

					CK_MICROCARD_WriteData(card.START_SECTOR + card.CURRENT_SECTOR++);
				}
    			break;

    		case SPI_DMA_POLLING_MULTIBLOCK:
    		case SPI_POLLING_MULTIBLOCK:

    			// TESTED WORKS.
    			CK_MICROCARD_WriteData(card.START_SECTOR + card.CURRENT_SECTOR);

    			break;


    		default:
    			break;

    		}

    	}
    }
}

void CK_MICROCARD_ReadData(uint32_t sector, uint32_t length, uint8_t* buffer){

	uint8_t resp;

	// Single block read
	if(length == 1){

		CK_MICROCARD_SelectCard();

		do{
			// Send single block read command
			resp = CK_MICROCARD_SendCmd(CMD17, sector + card.SECTOR_OFFSET, 0);
		}
		while(resp != 0x00);

		do{
			resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
		}
		while(resp != 0xFE);

		for(int index = 0; index < 512; index++){

			*buffer++ = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
		}

		do{
			resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF); // CRC
		}
		while(resp != 0xFF);

		CK_MICROCARD_DeselectCard();

	}

	// Multi block read
	// For multi reading i did not implemented array return concept for each sector
	// If it is needed later i will implement.

	else if(length > 1){

		CK_MICROCARD_SelectCard();

		do{
			resp = CK_MICROCARD_SendCmd(CMD18, sector + card.SECTOR_OFFSET, 0); // Send multi block read command
		}
		while(resp != 0x00);

		do{
			resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
		}
		while(resp != 0xFE);

		for(uint32_t current_sector = 0; current_sector < length; current_sector++){

			if(current_sector != 0){
				CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
			}

			for(int index = 0; index < 512; index++){

				*buffer++ = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);

			}

			do{
				resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF); // CRC
			}
			while(resp != 0xFF);

		}

		resp = CK_MICROCARD_SendCmd(CMD12, 0, 0); // Send stop command

		CK_MICROCARD_DeselectCard();

	}

}

void CK_MICROCARD_WriteData(uint32_t sector){

	uint8_t resp; UNUSED(resp);
	int num;

	switch(transfer_mode){

	case SPI_POLLING_SINGLEBLOCK:
	    CK_MICROCARD_SelectCard();

	    resp = CK_MICROCARD_SendCmd(CMD24, sector, 0); // send sector number

	    CK_SPI_Transfer(MICROCARD_SPI, 0xFE); // Send Start Token

	    for(int i = 0; i < 512; i++){
	        resp = CK_SPI_Transfer(MICROCARD_SPI, '.');
	    }

	    resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
	    resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);

	    // Card deselected at the end of wait busy
	    resp = CK_MICROCARD_WaitCardBusy();
		break;

	case SPI_POLLING_MULTIBLOCK:

		if(!card.is_multi_started){

			// SPI will not be deselected untill whole block of sectors are written
			CK_MICROCARD_SelectCard();

			// This also needed to be send once
			resp = CK_MICROCARD_SendCmd(CMD25, sector, 0);

			card.is_multi_started = true;
		}

	    //it will write to numOfSector sector at once
	   	num = card.multi_number_of_sector;
	    while(num--){

	        CK_SPI_Transfer(MICROCARD_SPI, 0xFC); // Send Start Token

	        for(int i = 0; i < 512; i++){
	            resp = CK_SPI_Transfer(MICROCARD_SPI, '#');
	        }

	        resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
	        resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);

	        // Wait card busy
			CK_TIME_SetTimeOut(card.TIME_OUT*10);
			while((CK_MICROCARD_MultiWrite_CheckIsCardBusy() != 0xFF) && CK_TIME_GetTimeOut());

	    }

	    resp = CK_SPI_Transfer(MICROCARD_SPI, 0x4D); // Send Stop Token
	    resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);

	    CK_MICROCARD_DeselectCard();

	    card.is_multi_started = false;
		break;

	case SPI_DMA_POLLING_SINGLEBLOCK:

	    CK_MICROCARD_SelectCard();

	    resp = CK_MICROCARD_SendCmd(CMD24, sector, 0);

	    CK_SPI_Transfer(MICROCARD_SPI, 0xFE); // Send Start Token

	    // CK_SPI_DMA_SetBuffer is called before enabling dma
	    CK_SPI_DMA_Enable(MICROCARD_DMA_STREAM);
	    CK_SPI_EnableDMA(MICROCARD_SPI);

	    CK_TIME_SetTimeOut(card.TIME_OUT*10);
	    while(!CK_SPI_DMA_IsTransferComplete(MICROCARD_DMA) && CK_TIME_GetTimeOut());

	    CK_SPI_DMA_Disable(MICROCARD_DMA_STREAM);
	    CK_SPI_DisableDMA(MICROCARD_SPI);

	    // Send 2byte crc. CRC value does not matter.
	    CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
	    CK_SPI_Transfer(MICROCARD_SPI, 0xFF);

	    // Card deselected at the end of wait busy
	    resp = CK_MICROCARD_WaitCardBusy();

		break;

	case SPI_DMA_POLLING_MULTIBLOCK:

		if(!card.is_multi_started){

			// SPI will not be deselected untill whole block of sectors are written
			CK_MICROCARD_SelectCard();

			// This also needed to be send once
			resp = CK_MICROCARD_SendCmd(CMD25, sector, 0);

			card.is_multi_started = true;
		}

		 //it will write to numOfSector sector at once
	    num = card.multi_number_of_sector;
	    while(num--){

	        CK_SPI_Transfer(MICROCARD_SPI, 0xFC); // Send Start Token

	        // CK_SPI_DMA_SetBuffer is called before enabling dma
	        // so now just start transfer
	        CK_SPI_DMA_Enable(MICROCARD_DMA_STREAM);
	        CK_SPI_EnableDMA(MICROCARD_SPI);

	        CK_TIME_SetTimeOut(card.TIME_OUT*10);
	        while(!CK_SPI_DMA_IsTransferComplete(MICROCARD_DMA) && CK_TIME_GetTimeOut());

	        CK_SPI_DMA_Disable(MICROCARD_DMA_STREAM);
	        CK_SPI_DisableDMA(MICROCARD_SPI);

	        // Send 2byte crc. CRC value does not matter.
	        resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
	        resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);

	        // Wait card busy
	        CK_TIME_SetTimeOut(card.TIME_OUT*10);
	        while((CK_MICROCARD_MultiWrite_CheckIsCardBusy() != 0xFF) && CK_TIME_GetTimeOut());

	    }

	    resp = CK_SPI_Transfer(MICROCARD_SPI, 0x4D); // Send Stop Token
	    resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);

	    card.is_multi_started = false;

	    CK_MICROCARD_DeselectCard();
		break;

	case SPI_DMA_INTERRUPT_SINGLEBLOCK:

	    CK_MICROCARD_SelectCard();

	    resp = CK_MICROCARD_SendCmd(CMD24, sector, 0);

	    CK_SPI_Transfer(MICROCARD_SPI, 0xFE); // Send Start Token

	    // CK_SPI_DMA_SetBuffer is called before enabling dma
	    // so now just start transfer
	    CK_SPI_DMA_Enable(MICROCARD_DMA_STREAM);
	    CK_SPI_EnableDMA(MICROCARD_SPI);
		break;

	case SPI_DMA_INTERRUPT_MULTIBLOCK:

		// This part will be used once at the start only.
		if(!card.is_multi_started){

			// SPI will not be deselected until all sectors are written
			CK_MICROCARD_SelectCard();

			// This also needed to be send once
			resp = CK_MICROCARD_SendCmd(CMD25, sector, 0);

			card.is_multi_started = true;
		}

		CK_SPI_Transfer(MICROCARD_SPI, 0xFC); // Send Start Token

		// CK_SPI_DMA_SetBuffer is called before enabling dma
		// so now just start transfer
	    CK_SPI_DMA_Enable(MICROCARD_DMA_STREAM);
	    CK_SPI_EnableDMA(MICROCARD_SPI);
		break;

	default:
		break;

	}

}


void CK_MICROCARD_TransferReady(void){
	card.is_log_buffer_full = true;
}

uint8_t CK_MICROCARD_SendAppCommand(uint8_t cmd, uint32_t arg){

    CK_MICROCARD_SendCmd(CMD55, 0, 0);

    return CK_MICROCARD_SendCmd(cmd, arg, 0);
}

uint8_t CK_MICROCARD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc){

    //CK_MICROCARD_WaitForIdle(8);
	CK_SPI_Transfer(MICROCARD_SPI,0xFF); // Works as well do not spend extra time with 8 spi transfer

    //Send command packet
    CK_SPI_Transfer(MICROCARD_SPI, cmd | 0x40);
    CK_SPI_Transfer(MICROCARD_SPI, arg >> 24);
    CK_SPI_Transfer(MICROCARD_SPI, arg >> 16);
    CK_SPI_Transfer(MICROCARD_SPI, arg >> 8);
    CK_SPI_Transfer(MICROCARD_SPI, arg);
    CK_SPI_Transfer(MICROCARD_SPI, crc); // important for cmd0 and cmd8

    // Command response time is 1 to 8 bytes
    return CK_MICROCARD_WaitForResponse(8); // 8 is to be sure

}

uint8_t CK_MICROCARD_WaitForResponse(int bytesToWait){

    for(int i=0; i < bytesToWait + 1; i++){
        uint8_t response = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
        if (response != 0xFF){
            return response;
        }
    }
    return 0xFF;
}

int CK_MICROCARD_WaitForIdle(int bytesToWait){

    while(bytesToWait > 0){
        uint8_t res = CK_SPI_Transfer(MICROCARD_SPI,0xFF);
        if (res == 0xFF){
            return 1;
        }
        bytesToWait--;
    }
    return 0;
}

void CK_MICROCARD_DeselectCard(void){

    for(int i=0; i < 8; i++){
        uint8_t resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
        UNUSED(resp);
    }

    CK_GPIO_SetPin(MICROCARD_CS_PORT, MICROCARD_CS_PIN);
}

void CK_MICROCARD_SelectCard(void){

    CK_GPIO_ClearPin(MICROCARD_CS_PORT, MICROCARD_CS_PIN);
}

uint8_t CK_MICROCARD_WaitCardBusy(void){

    uint8_t resp;
    CK_TIME_SetTimeOut(card.TIME_OUT);
    while(((resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF)) != 0xFF) && CK_TIME_GetTimeOut());

    CK_MICROCARD_DeselectCard();
    return resp;// 0xFF means OK.

}

uint8_t CK_MICROCARD_CheckIsCardBusy(void){

	// In single write the card busy will be checked at the end
	// so now card can be deselected as well.

    CK_MICROCARD_SelectCard();

    uint8_t resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);// 0xFF means OK.

    CK_MICROCARD_DeselectCard();

    return resp;

}

uint8_t CK_MICROCARD_MultiWrite_CheckIsCardBusy(void){

	// In multi write spi will not deactivated until the end of all sectors written

    uint8_t resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);// 0xFF means OK.
    return resp;

}

void CK_MICROCARD_WaitTransferComplete(void){

    while(!CK_SPI_DMA_IsTransferComplete(MICROCARD_DMA));
}

uint16_t CK_MICROCARD_NumberOfDataLeft(void){

    return CK_SPI_DMA_NumberOfDataLeft(MICROCARD_DMA_STREAM);
}

void DMA1_Stream4_IRQHandler(void){

	uint8_t resp; UNUSED(resp);

    if(CK_SPI_DMA_IsTransferComplete(MICROCARD_DMA)){ // Transfer of one sector is done.

		CK_SPI_DMA_Disable(MICROCARD_DMA_STREAM);

        CK_SPI_DisableDMA(MICROCARD_SPI);

        CK_SPI_DMA_ClearFlag(MICROCARD_DMA);

        // Send 2byte CRC. CRC value does not matter.
        resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
        resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);

        card.is_log_buffer_full = false;    // log data is sent fill it again.

		card.is_dma_ready = true;       	// dma transfer is done

        card.CURRENT_SECTOR++;          	// move to the next sector.

		// Later use this part to decide to end recording etc.
        // For example a disarm would trig here
        // Or simply put large enough value to multi write
        // which will be more than enough for full flight.
        // 250M file has 500.000sectors to write
        // If 32byte is logged then 500 sector will be written every second.
		if(card.CURRENT_SECTOR == card.multi_number_of_sector){


			if(transfer_mode == SPI_DMA_INTERRUPT_MULTIBLOCK){

				// Read note on MICROCARD_STM32F405 project's main
				// (can be ignored but record will finish a few sector earlier.)
				for(int i = 0; i < 100; i++){
					CK_SPI_Transfer(MICROCARD_SPI, 0xFF);
				}

				// Send the stop token when all sectors are written
				resp = CK_SPI_Transfer(MICROCARD_SPI, 0x4D); 	// Send Stop Token
				resp = CK_SPI_Transfer(MICROCARD_SPI, 0xFF);

				CK_MICROCARD_DeselectCard();

				card.is_multi_started = false;

			}
			else if(transfer_mode == SPI_DMA_INTERRUPT_SINGLEBLOCK){

				while(1);

			}


			#if defined(DEBUG_TIMING)
			busyIndex = 0;
			sectorIndex = 0;
			UNUSED(sector_results);
			UNUSED(busy_results);
			#endif
			while(1);

		}

		#if defined(DEBUG_TIMING)
		sector_update_time = CK_TIME_GetMicroSec_DWT() - sector_start_time;
		sector_results[sectorIndex++] = sector_update_time;

		busy_start_time = CK_TIME_GetMicroSec_DWT();
		#endif

    }
}

uint16_t CK_MICROCARD_GetStartByteOfFile(uint8_t* buffer, uint8_t* filename_to_look){

	int states = 0;
	int start_byte_of_file = 0;

	for(int index = 0; index < 512; index++){

		uint8_t current_byte = buffer[index];

		switch(states){
		case 0:
			if(current_byte == filename_to_look[0]){
				states++;
				start_byte_of_file = index;
				break;
			}
			start_byte_of_file = 0;
			states = 0;
			break;

		case 1:
			if(current_byte == filename_to_look[1]){
				states++;
				break;
			}
			start_byte_of_file = 0;
			states = 0;
			break;

		case 2:
			if(current_byte == filename_to_look[2]){
				states++;
				break;
			}
			start_byte_of_file = 0;
			states = 0;
			break;

		case 3:
			if(current_byte == filename_to_look[3]){
				states++;
				break;
			}
			start_byte_of_file = 0;
			states = 0;
			break;

		case 4:
			if(current_byte == filename_to_look[4]){
				states++;
				break;
			}
			start_byte_of_file = 0;
			states = 0;
			break;

		case 5:
			if(current_byte == filename_to_look[5]){
				states++;
				break;
			}
			start_byte_of_file = 0;
			states = 0;
			break;

		case 6:
			if(current_byte == filename_to_look[6]){
				states++;
				break;
			}
			start_byte_of_file = 0;
			states = 0;
			break;

		case 7:
			if(current_byte == filename_to_look[7]){
				states++;
				break;
			}
			start_byte_of_file = 0;
			states = 0;
			break;

		case 8:
			if(current_byte == filename_to_look[8]){
				states++;
				break;
			}
			start_byte_of_file = 0;
			states = 0;
			break;

		case 9:
			if(current_byte == filename_to_look[9]){
				states++;
				break;
			}
			start_byte_of_file = 0;
			states = 0;
			break;

		case 10:
			if(current_byte == filename_to_look[10]){
				states = 0;
				return start_byte_of_file;
				break;
			}
			start_byte_of_file = 0;
			states = 0;
			break;

		}
	}

	return 0;

}
