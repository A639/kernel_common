/*
 * Hardware definitions for ASUS MyPal A639
 *
 * Authors:
 *	Oran Avraham <oranav@gmail.com>
 *	Adir Gruss <adirgruss@gmail.com>
 *	Miki Komraz <mikomraz@gmail.com>
 *
 * Based on work of:
 *	Alexander Tarasikov <alexander.tarasikov@gmail.com>
 *	Xiao Huang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/delay.h>

#include <linux/leds.h>
#include <linux/pwm_backlight.h>
#include <linux/gpio_keys.h>
#include <linux/power_supply.h>
#include <linux/wm97xx.h>
#include <linux/usb/gpio_vbus.h>
#include <linux/pda_power.h>

#include <asm/gpio.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <mach/mmc.h>
#include <mach/udc.h>
#include <mach/audio.h>
#include <mach/pxafb.h>
#include <mach/pxa27x.h>
#include <mach/pxa27x-udc.h>
#include <mach/hardware.h>

#include <mach/a639.h>
#include "generic.h"
#include "devices.h"

/******************************************************************************
 * GPIO setup
 ******************************************************************************/
static unsigned long a639_pin_config[] __initdata = {
	/* A639-specific GPIOs */
	GPIO10_GPIO | WAKEUP_ON_EDGE_FALL,
	GPIO12_GPIO | WAKEUP_ON_EDGE_FALL,
	GPIO21_GPIO | WAKEUP_ON_EDGE_FALL,
	GPIO25_GPIO | WAKEUP_ON_EDGE_RISE,
	GPIO27_GPIO | WAKEUP_ON_EDGE_BOTH,
	GPIO81_GPIO | WAKEUP_ON_EDGE_BOTH,
	GPIO97_GPIO | WAKEUP_ON_EDGE_FALL,

	/* SD card */
	GPIO14_GPIO | WAKEUP_ON_EDGE_BOTH,
	GPIO79_GPIO,

	/* PWM 0 (Backlight) */
	GPIO16_PWM0_OUT,

	/* AC97 */
	GPIO28_AC97_BITCLK,
	GPIO29_AC97_SDATA_IN_0,
	GPIO30_AC97_SDATA_OUT,
	GPIO31_AC97_SYNC,
	GPIO98_AC97_SYSCLK,
	GPIO113_AC97_nRESET,

	/* PCMCIA + WiFi */
	GPIO15_nPCE_1,
	GPIO49_nPWE,
	GPIO48_nPOE,
	GPIO50_nPIOR,
	GPIO51_nPIOW,
	GPIO55_nPREG,
	GPIO56_nPWAIT,
	GPIO57_nIOIS16,

	/* LCD */
	GPIOxx_LCD_TFT_16BPP,

	/* STUART (IrDA) */
	GPIO46_STUART_RXD,
	GPIO47_STUART_TXD,

	/* FFUART (RS-232) */
	GPIO91_GPIO | WAKEUP_ON_EDGE_BOTH,
	GPIO34_FFUART_RXD,
	GPIO35_FFUART_CTS,
	GPIO36_FFUART_DCD,
	GPIO37_FFUART_DSR,
	GPIO39_FFUART_TXD,
	GPIO40_FFUART_DTR,
	GPIO41_FFUART_RTS,

	/* MMC */
	GPIO32_MMC_CLK,
	GPIO92_MMC_DAT_0,
	GPIO109_MMC_DAT_1,
	GPIO110_MMC_DAT_2,
	GPIO111_MMC_DAT_3,
	GPIO112_MMC_CMD,

	/* BTUART */
	GPIO42_BTUART_RXD,
	GPIO43_BTUART_TXD,
	GPIO44_BTUART_CTS,
	GPIO45_BTUART_RTS,

	/* I2C */
	GPIO117_I2C_SCL,
	GPIO118_I2C_SDA,
};


/******************************************************************************
 * LCD
 ******************************************************************************/
static struct gpio a639_lcd_gpios[] = {
	{ GPIO_NR_A639_LCD_POWER_1, GPIOF_INIT_LOW, "LCD power 1" },
	{ GPIO_NR_A639_LCD_POWER_2, GPIOF_INIT_LOW, "LCD power 2" },
};

static int a639_setup_fb_gpios(void)
{
	return gpio_request_array(ARRAY_AND_SIZE(a639_lcd_gpios));
}

static void a639_lcd_power(int on, struct fb_var_screeninfo *si)
{
	gpio_set_value(GPIO_NR_A639_LCD_POWER_1, !!on);
	ndelay(1000);
	gpio_set_value(GPIO_NR_A639_LCD_POWER_2, !!on);
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
};

static void __init a639_lcd_init(void)
{
	if (!a639_setup_fb_gpios())
		pxa_set_fb_info(NULL, &a639_pxafb_mach_info);
	else
		pr_err("Failed to setup fb gpios\n");
}


/******************************************************************************
 * Backlight
 ******************************************************************************/
static struct platform_pwm_backlight_data a639_backlight_data = {
	.pwm_id = 0,
	.max_brightness = 255,
	.dft_brightness = 150,
	.pwm_period_ns = 78769,
};

static struct platform_device a639_backlight = {
	.name = "pwm-backlight",
	.id = -1,
	.dev = {
		.parent = &pxa27x_device_pwm0.dev,
		.platform_data = &a639_backlight_data,
	},
};


/******************************************************************************
 * MMC controller
 ******************************************************************************/
static struct pxamci_platform_data a639_mci_platform_data = {
	.ocr_mask		= MMC_VDD_32_33 | MMC_VDD_33_34,
	.gpio_power		= -1,
	.gpio_card_detect	= GPIO_NR_A639_SD_DETECT_N,
	.gpio_card_ro		= GPIO_NR_A639_SD_RO,
	.detect_delay_ms	= 200,
};


/******************************************************************************
 * Buttons
 ******************************************************************************/
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
/*	GPIO_BUTTON(GPIO_NR_A639_BUTTON_HOLD, KEY_???, "Hold button"),*/
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

struct platform_device a639_buttons = {
	.name	= "gpio-keys",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_key_info,
	},
};


/******************************************************************************
 * USB Device Controller (UDC)
 ******************************************************************************/
static struct gpio_vbus_mach_info gpio_vbus_data = {
	.gpio_vbus = GPIO_NR_A639_USB_CABLE_DETECT_N,
	.gpio_vbus_inverted = 1,
	.gpio_pullup = GPIO_NR_A639_USB_PULLUP,
};

static struct platform_device a639_vbus = {
	.name = "gpio-vbus",
	.id = -1,
	.dev = {
		.platform_data = &gpio_vbus_data,
	},
};

static void __init a639_udc_init(void)
{
	if (!gpio_request(GPIO_NR_A639_USB_PULLUP, "UDC Vbus")) {
		gpio_direction_output(GPIO_NR_A639_USB_PULLUP, 1);
		gpio_free(GPIO_NR_A639_USB_PULLUP);
	}
}


/******************************************************************************
 * Power supply
 ******************************************************************************/
static char *supplicants[] = {
	"battery",
};

static int a639_usb_online(void)
{
	return !gpio_get_value(GPIO_NR_A639_USB_CHARGE_DETECT_N);
}

static int a639_ac_online(void)
{
	return !gpio_get_value(GPIO_NR_A639_USB_AC_DETECT_N);
}

static struct pda_power_pdata power_pdata = {
	.is_usb_online = a639_usb_online,
	.is_ac_online = a639_ac_online,
	.supplied_to = supplicants,
	.num_supplicants = ARRAY_SIZE(supplicants),
};

static struct resource power_resources[] = {
	[0] = {
		.name = "ac",
		.start = gpio_to_irq(GPIO_NR_A639_USB_CABLE_DETECT_N),
		.end = gpio_to_irq(GPIO_NR_A639_USB_CABLE_DETECT_N),
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE |
			IORESOURCE_IRQ_LOWEDGE,
	      },
	[1] = {
		.name = "usb",
		.start = gpio_to_irq(GPIO_NR_A639_USB_CABLE_DETECT_N),
		.end = gpio_to_irq(GPIO_NR_A639_USB_CABLE_DETECT_N),
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE |
			IORESOURCE_IRQ_LOWEDGE,
	      },
};

static struct platform_device a639_powerdev = {
	.name = "pda-power",
	.id = -1,
	.resource = power_resources,
	.num_resources = ARRAY_SIZE(power_resources),
	.dev = {
		.platform_data = &power_pdata,
	},
};


/******************************************************************************
 * LEDs
 ******************************************************************************/
static struct gpio_led a639_gpio_leds[] = {
	/* TODO: Add charging and connectivity LEDs */
	{
		.name = "a639:blue:buttons",
		.gpio = GPIO_NR_A639_LED_BUTTONS,
	},
};

static struct gpio_led_platform_data a639_gpio_leds_platform_data = {
	.leds = a639_gpio_leds,
	.num_leds = ARRAY_SIZE(a639_gpio_leds),
};

static struct platform_device a639_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev = {
		.platform_data = &a639_gpio_leds_platform_data,
	},
};


/******************************************************************************
 * WM9712 controller
 ******************************************************************************/
static struct wm97xx_batt_pdata a639_battery_data = {
	/* FIXME: Reading batt_aux fails */
	.batt_aux = WM97XX_AUX_ID3,
	.temp_aux = WM97XX_AUX_ID2,
	.charge_gpio = -1,
	.max_voltage = 0xa30,
	.min_voltage = 0x800,
	.batt_mult = 1,
	.batt_div = 1,
	.temp_div = 1,
	.temp_mult = 1,
	.batt_tech = POWER_SUPPLY_TECHNOLOGY_LION,
	.batt_name = "battery",
};

static struct wm97xx_pdata a639_wm97xx_pdata = {
	.batt_pdata = &a639_battery_data,
};

static pxa2xx_audio_ops_t a639_ac97_pdata = {
	.codec_pdata = {
		&a639_wm97xx_pdata,
	}
};


/******************************************************************************
 * Initialization
 ******************************************************************************/
static struct platform_device *devices[] __initdata = {
	&a639_backlight,
	&a639_buttons,
	&a639_vbus,
	&a639_powerdev,
	&a639_leds,
};

static void __init a639_init(void)
{
	pxa2xx_mfp_config(ARRAY_AND_SIZE(a639_pin_config));
	pxa_set_ffuart_info(NULL);
	pxa_set_btuart_info(NULL);
	pxa_set_stuart_info(NULL);

	a639_lcd_init();
	a639_udc_init();
	pxa_set_mci_info(&a639_mci_platform_data);
	pxa_set_ac97_info(&a639_ac97_pdata);

	platform_add_devices(devices, ARRAY_SIZE(devices));
}

MACHINE_START(A639, "A639")
	/* Maintainer: Oran Avraham (oranav@gmail.com) */
	.boot_params	= 0xa0000100,
	.map_io		= pxa27x_map_io,
	.init_irq	= pxa27x_init_irq,
	.init_machine	= a639_init,
	.timer		= &pxa_timer,
MACHINE_END
