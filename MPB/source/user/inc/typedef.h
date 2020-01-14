#ifndef __TYPEDEF_H
#define __TYPEDEF_H

#include "stm32f10x_type.h"

#define BUF_LENGTH      300
#define offsetof(T, member)          (size_t((&((T *)0)->member)))
//General Struct.
//typedef unsigned long long u64;

typedef struct
{
    u8 Second;
    u8 Minute;
    u8 Hour;
}TimeType;      // hex struct

typedef struct
{
    u8 Day;
    u8 Month;
    u8 Year;
}DateType;      // hex struct

typedef struct {
    TimeType Time;
    DateType Date;
}LongTimeType;

typedef struct
{
    u16 ptr;
    u16 len;
    u8  buf[BUF_LENGTH];
} tCombuf;      // communication buffer

typedef union {
    struct {
        u16 MsgProc     : 1;
        u16 AutoProgMcu : 1;
        u16 EraseMcu    : 1;
        u16 ProgMcu     : 1;
        u16 VerifyMcu   : 1;
        u16 Second      : 1;
        u16 CalRTC      : 1;
    }Bits;
    u16 Value;
}EventStruct;

typedef struct
{
    u8  status;
    u16 ptr;
    u16 FrameLen;
    u8  FrameChksum;
    u8  Timeout;
    u8  buf[BUF_LENGTH];
} tTransceiverFSM;

//extern u32 TmpTime;
extern u32 SysTime;
extern u32 DowTime;
#endif

