/*
 * Intel Baytrail ULPMC FW Update driver
 *
 * Copyright (C) 2013 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>
#include <linux/miscdevice.h>
#include <linux/atomic.h>
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>
#include <linux/power/byt_ulpmc_battery.h>

#define DRIVER_NAME	"ulpmc-fwupdate"


//Add by emdoor jim.kuang for ite burning 20140422
////////////////////////////////////////////////////////////////
static u8 xbSMBusWriteBuff[6]={0,0,0,0,0,0};
static u8 xbSMBusReadBuff[1]={0};
#define EFLASH_CMD_BYTE_PROGRAM	 	0x02
#define EFLASH_CMD_WRITE_DISABLE 	0x04
#define EFLASH_CMD_READ_STATUS	 	0x05
#define EFLASH_CMD_WRITE_ENABLE		0x06
#define EFLASH_CMD_FAST_READ		0x0B
#define EFLASH_CMD_CHIP_ERASE	 	0x60
#define EFLASH_CMD_READ_ID		0x9F
#define EFLASH_CMD_AAI_WORD_PROGRAM 	0xAD
#define EFLASH_CMD_SECTOR_ERASE	 	0xD7
//Add by emdoor jim.kuang for ite burning 20140422
////////////////////////////////////////////////////////////////
#define BSL_MESSAGE_CODE		0x3B

//#define CMD_ENTER_BSL_MODE		0xF0
#define CMD_ENTER_BSL_MODE		0xB4 //modify by emdoor jim.kuang for fireware update

#define CMD_RX_DATA_BLOCK		0x10
#define CMD_RX_DATA_BLOCK_FAST		0x1B
#define CMD_RX_PASSWORD			0x11
#define CMD_ERASE_SEGMENT		0x12
#define CMD_UNLOCK_LOCK_INFO		0x13
#define CMD_RESERVED			0x14
#define CMD_MASS_ERASE			0x15
#define CMD_CRC_CHECK			0x16
#define CMD_LOAD_PC			0x17
#define CMD_TX_DATA_BLOCK		0x18
#define CMD_TX_BSL_VERSION		0x19
#define CMD_TX_BUFFER_SIZE		0x1A

#define ULPMC_CMD_HEADER		0x80
#define ULPMC_RST_PMMCTL0_REG		0x120
#define PMMCTL0_REG_VAL_LSB		0x04
#define PMMCTL0_REG_VAL_MSB		0xA5

#define MASS_ERASE_RESPONSE_LENGTH		(6 + 2)
#define CRC_CHECK_RESPONSE_LENGTH		(6 + 3)
#define TX_BSL_VERSION_RESPONSE_LENGTH		(6 + 5)
#define TX_BUFFER_SIZE_RESPONSE_LENGTH		(6 + 3)
#define LOAD_PC_RESPONSE_LENGTH			(6 + 2)
#define RX_DATA_BLOCK_RESPONSE_LENGTH		(6 + 2)
#define RX_PASSWORD_RESPONSE_LENGTH		(6 + 2)
#define ERASE_SEGMENT_RESPONSE_LENGTH		(6 + 2)
#define UNLOCK_LOCK_INFO_RESPONSE_LENGTH	(6 + 2)

#define ULPMC_SEGMENT_OFFSET_B			0x1900
#define ULPMC_SEGMENT_OFFSET_C			0x1880
#define ULPMC_SEGMENT_OFFSET_D			0x1800

#define TX_DATA_BLOCK_RESPONSE_LENGTH		6	/* variable length */

#define RX_TX_BUFFER_LEN			256
#define TX_DATA_BUFFER_SIZE			240
#define INPUT_DATA_LEN				32
#define TEMP_BUFFER_LEN				8

/* No of times we should retry on -EAGAIN error */
#define NR_RETRY_CNT	3

static u8 input_data[INPUT_DATA_LEN];
static u8 *file_buf;
static u32 fdata_len;
static u32 fcur_idx;
static u32 fcur_addr;

static struct platform_device *pdev;

struct ulpmc_fwu_info {
	struct platform_device *pdev;
	struct i2c_client	*client;
	struct notifier_block	nb;

	atomic_t		fcount;
	u16			crc;
	struct miscdevice	ulpmc_mdev;
};
struct ulpmc_fwu_info_burning {
	struct i2c_client	*client;
};

static u32 file_len=0;
static struct ulpmc_fwu_info *fwu_ptr;
static struct ulpmc_fwu_info *ite_fwu_ptr=NULL;
static struct ulpmc_fwu_info_burning *ite_burning=NULL;

static void crc_init(struct ulpmc_fwu_info *fwu)
{
	fwu->crc = 0xffff;
}

static void crc_ccitt_update(struct ulpmc_fwu_info *fwu, u8 x)
{
	u16 crc_new =
			(fwu->crc >> 8) | (fwu->crc << 8);
	crc_new ^= x;
	crc_new ^= (crc_new & 0xff) >> 4;
	crc_new ^= crc_new << 12;
	crc_new ^= (crc_new & 0xff) << 5;
	fwu->crc = crc_new;
}

static u16 crc_ccitt_crc(struct ulpmc_fwu_info *fwu)
{
	return fwu->crc;
}

static int do_i2c_transfer(struct ulpmc_fwu_info *fwu,
				struct i2c_msg *msg, int num)
{
	int ret, nc;

	for (nc = 0; nc < NR_RETRY_CNT; nc++) {
		ret = i2c_transfer(fwu->client->adapter, msg, num);
		if (ret < 0)
			continue;
		else
			break;
	}

	return ret;
}

static int fill_BSLCmd_packet(struct ulpmc_fwu_info *fwu, u8 cmd, u32 addr,
				u8 *data, u16 size, u8 *buf, u16 *buflen)
{
	u16 len = 1, chksum, widx = 0;
	u32 i;

	if ((cmd == 0) && (addr == 0)) {
		dev_err(&fwu->pdev->dev, "error:%s\n", __func__);
		return -EINVAL;
	}

	if (cmd == CMD_RX_DATA_BLOCK ||
		cmd == CMD_RX_DATA_BLOCK_FAST ||
		cmd == CMD_ERASE_SEGMENT ||
		cmd == CMD_CRC_CHECK ||
		cmd == CMD_LOAD_PC ||
		cmd == CMD_TX_DATA_BLOCK) {
		/* adding Size of aata and address size(3 Bytes) */
		len += size + 3;
	} else {
		/* adding the size of the data */
		len += size;
	}

	buf[widx++] = ULPMC_CMD_HEADER;
	buf[widx++] = len & 0xFF;
	buf[widx++] = (len >> 8) & 0xFF;
	/* initialize CRC here */
	crc_init(fwu);
	buf[widx++] = cmd;
	crc_ccitt_update(fwu, cmd);
	if (addr != 0) {
		buf[widx++] = addr & 0xFF;
		crc_ccitt_update(fwu, (u8)(addr & 0xFF));
		buf[widx++] = (addr >> 8) & 0xFF;
		crc_ccitt_update(fwu, (u8)((addr >> 8) & 0xFF));
		buf[widx++] = (addr >> 16) & 0xFF;
		crc_ccitt_update(fwu, (u8)((addr >> 16) & 0xFF));
	}
	for (i = 0; i < size; i++) {
		buf[widx++] = data[i];
		crc_ccitt_update(fwu, data[i]);
	}
	chksum = crc_ccitt_crc(fwu);
	buf[widx++] = chksum & 0xFF;
	buf[widx++] = (chksum >> 8) & 0xFF;

	*buflen = widx;
	return 0;
}

static int recieve_BSLResponse(struct ulpmc_fwu_info *fwu, u16 read_len,
				u8 *data, u16 *len)
{
	int ret;
	u8 res_buf[RX_TX_BUFFER_LEN];
	struct i2c_msg msg;
	u16 i, res_len, crcval, rcvd_crcval;
	
	mdelay(100);
	
	memset(res_buf, 0x0, RX_TX_BUFFER_LEN);
	/* send mass erase cmd */
	msg.addr = 0x5b;//fwu->client->addr;
	msg.flags = I2C_M_RD;
	msg.buf = res_buf;
	msg.len = read_len;
	ret = do_i2c_transfer(fwu, &msg, 1);
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", ret);
		return ret;
	}
	
	printk("################ %s  0x%x 0x%x ##################\r\n",__func__,res_buf[0],res_buf[1]);

	if ((res_buf[0] != 0x0) || (res_buf[1] != ULPMC_CMD_HEADER)) {
		dev_err(&fwu->pdev->dev, "BSL Response failed:[0]:%x,[1]:%x\n",
							res_buf[0], res_buf[1]);
		goto bsl_response_failed;
	}

	res_len = (u16)res_buf[2] | ((u16)res_buf[3] << 8);
	*len = res_len;

	if (res_len > read_len) {
		dev_err(&fwu->pdev->dev, "Len mismatch:read_len%x,res_len:%x\n",
							read_len, res_len);
		goto bsl_response_failed;
	}

	crc_init(fwu);
	for (i = 0; i < res_len; i++) {
		data[i] = res_buf[4 + i];
		crc_ccitt_update(fwu, data[i]);
	}
	crcval = crc_ccitt_crc(fwu);
	rcvd_crcval = (u16)res_buf[4 + res_len] |
			(u16)(res_buf[4 + res_len + 1] << 8);

	if (crcval != rcvd_crcval) {
		dev_err(&fwu->pdev->dev, "BSL Response CRC chk failed\n");
		goto bsl_response_failed;
	}

	if ((data[0] == BSL_MESSAGE_CODE) && (data[1] != 0x0)) {
		dev_err(&fwu->pdev->dev, "BSL Core response error\n");
		goto bsl_response_failed;
	}

	return 0;

bsl_response_failed:
	return -EIO;
}

static int enter_bsl_mode(struct  ulpmc_fwu_info *fwu)
{
	int ret;
	u8 wbuf[TEMP_BUFFER_LEN];
	u16 idx = 0;
	struct i2c_msg msg;

	dev_info(&fwu->pdev->dev, ":%s\n", __func__);

	memset(wbuf, 0x0, TEMP_BUFFER_LEN);
	msg.addr = 0x76;//fwu->client->addr;
	msg.flags = 0;
	wbuf[idx] = 0xb4;
	msg.buf = &wbuf[idx];
	msg.len = 1;
	
	//Set 0xb4 as 00 to Change work mode to burning mode
	ret = do_i2c_transfer(fwu, &msg, 1);
	if (ret < 0)
		dev_err(&fwu->pdev->dev, "i2c tx failed\n");

	memset(wbuf, 0x0, TEMP_BUFFER_LEN);
	msg.addr = 0x5b;//fwu->client->addr;
	msg.flags = 0;// 0 as write  ; 1 as read
	wbuf[0] = 0x17;
	msg.buf = &wbuf[0];
	msg.len = 1;
	ret = do_i2c_transfer(fwu, &msg, 1);
	if (ret < 0)
		dev_err(&fwu->pdev->dev, "i2c tx failed\n");

	memset(wbuf, 0x0, TEMP_BUFFER_LEN);
	msg.addr = 0x5b;//fwu->client->addr;
	msg.flags = 0;// 0 as write  ; 1 as read
	wbuf[0] = 0x18;
	wbuf[1] = 0x9f;
	msg.buf = &wbuf[0];
	msg.len = 2;
	ret = do_i2c_transfer(fwu, &msg, 1);
	if (ret < 0)
		dev_err(&fwu->pdev->dev, "i2c tx failed\n");

	memset(wbuf, 0x0, TEMP_BUFFER_LEN);
	msg.addr = 0x5b;//fwu->client->addr;
	msg.flags = 1;// 0 as write  ; 1 as read
	wbuf[0] = 0x18;
	msg.buf = &wbuf[0];
	msg.len = 3;
	ret = do_i2c_transfer(fwu, &msg, 1);
	
	if (ret < 0)
			dev_err(&fwu->pdev->dev, "i2c tx failed\n");

	if(wbuf[0] == 0xff && wbuf[1] == 0xff && wbuf[2] == 0xfe )
		ret = 1;
	else
		ret = -1;

	printk("###################wbuf[0][1][2] = [ 0x%x 0x%x 0x%x ] ################\r\n",wbuf[0], wbuf[1],wbuf[2]);
	/* start up delay */
	mdelay(250);

	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////
//Erase ite ic function 2014-04-22

//payload under 256 can call this function
// S 0x5B [W] [A] 0x17 [A] S 0x5B [W] [A] 0x18 [A] cmd1 [A] payload[0] [A] ...
// ... payload[N] [NA] [P]

static int ite_i2c_pre_define_cmd_write(struct  ulpmc_fwu_info *fwu,unsigned char cmd1,unsigned int payload_len,unsigned char payload[])
{
	int ret=0,i;
	struct i2c_msg msg;
	unsigned char buf0[1]={0x17};           //CS High
	unsigned char buf1[256]={0x18};      
	
	buf1[1]=cmd1;

	if(payload_len < 256) {
		//printk("\n\rFill PayLoad: ");
		for(i=0;i<payload_len;i++) {
			buf1[i+2] =payload[i];
			//printk(" %x",payload[i]);
		}
	}else {
		printk("\n\rite_i2c_pre_define_cmd_write: payload_len over 256!");
		ret = -1;
		return ret;
	}
	
	//CS High
	msg.addr = 0x5B;
	msg.len = 1;
	msg.flags = 0;
	msg.buf = (unsigned char *) &buf0;
	
	ret = do_i2c_transfer(fwu, &msg, 1);
	
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", ret);
		return ret;
	}
	
	//CMD1
	msg.addr = 0x5B;
	msg.len = (payload_len+2);
	msg.flags = 0;
	msg.buf =(unsigned char *) &buf1;
	
	ret = do_i2c_transfer(fwu, &msg, 1);
	
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", ret);
		return ret;
	}
	
	return ret;
}

int ite_i2c_pre_define_cmd_read(struct  ulpmc_fwu_info *fwu,unsigned char cmd1,unsigned int payload_len,unsigned char payload[])
{
	int ret=0,i;
	struct i2c_msg msg;
	unsigned char buf0[1]={0x17};           //CS High
	unsigned char buf1[2]={0x18};      
	
	//CS High
	msg.addr = 0x5B;
	msg.len = 1;
	msg.flags = 0;
	msg.buf = (unsigned char *) &buf0;
	
	ret = do_i2c_transfer(fwu, &msg, 1);

	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", ret);
		return ret;
	}
	
	buf1[1]=cmd1;
		
	//CMD1
	msg.addr = 0x5B;
	msg.len = 2;
	msg.flags = 0;
	msg.buf =(unsigned char *) &buf1;
	
	ret = do_i2c_transfer(fwu, &msg, 1);
	
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", ret);
		return ret;
	}
	
	//CMD1 Read Payload
	msg.addr = 0x5B;
	msg.len = payload_len;
	msg.flags = I2C_M_RD;
	msg.buf =(unsigned char *) payload;

	ret = do_i2c_transfer(fwu, &msg, 1);
	
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", ret);
		return ret;
	}
	return ret;
}

static int cmd_write_enable(struct  ulpmc_fwu_info *fwu)
{
	int result;
	
	result = ite_i2c_pre_define_cmd_write(fwu,EFLASH_CMD_WRITE_ENABLE,0,NULL);
	return result;
}

static int cmd_write_disable(struct  ulpmc_fwu_info *fwu)
{
	int result;
	
	result = ite_i2c_pre_define_cmd_write(fwu,EFLASH_CMD_WRITE_DISABLE,0,NULL);
	return result;
}

static int cmd_erase_all(struct  ulpmc_fwu_info *fwu)
{
	int result;

	result = ite_i2c_pre_define_cmd_write(fwu,EFLASH_CMD_CHIP_ERASE,0,NULL);
	return result;
}

static unsigned char cmd_check_status(struct  ulpmc_fwu_info *fwu)
{
	int result;
	unsigned char status[2];
	
	ite_i2c_pre_define_cmd_read(fwu,EFLASH_CMD_READ_STATUS,2,status);
	return status[1];
}

//payload under 256 can call this function
// S 0x5B [W] [A] 0x17 [A] S 0x5B [W] [A] 0x18 [A] cmd1 [A] payload[0] [A] ...
// ... payload[N] [NA] [P]
static int ite_i2c_pre_define_cmd_write_with_status(struct  ulpmc_fwu_info *fwu,unsigned char cmd1,unsigned int payload_len,unsigned char payload[])
{
	int result=0,i;
	int read_status[3];
	struct i2c_msg msg;
	unsigned char buf0[1]={0x17};           //CS High
	unsigned char buf1[256]={0x18};
	unsigned char cmd_status[2]={0x18,EFLASH_CMD_READ_STATUS};
	
	buf1[1]=cmd1;
	if(payload_len < 256) {
		for(i=0;i<payload_len;i++) {
			buf1[i+2] =payload[i];
		}
	}else {
		printk("\n\rite_i2c_pre_define_cmd_write: payload_len over 256!");
		result = -1;
		return result;
	}
	
	//CS High
	msg.addr = 0x5B;
	msg.len = 1;
	msg.flags = 0;
	msg.buf = (unsigned char *) &buf0;
	
	result = do_i2c_transfer(fwu, &msg, 1);
	
	if (result < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n",result);
		return result;
	}
	
	//CMD1
	msg.addr = 0x5B;
	msg.len = (payload_len+2);
	msg.flags = 0;
	msg.buf =(unsigned char *) &buf1;
	
	
	result = do_i2c_transfer(fwu, &msg, 1);
	
	if (result < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", result);
		return result;
	}
	
	//==
	// 1st read status
	//CS High
	msg.addr = 0x5B;
	msg.len = 1;
	msg.flags = 0;
	msg.buf = (unsigned char *) &buf0;
	
	result = do_i2c_transfer(fwu, &msg, 1);
	
	if (result < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", result);
		return result;
	}
	
	//CMD1 change to EFLASH_CMD_READ_STATUS
	//buf1[1] = EFLASH_CMD_READ_STATUS;
	msg.addr = 0x5B;
	msg.len = 2;
	msg.flags = 0;
	msg.buf =(unsigned char *) &cmd_status;
		
	//CMD1 Read Payload
	msg.addr = 0x5B;
	msg.len = 3;//payload_len;
	msg.flags = I2C_M_RD;
	//work_queue.msgs[4].buf = (unsigned char *) payload;
	msg.buf = (unsigned char *) &read_status;
		
	result = do_i2c_transfer(fwu, &msg, 1);
	
	if (result < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", result);
		return result;
	}

	result= read_status[2];
	return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//


static int i2ec_writebyte(struct ulpmc_fwu_info *fwu,unsigned int address,unsigned char data)
{
	int ret=0,i;
	struct i2c_msg msg;
	unsigned char buf0[3]={0x10};
	unsigned char buf2[2]={0x11};
	unsigned char buf3[1];
	
	buf0[1] = (address >> 8) & 0xFF; //addr[15:8]
	buf0[2] = (address) & 0xFF; //addr[7:0]

	msg.addr = 0x5b;
	msg.len = 3;
	msg.flags = 0;
	msg.buf = (unsigned char *) &buf0;
	
	ret = do_i2c_transfer(fwu, &msg, 1);
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed\n");
		return ret;
	}

	//I2EC WRITE BYTE DATA
	buf2[1]=data;
	msg.addr = 0x5b;
	msg.len = 2;
	msg.flags = 0;
	msg.buf = (unsigned char *) &buf2;
	
	ret = do_i2c_transfer(fwu, &msg, 1);
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed\n");
		return ret;
	}
	
	ret = buf3[0];
	
err:
	return ret;
}


static int i2ec_readbyte(struct ulpmc_fwu_info *fwu,unsigned int address)
{
	int ret=0,i;
	struct i2c_msg msg;
	unsigned char buf0[3]={0x10};

	unsigned char buf2[1]={0x11};
	unsigned char buf3[1];
	
	buf0[1] = (address >> 8) & 0xFF; //addr[15:8]
	buf0[2] = (address) & 0xFF; //addr[7:0]
	
	//printk("\n\raddress=%x buf0[1,2]=%x %x",address,buf0[1],buf0[2]);
	
	//I2EC ADDR WRITE
	msg.addr = 0x5b;
	msg.len = 3;
	msg.flags = 0;
	msg.buf = (unsigned char *) &buf0;
	
	ret = do_i2c_transfer(fwu, &msg, 1);
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed\n");
		return ret;
	}
	
	msg.addr = 0x5b;
	msg.len = 1;
	msg.flags = 0;
	msg.buf = (unsigned char *) &buf2;
	
	ret = do_i2c_transfer(fwu, &msg, 1);
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed\n");
		return ret;
	}
	
	msg.addr = 0x5b;
	msg.len = 1;
	msg.flags = I2C_M_RD;
	msg.buf = (unsigned char *) &buf3;
	
	ret = do_i2c_transfer(fwu, &msg, 1);
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed\n");
		return ret;
	}
	
	ret = buf3[0];
	
err:
	return ret;
}

static void do_reset(struct ulpmc_fwu_info *fwu)
{
	unsigned char tmp1;
	
	tmp1 = i2ec_readbyte(fwu,0x1F01);
	i2ec_writebyte(fwu,0x1F01,0x20);
	i2ec_writebyte(fwu,0x1F07,0x01);
	i2ec_writebyte(fwu,0x1F01,tmp1);
	
}
static void bSMBusSendByte(struct ulpmc_fwu_info *fwu, u8 addr,u8 val)
{
	int ret=0,i;
	struct i2c_msg msg;
	unsigned char buf0[1];           //CS High

	buf0[0]=val;
	msg.addr = addr;
	msg.len = 1;
	msg.flags = 0;
	msg.buf = (unsigned char *) &buf0;

	ret = do_i2c_transfer(fwu, &msg, 1);
	
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", ret);
		return ret;
	}	
}
static void bI2cWrite(struct ulpmc_fwu_info *fwu, u8 addr, u8 reg, u8 index)
{
	int ret=0,i;
	struct i2c_msg msg;
	u8 m_xbSMBusWriteBuff[7]={0,0,0,0,0,0,0};
	
	m_xbSMBusWriteBuff[0]=reg;
	for(i=1;i<=index;i++)
	{
		m_xbSMBusWriteBuff[i]=xbSMBusWriteBuff[i-1];
	}
	msg.addr = addr;
	msg.len = index+1;
	msg.flags = 0;
	msg.buf = (unsigned char *) &m_xbSMBusWriteBuff[0];

	ret = do_i2c_transfer(fwu, &msg, 1);
	
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", ret);
		return ret;
	}		
}
static void bI2cWriteToRead_WordCmd(struct ulpmc_fwu_info *fwu, u8 addr, u8 reg, u8 val, u8 index)
{
	int ret=0,i;
	struct i2c_msg msg;
	u8 m_xbSMBusWriteBuff[2]={0,0};
	
	m_xbSMBusWriteBuff[0]=reg;
	m_xbSMBusWriteBuff[1]=val;
	
	msg.addr = addr;
	msg.len = index+1;
	msg.flags = 0;
	msg.buf = (unsigned char *) &m_xbSMBusWriteBuff[0];

	ret = do_i2c_transfer(fwu, &msg, 1);
	
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", ret);
	}		
	
	m_xbSMBusWriteBuff[0]=reg;
	m_xbSMBusWriteBuff[1]=val;
	
	msg.addr = addr;
	msg.len = 2;
	msg.flags = I2C_M_RD;
	msg.buf = (unsigned char *) &m_xbSMBusWriteBuff[0];

	ret = do_i2c_transfer(fwu, &msg, 1);
	
	xbSMBusReadBuff[0]=m_xbSMBusWriteBuff[1];
	
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "i2c tx failed:%d\n", ret);
	}	
}

#define BIT0 (1)


#if 0

static int do_mass_erase(struct  ulpmc_fwu_info *fwu)
{
	printk("\n\rERASE.................start\r\n");
	cmd_write_enable(fwu);
	while((cmd_check_status(fwu) & 0x02)!=0x02);
	cmd_erase_all(fwu);
	while(cmd_check_status(fwu) & 0x01);
	cmd_write_disable(fwu);	
	printk("\n\rERASE.................End\r\n");
	return 0;
}

#else

static int do_mass_erase(struct ulpmc_fwu_info *fwu)
{
		u8 xbTmp0 = 0x00;
		u8 xbTmp1 = 0x00;
		u8 xbTmp2 = 0x00;
		u8 xbTmp3 = 0x00;
		u8 xbTmp7 = 0;
		
		while (xbTmp7 < 60) {//Add by emdoor jim.kuang 2014-05-09 for 60k
			bSMBusSendByte(fwu, 0x5B, 0x17);			// CS high

			// Write enable
			xbSMBusWriteBuff[0] = 0x06;
			bI2cWrite(fwu, 0x5B, 0x18, 1);

			// Sector erase
			xbSMBusWriteBuff[0] = 0xD7;
			xbSMBusWriteBuff[1] = xbTmp3;
			xbSMBusWriteBuff[2] = xbTmp2;
			xbSMBusWriteBuff[3] = xbTmp1;

			bSMBusSendByte(fwu, 0x5B, 0x17);		// CS high
			bI2cWrite(fwu, 0x5B, 0x18, 4);		// erase 1k

			xbTmp2 += 4;
			if (xbTmp2 == 0) {
				xbTmp3++;
			}

			xbSMBusReadBuff[0] = 0xFF;
			while (xbSMBusReadBuff[0] & BIT0) {
				// CS high
				bSMBusSendByte(fwu, 0x5B, 0x17);
				// Read status
				bI2cWriteToRead_WordCmd(fwu, 0x5B, 0x18, 0x05, 1);
			}

			xbTmp7++;
		}

		bSMBusSendByte(fwu,0x5B, 0x17);			// CS high

		// Write disable
		xbSMBusWriteBuff[0] = 0x04;
		bI2cWrite(fwu, 0x5B, 0x18, 1);

		bSMBusSendByte(fwu, 0x5B, 0x17);			// CS high
		return 0;
}

#endif

static int do_program(struct ulpmc_fwu_info *fwu,unsigned char *gBuffer)
{

	u8 *data_pntr=gBuffer;
	u32 index=0;

	u8 xbTmp0,xbTmp1,xbTmp2,xbTmp3;
	
	// CS high
	bSMBusSendByte(fwu, 0x5B, 0x17);
	
	// Write enable
	xbSMBusWriteBuff[0] = 0x06;
	bI2cWrite(fwu, 0x5B, 0x18, 1);
	
	// Programming
	xbSMBusWriteBuff[0] = 0xAD;
	xbSMBusWriteBuff[1] = 0x00;		// Addr H
	xbSMBusWriteBuff[2] = 0x00;		// Addr M
	xbSMBusWriteBuff[3] = 0x00;		// Addr L
	xbTmp3 = 0;
	xbSMBusWriteBuff[4] = *data_pntr;
	data_pntr++;
	xbSMBusWriteBuff[5] = *data_pntr;
	data_pntr++;
	index = 2;
	bSMBusSendByte(fwu, 0x5B, 0x17);		// CS high
	bI2cWrite(fwu, 0x5B, 0x18, 6);		// Programming

	// Wait busy
	xbSMBusReadBuff[0] = 0xFF;
	while (xbSMBusReadBuff[0] & BIT0) {
		// CS high
		bSMBusSendByte(fwu, 0x5B, 0x17);
		// Read status
		bI2cWriteToRead_WordCmd(fwu, 0x5B, 0x18, 0x05, 1);
	}

	xbTmp1 = xbSMBusWriteBuff[1];	// Addr H
	xbTmp2 = xbSMBusWriteBuff[2];	// Addr M
	xbTmp3 = xbSMBusWriteBuff[3];	// Addr L

	while ( index < fdata_len ) {
		
		bSMBusSendByte(fwu, 0x5B, 0x17);		// CS high
		
		xbSMBusWriteBuff[0] = 0xAD;
		xbSMBusWriteBuff[1] = *data_pntr;
		
		data_pntr++;
		
		xbSMBusWriteBuff[2] = *data_pntr;
		
		data_pntr++;
		
		index = index+2 ;
		
		bI2cWrite(fwu, 0x5B, 0x18, 3);		// Programming
		
		// Wait busy
		xbSMBusReadBuff[0] = 0xFF;
		while (xbSMBusReadBuff[0] & BIT0) {
			// CS high
			bSMBusSendByte(fwu, 0x5B, 0x17);
			// Read status
			bI2cWriteToRead_WordCmd(fwu, 0x5B, 0x18, 0x05, 1);
		}
		
	}

	bSMBusSendByte(fwu, 0x5B, 0x17);		// CS high

	// Write disable
	xbSMBusWriteBuff[0] = 0x04;
	bI2cWrite(fwu, 0x5B, 0x18, 1);

	bSMBusSendByte(fwu, 0x5B, 0x17);		// CS high
	xbTmp0 = 0;
	
	printk("\n\rProgram...............OK!\r\n");
	return 0;
}
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int parse_udata(struct ulpmc_fwu_info *fwu,
				u32 *addr, u8 *data, u16 *len)
{
	u16 i, j;
	u8 temp_buf[TEMP_BUFFER_LEN];
	u32 temp_addr = 0, temp_data = 0;
	u16 cur_datalen = 0;
	bool addr_flag;
	long val;

	if (fcur_idx >= fdata_len)
		goto len_mismatch_err;

	if (file_buf[fcur_idx] == '@')
		addr_flag = true;
	else
		addr_flag = false;

	for (i = fcur_idx;
		(i < fdata_len) && (cur_datalen < TX_DATA_BUFFER_SIZE); i++) {
		if (file_buf[i] == '@') {
			if (i != fcur_idx)
				break;
			i++;
			for (j = 0; j < TEMP_BUFFER_LEN; i++) {
				if (i >= fdata_len)
					goto len_mismatch_err;

				if (isxdigit(file_buf[i])) {
					temp_buf[j++] = file_buf[i];
				} else if (file_buf[i] == '\n') {
					temp_buf[j] = '\0';
					if (kstrtol(temp_buf, 16, &val))
						goto str_conv_failed;
					temp_addr = (u32)val;
					break;
				}
			}
		} else {

			if (i >= fdata_len ||
				cur_datalen >= TX_DATA_BUFFER_SIZE)
				break;

			if (isxdigit(file_buf[i])) {
				temp_buf[0] = file_buf[i++];
				temp_buf[1] = file_buf[i];
				temp_buf[2] = '\0';

				if (kstrtol(temp_buf, 16, &val))
					goto str_conv_failed;
				temp_data = (u32)val;
				data[cur_datalen++] = (u8)temp_data;
			}
		}
	}

	fcur_idx = i;
	*len = cur_datalen;

	if (addr_flag) {
		*addr = temp_addr;
		fcur_addr = *addr;
	} else {
		*addr = 0;
	}

	return 0;

str_conv_failed:
	dev_err(&fwu->pdev->dev, "kstrtol error:%s:\n", temp_buf);
	return -EINVAL;

len_mismatch_err:
	dev_err(&fwu->pdev->dev, "len mismatch:%s\n", __func__);
	return -EINVAL;
}

static int ulpmc_SW_por_reset(struct ulpmc_fwu_info *fwu)
{
	int ret;
	u8 cmd_buf[RX_TX_BUFFER_LEN], data[RX_TX_BUFFER_LEN];
	u16 cmd_len, data_len = 0;
	u32 dst_addr;
	struct i2c_msg msg;

	mdelay(100);

	memset(cmd_buf, 0x0, RX_TX_BUFFER_LEN);
	dst_addr = ULPMC_RST_PMMCTL0_REG;
	data[data_len++] = PMMCTL0_REG_VAL_LSB;
	data[data_len++] = PMMCTL0_REG_VAL_MSB;

	ret = fill_BSLCmd_packet(fwu, CMD_RX_DATA_BLOCK, dst_addr,
				data, data_len, cmd_buf, &cmd_len);
	if (ret < 0) {
		dev_err(&fwu->pdev->dev, "bsl failed:%d\n", ret);
		return ret;
	}
	msg.addr = 0x5b;
	msg.flags = 0;
	msg.buf = cmd_buf;
	msg.len = cmd_len;
	ret = do_i2c_transfer(fwu, &msg, 1);
	if (ret < 0)
		dev_err(&fwu->pdev->dev, "i2c tx failed\n");

	return ret;
}

static int start_fw_update(struct  ulpmc_fwu_info *fwu, u8 *data, u32 data_len)
{
	int ret;
	dev_info(&fwu->pdev->dev, ":%s\n", __func__);

	/* enter BSL mode */	
	ret = enter_bsl_mode(fwu);
	if (ret < 0)
		goto fwupdate_fail;	

	/* do mass erase */
	ret = do_mass_erase(fwu);
	
	if (ret < 0)
		goto fwupdate_fail;

	dev_info(&fwu->pdev->dev, "fw update successful\n");
	return 0;
	
fwupdate_fail:
	dev_err(&fwu->pdev->dev, "fw update failed\n");
	return ret;
}

static int dev_file_open(struct inode *i, struct file *f)
{
	struct	ulpmc_fwu_info *fwu = fwu_ptr;

	if (atomic_read(&fwu->fcount))
		return -EBUSY;

	atomic_inc(&fwu->fcount);
	return 0;
}

static int dev_file_close(struct inode *i, struct file *f)
{
	struct	ulpmc_fwu_info *fwu = fwu_ptr;

	atomic_dec(&fwu->fcount);
	return 0;
}

static ssize_t dev_file_write(struct file *f, const char __user *buf,
			size_t len, loff_t *off)
{
	struct	ulpmc_fwu_info *fwu = fwu_ptr;
	u8	*udata;
	int	ret = 0;

	mdelay(100);

	if (len <= 0)
		return -EINVAL;

	dev_info(&fwu->pdev->dev, "writing %d bytes..\n", len);

	udata = kzalloc(len, GFP_KERNEL);
	if (!udata) {
		dev_err(&fwu->pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	/* copy the data to kernel memory */
	if (copy_from_user(udata, buf, len)) {
		dev_err(&fwu->pdev->dev, "failed to copy usr data\n");
		ret = -EFAULT;
		goto fw_setup_failed;
	}

	/* set up the update process */
	ulpmc_fwupdate_enter();
	fwu->client = ulpmc_get_i2c_client();
	if (!fwu->client) {
		dev_err(&fwu->pdev->dev, "failed to get i2c client\n");
		ret = -EINVAL;
		goto fw_setup_failed;
	}

	/* init global data */
	file_buf = udata;
	fdata_len = len;
	fcur_idx = 0;
	fcur_addr = 0;
	
	/* start the fw update procedure */
	ret = start_fw_update(fwu, udata, len);
	do_program(fwu,udata);
	do_reset(fwu);
fw_setup_failed:
	kfree(udata);
	if (ret >= 0)
		ret = len;
	return ret;
}

static const struct file_operations ulpmc_fops = {
	.owner = THIS_MODULE,
	.open = &dev_file_open,
	.release = &dev_file_close,
	.write = &dev_file_write,
};

static int ulpmc_fwupdate_probe(struct platform_device *pdev)
{
	struct ulpmc_fwu_info *fwu;
	int ret = 0;

	fwu = kzalloc(sizeof(*fwu), GFP_KERNEL);
	if (!fwu) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	fwu->pdev = pdev;
	platform_set_drvdata(pdev, fwu);
	fwu_ptr = fwu;
	
	//Add by emdoor jim.kuang 2014-04-21
	ite_fwu_ptr=fwu;

	fwu->ulpmc_mdev.name = DRIVER_NAME;
	fwu->ulpmc_mdev.fops = &ulpmc_fops;
	fwu->ulpmc_mdev.minor = MISC_DYNAMIC_MINOR;
	ret = misc_register(&fwu->ulpmc_mdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "msic dev register failed\n");
		kfree(fwu);
		return ret;
	}

	dev_info(&pdev->dev, "probe done:\n");
	return 0;
}

static int ulpmc_fwupdate_remove(struct platform_device *pdev)
{
	struct ulpmc_fwu_info *fwu = dev_get_drvdata(&pdev->dev);

	misc_deregister(&fwu->ulpmc_mdev);
	kfree(fwu);
	return 0;
}

static struct platform_driver ulpmc_fwupdate_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = ulpmc_fwupdate_probe,
	.remove = ulpmc_fwupdate_remove,
};

/////////////////////////////////////////////////////////////////////////////////////////////////
//Add by emdoor to debug ite IC

static int ulpmc_write_reg16(struct i2c_client *client, u8 reg, u16 value)
{
	int ret, i;

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = i2c_smbus_write_word_data(client, reg, value);
		if (ret < 0)
			continue;
		else
			break;
	}

	if (ret < 0)
		dev_err(&client->dev, "I2C SMbus Write error:%d\n", ret);

	return ret;
}

static int ulpmc_read_reg16(struct i2c_client *client, u8 reg)
{
	int ret, i;

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = i2c_smbus_read_word_data(client, reg);
		if (ret < 0)
			continue;
		else
			break;
	}

	if (ret < 0)
		dev_err(&client->dev, "I2C SMbus Read error:%d\n", ret);

	return ret;
}

static int ulpmc_write_reg8(struct i2c_client *client, u8 reg, u8 value)
{
	int ret, i;

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = i2c_smbus_write_byte_data(client, reg, value);
		if (ret < 0)
			continue;
		else
			break;
	}

	if (ret < 0)
		dev_err(&client->dev, "I2C SMbus Write error:%d\n", ret);

	return ret;
}

static int ulpmc_read_reg8(struct i2c_client *client, u8 reg)
{
	int ret, i;

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret < 0)
			continue;
		else
			break;
	}

	if (ret < 0)
		dev_err(&client->dev, "I2C SMbus Read error:%d\n", ret);

	return ret;
}

static u8 ite_reg=0x0;
static u8 ite_val=0x0;
static u32 regstore_reg=0x0;
static u32 regstore_val=0x0;

static ssize_t ulpmc_value_store(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{	
	ite_reg = simple_strtoul(buf, NULL, 16);
	sprintf(buf,"ite_reg = 0x%x \r\n",ite_reg);
	return count;
}

static ssize_t ulpmc_value_show(struct class *class, 
			struct class_attribute *attr,	char *buf)
{
	struct i2c_msg msg;
	
	ite_fwu_ptr->client = ulpmc_get_i2c_client();
	
	if(!ite_fwu_ptr->client)
		return 0;
	
	ite_fwu_ptr->client->addr=0x76;
	ite_val = ulpmc_read_reg8(ite_fwu_ptr->client, ite_reg);
	
	return sprintf(buf,"ite_reg = 0x%x ite_val = 0x%x ite_fwu_ptr->client =0x%x \r\n" , ite_reg , ite_val,ite_fwu_ptr->client->addr );
}

static ssize_t regstore_value_store(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{	
	u32 tmp_reg=0;
	tmp_reg = simple_strtoul(buf, NULL, 16);
	
	regstore_reg=(tmp_reg>>8) & 0xff;
	regstore_val= tmp_reg & 0xff ;
	
	ite_fwu_ptr->client = ulpmc_get_i2c_client();
	
	if(!ite_fwu_ptr->client)
		return 0;
	
	ite_fwu_ptr->client->addr=0x76;
	ulpmc_write_reg8(ite_fwu_ptr->client,regstore_reg,regstore_val);
	
	sprintf(buf,"regstore_reg = 0x%x  regstore_val = 0x%x  ite_fwu_ptr->client = 0x%x \r\n",regstore_reg,regstore_val,ite_fwu_ptr->client->addr);
	
	return count;
}

static ssize_t regstore_value_show(struct class *class, 
			struct class_attribute *attr,	char *buf)
{
	struct i2c_msg msg;
	
	ite_fwu_ptr->client = ulpmc_get_i2c_client();
	
	if(!ite_fwu_ptr->client)
		return 0;
	
	ite_fwu_ptr->client->addr=0x76;
	ite_reg = (u8)regstore_reg;
	ite_val = ulpmc_read_reg8(ite_fwu_ptr->client,ite_reg );
	
	return sprintf(buf,"ite_reg = 0x%x ite_val = 0x%x  ite_fwu_ptr->client = 0x%x\r\n" , ite_reg , ite_val,ite_fwu_ptr->client->addr);
}

//ite burning mode reg reading
static u8 ite_burning_reg=0x0;
static u8 ite_burning_val=0x0;

static ssize_t burning_value_store(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{	
	u32 temp= 0;
	struct i2c_client	*burn_client;
	
	temp = simple_strtoul(buf,NULL,16);
	
	ite_burning_reg = ( temp >> 8 ) & 0xff ;
	ite_burning_val = ( temp ) & 0xff ;

	ite_fwu_ptr->client = ulpmc_get_i2c_client();
	
	if(!ite_fwu_ptr->client)
		return 0;
	
	burn_client =ite_fwu_ptr->client; 
	burn_client->addr=0x5b;
	
	ulpmc_write_reg8(burn_client , ite_burning_reg,ite_burning_val);
	
	return 	sprintf(buf,"ite_burning_reg  = 0x%x ite_burning_val = 0x%x client_addr=0x%x \r\n",ite_burning_reg,ite_burning_val,ite_fwu_ptr->client->addr);;
}

static ssize_t  burning_value_show(struct class *class, 
			struct class_attribute *attr,	char *buf)
{
	struct i2c_client	*burn_client;
	ite_fwu_ptr->client = ulpmc_get_i2c_client();
	
	if(!ite_fwu_ptr->client)
		return 0;
	
	burn_client =ite_fwu_ptr->client; 
	burn_client->addr=0x5b;
	
	ite_burning_val = ulpmc_read_reg8(burn_client , ite_burning_reg);
	
	return sprintf(buf,"ite_burning_reg = 0x%x ite_burning_val = 0x%x client= 0x%x \r\n" , ite_burning_reg , ite_burning_val,ite_fwu_ptr->client->addr);
}


static ssize_t reg18_value_store(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{	
	u32 temp= 0;
	struct i2c_client	*burn_client;
	
	temp = simple_strtoul(buf,NULL,16);
	
	ite_burning_reg = ( temp >> 8 ) & 0xff ;
	ite_burning_val = ( temp ) & 0xff ;

	ite_fwu_ptr->client = ulpmc_get_i2c_client();
	
	if(!ite_fwu_ptr->client)
		return 0;
	
	burn_client =ite_fwu_ptr->client; 
	burn_client->addr=0x5b;
	
	ulpmc_write_reg8(burn_client , ite_burning_reg,ite_burning_val);
	
	return 	sprintf(buf,"ite_burning_reg  = 0x%x ite_burning_val = 0x%x client_addr=0x%x \r\n",ite_burning_reg,ite_burning_val,ite_fwu_ptr->client->addr);
}


static ssize_t	reg18_value_show(struct class *class, 
				struct class_attribute *attr,	char *buf)
{

	int ret;
	u8 wbuf[TEMP_BUFFER_LEN];
	u16 idx = 0;
	struct i2c_msg msg;

	//dev_info(&fwu->pdev->dev, ":%s\n", __func__);
	
	ite_fwu_ptr->client = ulpmc_get_i2c_client();
	
	memset(wbuf, 0x0, TEMP_BUFFER_LEN);
	msg.addr = 0x5b;
	msg.flags = I2C_M_RD;
	wbuf[idx] = 0x18;
	msg.buf = &wbuf[idx];
	msg.len = 3;
	ret = do_i2c_transfer(ite_fwu_ptr, &msg, 1);
	return 	sprintf(buf,"wbuf[0]  = 0x%x wbuf[1] = 0x%x wbuf[2]=0x%x \r\n",wbuf[0],wbuf[1],wbuf[2]);
}


static struct class_attribute ulpmc_class_attrs[] = {
	__ATTR(ulpmc,0644 ,ulpmc_value_show ,ulpmc_value_store),		
	__ATTR(regstore,0644 ,regstore_value_show ,regstore_value_store),
	__ATTR(burning,0644 ,burning_value_show ,burning_value_store),		
	__ATTR(reg18,0644 ,reg18_value_show ,reg18_value_store),	
    __ATTR_NULL
};

static struct class ulpmc_interface_class = {
        .name = "ulpmc_interface",
        .class_attrs = ulpmc_class_attrs,
    };

//
/////////////////////////////////////////////////////////////////////////////////////////////////




static int __init ulpmc_fwu_init(void)
{
	int err;
	int ret = 0;

	err = platform_driver_register(&ulpmc_fwupdate_driver);
	if (err < 0) {
		err = -ENOMEM;
		pr_err("Driver registartion failed\n");
		goto exit;
	}

	pdev = platform_device_alloc(DRIVER_NAME, 0);
	if (!pdev) {
		err = -ENOMEM;
		pr_err("Device allocation failed\n");
		goto exit;
	}

	err = platform_device_add(pdev);
	if (err) {
		pr_err("Device addition failed (%d)\n", err);
		goto exit;
	}

	//register usr space
	ret = class_register(&ulpmc_interface_class);
	if (ret) {
   		printk("%s failed!",__func__);
	}	
exit:
	return err;
}
fs_initcall(ulpmc_fwu_init);

static void __exit ulpmc_fwu_exit(void)
{
	platform_device_unregister(pdev);
	platform_driver_unregister(&ulpmc_fwupdate_driver);
	class_unregister(&ulpmc_interface_class);
}
module_exit(ulpmc_fwu_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_DESCRIPTION("BYT ULPMC FW Update driver");
MODULE_LICENSE("GPL");
