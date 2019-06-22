#ifndef  __SIM800
#define __SIM800

#include <stdint.h>
#include <string.h>
#include "time_count.h"
// #include "pch.h"

enum SIMRET{SIM800_IDLE = 0,SIM800_BUSY = 1,SIM800_FAIL = 2,SIM800_SUCCESS = 3};

typedef struct SIM800C_BASE
{
	void (*sendmsg)(uint8_t*,uint16_t);
	uint16_t (*readmsg)(uint8_t*);
	TimeCount timecount;

	enum SIMRET ATE_sta;

	enum SIMRET _msg_send_sta;

	enum SIMRET cpin_init_sta;
	enum SIMRET cgclass_init_sta;
	enum SIMRET cgdcont_init_sta;
	enum SIMRET cgatt_init_sta;

	enum SIMRET msg_textfont_init_sta;
	enum SIMRET msg_textmod_init_sta;
	enum SIMRET msg_newmsgremind_init_sta;

	enum SIMRET _connectstart_sta;//tcp连接状态，开始连接状态，发送连接指令
	enum SIMRET _connect_sta;//tcp连接状态，连接结果状态，可能失败
	enum SIMRET _cipsend_sta;//tcp发送数据状态，表示可以发送数据到sim800
	enum SIMRET _cipsended_sta;//tcp发送数据完成状态，表示数据是否到达目标IP
	uint8_t msgbuf[100];

	uint16_t errorcount;
	uint16_t warncount;
	uint16_t successcount;
}SIM800C_BASE;


void sim800_create_socket(SIM800C_BASE *base);
void sim800c_Timecountadd_ms(SIM800C_BASE *base,uint16_t inter_ms);
uint8_t sim800_connect_socket(SIM800C_BASE *base,uint8_t *ip,uint16_t port,uint16_t waitms);
uint8_t sim800_write_socket(SIM800C_BASE *base,uint8_t *buf,uint16_t len);
uint8_t sim800_read_socket(SIM800C_BASE *base,uint8_t *buf,uint16_t *len);
uint8_t sim800_close_socket(SIM800C_BASE*base);
uint8_t sim800c_recmsg(SIM800C_BASE*base,uint8_t*msg,uint8_t *msglen,uint8_t*phonenum);
uint8_t sim800c_Init(SIM800C_BASE *base, void(*sendmsg)(uint8_t*, uint16_t), uint16_t(*readmsg)(uint8_t*));


#endif
