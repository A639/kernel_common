/*
 * Hardware definitions for the Asus A639
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * 2008-11-09   Initial Version by Xiao Huang
 * 2011-11-25	Modified to match A639
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>

#include <linux/platform_device.h>

#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>

#include <linux/gpio_keys.h>
#include <linux/usb/gpio_vbus.h>
#include <linux/pda_power.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>

#include <asm/gpio.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <linux/spi/pxa2xx_spi.h>
#include <mach/mmc.h>
#include <mach/udc.h>
#include <mach/audio.h>
#include <mach/ohci.h>
#include <mach/pxafb.h>

#include <mach/pxa27x.h>
#include <mach/pxa27x-udc.h>

#include <mach/a639.h>

#include "generic.h"

/**
 * LCD
 */

static void a639_lcd_power(int on, struct fb_var_screeninfo *si)
{
	if (on) {
		gpio_set_value(GPIO_NR_A639_LCD_POWER_1, 1);
		ndelay(1000);
		gpio_set_value(GPIO_NR_A639_LCD_POWER_2, 1);
	} else {
		gpio_set_value(GPIO_NR_A639_LCD_POWER_1, 0);
		ndelay(1000);
		gpio_set_value(GPIO_NR_A639_LCD_POWER_2, 0);
	}
}

static int a639_setup_fb_gpios(void)
{
	int err;

	if ((err = gpio_request(GPIO_NR_A639_LCD_POWER_1, "LCD_EN1")))
		goto out_err;

	if ((err = gpio_direction_output(GPIO_NR_A639_LCD_POWER_1, 0)))
		goto out_err_lcd1;

	if ((err = gpio_request(GPIO_NR_A639_LCD_POWER_2, "LCD_EN2")))
		goto out_err_lcd1;

	if ((err = gpio_direction_output(GPIO_NR_A639_LCD_POWER_2, 0)))
		goto out_err_lcd2;

	return 0;

out_err_lcd1:
	gpio_free(GPIO_NR_A639_LCD_POWER_1);
out_err_lcd2:
	gpio_free(GPIO_NR_A639_LCD_POWER_2);
out_err:
	return err;
}

static struct pxafb_mode_info a639_pxafb_mode_info = {
	.pixclock	= 269230,
	.bpp		= 16,
	.xres		= 240,
	.yres		= 320,
	.hsync_len	= 10,
	.vsync_len	= 2,
	.left_margin	= 20,
	.upper_margin	= 4,
	.right_margin	= 10,
	.lower_margin	= 1,
	.sync		= 0,
};

static struct pxafb_mach_info a639_pxafb_mach_info = {
	.modes			= &a639_pxafb_mode_info,
	.num_modes		= 1,
	.lcd_conn		= LCD_COLOR_TFT_16BPP,
	.pxafb_lcd_power	= a639_lcd_power,
	.pxafb_backlight_power	= NULL,
};

static void __init a639_lcd_init(void)
{
	if (a639_setup_fb_gpios())
		pr_err("Failed to setup fb gpios\n");
	else
		pxa_set_fb_info(NULL, &a639_pxafb_mach_info);

	a639_lcd_power(0, NULL);
}

/**
 * MMC
 */

static struct pxamci_platform_data a639_mci_platform_data = {
	.ocr_mask		= MMC_VDD_32_33 | MMC_VDD_33_34,
	.gpio_power		= -1,
	.gpio_card_detect	= GPIO_NR_A639_SD_DETECT_N,
	.gpio_card_ro		= GPIO_NR_A639_SD_RO,
	.detect_delay_ms	= 200,
};

static void __init a639_mmc_init(void)
{
	pxa_set_mci_info(&a639_mci_platform_data);
}

/**
 * Buttons
 */

#define GPIO_BUTTON(gpio_num, event_code, description)	\
	{						\
		.code = event_code,			\
		.gpio = gpio_num,			\
		.active_low = 0,			\
		.desc = description,			\
		.type = EV_KEY,				\
		.wakeup = 0,				\
		.debounce_interval = 4			\
	}

static struct gpio_keys_button gpio_keys[] = {
	GPIO_BUTTON(GPIO_NR_A639_BUTTON_POWER, KEY_POWER, "Power button"),
	GPIO_BUTTON(GPIO_NR_A639_BUTTON_UP, KEY_UP, "Up button"),
	GPIO_BUTTON(GPIO_NR_A639_BUTTON_DOWN, KEY_DOWN, "Down button"),
	GPIO_BUTTON(GPIO_NR_A639_BUTTON_LEFT, KEY_LEFT, "Left button"),
	GPIO_BUTTON(GPIO_NR_A639_BUTTON_RIGHT, KEY_RIGHT, "Right button"),
	GPIO_BUTTON(GPIO_NR_A639_BUTTON_ENTER, KEY_KPENTER, "Action button"),
	GPIO_BUTTON(GPIO_NR_A639_BUTTON_NOTES, KEY_RECORD, "Notes button"),
	GPIO_BUTTON(GPIO_NR_A639_BUTTON_CONTACTS, KEY_ADDRESSBOOK, "Contacts button"),
	GPIO_BUTTON(GPIO_NR_A639_BUTTON_CALENDAR, KEY_CALENDAR, "Calendar button"),
	GPIO_BUTTON(GPIO_NR_A639_BUTTON_ROTATE, KEY_SEARCH, "Rotate button"),
};

struct gpio_keys_platform_data gpio_key_info = {
	.buttons	= gpio_keys,
	.nbuttons	= ARRAY_SIZE(gpio_keys),
};

struct platform_device keys_gpio = {
	.name	= "gpio-keys",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_key_info,
	},
};

static void __init a639_button_init(void)
{
	platform_device_register(&keys_gpio);
}

/**
 * USB Device Controller
 */

static int a639_udc_is_connected(void)
{
	return (gpio_get_value(GPIO_NR_A639_USB_DETECT) == 0);
}

static struct pxa2xx_udc_mach_info a639_udc_info = {
	.udc_is_connected = a639_udc_is_connected,
	.gpio_pullup_inverted = 0,
	.gpio_pullup = GPIO_NR_A639_USB_PULLUP,
};

static void a639_udc_init(void)
{
	if (gpio_direction_input(GPIO_NR_A639_USB_DETECT))
		return;

	pxa_set_udc_info(&a639_udc_info);
}

/*
 * Init
 */

static void __init a639_map_io(void)
{
	pxa27x_map_io();
}

static void __init a639_init_irq(void)
{
	pxa27x_init_irq();
}

static void __init a639_init(void)
{
	pxa_set_ffuart_info(NULL);
	pxa_set_btuart_info(NULL);
	pxa_set_stuart_info(NULL);

	a639_lcd_init();
	a639_mmc_init();
	a639_button_init();
	pxa_set_ac97_info(NULL);
	a639_udc_init();

	printk(KERN_INFO "A639 is up\n");
}

MACHINE_START(A639, "Asus A639")
	/* Maintainer: Oran Avraham (oranav@gmail.com) */
	.boot_params	= 0xa0000100,
	.map_io		= a639_map_io,
	.init_irq	= a639_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= a639_init,
MACHINE_END
