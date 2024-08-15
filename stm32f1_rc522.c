#include "stm32f1_rc522.h"

uint8_t RC522_SPI_Transfer(u8 data);
void Write_MFRC522(uchar addr, uchar val);
uchar Read_MFRC522(uchar addr);
void SetBitMask(uchar reg, uchar mask);
void ClearBitMask(uchar reg, uchar mask)  ;
void AntennaOn(void);
void AntennaOff(void);
void MFRC522_Reset(void);
uchar MFRC522_ToCard(uchar command, uchar *sendData, uchar sendLen, uchar *backData, uint *backLen);

uint8_t RC522_SPI_Transfer(u8 data)
{
	while(SPI_I2S_GetFlagStatus(MFRC522_SPI, SPI_I2S_FLAG_TXE)==RESET);
	SPI_I2S_SendData(MFRC522_SPI,data);

	while(SPI_I2S_GetFlagStatus(MFRC522_SPI, SPI_I2S_FLAG_RXNE)==RESET);
	return SPI_I2S_ReceiveData(MFRC522_SPI);
}

void Write_MFRC522(uchar addr, uchar val)
{
	/* CS LOW */
	GPIO_ResetBits(MFRC522_CS_GPIO, MFRC522_CS_PIN);

	/* 0XXXXXX0 */
	RC522_SPI_Transfer((addr<<1)&0x7E);	
	RC522_SPI_Transfer(val);
	
	/* CS HIGH */
	GPIO_SetBits(MFRC522_CS_GPIO, MFRC522_CS_PIN);
}

uchar Read_MFRC522(uchar addr)
{
	uchar val;

	/* CS LOW */
	GPIO_ResetBits(MFRC522_CS_GPIO, MFRC522_CS_PIN);

	/* 1XXXXXX0 */
	RC522_SPI_Transfer(((addr<<1)&0x7E) | 0x80);	
	val = RC522_SPI_Transfer(0x00);
	
	/* CS HIGH */
	GPIO_SetBits(MFRC522_CS_GPIO, MFRC522_CS_PIN);
	
	return val;	
	
}

void SetBitMask(uchar reg, uchar mask)  
{
    uchar tmp;
    tmp = Read_MFRC522(reg);
    Write_MFRC522(reg, tmp | mask);  /* set bit mask */
}



void ClearBitMask(uchar reg, uchar mask)  
{
    uchar tmp;
    tmp = Read_MFRC522(reg);
    Write_MFRC522(reg, tmp & (~mask));  /* clear bit mask */
} 

void AntennaOn(void)
{
	uchar temp;

	temp = Read_MFRC522(TxControlReg);
/*	if (!(temp & 0x03))
	{
		SetBitMask(TxControlReg, 0x03);
	} */
	SetBitMask(TxControlReg, 0x03);
}

void AntennaOff(void)
{
	ClearBitMask(TxControlReg, 0x03);
}


void MFRC522_Reset(void)
{
    Write_MFRC522(CommandReg, PCD_RESETPHASE);
}

void MFRC522_SPI_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	SPI_InitTypeDef SPI_InitStruct;
	
	RCC_APB2PeriphClockCmd(MFRC522_SPI_GPIO_RCC | RCC_APB2Periph_AFIO,ENABLE);		/* Enable clock GPIO */
	
	if(MFRC522_SPI==SPI1)
	{
		RCC_APB2PeriphClockCmd(MFRC522_SPI_RCC,ENABLE);																/* Enable clock SPI1 */
	}
	else
	{
		RCC_APB1PeriphClockCmd(MFRC522_SPI_RCC,ENABLE);																/* Enable clock SPI2 or SPI3 */
	}
	
	GPIO_InitStruct.GPIO_Pin = MFRC522_SCK_PIN | MFRC522_MISO_PIN | MFRC522_MOSI_PIN;	/* SCK, MISO, MOSI */
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(MFRC522_SPI_GPIO, &GPIO_InitStruct);																	/* init GPIO SPI */

	SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
	SPI_InitStruct.SPI_Direction= SPI_Direction_2Lines_FullDuplex;
	SPI_InitStruct.SPI_Mode = SPI_Mode_Master;
	SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStruct.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStruct.SPI_NSS = SPI_NSS_Soft;
	SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStruct.SPI_CRCPolynomial = 7;
	SPI_Init(MFRC522_SPI, &SPI_InitStruct);

	SPI_Cmd(MFRC522_SPI, ENABLE);
}

void MFRC522_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	/* GPIOD Periph clock enable */
  RCC_APB2PeriphClockCmd(MFRC522_CS_RCC | MFRC522_RST_RCC, ENABLE);

  /* Configure CS is output pushpull mode */
  GPIO_InitStructure.GPIO_Pin = MFRC522_CS_PIN;				/* Set digital pin 10 as OUTPUT to connect it to the RFID /ENABLE pin  */
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(MFRC522_CS_GPIO, &GPIO_InitStructure);
	
  GPIO_InitStructure.GPIO_Pin = MFRC522_RST_PIN;			/* Set digital pin 10 , Not Reset and Power-down */
	GPIO_Init(MFRC522_RST_GPIO, &GPIO_InitStructure);
	
	GPIO_SetBits(MFRC522_CS_GPIO,MFRC522_CS_PIN);						/* Activate the RFID reader */
	GPIO_SetBits(MFRC522_RST_GPIO,MFRC522_RST_PIN);					/* not reset */

	/* spi config */
	MFRC522_SPI_Init();
	
	MFRC522_Reset();
	 	
	/* Timer: TPrescaler*TreloadVal/6.78MHz = 24ms */
	Write_MFRC522(TModeReg, 0x8D);		/*  Tauto=1; f(Timer) = 6.78MHz/TPreScaler */
	Write_MFRC522(TPrescalerReg, 0x3E);	/* TModeReg[3..0] + TPrescalerReg */
	Write_MFRC522(TReloadRegL, 30);           
	Write_MFRC522(TReloadRegH, 0);
	
	Write_MFRC522(TxAutoReg, 0x40);		/* 100%ASK */
	Write_MFRC522(ModeReg, 0x3D);		/*  CRC Gia tri ban dau 0x6363	??? */

	/* ClearBitMask(Status2Reg, 0x08);		MFCrypto1On=0
	Write_MFRC522(RxSelReg, 0x86);		RxWait = RxSelReg[5..0]
	Write_MFRC522(RFCfgReg, 0x7F);   		RxGain = 48dB */

	AntennaOn();		/* Mo Anten */
}

uchar MFRC522_ToCard(uchar command, uchar *sendData, uchar sendLen, uchar *backData, uint *backLen)
{
    uchar status = MI_ERR;
    uchar irqEn = 0x00;
    uchar waitIRq = 0x00;
    uchar lastBits;
    uchar n;
    uint i;

    switch (command)
    {
        case PCD_AUTHENT:		/* Xac nhan the gan */
		{
			irqEn = 0x12;
			waitIRq = 0x10;
			break;
		}
		case PCD_TRANSCEIVE:	/* Gui du lieu FIFO */
		{
			irqEn = 0x77;
			waitIRq = 0x30;
			break;
		}
		default:
			break;
    }
   
    Write_MFRC522(CommIEnReg, irqEn|0x80);	/* Yeu cau ngat */
    ClearBitMask(CommIrqReg, 0x80);			/* Clear tat ca cac bit yeu cau ngat */
    SetBitMask(FIFOLevelReg, 0x80);			/* FlushBuffer=1, Khoi tao FIFO */
    
	Write_MFRC522(CommandReg, PCD_IDLE);	/* NO action; Huy bo lenh hien hanh	??? */

		/* Ghi du lieu vao FIFO */
    for (i=0; i<sendLen; i++)
    {   
		Write_MFRC522(FIFODataReg, sendData[i]);    
	}

	Write_MFRC522(CommandReg, command);
    if (command == PCD_TRANSCEIVE)
    {    
		SetBitMask(BitFramingReg, 0x80);		/* StartSend=1,transmission of data starts  */
	}   
    
	i = 2000;
    do 
    {
		/* CommIrqReg[7..0] */
		/* Set1 TxIRq RxIRq IdleIRq HiAlerIRq LoAlertIRq ErrIRq TimerIRq */
        n = Read_MFRC522(CommIrqReg);
        i--;
    }
    while ((i!=0) && !(n&0x01) && !(n&waitIRq));

    ClearBitMask(BitFramingReg, 0x80);			
	
    if (i != 0)
    {    
        if(!(Read_MFRC522(ErrorReg) & 0x1B))	
        {
            status = MI_OK;
            if (n & irqEn & 0x01)
            {   
				status = MI_NOTAGERR;			
			}

            if (command == PCD_TRANSCEIVE)
            {
               	n = Read_MFRC522(FIFOLevelReg);
              	lastBits = Read_MFRC522(ControlReg) & 0x07;
                if (lastBits)
                {   
					*backLen = (n-1)*8 + lastBits;   
				}
                else
                {   
					*backLen = n*8;   
				}

                if (n == 0)
                {   
					n = 1;    
				}
                if (n > MAX_LEN)
                {   
					n = MAX_LEN;   
				}
				
                for (i=0; i<n; i++)
                {   
					backData[i] = Read_MFRC522(FIFODataReg);    
				}
            }
        }
        else
        {   
			status = MI_ERR;  
		}
        
    }
	
    /* SetBitMask(ControlReg,0x80);           */
    /* Write_MFRC522(CommandReg, PCD_IDLE);  */
 
    return status;
}

uchar MFRC522_Request(uchar reqMode, uchar *TagType)
{
	uchar status;  
	uint backBits;	

	Write_MFRC522(BitFramingReg, 0x07);	
	
	TagType[0] = reqMode;
	status = MFRC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);

	if ((status != MI_OK) || (backBits != 0x10))
	{    
		status = MI_ERR;
	}
   
	return status;
}

uchar MFRC522_Anticoll(uchar *serNum)
{
    uchar status;
    uchar i;
	uchar serNumCheck=0;
    uint unLen;
    

    /* ClearBitMask(Status2Reg, 0x08);		*/
    /* ClearBitMask(CollReg,0x80);			*/
	  Write_MFRC522(BitFramingReg, 0x00);		/* TxLastBists = BitFramingReg[2..0] */
 
    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

    if (status == MI_OK)
	{
		/* Check serial */
		for (i=0; i<4; i++)
		{   
		 	serNumCheck ^= serNum[i];
		}
		if (serNumCheck != serNum[i])
		{   
			status = MI_ERR;    
		}
    }

    /* SetBitMask(CollReg, 0x80);		*/

    return status;
} 


void CalulateCRC(uchar *pIndata, uchar len, uchar *pOutData)
{
    uchar i, n;

    ClearBitMask(DivIrqReg, 0x04);			
    SetBitMask(FIFOLevelReg, 0x80);			
    /* Write_MFRC522(CommandReg, PCD_IDLE); */

    for (i=0; i<len; i++)
    {   
		Write_MFRC522(FIFODataReg, *(pIndata+i));   
	}
    Write_MFRC522(CommandReg, PCD_CALCCRC);

    i = 0xFF;
    do 
    {
        n = Read_MFRC522(DivIrqReg);
        i--;
    }
    while ((i!=0) && !(n&0x04));			/* CRCIrq = 1 */

    pOutData[0] = Read_MFRC522(CRCResultRegL);
    pOutData[1] = Read_MFRC522(CRCResultRegM);
}


uchar MFRC522_SelectTag(uchar *serNum)
{
	uchar i;
	uchar status;
	uchar size;
	uint recvBits;
	uchar buffer[9]; 

    buffer[0] = PICC_SElECTTAG;
    buffer[1] = 0x70;
    for (i=0; i<5; i++)
    {
    	buffer[i+2] = *(serNum+i);
    }
	CalulateCRC(buffer, 7, &buffer[7]);	
    status = MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 9, buffer, &recvBits);
    
    if ((status == MI_OK) && (recvBits == 0x18))
    {   
		size = buffer[0]; 
	}
    else
    {   
		size = 0;    
	}

    return size;
}

uchar MFRC522_Auth(uchar authMode, uchar BlockAddr, uchar *Sectorkey, uchar *serNum)
{
    uchar status;
    uint recvBits;
    uchar i;
	uchar buff[12]; 

    buff[0] = authMode;
    buff[1] = BlockAddr;
    for (i=0; i<6; i++)
    {    
		buff[i+2] = *(Sectorkey+i);   
	}
    for (i=0; i<4; i++)
    {    
		buff[i+8] = *(serNum+i);   
	}
    status = MFRC522_ToCard(PCD_AUTHENT, buff, 12, buff, &recvBits);

    if ((status != MI_OK) || (!(Read_MFRC522(Status2Reg) & 0x08)))
    {   
		status = MI_ERR;   
	}
    
    return status;
}


uchar MFRC522_Read(uchar blockAddr, uchar *recvData)
{
    uchar status;
    uint unLen;

    recvData[0] = PICC_READ;
    recvData[1] = blockAddr;
    CalulateCRC(recvData,2, &recvData[2]);
    status = MFRC522_ToCard(PCD_TRANSCEIVE, recvData, 4, recvData, &unLen);

    if ((status != MI_OK) || (unLen != 0x90))
    {
        status = MI_ERR;
    }
    
    return status;
}


uchar MFRC522_Write(uchar blockAddr, uchar *writeData)
{
    uchar status;
    uint recvBits;
    uchar i;
	uchar buff[18]; 
    
    buff[0] = PICC_WRITE;
    buff[1] = blockAddr;
    CalulateCRC(buff, 2, &buff[2]);
    status = MFRC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff, &recvBits);

    if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A))
    {   
		status = MI_ERR;   
	}
        
    if (status == MI_OK)
    {
        for (i=0; i<16; i++)		/* 16 byte FIFO ghi du lieu vao */
        {    
        	buff[i] = *(writeData+i);   
        }
        CalulateCRC(buff, 16, &buff[16]);
        status = MFRC522_ToCard(PCD_TRANSCEIVE, buff, 18, buff, &recvBits);
        
		if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A))
        {   
			status = MI_ERR;   
		}
    }
    
    return status;
}

void MFRC522_Halt(void)
{
	uint unLen;
	uchar buff[4]; 

	buff[0] = PICC_HALT;
	buff[1] = 0;
	CalulateCRC(buff, 2, &buff[2]);
 
	MFRC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff,&unLen);
}

uchar MFRC522_ReadCardID(uchar *pCardID)
{
  uchar status;
	
	MFRC522_Halt();
	/* Find cards, return card type */
  status = MFRC522_Request(PICC_REQIDL, pCardID);	
  if (status == MI_OK)
  {
  	
    /* Card detected */
    /* Anti-collision, return card serial number 4 bytes */
  	status = MFRC522_Anticoll(pCardID);
  }
	
	return status;
}

