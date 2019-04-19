#include "main.h"
const char * SENS_TYPE_STR[] = {"SEN_TEMP_HUMI", "SEN_ILLUM_T_H", "SEN_CO2_T_H", "SEN_COND_SALT"};
const uint8_t SENS_RESP_SAMPLE[6][13] = {{0x01,0x03,0x08,0x02,0x89,0x00,0xF6,0x00,0x00,0x00,0x00,0xC4,0xD3},\
																			{0x02,0x03,0x06,0x02,0x4F,0x01,0x1C,0x02,0xB2,0x20,0x86},\
																			{0x03,0x03,0x04,0x00,0x00,0x00,0xEE,0x59,0xBF},\
																			{0x04,0x03,0x04,0x00,0x00,0x00,0xEF,0xEE,0xBF},\
																			{0x05,0x03,0x04,0x00,0x07,0x00,0xF3,0x4E,0x77},\
																			{0x0A,0x01,0x04,0x00,0x00,0x00,0x00,0x41,0x11}};
/*发送传感器Modbus命令*/
BOOL_t Modbus_Send_Cmd(Sensor_Handle_t * hs)
{
	RS485_TX_EN();
	if (HAL_UART_Transmit(&huart2, hs->modbus_cmd->cmdbytes, hs->mb_cmdsize, MODBUS_CMD_TIMEOUT)) {
		RS485_RX_EN();
		return TRUE;
	}
	RS485_RX_EN();
	return FALSE;
}
/*接收传感器Modbus响应*/
BOOL_t Modbus_Receive_Resp(Sensor_Handle_t * hs)
{
#if (SENS_SIM == 1)
	memcpy((void *)(hs->modbus_resp->respbytes), (const void *)(SENS_RESP_SAMPLE[hs->type]), (unsigned int)hs->mb_respsize);
	hs->mb_resp_new = TRUE;
	return TRUE;
#else
	if (HAL_UART_Receive(&huart2, hs->modbus_resp->respbytes, hs->mb_respsize, MODBUS_RESP_TIMEOUT)) {
		hs->mb_resp_new = TRUE;
		return TRUE;
	}
	hs->mb_resp_new = FALSE;
	return FALSE;
#endif
}

//传感器数据解析
BOOL_t Modbus_Data_Proc(Sensor_Handle_t * hs, void * result)
{
	if (hs->mb_resp_new == FALSE)
	{
		return FALSE;
	}
	if (crc16_calc(hs->modbus_resp->respbytes, hs->mb_respsize))
	{
		return FALSE;
	}
	switch (hs->type)
	{
		case SEN_TEMP_HUMI:
			rbytes((uint8_t *)&(hs->modbus_resp->resp_ht.humidity), sizeof(hs->modbus_resp->resp_ht.humidity));
			rbytes((uint8_t *)&(hs->modbus_resp->resp_ht.temperature), sizeof(hs->modbus_resp->resp_ht.temperature));
			printf("@%s,%.1f,%.1f", SENS_TYPE_STR[hs->type], (double)(hs->modbus_resp->resp_ht.humidity) / 10, (double)(hs->modbus_resp->resp_ht.temperature) / 10);
		case SEN_ILLUM_T_H:
			rbytes((uint8_t *)&(hs->modbus_resp->resp_iht.humidity), sizeof(hs->modbus_resp->resp_iht.humidity));
			rbytes((uint8_t *)&(hs->modbus_resp->resp_iht.temperature), sizeof(hs->modbus_resp->resp_iht.temperature));
			rbytes((uint8_t *)&(hs->modbus_resp->resp_iht.illuminance), sizeof(hs->modbus_resp->resp_iht.illuminance));
			printf("@%s,%.1f,%.1f,%d", SENS_TYPE_STR[hs->type], (double)(hs->modbus_resp->resp_iht.humidity) / 10, (double)(hs->modbus_resp->resp_iht.temperature) / 10, hs->modbus_resp->resp_iht.illuminance);
		case SEN_CO2_T_H:
			rbytes((uint8_t *)&(hs->modbus_resp->resp_co2ht.humidity), sizeof(hs->modbus_resp->resp_co2ht.humidity));
			rbytes((uint8_t *)&(hs->modbus_resp->resp_co2ht.temperature), sizeof(hs->modbus_resp->resp_co2ht.temperature));
			rbytes((uint8_t *)&(hs->modbus_resp->resp_co2ht.co2), sizeof(hs->modbus_resp->resp_co2ht.co2));
			printf("@%s,%.1f,%.1f,%d", SENS_TYPE_STR[hs->type], (double)(hs->modbus_resp->resp_co2ht.humidity) / 10, (double)(hs->modbus_resp->resp_co2ht.temperature) / 10, hs->modbus_resp->resp_co2ht.co2);
		case SEN_COND_SALT:
			rbytes((uint8_t *)&(hs->modbus_resp->resp_cs.cond), sizeof(hs->modbus_resp->resp_cs.cond));
			rbytes((uint8_t *)&(hs->modbus_resp->resp_cs.salt), sizeof(hs->modbus_resp->resp_cs.salt));
			printf("@%s,%d,%d", SENS_TYPE_STR[hs->type], hs->modbus_resp->resp_cs.cond, hs->modbus_resp->resp_cs.salt);
	}
	return TRUE;
}

/*解析传感器数据，以指针形式返回解析结果*/
BOOL_t Sens_Data_Proc(Sensor_Handle_t * hs)
{
	return TRUE;
}

void Sensors_Polling(PtrQue_TypeDef * sq)
{
	int i;
	Sensor_Handle_t * hs;
	for (i = 0; i < __PTRQUE_COUNT(sq); i++)
	{
		if (PtrQue_Query(sq, (void **)&hs))
		{
			if (Modbus_Send_Cmd(hs)) {
				Modbus_Receive_Resp(hs);
			}
		}
	}
}

#if (DEBUG_ON == 1)
void Sensors_Que_Print(PtrQue_TypeDef * sq)
{
	int i, j;
	Sensor_Handle_t * sh;
	printf("\r\n ######## sensors que ########\r\n");
	for (i = 0; i < __PTRQUE_COUNT(sq); i++)
	{
		printf("***** Sensor %d *****\r\n", i + 1);
		PtrQue_Query(sq, (void **)&sh);
		printf("type: %s\r\n", SENS_TYPE_STR[sh->type]);
		printf("modbus command: ");
		for (j = 0; j < sh->mb_cmdsize; j++) {
			printf("0x%x ", sh->modbus_cmd->cmdbytes[j]);
		}
		printf("\r\n");
		printf("modbus response: ");
		for (j = 0; j < sh->mb_respsize; j++) {
			printf("0x%x ", sh->modbus_resp->respbytes[j]);
		}
		printf("\r\n");
	}
	printf("\r\n #############################\r\n");
}
#endif

void Sensors_Que_Init(PtrQue_TypeDef * sq)
{
	Sensor_Handle_t * sh;
	ModBus_Cmd_Union_t * mbcmd;
	ModBus_Resp_Union_t * mbresp;
	int i;
	for (i = 0; i < PTRQUESIZE; i++) //初始化传感器队列，并完成传感器句柄部分初始化
	{
		sh = (Sensor_Handle_t *)malloc(sizeof(Sensor_Handle_t));
		mbcmd = (ModBus_Cmd_Union_t *)malloc(sizeof(ModBus_Cmd_Union_t));
		mbresp = (ModBus_Resp_Union_t *)malloc(sizeof(ModBus_Resp_Union_t));
		sh->modbus_cmd = mbcmd;
		sh->modbus_resp = mbresp;
		PtrQue_In(sq, (void *)sh);
	}
	if (PtrQue_Query(sq, (void **)&sh)) //对传感器队列中的光照温湿度传感器继续完成初始化
	{
		sh->type = SEN_ILLUM_T_H;
		sh->mb_cmdsize = sizeof(ModBus_Cmd_t);
		sh->modbus_cmd->cmd.rs485addr = 0x01;
		sh->modbus_cmd->cmd.funcode = 0x03;
		sh->modbus_cmd->cmd.regaddr =0x0000u;
		sh->modbus_cmd->cmd.regcount = 0x0004u;
		rbytes((uint8_t *)&(sh->modbus_cmd->cmd.regcount), sizeof(sh->modbus_cmd->cmd.regcount));
		sh->modbus_cmd->cmd.crc16 = crc16_calc(sh->modbus_cmd->cmdbytes, sh->mb_cmdsize - 2);
		sh->mb_respsize = sizeof(ModBus_Resp_IHT_t);
	}
	if (PtrQue_Query(sq, (void **)&sh)) //对传感器队列中的CO2温湿度传感器继续完成初始化
	{
		sh->type = SEN_CO2_T_H;
		sh->mb_cmdsize = sizeof(ModBus_Cmd_t);
		sh->modbus_cmd->cmd.rs485addr = 0x02;
		sh->modbus_cmd->cmd.funcode = 0x03;
		sh->modbus_cmd->cmd.regaddr = 0x0000u;
		sh->modbus_cmd->cmd.regcount = 0x0003u;
		rbytes((uint8_t *)&(sh->modbus_cmd->cmd.regcount), sizeof(sh->modbus_cmd->cmd.regcount));
		sh->modbus_cmd->cmd.crc16 = crc16_calc(sh->modbus_cmd->cmdbytes, sh->mb_cmdsize - 2);
		sh->mb_respsize = sizeof(ModBus_Resp_CO2HT_t);
	}
	if (PtrQue_Query(sq, (void **)&sh)) //对传感器队列中的土壤温湿度传感器继续完成初始化
	{
		sh->type = SEN_TEMP_HUMI;
		sh->mb_cmdsize = sizeof(ModBus_Cmd_t);
		sh->modbus_cmd->cmd.rs485addr = 0x03;
		sh->modbus_cmd->cmd.funcode = 0x03;
		sh->modbus_cmd->cmd.regaddr = 0x0000u;
		sh->modbus_cmd->cmd.regcount = 0x0002u;
		rbytes((uint8_t *)&(sh->modbus_cmd->cmd.regcount), sizeof(sh->modbus_cmd->cmd.regcount));
		sh->modbus_cmd->cmd.crc16 = crc16_calc(sh->modbus_cmd->cmdbytes, sh->mb_cmdsize - 2);
		sh->mb_respsize = sizeof(ModBus_Resp_HT_t);
	}
	if (PtrQue_Query(sq, (void **)&sh)) //对传感器队列中的土壤温湿度传感器继续完成初始化
	{
		sh->type = SEN_TEMP_HUMI;
		sh->mb_cmdsize = sizeof(ModBus_Cmd_t);
		sh->modbus_cmd->cmd.rs485addr = 0x04;
		sh->modbus_cmd->cmd.funcode = 0x03;
		sh->modbus_cmd->cmd.regaddr = 0x0000u;
		sh->modbus_cmd->cmd.regcount = 0x0002u;
		rbytes((uint8_t *)&(sh->modbus_cmd->cmd.regcount), sizeof(sh->modbus_cmd->cmd.regcount));
		sh->modbus_cmd->cmd.crc16 = crc16_calc(sh->modbus_cmd->cmdbytes, sh->mb_cmdsize - 2);
		sh->mb_respsize = sizeof(ModBus_Resp_HT_t);
	}
	if (PtrQue_Query(sq, (void **)&sh)) //对传感器队列中的土壤温湿度传感器继续完成初始化
	{
		sh->type = SEN_TEMP_HUMI;
		sh->mb_cmdsize = sizeof(ModBus_Cmd_t);
		sh->modbus_cmd->cmd.rs485addr = 0x05;
		sh->modbus_cmd->cmd.funcode = 0x03;
		sh->modbus_cmd->cmd.regaddr = 0x0000u;
		sh->modbus_cmd->cmd.regcount = 0x0002u;
		rbytes((uint8_t *)&(sh->modbus_cmd->cmd.regcount), sizeof(sh->modbus_cmd->cmd.regcount));
		sh->modbus_cmd->cmd.crc16 = crc16_calc(sh->modbus_cmd->cmdbytes, sh->mb_cmdsize - 2);
		sh->mb_respsize = sizeof(ModBus_Resp_HT_t);
	}
	if (PtrQue_Query(sq, (void **)&sh)) //对传感器队列中的电导率盐分传感器继续完成初始化
	{
		sh->type = SEN_COND_SALT;
		sh->mb_cmdsize = sizeof(ModBus_Cmd_Ext_t);
		sh->modbus_cmd->cmd_ext.rs485addr = 0x0a;
		sh->modbus_cmd->cmd_ext.funcode = 0x01;
		sh->modbus_cmd->cmd_ext.datasize = 0x04;
		sh->modbus_cmd->cmd_ext.regaddr = 0x0000u;
		sh->modbus_cmd->cmd_ext.regcount = 0x0002u;
		rbytes((uint8_t *)&(sh->modbus_cmd->cmd.regcount), sizeof(sh->modbus_cmd->cmd.regcount));
		sh->modbus_cmd->cmd_ext.crc16 = crc16_calc(sh->modbus_cmd->cmdbytes, sh->mb_cmdsize - 2);
		sh->mb_respsize = sizeof(ModBus_Resp_CS_t);
	}
//#if (DEBUG_ON == 1)
//	Sensors_Que_Print(sq);
//#endif
}
