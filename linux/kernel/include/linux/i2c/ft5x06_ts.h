#ifndef __LINUX_FT5X0X_TS_H__
#define __LINUX_FT5X0X_TS_H__

/* -- dirver configure -- */


#define PRESS_MAX	0xFF
#define FT_PRESS	0x08

#define FT5X0X_NAME	"ft5x06"


#define FT_MAX_ID	0x0F



/*register address*/
#define FT5x0x_REG_FW_VER		0xA6
#define FT5x0x_REG_POINT_RATE	0x88
#define FT5X0X_REG_THGROUP	0x80

#define FT5X0X_ENABLE_IRQ	1
#define FT5X0X_DISABLE_IRQ	0

int ft5x0x_i2c_Read(struct i2c_client *client, char *writebuf, int writelen,
		    char *readbuf, int readlen);
int ft5x0x_i2c_Write(struct i2c_client *client, char *writebuf, int writelen);

void ft5x0x_reset_tp(int HighOrLow);

void ft5x0x_Enable_IRQ(struct i2c_client *client, int enable);

/* The platform data for the Focaltech ft5x0x touchscreen driver */
struct ft5x0x_platform_data {
	unsigned int x_max;
	unsigned int y_max;
	unsigned long irqflags;	/*default:IRQF_TRIGGER_FALLING*/
	unsigned int irq;
	unsigned int reset;
};

#endif
