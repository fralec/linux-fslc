/* ------------------------------------------------------------------------- */
/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.							     */
/* ------------------------------------------------------------------------- */

#include "ilitek_ts.h"
#include <linux/firmware.h>
#include <linux/vmalloc.h>

#ifdef ILITEK_UPDATE_FW
#if !ILITEK_UPGRADE_WITH_BIN
#include "ilitek_fw.h"
#endif
#endif

unsigned int UpdateCRC(unsigned int crc, unsigned char newbyte)
{
   char i;                                  // loop counter
   #define CRC_POLY 0x8408      // CRC16-CCITT FCS (X^16+X^12+X^5+1)

   crc = crc ^ newbyte;

   for (i = 0; i < 8; i++)
   {
      if (crc & 0x01)
      {
         crc = crc >> 1;
         crc ^= CRC_POLY;
      }
      else
      {
         crc = crc >> 1;
      }
   }
   return crc;
}

int check_busy(int delay)
{
	int i;
	unsigned char buf[2];
	for(i = 0; i < 1000; i ++){
		buf[0] = 0x80;
		if(ilitek_i2c_write_and_read(buf, 1, delay, buf, 1) < 0) {
			return ILITEK_I2C_TRANSFER_ERR;
		}
		if(buf[0] == 0x50) {
			//tp_log_info("check_busy i = %d\n", i);
			return 0;
		}
	}
	tp_log_info("check_busy error\n");
	return -1;
}
int ilitek_changetoblmode(bool mode) {
	int ret = 0, i = 0;
	unsigned char buf[8];
	buf[0] = 0xc0;
	ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 2);
	if(ret < 0)
	{
		tp_log_err("ilitek_i2c_write_and_read\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}		
	msleep(30);
	tp_log_info("ilitek ic. mode =%d , it's %s \n",buf[0],((buf[0] == 0x5A)?"AP MODE":((buf[0] == ILITEK_TP_MODE_BOOTLOADER) ? "BL MODE" : "UNKNOW MODE")));
	if ((buf[0] == ILITEK_TP_MODE_APPLICATION && !mode) || (buf[0] == ILITEK_TP_MODE_BOOTLOADER && mode)) {
		if (mode) {
			tp_log_info("ilitek change to BL mode ok already BL mode\n");
		}
		else {
			tp_log_info("ilitek change to AP mode ok already AP mode\n");
		}
	}
	else {
		for (i = 0; i < 5; i++) {
			buf[0] = ILITEK_TP_CMD_WRITE_ENABLE;
			buf[1] = 0x5A;
			buf[2] = 0xA5;
			ret = ilitek_i2c_write(buf, 3);
			if(ret < 0)
			{
				tp_log_err("ilitek_i2c_write_and_read\n");
				return ILITEK_I2C_TRANSFER_ERR;
			}		
			msleep(20);
			if (mode) {
				buf[0] = 0xc2;
			}
			else {
				buf[0] = 0xc1;
			}
			ret = ilitek_i2c_write(buf, 1);
			if(ret < 0)
			{
				tp_log_err("ilitek_i2c_write ERR\n");
				return ILITEK_I2C_TRANSFER_ERR;
			}		
			msleep(200);
			buf[0] = 0xc0;
			ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 2);
			if(ret < 0)
			{
				tp_log_err("ilitek_i2c_write_and_read\n");
				return ILITEK_I2C_TRANSFER_ERR;
			}		
			msleep(30);
			tp_log_info("ilitek ic. mode =%d , it's  %s \n",buf[0],((buf[0] == ILITEK_TP_MODE_APPLICATION) ? "AP MODE" : ((buf[0] == ILITEK_TP_MODE_BOOTLOADER) ? "BL MODE" : "UNKNOW MODE")));
			if ((buf[0] == ILITEK_TP_MODE_APPLICATION && !mode) || (buf[0] == ILITEK_TP_MODE_BOOTLOADER && mode)) {
				if (mode) {
					tp_log_info("ilitek change to BL mode ok\n");
				}
				else {
					tp_log_info("ilitek change to AP mode ok\n");
				}
				break;
			}
		}
	}
	if (i >= 5) {
		if (mode) {
			tp_log_err("change to bl mode err, 0x%X\n", buf[0]);
			return ILITEK_TP_CHANGETOBL_ERR;
		}
		else {
			tp_log_err("change to ap mode err, 0x%X\n", buf[0]);
			return ILITEK_TP_CHANGETOAP_ERR;
		}
	}
	else {
		return 0;
	}
}
int ilitek_upgrade_2302or2312(unsigned int df_startaddr, unsigned int df_endaddr, unsigned int df_checksum,
	unsigned int ap_startaddr, unsigned int ap_endaddr, unsigned int ap_checksum, unsigned char * CTPM_FW) {
	int ret = 0, upgrade_status = 0, i = 0, j = 0, k = 0, tmp_ap_end_addr = 0;
	unsigned char buf[64] = {0};
	if (ilitek_data->bl_ver[0] >= 1 && ilitek_data->bl_ver[1] >= 4) {
		buf[0] = (unsigned char)ILITEK_TP_CMD_WRITE_ENABLE;//0xc4
		buf[1] = 0x5A;
		buf[2] = 0xA5;
		buf[3] = 0x81;
		
		if (!ilitek_data->has_df) { 
			tp_log_info("ilitek no df data set df_endaddr 0x1ffff\n");
			df_endaddr = 0x1ffff;
			df_checksum = 0x1000 * 0xff;
			buf[4] = df_endaddr >> 16;
			buf[5] = (df_endaddr >> 8) & 0xFF;
			buf[6] = (df_endaddr) & 0xFF;
			buf[7] = df_checksum >> 16;
			buf[8] = (df_checksum >> 8) & 0xFF;
			buf[9] = df_checksum & 0xFF;
		}
		else {
			buf[4] = df_endaddr >> 16;
			buf[5] = (df_endaddr >> 8) & 0xFF;
			buf[6] = (df_endaddr) & 0xFF;
			buf[7] = df_checksum >> 16;
			buf[8] = (df_checksum >> 8) & 0xFF;
			buf[9] = df_checksum & 0xFF;
		}
		tp_log_info("ilitek df_startaddr=0x%X, df_endaddr=0x%X, df_checksum=0x%X\n", df_startaddr, df_endaddr, df_checksum);
		ret = ilitek_i2c_write(buf, 10);
		if(ret < 0)
		{
			tp_log_err("ilitek_i2c_write\n");
		}		
		msleep(20);
		j = 0;
		for(i = df_startaddr; i < df_endaddr; i += 32)
		{
			buf[0] = ILITEK_TP_CMD_WRITE_DATA;
			for(k = 0; k < 32; k++)
			{
				if (ilitek_data->has_df) { 
					if ((i + k) >= df_endaddr) {
						buf[1 + k] = 0x00;
					}
					else {
						buf[1 + k] = CTPM_FW[i + 32 + k];
					}
				}
				else {
					buf[1 + k] = 0xff;
				}
			}
		
			ret = ilitek_i2c_write(buf, 33);
			if(ret < 0)
			{
				tp_log_err("ilitek_i2c_write_and_read\n");
				return ILITEK_I2C_TRANSFER_ERR;
			}		
			j += 1;
			
		#ifdef ILI_UPDATE_BY_CHECK_INT
			for (k = 0; k < 40; k++) {
				if (!(ilitek_poll_int())) {
					break;
				}
				else {
					mdelay(1);
				}
			}
			if (k >= 40) {
				tp_log_err("upgrade check int fail retry = %d\n", k);
			}
		#else
			if((j % (ilitek_data->page_number)) == 0) {
				//msleep(30);
				mdelay(20);
			}
			mdelay(10);
		#endif
			upgrade_status = ((i - df_startaddr) * 100) / (df_endaddr - df_startaddr);
			if (upgrade_status % 10 == 0) {
				tp_log_info("%c ilitek ILITEK: Firmware Upgrade(Data flash), %02d%c. \n",0x0D,upgrade_status,'%');
			}
		}
		buf[0] = (unsigned char)ILITEK_TP_CMD_WRITE_ENABLE;//0xc4
		buf[1] = 0x5A;
		buf[2] = 0xA5;
		buf[3] = 0x80;
		if (((ap_endaddr + 1) % 32) != 0) {
			tp_log_info("ap_endaddr = 0x%X\n", (int)(ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32)));
			buf[4] = (ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32)) >> 16;
			buf[5] = ((ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32)) >> 8) & 0xFF;
			buf[6] = ((ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32))) & 0xFF;
			tp_log_info("ap_checksum = 0x%X\n", (int)(ap_checksum + (32 + 32 - ((ap_endaddr + 1) % 32)) * 0xff));
			buf[7] = (ap_checksum + (32 + 32 -((ap_endaddr + 1) % 32)) * 0xff) >> 16;
			buf[8] = ((ap_checksum + (32 + 32 -((ap_endaddr + 1) % 32)) * 0xff) >> 8) & 0xFF;
			buf[9] = (ap_checksum + (32 + 32 - ((ap_endaddr + 1) % 32)) * 0xff) & 0xFF;
		}
		else {
			tp_log_info("32 0 ap_endaddr  32 = 0x%X\n", (int)(ap_endaddr + 32));
			tp_log_info("ap_endaddr = 0x%X\n", (int)(ap_endaddr + 32));
			buf[4] = (ap_endaddr + 32) >> 16;
			buf[5] = ((ap_endaddr + 32) >> 8) & 0xFF;
			buf[6] = ((ap_endaddr + 32)) & 0xFF;
			tp_log_info("ap_checksum = 0x%X\n", (int)(ap_checksum + 32 * 0xff));
			buf[7] = (ap_checksum + 32 * 0xff) >> 16;
			buf[8] = ((ap_checksum + 32 * 0xff) >> 8) & 0xFF;
			buf[9] = (ap_checksum + 32 * 0xff) & 0xFF;
		}
		ret = ilitek_i2c_write(buf, 10);
		msleep(20);
		tmp_ap_end_addr = ap_endaddr;
		ap_endaddr += 32;
		j = 0;
		for(i = ap_startaddr; i < ap_endaddr; i += 32)
		{
			buf[0] = ILITEK_TP_CMD_WRITE_DATA;
			for(k = 0; k < 32; k++)
			{
				if((i + k) > tmp_ap_end_addr) {
					buf[1 + k] = 0xff;
				}
				else {
					buf[1 + k] = CTPM_FW[i + k + 32];
				}
			}
		
			buf[0] = 0xc3;
			ret = ilitek_i2c_write(buf, 33);
			if(ret < 0)
			{
				tp_log_err("%s, ilitek_i2c_write_and_read\n", __func__);
				return ILITEK_I2C_TRANSFER_ERR;
			}		
			j += 1;
	#ifdef ILI_UPDATE_BY_CHECK_INT
			for (k = 0; k < 40; k++) {
				if (!(ilitek_poll_int())) {
					break;
				}
				else {
					mdelay(1);
				}
			}
			if (k >= 40) {
				tp_log_err("upgrade check int fail retry = %d\n", k);
			}
	#else
			if((j % (ilitek_data->page_number)) == 0) {
				mdelay(20);
			}
			mdelay(10);
	#endif
			upgrade_status = (i * 100) / ap_endaddr;
			if (upgrade_status % 10 == 0) {
				tp_log_info("%c ilitek ILITEK: Firmware Upgrade(AP), %02d%c. \n", 0x0D, upgrade_status, '%');
			}
		}
	}
	else {
		tp_log_info("not support this bl version upgrade flow\n");
	}
	return 0;
}
int ilitek_upgrade_2511(unsigned int df_startaddr, unsigned int df_endaddr,
	unsigned int ap_startaddr, unsigned int ap_endaddr, unsigned char * CTPM_FW) {
	int ret = 0, upgrade_status = 0, i = 0, k = 0, CRC_DF = 0, CRC_AP = 0;
	unsigned char buf[64] = {0};
	for(i = df_startaddr + 2; i < df_endaddr; i++)
	{
		CRC_DF = UpdateCRC(CRC_DF, CTPM_FW[i + 32]);
	}	
	tp_log_info("CRC_DF = 0x%X\n", CRC_DF);
	for(i = ap_startaddr; i < ap_endaddr - 1; i++)
	{
		CRC_AP = UpdateCRC(CRC_AP, CTPM_FW[i + 32]);
	}
	tp_log_info("CRC_AP = 0x%x\n", CRC_AP);

	buf[0] = (unsigned char)ILITEK_TP_CMD_WRITE_ENABLE;//0xc4
	buf[1]=0x5A;
	buf[2]=0xA5;
	buf[3]=0x01;
	buf[4]=df_endaddr >> 16;
	buf[5]=(df_endaddr >> 8) & 0xFF;
	buf[6]=(df_endaddr) & 0xFF;
	buf[7]=CRC_DF >> 16;
	buf[8]=(CRC_DF >> 8) & 0xFF;
	buf[9]=CRC_DF & 0xFF; 
	ret = ilitek_i2c_write(buf, 10);
	if(ret < 0)
	{
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	check_busy(1);
	if(((df_endaddr) % 32) != 0) {
		df_endaddr += 32;
	}
	tp_log_info("write data to df mode\n");
	for(i = df_startaddr; i < df_endaddr; i += 32) {
		// we should do delay when the size is equal to 512 bytes
		buf[0] = (unsigned char)ILITEK_TP_CMD_WRITE_DATA;
		for(k = 0; k < 32; k++) {
			buf[1 + k] = CTPM_FW[i + k + 32];
		}
		if(ilitek_i2c_write(buf, 33) < 0) {
			tp_log_err("%s, ilitek_i2c_write_and_read\n", __func__);
			return ILITEK_I2C_TRANSFER_ERR;
		}
		check_busy(1);
		upgrade_status = ((i - df_startaddr) * 100) / (df_endaddr - df_startaddr);
		//tp_log_info("%c ilitek ILITEK: Firmware Upgrade(Data flash), %02d%c. \n",0x0D,upgrade_status,'%');
	}
	
	buf[0] = (unsigned char) 0xC7;
	if(ilitek_i2c_write(buf, 1) < 0) {
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	check_busy(1);
	buf[0] = (unsigned char)0xC7;
	ilitek_i2c_write_and_read(buf, 1, 10, buf, 4);
	tp_log_info("upgrade end write c7 read 0x%X, 0x%X, 0x%X, 0x%X\n", buf[0], buf[1], buf[2], buf[3]);
	if (CRC_DF != buf[1] * 256 + buf[0]) {
		tp_log_err("CRC DF compare error\n");
		//return ILITEK_UPDATE_FAIL;
	}
	else {
		tp_log_info("CRC DF compare Right\n");
	}
	buf[0] = (unsigned char)ILITEK_TP_CMD_WRITE_ENABLE;//0xc4
	buf[1]=0x5A;
	buf[2]=0xA5;
	buf[3]=0x00;
	buf[4]=(ap_endaddr + 1) >> 16;
	buf[5]=((ap_endaddr + 1) >> 8) & 0xFF;
	buf[6]=((ap_endaddr + 1)) & 0xFF;
	buf[7]=0;
	buf[8]=(CRC_AP & 0xFFFF) >> 8;
	buf[9]=CRC_AP & 0xFFFF; 
	ret = ilitek_i2c_write(buf, 10);
	if(ret < 0)
	{
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	check_busy(1);
	if(((ap_endaddr + 1) % 32) != 0) {
		tp_log_info("ap_endaddr += 32\n");
		ap_endaddr += 32;
	}
	tp_log_info("write data to AP mode\n");
	for(i = ap_startaddr; i < ap_endaddr; i += 32) {
		buf[0] = (unsigned char)ILITEK_TP_CMD_WRITE_DATA;
		for(k = 0; k < 32; k++) {
			buf[1 + k] = CTPM_FW[i + k + 32];
		}
		if (ilitek_i2c_write(buf, 33) < 0) {
			tp_log_err("ilitek_i2c_write\n");
			return ILITEK_I2C_TRANSFER_ERR;
		}
		check_busy(1);
		upgrade_status = ((i - ap_startaddr) * 100) / (ap_endaddr - ap_startaddr);
		//tp_log_info("%c ilitek ILITEK: Firmware Upgrade(AP), %02d%c. \n",0x0D,upgrade_status,'%');
	}
	buf[0] = (unsigned char) 0xC7;
	if(ilitek_i2c_write(buf, 1) < 0) {
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	check_busy(1);
	buf[0] = (unsigned char)0xC7;
	ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 4);
	if(ret < 0)
	{
		tp_log_err("ilitek_i2c_write_and_read 0xc7\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	tp_log_info("upgrade end write c7 read 0x%X, 0x%X, 0x%X, 0x%X\n", buf[0], buf[1], buf[2], buf[3]);
	if (CRC_AP != buf[1] * 256 + buf[0]) {
		tp_log_err("CRC compare error\n");
		return ILITEK_TP_UPGRADE_FAIL;
	}
	else {
		tp_log_info("CRC compare Right\n");
	}
	return 0;
}

int inwrite(unsigned int address)
{
	uint8_t outbuff[64];
	int data, ret;
	outbuff[0] = REG_START_DATA;
	outbuff[1] = (char)((address & DATA_SHIFT_0) >> 0);
	outbuff[2] = (char)((address & DATA_SHIFT_8) >> 8);
	outbuff[3] = (char)((address & DATA_SHIFT_16) >> 16);
	ret = ilitek_i2c_write(outbuff, 4);
	if (ret < 0) {
		tp_log_err("i2c communication err\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	mdelay(1);
	ret = ilitek_i2c_read(outbuff, 4);
	if (ret < 0) {
		tp_log_err("i2c communication err\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	data = (outbuff[0] + outbuff[1] * 256 + outbuff[2] * 256 * 256 + outbuff[3] * 256 * 256 * 256);
	tp_log_debug("data=0x%x, outbuff[0]=%x, outbuff[1]=%x, outbuff[2]=%x, outbuff[3]=%x\n", data, outbuff[0], outbuff[1], outbuff[2], outbuff[3]);
	return data;
}

int outwrite(unsigned int address, unsigned int data, int size)
{
	int ret, i;
	char outbuff[64];
	outbuff[0] = REG_START_DATA;
	outbuff[1] = (char)((address & DATA_SHIFT_0) >> 0);
	outbuff[2] = (char)((address & DATA_SHIFT_8) >> 8);
	outbuff[3] = (char)((address & DATA_SHIFT_16) >> 16);
	for(i = 0; i < size; i++)
	{
		outbuff[i + 4] = (char)(data >> (8 * i));
	}
	ret = ilitek_i2c_write(outbuff, size + 4);
	if (ret < 0) {
		tp_log_err("i2c communication err\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	return ret;
}

int ilitek_ready_upgrade_2120(void) {
	int ret = 0;
	ret = outwrite(ENTER_ICE_MODE, ENTER_ICE_MODE_NO_DATA, 0);
	if (ret < 0) {
		tp_log_err("communication err ret = %d\n", ret);
		return ret;
	}
	msleep(2);
	ret = outwrite(WDTRLDT, WDTRLDT_CLOSE, 2);
	if (ret < 0) {
		tp_log_err("communication err ret = %d\n", ret);
		return ret;
	}
	msleep(2);
	ret = outwrite(WDTCNT1, WDTCNT1_OPEN, 1);
	if (ret < 0) {
		tp_log_err("communication err ret = %d\n", ret);
		return ret;
	}
	msleep(2);
	ret = outwrite(WDTCNT1, WDTCNT1_CLOSE, 1);
	if (ret < 0) {
		tp_log_err("communication err ret = %d\n", ret);
		return ret;
	}
	msleep(2);
	ret = outwrite(CLOSE_10K_WDT1, CLOSE_10K_WDT1_VALUE, 4);
	if (ret < 0) {
		tp_log_err("communication err ret = %d\n", ret);
		return ret;
	}
	msleep(2);
	ret = outwrite(CLOSE_10K_WDT2, CLOSE_10K_WDT2_VALUE, 1);
	if (ret < 0) {
		tp_log_err("communication err ret = %d\n", ret);
		return ret;
	}
	msleep(2);
	ret = outwrite(CLOSE_10K_WDT3, CLOSE_10K_WDT3_VALUE, 4);
	if (ret < 0) {
		tp_log_err("communication err ret = %d\n", ret);
		return ret;
	}
	msleep(2);
	tp_log_info("%s, release Power Down Release mode\n", __func__);
	ret = outwrite(REG_FLASH_CMD, REG_FLASH_CMD_RELEASE_FROM_POWER_DOWN, 1);
	if (ret < 0) {
		tp_log_err("communication err ret = %d\n", ret);
		return ret;
	}
	msleep(2);
	ret = outwrite(REG_PGM_NUM, REG_PGM_NUM_TRIGGER_KEY, 4);
	if (ret < 0) {
		tp_log_err("communication err ret = %d\n", ret);
		return ret;
	}
	msleep(2);
	ret = outwrite(REG_TIMING_SET, REG_TIMING_SET_10MS, 1);
	if (ret < 0) {
		tp_log_err("communication err ret = %d\n", ret);
		return ret;
	}
	msleep(10);
	return ret;
}

int ilitek_erase_sector_2120(void) {
	int ret = 0, i = 0, j = 0, temp = 0;
	unsigned char buf[64] = {0};
	for (i = 0; i <= SECTOR_ENDADDR; i += SECTOR_SIZE) {
		tp_log_debug("i = %X\n", i);
		ret = outwrite(REG_FLASH_CMD, REG_FLASH_CMD_WRITE_ENABLE, 1);
		if (ret < 0) {
			tp_log_err("communication err ret = %d\n", ret);
			return ret;
		}
		msleep(1);
		ret = outwrite(REG_PGM_NUM, REG_PGM_NUM_TRIGGER_KEY, 4);
		if (ret < 0) {
			tp_log_err("communication err ret = %d\n", ret);
			return ret;
		}
		msleep(1);
		temp = (i << 8) + REG_FLASH_CMD_DATA_ERASE;
		ret = outwrite(REG_FLASH_CMD, temp, 4);
		if (ret < 0) {
			tp_log_err("communication err ret = %d\n", ret);
			return ret;
		}
		msleep(1);
		ret = outwrite(REG_PGM_NUM, REG_PGM_NUM_TRIGGER_KEY, 4);
		if (ret < 0) {
			tp_log_err("communication err ret = %d\n", ret);
			return ret;
		}
		msleep(25);
		for (j = 0; j < 50; j++) {
			ret = outwrite(REG_FLASH_CMD, REG_FLASH_CMD_READ_FLASH_STATUS, 1);
			if (ret < 0) {
				tp_log_err("communication err ret = %d\n", ret);
				return ret;
			}
			ret = outwrite(REG_PGM_NUM, REG_PGM_NUM_TRIGGER_KEY, 4);
			if (ret < 0) {
				tp_log_err("communication err ret = %d\n", ret);
				return ret;
			}
			msleep(1);
			buf[0] = inwrite(FLASH_STATUS);
			if (ret < 0) {
				tp_log_err("communication err ret = %d\n", ret);
				return ret;
			}
			tp_log_debug("buf[0] = %X\n", buf[0]);
			if (buf[0] == 0) {
				break;
			}
			else {
				msleep(2);
			};
		}
		if (j >= 50) {
			tp_log_info("FLASH_STATUS ERROR j = %d, buf[0] = 0x%X\n", j, buf[0]);
		}
	}
	msleep(100);
	return 0;
}

int ilitek_write_data_upgrade_2120(unsigned char * CTPM_FW) {
	int ret = 0, i = 0, k = 0, temp = 0;
	unsigned char buf[512] = {0};
	for(i = AP_STARTADDR; i < AP_ENDADDR; i += UPGRADE_TRANSMIT_LEN) {
		tp_log_debug("i = %X\n", i);
		ret = outwrite(REG_FLASH_CMD, REG_FLASH_CMD_WRITE_ENABLE, 1);
		if (ret < 0) {
			tp_log_err("communication err ret = %d\n", ret);
			return ret;
		}
		ret = outwrite(REG_PGM_NUM, REG_PGM_NUM_TRIGGER_KEY, 4);
		if (ret < 0) {
			tp_log_err("communication err ret = %d\n", ret);
			return ret;
		}
		temp = (i << 8) + REG_FLASH_CMD_DATA_PROGRAMME;
		ret = outwrite(REG_FLASH_CMD, temp, 4);
		if (ret < 0) {
			tp_log_err("communication err ret = %d\n", ret);
			return ret;
		}
		ret = outwrite(REG_PGM_NUM, REG_PGM_LEN, 4);
		if (ret < 0) {
			tp_log_err("communication err ret = %d\n", ret);
			return ret;
		}
		buf[0] = REG_START_DATA;
		buf[3] = (char)((REG_PGM_DATA  & DATA_SHIFT_16) >> 16);
		buf[2] = (char)((REG_PGM_DATA  & DATA_SHIFT_8) >> 8);
		buf[1] = (char)((REG_PGM_DATA  & DATA_SHIFT_0));
		for(k = 0; k < UPGRADE_TRANSMIT_LEN; k++)
		{
			buf[4 + k] = CTPM_FW[i  + k + 32];
		}
		ret = ilitek_i2c_write(buf, UPGRADE_TRANSMIT_LEN + REG_LEN);
		if(ret < 0) {
			tp_log_err("write data error, address = 0x%X, start_addr = 0x%X, end_addr = 0x%X\n", (int)i, (int)AP_STARTADDR, (int)AP_ENDADDR);
			tp_log_err("communication err ret = %d\n", ret);
			return ret;
		}
		mdelay(3);
	}
	return 0;
}
int ilitek_upgrade_2120(unsigned char * CTPM_FW) {
	int ret = 0, retry = 0;
	unsigned char buf[64] = {0};
Retry:
	if (retry < 2) {
		retry++;
	}
	else {
		tp_log_err("retry 2 times upgrade fail\n");
		return ret;
	}
	ret = ilitek_ready_upgrade_2120();
	if (ret < 0) {
		tp_log_err("ilitek_ready_upgrade_2120 err ret = %d\n", ret);
		goto Retry;
	}
	ret = ilitek_erase_sector_2120();
	if (ret < 0) {
		tp_log_err("ilitek_erase_sector_2120 err ret = %d\n", ret);
		goto Retry;
	}
	ret = ilitek_write_data_upgrade_2120(CTPM_FW);
	if (ret < 0) {
		tp_log_err("ilitek_write_data_upgrade_2120 err ret = %d\n", ret);
		goto Retry;
	}
	buf[0] = (unsigned char)(EXIT_ICE_MODE & DATA_SHIFT_0);
	buf[1] = (unsigned char)((EXIT_ICE_MODE & DATA_SHIFT_8) >> 8);
	buf[2] = (unsigned char)((EXIT_ICE_MODE & DATA_SHIFT_16) >> 16);
	buf[3] = (unsigned char)((EXIT_ICE_MODE & DATA_SHIFT_24) >> 24);
	ilitek_i2c_write(buf, 4);
	ilitek_reset(100);

	buf[0] = ILITEK_TP_CMD_GET_TOUCH_INFORMATION;
	ilitek_i2c_write(buf, 1);
	ret = ilitek_i2c_read(buf, 3);
	tp_log_info("buf = %X, %X, %X\n", buf[0], buf[1], buf[2]);
	if (buf[1] >= 0X80) {
		tp_log_info("upgrade ok ok \n");
	}else {
		goto Retry;
	}
	return 0;
}

int ilitek_upgrade_bigger_size_ic(unsigned int df_startaddr, unsigned int df_endaddr, unsigned int df_checksum,
	unsigned int ap_startaddr, unsigned int ap_endaddr, unsigned int ap_checksum, unsigned char * CTPM_FW) {
	int ret = 0, retry = 0, i = 0;
	unsigned char buf[64] = {0};
Retry:
	if (retry < 2) {
		retry++;
	}
	else {
		tp_log_err("retry 2 times upgrade fail\n");
		return ret;
	}
	buf[0] = 0xF2;
	buf[1] = 0x01;
	ret = ilitek_i2c_write(buf, 2);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write\n");
		goto Retry;
	}
	check_busy(1);
	//check ic type
	buf[0] = ILITEK_TP_CMD_GET_KERNEL_VERSION;
	ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 5);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write_and_read\n");
		goto Retry;
	}
	for (i = 0; i < 5; i++) {
		ilitek_data->mcu_ver[i] = buf[i];
	}
	tp_log_info("MCU KERNEL version:%d.%d.%d.%d.%d\n", buf[0], buf[1], buf[2],buf[3], buf[4]);

	ret = ilitek_changetoblmode(true);
	if(ret) {
		tp_log_err("change to bl mode err ret = %d\n", ret);
		goto Retry;
	}
	else {
		tp_log_info("ilitek change to bl mode ok\n");
	}
	buf[0] = ILITEK_TP_CMD_GET_PROTOCOL_VERSION;
	ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 4);
	if(ret < 0)
	{
		tp_log_err("ilitek_i2c_write_and_read ret = %d\n", ret);
		goto Retry;
	}
	tp_log_info("bl protocol version %d.%d\n", buf[0], buf[1]);
	ilitek_data->bl_ver[0] = buf[0];
	ilitek_data->bl_ver[1] = buf[1];
	if (buf[0] == 1 && buf[1] >= 4) {
		buf[0] = ILITEK_TP_CMD_GET_KERNEL_VERSION;
		ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 6);
		df_startaddr = buf[2] * 256 * 256 + buf[3] * 256 + buf[4];
		tp_log_info("df_start_addr = %x\n", (int)df_startaddr);
		if(buf[0] != 0x05) {
			ilitek_data->page_number = 16;
			tp_log_info("ilitek page_number = 16, page is 512 bytes\n");
		} else {
			ilitek_data->page_number = 8;
			tp_log_info("ilitek page_number = 8, page is 256 bytes\n");
		}
		buf[0] = 0xc7;
		ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 1);
		tp_log_info("0xc7 read= %x\n", buf[0]);
	}
		
	if ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25) {
		df_startaddr = 0xF000;
		ret = ilitek_upgrade_2511(df_startaddr, df_endaddr, ap_startaddr, ap_endaddr, CTPM_FW);
		if (ret < 0) {
			tp_log_err("ilitek_upgrade_2511 err ret = %d\n", ret);
			goto Retry;
		}
	}
	else {
		df_startaddr = 0x1F000;
		if (df_startaddr < df_endaddr) {
			ilitek_data->has_df = true;
		}
		else {
			ilitek_data->has_df = false;
		}
		ret = ilitek_upgrade_2302or2312(df_startaddr, df_endaddr, df_checksum, ap_startaddr, ap_endaddr, ap_checksum, CTPM_FW);
		if (ret < 0) {
			tp_log_err("ilitek_upgrade_2302or2312 err ret = %d\n", ret);
			goto Retry;
		}
	}
	tp_log_info("upgrade firmware completed	reset\n");
	ilitek_reset(300);
	
	ret = ilitek_changetoblmode(false);
	if(ret) {
		tp_log_err("change to ap mode err\n");
		return ILITEK_TP_CHANGETOAP_ERR;
	}
	else {
		tp_log_info("ilitek change to ap mode ok\n");
	}
	return 0;
}
#ifdef ILITEK_UPDATE_FW
int ilitek_upgrade_firmware(void) {
	int ret = 0, i = 0, ap_len = 0, df_len = 0;
	unsigned int ap_startaddr = 0, df_startaddr = 0, ap_endaddr = 0, df_endaddr = 0, ap_checksum = 0, df_checksum = 0;
	unsigned char firmware_ver[8] = {0};
	const struct firmware *fw;
	uint8_t * CTPM_FW_BIN = NULL;
	ap_startaddr = ( CTPM_FW[0] << 16 ) + ( CTPM_FW[1] << 8 ) + CTPM_FW[2];
	ap_endaddr = ( CTPM_FW[3] << 16 ) + ( CTPM_FW[4] << 8 ) + CTPM_FW[5];
	ap_checksum = ( CTPM_FW[6] << 16 ) + ( CTPM_FW[7] << 8 ) + CTPM_FW[8];
	df_startaddr = ( CTPM_FW[9] << 16 ) + ( CTPM_FW[10] << 8 ) + CTPM_FW[11];
	df_endaddr = ( CTPM_FW[12] << 16 ) + ( CTPM_FW[13] << 8 ) + CTPM_FW[14];
	df_checksum = ( CTPM_FW[15] << 16 ) + ( CTPM_FW[16] << 8 ) + CTPM_FW[17];
	firmware_ver[0] = CTPM_FW[18];
	firmware_ver[1] = CTPM_FW[19];
	firmware_ver[2] = CTPM_FW[20];
	firmware_ver[3] = CTPM_FW[21];
	firmware_ver[4] = CTPM_FW[22];
	firmware_ver[5] = CTPM_FW[23];
	firmware_ver[6] = CTPM_FW[24];
	firmware_ver[7] = CTPM_FW[25];
	df_len = ( CTPM_FW[26] << 16 ) + ( CTPM_FW[27] << 8 ) + CTPM_FW[28];
	ap_len = ( CTPM_FW[29] << 16 ) + ( CTPM_FW[30] << 8 ) + CTPM_FW[31];
	if (ilitek_data->ic_2120 && ILITEK_UPGRADE_WITH_BIN) {
		CTPM_FW_BIN = vmalloc(64 * 1024);
		if (!CTPM_FW_BIN) {
			tp_log_err("CTPM_FW_BIN alloctation memory failed\n");
			return ILITEK_TP_UPGRADE_FAIL;
		}
		ap_startaddr = AP_STARTADDR;
		ap_endaddr = AP_ENDADDR;
		ret = request_firmware(&fw, ILITEK_FW_FILENAME, &ilitek_data->client->dev);
		if (ret) {
			tp_log_err("failed to request firmware %s: %d\n", ILITEK_FW_FILENAME, ret);
			return ret;
		}
		tp_log_info("ilitek fw->size = %d\n", (int)fw->size);
		if (fw->size != 0xE000) {
			tp_log_err("ilitek fw->size = %d err\n", (int)fw->size);
			return ILITEK_TP_UPGRADE_FAIL;
		}
		for (ret = 0; ret < fw->size; ret++) {
			CTPM_FW_BIN[ret + 32] = fw->data[ret];
		}
		firmware_ver[0] = 0x0;
		firmware_ver[1] = CTPM_FW_BIN[FW_VERSION1 + 32];
		firmware_ver[2] = CTPM_FW_BIN[FW_VERSION2 + 32];
		firmware_ver[3] = CTPM_FW_BIN[FW_VERSION3 + 32];
		release_firmware(fw);
		
		tp_log_info("firmware_ver[0] = %d, firmware_ver[1] = %d firmware_ver[2]=%d firmware_ver[3]=%d\n",firmware_ver[0], firmware_ver[1], firmware_ver[2], firmware_ver[3]);
	}
	tp_log_info("ilitek ap_startaddr=0x%X, ap_endaddr=0x%X, ap_checksum=0x%X, ap_len = 0x%d\n", ap_startaddr, ap_endaddr, ap_checksum, ap_len);
	tp_log_info("ilitek df_startaddr=0x%X, df_endaddr=0x%X, df_checksum=0x%X, df_len = 0x%d\n", df_startaddr, df_endaddr, df_checksum, df_len);
	if (!(ilitek_data->force_update)) {
		for (i = 0; i < 8; i++) {
			tp_log_info("ilitek_data.firmware_ver[%d] = %d, firmware_ver[%d] = %d\n", i, ilitek_data->firmware_ver[i], i, firmware_ver[i]);
			if (!ilitek_data->ic_2120) {
				if (firmware_ver[i] < ilitek_data->firmware_ver[i]) {
					i = 8;
					break;
				}
				if (firmware_ver[i] > ilitek_data->firmware_ver[i]) {
					break;
				}
			}
			else {
				if (firmware_ver[i] != ilitek_data->firmware_ver[i]) {
					break;
				}
			}
		}
		if (i >= 8) {
			if (!ilitek_data->ic_2120) {
				tp_log_info("firmware version is older so not upgrade\n");
			}
			else {
				tp_log_info("firmware version is same so not upgrade\n");
				if (ILITEK_UPGRADE_WITH_BIN) {
					if (CTPM_FW_BIN) {
						vfree(CTPM_FW_BIN);
						CTPM_FW_BIN = NULL;
					}
				}
			}
			return 1;
		}
	}
	if (!ilitek_data->ic_2120) {
		ret = ilitek_upgrade_bigger_size_ic(df_startaddr, df_endaddr, df_checksum, ap_startaddr, ap_endaddr, ap_checksum, CTPM_FW);
	}
	else {
		if (ILITEK_UPGRADE_WITH_BIN) {
			ret = ilitek_upgrade_2120(CTPM_FW_BIN);
		}
		else {
			ret = ilitek_upgrade_2120(CTPM_FW);
		}
	}
	if (ret < 0) {
		tp_log_err("upgrade fail\n");
		return ret;
	}
	if (ILITEK_UPGRADE_WITH_BIN) {
		if (CTPM_FW_BIN) {
			vfree(CTPM_FW_BIN);
			CTPM_FW_BIN = NULL;
		}
	}
	return 0;
}
#endif
