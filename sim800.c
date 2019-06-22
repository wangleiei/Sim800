#include "sim800.h"
static void NUM_TO_STR(uint16_t a,uint8_t* buf);
static inline enum SIMRET GetCommandRes(SIM800C_BASE*base,uint8_t *Response,uint8_t Responselen, uint16_t Timeoutms);
static enum SIMRET sim800c_sendcommand(SIM800C_BASE*base,uint8_t *Command,uint8_t Commandlen,uint8_t *Response,uint8_t Responselen, uint16_t Timeoutms);
// 0:初始化成功，1初始化中，2：初始化失败
uint8_t sim800c_Init(SIM800C_BASE *base,void (*sendmsg)(uint8_t*,uint16_t),uint16_t (*readmsg)(uint8_t*)){

#define STATIC_ORDER(sta,order,resp)  if(sta != SIM800_SUCCESS){				    \
	    sta = sim800c_sendcommand(base,order,strlen(order),resp,strlen(resp),1000); \
		if((SIM800_IDLE == sta) || (SIM800_BUSY == sta))							\
		{																			\
		   return 1;																\
		}else if(SIM800_FAIL == sta){												\
		 	return 2;																\
		}																		\
	}																				
	// enum SIMRET temp_ret;
	base->sendmsg = sendmsg;	
	base->readmsg = readmsg;		

	// sim卡查询
	STATIC_ORDER(base->cpin_init_sta,"AT+CPIN?\r\n","+CPIN: READY");

	// 移动台类别
	STATIC_ORDER(base->cgclass_init_sta,"AT+CGCLASS=\"B\"\r\n","OK\r\n");

	// PDP上下文
	STATIC_ORDER(base->cgdcont_init_sta,"AT+CGDCONT=1,\"IP\",\"CMNET\"\r\n","OK\r\n");

	// CGATT上下文
	STATIC_ORDER(base->cgatt_init_sta,"AT+CGATT=1\r\n","OK\r\n");

	// 设置短信为文本模式
	STATIC_ORDER(base->msg_textmod_init_sta,"AT+CMGF=1\r\n","OK\r\n");

	// 设置 GSM字符集，
	STATIC_ORDER(base->msg_textfont_init_sta,"AT+CSCS=\"GSM\"\r\n","OK\r\n");

	// AT+CNMI=2,1 新消息提醒
	STATIC_ORDER(base->msg_newmsgremind_init_sta,"AT+CNMI=2,1\r\n","OK\r\n");
	
	// 回显关闭
	STATIC_ORDER(base->ATE_sta,"ATE0\r\n","OK\r\n");
	
	base->errorcount = 0;
	base->warncount = 0;
	base->successcount = 0;
	return 0;

}
void sim800_create_socket(SIM800C_BASE *base){
	base->_connectstart_sta = SIM800_IDLE;	
	base->_cipsend_sta = SIM800_IDLE;	
	base->_cipsended_sta = SIM800_IDLE;	
}
void sim800c_Timecountadd_ms(SIM800C_BASE *base,uint16_t inter_ms){
	time_addms(&base->timecount,inter_ms);
}
// 0:连接成功，1连接中，2：连接超时
uint8_t sim800_connect_socket(SIM800C_BASE *base,uint8_t *ip,uint16_t port,uint16_t waitms){
#define STR_CONNECT_NUM(desbuf,num)  do					\
		{											\
	uint8_t t_buf[10];								\
	memset(t_buf,0,sizeof(t_buf));					\
	NUM_TO_STR(num,t_buf);							\
	strcat(desbuf,t_buf);							\
		} while (0);					

	uint8_t tempbuf[sizeof("AT+CIPSTART=\"TCP\",\"127.000.000.000\",\"12345\"\r\n") + 1];
	enum SIMRET ret = SIM800_IDLE;

	if(base->_connect_sta == SIM800_SUCCESS) return 0;//已经连接成功

	if((SIM800_IDLE == base->_connectstart_sta) || (SIM800_FAIL == base->_connectstart_sta)){
		// 空闲状态和失败状态下调用连接函数时，对数据进行组装
		memset(tempbuf, 0, sizeof(tempbuf)); 
		strcpy(tempbuf, "AT+CIPSTART=\"TCP\",\""); 
		STR_CONNECT_NUM(tempbuf,ip[0]);	
		strcat(tempbuf, "."); 
		STR_CONNECT_NUM(tempbuf,ip[1]);	
		strcat(tempbuf,  "."); 
		STR_CONNECT_NUM(tempbuf,ip[2]);	
		strcat(tempbuf,  "."); 
		STR_CONNECT_NUM(tempbuf,ip[3]);	 

		strcat(tempbuf, "\",\""); 
		STR_CONNECT_NUM(tempbuf,port); 
		strcat(tempbuf, "\"\r\n"); 
	}
	// 调用连接命令，
	if((SIM800_BUSY == base->_connectstart_sta) || (SIM800_IDLE == base->_connectstart_sta)){
		uint8_t response[] = {"OK"};//需要增加
		// ERROR
		// ALREADY CONNECT 判断

		base->_connectstart_sta = sim800c_sendcommand(base,tempbuf,strlen(tempbuf),response,sizeof(response)-1,3000);
	}
	if(SIM800_SUCCESS == base->_connectstart_sta){
		// 命令发送成功，
		// 等待连接成功
		uint8_t response[] = {"CONNECT OK"};
		if((SIM800_BUSY == base->_connect_sta) || (SIM800_IDLE == base->_connect_sta)){
			base->_connect_sta = sim800c_sendcommand(base,tempbuf,0,response,sizeof(response)-1,30000);
		}
		if(SIM800_FAIL == base->_connect_sta){
			return 2;
		}else if(SIM800_SUCCESS == base->_connect_sta){
			return 0;
		}else{
			return 1;
		}

	}else if(SIM800_FAIL == base->_connectstart_sta){
		// 连接命令返回失败
		base->_connect_sta = SIM800_FAIL;
		return 2;
	}

// AT+CNMI=2,1
// OK
// AT+CIPSTART="TCP","112.74.59.250","10002"
// OK

// CONNECT OK
// _D1_200_D2_D3_D4
		

	// return ;
}
// 0:发送成功
// 1：发送中
// 2:发送失败
// 3:无法使用该功能，原因是socket的读写是互斥
uint8_t sim800_write_socket(SIM800C_BASE *base,uint8_t *buf,uint16_t len){
	// if(base->) return 3;
	// socket 必须连接上
	if((SIM800_SUCCESS != base->_connect_sta) || (SIM800_SUCCESS != base->_connectstart_sta)) return 2;

	if((SIM800_IDLE == base->_cipsend_sta) || (SIM800_FAIL == base->_cipsend_sta) || (SIM800_BUSY == base->_cipsend_sta)){
		base->_cipsend_sta = sim800c_sendcommand(base,"AT+CIPSEND\r\n",strlen("AT+CIPSEND\r\n"), ">",1, 3000);
	}
	if(SIM800_BUSY == base->_cipsend_sta){
		// base->_cipsend_sta = SIM800_SUCCESS;
		return 1;
	}else if(SIM800_SUCCESS == base->_cipsend_sta){
		uint8_t buftemp[1] = {0x1a};
		if(SIM800_IDLE == base->_cipsended_sta){
			base->sendmsg(buf,len);
			base->sendmsg(buftemp,1);
		}
		// 需要等待send ok
		base->_cipsended_sta = GetCommandRes(base,"SEND OK",strlen("SEND OK"),10000);
		if(SIM800_SUCCESS == base->_cipsended_sta){
			base->_cipsend_sta = SIM800_IDLE;
			base->_cipsended_sta = SIM800_IDLE;
			return 0;
		}else if(SIM800_FAIL == base->_cipsended_sta){
			base->_cipsend_sta = SIM800_IDLE;
			base->_cipsended_sta = SIM800_IDLE;

			// 
			base->_connect_sta = SIM800_IDLE;
			base->_connectstart_sta = SIM800_IDLE;
			return 2;
		}
		return 1;
	}else{
		// 如果tcp发送失败，那就要
		return 2;
	}
	return 1;
}
// 0：有数据输出
// 1:没有数据输出
// 2:无法使用该功能，原因是socket的读写是互斥
uint8_t sim800_read_socket(SIM800C_BASE *base,uint8_t *buf,uint16_t *len){
	uint16_t msglen = 0;
	if(SIM800_IDLE != base->_cipsend_sta) return 2;

	if((SIM800_SUCCESS != base->_connect_sta) || (SIM800_SUCCESS != base->_connectstart_sta)) return 2;

	msglen = base->readmsg(buf);
	if(msglen){
		*len = msglen;
		return 0;
	}
	return 1;
}
// 0：关闭成功
// 1：关闭中
// 2：无法关闭
uint8_t sim800_close_socket(SIM800C_BASE*base){
	enum SIMRET ret = sim800c_sendcommand(base,"AT+CIPCLOSE=1\r\n",strlen("AT+CIPCLOSE=1\r\n"), "OK\r\n", strlen("OK\r\n"), 2000);
	
	if(SIM800_SUCCESS == ret){
		base->_cipsend_sta = SIM800_IDLE;
		base->_connectstart_sta = SIM800_IDLE;	
		return 0;
	}
	if(SIM800_FAIL == ret){
		return 2;
	}
	return 1;
}
// 0：接受到消息
// 1：没有受到消息
uint8_t sim800c_recmsg(SIM800C_BASE*base,uint8_t*msg,uint8_t *msglen,uint8_t*phonenum){
	// 第一种接受方式：随时可能会有消息进来
	// uint16_t msglen = base->readmsg(base->msgbuf);
	// uint16_t i = 0;

	uint16_t reclen = base->readmsg(base->msgbuf);
	uint16_t i = 0;
	if(reclen > 10){
		// 
		for(i = 0;i < reclen; i++){
			if(((base->msgbuf[i-3] == 'C'))&&(base->msgbuf[i-2] == 'M')&&(base->msgbuf[i-1] == 'G')&&(base->msgbuf[i] == 'R')){
				uint8_t corcount = 0;
				uint8_t j = 0;
				for(j = 2;j < reclen;j++){
					if('+' == base->msgbuf[j]){
						memcpy(phonenum,&base->msgbuf[j+3],11);
						break;
					}
				}
				
				for(i = 0;i < reclen;i++){
					if(base->msgbuf[i] == '"'){
						corcount++;
						if(corcount>=8){
							break;
						}
					}
				}
				if(corcount >= 8){
					memcpy(msg,&base->msgbuf[i+3],reclen-i);
					*msglen = reclen-i;
					return 0;
				}
				return 1;
			}
		}
	}
	return 1;
	// 第二种接受方式：
}
// 命令发送函数，
// 0:成功
// 1：等待返回中
// 2：失败
static enum SIMRET sim800c_sendcommand(SIM800C_BASE*base,uint8_t *Command,uint8_t Commandlen,uint8_t *Response,uint8_t Responselen, uint16_t Timeoutms){
	// uint8_t recbuf[40];//模块返回的数据，有时候会很多，会超过容量，采用base->msgbuf容量更大些
	// uint8_t resbuf[40];
	if (base->_msg_send_sta ==SIM800_IDLE){
		if(Commandlen){
			base->sendmsg(Command,Commandlen);
		}
		base->_msg_send_sta = SIM800_SUCCESS;
	}

	return GetCommandRes(base,Response,Responselen,Timeoutms);
}
static inline enum SIMRET GetCommandRes(SIM800C_BASE*base,uint8_t *Response,uint8_t Responselen, uint16_t Timeoutms){
	uint8_t resbuf[40];
	time_startcount(&base->timecount,Timeoutms);
	if ((0 == Timeoutms) || (time_end(&base->timecount))){//没有到达时间
		// base->readmsg(recbuf);//没有数据

		if ((0 != base->readmsg(base->msgbuf)) )//数据匹配
		{			
			memcpy(resbuf,Response,Responselen);
			resbuf[Responselen] = '\0';
			if(strstr(base->msgbuf, resbuf)){
				time_stop(&base->timecount);
				base->_msg_send_sta = SIM800_IDLE;
				base->successcount++;
				return SIM800_SUCCESS;
			}
		}else{
			return SIM800_BUSY;
		}
	}else{
		base->_msg_send_sta = SIM800_IDLE;
		base->errorcount++;
		return SIM800_FAIL;
	}	
}
static void NUM_TO_STR(uint16_t a,uint8_t* buf){
	uint8_t i = 0,j = 0,k = 0;				
	uint8_t TEMP = 0;						
	uint8_t TEMPBUF[10];					
	do										
	{										
		buf[i] = TEMPBUF[i] = (a%10)+'0';			//a = 255 i =1 a= 25 i = 2 a = 	2 i = 3 a= 0
		i++;
		a /= 10;							
	} while (a);							
	TEMPBUF[i] = 0; 		//				i = 3
	buf[i] = 0; 	//"0070"						
	// tembuf = "543210"  "552\0"
	k = i/2;		//I = 3,K=1					
	for(j = 0;j < k;j++){					
		TEMP = TEMPBUF[i-1-j];				
		buf[j] = TEMP;						
		buf[i-1-j] = TEMPBUF[j];			
	}							       
}
