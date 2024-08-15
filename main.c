#include "stm32f10x.h"
#include "stm32f1_rc522.h"
#include "liquidcrystal_i2c.h"
#include <stdio.h>

#define NUMBER_CARD_ID                 5
#define MAX_SLAVE_CARD                 50
#define FLASH_ADDRESS_SECTOR1          0x8008000U
#define FLASH_ADDRESS_SECTOR2          0x8008800U
#define FLASH_ADDRESS_SECTOR3          0x8009000U
#define SECTOR_SIZE                    2048



typedef enum
{
    ENTER = 	0U,
    EXIT       	,
    NONE      
} PersonStatusType;



typedef enum
{
	DO_NOT_FIND_CARD = 0,
	SLAVE_CARD,
	MASTER_CARD
}CardInfoType;

typedef enum
{
	SECTOR1 	 = 0xFF01,
	SECTOR2 	 = 0xFF02,
	NO_SECTORS = 0xFFFF
}SectorActiveType;

typedef enum
{
    FALSE = 0,
    TRUE = 1
} Boolean;

typedef enum
{
	DO_NOTHING       = 0,
	ADD_SLAVE_CARD      ,
	REMOVE_SLAVE_CARD
}MasterCardIDStateType;

CardInfoType CheckInfoCardID(void);
void I2C_LCD_Configuration(void);
void AddSlaveCard(void);
void RemoveSlaveCard(void);
void OpendDoor(void);
void TIM2_IRQHandler(void);
void Timer2RegisterInterupt(void);
void Flash_Unlock(void);
void Flash_Write(volatile uint32_t u32StartAddr, uint8_t* u8BufferWrite, uint32_t u32Length);
void Flash_Read(volatile uint32_t u32StartAddr, uint8_t* u8BufferRead, uint32_t u32Length);
void Flash_Erase(volatile uint32_t u32StartAddr);
void ReadCardFromFlash(void);
void PWM_Init(void) ;

void GPIO_Config(void);
PersonStatusType GetPersonStatus(void);

static uchar CardID[NUMBER_CARD_ID];
static uchar MasterCardID[NUMBER_CARD_ID] = {0x43, 0x58, 0x44, 0x2D, 0x72};
static uchar SlaveCardID[MAX_SLAVE_CARD][NUMBER_CARD_ID];
static CardInfoType CardIDInfo;
static MasterCardIDStateType CardIDforce = DO_NOTHING;
static MasterCardIDStateType CountStateMasterCard = DO_NOTHING;
static int8_t s8Time = 0;
static uint16_t u32CheckAtive = 0;
static SectorActiveType SectorActive = NO_SECTORS;

static int32_t s32PeopleInRoom = 0;

	
int main(void)
{
	PWM_Init();
	/* Push Sevor to 0*/
	TIM4->CCR4 = 920;
	GPIO_Config();
	I2C_LCD_Configuration();
  HD44780_Init(2);
  HD44780_Clear();
	
	MFRC522_Init();	
	Timer2RegisterInterupt();
	char Str[16];
	
	/* Check if do not have any active card in memory */
	Flash_Read(FLASH_ADDRESS_SECTOR3, (uint8_t *)&u32CheckAtive, 2U);
	if (u32CheckAtive == SECTOR2)
	{
		SectorActive = SECTOR2;		
	}
	else
	{
		SectorActive = SECTOR1;
	}
	
	ReadCardFromFlash();
	
	Delay_Ms(100);
	TIM4->CCR4 = 0;
	
	while(1) 
	{
		CardIDInfo = CheckInfoCardID();
		if ((CardIDInfo == MASTER_CARD) || (CardIDforce != DO_NOTHING))
		{
			CountStateMasterCard++;
			if ((CountStateMasterCard == ADD_SLAVE_CARD) || (CardIDforce == ADD_SLAVE_CARD))
			{
				AddSlaveCard();
			}
			else if ((CountStateMasterCard == REMOVE_SLAVE_CARD) || (CardIDforce == REMOVE_SLAVE_CARD))
			{
				RemoveSlaveCard();
			}
			else
			{
				CountStateMasterCard = DO_NOTHING;
				CardIDforce = DO_NOTHING;
				HD44780_Clear();
				HD44780_SetCursor(0,0);
				HD44780_PrintStr("Welcome To Room");
				HD44780_SetCursor(0,1);
				HD44780_PrintStr(" Put Your Card");
				Delay_Ms(800);
			}
		}
		else if (CardIDInfo == SLAVE_CARD)
		{
			OpendDoor();
		}
		else
		{
			
		}
		
		if ((CountStateMasterCard == DO_NOTHING) && (CardIDforce == DO_NOTHING))
		{
			  HD44780_SetCursor(0,0);
				HD44780_PrintStr("Welcome To Room");
				HD44780_SetCursor(0,1);
				HD44780_PrintStr(" Put Your Card");
		}
		
		/* Auto on/off light*/
		if (ENTER == GetPersonStatus())
		{
			/* Wait until people enters in room */
			while(ENTER == GetPersonStatus())
			{
				/* wait */
			}
			s32PeopleInRoom++;
			HD44780_SetCursor(0,0);
			HD44780_PrintStr("Welcom To Room");
			HD44780_SetCursor(0,1);
			HD44780_PrintStr("Per In Rm: ");
			sprintf(&Str[0], "%d    ", s32PeopleInRoom);
			HD44780_PrintStr(&Str[0]);
			Delay_Ms(2000);
		}
		
		if (EXIT == GetPersonStatus() && (s32PeopleInRoom !=0))
		{
			/* Wait until people exit in room */
			while(EXIT == GetPersonStatus())
			{
				/* wait */
			}
		  s32PeopleInRoom--;
			/* Opend Door */
			TIM4->CCR4 = 1650;
			Delay_Ms(2000);
			HD44780_Clear();
			s8Time = 5;
			/* Counter  = 0*/
			TIM2->CNT = 0U;
			while (s8Time > 0)
			{
				HD44780_SetCursor(0,0);
				HD44780_PrintStr("Good Bye");
				HD44780_SetCursor(0,1);
				HD44780_PrintStr("Per In Rm: ");
				sprintf(&Str[0], "%d    ", s32PeopleInRoom);
				HD44780_PrintStr(&Str[0]);
			}
			TIM4->CCR4 = 920;
			Delay_Ms(500);
			HD44780_Clear();
			TIM4->CCR4 = 0;
			if (s32PeopleInRoom <=0 )
			{
				s32PeopleInRoom = 0;
			}
		}
		
		if (s32PeopleInRoom == 0)
		{
			/* Turn Off light */
			GPIO_ResetBits(GPIOC, GPIO_Pin_13);
			GPIO_ResetBits(GPIOC, GPIO_Pin_14);
			GPIO_ResetBits(GPIOB, GPIO_Pin_8);
			
//			GPIOC->BRR |= (1<<13)|(1<<14);
//		  GPIOB->BRR |= (1<<8);
			
		}
		else
		{
			/* Turn ON light */
			GPIO_SetBits(GPIOC, GPIO_Pin_13);
			GPIO_SetBits(GPIOC, GPIO_Pin_14);
			GPIO_SetBits(GPIOB, GPIO_Pin_8);
			
//			GPIOC->BSRR |= (1<<13)|(1<<14);
//			GPIOB->BSRR |= (1<<8);
		}
	}
}

PersonStatusType GetPersonStatus(void)
{
	PersonStatusType Status = NONE;
	
	if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8) == 0U)
  {
      Delay_Ms(80);
      if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8) == 0U)
      {
          Status = EXIT;
      }
  }
  else if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) == 0U)
  {
      Delay_Ms(80);
      if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) == 0U)
      {
          Status = ENTER;
      }
  }
  else
  {
      Status = NONE;
  }
	
	return Status;
}


void ReadCardFromFlash(void)
{
	uint8_t u8Count = 0;
	
  for (u8Count = 0; u8Count < MAX_SLAVE_CARD; u8Count++)
	{
		if (SectorActive == SECTOR2)
		{
			Flash_Read((FLASH_ADDRESS_SECTOR2 + 8*u8Count), &SlaveCardID[u8Count][0], 5U);
		}
		else
		{
			Flash_Read((FLASH_ADDRESS_SECTOR1 + 8*u8Count), &SlaveCardID[u8Count][0], 5U);
		}
	}
}
void OpendDoor(void)
{
	char Str[16];
	uint8_t u8Count = 0;
	Boolean u8CountExistCard = FALSE;
	
	MFRC522_Halt();
	if (MFRC522_ReadCardID(CardID) == MI_OK)
	{
	/* Scan all card */
		for (u8Count = 0; u8Count < NUMBER_CARD_ID; u8Count++)
		{
			if ((SlaveCardID[u8Count][0] == CardID[0]) && (SlaveCardID[u8Count][1] == CardID[1]) && (SlaveCardID[u8Count][2] == CardID[2]) \
					 		&& (SlaveCardID[u8Count][3] == CardID[3]) && (SlaveCardID[u8Count][4] == CardID[4]) \
					 		)
			{
				u8CountExistCard = TRUE;
			}
		}
		
		if (u8CountExistCard == TRUE)
		{
			HD44780_Clear();
			HD44780_SetCursor(0,0);
			HD44780_PrintStr("Welcome To Room");
			HD44780_SetCursor(0,1);
			HD44780_PrintStr("Door Unlocked ");
			/* Opend Door */
			TIM4->CCR4 = 1650;
			Delay_Ms(2000);
			HD44780_Clear();
			s8Time = 5;
			/* Counter  = 0*/
			TIM2->CNT = 0U;
			while (s8Time > 0)
			{
				HD44780_SetCursor(0,0);
				HD44780_PrintStr("Door Will Close");
				HD44780_SetCursor(0,1);
				HD44780_PrintStr("In ");
				sprintf(&Str[0], "%0.1d ", s8Time);
				HD44780_PrintStr(&Str[0]);
				HD44780_PrintStr("Sec HurryUp");
			}
			TIM4->CCR4 = 920;
			Delay_Ms(500);
			HD44780_Clear();
			TIM4->CCR4 = 0;
		}
		else
		{
			HD44780_Clear();
			HD44780_SetCursor(0,0);
			HD44780_PrintStr("Welcome To Room");
			HD44780_SetCursor(0,1);
			HD44780_PrintStr("Card Not Found! ");
			Delay_Ms(2000);
			HD44780_Clear();
		}
	}
}
void RemoveSlaveCard(void)
{
	uint8_t u8CountMaterID = 0;
	uint8_t u8Count = 0;
	uint8_t u8CountExistCard = 0;
	uint32_t u8Inter = 0;
	
	HD44780_Clear();
	Delay_Ms(200);
	
	while (1)
	{
		HD44780_SetCursor(0,0);
		HD44780_PrintStr("Del Slave Card");
		HD44780_SetCursor(0,1);
		HD44780_PrintStr("Waiting ...");
		/* Clear */
		u8CountMaterID = 0;
		u8CountExistCard = 0;
		
		if (MFRC522_ReadCardID(CardID) == MI_OK)
		{
			for (u8Count = 0; u8Count < NUMBER_CARD_ID; u8Count++)
			{
				if(MasterCardID[u8Count] == CardID[u8Count])
				{
					u8CountMaterID++;
				}
			}
			if (u8CountMaterID != NUMBER_CARD_ID)
			{
				 /* Scan all card if it not already exist */
				for (u8Count = 0; u8Count < NUMBER_CARD_ID; u8Count++)
				{
					 /* Scan all card if it already exist */
					 if ((SlaveCardID[u8Count][0] == CardID[0]) && (SlaveCardID[u8Count][1] == CardID[1]) && (SlaveCardID[u8Count][2] == CardID[2]) \
					 		&& (SlaveCardID[u8Count][3] == CardID[3]) && (SlaveCardID[u8Count][4] == CardID[4]) \
					 		)
					 	{
							/* Remove Card */
							SlaveCardID[u8Count][0] = 0xFF;
							SlaveCardID[u8Count][1] = 0xFF;
							SlaveCardID[u8Count][2] = 0xFF;
							SlaveCardID[u8Count][3] = 0xFF;
							SlaveCardID[u8Count][4] = 0xFF;
							
							/* Remove from flash */
							
								if (SectorActive == SECTOR2)
								{
										SectorActive = SECTOR1;
										Flash_Erase(FLASH_ADDRESS_SECTOR1);
										for (u8Inter = 0; u8Inter < NUMBER_CARD_ID; u8Inter++)
										{
											Flash_Write((FLASH_ADDRESS_SECTOR1 + u8Inter*8), (uint8_t *)&SlaveCardID[u8Inter], 6U);
										}
										Flash_Erase(FLASH_ADDRESS_SECTOR3);
										Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U);
										Flash_Erase(FLASH_ADDRESS_SECTOR2);
								}
								else
								{
									SectorActive = SECTOR2;
									Flash_Erase(FLASH_ADDRESS_SECTOR2);
									for (u8Inter = 0; u8Inter < NUMBER_CARD_ID; u8Inter++)
								  {
										Flash_Write((FLASH_ADDRESS_SECTOR2 + u8Inter*8), (uint8_t *)&SlaveCardID[u8Inter], 6U);
									}
									Flash_Erase(FLASH_ADDRESS_SECTOR3);
								  Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U);
									Flash_Erase(FLASH_ADDRESS_SECTOR1);
								}
							
					 		HD44780_Clear();
					 		HD44780_SetCursor(0,0);
					 		HD44780_PrintStr("Del Slave Card ");
					 		HD44780_SetCursor(0,1);
					 		HD44780_PrintStr("Success!!!");
					 		Delay_Ms(1000);
					 		HD44780_Clear();
							u8CountExistCard++;
							if (u8CountExistCard > 0)
							{
								break;
							}
				   	}
				}
				/* Card not exist and need to add slave card */
				if (u8CountExistCard == 0)
				{
					HD44780_Clear();
					HD44780_SetCursor(0,0);
					HD44780_PrintStr("Add Slave Card ");
					HD44780_SetCursor(0,1);
					HD44780_PrintStr("Not Added Yet");
					Delay_Ms(1000);
					HD44780_Clear();
				}
			}
			else
			{
				CardIDforce = DO_NOTHING;
				break;
			}
		}
		
		if (CardIDforce == DO_NOTHING)
		{
		  Delay_Ms(1000);
			break;
		}
	}
}
void AddSlaveCard(void)
{
	uint8_t u8CountMaterID = 0;
	uint8_t u8Count = 0;
	uint32_t u8Inter = 0;
	uint8_t u8CountSlaveCard = 0;
	uint8_t u8CountExistCard = 0;
	uint8_t ReadCardID[5] = {0};
	
	HD44780_Clear();
	Delay_Ms(200);
	
	while (1)
	{
		HD44780_SetCursor(0,0);
		HD44780_PrintStr("Add Slave Card ");
		HD44780_SetCursor(0,1);
		HD44780_PrintStr("Waiting ...");
		/* Clear */
		u8CountMaterID = 0;
		u8CountExistCard = 0;
		
		if (MFRC522_ReadCardID(CardID) == MI_OK)
		{
			for (u8Count = 0; u8Count < NUMBER_CARD_ID; u8Count++)
			{
				if(MasterCardID[u8Count] == CardID[u8Count])
				{
					u8CountMaterID++;
				}
			}
			if (u8CountMaterID != NUMBER_CARD_ID)
			{
				 /* Scan all card if it already exist */
				for (u8Count = 0; u8Count < NUMBER_CARD_ID; u8Count++)
				{
					 /* Scan all card if it already exist */
					 if ((SlaveCardID[u8Count][0] == CardID[0]) && (SlaveCardID[u8Count][1] == CardID[1]) && (SlaveCardID[u8Count][2] == CardID[2]) \
					 		&& (SlaveCardID[u8Count][3] == CardID[3]) && (SlaveCardID[u8Count][4] == CardID[4]) \
					 		)
					 	{
							
					 		HD44780_Clear();
					 		HD44780_SetCursor(0,0);
					 		HD44780_PrintStr("Add Slave Card ");
					 		HD44780_SetCursor(0,1);
					 		HD44780_PrintStr("Card Was Added");
					 		Delay_Ms(1000);
					 		HD44780_Clear();
							u8CountExistCard++;
							if (u8CountExistCard > 0)
							{
								break;
							}
				   	}
				}
				/* Card not exist and need to add slave card */
				if (u8CountExistCard == 0)
				{
					/* Scan all card */
					for (u8Count = 0; u8Count < NUMBER_CARD_ID; u8Count++)
					{
						if((SlaveCardID[u8Count][0] == 0xFF) && (SlaveCardID[u8Count][1] == 0xFF) && (SlaveCardID[u8Count][2] == 0xFF) && (SlaveCardID[u8Count][3] == 0xFF) && (SlaveCardID[u8Count][4] == 0xFF) )
						{
								for (u8Inter = 0; u8Inter < NUMBER_CARD_ID; u8Inter++)
								{
									SlaveCardID[u8Count][u8Inter] = CardID[u8Inter];
								}
								
								for (u8Inter = 0; u8Inter < SECTOR_SIZE; u8Inter++)
								{
									if (SectorActive == SECTOR2)
									{
										Flash_Read((FLASH_ADDRESS_SECTOR2 + u8Inter*8), &ReadCardID[0], 5U);
										if ((ReadCardID[0] == 0xFF) && (ReadCardID[1] == 0xFF) && (ReadCardID[2] == 0xFF) && (ReadCardID[3] == 0xFF) && (ReadCardID[4] == 0xFF))
										{
											Flash_Write((FLASH_ADDRESS_SECTOR2 + u8Inter*8), (uint8_t *)&CardID[0], 8U);
											Flash_Erase(FLASH_ADDRESS_SECTOR3);
											Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U);
											break;
										}
									}
									else
									{
										Flash_Read((FLASH_ADDRESS_SECTOR1 + u8Inter*8), (uint8_t *)&ReadCardID[0], 5U);
										if ((ReadCardID[0] == 0xFF) && (ReadCardID[1] == 0xFF) && (ReadCardID[2] == 0xFF) && (ReadCardID[3] == 0xFF) && (ReadCardID[4] == 0xFF))
										{
											Flash_Write((FLASH_ADDRESS_SECTOR1 + u8Inter*8), (uint8_t *)&CardID[0], 8U);
											Flash_Erase(FLASH_ADDRESS_SECTOR3);
											Flash_Write(FLASH_ADDRESS_SECTOR3, (uint8_t *)&SectorActive, 2U);
											break;
										}
									}
								}
								
								HD44780_Clear();
								HD44780_SetCursor(0,0);
								HD44780_PrintStr("Add Slave Card ");
								HD44780_SetCursor(0,1);
								HD44780_PrintStr("Success!!!");
								Delay_Ms(1000);
								HD44780_Clear();
								break;
						}
						else
						{
								u8CountSlaveCard++;
						}
					}
					if (u8CountSlaveCard == NUMBER_CARD_ID)
					{
						/* Slave FULL */
						HD44780_Clear();
						HD44780_SetCursor(0,0);
						HD44780_PrintStr("Add Slave Card ");
						HD44780_SetCursor(0,1);
						HD44780_PrintStr("Failure!!!");
						Delay_Ms(1000);
						HD44780_Clear();
					}
				}
			}
			else
			{
				CardIDforce = REMOVE_SLAVE_CARD;
				break;
			}
		}
		
		if (CardIDforce == REMOVE_SLAVE_CARD)
		{
		  Delay_Ms(1000);
			break;
		}
	}
}
CardInfoType CheckInfoCardID(void)
{
	uint8_t u8Count = 0;
	CardInfoType CardInfo = DO_NOT_FIND_CARD;
	uint8_t u8CountMaterCard = 0;
	
	if (MFRC522_ReadCardID(CardID) == MI_OK)
	{
		/* Find Master Card */
		for (u8Count = 0; u8Count < NUMBER_CARD_ID; u8Count++)
		{
			if(MasterCardID[u8Count] == CardID[u8Count])
			{
				u8CountMaterCard++;
			}
		}
		/* Already found matest card */
		if (u8CountMaterCard == NUMBER_CARD_ID)
		{
			CardInfo = MASTER_CARD;
		}
		else
		{
			CardInfo = SLAVE_CARD;
		}
	}
	
	return CardInfo;
}

void I2C_LCD_Configuration(void)
{
	  GPIO_InitTypeDef  GPIO_InitStructure;
    I2C_InitTypeDef  I2C_InitStructure;
		
	  /* PortB clock enable */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	  /* I2C1 clock enable */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    GPIO_StructInit(&GPIO_InitStructure);
	
    /* SCL (PB6) */
	  /* SDA (PB7) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

		/* Configure I2Cx */
    I2C_StructInit(&I2C_InitStructure);
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 100000;

    I2C_Init(I2C_Chanel, &I2C_InitStructure);
    I2C_Cmd(I2C_Chanel, ENABLE); 
}
/*
 Clock Time 2 = 10000HZ -> 1 ticket = 1/10000s
                           10000 ticet = 1s
*/                     
void Timer2RegisterInterupt(void)
{
	/* Enable clock for timer2*/
	RCC->APB1ENR |= 0x01;
/*	RCC->APB1ENR |= (1 << (1 * 0)); */
	
	/* IMx auto-reload register (TIMx_ARR) */
	TIM2->ARR = 10000 - 1;
	/* Counter  = 0*/
	TIM2->CNT = 0U;
	/* PSC[15:0]: Prescaler value */
	TIM2->PSC = 7200 - 1;
	/* Bit 0 UIE: Update interrupt enable */
	TIM2->DIER = 0x01;
/*	TIM2->DIER |= (1<<(1*0));*/
	
	/* Clear interrupt flag */
	TIM2->SR &= ~(0x01);
	/* Bit 0 CEN: Counter enable and  0: Counter used as upcounter*/
	TIM2->CR1 = 0x01;
	/* Generate an update event to reload the Prescaler and the Repetition counter values immediately */
	TIM2->EGR = 0x01;
	/* 6 settable EXTI0 Line0 interrupt*/
	NVIC->ISER[0] = 0x10000000;
	
	NVIC->IP[28] = 0x4U << 4U;
}
void TIM2_IRQHandler(void)
{
	if (((TIM2->SR & 0x01U) != 0U) && (((TIM2->DIER)&0x01U) == 0x01U))
	{
		TIM2->SR &= ~(0x01U);
		s8Time --;
	}
}	
/**********************************************************************************************************************
*                                              FLASH
***********************************************************************************************************************/
void Flash_Unlock(void)
{
    /* This sequence consists of two write cycles, where two key values (KEY1 and KEY2) are written to the FLASH_KEYR address*/
    FLASH->KEYR = 0x45670123;
    FLASH->KEYR = 0xCDEF89AB;
}

void Flash_Erase(volatile uint32_t u32StartAddr)
{
    /* Check that no Flash memory operation is ongoing by checking the BSY bit in the FLASH_CR register */
    while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY);

    /* Check unlock sequences */
    if ((FLASH->CR&FLASH_CR_LOCK) == FLASH_CR_LOCK )
    {
        Flash_Unlock();
    }

    /* Set the PER bit in the FLASH_CR register */
    FLASH->CR |= FLASH_CR_PER;
    /* Program the FLASH_AR register to select a page to erase */
    FLASH->AR = u32StartAddr;
    /* Set the STRT bit in the FLASH_CR register */
    FLASH->CR |= FLASH_CR_STRT;
    /* Wait for the BSY bit to be reset */
    while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY);
    /* Clear PER bit in the FLASH_CR register */
    FLASH->CR &= ~(uint32_t)FLASH_CR_PER;
    /* Clear STRT bit in the FLASH_CR register */
    FLASH->CR &= ~(uint32_t)FLASH_CR_STRT;

}
void Flash_Write(volatile uint32_t u32StartAddr, uint8_t* u8BufferWrite, uint32_t u32Length)
{
    uint32_t u32Count = 0U;

    /* Check input paras */
    if((u8BufferWrite == 0x00U) || (u32Length == 0U) || u32Length%2U != 0U)
    {
        return;
    }
    /* Check that no Flash memory operation is ongoing by checking the BSY bit in the FLASH_CR register */
    while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY)
    {
        /*  Wating for Bsy bit */
    }
    /* Check unlock sequences */
    if ((FLASH->CR&FLASH_CR_LOCK) == FLASH_CR_LOCK )
    {
        Flash_Unlock();
    }

    /* Write FLASH_CR_PG to 1 */
    FLASH->CR |= FLASH_CR_PG;

    /* Perform half-word write at the desired address*/
    for(u32Count = 0U; u32Count < (u32Length/2); u32Count ++ )
    {
        *(uint16_t*)(u32StartAddr + u32Count*2U) = *(uint16_t*)((uint32_t)u8BufferWrite + u32Count*2U);
        /* Wait for the BSY bit to be reset */
        while((FLASH->SR&FLASH_SR_BSY) == FLASH_SR_BSY);
    }

    /* Clear PG bit in the FLASH_CR register */
    FLASH->CR &= ~(uint32_t)FLASH_CR_PG;

}
void Flash_Read(volatile uint32_t u32StartAddr, uint8_t* u8BufferRead, uint32_t u32Length)
{

    /* Check input paras */
    if((u8BufferRead == 0x00U) || (u32Length == 0U))
    {
        return;
    }
    do
    {
        if(( u32StartAddr%4U == 0U) && ((uint32_t)u8BufferRead%4U == 0U) && (u32Length >= 4U))
        {
            *(uint32_t*)((uint32_t)u8BufferRead) = *(uint32_t*)(u32StartAddr);
            u8BufferRead = u8BufferRead + 4U;
            u32StartAddr = u32StartAddr + 4U;
            u32Length = u32Length - 4U;
        }
        else if(( u32StartAddr%2U == 0U) && ((uint32_t)u8BufferRead%2U == 0U) && (u32Length >= 2U))
        {
            *(uint16_t*)((uint32_t)u8BufferRead) = *(uint16_t*)(u32StartAddr);
            u8BufferRead = u8BufferRead + 2U;
            u32StartAddr = u32StartAddr + 2U;
            u32Length = u32Length - 2U;
        }
        else
        {
            *(uint8_t*)(u8BufferRead) = *(uint8_t*)(u32StartAddr);
            u8BufferRead = u8BufferRead + 1U;
            u32StartAddr = u32StartAddr + 1U;
            u32Length = u32Length - 1U;
        }
    }
    while(u32Length > 0U);
}	
void PWM_Init(void) 
{
	 /* Initialization struct */
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
	TIM_OCInitTypeDef TIM_OCInitStruct;
	GPIO_InitTypeDef GPIO_InitStruct;
	
	/* Step 1: Initialize TIM4 */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
/*	RCC->APB1ENR |= (1<<2);*/
	
	/* Create 50Hz PWM */
	/* Prescale timer clock from 72MHz to 720kHz by prescaler = 100 */
	TIM_TimeBaseInitStruct.TIM_Prescaler = 100;
	/*TIM4->PSC = 100;*/
	
	/* TIM_Period = (timer_clock / PWM_frequency) - 1 */
	/* TIM_Period = (720kHz / 50Hz) - 1 = 14399  */
	TIM_TimeBaseInitStruct.TIM_Period = 14399;
	/*TIM4->ARR = 14399;*/
	TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	/*TIM4->CR1 = 0x0000;*/
	TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
	/*TIM4->CR1 &= ~(1<<4);*/
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStruct);

	TIM_Cmd(TIM4, ENABLE);
	TIM4->CR1 |= (1<<0);
	
	TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
	/*TIM4->CCMR2 = 0x0060;
	TIM4->CCER |= (1<<12);*/
	TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStruct.TIM_Pulse = 0;
	/*TIM4->CCR4 = 0;*/
	TIM_OC4Init(TIM4, &TIM_OCInitStruct);
	TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);
	/* TIM4->CCMR2 |= (1<<11);*/
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStruct);
	/*RCC->APB2ENR |= (1<<3);
	GPIOB->CRH |= (0xA<<(4*1));*/
	
}

void GPIO_Config(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOA, ENABLE);
		/*RCC->APB2ENR |= (1<<2)|(1<<3)|(1<<4);*/
	
	  /* Note: A3 -> Enter */
		/* Note: A8 -> Exit */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3|GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
	  
/*		GPIOA->CRL |= (0x8<<(4*3));
	  GPIOA->CRH |= (0x8<<(4*0));*/

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13|GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
		
/*		GPIOC->CRH |= (0x3<<20)|(0x3<<24);*/
	
	  
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
		
/*		GPIOB->CRH |= (0x3<<0);*/
		
    /* Turn Off light */
    GPIO_ResetBits(GPIOC, GPIO_Pin_13);
		GPIO_ResetBits(GPIOC, GPIO_Pin_14);
		GPIO_ResetBits(GPIOB, GPIO_Pin_8);
		
/*		GPIOC->BRR |= (1<<13)|(1<<14);
  		GPIOB->BRR |= (1<<8);*/
		
}
