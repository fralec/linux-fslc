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

#include <uapi/linux/sched/types.h>

#include "ilitek_ts.h"


char ilitek_driver_information[] = {DERVER_VERSION_MAJOR, DERVER_VERSION_MINOR, CUSTOMER_ID, MODULE_ID, PLATFORM_ID, PLATFORM_MODULE, ENGINEER_ID};
int ilitek_log_level_value = ILITEK_DEFAULT_LOG_LEVEL;
static bool ilitek_repeat_start = true;
static bool ilitek_exit_report = false;

#if ILITEK_PLAT == ILITEK_PLAT_MTK
extern struct tpd_device *tpd;
#endif

#ifdef ILITEK_TUNING_MESSAGE
static struct sock * ilitek_netlink_sock;
bool ilitek_debug_flag = false;
static void ilitek_udp_reply(int pid,int seq,void *payload,int size)
{
	struct sk_buff	*skb;
	struct nlmsghdr	*nlh;
	int		len = NLMSG_SPACE(size);
	void		*data;
	int ret;

	tp_log_info("udp_reply\n");
	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb) {
		tp_log_info("alloc skb error\n");
		return;
	}
	//tp_log_info("ilitek udp_reply\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
	nlh= nlmsg_put(skb, pid, seq, 0, size, 0);
#else
	nlh= NLMSG_PUT(skb, pid, seq, 0, size);
#endif
	nlh->nlmsg_flags = 0;
	data=NLMSG_DATA(nlh);
	memcpy(data, payload, size);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
	NETLINK_CB(skb).portid = 0;		  /* from kernel */
#else
	NETLINK_CB(skb).pid = 0;		 /* from kernel */
#endif
	NETLINK_CB(skb).dst_group = 0;  /* unicast */
	ret=netlink_unicast(ilitek_netlink_sock, skb, pid, MSG_DONTWAIT);
	if (ret <0) {
		tp_log_err("ilitek send failed\n");
		return;
	}
	return;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
	nlmsg_failure:			/* Used by NLMSG_PUT */
		if (skb) {
			kfree_skb(skb);
		}
#endif
}

/* Receive messages from netlink socket. */
static u_int ilitek_pid = 100, ilitek_seq = 23/*, sid*/;
static void udp_receive(struct sk_buff  *skb)
{
  	int count = 0, ret = 0, i = 0;
	uint8_t * data;
	struct nlmsghdr *nlh;
	nlh = (struct nlmsghdr *)skb->data;
	ilitek_pid  = 100;//NETLINK_CREDS(skb)->pid;
	//uid  = NETLINK_CREDS(skb)->uid;
	//sid  = NETLINK_CB(skb).sid;
	ilitek_seq  = 23;//nlh->nlmsg_seq;
	data = (uint8_t *)NLMSG_DATA(nlh);
	count = nlmsg_len(nlh);
	if(!strcmp(data,"Open!")) {
		tp_log_info("data is :%s\n",(char *)data);
		ilitek_data->operation_protection = true;
		ilitek_udp_reply(ilitek_pid, ilitek_seq, data, sizeof("Open!"));
	}
	else if(!strcmp(data,"Close!")) {
		tp_log_info("data is :%s\n",(char *)data);
		ilitek_data->operation_protection = false;
	}
	tp_log_debug("count = %d  data[count -3] = %d data[count -2] = %c\n", count, data[count -3], data[count -2]);
	for (i = 0; i < count; i++) {
		//tp_log_info("data[%d] = 0x%x\n", i, data[i]);
	}
	if (data[count -2] == 'I' && (count == 20 || count == 52) && data[0] == 0x77 && data[1] == 0x77) {
		
		tp_log_debug("IOCTL_WRITE CMD = %d\n", data[2]);
		switch (data[2]) {
			case 13:
				//ilitek_irq_enable();
				tp_log_info("ilitek_irq_enable. do nothing\n");
				break;
			case 12:
				//ilitek_irq_disable();
				tp_log_info("ilitek_irq_disable. do nothing\n");
				break;
			case 19:
				ilitek_reset(200);
				break;
#ifdef ILITEK_TUNING_MESSAGE
			case 21:
				tp_log_info("ilitek The ilitek_debug_flag = %d.\n", data[3]);
				if (data[3] == 0) {
					ilitek_debug_flag = false;
				}
				else if (data[3] == 1) {
					ilitek_debug_flag = true;
				}
				break;
#endif
			case 15:
				if (data[3] == 0) {
					ilitek_irq_disable();
					tp_log_debug("ilitek_irq_disable.\n");
				}
				else {
					ilitek_irq_enable();
					tp_log_debug("ilitek_irq_enable.\n");
				}
				break;
			case 16:
				ilitek_data->operation_protection = data[3];
				tp_log_info("ilitek_data->operation_protection = %d\n", ilitek_data->operation_protection);
				break;
			case 8:
				tp_log_info("get driver version\n");
				ilitek_udp_reply(ilitek_pid, ilitek_seq, ilitek_driver_information, 7);
				break;
			case 18:
				tp_log_debug("firmware update write 33 bytes data\n");
				ret = ilitek_i2c_write(&data[3], 33);
				if (ret < 0) {
					tp_log_err("i2c write error, ret %d, addr %x \n", ret,ilitek_data->client->addr);
				}
				if (ret < 0) {
					data[0] = 1;
				}
				else {
					data[0] = 0;
				}
				ilitek_udp_reply(ilitek_pid, ilitek_seq, data, 1);
				return;
				break;
				default:
					return;
		}
	}
	else if (data[count -2] == 'W') {
		ret = ilitek_i2c_write(data, count -2);
		if(ret < 0){
			tp_log_err("i2c write error, ret %d, addr %x \n", ret,ilitek_data->client->addr);
		}
		if (ret < 0) {
			data[0] = 1;
		}
		else {
			data[0] = 0;
		}
		ilitek_udp_reply(ilitek_pid, ilitek_seq, data, 1);
	}
	else if (data[count -2] == 'R') {
		ret = ilitek_i2c_read(data, count - 2);
		if(ret < 0){
			tp_log_err("i2c read error, ret %d, addr %x \n", ret,ilitek_data->client->addr);
		}
		if (ret < 0) {
			data[count - 2] = 1;
		}
		else {
			data[count - 2] = 0;
		}
		ilitek_udp_reply(ilitek_pid, ilitek_seq, data, count - 1);
	}
	return ;
}
#endif

#ifdef ILITEK_GESTURE
static ssize_t ilitek_gesture_show(struct device *dev,
	struct device_attribute *attr, char *buf) {
	if (ilitek_data->enable_gesture) {
		return sprintf(buf, "gesture: on\n");
	}
	else {
		return sprintf(buf, "gesture: off\n");
	}
}
static ssize_t ilitek_gesture_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size) {
	if (buf[0]) {
		ilitek_data->enable_gesture = true;
	}
	else {
		ilitek_data->enable_gesture = false;
	}
	return size;
}
static DEVICE_ATTR(gesture, 0664, ilitek_gesture_show, ilitek_gesture_store);
#endif

#ifdef ILITEK_GLOVE
static int ilitek_into_glovemode(bool glovemode) {
	int ret = 0;
	uint8_t cmd[2] = {0};
	tp_log_info("enter....... glovemode = %d\n", glovemode);
	cmd[0] = 0x06;
	if (glovemode) {
		cmd[1] = 0x01;
	}
	else {
		cmd[1] = 0x00;
	}
	mutex_lock(&ilitek_data->ilitek_mutex);
	ret = ilitek_i2c_write(cmd, 2);
	mutex_unlock(&ilitek_data->ilitek_mutex);
	if(ret < 0){
		tp_log_err("ilitek_i2c_write err,ilitek_into_glovemode %d err ret %d\n", glovemode, ret);
		return ret;
	}
	return 0;
}

static ssize_t ilitek_glove_show(struct device *dev,
	struct device_attribute *attr, char *buf) {
	if (ilitek_data->enable_glove) {
		return sprintf(buf, "glove: on\n");
	}
	else {
		return sprintf(buf, "glove: off\n");
	}
}

static ssize_t ilitek_glove_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size) {
	if (buf[0]) {
		ilitek_data->enable_glove = true;
	}
	else {
		ilitek_data->enable_glove = false;
	}
	ilitek_into_glovemode(ilitek_data->enable_glove);
	return size;
}
static DEVICE_ATTR(glove, 0664, ilitek_glove_show, ilitek_glove_store);
#endif

static ssize_t ilitek_firmware_version_show(struct device *dev,
	struct device_attribute *attr, char *buf) {
	int ret = 0;
	tp_log_info("\n");
	ilitek_irq_disable();
	ret = ilitek_read_tp_info();
	ilitek_irq_enable();
	if (ret < 0) {
		tp_log_err("ilitek_read_tp_info err ret = %d\n", ret);
		return sprintf(buf, "ilitek firmware version read error ret = %d\n", ret);
	}
	else {
		return sprintf(buf, "ilitek firmware version is %d.%d.%d.%d.%d.%d.%d.%d\n", ilitek_data->firmware_ver[0], ilitek_data->firmware_ver[1],
			ilitek_data->firmware_ver[2], ilitek_data->firmware_ver[3], ilitek_data->firmware_ver[4], ilitek_data->firmware_ver[5],
			ilitek_data->firmware_ver[6], ilitek_data->firmware_ver[7]);
	}
}

static DEVICE_ATTR(firmware_version, 0664, ilitek_firmware_version_show, NULL);

static struct attribute *ilitek_sysfs_attrs_ctrl[] = {
	&dev_attr_firmware_version.attr,
#ifdef ILITEK_GESTURE
	&dev_attr_gesture.attr,
#endif
#ifdef ILITEK_GLOVE
	&dev_attr_glove.attr,
#endif
	NULL
};
static struct attribute_group ilitek_attribute_group[] = {
	{.attrs = ilitek_sysfs_attrs_ctrl },
};




#ifdef ILITEK_CHARGER_DETECTION
static void ilitek_read_file(char *pFilePath, u8 *pBuf, u16 nLength) {
	struct file *pFile = NULL;
	mm_segment_t old_fs;
	ssize_t nReadBytes = 0;

	old_fs = get_fs();
	set_fs(get_ds());

	pFile = filp_open(pFilePath, O_RDONLY, 0);
	if (IS_ERR(pFile)) {
		tp_log_err("Open file failed: %s\n", pFilePath);
		return;
	}

	pFile->f_op->llseek(pFile, 0, SEEK_SET);
	nReadBytes = pFile->f_op->read(pFile, pBuf, nLength, &pFile->f_pos);
	tp_log_info("Read %d bytes!\n", (int)nReadBytes);

	set_fs(old_fs);
	filp_close(pFile, NULL);
	return;
}

static int ilitek_into_chargemode(bool chargemode) {
	int ret = 0;
	uint8_t cmd[2] = {0};
	tp_log_info("enter....... chargemode = %d\n", chargemode);
	cmd[0] = 0xBE;
	if (chargemode) {
		cmd[1] = 0x01;
	}
	else {
		cmd[1] = 0x00;
	}
	mutex_lock(&ilitek_data->ilitek_mutex);
	ret = ilitek_i2c_write(cmd, 2);
	mutex_unlock(&ilitek_data->ilitek_mutex);
	if(ret < 0){
		tp_log_err("ilitek_i2c_write err,ilitek_into_chargemode %d err ret %d\n", chargemode, ret);
		return ret;
	}
	return 0;
}

static void ilitek_charge_check(struct work_struct *work) {
	static int charge_mode = 0; 
    u8 ChargerStatus[20] = {0};
	tp_log_info("enter.......\n");
	if(ilitek_data->operation_protection){
		tp_log_info("ilitek charger ilitek_data->operation_protection is true SO not check\n");
		goto ilitek_charge_check_out;
	}
	if(ilitek_data->charge_check){
		ilitek_read_file(POWER_SUPPLY_BATTERY_STATUS_PATCH, ChargerStatus, 20);
		tp_log_info("Battery Status : %s\n", ChargerStatus);
		if (strstr(ChargerStatus, "Charging") != NULL || strstr(ChargerStatus, "Full") != NULL || strstr(ChargerStatus, "Fully charged") != NULL) {
			if (charge_mode != 1) {
				ilitek_into_chargemode(true); // charger plug-in
				charge_mode = 1;
			}
		}
		else { // Not charging
			if (charge_mode != 2) {
				ilitek_into_chargemode(false); // charger plug-out
				charge_mode = 2;
			}
		}
	}
	else{
		tp_log_info("charger not need check ilitek_data->esd_check is false!!!\n");
		goto ilitek_charge_check_out;
	}
	
ilitek_charge_check_out:
	ilitek_data->charge_check = true;
	queue_delayed_work(ilitek_data->charge_wq, &ilitek_data->charge_work, ilitek_data->charge_delay);
	return;
}
#endif

#ifdef ILITEK_ESD_PROTECTION
static void ilitek_esd_check(struct work_struct *work) {
	int i = 0;	
	unsigned char buf[4]={0};
	tp_log_info("enter.......\n");
	if(ilitek_data->operation_protection){
		tp_log_info("ilitek esd ilitek_data->operation_protection is true so not check\n");
		goto ilitek_esd_check_out;
	}
	mutex_lock(&ilitek_data->ilitek_mutex);
	buf[0] = ILITEK_TP_CMD_GET_PROTOCOL_VERSION;
	if(ilitek_data->esd_check){
		for (i = 0; i < 3; i++) {
			if(ilitek_i2c_write_and_read(buf, 1, 0, buf, 2) < 0){
				tp_log_err("ilitek esd  i2c communication error \n");
				if ( i == 2) {
					tp_log_err("esd i2c communication failed three times reset now\n");
					break;
				}
			}
			else {
				if (buf[0] == 0x03) {
					tp_log_info("esd ilitek_ts_send_cmd successful, response ok\n");
						goto ilitek_esd_check_out;
				}
				else {
					tp_log_err("esd ilitek_ts_send_cmd successful, response failed\n");
					if ( i == 2) {
						tp_log_err("esd ilitek_ts_send_cmd successful, response failed three times reset now\n");
						break;
					}
				}
			}
		}
	}
	else{
		tp_log_info("esd not need check ilitek_data->esd_check is false!!!\n");
		goto ilitek_esd_check_out;
	}
	
	ilitek_reset(200);
ilitek_esd_check_out:	
	mutex_unlock(&ilitek_data->ilitek_mutex);
	ilitek_data->esd_check = true;
	queue_delayed_work(ilitek_data->esd_wq, &ilitek_data->esd_work, ilitek_data->esd_delay);
	return;
}
#endif

static DECLARE_WAIT_QUEUE_HEAD(waiter);
void ilitek_irq_enable(void) {
    unsigned long irqflag = 0;
	spin_lock_irqsave(&ilitek_data->irq_lock, irqflag);
	if (!(ilitek_data->irq_status)) {
        enable_irq(ilitek_data->client->irq);
		ilitek_data->irq_status = true;
		tp_log_debug("\n");
	}
	spin_unlock_irqrestore(&ilitek_data->irq_lock, irqflag);
}

void ilitek_irq_disable(void) {
    unsigned long irqflag = 0;
	spin_lock_irqsave(&ilitek_data->irq_lock, irqflag);
	if ((ilitek_data->irq_status)) {
        disable_irq(ilitek_data->client->irq);
		ilitek_data->irq_status = false;
		tp_log_info("\n");
	}
	spin_unlock_irqrestore(&ilitek_data->irq_lock, irqflag);
}

int ilitek_i2c_transfer(struct i2c_msg *msgs, int cnt)
{
	int ret = 0;
	struct i2c_client * client = ilitek_data->client;
	int count=ILITEK_I2C_RETRY_COUNT;
#if ILITEK_PLAT == ILITEK_PLAT_ROCKCHIP
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)
	int i = 0;
	for (i = 0; i < cnt; i++) {
		msgs[i].scl_rate = 400000;
	}
#endif
#endif
	while(count >= 0){
		count-= 1;
		ret = i2c_transfer(client->adapter, msgs, cnt);
		if(ret < 0){
			tp_log_err("ilitek_i2c_transfer err\n");
			mdelay(20);
			continue;
		}
		break;
	}
	return ret;
}

int ilitek_i2c_write(uint8_t * cmd, int length)
{
	int ret = 0;
	struct i2c_client * client = ilitek_data->client;
	struct i2c_msg msgs[] = {
		{.addr = client->addr, .flags = 0, .len = length, .buf = cmd,}
	};

	ret = ilitek_i2c_transfer(msgs, 1);
	if(ret < 0) {
		tp_log_err("%s, i2c write error, ret %d\n", __func__, ret);
	}
	return ret;
}

int ilitek_i2c_read(uint8_t *data, int length)
{
	int ret = 0;
	struct i2c_client * client = ilitek_data->client;
	struct i2c_msg msgs_ret[] = {
		{.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = data,}
	};


	ret = ilitek_i2c_transfer(msgs_ret, 1);
	if(ret < 0) {
		tp_log_err("%s, i2c read error, ret %d\n", __func__, ret);
	}

	return ret;
}

int ilitek_i2c_write_and_read(uint8_t *cmd,
		int write_len, int delay, uint8_t *data, int read_len)
{
	int ret = 0;
	struct i2c_client * client = ilitek_data->client;
	struct i2c_msg msgs_send[] = {
		{.addr = client->addr, .flags = 0, .len = write_len, .buf = cmd,},
		{.addr = client->addr, .flags = I2C_M_RD, .len = read_len, .buf = data,}
	};
	struct i2c_msg msgs_receive[] = {
		{.addr = client->addr, .flags = I2C_M_RD, .len = read_len, .buf = data,}
	};
	if (ilitek_repeat_start) {
		if (read_len == 0) {
			if (write_len > 0) {
				ret = ilitek_i2c_transfer(msgs_send, 1);
				if(ret < 0) {
					tp_log_err("%s, i2c write error, ret = %d\n", __func__, ret);
				}
			}
			if(delay > 0) {
				mdelay(delay);
			}
		}
		else if (write_len == 0) {
			if(read_len > 0){
				ret = ilitek_i2c_transfer(msgs_receive, 1);
				if(ret < 0) {
					tp_log_err("%s, i2c read error, ret = %d\n", __func__, ret);
				}
				if(delay > 0) {
					mdelay(delay);
				}
			}
		}
		else if (delay > 0) {
			if (write_len > 0) {
				ret = ilitek_i2c_transfer(msgs_send, 1);
				if(ret < 0) {
					tp_log_err("%s, i2c write error, ret = %d\n", __func__, ret);
				}
			}
			if(delay > 0) {
				mdelay(delay);
			}
			if(read_len > 0){
				ret = ilitek_i2c_transfer(msgs_receive, 1);
				if(ret < 0) {
					tp_log_err("%s, i2c read error, ret = %d\n", __func__, ret);
				}
			}
		}
		else {
			ret = ilitek_i2c_transfer(msgs_send, 2);
			if(ret < 0) {
				tp_log_err("%s, i2c repeat start error, ret = %d\n", __func__, ret);
			}
		}
	}
	else {
		if (write_len > 0) {
			ret = ilitek_i2c_transfer(msgs_send, 1);
			if(ret < 0) {
				tp_log_err("%s, i2c write error, ret = %d\n", __func__, ret);
			}
		}
		if(delay > 0) {
			mdelay(delay);
		}
		if(read_len > 0){
			ret = ilitek_i2c_transfer(msgs_receive, 1);
			if(ret < 0) {
				tp_log_err("%s, i2c read error, ret = %d\n", __func__, ret);
			}
		}
	}
	return ret;
}

int ilitek_poll_int(void) 
{
	return gpio_get_value(ilitek_data->irq_gpio);
}

void ilitek_reset(int delay) {
	tp_log_info("delay = %d\n", delay);
	if (ilitek_data->reset_gpio > 0) {
	#if ILITEK_PLAT != ILITEK_PLAT_MTK
		gpio_direction_output(ilitek_data->reset_gpio,1);
		mdelay(10);
		gpio_direction_output(ilitek_data->reset_gpio,0);
		mdelay(10);
		gpio_direction_output(ilitek_data->reset_gpio,1);
		mdelay(delay);
	#else	
		tpd_gpio_output(ilitek_data->reset_gpio, 1);
		mdelay(10); 
		tpd_gpio_output(ilitek_data->reset_gpio, 0);
		mdelay(10);
		tpd_gpio_output(ilitek_data->reset_gpio, 1);
		mdelay(delay);
	#endif
	}
	else {
		tp_log_err("reset pin is invalid\n");
	}
	return;
}

#if ILITEK_PLAT != ILITEK_PLAT_ALLWIN
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
void ilitek_regulator_release(void) {
	if (ilitek_data->vdd) {
		regulator_put(ilitek_data->vdd);
	}	
	if (ilitek_data->vdd_i2c) {
		regulator_put(ilitek_data->vdd_i2c);
	}
}
#endif
#endif
int ilitek_free_gpio(void) {
	if (gpio_is_valid(ilitek_data->reset_gpio)) {
		tp_log_info("reset_gpio is valid so free\n");
		gpio_free(ilitek_data->reset_gpio);
	}
	if (gpio_is_valid(ilitek_data->irq_gpio)) {
		tp_log_info("irq_gpio is valid so free\n");
		gpio_free(ilitek_data->irq_gpio);
	}
	return 0;
}

#if 0
static int ilitek_set_input_param(void)
{
	int ret = 0;
	int i = 0;
	struct input_dev *input = ilitek_data->input_dev;
	tp_log_debug("ilitek_set_input_param\n");
#ifdef ILITEK_USE_MTK_INPUT_DEV
	if (tpd_dts_data.use_tpd_button) {
		for (i = 0; i < tpd_dts_data.tpd_key_num; i ++) {
			input_set_capability(input, EV_KEY, tpd_dts_data.tpd_key_local[i]);
		}
	}
#else
	
	#if 0
	__set_bit(INPUT_PROP_DIRECT, input->propbit);
	input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	#endif
		
	
	#if 0
#if !ILITEK_ROTATE_FLAG
	#ifdef ILITEK_USE_LCM_RESOLUTION
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, TOUCH_SCREEN_X_MAX, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, TOUCH_SCREEN_Y_MAX, 0, 0);
	#else
	input_set_abs_params(input, ABS_MT_POSITION_X, ilitek_data->screen_min_x, ilitek_data->screen_max_x, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, ilitek_data->screen_min_y, ilitek_data->screen_max_y, 0, 0);
	#endif
#else
	#ifdef ILITEK_USE_LCM_RESOLUTION
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, TOUCH_SCREEN_Y_MAX, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, TOUCH_SCREEN_X_MAX, 0, 0);
	#else
	input_set_abs_params(input, ABS_MT_POSITION_X, ilitek_data->screen_min_y, ilitek_data->screen_max_y, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, ilitek_data->screen_min_x, ilitek_data->screen_max_x, 0, 0);
	#endif
#endif
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	#endif
	
	//ben
	input_set_abs_params(input, ABS_X, 0, TOUCH_SCREEN_X_MAX, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, TOUCH_SCREEN_Y_MAX, 0, 0);
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);	
	
	input->name = ILITEK_TS_NAME;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &(ilitek_data->client)->dev;
#endif

#ifdef ILITEK_TOUCH_PROTOCOL_B
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
		input_mt_init_slots(input, ilitek_data->max_tp, INPUT_MT_DIRECT);
	#else
		input_mt_init_slots(input, ilitek_data->max_tp);
	#endif
#endif
	input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, ilitek_data->max_tp, 0, 0);
#ifdef ILITEK_REPORT_PRESSURE
	input_set_abs_params(input, ABS_MT_PRESSURE, 0, 255, 0, 0);
#endif
	
	for(i = 0; i < ilitek_data->keycount; i++) {
		set_bit(ilitek_data->keyinfo[i].id & KEY_MAX, input->keybit);
	}
	
#ifdef ILITEK_GESTURE
	input_set_capability(input, EV_KEY, KEY_POWER);
	input_set_capability(input, EV_KEY, KEY_W);
	input_set_capability(input, EV_KEY, KEY_O);
	input_set_capability(input, EV_KEY, KEY_C);
	input_set_capability(input, EV_KEY, KEY_E);
	input_set_capability(input, EV_KEY, KEY_M);
#endif

#ifndef ILITEK_USE_MTK_INPUT_DEV
	ret = input_register_device(ilitek_data->input_dev);
	if (ret) {
		tp_log_err("register input device, error\n");
	}
#endif
	return ret;
}
#endif
static int ilitek_set_input_param(void)
{
	int ret = 0;
	struct input_dev *input = ilitek_data->input_dev;
	tp_log_debug("ilitek_set_input_param\n");
#ifdef ILITEK_USE_MTK_INPUT_DEV
	if (tpd_dts_data.use_tpd_button) {
		for (i = 0; i < tpd_dts_data.tpd_key_num; i ++) {
			input_set_capability(input, EV_KEY, tpd_dts_data.tpd_key_local[i]);
		}
	}
#else
	
	#if 0
	__set_bit(INPUT_PROP_DIRECT, input->propbit);
	input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	#endif
	__set_bit(EV_SYN, input->evbit);
	__set_bit(EV_KEY, input->evbit);
	__set_bit(EV_ABS, input->evbit);
	set_bit(BTN_TOUCH, input->keybit);
	set_bit(ABS_X, input->absbit);
    set_bit(ABS_Y, input->absbit);
	set_bit(ABS_PRESSURE, input->absbit);		

	//ben
	input_set_abs_params(input, ABS_X, 0, TOUCH_SCREEN_X_MAX, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, TOUCH_SCREEN_Y_MAX, 0, 0);
	//input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	//input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
		
	
	input->name = ILITEK_TS_NAME;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &(ilitek_data->client)->dev;
#endif

	/*
	for(i = 0; i < ilitek_data->keycount; i++) {
		set_bit(ilitek_data->keyinfo[i].id & KEY_MAX, input->keybit);
	}
	*/
	
	/*
#ifdef ILITEK_GESTURE
	input_set_capability(input, EV_KEY, KEY_POWER);
	input_set_capability(input, EV_KEY, KEY_W);
	input_set_capability(input, EV_KEY, KEY_O);
	input_set_capability(input, EV_KEY, KEY_C);
	input_set_capability(input, EV_KEY, KEY_E);
	input_set_capability(input, EV_KEY, KEY_M);
#endif
	*/

#ifndef ILITEK_USE_MTK_INPUT_DEV
	ret = input_register_device(ilitek_data->input_dev);
	if (ret) {
		tp_log_err("register input device, error\n");
	}
#endif
	return ret;
}

#if 0
static int ilitek_touch_down(int id, int x, int y, int pressure) {
	struct input_dev *input = ilitek_data->input_dev;
#if defined(ILITEK_USE_MTK_INPUT_DEV) || defined(ILITEK_USE_LCM_RESOLUTION)
	x = (x - ilitek_data->screen_min_x) * TOUCH_SCREEN_X_MAX / (ilitek_data->screen_max_x - ilitek_data->screen_min_x);
	y = (y - ilitek_data->screen_min_y) * TOUCH_SCREEN_Y_MAX / (ilitek_data->screen_max_y - ilitek_data->screen_min_y);
#endif
	input_report_key(input, BTN_TOUCH, 1);
#ifdef ILITEK_TOUCH_PROTOCOL_B
	input_mt_slot(input, id);
	input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
#endif

#if 0
#if !ILITEK_ROTATE_FLAG
	input_event(input, EV_ABS, ABS_MT_POSITION_X, x);
	input_event(input, EV_ABS, ABS_MT_POSITION_Y, y);
#else
	input_event(input, EV_ABS, ABS_MT_POSITION_X, y);
	input_event(input, EV_ABS, ABS_MT_POSITION_Y, x);
#endif
#endif

	input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, 128);
#ifdef ILITEK_REPORT_PRESSURE
	input_event(input, EV_ABS, ABS_MT_PRESSURE, pressure);
#endif
#ifndef ILITEK_TOUCH_PROTOCOL_B
	//input_event(input, EV_ABS, ABS_MT_TRACKING_ID, id);
	input_mt_sync(input);
#endif

#if ILITEK_PLAT == ILITEK_PLAT_MTK
#ifdef CONFIG_MTK_BOOT
	if (tpd_dts_data.use_tpd_button) {
		if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
		{	
			tpd_button(x, y, 1);	
			tp_log_debug("tpd_button(x, y, 1) = tpd_button(%d, %d, 1)\n", x, y);
		}
	}
#endif
#endif
	return 0;
}
#endif

//add ben
static int old_x = 0;
static int old_y = 0;
static int ilitek_touch_down(int id, int x, int y, int pressure) {
	struct input_dev *input = ilitek_data->input_dev;
	int tmp_x;
	int tmp_y;
	
#if defined(ILITEK_USE_MTK_INPUT_DEV) || defined(ILITEK_USE_LCM_RESOLUTION)
	x = (x - ilitek_data->screen_min_x) * TOUCH_SCREEN_X_MAX / (ilitek_data->screen_max_x - ilitek_data->screen_min_x);
	y = (y - ilitek_data->screen_min_y) * TOUCH_SCREEN_Y_MAX / (ilitek_data->screen_max_y - ilitek_data->screen_min_y);
#endif

#if 0
	//printk("ilitek_touch_down:%d,%d \n", x, y);
	//if ((old_x == 0) && (old_y == 0)) 
	//{
		old_x = x;
		old_y = y;
		input_report_key(input,BTN_TOUCH,1);
				
		//input_report_abs(input, ABS_MT_TOUCH_MAJOR, 128);
		
				
		input_report_abs(input, ABS_X, x);
		input_report_abs(input, ABS_Y, y);
		input_report_abs(input, ABS_PRESSURE, 1);
		input_sync(input);		
	//}	
#endif

	//test 20180131
	//swap_x = y;
	//swap_y = x;
	tmp_y = 1920 - x;
	tmp_x =  y;
	old_x = x;
	old_y = y;
	input_report_key(input,BTN_TOUCH,1);
				
	//input_report_abs(input, ABS_MT_TOUCH_MAJOR, 128);
	//printk("ilitek_touch_down:%d,%d,%d,%d\n", x,y,tmp_x,tmp_y);	
	input_report_abs(input, ABS_X, x);
	input_report_abs(input, ABS_Y, y);
	input_report_abs(input, ABS_PRESSURE, 1);
	input_sync(input);	
		
	//
	#if 0
	if ((old_x != x) || (old_y != y)) 
	{
		old_x = x;
		old_y = y;
		input_report_key(input,BTN_TOUCH,1);
				
		//input_report_abs(input, ABS_MT_TOUCH_MAJOR, 128);
		
				
		input_report_abs(input, ABS_X, x);
		input_report_abs(input, ABS_Y, y);
		input_report_abs(input, ABS_PRESSURE, 1);
		input_sync(input);		
	}	
	#endif
	
	return 0;
}

#if 0
static int ilitek_touch_release(int id) {
	struct input_dev *input = ilitek_data->input_dev;
#ifdef ILITEK_TOUCH_PROTOCOL_B
	if(ilitek_data->touch_flag[id] == 1) {
		tp_log_debug("release point id = %d\n", id);
		input_mt_slot(input, id);
		input_mt_report_slot_state(input, MT_TOOL_FINGER, false);
	}
#else
	input_report_key(input, BTN_TOUCH, 0);
	input_mt_sync(input);
#endif
	ilitek_data->touch_flag[id] = 0;
#if ILITEK_PLAT == ILITEK_PLAT_MTK
#ifdef CONFIG_MTK_BOOT
	if (tpd_dts_data.use_tpd_button) {
		if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
		{	
			tpd_button(0, 0, 0);	
			tp_log_debug("tpd_button(x, y, 0) = tpd_button(%d, %d, 0)\n", 0, 0);
		}
	}
#endif
#endif

	return 0;
}
#endif
static int ilitek_touch_release(int id) {
	struct input_dev *input = ilitek_data->input_dev;

	old_x = 0;
	old_y = 0;
	
	input_report_key(input, BTN_TOUCH, 0);
	input_report_abs(input, ABS_PRESSURE, 0);
	input_sync(input);

	ilitek_data->touch_flag[id] = 0;

	return 0;
}


static int ilitek_touch_release_all_point(void) {
	struct input_dev *input = ilitek_data->input_dev;
	int i = 0;
#ifdef ILITEK_TOUCH_PROTOCOL_B
	input_report_key(input, BTN_TOUCH, 0);
	for(i = 0; i < ilitek_data->max_tp; i++)
	{
		ilitek_touch_release(i);
	}
#else
	for(i = 0; i < ilitek_data->max_tp; i++)
	{
		ilitek_data->touch_flag[i] = 0;
	}
	ilitek_touch_release(0);
#endif
	input_sync(input);
	return 0;
}

static int ilitek_check_key_down(int x, int y) {
#ifndef ILITEK_REPORT_KEY_WITH_COORDINATE
	struct input_dev *input = ilitek_data->input_dev;
#endif
	int j = 0;
	for(j = 0; j < ilitek_data->keycount; j++){
		if((x >= ilitek_data->keyinfo[j].x && x <= ilitek_data->keyinfo[j].x + ilitek_data->key_xlen) &&
			(y >= ilitek_data->keyinfo[j].y && y <= ilitek_data->keyinfo[j].y + ilitek_data->key_ylen)) {
			#ifndef ILITEK_REPORT_KEY_WITH_COORDINATE
			input_report_key(input,  ilitek_data->keyinfo[j].id, 1);
			#else
				#if ILITEK_PLAT == ILITEK_PLAT_MTK
				if (tpd_dts_data.use_tpd_button) {
					tp_log_debug("key index=%x, tpd_dts_data.tpd_key_local[%d]=%d key down\n", j, j, tpd_dts_data.tpd_key_local[j]);
					ilitek_touch_down(0, tpd_dts_data.tpd_key_dim_local[j].key_x, tpd_dts_data.tpd_key_dim_local[j].key_y, 10);
				}
				#endif
			#endif
			ilitek_data->keyinfo[j].status = 1;
			ilitek_data->touch_key_hold_press = true;
			ilitek_data->is_touched = true;
			tp_log_debug("Key, Keydown ID=%d, X=%d, Y=%d, key_status=%d\n",
				ilitek_data->keyinfo[j].id ,x ,y , ilitek_data->keyinfo[j].status);
			break;
		}
	}
	return 0;
}

static int ilitek_check_key_release(int x, int y, int check_point) {
#ifndef ILITEK_REPORT_KEY_WITH_COORDINATE
	struct input_dev *input = ilitek_data->input_dev;
#endif
	int j = 0;
	for (j = 0; j < ilitek_data->keycount; j++) {
		if (check_point) {
			if ((ilitek_data->keyinfo[j].status == 1) && (x < ilitek_data->keyinfo[j].x ||
				x > ilitek_data->keyinfo[j].x + ilitek_data->key_xlen || y < ilitek_data->keyinfo[j].y ||
				y > ilitek_data->keyinfo[j].y + ilitek_data->key_ylen)) {
				#ifndef ILITEK_REPORT_KEY_WITH_COORDINATE
				input_report_key(input,  ilitek_data->keyinfo[j].id, 0);
				#else
					#if ILITEK_PLAT == ILITEK_PLAT_MTK
					if (tpd_dts_data.use_tpd_button) {
						tp_log_debug("key index=%x, tpd_dts_data.tpd_key_local[%d]=%d key up\n", j, j, tpd_dts_data.tpd_key_local[j]);
						ilitek_touch_release(0);
					}
					#endif
				#endif
				ilitek_data->keyinfo[j].status = 0;
				ilitek_data->touch_key_hold_press = false;
				tp_log_debug("Key, Keyout ID=%d, X=%d, Y=%d, key_status=%d\n",
					ilitek_data->keyinfo[j].id, x , y, ilitek_data->keyinfo[j].status);
				break;
			}
		}
		else {
			if ((ilitek_data->keyinfo[j].status == 1)) {
			#ifndef ILITEK_REPORT_KEY_WITH_COORDINATE
				input_report_key(input,  ilitek_data->keyinfo[j].id, 0);
			#else
				#if ILITEK_PLAT == ILITEK_PLAT_MTK
					if (tpd_dts_data.use_tpd_button) {
						tp_log_debug("key index=%x, tpd_dts_data.tpd_key_local[%d]=%d key up\n", j, j, tpd_dts_data.tpd_key_local[j]);
						ilitek_touch_release(0);
					}
				#endif
			#endif
				ilitek_data->keyinfo[j].status = 0;
				ilitek_data->touch_key_hold_press = false;
				tp_log_debug("Key, Keyout ID=%d, X=%d, Y=%d, key_status=%d\n",
					ilitek_data->keyinfo[j].id, x , y, ilitek_data->keyinfo[j].status);
				break;
			}
		}
	}
	return 0;
}

#ifdef ILITEK_GESTURE
#if ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP

static struct timeval   start_event_time;
int event_spacing = 0;
static unsigned char finger_state = 0;    //0,1,2,3,4
static int start_x = 0;
static int start_y = 0;
static int current_x = 0;
static int current_y = 0;
#define ABSSUB(a, b) ((a > b) ? (a - b) : (b - a))

#define DOUBLE_CLICK_DISTANCE						1000
#define DOUBLE_CLICK_ONE_CLICK_USED_TIME			800
#define DOUBLE_CLICK_NO_TOUCH_TIME					1000
#define DOUBLE_CLICK_TOTAL_USED_TIME				(DOUBLE_CLICK_NO_TOUCH_TIME + (DOUBLE_CLICK_ONE_CLICK_USED_TIME * 2))

static int ilitek_get_time_diff (struct timeval *past_time) {
    ktime_t time_now = ktime_get();
    int diff_milliseconds = (time_now.tv_sec - past_time->tv_sec)*1000;
	
    if (time_now.tv_usec < past_time->tv_usec) {
        diff_milliseconds -= 1000;
        diff_milliseconds += (1000 * 1000 + time_now.tv_usec - past_time->tv_usec) / 1000;
    }
    else {
        diff_milliseconds += (time_now.tv_usec - past_time->tv_usec) / 1000;
    }

    if (diff_milliseconds < (-10000)) {
        diff_milliseconds = 10000;
    }
	//tp_log_info("diff_milliseconds = %d\n", diff_milliseconds);
    return diff_milliseconds;
}

static int ilitek_double_click_touch(int x,int y,char finger_state,int finger_id) {
	tp_log_info("start finger_state = %d\n", finger_state);
	if (finger_id > 0){
		finger_state = 0;
		goto out;
	}
	if (finger_state == 0||finger_state == 5) {
	
		finger_state = 1;
		start_x = x;
		start_y = y;
		current_x = 0;
		current_y = 0;
		event_spacing = 0;
		do_gettimeofday(&start_event_time);
	}
	else if (finger_state == 1) {
		event_spacing = ilitek_get_time_diff(&start_event_time);
		if (event_spacing > DOUBLE_CLICK_ONE_CLICK_USED_TIME) {
			finger_state = 4;
		}
	}
	else if (finger_state == 2) {
		finger_state = 3;
		current_x = x;
		current_y = y;
		event_spacing = ilitek_get_time_diff(&start_event_time);
		if (event_spacing > (DOUBLE_CLICK_ONE_CLICK_USED_TIME + DOUBLE_CLICK_NO_TOUCH_TIME)) {
			finger_state = 0;
		}
	}
	else if (finger_state == 3) {
		current_x = x;
		current_y = y;
		event_spacing = ilitek_get_time_diff(&start_event_time);							   	  
		if (event_spacing > DOUBLE_CLICK_TOTAL_USED_TIME) {
			start_x = current_x;
			start_y = current_y;
			finger_state = 4;
		}
	}
out:
	tp_log_info("finger_state = %d event_spacing = %d\n", finger_state, event_spacing);
	return finger_state;
}

static int ilitek_double_click_release(char finger_state){
	tp_log_info("start finger_state = %d\n", finger_state);
	if (finger_state == 1) {
		finger_state = 2;
		event_spacing = ilitek_get_time_diff(&start_event_time);
		if (event_spacing > DOUBLE_CLICK_ONE_CLICK_USED_TIME) {
			finger_state = 0;
		}	
	}
	if (finger_state == 3) {
		event_spacing = ilitek_get_time_diff(&start_event_time);
		if ((event_spacing < DOUBLE_CLICK_TOTAL_USED_TIME && event_spacing > 50) && (ABSSUB(current_x, start_x) < DOUBLE_CLICK_DISTANCE) && ((ABSSUB(current_y, start_y) < DOUBLE_CLICK_DISTANCE))) {
			finger_state = 5;
			goto out;
		}
		else {
			finger_state = 0;
		}
	}
	else if (finger_state == 4) {
		finger_state = 0;
	}
out:
	tp_log_info("finger_state = %d event_spacing = %d\n", finger_state, event_spacing);
	return finger_state;
}

#endif
#endif
static int ilitek_read_data_and_report_3XX(void) {
	int ret = 0;
	int packet = 0;
	int report_max_point = 6;
	int release_point = 0;
	int tp_status = 0;
	int i = 0;
	int x = 0;
	int y = 0;
	struct input_dev *input = ilitek_data->input_dev;
	unsigned char buf[64]={0};
	buf[0] = ILITEK_TP_CMD_GET_TOUCH_INFORMATION;
	ret = ilitek_i2c_write_and_read(buf, 1, 0, buf, 31);
	if (ret < 0) {
		tp_log_err("get touch information err\n");
		if (ilitek_data->is_touched) {
			ilitek_touch_release_all_point();
		}
		return ret;
	}
	packet = buf[0];
	if (packet == 2) {
		ret = ilitek_i2c_read(buf+31, 20);
		if (ret < 0) {
			tp_log_err("get touch information packet 2 err\n");
			if (ilitek_data->is_touched) {
				ilitek_touch_release_all_point();
			}
			return ret;
		}
		report_max_point = 10;
	}
	
	//ben
	report_max_point = 1;
	
#ifdef ILITEK_TUNING_MESSAGE
	if (ilitek_debug_flag) {
		ilitek_udp_reply(ilitek_pid, ilitek_seq, buf, sizeof(buf));
	}
#endif
	if (buf[1] == 0x5F || buf[0] == 0xDB) {
		tp_log_debug("debug message return\n");
		return 0;
	}
	for (i = 0; i < report_max_point; i++) {
		tp_status = buf[i*5+1] >> 7;
		tp_log_debug("ilitek tp_status = %d buf[i*5+1] = 0x%X\n", tp_status, buf[i*5+1]);
		if (tp_status) {
			ilitek_data->touch_flag[i] = 1;
			x = ((buf[i*5+1] & 0x3F) << 8) + buf[i*5+2];
			y = (buf[i*5+3] << 8) + buf[i*5+4];
			tp_log_debug("ilitek x = %d y = %d\n", x, y);
			if (ilitek_data->system_suspend) {
				tp_log_info("system is suspend not report point\n");
				#ifdef ILITEK_GESTURE
				#if ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
					finger_state = ilitek_double_click_touch(x, y, finger_state, i);
				#endif
				#endif
			}
			else {
				if(!(ilitek_data->is_touched)) {
					ilitek_check_key_down(x, y);
				}
				if (!(ilitek_data->touch_key_hold_press)) {
					if (x > ilitek_data->screen_max_x || y > ilitek_data->screen_max_y ||
						x < ilitek_data->screen_min_x || y < ilitek_data->screen_min_y) {
						tp_log_info("Point (x > screen_max_x || y > screen_max_y) , ID=%02X, X=%d, Y=%d\n", i, x, y); 
					}
					else {
						ilitek_data->is_touched = true;
						if (ILITEK_REVERT_X) {
							x = ilitek_data->screen_max_x - x + ilitek_data->screen_min_x;
						}
							
						if (ILITEK_REVERT_Y) {
							y = ilitek_data->screen_max_y - y + ilitek_data->screen_min_y;
						}
						tp_log_debug("Point, ID=%02X, X=%04d, Y=%04d\n",i, x,y); 
						ilitek_touch_down(i, x, y, 10);
					}
				}
				//if ((ilitek_data->touch_key_hold_press)){
				//	ilitek_check_key_release(x, y, 1);
				//}
			}
		}
		else {
			release_point++;
			#ifdef ILITEK_TOUCH_PROTOCOL_B
			ilitek_touch_release(i);
			#endif
		}
	}
	tp_log_debug("release point counter =  %d packet = %d\n", release_point, packet);
	if (packet == 0 || release_point == report_max_point) {
		if (ilitek_data->is_touched) {
			ilitek_touch_release_all_point();
		}
		ilitek_check_key_release(x, y, 0);
		ilitek_data->is_touched = false;
		if (ilitek_data->system_suspend) {
		#ifdef ILITEK_GESTURE
		#if ILITEK_GESTURE == ILITEK_CLICK_WAKEUP
			input_report_key(input, KEY_POWER, 1);
			input_sync(input);
			input_report_key(input, KEY_POWER, 0);
			input_sync(input);
			ilitek_data->system_suspend = false;
		#elif ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
			finger_state = ilitek_double_click_release(finger_state);
			if (finger_state == 5) {
				tp_log_info("double click wakeup\n");
				input_report_key(input, KEY_POWER, 1);
				input_sync(input);
				input_report_key(input, KEY_POWER, 0);
				input_sync(input);
				ilitek_data->system_suspend = false;
			}
		#endif
		#endif
		}
	}
	input_sync(input);
	return 0;
}


static int ilitek_read_data_and_report_2120(void) {
	int ret = 0;
	int touch_point_num = 0;
	int release_point = 0;
	int tp_status = 0;
	int i = 0;
	int x = 0;
	int y = 0;
	struct input_dev *input = ilitek_data->input_dev;
	unsigned char buf[64]={0};
	buf[0] = ILITEK_TP_CMD_GET_TOUCH_INFORMATION;
	ret = ilitek_i2c_write_and_read(buf, 1, 0, buf, 53);
	if (ret < 0) {
		tp_log_err("get touch information err\n");
		if (ilitek_data->is_touched) {
			ilitek_touch_release_all_point();
		}
		return ret;
	}
#ifdef ILITEK_TUNING_MESSAGE
	if (ilitek_debug_flag) {
		ilitek_udp_reply(ilitek_pid, ilitek_seq, buf, sizeof(buf));
	}
#endif
	if (buf[1] == 0x5F) {
		tp_log_debug("debug message return\n");
		return 0;
	}
#ifdef ILITEK_GESTURE
	if (ilitek_data->system_suspend) {
		tp_log_info("gesture wake up 0x%x, 0x%x, 0x%x\n", buf[0], buf[1], buf[2]);
		if (buf[2] == 0x60) {
			tp_log_info("gesture wake up this is c\n");
			input_report_key(input, KEY_C, 1);
			input_sync(input);
			input_report_key(input, KEY_C, 0);
			input_sync(input);
		}
		else if (buf[2] == 0x62) {
			tp_log_info("gesture wake up this is e\n");
			input_report_key(input, KEY_E, 1);
			input_sync(input);
			input_report_key(input, KEY_E, 0);
			input_sync(input);
		}
		else if (buf[2] == 0x64) {
			tp_log_info("gesture wake up this is m\n");
			input_report_key(input, KEY_M, 1);
			input_sync(input);
			input_report_key(input, KEY_M, 0);
			input_sync(input);
		}
		else if (buf[2] == 0x66) {
			tp_log_info("gesture wake up this is w\n");
			input_report_key(input, KEY_W, 1);
			input_sync(input);
			input_report_key(input, KEY_W, 0);
			input_sync(input);
		}
		else if (buf[2] == 0x68) {
			tp_log_info("gesture wake up this is o\n");
			input_report_key(input, KEY_O, 1);
			input_sync(input);
			input_report_key(input, KEY_O, 0);
			input_sync(input);
		}
		else if (buf[2] == 0x22) {
			tp_log_info("gesture wake up this is double click\n");
			if (false) {
				input_report_key(input, KEY_O, 1);
				input_sync(input);
				input_report_key(input, KEY_O, 0);
				input_sync(input);
			}
		}
		input_report_key(input, KEY_POWER, 1);
		input_sync(input);
		input_report_key(input, KEY_POWER, 0);
		input_sync(input);
		return 0;
	}
#endif
	touch_point_num = buf[0];
	for (i = 0; i < ilitek_data->max_tp; i++) {
		tp_status = buf[i*5+3] >> 7;	
		tp_log_debug("ilitek tp_status = %d buf[i*5+3] = 0x%X\n", tp_status, buf[i*5+3]);
		if (tp_status) {
			ilitek_data->touch_flag[i] = 1;
			x = (((int)(buf[i*5+3] & 0x3F) << 8) + buf[i*5+4]);
			y = (((int)(buf[i*5+5] & 0x3F) << 8) + buf[i*5+6]);
			tp_log_debug("ilitek x = %d y = %d\n", x, y);
			if(!(ilitek_data->is_touched)) {
				ilitek_check_key_down(x, y);
			}
			if (!(ilitek_data->touch_key_hold_press)) {
				if (x > ilitek_data->screen_max_x || y > ilitek_data->screen_max_y ||
					x < ilitek_data->screen_min_x || y < ilitek_data->screen_min_y) {
					tp_log_info("Point (x > screen_max_x || y > screen_max_y) , ID=%02X, X=%d, Y=%d\n", i, x, y); 
				}
				else {
					ilitek_data->is_touched = true;
					if (ILITEK_REVERT_X) {
						x = ilitek_data->screen_max_x - x + ilitek_data->screen_min_x;
					}
						
					if (ILITEK_REVERT_Y) {
						y = ilitek_data->screen_max_y - y + ilitek_data->screen_min_y;
					}
					tp_log_debug("Point, ID=%02X, X=%04d, Y=%04d\n",i, x,y); 
					ilitek_touch_down(i, x, y, 10);
				}
			}
			//if ((ilitek_data->touch_key_hold_press)){
			//	ilitek_check_key_release(x, y, 1);
			//}
		}
		else {
			release_point++;
			#ifdef ILITEK_TOUCH_PROTOCOL_B
			ilitek_touch_release(i);
			#endif
		}
	}
	tp_log_debug("release point counter =  %d touch_point_num = %d\n", release_point, touch_point_num);
	if (touch_point_num == 0 || release_point == ilitek_data->max_tp) {
		if (ilitek_data->is_touched) {
			ilitek_touch_release_all_point();
		}
		ilitek_check_key_release(x, y, 0);
		ilitek_data->is_touched = false;
	}
	input_sync(input);
	return 0;
}

static irqreturn_t ilitek_i2c_isr(int irq, void *dev_id) {
    unsigned long irqflag = 0;
	tp_log_debug("\n");
#ifdef ILITEK_ESD_PROTECTION
	ilitek_data->esd_check = false;
#endif
	if (ilitek_data->firmware_updating) {
		tp_log_debug("firmware_updating return\n");
		return IRQ_HANDLED;
	}
	ilitek_data->irq_trigger = true;
	wake_up_interruptible(&waiter);
	spin_lock_irqsave(&ilitek_data->irq_lock, irqflag);
	if (ilitek_data->irq_status) {
        disable_irq_nosync(ilitek_data->client->irq);
		ilitek_data->irq_status = false;
	}
	spin_unlock_irqrestore(&ilitek_data->irq_lock, irqflag);
    return IRQ_HANDLED;
}
static int ilitek_request_irq(void) {
	int ret = 0;
#if ILITEK_PLAT == ILITEK_PLAT_MTK
	struct device_node *node;
#endif
    spin_lock_init(&ilitek_data->irq_lock);
	ilitek_data->irq_status = true;
#if ILITEK_PLAT != ILITEK_PLAT_MTK
	ilitek_data->client->irq  = gpio_to_irq(ilitek_data->irq_gpio);
#else
	node = of_find_matching_node(NULL, touch_of_match);
	if (node) {
		ilitek_data->client->irq = irq_of_parse_and_map(node, 0);
	}
#endif
	if (ilitek_data->client->irq > 0) {
		//ret = request_irq(ilitek_data->client->irq, ilitek_i2c_isr, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ilitek_i2c_irq", ilitek_data);
		ret = request_threaded_irq(ilitek_data->client->irq, NULL,ilitek_i2c_isr, IRQF_TRIGGER_FALLING/*IRQF_TRIGGER_LOW*/ | IRQF_ONESHOT,"ilitek_touch_irq", ilitek_data);
		if (ret) {
			tp_log_err("ilitek_request_irq, error\n");
		}
	}
	else {
		ret = -EINVAL;
	}
	return ret;
}

static int ilitek_i2c_process_and_report(void) {
	int ret = 0;
	mutex_lock(&ilitek_data->ilitek_mutex);
	if (ilitek_data->ic_2120) {
		ret = ilitek_read_data_and_report_2120();
	}
	else {
		ret = ilitek_read_data_and_report_3XX();
	}
	mutex_unlock(&ilitek_data->ilitek_mutex);
	return ret;
}

#ifdef ILITEK_UPDATE_FW
static int ilitek_update_thread(void *arg)
{

	int ret=0;
	tp_log_info("\n");

	if(kthread_should_stop()){
		tp_log_info("ilitek_update_thread, stop\n");
		return -1;
	}
	
	mdelay(100);
	ilitek_data->firmware_updating = true;
	ilitek_data->operation_protection = true;
	ret = ilitek_upgrade_firmware();
	ret = ilitek_read_tp_info();
	ilitek_data->operation_protection = false;
	ilitek_data->firmware_updating = false;
	ret = ilitek_set_input_param();
	if (ret) {
		tp_log_err("register input device, error\n");
	}
	ret = ilitek_request_irq();
	if (ret) {
		tp_log_err("ilitek_request_irq, error\n");
	}
	return ret;
}
#endif

static int ilitek_irq_handle_thread(void *arg) {
	int ret=0;
	struct sched_param param = { .sched_priority = 4};
	sched_setscheduler(current, SCHED_RR, &param);	
	tp_log_info("%s, enter\n", __func__);

	// mainloop
	while(!kthread_should_stop() && !ilitek_exit_report){
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, ilitek_data->irq_trigger);
		ilitek_data->irq_trigger = false;
		set_current_state(TASK_RUNNING);
		if (ilitek_i2c_process_and_report() < 0){
			tp_log_err("process error\n");
		}
		ilitek_irq_enable();
	}
	return ret;
}

void ilitek_suspend(void) {
	int ret = 0;
	uint8_t cmd[2] = {0};
	tp_log_info("\n");
#ifdef ILITEK_ESD_PROTECTION
	ilitek_data->esd_check = false;
	cancel_delayed_work_sync(&ilitek_data->esd_work);
#endif

#ifdef ILITEK_CHARGER_DETECTION
	ilitek_data->charge_check = false;
	cancel_delayed_work_sync(&ilitek_data->charge_work);
#endif

	if (ilitek_data->operation_protection || ilitek_data->firmware_updating) {
		tp_log_info("operation_protection or firmware_updating return\n");
		return;
	}
	mutex_lock(&ilitek_data->ilitek_mutex);
	if (ilitek_data->ic_2120) {
		cmd[0] = 0x01;
		cmd[1] = 0x00;
		ret = ilitek_i2c_write(cmd, 2);
		if(ret < 0){
			tp_log_err("0x01 0x00 set tp suspend err, ret %d\n", ret);
		}
		mdelay(10);
	#if defined(ILITEK_GESTURE)
		if (ilitek_data->enable_gesture) {
			cmd[0] = 0x0A;
			cmd[1] = 0x01;
		}
		else {
			cmd[0] = ILITEK_TP_CMD_SLEEP_2120;
			cmd[1] = 0x00;
		}
		ret = ilitek_i2c_write(cmd, 2);
		if(ret < 0){
			tp_log_err("0x%X 0x%X set tp suspend err, ret %d\n", cmd[0], cmd[1], ret);
		}
	#else
		cmd[0] = ILITEK_TP_CMD_SLEEP_2120;
		cmd[1] = 0x00;
		ret = ilitek_i2c_write(cmd, 2);
		if(ret < 0){
			tp_log_err("0x02 0x00 set tp suspend err, ret %d\n", ret);
		}
	#endif
	}
	else {
	#ifndef ILITEK_GESTURE
		cmd[0] = ILITEK_TP_CMD_SLEEP;
		ret = ilitek_i2c_write(cmd, 1);
		if(ret < 0){
			tp_log_err("0x30 set tp suspend err, ret %d\n", ret);
		}
	#endif
	}
#ifndef ILITEK_GESTURE
	ilitek_irq_disable();
#endif
	mutex_unlock(&ilitek_data->ilitek_mutex);
	ilitek_data->system_suspend = true;
}

void ilitek_resume(void) {
	u8 cmd[2] = {0};
	int ret = 0;
#ifdef ILITEK_CHARGER_DETECTION
	u8 ChargerStatus[20] = {0};
#endif
	tp_log_info("\n");
	ilitek_touch_release_all_point();
	if (ilitek_data->operation_protection || ilitek_data->firmware_updating) {
		tp_log_info("operation_protection or firmware_updating return\n");
		return;
	}
	mutex_lock(&ilitek_data->ilitek_mutex);
#ifdef ILITEK_GESTURE
	ilitek_irq_disable();
#endif
	if (ilitek_data->ic_2120) {
		ilitek_reset(50);
	}
	else {
		ilitek_reset(200);
	}
	mutex_unlock(&ilitek_data->ilitek_mutex);
#ifdef ILITEK_GLOVE
	if (ilitek_data->enable_gesture) {
		ilitek_into_glovemode(true);
	}
#endif
	if ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25) {
		tp_log_info("ILI2511 resume write 0x20\n");
		cmd[0] = ILITEK_TP_CMD_GET_TP_RESOLUTION;
		ret = ilitek_i2c_write(cmd, 1);
		if (ret < 0) {
			tp_log_err("ILI251X resume write 0x20 err ret = %d\n", ret);
		}
	}
	ilitek_irq_enable();
	ilitek_data->system_suspend = false;
	#ifdef ILITEK_GESTURE
	#if ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
		finger_state = 0;
	#endif
	#endif
#ifdef ILITEK_ESD_PROTECTION
	ilitek_data->esd_check = true;
	if (ilitek_data->esd_wq) {
		queue_delayed_work(ilitek_data->esd_wq, &ilitek_data->esd_work, ilitek_data->esd_delay);
	}
#endif
#ifdef ILITEK_CHARGER_DETECTION
	ilitek_read_file(POWER_SUPPLY_BATTERY_STATUS_PATCH, ChargerStatus, 20);
	tp_log_info("*** Battery Status : %s ***\n", ChargerStatus);
	if (strstr(ChargerStatus, "Charging") != NULL || strstr(ChargerStatus, "Full") != NULL || strstr(ChargerStatus, "Fully charged") != NULL) {
		ilitek_into_chargemode(true); // charger plug-in
	}
	else { // Not charging
		ilitek_into_chargemode(false); // charger plug-out
	}
	ilitek_data->charge_check = true;
	if (ilitek_data->charge_wq) {
		queue_delayed_work(ilitek_data->charge_wq, &ilitek_data->charge_work, ilitek_data->charge_delay);
	}
#endif
}

#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
int ilitek_suspend_allwin(struct i2c_client *client, pm_message_t mesg) {
	ilitek_suspend();
	return 0;
}
int ilitek_resume_allwin(struct i2c_client *client) {
	ilitek_resume();
	return 0;
}
#endif

#if ILITEK_PLAT != ILITEK_PLAT_MTK
#ifdef CONFIG_FB
static int ilitek_fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data) {
	struct fb_event *ev_data = data;
	int *blank;
	tp_log_info("FB EVENT event = %ld\n", event);

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK) {
			ilitek_resume();
				
		}
		else if (*blank == FB_BLANK_POWERDOWN) {
			ilitek_suspend();
		}
	}
	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ilitek_early_suspend(struct early_suspend *h) {
	ilitek_suspend();
}

static void ilitek_late_resume(struct early_suspend *h) {
	ilitek_resume();
}
#endif
#endif

int ilitek_get_gpio_num(void) {
	int ret = 0;
#ifdef ILITEK_GET_GPIO_NUM
#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
	tp_log_info("(config_info.wakeup_gpio.gpio) = %d (config_info.int_number) = %d\n", (config_info.wakeup_gpio.gpio), (config_info.int_number));
	ilitek_data->reset_gpio = (config_info.wakeup_gpio.gpio);
	ilitek_data->irq_gpio = (config_info.int_number);
#else
	#ifdef CONFIG_OF
	struct device *dev = &(ilitek_data->client->dev);
	struct device_node *np = dev->of_node;
	ilitek_data->reset_gpio = of_get_named_gpio(np, "ilitek,reset-gpio", 0);
	if (ilitek_data->reset_gpio < 0) {
		tp_log_err("reset_gpio = %d\n", ilitek_data->reset_gpio);
	}
	ilitek_data->irq_gpio = of_get_named_gpio(np, "ilitek,irq-gpio", 0);
	if (ilitek_data->irq_gpio < 0) {
		tp_log_err("irq_gpio = %d\n", ilitek_data->irq_gpio);
	}
	#endif
#endif
#else
	ilitek_data->reset_gpio = ILITEK_RESET_GPIO;
	ilitek_data->irq_gpio = ILITEK_IRQ_GPIO;
#endif
	tp_log_info("reset_gpio = %d irq_gpio = %d\n", ilitek_data->reset_gpio, ilitek_data->irq_gpio);
	return ret;
}

int ilitek_request_gpio(void) {
	int ret = 0;
	ilitek_get_gpio_num();
#if ILITEK_PLAT != ILITEK_PLAT_MTK
	if (ilitek_data->reset_gpio > 0) {
		ret= gpio_request(ilitek_data->reset_gpio, "ilitek-reset-gpio");
		if (ret) {
			tp_log_err("Failed to request reset_gpio so free retry\n");
			gpio_free(ilitek_data->reset_gpio);
			ret= gpio_request(ilitek_data->reset_gpio, "ilitek-reset-gpio");
			if (ret) {
				tp_log_err("Failed to request reset_gpio \n");
			}
		}
		if (ret) {
			tp_log_err("Failed to request reset_gpio \n");
		}
		else {
			ret = gpio_direction_output(ilitek_data->reset_gpio, 1);
			if (ret) {
				tp_log_err("Failed to direction output rest gpio err\n");
			}
		}
	}
	if (ilitek_data->irq_gpio > 0) {
		ret= gpio_request(ilitek_data->irq_gpio, "ilitek-irq-gpio");
		if (ret) {
			tp_log_err("Failed to request irq_gpio so free retry\n");
			gpio_free(ilitek_data->irq_gpio);
			ret= gpio_request(ilitek_data->irq_gpio, "ilitek-irq-gpio");
			if (ret) {
				tp_log_err("Failed to request irq_gpio \n");
			}
		}
		if (ret) {
			tp_log_err("Failed to request irq_gpio \n");
		}
		else {
			ret = gpio_direction_input(ilitek_data->irq_gpio);
			if (ret) {
				tp_log_err("Failed to direction input irq gpio err\n");
			}
		}
	}
#endif
	return ret;
}


int	ilitek_power_on(bool status) {
	int ret = 0;
	tp_log_info("%s\n", status ? "POWER ON":"POWER OFF");
#if ILITEK_PLAT != ILITEK_PLAT_ALLWIN
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
	if (status) {
		if (ilitek_data->vdd) {
			ret = regulator_enable(ilitek_data->vdd);
			if (ret < 0) {
				tp_log_err("regulator_enable vdd fail\n");
				return -EINVAL;
			}
		}	
		if (ilitek_data->vdd_i2c) {
			ret = regulator_enable(ilitek_data->vdd_i2c);
			if (ret < 0) {
				tp_log_err("regulator_enable vdd_i2c fail\n");
				return -EINVAL;
			}
		}	
	}
	else {
		if (ilitek_data->vdd) {
			ret = regulator_disable(ilitek_data->vdd);
			if (ret < 0) {
				tp_log_err("regulator_enable vdd fail\n");
				//return -EINVAL;
			}
		}	
		if (ilitek_data->vdd_i2c) {
			ret = regulator_disable(ilitek_data->vdd_i2c);
			if (ret < 0) {
				tp_log_err("regulator_enable vdd_i2c fail\n");
				//return -EINVAL;
			}
		}	
	}
#endif
#else
	input_set_power_enable(&(config_info.input_type), status);
#endif
	return ret;
}

static int ilitek_create_esdandcharge_workqueue(void) {
#ifdef ILITEK_CHARGER_DETECTION
	u8 ChargerStatus[20] = {0};
#endif
#ifdef ILITEK_ESD_PROTECTION
	INIT_DELAYED_WORK(&ilitek_data->esd_work, ilitek_esd_check);
	ilitek_data->esd_wq = create_singlethread_workqueue("ilitek_esd_wq");
	if (!ilitek_data->esd_wq) {
		tp_log_err("create workqueue esd work err\n");
	}
	else {
		ilitek_data->esd_check = true;
		ilitek_data->esd_delay = 2 * HZ;
		queue_delayed_work(ilitek_data->esd_wq, &ilitek_data->esd_work, ilitek_data->esd_delay);
	}
#endif
	
#ifdef ILITEK_CHARGER_DETECTION
	ilitek_read_file(POWER_SUPPLY_BATTERY_STATUS_PATCH, ChargerStatus, 20);
	tp_log_info("*** Battery Status : %s ***\n", ChargerStatus);
	if (strstr(ChargerStatus, "Charging") != NULL || strstr(ChargerStatus, "Full") != NULL || strstr(ChargerStatus, "Fully charged") != NULL) {
		ilitek_into_chargemode(true); // charger plug-in
	}
	else { // Not charging
		ilitek_into_chargemode(false); // charger plug-out
	}
	INIT_DELAYED_WORK(&ilitek_data->charge_work, ilitek_charge_check);
	ilitek_data->charge_wq = create_singlethread_workqueue("ilitek_charge_wq");
	if (!ilitek_data->charge_wq) {
		tp_log_err("create workqueue charge work err\n");
	}
	else {
		ilitek_data->charge_check = true;
		ilitek_data->charge_delay = 2 * HZ;
		queue_delayed_work(ilitek_data->charge_wq, &ilitek_data->charge_work, ilitek_data->charge_delay);
	}
#endif
	return 0;
}

static int ilitek_create_sysfsnode (void) {
	int ret = 0;
	ilitek_data->ilitek_func_kobj = kobject_create_and_add("touchscreen", NULL) ;
	if (ilitek_data->ilitek_func_kobj == NULL) {
		tp_log_err("kobject_create_and_add failed\n");
	}
	else {
		ret = sysfs_create_group(ilitek_data->ilitek_func_kobj, ilitek_attribute_group);
		if (ret < 0) {
			tp_log_err("sysfs_create_group failed\n");
			kobject_put(ilitek_data->ilitek_func_kobj);
		}
	}
	return ret;
}

static int ilitek_register_resume_suspend(void) {
	int ret = 0;
#if ILITEK_PLAT != ILITEK_PLAT_MTK
#ifdef CONFIG_FB
	ilitek_data->fb_notif.notifier_call = ilitek_fb_notifier_callback;
	
	ret = fb_register_client(&ilitek_data->fb_notif);
	
	if (ret) {
		tp_log_err("Unable to register fb_notifier: %d\n", ret);
	}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ilitek_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ilitek_data->early_suspend.suspend = ilitek_early_suspend;
	ilitek_data->early_suspend.resume = ilitek_late_resume;
	register_early_suspend(&ilitek_data->early_suspend);
#endif
#endif
#if ILITEK_PLAT == ILITEK_PLAT_ALLWIN
	device_enable_async_suspend(&ilitek_data->client->dev);
	pm_runtime_set_active(&ilitek_data->client->dev);
	pm_runtime_get(&ilitek_data->client->dev);
	pm_runtime_enable(&ilitek_data->client->dev);
#endif
	return ret;
}

static int ilitek_init_netlink(void) {
#ifdef ILITEK_TUNING_MESSAGE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
		struct netlink_kernel_cfg cfg = {
			.groups = 0,
			.input	= udp_receive,
		};
#endif		
#endif

#ifdef ILITEK_TUNING_MESSAGE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
	ilitek_netlink_sock = netlink_kernel_create(&init_net, 21, &cfg);
#else
	ilitek_netlink_sock = netlink_kernel_create(&init_net, 21, 0,udp_receive, NULL, THIS_MODULE);
#endif
#endif
	return 0;
}

int ilitek_read_tp_info(void) {
	int ret = 0;
	int i = 0;
	unsigned char buf[64] = {0};
	tp_log_info("driver version %d.%d.%d.%d.%d.%d.%d\n", ilitek_driver_information[0], ilitek_driver_information[1],
		ilitek_driver_information[2], ilitek_driver_information[3], ilitek_driver_information[4],
		ilitek_driver_information[5], ilitek_driver_information[6]);
	buf[0] = ILITEK_TP_CMD_GET_KERNEL_VERSION;
	ret = ilitek_i2c_write_and_read(buf, 1, 5, buf, 5);
	if (ret < 0) {
		goto transfer_err;
	}
	for (i = 0; i < 5; i++) {
		ilitek_data->mcu_ver[i] = buf[i];
	}
	tp_log_info("MCU KERNEL version:%d.%d.%d.%d.%d\n", buf[0], buf[1], buf[2],buf[3], buf[4]);
	if ((buf[0] == 0 && buf[1] == 0) || (buf[0] == 0xFF && buf[1] == 0xFF)) {
		ilitek_data->ic_2120 = true;
	}
	else {
		ilitek_data->ic_2120 = false;
		#ifdef ILITEK_GESTURE
		ilitek_data->enable_gesture = true;
		#endif
	}
	if ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25) {
		ilitek_repeat_start = false;
		tp_log_info("ILI2511 not support repeat start\n");
	}
	tp_log_info("ic_2120 is %d\n", ilitek_data->ic_2120);
	if (!ilitek_data->ic_2120) {
		buf[0] = ILITEK_TP_CMD_READ_MODE;
		ret =ilitek_i2c_write_and_read(buf, 1, 5, buf, 2);
		if (ret < 0) {
			goto transfer_err;
		}
		tp_log_info("ilitek ic. mode =%d , it's %s \n",buf[0],((buf[0] == 0x5A)?"AP MODE":"BL MODE"));
#ifdef ILITEK_UPDATE_FW
		if (buf[0] == 0x55) {
			ilitek_data->force_update = true;
		}
#endif
		buf[0] = ILITEK_TP_CMD_GET_FIRMWARE_VERSION;
		ret = ilitek_i2c_write_and_read(buf, 1, 5, buf, 8);
		if (ret < 0) {
			goto transfer_err;
		}
		for (i = 0; i < 8; i++) {
			ilitek_data->firmware_ver[i] = buf[i];
		}
		tp_log_info("firmware version:%d.%d.%d.%d.%d.%d.%d.%d\n",
			buf[0], buf[1], buf[2],buf[3], buf[4], buf[5], buf[6],buf[7]);
		buf[0] = ILITEK_TP_CMD_GET_PROTOCOL_VERSION;
		ret = ilitek_i2c_write_and_read(buf, 1, 5, buf, 2);
		if (ret < 0) {
			goto transfer_err;
		}
		ilitek_data->protocol_ver = (((int)buf[0]) << 8) + buf[1];
		tp_log_info("protocol version: %d.%d  ilitek_data->protocol_ver = 0x%x\n", buf[0], buf[1], ilitek_data->protocol_ver);
		buf[0] = ILITEK_TP_CMD_GET_SCREEN_RESOLUTION;
		ret = ilitek_i2c_write_and_read(buf, 1, 5, buf, 8);
		if (ret < 0) {
			goto transfer_err;
		}
		ilitek_data->screen_min_x = buf[0];
		ilitek_data->screen_min_x+= ((int)buf[1]) * 256;
		ilitek_data->screen_min_y = buf[2];
		ilitek_data->screen_min_y+= ((int)buf[3]) * 256;
		ilitek_data->screen_max_x = buf[4];
		ilitek_data->screen_max_x+= ((int)buf[5]) * 256;
		ilitek_data->screen_max_y = buf[6];
		ilitek_data->screen_max_y+= ((int)buf[7]) * 256;
		tp_log_info("screen_min_x: %d, screen_min_y: %d screen_max_x: %d, screen_max_y: %d\n",
			ilitek_data->screen_min_x, ilitek_data->screen_min_y, ilitek_data->screen_max_x, ilitek_data->screen_max_y);
		buf[0] = ILITEK_TP_CMD_GET_TP_RESOLUTION;
		ret = ilitek_i2c_write_and_read(buf, 1, 5, buf, 10);
		if (ret < 0) {
			goto transfer_err;
		}
	
		ilitek_data->max_tp = buf[6];
		ilitek_data->max_btn = buf[7];
		ilitek_data->keycount = buf[8];
		if (ilitek_data->keycount > 20) {
			tp_log_info("exception keycount > 20 is %d set keycount = 0\n", ilitek_data->keycount);
			ilitek_data->keycount = 0;
		}
		ilitek_data->tp_max_x = buf[0];
		ilitek_data->tp_max_x+= ((int)buf[1]) * 256;
		ilitek_data->tp_max_y = buf[2];
		ilitek_data->tp_max_y+= ((int)buf[3]) * 256;
		ilitek_data->x_ch = buf[4];
		ilitek_data->y_ch = buf[5];
		if (ilitek_data->keycount > 0) {
			//get key infotmation
			buf[0] = ILITEK_TP_CMD_GET_KEY_INFORMATION;
			ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 29);
			if (ret < 0) {
				goto transfer_err;
			}
			if (ilitek_data->keycount > 5) {
				for (i = 0; i < ((ilitek_data->keycount % 5) ? (((ilitek_data->keycount - 5) / 5) + 1) : ((ilitek_data->keycount - 5) / 5)); i++) {
					tp_log_info("read keyinfo times i = %d\n", i);
					ret = ilitek_i2c_write_and_read(buf, 0, 10, buf + 29 + 25 * i, 25);
					if (ret < 0) {
						goto transfer_err;
					}
				}
			}
			
			ilitek_data->key_xlen = (buf[0] << 8) + buf[1];
			ilitek_data->key_ylen = (buf[2] << 8) + buf[3];
			tp_log_info("key_xlen: %d, key_ylen: %d\n", ilitek_data->key_xlen, ilitek_data->key_ylen);
			
			//print key information
			for(i = 0; i < ilitek_data->keycount; i++){
				ilitek_data->keyinfo[i].id = buf[i*5+4]; 
				ilitek_data->keyinfo[i].x = (buf[i*5+5] << 8) + buf[i*5+6];
				ilitek_data->keyinfo[i].y = (buf[i*5+7] << 8) + buf[i*5+8];
				ilitek_data->keyinfo[i].status = 0;
				tp_log_info("key_id: %d, key_x: %d, key_y: %d, key_status: %d\n",
					ilitek_data->keyinfo[i].id, ilitek_data->keyinfo[i].x, ilitek_data->keyinfo[i].y, ilitek_data->keyinfo[i].status);
			}
		}
	}
	else {
		for (i = 0; i < 3; i++) {
			buf[0] = ILITEK_TP_CMD_GET_TOUCH_INFORMATION;
			ilitek_i2c_write_and_read(buf, 1, 10, buf, 3);
			tp_log_info("ilitek %s, write 0x10 read buf = %X, %X, %X\n", __func__, buf[0], buf[1], buf[2]);
			if (buf[1] >= 0x80) {
				tp_log_info("FW is ready  ok ok \n");
				break;
			}else {
				mdelay(5);
			}
		}
		if (i >= 3) {
#ifdef ILITEK_UPDATE_FW
			tp_log_err("ilitek wirte 0x10 read data error (< 0x80) so set force_update = true\n");
			ilitek_data->force_update = true;
#endif
		}
		buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL_2120;
		buf[1] = ILITEK_TP_CMD_GET_FIRMWARE_VERSION_2120;
		ret = ilitek_i2c_write_and_read(buf, 2, 10, buf, 0);
		if (ret < 0) {
			goto transfer_err;
		}
		buf[0] = ILITEK_TP_CMD_GET_FIRMWARE_VERSION_2120;
		ret = ilitek_i2c_write_and_read(buf, 1, 0, buf, 4);
		if (ret < 0) {
			goto transfer_err;
		}
		tp_log_info(" firmware version:%d.%d.%d.%d\n", buf[0], buf[1], buf[2], buf[3]);
		ilitek_data->firmware_ver[0] = 0;
		for(i = 1; i < 4; i++)
		{
			ilitek_data->firmware_ver[i] = buf[i - 1];
			if (ilitek_data->firmware_ver[i] == 0xFF) {
				tp_log_info("firmware version:[%d] = 0xFF so set 0 \n", i);
				ilitek_data->firmware_ver[1] = 0;
				ilitek_data->firmware_ver[2] = 0;
				ilitek_data->firmware_ver[3] = 0;
				break;
			}
		}
		
		buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL_2120;
		buf[1] = ILITEK_TP_CMD_GET_PROTOCOL_VERSION_2120;
		ret = ilitek_i2c_write_and_read(buf, 2, 10, buf, 0);
		if (ret < 0) {
			goto transfer_err;
		}
		
		buf[0] = ILITEK_TP_CMD_GET_PROTOCOL_VERSION_2120;
		ret = ilitek_i2c_write_and_read(buf, 1, 0, buf, 2);
		if (ret < 0) {
			goto transfer_err;
		}
		ilitek_data->protocol_ver = (((int)buf[0]) << 8) + buf[1];
		tp_log_info("protocol version:%d.%d\n", buf[0], buf[1]);
		
		buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL_2120;
		buf[1] = ILITEK_TP_CMD_GET_TP_RESOLUTION;
		ret = ilitek_i2c_write_and_read(buf, 2, 10, buf, 0);
		if (ret < 0) {
			goto transfer_err;
		}
		buf[0] = ILITEK_TP_CMD_GET_TP_RESOLUTION;
		ret = ilitek_i2c_write_and_read(buf, 1, 0, buf, 10);
		if (ret < 0) {
			goto transfer_err;
		}
		
		ilitek_data->screen_max_x = buf[2];
		ilitek_data->screen_max_x+= ((int)buf[3]) * 256;
		ilitek_data->screen_max_y = buf[4];
		ilitek_data->screen_max_y+= ((int)buf[5]) * 256;
		ilitek_data->screen_min_x = buf[0];
		ilitek_data->screen_min_y = buf[1];
		ilitek_data->x_ch = buf[6];
		ilitek_data->y_ch = buf[7];
		ilitek_data->max_tp = buf[8];
		ilitek_data->keycount = buf[9];
		if (ilitek_data->keycount > 20) {
			tp_log_info("exception keycount > 20 is %d set keycount = 0\n", ilitek_data->keycount);
			ilitek_data->keycount = 0;
		}
		tp_log_info("screen_max_x: %d, screen_max_y: %d, screen_min_x: %d, screen_min_y: %d key_count: %d\n",
			ilitek_data->screen_max_x, ilitek_data->screen_max_y, ilitek_data->screen_min_x, ilitek_data->screen_min_y, ilitek_data->keycount);
		if (ilitek_data->keycount > 0) {
			buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL_2120;
			buf[1] = ILITEK_TP_CMD_GET_KEY_INFORMATION;
			ret = ilitek_i2c_write_and_read(buf, 2, 10, buf, 0);
			if (ret < 0) {
				goto transfer_err;
			}
			buf[0] = ILITEK_TP_CMD_GET_KEY_INFORMATION;
			ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 29);
			if (ret < 0) {
				tp_log_err("ilitek read ILITEK_TP_CMD_GET_KEY_INFORMATION err\n");
				goto transfer_err;
			}
			if (ilitek_data->keycount > 5) {
				for (i = 0; i < ((ilitek_data->keycount % 5) ? (((ilitek_data->keycount - 5) / 5) + 1) : ((ilitek_data->keycount - 5) / 5)); i++) {
					tp_log_info("ilitek i = %d\n", i);
					ret = ilitek_i2c_write_and_read(buf, 0, 10, buf + 29 + 25 * i, 25);
					if (ret < 0) {
						tp_log_err("ilitek read ILITEK_TP_CMD_GET_KEY_INFORMATION err\n");
						goto transfer_err;
					}
				}
			}
			
			ilitek_data->key_xlen = (buf[0] << 8) + buf[1];
			ilitek_data->key_ylen = (buf[2] << 8) + buf[3];
			tp_log_info("key_xlen: %d, key_ylen: %d\n", ilitek_data->key_xlen, ilitek_data->key_ylen);
			
			//print key information
			for(i = 0; i < ilitek_data->keycount; i++){
				ilitek_data->keyinfo[i].id = buf[i*5+4]; 
				ilitek_data->keyinfo[i].x = (buf[i*5+5] << 8) + buf[i*5+6];
				ilitek_data->keyinfo[i].y = (buf[i*5+7] << 8) + buf[i*5+8];
				ilitek_data->keyinfo[i].status = 0;
				tp_log_info("key_id: %d, key_x: %d, key_y: %d, key_status: %d\n", ilitek_data->keyinfo[i].id,
					ilitek_data->keyinfo[i].x, ilitek_data->keyinfo[i].y, ilitek_data->keyinfo[i].status);
			}
		}
	}
	tp_log_info("tp_min_x: %d, tp_max_x: %d, tp_min_y: %d, tp_max_y: %d, ch_x: %d, ch_y: %d, max_tp: %d, key_count: %d\n",
		ilitek_data->tp_min_x, ilitek_data->tp_max_x, ilitek_data->tp_min_y, ilitek_data->tp_max_y, ilitek_data->x_ch,
		ilitek_data->y_ch, ilitek_data->max_tp, ilitek_data->keycount);
transfer_err:
	return ret; 
}

int ilitek_main_probe(struct ilitek_ts_data * ilitek_ts_data) {
	int ret = 0;

	tp_log_info("\n");
	mutex_init(&ilitek_data->ilitek_mutex);
	ret = ilitek_power_on(true);
	ret = ilitek_request_gpio();
	ilitek_reset(200);
	ret = ilitek_read_tp_info();
	if (ret < 0) {
		tp_log_err("init read tp info error so exit\n");
		goto read_info_err;
	}
#ifdef ILITEK_USE_MTK_INPUT_DEV
	ilitek_data->input_dev = tpd->dev;
#else
	ilitek_data->input_dev = input_allocate_device();
#endif
	if(NULL == ilitek_data->input_dev){
		tp_log_err("allocate input device, error\n");
		goto read_info_err;
	}
#ifndef ILITEK_UPDATE_FW
	ret = ilitek_set_input_param();
	if (ret) {
		tp_log_err("register input device, error\n");
		goto input_register_err;
	}
	ret = ilitek_request_irq();
	if (ret) {
		tp_log_err("ilitek_request_irq, error\n");
		goto input_register_err;
	}
#endif
	ilitek_data->irq_thread = kthread_run(ilitek_irq_handle_thread, NULL, "ilitek_irq_thread");
	if (ilitek_data->irq_thread == (struct task_struct*)ERR_PTR){
		ilitek_data->irq_thread = NULL;
		tp_log_err("kthread create ilitek_irq_handle_thread, error\n");
		goto kthread_run_irq_thread_err;
	}
#ifdef ILITEK_UPDATE_FW
	ilitek_data->update_thread = kthread_run(ilitek_update_thread, NULL, "ilitek_update_thread");
	if (ilitek_data->update_thread == (struct task_struct*)ERR_PTR){
		ilitek_data->update_thread = NULL;
		tp_log_err("kthread create ilitek_update_thread, error\n");
	}
#endif
	ilitek_register_resume_suspend();

	ilitek_create_sysfsnode();

#ifdef ILITEK_TOOL
	ilitek_create_tool_node();
#endif
	ilitek_init_netlink();

	ilitek_create_esdandcharge_workqueue();
	device_init_wakeup(&ilitek_data->client->dev, 1);
	return 0;
kthread_run_irq_thread_err:
#ifndef ILITEK_UPDATE_FW
	free_irq(ilitek_data->client->irq, ilitek_data);
input_register_err:
	input_free_device(ilitek_data->input_dev);
#endif
read_info_err:
#if ILITEK_PLAT != ILITEK_PLAT_ALLWIN
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
	ilitek_regulator_release();
#endif
#endif
	ilitek_free_gpio();
	kfree(ilitek_data);
	return -ENODEV;
}

int ilitek_main_remove(struct ilitek_ts_data * ilitek_data) {
	tp_log_info("\n");
	free_irq(ilitek_data->client->irq, ilitek_data);

#ifdef ILITEK_TUNING_MESSAGE
	netlink_kernel_release(ilitek_netlink_sock);
#endif

#ifdef CONFIG_FB
	fb_unregister_client(&ilitek_data->fb_notif);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ilitek_data->early_suspend);
#endif
	if(ilitek_data->irq_thread != NULL){
		tp_log_info("irq_thread\n");
		ilitek_exit_report = true;
		ilitek_data->irq_trigger = true;
		wake_up_interruptible(&waiter);
		kthread_stop(ilitek_data->irq_thread);
		ilitek_data->irq_thread = NULL;
	}
	if (ilitek_data->input_dev) {
		input_unregister_device(ilitek_data->input_dev);
		ilitek_data->input_dev = NULL;
	}
#ifdef ILITEK_TOOL
	ilitek_remove_tool_node();
#endif
	if (ilitek_data->ilitek_func_kobj) {
		sysfs_remove_group(ilitek_data->ilitek_func_kobj, ilitek_attribute_group);
		kobject_put(ilitek_data->ilitek_func_kobj);
		ilitek_data->ilitek_func_kobj = NULL;
	}
#ifdef ILITEK_ESD_PROTECTION
		if (ilitek_data->esd_wq) {
			destroy_workqueue(ilitek_data->esd_wq);
			ilitek_data->esd_wq = NULL;
		}
#endif
#ifdef ILITEK_CHARGER_DETECTION
		if (ilitek_data->charge_wq) {
			destroy_workqueue(ilitek_data->charge_wq);
			ilitek_data->charge_wq = NULL;
		}
#endif

#if ILITEK_PLAT != ILITEK_PLAT_ALLWIN
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
		ilitek_regulator_release();
#endif
#endif

	ilitek_free_gpio();
	kfree(ilitek_data);
	return 0;
}
