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
#ifdef ILITEK_TOOL
// device data
struct dev_data {
	// device number
	dev_t devno;
	// character device
	struct cdev cdev;
	// class device
	struct class *class;
};

static struct dev_data ilitek_dev;
static struct proc_dir_entry * ilitek_proc;
struct proc_dir_entry *ilitek_proc_entry;

static char ilitek_hex_path[256] = "/data/local/tmp/2511dftest.hex";

// define the application command
#define ILITEK_IOCTL_BASE                       100
#define ILITEK_IOCTL_I2C_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 0, unsigned char*)
#define ILITEK_IOCTL_I2C_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 1, int)
#define ILITEK_IOCTL_I2C_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 2, unsigned char*)
#define ILITEK_IOCTL_I2C_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 3, int)
#define ILITEK_IOCTL_USB_WRITE_DATA             _IOWR(ILITEK_IOCTL_BASE, 4, unsigned char*)
#define ILITEK_IOCTL_USB_WRITE_LENGTH           _IOWR(ILITEK_IOCTL_BASE, 5, int)
#define ILITEK_IOCTL_USB_READ_DATA              _IOWR(ILITEK_IOCTL_BASE, 6, unsigned char*)
#define ILITEK_IOCTL_USB_READ_LENGTH            _IOWR(ILITEK_IOCTL_BASE, 7, int)

#define ILITEK_IOCTL_DRIVER_INFORMATION		    _IOWR(ILITEK_IOCTL_BASE, 8, int)
#define ILITEK_IOCTL_USB_UPDATE_RESOLUTION      _IOWR(ILITEK_IOCTL_BASE, 9, int)

#define ILITEK_IOCTL_I2C_INT_FLAG	            _IOWR(ILITEK_IOCTL_BASE, 10, int)
#define ILITEK_IOCTL_I2C_UPDATE                 _IOWR(ILITEK_IOCTL_BASE, 11, int)
#define ILITEK_IOCTL_STOP_READ_DATA             _IOWR(ILITEK_IOCTL_BASE, 12, int)
#define ILITEK_IOCTL_START_READ_DATA            _IOWR(ILITEK_IOCTL_BASE, 13, int)
#define ILITEK_IOCTL_GET_INTERFANCE				_IOWR(ILITEK_IOCTL_BASE, 14, int)//default setting is i2c interface
#define ILITEK_IOCTL_I2C_SWITCH_IRQ				_IOWR(ILITEK_IOCTL_BASE, 15, int)

#define ILITEK_IOCTL_UPDATE_FLAG				_IOWR(ILITEK_IOCTL_BASE, 16, int)
#define ILITEK_IOCTL_I2C_UPDATE_FW				_IOWR(ILITEK_IOCTL_BASE, 18, int)
#define ILITEK_IOCTL_RESET						_IOWR(ILITEK_IOCTL_BASE, 19, int)
#define ILITEK_IOCTL_INT_STATUS						_IOWR(ILITEK_IOCTL_BASE, 20, int)
#ifdef ILITEK_TUNING_MESSAGE
extern bool ilitek_debug_flag;

#define ILITEK_IOCTL_DEBUG_SWITCH						_IOWR(ILITEK_IOCTL_BASE, 21, int)
#endif



static int ilitek_file_open(struct inode *inode, struct file *filp) {
	ilitek_data->operation_protection = true;
	tp_log_info("operation_protection = %d\n", ilitek_data->operation_protection);
	return 0;
}
static ssize_t ilitek_file_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {
	int ret;
	unsigned char buffer[512]={0};
	// check the buffer size whether it exceeds the local buffer size or not
	if (count > 512) {
		tp_log_err("%s, buffer exceed 512 bytes\n", __func__);
		//return -1;
	}

	// copy data from user space
	ret = copy_from_user(buffer, buf, count-1);
	if (ret < 0) {
		tp_log_err("%s, copy data from user space, failed", __func__);
		return -1;
	}

	if (buffer[count -2] == 'I' && (count == 20 || count == 52) && buffer[0] == 0x77 && buffer[1] == 0x77) {
		
		tp_log_info("IOCTL_WRITE CMD = %d\n", buffer[2]);
		switch (buffer[2]) {
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
				tp_log_info("ilitek The ilitek_debug_flag = %d.\n", buffer[3]);
				if (buffer[3] == 0) {
					ilitek_debug_flag = false;
				}
				else if (buffer[3] == 1) {
					ilitek_debug_flag = true;
				}
				break;
#endif
			case 15:
				if (buffer[3] == 0) {
					ilitek_irq_disable();
					tp_log_info("ilitek_irq_disable.\n");
				}
				else {
					ilitek_irq_enable();
					tp_log_info("ilitek_irq_enable.\n");
				}
				break;
			case 16:
				ilitek_data->operation_protection = buffer[3];
				tp_log_info("ilitek_data->operation_protection = %d\n", ilitek_data->operation_protection);
				break;
			case 18:
				ret = ilitek_i2c_write(&buffer[3], 33);
				if (ret < 0) {
					tp_log_err("i2c write error, ret %d, addr %x \n", ret,ilitek_data->client->addr);
					return ret;
				}
				return 0;
				break;
				default:
					return -1;
		}
	}
	if (buffer[count -2] == 'W') {
		ret = ilitek_i2c_write(buffer, count -2);
		if(ret < 0){
			tp_log_err("i2c write error, ret %d, addr %x \n", ret,ilitek_data->client->addr);
			return ret;
		}
	}
	else if (strcmp(buffer, "dbg_debug") == 0) {
		ilitek_log_level_value = ILITEK_DEBUG_LOG_LEVEL;
		tp_log_info("ilitek_log_level_value = %d.\n", ilitek_log_level_value);
	}
	else if (strcmp(buffer, "dbg_info") == 0) {
		ilitek_log_level_value = ILITEK_INFO_LOG_LEVEL;
		tp_log_info("ilitek_log_level_value = %d.\n", ilitek_log_level_value);
	}
	else if (strcmp(buffer, "dbg_err") == 0) {
		ilitek_log_level_value = ILITEK_ERR_LOG_LEVEL;
		tp_log_info("ilitek_log_level_value = %d.\n", ilitek_log_level_value);
	}
	#ifdef ILITEK_TUNING_MESSAGE
	else if(strcmp(buffer, "truning_dbg_flag") == 0){
		ilitek_debug_flag = !ilitek_debug_flag;
		tp_log_info(" %s debug_flag message(%X).\n", ilitek_debug_flag?"Enabled":"Disabled",ilitek_debug_flag);
	}
	#endif
	else if(strcmp(buffer, "irq_status") == 0){
		tp_log_info("gpio_get_value(i2c.irq_gpio) = %d.\n", gpio_get_value(ilitek_data->irq_gpio));
	}
	else if(strcmp(buffer, "enable") == 0){
		enable_irq(ilitek_data->client->irq);
		tp_log_info("irq enable\n");
	}else if(strcmp(buffer, "disable") == 0){
		disable_irq(ilitek_data->client->irq);
		tp_log_info("irq disable\n");
	}
	else if(strcmp(buffer, "info") == 0){
		ilitek_read_tp_info();
	}
	else if(strcmp(buffer, "reset") == 0){
		ilitek_reset(200);
	}
	tp_log_debug("ilitek return count = %d\n", (int)count);
	return count;
}

/*
   description
   ioctl function for character device driver
   prarmeters
   inode
   file node
   filp
   file pointer
   cmd
   command
   arg
   arguments
   return
   status
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long ilitek_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int  ilitek_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	static unsigned char buffer[64]={0};
	static int len = 0, i;
	int ret;
	// parsing ioctl command
	switch(cmd){
		case ILITEK_IOCTL_I2C_WRITE_DATA:
			ret = copy_from_user(buffer, (unsigned char*)arg, len);
			if(ret < 0){
				tp_log_err("copy data from user space, failed\n");
				return -1;
			}
			ret = ilitek_i2c_write(buffer, len);
			if(ret < 0){
				tp_log_err("i2c write, failed\n");
				return -1;
			}
			break;
		case ILITEK_IOCTL_I2C_READ_DATA:
			ret = ilitek_i2c_read(buffer, len);
			if(ret < 0){
				tp_log_err("i2c read, failed\n");
				return -1;
			}
			ret = copy_to_user((unsigned char*)arg, buffer, len);
			if(ret < 0){
				tp_log_err("copy data to user space, failed\n");
				return -1;
			}
			break;
		case ILITEK_IOCTL_I2C_WRITE_LENGTH:
		case ILITEK_IOCTL_I2C_READ_LENGTH:
			len = arg;
			break;
		case ILITEK_IOCTL_DRIVER_INFORMATION:
			for (i = 0; i < 7; i++) {
				buffer[i] = ilitek_driver_information[i];
			}
			ret = copy_to_user((unsigned char*)arg, buffer, 7);
			break;
		case ILITEK_IOCTL_I2C_UPDATE:
			break;
		case ILITEK_IOCTL_I2C_INT_FLAG:
			buffer[0] = !(gpio_get_value(ilitek_data->irq_gpio));
			ret = copy_to_user((unsigned char*)arg, buffer, 1);
			if (ret < 0) {
				tp_log_err("copy data to user space, failed\n");
				return -1;
			}
			tp_log_info("ILITEK_IOCTL_I2C_INT_FLAG = %d.\n", buffer[0]);
			break;
		case ILITEK_IOCTL_START_READ_DATA:
			//ilitek_irq_enable();
			tp_log_info("ILITEK_IOCTL_START_READ_DATA do nothing.\n");
			break;
		case ILITEK_IOCTL_STOP_READ_DATA:
			//ilitek_irq_disable();
			tp_log_info("ILITEK_IOCTL_STOP_READ_DATA do nothing.\n");
			break;
		case ILITEK_IOCTL_RESET:
			ilitek_reset(200);
			break;
		case ILITEK_IOCTL_INT_STATUS:
			put_user(gpio_get_value(ilitek_data->irq_gpio), (int *)arg);
			break;	
	#ifdef ILITEK_TUNING_MESSAGE
		case ILITEK_IOCTL_DEBUG_SWITCH:
			ret = copy_from_user(buffer, (unsigned char*)arg, 1);
			tp_log_info("ilitek The debug_flag = %d.\n", buffer[0]);
			if (buffer[0] == 0) {
				ilitek_debug_flag = false;
			}
			else if (buffer[0] == 1) {
				ilitek_debug_flag = true;
			}
			break;
	#endif
		case ILITEK_IOCTL_I2C_SWITCH_IRQ:
			ret = copy_from_user(buffer, (unsigned char*)arg, 1);
			if (buffer[0] == 0)
			{
				tp_log_info("ilitek_i2c_irq_disable .  \n");
				ilitek_irq_disable();
			}
			else
			{
				tp_log_info("ilitek_i2c_irq_enable. \n");
				ilitek_irq_enable();
			}
			break;
		case ILITEK_IOCTL_UPDATE_FLAG:
			ilitek_data->operation_protection = arg;
			tp_log_info("operation_protection = %d\n", ilitek_data->operation_protection);
			break;
		case ILITEK_IOCTL_I2C_UPDATE_FW:
			ret = copy_from_user(buffer, (unsigned char*)arg, 35);
			if(ret < 0){
				tp_log_err("copy data from user space, failed\n");
				return -1;
			}
			ret = ilitek_i2c_write(buffer, buffer[34]);
			if(ret < 0){
				tp_log_err("i2c write, failed\n");
				return -1;
			}
			break;

		default:
			return -1;
	}
	return 0;
}

/*
   description
   read function for character device driver
   prarmeters
   filp
   file pointer
   buf
   buffer
   count
   buffer length
   f_pos
   offset
   return
   status
 */
static ssize_t ilitek_file_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	unsigned char *tmp;
	int ret;
	long rc;

	//tp_log_info("%s enter count = %d\n", __func__, count);

	if (count > 8192)
		count = 8192;

	tmp = kmalloc(count, GFP_KERNEL);
	if (tmp == NULL)
		return -ENOMEM;

	ret = ilitek_i2c_read(tmp, count);
	if (ret < 0)
		tp_log_err("i2c read error, ret %d,addr %x \n", ret, ilitek_data->client->addr);

	rc = copy_to_user(buf, tmp, count);

	kfree(tmp);
	return ret;
}


/*
   description
   close function
   prarmeters
   inode
   inode
   filp
   file pointer
   return
   status
 */
static int ilitek_file_close(struct inode *inode, struct file *filp)
{
	ilitek_data->operation_protection = false;
	tp_log_info("operation_protection = %d\n", ilitek_data->operation_protection);
	return 0;
}

// declare file operations
struct file_operations ilitek_fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	.unlocked_ioctl = ilitek_file_ioctl,
#else
	.ioctl = ilitek_file_ioctl,
#endif
	.read = ilitek_file_read,
	.write = ilitek_file_write,
	.open = ilitek_file_open,
	.release = ilitek_file_close,
};

static int short_test_result = 0;
static int open_test_result = 0;
static int allnode_test_result = 0;
static int ilitek_short_threshold = 7;
static int ilitek_open_threshold = 0;//200;
static int ilitek_allnode_max_threshold = 8500;
static int ilitek_allnode_min_threshold = 0;//1500;
static int ilitek_open_txdeltathrehold = 120;
static int ilitek_open_rxdeltathrehold = 120;
static int ilitek_allnodetestw1 = 140;
static int ilitek_allnodetestw2 = 140;
static int ilitek_allnodemodule= 20;
static int ilitek_allnodetx= 3;

static int ilitek_printsensortestdata = 1;
static char sensor_test_data_path[256] = "/data/local/tmp/";
static char sensor_test_data_path_tmp[256] = "/data/local/tmp/";

static int noisefre_start = 30;
static int noisefre_end = 120;
static int noisefre_step = 5;
static char noisefre_data_path[256] = "/data/local/tmp/";
static char noisefre_data_path_tmp[256] = "/data/local/tmp/";
static void ilitek_printf_sensortest_data(int * short_xdata1, int * short_xdata2, int * short_ydata1,
	int * short_ydata2, int * open_data, int * allnode_data, struct seq_file *m) {
	int j = 0, len = 0;
	struct file *filp;
	mm_segment_t fs;
	unsigned char buf[128];
    struct  timeval   time_now;
	struct rtc_time tm; 
    do_gettimeofday(&time_now);
	rtc_time_to_tm(time_now.tv_sec, &tm); 
	tp_log_info("%d_%d_%d_%d_%d_%d\n", (tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (short_test_result == 0 && open_test_result == 0 && allnode_test_result ==0) {
		len = sprintf(buf, "ilitek_sensortest_%d%02d%02d%02d%02d%02d_pass.csv", (tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	}
	else {
		len = sprintf(buf, "ilitek_sensortest_%d%02d%02d%02d%02d%02d_fail.csv", (tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	}
	for(j = 0; j < 256; j++) {
		sensor_test_data_path_tmp[j] = sensor_test_data_path[j];
	}
	strcat(sensor_test_data_path, buf);
	tp_log_info("sensor_test_data_path = %s\n", sensor_test_data_path);
	filp = filp_open(sensor_test_data_path, O_CREAT | O_WRONLY, 0777);
	if(IS_ERR(filp)) {
		tp_log_err("save sensor test  File Open Error path = %s\n", sensor_test_data_path);
	}
	else {
		fs = get_fs();
		set_fs(KERNEL_DS);

		len = sprintf(buf, "short\n");
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		len = sprintf(buf, "short_threshold = %d\n", ilitek_short_threshold);
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		for (j = 0; j < ilitek_data->x_ch; j++) {
			len = sprintf(buf, "%d,", short_xdata1[j]);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
		}
		len = sprintf(buf, "\n");
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		for (j = 0; j < ilitek_data->x_ch; j++) {
			len = sprintf(buf, "%d,", short_xdata2[j]);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
		}
		len = sprintf(buf, "\n");
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		if (!(ilitek_data->ic_2120)) {
			len = sprintf(buf, "\n");
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
			for (j = 0; j < ilitek_data->y_ch; j++) {
				len = sprintf(buf, "%d,", short_ydata1[j]);
				filp->f_op->write(filp, buf, len, &(filp->f_pos));
			}
			len = sprintf(buf, "\n");
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
			for (j = 0; j < ilitek_data->y_ch; j++) {
				len = sprintf(buf, "%d,", short_ydata2[j]);
				filp->f_op->write(filp, buf, len, &(filp->f_pos));
			}
		}
		
		len = sprintf(buf, "\n");
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		len = sprintf(buf, "open:\n");
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		len = sprintf(buf, "open_threshold = %d\n", ilitek_open_threshold);
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		if (!(ilitek_data->ic_2120)) {
			len = sprintf(buf, "open_txdeltathrehold = %d\n", ilitek_open_txdeltathrehold);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
			len = sprintf(buf, "open_rxdeltathrehold = %d\n", ilitek_open_rxdeltathrehold);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
		}
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			len = sprintf(buf, "%d,", open_data[j]);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
			if((j + 1) % ilitek_data->x_ch == 0) {
				len = sprintf(buf, "\n");
				filp->f_op->write(filp, buf, len, &(filp->f_pos));
			}
		}
		len = sprintf(buf, "\n");
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		len = sprintf(buf, "allnode:\n");
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		len = sprintf(buf, "allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		len = sprintf(buf, "allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		if (!(ilitek_data->ic_2120)) {
			len = sprintf(buf, "allnodetestw1 = %d\n", ilitek_allnodetestw1);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
			len = sprintf(buf, "allnodetestw2 = %d\n", ilitek_allnodetestw2);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
			len = sprintf(buf, "allnodemodule = %d\n", ilitek_allnodemodule);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
			len = sprintf(buf, "allnodetx = %d\n", ilitek_allnodetx);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
		}
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			len = sprintf(buf, "%d,", allnode_data[j]);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
			if((j + 1) % ilitek_data->x_ch == 0) {
				len = sprintf(buf, "\n");
				filp->f_op->write(filp, buf, len, &(filp->f_pos));
			}
		}
		len = sprintf(buf, "\n");
		filp->f_op->write(filp, buf, len, &(filp->f_pos));

		set_fs(fs);
	}
	for(j = 0; j < 256; j++) {
		sensor_test_data_path[j] = sensor_test_data_path_tmp[j];
	}

	if (ilitek_printsensortestdata) {
		if (short_test_result == 0 && open_test_result == 0 && allnode_test_result ==0) {
			printk("pass\n");
		}
		else {
			printk("fail\n");
		}
		
		printk("%s", "\n");
		printk("%s", "short\n");
		printk("short_threshold = %d\n", ilitek_short_threshold);
		
		for (j = 0; j < ilitek_data->x_ch; j++) {
			printk("%d,", short_xdata1[j]);
		}
		printk("%s", "\n");
		for (j = 0; j < ilitek_data->x_ch; j++) {
			printk( "%d,", short_xdata2[j]);
		}
		printk( "%s", "\n");
		if (!(ilitek_data->ic_2120)) {
			printk( "%s", "\n");
			for (j = 0; j < ilitek_data->y_ch; j++) {
				printk( "%d,", short_ydata1[j]);
			}
			printk( "%s", "\n");
			for (j = 0; j < ilitek_data->y_ch; j++) {
				printk( "%d,", short_ydata2[j]);
			}
		}
		
		printk( "%s", "\n");
		printk( "%s", "open:\n");
		printk( "open_threshold = %d\n", ilitek_open_threshold);
		if (!(ilitek_data->ic_2120)) {
			printk( "open_txdeltathrehold = %d\n", ilitek_open_txdeltathrehold);
			printk( "open_rxdeltathrehold = %d\n", ilitek_open_rxdeltathrehold);
		}
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			printk( "%d,", open_data[j]);
			if((j + 1) % ilitek_data->x_ch == 0) {
				printk( "%s", "\n");
			}
		}
		printk( "%s", "\n");
		printk( "%s", "allnode:\n");
		printk( "allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
		printk( "allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
		if (!(ilitek_data->ic_2120)) {
			printk( "allnodetestw1 = %d\n", ilitek_allnodetestw1);
			printk( "allnodetestw2 = %d\n", ilitek_allnodetestw2);
			printk( "allnodemodule = %d\n", ilitek_allnodemodule);
			printk( "allnodetx = %d\n", ilitek_allnodetx);
		}
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			printk( "%d,", allnode_data[j]);
			if((j + 1) % ilitek_data->x_ch == 0) {
				printk( "%s", "\n");
			}
		}
		printk( "%s", "\n");
	}

	if (ilitek_printsensortestdata) {
		if (short_test_result == 0 && open_test_result == 0 && allnode_test_result ==0) {
			seq_printf(m, "pass\n");
		}
		else {
			seq_printf(m, "fail\n");
		}
		seq_printf(m, "%s", "\n");
		seq_printf(m, "%s", "short\n");
		seq_printf(m, "short_threshold = %d\n", ilitek_short_threshold);
		
		for (j = 0; j < ilitek_data->x_ch; j++) {
			seq_printf(m, "%d,", short_xdata1[j]);
		}
		seq_printf(m, "%s", "\n");
		for (j = 0; j < ilitek_data->x_ch; j++) {
			seq_printf(m, "%d,", short_xdata2[j]);
		}
		seq_printf(m, "%s", "\n");
		if (!(ilitek_data->ic_2120)) {
			seq_printf(m, "%s", "\n");
			for (j = 0; j < ilitek_data->y_ch; j++) {
				seq_printf(m, "%d,", short_ydata1[j]);
			}
			seq_printf(m, "%s", "\n");
			for (j = 0; j < ilitek_data->y_ch; j++) {
				seq_printf(m, "%d,", short_ydata2[j]);
			}
		}
		
		seq_printf(m, "%s", "\n");
		seq_printf(m, "%s", "open:\n");
		seq_printf(m, "open_threshold = %d\n", ilitek_open_threshold);
		if (!(ilitek_data->ic_2120)) {
			seq_printf(m, "open_txdeltathrehold = %d\n", ilitek_open_txdeltathrehold);
			seq_printf(m, "open_rxdeltathrehold = %d\n", ilitek_open_rxdeltathrehold);
		}
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			seq_printf(m, "%d,", open_data[j]);
			if((j + 1) % ilitek_data->x_ch == 0) {
				seq_printf(m, "%s", "\n");
			}
		}
		seq_printf(m, "%s", "\n");
		seq_printf(m, "%s", "allnode:\n");
		seq_printf(m, "allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
		seq_printf(m, "allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
		if (!(ilitek_data->ic_2120)) {
			seq_printf(m, "allnodetestw1 = %d\n", ilitek_allnodetestw1);
			seq_printf(m, "allnodetestw2 = %d\n", ilitek_allnodetestw2);
			seq_printf(m, "allnodemodule = %d\n", ilitek_allnodemodule);
			seq_printf(m, "allnodetx = %d\n", ilitek_allnodetx);
		}
		for (j = 0; j < ilitek_data->y_ch * ilitek_data->x_ch; j++) {
			seq_printf(m, "%d,", allnode_data[j]);
			if((j + 1) % ilitek_data->x_ch == 0) {
				seq_printf(m, "%s", "\n");
			}
		}
		seq_printf(m, "%s", "\n");
		tp_log_info("m->size = %d  m->count = %d\n", (int)m->size, (int)m->count);
	}
	return;
}

static int ilitek_check_busy(int delay)
{
	int i;
	unsigned char buf[2];
	for(i = 0; i < 1000; i ++){
		buf[0] = 0x80;
		if(ilitek_i2c_write_and_read(buf, 1, delay, buf, 1) < 0) {
			return ILITEK_I2C_TRANSFER_ERR;
		}
		if(buf[0] == 0x50) {
			tp_log_info("check_busy i = %d\n", i);
			return 0;
		}
	}
	tp_log_info("check_busy error\n");
	return -1;
}

static int ilitek_into_testmode(bool testmode) {
	int ret = 0;
	uint8_t cmd[2] = {0};
	cmd[0] = 0xF2;
	if (testmode) {
		cmd[1] = 0x01;
	}
	else {
		cmd[1] = 0x00;
	}
	ret = ilitek_i2c_write(cmd, 2);
	if(ret < 0){
		tp_log_err("ilitek_i2c_write err,ilitek_into_testmode_2120 err ret %d\n", ret);
		return ret;
	}
	mdelay(10);
	return 0;
}

static int ilitek_allnode_test(int *allnode_data) {
	int ret = 0, newMaxSize = 32, i = 0, j = 0, k = 0, read_len = 0, index = 0, diff = 0;
	int maxneighborDiff = 0,maxintercrossdiff = 0, txerror_count = 0, moduleerror_count = 0;
	bool txerror_result = false; 	  
	uint8_t cmd[4] = {0};
	uint8_t * buf_recv = NULL;
	int test_32 = 0;
	allnode_test_result = 0;
	test_32 = (ilitek_data->y_ch * ilitek_data->x_ch * 2) / (newMaxSize - 2);
	if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("kzalloc  test_32 = %d\n", test_32);
	buf_recv = (uint8_t *)vmalloc(sizeof(uint8_t) * (ilitek_data->y_ch * ilitek_data->x_ch * 2 + test_32 * 2 + 32));
	if(NULL == buf_recv) {
		tp_log_err("buf_recv NULL\n");
		return -ENOMEM;
	}
	mdelay(10);
	//initial
	cmd[0] = 0xF3;
	cmd[1] = 0x0B;
	cmd[2] = 0x00;
	if (ilitek_data->mcu_ver[1] == 0x23 || ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		cmd[3] = 0xE6;
	}
	else {
		cmd[3] = 0xE2;
	}
	ret = ilitek_i2c_write(cmd, 4);
	if (ret < 0) {
		tp_log_err("allnode test  initial set err ret = %d\n", ret);
	}
	mdelay(1000);
	ret = ilitek_check_busy(5);
	if (ret < 0) {
		tp_log_err("allnode test  check busy err ret = %d\n", ret);
	}
	mdelay(100);
	if (ilitek_data->mcu_ver[1] == 0x23 || 
		((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		test_32 = (ilitek_data->y_ch * ilitek_data->x_ch * 2) / (newMaxSize - 2);
		if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0) {
			test_32 += 1;
		}
		cmd[0] = 0xE6;
		ret = ilitek_i2c_write(cmd, 1);
		mdelay(10);
		tp_log_info("ilitek_allnode_test test_32 = %d\n", test_32);
		for(i = 0; i < test_32; i++) {
			if ((ilitek_data->y_ch * ilitek_data->x_ch * 2)%(newMaxSize - 2) != 0 && i == test_32 - 1) {
				ret = ilitek_i2c_read(buf_recv + newMaxSize*i, (ilitek_data->y_ch * ilitek_data->x_ch * 2)%(newMaxSize - 2) + 2);
			}
			else {
				ret = ilitek_i2c_read(buf_recv + newMaxSize*i, newMaxSize);
			}
			if(ret < 0){
				tp_log_err("err,i2c read error ret %d\n", ret);
			}
		}
		index = 0;
		for (i = 0; i < test_32; i++) {
			if (index == (ilitek_data->y_ch * ilitek_data->x_ch)) {
				break;
			}
			for (j = 2; j < newMaxSize;) {
				allnode_data[index++] = (buf_recv[newMaxSize * i + j]) + (buf_recv[newMaxSize * i + j + 1] << 8);
				j += 2;
				if (index == (ilitek_data->y_ch * ilitek_data->x_ch)) {
					break;
				}
			}
		}
	}
	else {
		test_32 = (ilitek_data->x_ch) / (newMaxSize - 2);
		if ((ilitek_data->x_ch) % (newMaxSize - 2) != 0) {
			test_32 += 1;
		}
		tp_log_info("ilitek_allnode_test test_32 = %d\n", test_32);
		cmd[0] = 0xE2;
		index = 0;
		for (j = 0; j < ilitek_data->y_ch; j++) {
			for(i = 0; i < test_32; i++) {
				if ((ilitek_data->x_ch)%(newMaxSize - 2) != 0 && i == test_32 - 1) {
					read_len = (ilitek_data->x_ch) % (newMaxSize - 2) + 2;
				}
				else {
					read_len = newMaxSize;
				}
				ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv, read_len);
				if(ret < 0){
					tp_log_err("i2c read err ret %d\n",ret);
				}
				for (k = 2; k < read_len; k++) {
					allnode_data[index++] = (buf_recv[k]);
					//tp_log_info("buf_recv[newMaxSize * i + j] = %d\n", buf_recv[k]);
				}
				msleep(10);
			}
		}
	}
	
	#if 1
	//handle
	for(i = 0; i < ilitek_data->y_ch; i++) {
		txerror_count = 0;
		for(j = 0; j < ilitek_data->x_ch; j++) {
			
			if(allnode_data[i * ilitek_data->x_ch + j] > ilitek_allnode_max_threshold ||
				allnode_data[i * ilitek_data->x_ch + j] < ilitek_allnode_min_threshold) {
				allnode_test_result = -1;
				tp_log_err(" err,allnode_test_result error allnode_data = %d, ilitek_allnode_max_threshold = %d,\
					ilitek_allnode_min_threshold = %d i = %d, j = %d\n",
					allnode_data[i * ilitek_data->x_ch + j], ilitek_allnode_max_threshold, ilitek_allnode_min_threshold, i, j);
				break;
			}
			
			if(i > 0) {
				diff = abs(allnode_data[i * ilitek_data->x_ch + j] - allnode_data[(i - 1) * ilitek_data->x_ch + j]);
				if(diff > maxneighborDiff) {
					maxneighborDiff = diff;
				}
				if(j > 0) {
					diff = abs((allnode_data[(i - 1) * ilitek_data->x_ch + j - 1] - allnode_data[i * ilitek_data->x_ch + j - 1]) - 
						(allnode_data[(i - 1) * ilitek_data->x_ch + j] - allnode_data[i * ilitek_data->x_ch + j]));
					if(diff > maxintercrossdiff) {
						maxintercrossdiff = diff;
					}
					if (diff > ilitek_allnodetestw2) {
						moduleerror_count++;
						txerror_count++;
						tp_log_err("allnodetestw2 err i = %d j = %d txerror_count = %d moduleerror_count = %d\n",
							i, j, txerror_count, moduleerror_count);
					}
				}
			}
		}
		if (txerror_count > ilitek_allnodetx) {
			txerror_result = true;
		}
	}
	
	
	if (ilitek_data->mcu_ver[1] == 0x23 || 
		((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		if (txerror_result && (moduleerror_count > ilitek_allnodemodule)) {
			allnode_test_result = -1;
		}
	}
	else {
		if(maxneighborDiff > ilitek_allnodetestw1) {
			allnode_test_result = -1;
			tp_log_err("maxneighborDiff = %d, ilitek_allnodetestw1 = %d\n", maxneighborDiff, ilitek_allnodetestw1);
		}
		if(maxintercrossdiff > ilitek_allnodetestw2) {
			allnode_test_result = -1;
			tp_log_err("maxintercrossdiff = %d, ilitek_allnodetestw2 = %d\n", maxintercrossdiff, ilitek_allnodetestw2);
		}
	}
	#endif
	if (buf_recv) {
		vfree(buf_recv);
		buf_recv = NULL;
	}
	return allnode_test_result;
}

static int ilitek_open_test(int *open_data) {
	int ret = 0, newMaxSize = 32, i = 0, j = 0, k = 0, read_len = 0, index = 0, value = 0;
	int rxfailindex = 0, rxfail_count = 0;
	int txfailindex = 0, txfail_count = 0;
	uint8_t cmd[4] = {0};
	uint8_t * buf_recv = NULL;
	int test_32 = 0;
	open_test_result = 0;
	test_32 = (ilitek_data->y_ch * ilitek_data->x_ch * 2) / (newMaxSize - 2);
	if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("kzalloc  test_32 = %d\n", test_32);
	buf_recv = (uint8_t *)vmalloc(sizeof(uint8_t) * (ilitek_data->y_ch * ilitek_data->x_ch * 2 + test_32 * 2 + 32));
	if(NULL == buf_recv) {
		tp_log_err("buf_recv NULL\n");
		return -ENOMEM;
	}
	mdelay(10);
	//initial
	cmd[0] = 0xF3;
	cmd[1] = 0x0C;
	cmd[2] = 0x00;
	if (ilitek_data->mcu_ver[1] == 0x23 || ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		cmd[3] = 0xE6;
	}
	else {
		cmd[3] = 0xE2;
	}
	ret = ilitek_i2c_write(cmd, 4);
	if (ret < 0) {
		tp_log_err("open test  initial set err ret = %d\n", ret);
	}
	mdelay(1000);
	ret = ilitek_check_busy(5);
	if (ret < 0) {
		tp_log_err("open test  check busy err ret = %d\n", ret);
	}
	mdelay(100);
	if (ilitek_data->mcu_ver[1] == 0x23 || 
		((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		test_32 = (ilitek_data->y_ch * ilitek_data->x_ch * 2) / (newMaxSize - 2);
		if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0) {
			test_32 += 1;
		}
		cmd[0] = 0xE6;
		ret = ilitek_i2c_write(cmd, 1);
		mdelay(10);
		tp_log_info("ilitek_open_test test_32 = %d\n", test_32);
		for(i = 0; i < test_32; i++){
			if ((ilitek_data->y_ch * ilitek_data->x_ch * 2)%(newMaxSize - 2) != 0 && i == test_32 - 1) {
				ret = ilitek_i2c_read(buf_recv + newMaxSize*i, (ilitek_data->y_ch * ilitek_data->x_ch * 2)%(newMaxSize - 2) + 2);
			}
			else {
				ret = ilitek_i2c_read(buf_recv + newMaxSize*i, newMaxSize);
			}
			if(ret < 0){
				tp_log_err("err,i2c read error ret %d\n", ret);
			}
		}
		index = 0;
		for (i = 0; i < test_32; i++) {
			if (index == (ilitek_data->y_ch * ilitek_data->x_ch)) {
				break;
			}
			for (j = 2; j < newMaxSize;) {
				open_data[index++] = (buf_recv[newMaxSize * i + j]) + (buf_recv[newMaxSize * i + j + 1] << 8);
				j += 2;
				if (index == (ilitek_data->y_ch * ilitek_data->x_ch)) {
					break;
				}
			}
		}
	}
	else {
		test_32 = (ilitek_data->x_ch) / (newMaxSize - 2);
		if ((ilitek_data->x_ch) % (newMaxSize - 2) != 0) {
			test_32 += 1;
		}
		tp_log_info("ilitek_open_test test_32 = %d\n", test_32);
		cmd[0] = 0xE2;
		index = 0;
		for (j = 0; j < ilitek_data->y_ch; j++) {
			for(i = 0; i < test_32; i++) {
				if ((ilitek_data->x_ch)%(newMaxSize - 2) != 0 && i == test_32 - 1) {
					read_len = (ilitek_data->x_ch) % (newMaxSize - 2) + 2;
				}
				else {
					read_len = newMaxSize;
				}
				ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv, read_len);
				if(ret < 0){
					tp_log_err("i2c read err ret %d\n",ret);
				}
				for (k = 2; k < read_len; k++) {
					open_data[index++] = (buf_recv[k]);
				}
				msleep(10);
			}
		}
	}
	//check
	#if 1
	index = 0;
	for(i = 0; i < ilitek_data->y_ch; i++) {
		for(j = 0; j < ilitek_data->x_ch; j++) {
			if(open_data[index++] < ilitek_open_threshold) {
				open_test_result = -1;
				tp_log_err(" err,open_test_result error open_data[%d] = %d, ilitek_open_threshold = %d\n",
					(index - 1) , open_data[index - 1], ilitek_open_threshold);
				break;
			}
		}
	}
	if (!open_test_result) {
		if (ilitek_data->mcu_ver[1] == 0x23 || 
			((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
			for (i = 0; i < ilitek_data->y_ch; i++) {
				rxfailindex = 0;
				rxfail_count = 0;
				for (j = 0; j < ilitek_data->x_ch - 1; j++) {
					value = abs(open_data[i * ilitek_data->x_ch + j + 1] - open_data[i * ilitek_data->x_ch + j]);
					if (value > ilitek_open_rxdeltathrehold) {
						tp_log_err(" open_test rxfail_count = %d i = %d j = %d\n", rxfail_count, i, j);
						if (rxfail_count == 0) {
							rxfailindex = j;
							rxfail_count++;
						}
						else {
							if ((j - rxfailindex) == 1) {
								rxfailindex = j;
								rxfail_count++;
								if (rxfail_count >= 3) {
									open_test_result = -1;
									tp_log_err(" err,open_test_result error rxfail_count = %d\n", rxfail_count);
									break;
								}
							}
							else {
								rxfailindex = j;
								rxfail_count = 1;
							}
						}
					}
				}
			}
			if (!open_test_result) {
				for (i = 0; i < ilitek_data->y_ch - 1; i++) {
					txfailindex = 0;
					txfail_count = 0;
					for (j = 0; j < ilitek_data->x_ch; j++) {
						value = abs(open_data[(i + 1) * ilitek_data->x_ch + j] - open_data[i * ilitek_data->x_ch + j]);
						if (value > ilitek_open_txdeltathrehold) {
							tp_log_err(" open_test txfail_count = %d i = %d j = %d\n", txfail_count, i, j);
							if (txfail_count == 0) {
								txfailindex = j;
								txfail_count++;
							}
							else {
								if ((j - txfailindex) == 1) {
									txfailindex = j;
									txfail_count++;
									if (txfail_count >= 3) {
										open_test_result = -1;
										tp_log_err(" err,open_test_result error txfail_count = %d\n", txfail_count);
										break;
									}
								}
								else {
									txfailindex = j;
									txfail_count = 1;
								}
							}
						}
					}
				}
			}
		}
	}
	#endif
	if (buf_recv) {
		vfree(buf_recv);
		buf_recv = NULL;
	}
	return open_test_result;
}

static int ilitek_short_test(int *short_xdata1, int *short_xdata2, int *short_ydata1, int *short_ydata2) {
	int ret = 0, newMaxSize = 32, i = 0, j = 0, index = 0;
	uint8_t cmd[4] = {0};
	uint8_t * buf_recv = NULL;
	int test_32 = 0;
	short_test_result = 0;
	index = ilitek_data->x_ch;
	if (ilitek_data->x_ch < ilitek_data->x_ch) {
		index = ilitek_data->y_ch;
	}
	test_32 = index / (newMaxSize - 2);
	if (index % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("kzalloc  test_32 = %d\n", test_32);
	buf_recv = (uint8_t *)vmalloc(sizeof(uint8_t) * (index + test_32 * 2 + 32));
	if(NULL == buf_recv) {
		tp_log_err("buf_recv NULL\n");
		return -ENOMEM;
	}
	//initial
	cmd[0] = 0xF3;
	cmd[1] = 0x09;
	cmd[2] = 0x00;
	cmd[3] = 0xE0;
	ret = ilitek_i2c_write(cmd, 4);
	if (ret < 0) {
		tp_log_err("short test  initial set err ret = %d\n", ret);
	}	
	ret = ilitek_check_busy(5);
	if (ret < 0) {
		tp_log_err("short test  check busy err ret = %d\n", ret);
	}
	mdelay(100);
	test_32 = ilitek_data->x_ch / (newMaxSize - 2);
	if (ilitek_data->x_ch % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("short xch  test_32 = %d\n", test_32);
	cmd[0] = 0xE0;
	for(i = 0; i < test_32; i++){
		if ((ilitek_data->x_ch)%(newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize*i, (ilitek_data->x_ch) % (newMaxSize - 2) + 2);
		}
		else {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize*i, newMaxSize);
		}
		if(ret < 0){
			tp_log_err("i2c read err ret %d\n",ret);
		}
	}
	
	j = 0;
	for(i = 0; i < test_32; i++) {
		if (j == ilitek_data->x_ch) {
			break;
		}
		for(index = 2; index < newMaxSize; index++) {
			short_xdata1[j] = buf_recv[i * newMaxSize + index];
			j++;
			if (j == ilitek_data->x_ch) {
				break;
			}
		}
	}
	if (ilitek_data->mcu_ver[1] == 0x23 || ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		cmd[0] = 0xF3;
		cmd[1] = 0x09;
		cmd[2] = 0x00;
		cmd[3] = 0xE1;
		ret = ilitek_i2c_write(cmd, 4);
		if (ret < 0) {
			tp_log_err("short test	initial set err ret = %d\n", ret);
		}	
		
		//check busy
		ret = ilitek_check_busy(5);
		if (ret < 0) {
			tp_log_err("short test	check busy err ret = %d\n", ret);
		}
	}

	test_32 = ilitek_data->y_ch / (newMaxSize - 2);
	if (ilitek_data->y_ch % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("short ych  test_32 = %d\n", test_32);
	msleep(100);
	cmd[0] = 0xE1;
	for(i = 0; i < test_32; i++){
		if ((ilitek_data->y_ch)%(newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize*i, (ilitek_data->y_ch) % (newMaxSize - 2) + 2);
		}
		else {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize*i, newMaxSize);
		}
		if(ret < 0){
			tp_log_err("i2c read err ret %d\n",ret);
		}
	}
	
	j = 0;
	for(i = 0; i < test_32; i++) {
		if (j == ilitek_data->y_ch) {
			break;
		}
		for(index = 2; index < newMaxSize; index++) {
			short_ydata1[j] = buf_recv[i * newMaxSize + index];
			j++;
			if (j == ilitek_data->y_ch) {
				break;
			}
		}
	}
	msleep(100);
	//initial
	cmd[0] = 0xF3;
	cmd[1] = 0x0A;
	cmd[2] = 0x00;
	cmd[3] = 0xE0;
	ret = ilitek_i2c_write(cmd, 4);
	if (ret < 0) {
		tp_log_err("short test	initial set err ret = %d\n", ret);
	}	
	
	//check busy
	ret = ilitek_check_busy(5);
	if (ret < 0) {
		tp_log_err("short test	check busy err ret = %d\n", ret);
	}
	mdelay(100);
	test_32 = ilitek_data->x_ch / (newMaxSize - 2);
	if (ilitek_data->x_ch % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("short xch  test_32 = %d\n", test_32);
	cmd[0] = 0xE0;
	for(i = 0; i < test_32; i++){
		if ((ilitek_data->x_ch)%(newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize*i, (ilitek_data->x_ch) % (newMaxSize - 2) + 2);
		}
		else {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize*i, newMaxSize);
		}
		if(ret < 0){
			tp_log_err("i2c read err ret %d\n",ret);
		}
	}
	
	j = 0;
	for(i = 0; i < test_32; i++) {
		if (j == ilitek_data->x_ch) {
			break;
		}
		for(index = 2; index < newMaxSize; index++) {
			short_xdata2[j] = buf_recv[i * newMaxSize + index];
			j++;
			if (j == ilitek_data->x_ch) {
				break;
			}
		}
	}

	if (ilitek_data->mcu_ver[1] == 0x23 || ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25)) {
		cmd[0] = 0xF3;
		cmd[1] = 0x0A;
		cmd[2] = 0x00;
		cmd[3] = 0xE1;
		ret = ilitek_i2c_write(cmd, 4);
		if (ret < 0) {
			tp_log_err("short test	initial set err ret = %d\n", ret);
		}	
		
		//check busy
		ret = ilitek_check_busy(5);
		if (ret < 0) {
			tp_log_err("short test	check busy err ret = %d\n", ret);
		}
	}
	test_32 = ilitek_data->y_ch / (newMaxSize - 2);
	if (ilitek_data->y_ch % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("short ych  test_32 = %d\n", test_32);
	msleep(100);
	cmd[0] = 0xE1;
	for(i = 0; i < test_32; i++){
		if ((ilitek_data->y_ch)%(newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize*i, (ilitek_data->y_ch) % (newMaxSize - 2) + 2);
		}
		else {
			ret = ilitek_i2c_write_and_read(cmd, 1, 2, buf_recv + newMaxSize*i, newMaxSize);
		}
		if(ret < 0){
			tp_log_err("i2c read err ret %d\n",ret);
		}
	}
	
	j = 0;
	for(i = 0; i < test_32; i++) {
		if (j == ilitek_data->y_ch) {
			break;
		}
		for(index = 2; index < newMaxSize; index++) {
			short_ydata2[j] = buf_recv[i * newMaxSize + index];
			j++;
			if (j == ilitek_data->y_ch) {
				break;
			}
		}
	}
	//check pass or fail
	for(i = 0; i < ilitek_data->x_ch; i++) {
		if(abs(short_xdata1[i] - short_xdata2[i]) > ilitek_short_threshold) {
			short_test_result = -1;
			tp_log_err("[TP_selftest] err,short_test_result error short_xdata1[%d] = %d, short_xdata2[%d] = %d, ilitek_short_threshold = %d\n",
				i , short_xdata1[i], i , short_xdata2[i], ilitek_short_threshold);
			break;
		} 
	}
	if(short_test_result == 0) {
		for(i = 0; i < ilitek_data->y_ch; i++) {
			if(abs(short_ydata1[i] - short_ydata2[i]) > ilitek_short_threshold) {
				short_test_result = -1;
				tp_log_err("[TP_selftest] err,short_test_result error short_ydata1[%d] = %d, short_ydata2[%d] = %d, ilitek_short_threshold = %d\n",
					i , short_ydata1[i], i , short_ydata2[i], ilitek_short_threshold);
				break;
			} 
		}
	}
	if (buf_recv) {
		vfree(buf_recv);
		buf_recv = NULL;
	}
	return short_test_result;
}

static int ilitek_sensortest_bigger_size_ic(int * short_xdata1, int * short_xdata2, int * short_ydata1,
	int * short_ydata2, int * open_data, int * allnode_data) {
	int ret = 0;
	uint8_t cmd[2] = {0};
	tp_log_info("\n");
	if(NULL == short_xdata1 || NULL == short_xdata2 || NULL == short_ydata1
		|| NULL == short_ydata2 || NULL == open_data || NULL == allnode_data){
		tp_log_err("save data buf is NULL\n");
		return -ENOMEM;
	}
	cmd[0] = 0xF4;
	cmd[1] = 0x51;
	ret = ilitek_i2c_write(cmd, 2);
	if(ret < 0){
		tp_log_err("ilitek_i2c_write err,f4 51 err ret %d\n", ret);
		//return ret;
	}
	mdelay(10);
	ret = ilitek_into_testmode(true);
	if (ret < 0) {
		tp_log_err("into test mode err ret = %d\n", ret);
		return ret;
	}
	ret = ilitek_short_test(short_xdata1, short_xdata2, short_ydata1, short_ydata2);
	if (ret < 0) {
		tp_log_err("short test fail ret = %d\n", ret);
	}
	ret = ilitek_into_testmode(true);
	if (ret < 0) {
		tp_log_err("into test mode err ret = %d\n", ret);
		//return ret;
	}
	ret = ilitek_open_test(open_data);
	if (ret < 0) {
		tp_log_err("open test fail ret = %d\n", ret);
	}
	ret = ilitek_into_testmode(true);
	if (ret < 0) {
		tp_log_err("into test mode err ret = %d\n", ret);
		//return ret;
	}
	ret = ilitek_allnode_test(allnode_data);
	if (ret < 0) {
		tp_log_err("allnode test fail ret = %d\n", ret);
	}
	ret = ilitek_into_testmode(false);
	if (ret < 0) {
		tp_log_err("into test mode err ret = %d\n", ret);
		//return ret;
	}
	cmd[0] = 0xF4;
	cmd[1] = 0x50;
	ret = ilitek_i2c_write(cmd, 2);
	if(ret < 0){
		tp_log_err("ilitek_i2c_write err,f4 50 err ret %d\n", ret);
		//return ret;
	}
	return 0;
}

static int ilitek_allnode_test_2120(int *allnode_data) {
	int ret = 0, newMaxSize = 32, i = 0, j = 0, index = 0;
	uint8_t cmd[4] = {0};
	uint8_t * buf_recv = NULL;

	int test_32 = (ilitek_data->y_ch * ilitek_data->x_ch * 2) / (newMaxSize - 2);
	if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	buf_recv = (uint8_t *)vmalloc(sizeof(uint8_t) * (ilitek_data->y_ch * ilitek_data->x_ch * 2 + test_32 * 2 + 32));
	if(NULL == buf_recv) {
		tp_log_err("buf_recv NULL\n");
		return -ENOMEM;
	}
	tp_log_info("ilitek_allnode_test test_32 = %d\n", test_32);
	allnode_test_result = 0;
	cmd[0] = 0xF1;
	cmd[1] = 0x08;//0x05;
	ret = ilitek_i2c_write(cmd, 2);
	if(ret < 0){
		tp_log_err("i2c err, ret %d\n",ret);
	}
	msleep(10);

	for (i =0; i < 300; i++ ) {
		ret = ilitek_poll_int();
		tp_log_info("%s:ilitek interrupt status = %d\n", __func__,ret);
		if (ret == 1) {
			break;
		}
		else {
			msleep(5);
		}
	}

	cmd[0] = 0xF6;
	cmd[1] = 0xF2;
	ret = ilitek_i2c_write(cmd, 2);
	if(ret < 0){
		tp_log_err("i2c err, ret %d\n",ret);
	}
	msleep(10);
	cmd[0] = 0xF2;
	ret = ilitek_i2c_write(cmd, 1);
	if(ret < 0){
		tp_log_err("i2c err, ret %d\n",ret);
	}
	msleep(10);
	for(i = 0; i < test_32; i++){
		if ((ilitek_data->y_ch * ilitek_data->x_ch * 2)%(newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_read(buf_recv + newMaxSize*i, (ilitek_data->y_ch * ilitek_data->x_ch * 2)%(newMaxSize - 2) + 2);
		}
		else {
			ret = ilitek_i2c_read(buf_recv + newMaxSize*i, newMaxSize);
		}
		if(ret < 0){
			tp_log_err("err,i2c read error ret %d\n", ret);
		}
	}
	j = 0;
	for(i = 0; i < test_32; i++) {
		if (j == ilitek_data->y_ch * ilitek_data->x_ch) {
			break;
		}
		for(index = 2; index < newMaxSize; index += 2) {
			allnode_data[j] = ((buf_recv[i * newMaxSize + index + 1] << 8) + buf_recv[i * newMaxSize + index]);
			if ( (allnode_data[j] < ilitek_allnode_min_threshold) ||(allnode_data[j] > ilitek_allnode_max_threshold) ) {
				tp_log_err("err,allnode_test_result error allnode_data[%d] = %d, ilitek_allnode_min_threshold = %d ilitek_allnode_max_threshold = %d\n",
					j , allnode_data[j], ilitek_allnode_min_threshold, ilitek_allnode_max_threshold);
				allnode_test_result = -1;
				break;
			}
			j++;

			if (j == ilitek_data->y_ch * ilitek_data->x_ch) {
				break;
			}
		}
	}

	if (buf_recv) {
		vfree(buf_recv);
		buf_recv = NULL;
	}

	return allnode_test_result;
}

static int ilitek_open_test_2120(int *open_data) {
	int ret = 0, newMaxSize = 32, i = 0, j = 0, index = 0;
	uint8_t cmd[4] = {0};
	uint8_t * buf_recv = NULL;

	int test_32 = (ilitek_data->y_ch * ilitek_data->x_ch * 2) / (newMaxSize - 2);
	if ((ilitek_data->y_ch * ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	buf_recv = (uint8_t *)vmalloc(sizeof(uint8_t) * (ilitek_data->y_ch * ilitek_data->x_ch * 2 + test_32 * 2 + 32));
	if(NULL == buf_recv) {
		tp_log_err("buf_recv NULL\n");
		return -ENOMEM;
	}
	open_test_result = 0;
	cmd[0] = 0xF1;
	cmd[1] = 0x06;
	cmd[2] = 0x00;
	ret = ilitek_i2c_write(cmd, 3);
	if(ret < 0){
		tp_log_err("i2c err, ret %d\n",ret);
	}

	msleep(1);
	for (i =0; i < 300; i++ ) {
		ret = ilitek_poll_int();
		tp_log_info("ilitek interrupt status = %d\n",ret);
		if (ret == 1) {
			break;
		}
		else {
			msleep(5);
		}
	}

	cmd[0] = 0xF6;
	cmd[1] = 0xF2;
	ret = ilitek_i2c_write(cmd, 2);
	if(ret < 0){
		tp_log_err("i2c err, ret %d\n",ret);
	}

	cmd[0] = 0xF2;
	ret = ilitek_i2c_write(cmd, 1);
	if(ret < 0){
		tp_log_err("i2c err, ret %d\n",ret);
	}

	tp_log_info("ilitek_open_test test_32 = %d\n", test_32);
	for(i = 0; i < test_32; i++){
		if ((ilitek_data->y_ch * ilitek_data->x_ch * 2)%(newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_read(buf_recv + newMaxSize*i, (ilitek_data->y_ch * ilitek_data->x_ch * 2)%(newMaxSize - 2) + 2);
		}
		else {
			ret = ilitek_i2c_read(buf_recv + newMaxSize*i, newMaxSize);
		}
		if(ret < 0){
			tp_log_err("err,i2c read error ret %d\n", ret);
		}
	}
	j = 0;
	for(i = 0; i < test_32; i++) {
		if (j == ilitek_data->y_ch * ilitek_data->x_ch) {
			break;
		}
		for(index = 2; index < newMaxSize; index += 2) {
			open_data[j] = ((buf_recv[i * newMaxSize + index + 1] << 8) + buf_recv[i * newMaxSize + index]);
			if (((buf_recv[i * newMaxSize + index + 1] << 8) + buf_recv[i * newMaxSize + index]) < ilitek_open_threshold) {
				tp_log_err("[TP_selftest] err,open_test_result error open_data[%d] = %d, ilitek_open_threshold = %d\n",
					j , open_data[j], ilitek_open_threshold);
				open_test_result = -1;
				break;
			}
			j++;
			if (j == ilitek_data->y_ch * ilitek_data->x_ch) {
				break;
			}
		}
	}

	if (buf_recv) {
		vfree(buf_recv);
		buf_recv = NULL;
	}
	return open_test_result;
}

static int ilitek_short_test_2120(int *short_data1, int *short_data2) {
	int ret = 0, newMaxSize = 32, i = 0, j = 0, index = 0;
	uint8_t cmd[4] = {0};
	uint8_t * buf_recv = NULL;

	int test_32 = (ilitek_data->x_ch * 2) / (newMaxSize - 2);
	if ((ilitek_data->x_ch * 2) % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("ilitek_short_test test_32 = %d\n", test_32);
	buf_recv = (uint8_t *)vmalloc(sizeof(uint8_t) * ((ilitek_data->x_ch * 2) + test_32 * 2 + 32));
	if(NULL == buf_recv) {
		tp_log_err("buf_recv NULL\n");
		return -ENOMEM;
	}
	short_test_result = 0;
	cmd[0] = 0xF1;
	cmd[1] = 0x04;
	cmd[2] = 0x00;
	ret = ilitek_i2c_write(cmd, 3);
	if(ret < 0){
		tp_log_err("i2c err, ret %d\n",ret);
	}

	msleep(1);
	for (i =0; i < 300; i++ ) {
		ret = ilitek_poll_int();
		tp_log_info("ilitek interrupt status = %d\n",ret);
		if (ret == 1) {
			break;
		}
		else {
			msleep(5);
		}
	}

	cmd[0] = 0xF6;
	cmd[1] = 0xF2;
	ret = ilitek_i2c_write(cmd, 2);
	if(ret < 0){
		tp_log_err("i2c err, ret %d\n",ret);
	}

	cmd[0] = 0xF2;
	ret = ilitek_i2c_write(cmd, 1);
	if(ret < 0){
		tp_log_err("i2c err, ret %d\n",ret);
	}

	for(i = 0; i < test_32; i++){
		if ((ilitek_data->x_ch * 2)%(newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_read(buf_recv + newMaxSize*i, (ilitek_data->x_ch * 2) % (newMaxSize - 2) + 2);
		}
		else {
			ret = ilitek_i2c_read(buf_recv + newMaxSize*i, newMaxSize);
		}
		if(ret < 0){
			tp_log_err("i2c read err ret %d\n",ret);
		}
	}
	j = 0;
	for(i = 0; i < test_32; i++) {
		if (j == ilitek_data->x_ch * 2) {
			break;
		}
		for(index = 2; index < newMaxSize; index++) {
			if (j < ilitek_data->x_ch) {
				short_data1[j] = buf_recv[i * newMaxSize + index];
			}
			else {
				short_data2[j - ilitek_data->x_ch] = buf_recv[i * newMaxSize + index];
			}
			j++;
			if (j == ilitek_data->x_ch * 2) {
				break;
			}
		}
	}

	for (i = 0; i < ilitek_data->x_ch; i++) {
		if (abs(short_data1[i] - short_data2[i]) > ilitek_short_threshold) {
			tp_log_err("[TP_selftest] err,short_test_result error short_data1[%d] = %d, short_data2[%d] = %d, ilitek_short_threshold = %d\n",
				i , short_data1[i], i , short_data2[i], ilitek_short_threshold);
			short_test_result = -1;
			break;
		}
	}
	if (buf_recv) {
		vfree(buf_recv);
		buf_recv = NULL;
	}
	return short_test_result;
}

static int ilitek_gesture_disable_sense_start_2120(void) {
	int ret = 0;
	uint8_t cmd[2] = {0};

	cmd[0] = 0x0A;
	cmd[1] = 0x00;
	ret = ilitek_i2c_write(cmd, 2);
	mdelay(10);
	cmd[0] = 0x01;
	cmd[1] = 0x01;
	ret = ilitek_i2c_write(cmd, 2);
	mdelay(10);
	return 0;
}

static int ilitek_into_testmode_2120(bool testmode) {
	int ret = 0;
	uint8_t cmd[2] = {0};
	cmd[0] = 0xF0;
	if (testmode) {
		cmd[1] = 0x01;
	}
	else {
		cmd[1] = 0x00;
	}
	ret = ilitek_i2c_write(cmd, 2);
	if(ret < 0){
		tp_log_err("ilitek_i2c_write err,ilitek_into_testmode_2120 err ret %d\n", ret);
		return ret;
	}
	mdelay(10);
	return 0;
}

static int ilitek_sensortest_2120(int * short_xdata1, int * short_xdata2, int * open_data, int * allnode_data) {
	int ret = 0;
	unsigned char buf[64]={0};
	tp_log_info("\n");
	if(NULL == short_xdata1 || NULL == short_xdata2 || NULL == open_data || NULL == allnode_data){
		tp_log_err("save data buf is NULL\n");
		return -ENOMEM;
	}
	buf[0] = ILITEK_TP_CMD_GET_TOUCH_INFORMATION;
	ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 3);
	tp_log_info("write 0x10 read 0x%d, 0x%d, 0x%d\n", buf[0], buf[1], buf[2]);
	buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL_2120;
	buf[1] = 0x13;
	ret = ilitek_i2c_write_and_read(buf, 2, 10, buf, 0);
	buf[0] = 0x13;
	ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 2);
	tp_log_info("write 0x13 read 0x%d, 0x%d\n", buf[0], buf[1]);
	ret = ilitek_into_testmode_2120(true);
	if (ret < 0) {
		tp_log_err("into test mode err ret = %d\n", ret);
		return ret;
	}
	ret = ilitek_gesture_disable_sense_start_2120();
	if (ret < 0) {
		tp_log_err("gesture_disable_sense_start err ret = %d\n", ret);
		return ret;
	}
	ret = ilitek_short_test_2120(short_xdata1, short_xdata2);
	if (ret < 0) {
		tp_log_err("short test fail ret = %d\n", ret);
	}
	ret = ilitek_open_test_2120(open_data);
	if (ret < 0) {
		tp_log_err("open test fail ret = %d\n", ret);
	}
	ret = ilitek_allnode_test_2120(allnode_data);
	if (ret < 0) {
		tp_log_err("allnode test fail ret = %d\n", ret);
	}
	ret = ilitek_into_testmode_2120(false);
	if (ret < 0) {
		tp_log_err("into normal mode err ret = %d\n", ret);
	}
	return ret;
}
static int ilitek_sensortest_proc_show(struct seq_file *m, void *v) {
	int ret = 0;
	int * short_xdata1 = NULL;
	int * short_xdata2 = NULL;
	int * short_ydata1 = NULL;
	int * short_ydata2 = NULL;
	int * open_data = NULL;
	int * allnode_data = NULL;
	tp_log_info("m->size = %d  m->count = %d\n", (int)m->size, (int)m->count);
	if (m->size <= (4096 * 4)) {
		m->count = m->size;
		return 0;
	}
	short_xdata1 = (int *)vmalloc(sizeof(int) * (ilitek_data->x_ch));
	short_xdata2 = (int *)vmalloc(sizeof(int) * (ilitek_data->x_ch));
	short_ydata1 = (int *)vmalloc(sizeof(int) * (ilitek_data->y_ch));
	short_ydata2 = (int *)vmalloc(sizeof(int) * (ilitek_data->y_ch));
	open_data = (int *)vmalloc(sizeof(int) * (ilitek_data->y_ch * ilitek_data->x_ch));
	allnode_data = (int *)vmalloc(sizeof(int) * (ilitek_data->y_ch * ilitek_data->x_ch));
	if(NULL == short_xdata1 || NULL == short_xdata2 || NULL == short_ydata1
		|| NULL == short_ydata2 || NULL == open_data || NULL == allnode_data){
		tp_log_err("kzalloc ERR NULL\n");
		ret = -ENOMEM;
		goto out;
	}
	ilitek_irq_disable();
	ilitek_data->operation_protection = true;
	if (!(ilitek_data->ic_2120)) {
		ret = ilitek_sensortest_bigger_size_ic(short_xdata1, short_xdata2, short_ydata1, short_ydata2, open_data, allnode_data);
	}
	else {
		ret = ilitek_sensortest_2120(short_xdata1, short_xdata2, open_data, allnode_data);
	}
	ilitek_reset(300);
	ilitek_irq_enable();
	ilitek_data->operation_protection = false;
	ilitek_printf_sensortest_data(short_xdata1, short_xdata2, short_ydata1, short_ydata2, open_data, allnode_data, m);
out:
	if (short_xdata1) {
		vfree(short_xdata1);
		short_xdata1 = NULL;
	}
	if (short_xdata2) {
		vfree(short_xdata2);
		short_xdata2 = NULL;
	}
	if (short_ydata1) {
		vfree(short_ydata1);
		short_ydata1 = NULL;
	}
	if (short_ydata2) {
		vfree(short_ydata2);
		short_ydata2 = NULL;
	}
	if (open_data) {
		vfree(open_data);
		open_data = NULL;
	}
	if (allnode_data) {
		vfree(allnode_data);
		allnode_data = NULL;
	}
	return 0;
}

static int ilitek_proc_open_sensortest(struct inode *inode, struct file *file) {
	tp_log_info("\n");
	return single_open(file, ilitek_sensortest_proc_show, NULL);
}
static ssize_t ilitek_sensortest_write(struct file *pFile, const char __user *buf, size_t size, loff_t *pPos) {	
	int ret = 0;
	tp_log_info("\n");
	if (!(ilitek_data->ic_2120)) {
		ret = sscanf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s",&ilitek_short_threshold, &ilitek_open_threshold, &ilitek_open_txdeltathrehold,
			&ilitek_open_rxdeltathrehold, &ilitek_allnode_max_threshold, &ilitek_allnode_min_threshold,
			&ilitek_allnodetestw1, &ilitek_allnodetestw2, &ilitek_allnodemodule, &ilitek_allnodetx, &ilitek_printsensortestdata, sensor_test_data_path);
		if (ret != 12) {
			tp_log_err("sscanf get value fail\n");
		}
		tp_log_info("ilitek_short_threshold = %d\n", ilitek_short_threshold);
		tp_log_info("ilitek_open_threshold = %d\n", ilitek_open_threshold);
		tp_log_info("ilitek_open_txdeltathrehold = %d\n", ilitek_open_txdeltathrehold);
		tp_log_info("ilitek_open_rxdeltathrehold = %d\n", ilitek_open_rxdeltathrehold);
		tp_log_info("ilitek_allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
		tp_log_info("ilitek_allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
		tp_log_info("ilitek_allnodetestw1 = %d\n", ilitek_allnodetestw1);
		tp_log_info("ilitek_allnodetestw2 = %d\n", ilitek_allnodetestw2);
		tp_log_info("ilitek_allnodemodule = %d\n", ilitek_allnodemodule);
		tp_log_info("ilitek_allnodetx = %d\n", ilitek_allnodetx);
	}
	else {
		ret = sscanf(buf, "%d,%d,%d,%d,%d,%s",&ilitek_short_threshold, &ilitek_open_threshold,
			&ilitek_allnode_max_threshold, &ilitek_allnode_min_threshold, &ilitek_printsensortestdata, sensor_test_data_path);
		if (ret != 6) {
			tp_log_err("sscanf get value fail\n");
		}
		tp_log_info("ilitek_short_threshold = %d\n", ilitek_short_threshold);
		tp_log_info("ilitek_open_threshold = %d\n", ilitek_open_threshold);
		tp_log_info("ilitek_allnode_max_threshold = %d\n", ilitek_allnode_max_threshold);
		tp_log_info("ilitek_allnode_min_threshold = %d\n", ilitek_allnode_min_threshold);
	}
	tp_log_info("ilitek_printsensortestdata = %d\n", ilitek_printsensortestdata);
	tp_log_info("sensor_test_data_path = %s\n", sensor_test_data_path);
	return size;
}
const struct file_operations ilitek_proc_fops_sensortest = {
	.open = ilitek_proc_open_sensortest,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = ilitek_sensortest_write,
	.release = single_release,
};

static void ilitek_printf_noisefre_data(uint8_t * noisefre_data, struct seq_file *m) {
	int i = 0, j = 0, len = 0, loop_10 = 0;
	int read_noisefre_data_len = 0;
	struct file *filp;
	mm_segment_t fs;
	unsigned char buf[128];
    struct  timeval   time_now;
	struct rtc_time tm; 
    do_gettimeofday(&time_now);
	rtc_time_to_tm(time_now.tv_sec, &tm); 
	tp_log_info("%d_%d_%d_%d_%d_%d\n", (tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	len = sprintf(buf, "ilitek_noisefre_%d%02d%02d%02d%02d%02d_pass.csv", (tm.tm_year + 1900), tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	for(j = 0; j < 256; j++) {
		noisefre_data_path_tmp[j] = noisefre_data_path[j];
	}
	strcat(noisefre_data_path, buf);
	tp_log_info("noisefre_data_path = %s\n", noisefre_data_path);
	
	read_noisefre_data_len = ((noisefre_step != 0) ? 
		(((noisefre_end - noisefre_start) * 10) / noisefre_step + 1) : (((noisefre_end - noisefre_start) * 10) * 2));
	loop_10 = read_noisefre_data_len / 20;
	if (read_noisefre_data_len % 20) {
		loop_10 += 1;
	}
	for (i = 0; i < loop_10; i++) {
		if ((read_noisefre_data_len % 20) && (i == (loop_10 - 1))) {
			for (j = 0; j < (read_noisefre_data_len % 20); j++) {
				seq_printf(m, "%04d,", (noisefre_start * 10) + ((i * 20 + j) * noisefre_step));
			}
			seq_printf(m, "\n");
			for (j = 0; j < (read_noisefre_data_len % 20); j++) {
				seq_printf(m, "%04d,", noisefre_data[(i * 20 + j)]);
			}
			seq_printf(m, "\n");
		}
		else {
			for (j = 0; j < 20; j++) {
				seq_printf(m, "%04d,", (noisefre_start * 10) + ((i * 20 + j) * noisefre_step));
			}
			seq_printf(m, "\n");
			for (j = 0; j < 20; j++) {
				seq_printf(m, "%04d,", noisefre_data[(i * 20 + j)]);
			}
			seq_printf(m, "\n");
			seq_printf(m, "\n");
		}
	}
	tp_log_info("m->size = %d  m->count = %d\n", (int)m->size, (int)m->count);
	
	filp = filp_open(noisefre_data_path, O_CREAT | O_WRONLY, 0777);
	if(IS_ERR(filp)) {
		tp_log_err("save noisefre data  File Open Error path = %s\n", noisefre_data_path);
	}
	else {
		fs = get_fs();
		set_fs(KERNEL_DS);

		for (j = (noisefre_start * 10); j < (noisefre_end * 10); j += noisefre_step) {
			len = sprintf(buf, "%03d,", j);
			printk("%03d,", j);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
		}
		len = sprintf(buf, "\n");
		printk("\n");
		filp->f_op->write(filp, buf, len, &(filp->f_pos));
		for (j = 0; j < read_noisefre_data_len; j++) {
			len = sprintf(buf, "%03d,", noisefre_data[j]);
			printk("%03d,", noisefre_data[j]);
			filp->f_op->write(filp, buf, len, &(filp->f_pos));
		}
		printk("\n");
		set_fs(fs);
	}
	for(j = 0; j < 256; j++) {
		noisefre_data_path[j] = noisefre_data_path_tmp[j];
	}
	return;
}

static int ilitek_noisefre_proc_show(struct seq_file *m, void *v) {
	int ret = 0, newMaxSize = 32, i =0, j = 0, index = 0;
	uint8_t cmd[8] = {0};
	int read_noisefre_data_len = 0;
	int test_32 = 0;
	uint8_t * buf_recv = NULL;
	tp_log_info("m->size = %d  m->count = %d\n", (int)m->size, (int)m->count);
	if (m->size <= (4096)) {
		m->count = m->size;
		return 0;
	}
	read_noisefre_data_len = ((noisefre_step != 0) ? 
		(((noisefre_end - noisefre_start) * 10) / noisefre_step + 1) : (((noisefre_end - noisefre_start) * 10) * 2));
	test_32 = (read_noisefre_data_len) / (newMaxSize - 2);
	if (read_noisefre_data_len % (newMaxSize - 2) != 0) {
		test_32 += 1;
	}
	tp_log_info("kzalloc  test_32 = %d\n", test_32);
	buf_recv = (uint8_t *)vmalloc(sizeof(uint8_t) * (read_noisefre_data_len + test_32 * 2 + 32));
	if(NULL == buf_recv) {
		tp_log_err("buf_recv NULL\n");
		return -ENOMEM;
	}
	ilitek_irq_disable();
	ret = ilitek_into_testmode(true);
	if (ret < 0) {
		tp_log_err("into test mode err ret = %d\n", ret);
		return ret;
	}
	cmd[0] = 0xF3;
	cmd[1] = 0x0F;
	cmd[2] = noisefre_start;
	cmd[3] = noisefre_end;
	cmd[4] = noisefre_step;
	ret = ilitek_i2c_write(cmd, 5);
	if(ret < 0){
		tp_log_err("ilitek_i2c_write err,ilitek_into_testmode_2120 err ret %d\n", ret);
		return ret;
	}
	ret = ilitek_check_busy(10);
	if (ret < 0) {
		tp_log_err("allnode test  check busy err ret = %d\n", ret);
	}
	
	cmd[0] = 0xE4;
	ret = ilitek_i2c_write(cmd, 1);
	if(ret < 0){
		tp_log_err("ilitek_i2c_write err,ilitek_into_testmode_2120 err ret %d\n", ret);
		return ret;
	}
	for(i = 0; i < test_32; i++){
		if (read_noisefre_data_len % (newMaxSize - 2) != 0 && i == test_32 - 1) {
			ret = ilitek_i2c_read(buf_recv + newMaxSize*i, read_noisefre_data_len % (newMaxSize - 2) + 2);
		}
		else {
			ret = ilitek_i2c_read(buf_recv + newMaxSize*i, newMaxSize);
		}
		if(ret < 0){
			tp_log_err("err,i2c read error ret %d\n", ret);
		}
	}
	j = 0;
	for(i = 0; i < test_32; i++) {
		if (j == read_noisefre_data_len) {
			break;
		}
		for(index = 2; index < newMaxSize; index++) {
			buf_recv[j] = (buf_recv[i * newMaxSize + index]);
			j++;
			if (j == read_noisefre_data_len) {
				break;
			}
		}
	}
	ilitek_reset(300);
	ilitek_irq_enable();
	ilitek_printf_noisefre_data(buf_recv, m);
	if (buf_recv) {
		vfree(buf_recv);
		buf_recv = NULL;
	}
	return 0;
}

static int ilitek_proc_open_noisefre(struct inode *inode, struct file *file) {
	tp_log_info("\n");
	return single_open(file, ilitek_noisefre_proc_show, NULL);
}

static ssize_t ilitek_noisefre_write(struct file *pFile, const char __user *buf, size_t size, loff_t *pPos) {	
	int ret = 0;
	tp_log_info("\n");
	ret = sscanf(buf, "%d,%d,%d,%s",&noisefre_start, &noisefre_end, &noisefre_step, noisefre_data_path);
	if (ret != 4) {
		tp_log_err("sscanf get value fail\n");
	}
	tp_log_info("noisefre_start = %d\n", noisefre_start);
	tp_log_info("noisefre_end = %d\n", noisefre_end);
	tp_log_info("noisefre_step = %d\n", noisefre_step);
	tp_log_info("noisefre_data_path = %s\n", noisefre_data_path);
	return size;
}

const struct file_operations ilitek_proc_fops_noisefre = {
	.open = ilitek_proc_open_noisefre,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = ilitek_noisefre_write,
	.release = single_release,
};

static unsigned int ilitek_hex_2_dec(unsigned char *hex, int len) {
	unsigned int ret = 0, temp = 0;
	int i, shift = (len - 1) * 4;
	for(i = 0; i < len; shift -= 4, i++) {
		if((hex[i] >= '0') && (hex[i] <= '9')) {
			temp = hex[i] - '0';
		} else if((hex[i] >= 'a') && (hex[i] <= 'z')) {
			temp = (hex[i] - 'a') + 10;
		} else {
			temp = (hex[i] - 'A') + 10;
		}
		ret |= (temp << shift);
	}
	return ret;
}

static int ilitek_parse_hex_file(unsigned int * df_startaddr, unsigned int * df_endaddr, unsigned int * df_checksum,
	unsigned int * ap_startaddr, unsigned int * ap_endaddr, unsigned int * ap_checksum, int hex_len,
	unsigned char * CTPM_FW, unsigned char * save_hex_content) {
	int i = 0, j = 0, k = 0;
	unsigned int checksum = 0, check = 0;
	unsigned int len = 0, addr = 0, type = 0, exaddr = 0;
	int hex_end = 0;
	int offset;
	tp_log_info("\n");
	if (save_hex_content == NULL) {
		tp_log_err("save_hex_content is null\n");
		return -2;
	}
	if (CTPM_FW == NULL) {
		tp_log_err("CTPM_FW is null\n");
		return -2;
	}
	for(i = 0; i < hex_len; ) {
		len = ilitek_hex_2_dec(&save_hex_content[i + 1], 2);
		addr = ilitek_hex_2_dec(&save_hex_content[i + 3], 4);
		type = ilitek_hex_2_dec(&save_hex_content[i + 7], 2);
		if (type == 1) {
			hex_end = 1;
		}
		if(type == 0x04) {
			exaddr = ilitek_hex_2_dec(&save_hex_content[i + 9], 4);
			tp_log_info("exaddr = %x\n", (int)exaddr);
		}
		addr = addr + (exaddr << 16);
		//calculate checksum
		checksum = 0;
		for(j = 8; j < (2 + 4 + 2 + (len * 2)); j += 2) {
			if(type == 0x00) {
				check = check + ilitek_hex_2_dec(&save_hex_content[i + 1 + j], 2);
				if(addr + (j - 8) / 2 < *df_startaddr) {
					*ap_checksum = *ap_checksum + ilitek_hex_2_dec(&save_hex_content[i + 1 + j], 2);
				} else {
					*df_checksum = *df_checksum + ilitek_hex_2_dec(&save_hex_content[i + 1 + j], 2);
				}
			} else {
				checksum = 0;
			}
		}
		if(save_hex_content[i + 1 + j + 2] == 0x0D) {
			offset = 2;
		} else {
			offset = 1;
		}
		if(addr < *df_startaddr) {
			*ap_checksum = *ap_checksum + checksum;
		} else {
			*df_checksum = *df_checksum + checksum;
		}
		if(type == 0x00) {
			if(addr < *ap_startaddr) {
				*ap_startaddr = addr;
			}
			if((addr + len) > *ap_endaddr && (addr < *df_startaddr)) {
				*ap_endaddr = addr + len - 1;
				if(*ap_endaddr > *df_startaddr) {
					*ap_endaddr = *df_startaddr - 1;
				}
			}
			if((addr + len) > *df_endaddr && (addr >= *df_startaddr)) {
				*df_endaddr = addr + len;
			}
	
			//fill data
			for(j = 0, k = 0; j < (len * 2); j += 2, k++) {
				CTPM_FW[32 + addr + k] = ilitek_hex_2_dec(&save_hex_content[i + 9 + j], 2);
			}
		}
		i += 1 + 2 + 4 + 2 + (len * 2) + 2 + offset;
	}
	if (hex_end == 0) {
		tp_log_err("hex file is invalid\n");
		return -1;
	}
	return 0;
}

static ssize_t ilitek_update_with_hex_read(struct file *pFile, char __user *buf, size_t nCount, loff_t *pPos) {
	int ret = 0, length = 0;
	struct file *filp;
	struct inode *inode;
	mm_segment_t fs;
	off_t fsize;
	unsigned int ap_startaddr = 0xFFFF, df_startaddr = 0xFFFF, ap_endaddr = 0, df_endaddr = 0, ap_checksum = 0, df_checksum = 0;
	unsigned char * CTPM_FW = NULL;
	unsigned char * save_hex_content = NULL;
	CTPM_FW = (unsigned char * )vmalloc(64 * 1024);/* buf size if even */
	//CTPM_FW = kmalloc(64 * 1024, GFP_ATOMIC);
	memset(CTPM_FW, 0, 64 * 1024);
	tp_log_info("\n");
    if (*pPos != 0) {
        return 0;
    }
	if (!(CTPM_FW)) {
		tp_log_err("alloctation CTPM_FW memory failed\n");
		length = sprintf(buf, "alloctation CTPM_FW memory failed\n");
		goto out;
	}
	if (!(strstr(ilitek_hex_path, ".hex"))) {
		tp_log_err("ilitek_hex_path is invalid ilitek_hex_path = %s\n", ilitek_hex_path);
		length = sprintf(buf, "ilitek_hex_path is invalid ilitek_hex_path = %s\n", ilitek_hex_path);
		goto out;
	}
	else {
		filp = filp_open(ilitek_hex_path, O_RDONLY, 0);
		if(IS_ERR(filp)) {
			tp_log_err("hex File Open Error ilitek_hex_path = %s\n", ilitek_hex_path);
			length = sprintf(buf, "hex File Open Error ilitek_hex_path = %s\n", ilitek_hex_path);
			goto out;
		}
		else{
			tp_log_info("hex File Open Right,O_RDONLY %s\n", ilitek_hex_path);
			if(!filp->f_op) {
				tp_log_err("File Operation Method Error\n");
				length = sprintf(buf, "File Operation Method Error\n");
				goto out;
			}
			else {
				inode = filp->f_dentry->d_inode;
				fsize = inode->i_size;

				tp_log_info("File size:%d \n", (int)fsize);
				save_hex_content = (unsigned char * )vmalloc((int)fsize);
				//save_hex_content = kmalloc((int)fsize, GFP_ATOMIC);		/* buf size if even */
				if (!(save_hex_content)) {
					tp_log_err("alloctation save_hex_content memory failed\n");
					length = sprintf(buf, "alloctation save_hex_content memory failed\n");
					goto out;
				}
				fs = get_fs();
				set_fs(KERNEL_DS);

				filp->f_op->read(filp, save_hex_content, fsize, &(filp->f_pos));
				set_fs(fs);

				filp_close(filp, NULL);
			}
		}
	}
	ilitek_irq_disable();
	ret = ilitek_read_tp_info();
	if (ret < 0) {
		tp_log_err("ilitek_read_tp_info err ret = %d\n", ret);
		length = sprintf(buf, "ilitek_read_tp_info err ret = %d\n", ret);
		goto out;
	}
	else {
		if (!ilitek_data->ic_2120) {
			if ((ilitek_data->mcu_ver[0] == 0x11 || ilitek_data->mcu_ver[0] == 0x10) && ilitek_data->mcu_ver[1] == 0x25) {
				df_startaddr = 0xF000;
			}
			else {
				df_startaddr = 0x1F000;
			}
		}
		else {
			df_startaddr = 0xE000;
		}
		ret = ilitek_parse_hex_file(&df_startaddr, &df_endaddr, &df_checksum, &ap_startaddr, &ap_endaddr, &ap_checksum, fsize, CTPM_FW, save_hex_content);
		if (ret < 0) {
			tp_log_err("ilitek_parse_hex_file err ret = %d\n", ret);
			length = sprintf(buf, "ilitek_parse_hex_file err ret = %d\n", ret);
			goto out;
		}
		tp_log_info("ilitek ap_startaddr=0x%X, ap_endaddr=0x%X, ap_checksum=0x%X\n", ap_startaddr, ap_endaddr, ap_checksum);
		tp_log_info("ilitek df_startaddr=0x%X, df_endaddr=0x%X, df_checksum=0x%X\n", df_startaddr, df_endaddr, df_checksum);
		ilitek_data->firmware_updating = true;
		ilitek_data->operation_protection = true;
		if (!(ilitek_data->ic_2120)) {
			ret = ilitek_upgrade_bigger_size_ic(df_startaddr, df_endaddr, df_checksum, ap_startaddr, ap_endaddr, ap_checksum, CTPM_FW);
		}
		else {
			ret = ilitek_upgrade_2120(CTPM_FW);
		}
		ilitek_data->operation_protection = false;
		ilitek_data->firmware_updating = false;
		if (ret < 0) {
			tp_log_err("upgrade fail ret = %d\n", ret);
			length = sprintf(buf, "upgrade fail ret = %d\n", ret);
			goto out;
		}
	}

	ret = ilitek_read_tp_info();
	length = sprintf(buf, "upgrade successfull ilitek firmware version is %d.%d.%d.%d.%d.%d.%d.%d\n", ilitek_data->firmware_ver[0], ilitek_data->firmware_ver[1],
		ilitek_data->firmware_ver[2], ilitek_data->firmware_ver[3], ilitek_data->firmware_ver[4], ilitek_data->firmware_ver[5],
		ilitek_data->firmware_ver[6], ilitek_data->firmware_ver[7]);
out:
	ilitek_irq_enable();
	if (CTPM_FW) {
		vfree(CTPM_FW);
	}
	if (save_hex_content) {
		vfree(save_hex_content);
	}
    *pPos += length;
	return length;
}


static ssize_t ilitek_update_with_hex_write(struct file *pFile, const char __user *buf, size_t size, loff_t *pPos) {	
	int i = 0;
	tp_log_info("size = %d\n", (int)size);
	if (size > 256) {
		tp_log_err("size > 256 not support size = %d\n", (int)size);
	}
	else {
		for (i = 0; i < (size - 1); i++) {
			tp_log_info("%c\n", buf[i]);
			ilitek_hex_path[i] = buf[i];
		}
		ilitek_hex_path[size - 1] = '\0';
		tp_log_info("ilitek_hex_path = %s\n", ilitek_hex_path);
	}
	return size;
}
static const struct file_operations ilitek_proc_fops_fwupdate = { 
	.read = ilitek_update_with_hex_read,
	.write = ilitek_update_with_hex_write,
};
static ssize_t ilitek_firmware_version_read(struct file *pFile, char __user *buf, size_t nCount, loff_t *pPos) {
	int ret = 0;
	int length = 0;
	tp_log_info("\n");
    if (*pPos != 0) {
        return 0;
    }
	ilitek_irq_disable();
	ret = ilitek_read_tp_info();
	ilitek_irq_enable();
	if (ret < 0) {
		tp_log_err("ilitek_read_tp_info err ret = %d\n", ret);
		length = sprintf(buf, "ilitek firmware version read error ret = %d\n", ret);
		
	}
	else {
		length = sprintf(buf, "ilitek firmware version is %d.%d.%d.%d.%d.%d.%d.%d\n", ilitek_data->firmware_ver[0], ilitek_data->firmware_ver[1],
			ilitek_data->firmware_ver[2], ilitek_data->firmware_ver[3], ilitek_data->firmware_ver[4], ilitek_data->firmware_ver[5],
			ilitek_data->firmware_ver[6], ilitek_data->firmware_ver[7]);
	}
    *pPos += length;
	return length;
}

static const struct file_operations ilitek_proc_fops_fwversion = { 
	.read = ilitek_firmware_version_read,
	.write = NULL,
};

int ilitek_create_tool_node(void) {
	int ret = 0;
	// allocate character device driver buffer
	ret = alloc_chrdev_region(&ilitek_dev.devno, 0, 1, "ilitek_file");
	if (ret) {
		tp_log_err("can't allocate chrdev\n");
		//return ret;
	}
	else {
		tp_log_info("%s, register chrdev(%d, %d)\n", __func__, MAJOR(ilitek_dev.devno), MINOR(ilitek_dev.devno));

		// initialize character device driver
		cdev_init(&ilitek_dev.cdev, &ilitek_fops);
		ilitek_dev.cdev.owner = THIS_MODULE;
		ret = cdev_add(&ilitek_dev.cdev, ilitek_dev.devno, 1);
		if (ret < 0) {
			tp_log_err("%s, add character device error, ret %d\n", __func__, ret);
			//return ret;
		}
		else {
			ilitek_dev.class = class_create(THIS_MODULE, "ilitek_file");
			if (IS_ERR(ilitek_dev.class)) {
				tp_log_err("create class, error\n");
				//return ret;
			}
			device_create(ilitek_dev.class, NULL, ilitek_dev.devno, NULL, "ilitek_ctrl");
		}
	}
	ilitek_proc = proc_create("ilitek_ctrl", 0666, NULL, &ilitek_fops);
	if (ilitek_proc == NULL) {
		tp_log_err("proc_create(ilitek_ctrl, 0666, NULL, &ilitek_fops) fail\n");
	}

	ilitek_proc_entry = proc_mkdir("ilitek", NULL);
	if (!ilitek_proc_entry) {
		tp_log_err("Error, failed to creat procfs.\n");
	}
	else {
		if (!proc_create("firmware_version", 0666, ilitek_proc_entry, &ilitek_proc_fops_fwversion)) {
			tp_log_err("Error, failed to creat procfs firmware_version.\n");
			remove_proc_entry("firmware_version", ilitek_proc_entry);
		}
		if (!proc_create("update_firmware", 0666, ilitek_proc_entry, &ilitek_proc_fops_fwupdate)) {
			tp_log_err("Error, failed to creat procfs update_firmware.\n");
			remove_proc_entry("update_firmware", ilitek_proc_entry);
		}
		if (!proc_create("sensor_test_data", 0666, ilitek_proc_entry, &ilitek_proc_fops_sensortest)) {
			tp_log_err("Error, failed to creat procfs sensor_test.\n");
			remove_proc_entry("sensor_test_data", ilitek_proc_entry);
		}
		if (!proc_create("noisefre_data", 0666, ilitek_proc_entry, &ilitek_proc_fops_noisefre)) {
			tp_log_err("Error, failed to creat procfs noisefre_data.\n");
			remove_proc_entry("noisefre_data", ilitek_proc_entry);
		}
	}
	return 0;
}

int ilitek_remove_tool_node(void) {
	cdev_del(&ilitek_dev.cdev);
	unregister_chrdev_region(ilitek_dev.devno, 1);
	device_destroy(ilitek_dev.class, ilitek_dev.devno);
	class_destroy(ilitek_dev.class);
	if (ilitek_proc) {
		tp_log_info("remove procfs ilitek_ctrl.\n");
		remove_proc_entry("ilitek_ctrl", NULL);
		ilitek_proc = NULL;
	}
	if (ilitek_proc_entry) {
		tp_log_info("remove procfs firmware_version.\n");
		remove_proc_entry("firmware_version", ilitek_proc_entry);
		tp_log_info("remove procfs update_firmware.\n");
		remove_proc_entry("update_firmware", ilitek_proc_entry);
		tp_log_info("remove procfs sensor_test_data.\n");
		remove_proc_entry("sensor_test_data", ilitek_proc_entry);
		tp_log_info("remove procfs noisefre_data.\n");
		remove_proc_entry("noisefre_data", ilitek_proc_entry);
		tp_log_info("remove procfs ilitek.\n");
		remove_proc_entry("ilitek", NULL);
		ilitek_proc_entry = NULL;
	}
	return 0;
}
#endif
