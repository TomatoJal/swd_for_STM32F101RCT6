#ifndef __SWD_APP_H__
#define __SWD_APP_H__
#include "stm32f10x_lib.h"


extern uint8   SWD_ReadByte(uint32 addr, uint8 *val);
extern uint8   SWD_WriteByte(uint32 addr, uint8 val);

extern uint8   SWD_ReadHalfWord(uint32 adr, uint32 *val);
extern uint8   SWD_WriteHalfWord(uint32 adr, uint32 val);

extern uint8   SWD_ReadWord(uint32 adr, uint32 *val);
extern uint8   SWD_WriteWord(uint32 adr, uint32 val);

extern uint8   SWD_ReadData(uint32 adr, uint32 *val); 
extern uint8   SWD_WriteData(uint32 adr, uint32 val);

extern uint8   SWD_ReadBlock(uint32 address, uint8 *data, uint32 size);
extern uint8   SWD_WriteBlock(uint32 address, uint8 *data, uint32 size);

extern uint8   SWD_ReadMemoryArray(uint32 address, uint8 *data, uint32 size);
extern uint8   SWD_WriteMemoryArray(uint32 address, uint8 *data, uint32 size);

extern uint8   SWD_ReadCoreRegister(uint32 registerID, uint32 *val);//¶ÁÄÚºË¼Ä´æÆ÷
extern uint8   SWD_WriteCoreRegister(uint32 registerID, uint32 val);//Ð´ÄÚºË¼Ä´æÆ÷


extern uint8   SWD_Init(void);
extern uint8   SWD_CoreReset(void);
extern uint8   SWD_CoreStop(void);
extern uint8   SWD_CoreStart(void);
#endif


