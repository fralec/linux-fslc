#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <asm-generic/bug.h>


#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/of_gpio.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>

static int customize_gpio_probe(struct platform_device *pdev)
{
	int rst,ret;
	int LCD_PWREN,DISP0_BLEN;
	int RTP_RST;
	struct device_node* np = pdev->dev.of_node;
	
	printk("ben: customize_gpio_probe \n");
	
	//LCD
	rst = of_get_named_gpio(np, "LCD3P3V_Npwren", 0);
	if (!gpio_is_valid(rst)){
    	printk("can not find LCD3P3V_Npwren gpio pins\n");
        return -1;
	}
    ret = gpio_request(rst, "LCD3P3V_Npwren");
    if(ret){
    	printk("request gpio LCD3P3V_Npwren failed\n");
        return;
  	}

	gpio_direction_output(rst, 0);
	//gpio_set_value(rst, 0);
	//mdelay(100);
	//gpio_set_value(rst, 1);


	//LCD_BL_EN
	rst = of_get_named_gpio(np, "LCD_BL_EN", 0);
	if (!gpio_is_valid(rst)){
    	printk("can not find LCD_BL_EN gpio pins\n");
        return -1;
	}
    ret = gpio_request(rst, "LCD_BL_EN");
    if(ret){
    	printk("request gpio LCD_BL_EN failed\n");
        return;
  	}

	gpio_direction_output(rst, 1);
	//gpio_set_value(rst, 0);
	//mdelay(100);
	//gpio_set_value(rst, 1);	
	
	//VDDLCD_PWREN
    rst = of_get_named_gpio(np, "VDDLCD_PWREN", 0);
    if (!gpio_is_valid(rst)){
        printk("can not find VDDLCD_PWREN gpio pins\n");
        return -1;
    }
    ret = gpio_request(rst, "VDDLCD_PWREN");
    if(ret){
        printk("request gpio VDDLCD_PWREN failed\n");
        return;
    }

    gpio_direction_output(rst, 1);

	//TP_PWR_EN
    rst = of_get_named_gpio(np, "TP_PWR_EN", 0);
    if (!gpio_is_valid(rst)){
		printk("can not find TP_PWR_EN gpio pins\n");
        return -1;
    }
    ret = gpio_request(rst, "TP_PWR_EN");
    if(ret){
        printk("request gpio TP_PWR_EN failed\n");
        return;
    }

    gpio_direction_output(rst, 1);

	
	//HUB_reset
    rst = of_get_named_gpio(np, "HUB_reset", 0);
    if (!gpio_is_valid(rst)){
		printk("can not find HUB_reset gpio pins\n");
        return -1;
    }
    ret = gpio_request(rst, "HUB_reset");
    if(ret){
        printk("request gpio HUB_reset failed\n");
        return;
    }

    gpio_direction_output(rst, 0);
	mdelay(200);
	gpio_direction_output(rst, 1);
	
	//
	
	return 0;
}

static int customize_gpio_remove(struct platform_device *pdev)
{
#if 0
         __novo_gpio_remove(pdev);                                     
         sysfs_remove_group(&pdev->dev.kobj, &novo_gpio_attr_group);
#endif
         return 0;
}

static const struct of_device_id gpio_dt_ids[] = {
    { .compatible = "customize,gpio", },
    { /* sentinel */ }
};

static struct platform_driver customize_gpio_driver = {
	.probe		= customize_gpio_probe,               
   	.remove     = customize_gpio_remove,
	.driver =
    {
        .name = "customize-gpio",
        .owner = THIS_MODULE,
        .of_match_table = gpio_dt_ids,
    }
};

static int __init customize_gpio_init(void)
{
	int ret;

    printk("customize gpio driver \n");

   	ret = platform_driver_register(&customize_gpio_driver);            
    if(ret)
    	printk("failed to register customize gpio driver \n");
         

	return ret;
}

static void customize_gpio_exit(void)
{
	platform_driver_unregister(&customize_gpio_driver);
}

module_init(customize_gpio_init);
module_exit(customize_gpio_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ben");
MODULE_DESCRIPTION("hio novo gpio driver");

