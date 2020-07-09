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

#ifndef __ILITEK_TS_H__
#define __ILITEK_TS_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/rtc.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/pm_runtime.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#endif

//driver information
#define DERVER_VERSION_MAJOR 		5
#define DERVER_VERSION_MINOR 		0
#define CUSTOMER_ID 				0
#define MODULE_ID					0
#define PLATFORM_ID					0
#define PLATFORM_MODULE				0
#define ENGINEER_ID					0

#define ILITEK_PLAT_QCOM												1
#define ILITEK_PLAT_MTK													2
#define ILITEK_PLAT_ROCKCHIP											3
#define ILITEK_PLAT_ALLWIN												4
#define ILITEK_PLAT_AMLOGIC												5

#define ILITEK_PLAT														ILITEK_PLAT_QCOM


#define ILITEK_TOOL

//#define ILITEK_GLOVE

//#define ILITEK_CHARGER_DETECTION
#define POWER_SUPPLY_BATTERY_STATUS_PATCH  "/sys/class/power_supply/battery/status"

//#define ILITEK_ESD_PROTECTION

//#define ILITEK_TOUCH_PROTOCOL_B
#define ILITEK_REPORT_PRESSURE

#define ILITEK_USE_LCM_RESOLUTION

//#define ILITEK_REPORT_KEY_WITH_COORDINATE
#define ILITEK_ROTATE_FLAG												0
#define ILITEK_REVERT_X													0
#define ILITEK_REVERT_Y													0
#define TOUCH_SCREEN_X_MAX   											(1920)  //LCD_WIDTH
#define TOUCH_SCREEN_Y_MAX   											(1080) //LCD_HEIGHT

#define ILITEK_ENABLE_REGULATOR_POWER_ON
#define ILITEK_GET_GPIO_NUM


#define ILITEK_CLICK_WAKEUP												0
#define ILITEK_DOUBLE_CLICK_WAKEUP										1
#define ILITEK_GESTURE_WAKEUP											2
//#define ILITEK_GESTURE													ILITEK_CLICK_WAKEUP

//#define ILITEK_UPDATE_FW
#define ILI_UPDATE_BY_CHECK_INT

#define ILITEK_TS_NAME													"ilitek_ts"
#define ILITEK_TUNING_MESSAGE											0xDB
#define ILITEK_TP_CMD_READ_MODE											0xC0
#define ILITEK_TP_MODE_APPLICATION										0x5A
#define ILITEK_TP_MODE_BOOTLOADER										0x55
#define ILITEK_TP_CMD_WRITE_ENABLE										0xC4
#define ILITEK_TP_CMD_WRITE_DATA										0xC3
#define ILITEK_TP_CMD_SLEEP												0x30
#define ILITEK_TP_CMD_GET_FIRMWARE_VERSION								0x40
#define ILITEK_TP_CMD_GET_PROTOCOL_VERSION								0x42
#define ILITEK_TP_CMD_GET_KERNEL_VERSION								0x61
#define ILITEK_TP_CMD_GET_TP_RESOLUTION									0x20
#define ILITEK_TP_CMD_GET_SCREEN_RESOLUTION								0x21
#define ILITEK_TP_CMD_GET_KEY_INFORMATION								0x22
#define ILITEK_TP_CMD_GET_TOUCH_INFORMATION								0x10

#define ILITEK_UPGRADE_WITH_BIN											0
#define ILITEK_FW_FILENAME												"ilitek_i2c.bin"

#define AP_STARTADDR													0x00
#define AP_ENDADDR														0xDFFF
#define UPGRADE_TRANSMIT_LEN											256
#define FW_VERSION1														0xD100
#define FW_VERSION2														0xD101
#define FW_VERSION3														0xD102

#define PRODUCT_ID_STARTADDR											0xE000
#define PRODUCT_ID_ENDADDR												0xE006

#define SECTOR_SIZE														0x1000
#define SECTOR_ENDADDR													0xD000

#define REG_LEN															4
#define REG_START_DATA													0x25

#define ENTER_ICE_MODE													0x181062
#define ENTER_ICE_MODE_NO_DATA											0x0

#define EXIT_ICE_MODE													0x1810621B

#define REG_FLASH_CMD													0x041000
#define REG_FLASH_CMD_DATA_ERASE										0x20
#define REG_FLASH_CMD_DATA_PROGRAMME									0x02
#define REG_FLASH_CMD_READ_FLASH_STATUS									0x5
#define REG_FLASH_CMD_WRITE_ENABLE										0x6
#define REG_FLASH_CMD_MEMORY_READ										0x3B
#define REG_FLASH_CMD_RELEASE_FROM_POWER_DOWN							0xab

#define REG_PGM_NUM														0x041004
#define REG_PGM_NUM_TRIGGER_KEY											0x66aa5500
#define REG_PGM_LEN														(0x66aa5500 + UPGRADE_TRANSMIT_LEN - 1)

#define REG_READ_NUM													0x041009
#define REG_READ_NUM_1													0x0

#define REG_CHK_EN														0x04100B
#define REG_CHK_EN_PARTIAL_READ 										0x3

#define REG_TIMING_SET													0x04100d
#define REG_TIMING_SET_10MS												0x00

#define REG_CHK_FLAG													0x041011
#define FLASH_READ_DATA													0x041012
#define FLASH_STATUS													0x041013
#define REG_PGM_DATA													0x041020

#define WDTRLDT                            								0x5200C
#define WDTRLDT_CLOSE                            						0
#define WDTCNT1                     									0x52020
#define WDTCNT1_OPEN                     								1
#define WDTCNT1_CLOSE                     								0

#define CLOSE_10K_WDT1													0x42000
#define CLOSE_10K_WDT1_VALUE											0x0f154900

#define CLOSE_10K_WDT2													0x42014
#define CLOSE_10K_WDT2_VALUE											0x02

#define CLOSE_10K_WDT3													0x42000
#define CLOSE_10K_WDT3_VALUE											0x00000000

#define DATA_SHIFT_0													0x000000FF
#define DATA_SHIFT_8													0x0000FF00
#define DATA_SHIFT_16													0x00FF0000
#define DATA_SHIFT_24													0xFF000000


#define ILITEK_TP_CMD_READ_DATA_CONTROL_2120							0xF6
#define ILITEK_TP_CMD_GET_FIRMWARE_VERSION_2120							0x21
#define ILITEK_TP_CMD_GET_PROTOCOL_VERSION_2120							0x22
#define ILITEK_TP_CMD_GET_KEY_INFORMATION_2120							0x27
#define ILITEK_TP_CMD_SLEEP_2120										0x02


#if ILITEK_PLAT == ILITEK_PLAT_MTK
#define ILITEK_USE_MTK_INPUT_DEV
#if defined ILITEK_GET_GPIO_NUM
#undef ILITEK_GET_GPIO_NUM
#endif

#include <linux/sched.h>
#include <linux/kthread.h>
//#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>

#include <linux/namei.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/module.h>


#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>

#ifdef CONFIG_MTK_BOOT
#include "mt_boot_common.h"
#endif


#include "tpd.h"
#endif

#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
#include <linux/irq.h>
#include <linux/init-input.h>
#include <linux/pm.h>
#include <linux/gpio.h>
extern struct ctp_config_info config_info;


#endif


#ifndef ILITEK_GET_GPIO_NUM
//must set
#if ILITEK_PLAT == ILITEK_PLAT_MTK
#define ILITEK_IRQ_GPIO													GTP_INT_PORT
#define ILITEK_RESET_GPIO												GTP_RST_PORT
#elif ILITEK_PLAT == ILITEK_PLAT_ALLWIN
#define ILITEK_IRQ_GPIO													(config_info.int_number)
#define ILITEK_RESET_GPIO												(config_info.wakeup_gpio.gpio)
#else
#define ILITEK_IRQ_GPIO													9
#define ILITEK_RESET_GPIO												10
#endif
#endif

#ifdef ILITEK_TUNING_MESSAGE
#include <linux/kernel.h> 
#include <linux/init.h> 
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <linux/module.h>  
#include <linux/netlink.h>  
#include <linux/sched.h>  
#include <net/sock.h>  
#include <linux/proc_fs.h>  
#include <linux/netfilter.h>  
#include <linux/netfilter_ipv4.h>  
#include <linux/ip.h>  
#include <linux/tcp.h>  
#include <linux/icmp.h>  
#include <linux/udp.h>
#endif

#define ILITEK_TP_UPGRADE_FAIL											(-5)
#define ILITEK_I2C_TRANSFER_ERR											(-4)
#define ILITEK_TP_CHANGETOBL_ERR										(-3)
#define ILITEK_TP_CHANGETOAP_ERR										(-2)

#define ILITEK_ERR_LOG_LEVEL 											(1)   
#define ILITEK_INFO_LOG_LEVEL 											(3)   
#define ILITEK_DEBUG_LOG_LEVEL 											(4)  
#define ILITEK_DEFAULT_LOG_LEVEL										(3)   
/*debug*/
//#define ILITEK_DEFAULT_LOG_LEVEL										(4) 

#define debug_level(level, fmt, arg...) do {\
	if (level <= ilitek_log_level_value) {\
		if (level == ILITEK_ERR_LOG_LEVEL) {\
			printk(" %s ERR  line = %d %s : "fmt, "ILITEK", __LINE__, __func__, ##arg);\
		}\
		else if (level == ILITEK_INFO_LOG_LEVEL) {\
			printk(" %s INFO line = %d %s : "fmt, "ILITEK", __LINE__, __func__, ##arg);\
		}\
		else if (level == ILITEK_DEBUG_LOG_LEVEL) {\
			printk(" %s DEBUG line = %d %s : "fmt, "ILITEK", __LINE__, __func__, ##arg);\
		}\
	}\
} while (0)

#define tp_log_err(fmt, arg...) debug_level(ILITEK_ERR_LOG_LEVEL, fmt, ##arg) 
#define tp_log_info(fmt, arg...) debug_level(ILITEK_INFO_LOG_LEVEL, fmt, ##arg) 
#define tp_log_debug(fmt, arg...) debug_level(ILITEK_DEBUG_LOG_LEVEL, fmt, ##arg) 


struct ilitek_key_info {
	int id;
	int x;
	int y;
	int status;
	int flag;
};

struct ilitek_ts_data {
    struct i2c_client *client;
	struct input_dev *input_dev;
	struct regulator *vdd;
	struct regulator *vdd_i2c;
	struct regulator *vcc_io;
	int irq_gpio;
	int reset_gpio;
	bool ic_2120;
	bool system_suspend;
	unsigned char firmware_ver[8];
	unsigned char mcu_ver[8];
	unsigned char bl_ver[8];
	int protocol_ver;
	int tp_max_x;
	int tp_max_y;
	int tp_min_x;
	int tp_min_y;
	int screen_max_x;
	int screen_max_y;
	int screen_min_x;
	int screen_min_y;
	int max_tp;
	int max_btn;
	int x_ch;
	int y_ch;
	int keycount;
	int key_xlen;
	int key_ylen;
	struct ilitek_key_info keyinfo[10];
	bool irq_status;
	bool irq_trigger;
	struct task_struct * irq_thread;
	spinlock_t irq_lock;
	bool is_touched;
	bool touch_key_hold_press;
	int touch_flag[10];
	
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
//#ifdef ILITEK_UPDATE_FW
	bool force_update;
	bool has_df;
	int page_number;
	struct task_struct * update_thread;
//#endif
	bool firmware_updating;
	bool operation_protection;

#ifdef ILITEK_GESTURE
	bool enable_gesture;
#endif

#ifdef ILITEK_GLOVE
	bool enable_glove;
#endif

#ifdef ILITEK_CHARGER_DETECTION
	struct workqueue_struct *charge_wq;
	struct delayed_work charge_work;
	bool charge_check;
	unsigned long charge_delay;
#endif

#ifdef ILITEK_ESD_PROTECTION
	 struct workqueue_struct *esd_wq;
	 struct delayed_work esd_work;
	 bool esd_check;
	 unsigned long esd_delay;
#endif

	struct kobject * ilitek_func_kobj;
	struct mutex ilitek_mutex;

};

#define ILITEK_I2C_RETRY_COUNT											3

extern int ilitek_log_level_value;
extern char ilitek_driver_information[];
extern struct ilitek_ts_data * ilitek_data;
extern void ilitek_resume(void);
extern void ilitek_suspend(void);

extern int ilitek_main_probe(struct ilitek_ts_data * ilitek_data);
extern int ilitek_main_remove(struct ilitek_ts_data * ilitek_data);
extern void ilitek_reset(int delay);
extern int ilitek_poll_int(void);
extern int ilitek_i2c_write(uint8_t * cmd, int length);
extern int ilitek_i2c_read(uint8_t *data, int length);
extern int ilitek_i2c_write_and_read(uint8_t *cmd,
			int write_len, int delay, uint8_t *data, int read_len);

extern void ilitek_irq_enable(void);
extern void ilitek_irq_disable(void);
extern int ilitek_read_tp_info(void);

#ifdef ILITEK_UPDATE_FW
extern int ilitek_upgrade_firmware(void);
#endif
extern int ilitek_upgrade_bigger_size_ic(unsigned int df_startaddr, unsigned int df_endaddr, unsigned int df_checksum,
	unsigned int ap_startaddr, unsigned int ap_endaddr, unsigned int ap_checksum, unsigned char * CTPM_FW);
extern int ilitek_upgrade_2120(unsigned char * CTPM_FW);

#ifdef ILITEK_TUNING_MESSAGE
extern bool ilitek_debug_flag;
#endif
#ifdef ILITEK_TOOL
extern int ilitek_create_tool_node(void);
extern int ilitek_remove_tool_node(void);
#endif
#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
extern int ilitek_suspend_allwin(struct i2c_client *client, pm_message_t mesg);
extern int ilitek_resume_allwin(struct i2c_client *client);
#endif

#endif
