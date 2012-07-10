/*
 * (C) Copyright 2004-2011
 * Texas Instruments, <www.ti.com>
 *
 * Author :
 *	Sunil Kumar <sunilsaini05@gmail.com>
 *	Shashi Ranjan <shashiranjanmca05@gmail.com>
 *
 * Derived from Beagle Board and 3430 SDP code by
 *	Richard Woodruff <r-woodruff2@ti.com>
 *	Syed Mohammed Khasim <khasim@ti.com>
 *
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <common.h>
#ifdef CONFIG_STATUS_LED
#include <status_led.h>
#endif
#include <twl4030.h>
#include <linux/mtd/nand.h>
#include <asm/io.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/mux.h>
#include <asm/arch/mem.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/mach-types.h>
#include "blueshark.h"
#include <command.h>

#define pr_debug(fmt, args...) debug(fmt, ##args)

#define TWL4030_I2C_BUS			0
#define EXPANSION_EEPROM_I2C_BUS	1
#define EXPANSION_EEPROM_I2C_ADDRESS	0x50
#define CONFIG_SYS_I2C_EEPROM_ADDR      0x50    /* EEPROM 24AA02E48 */
#define CONFIG_SYS_I2C_EEPROM_OFF_ADDR  0xFA    /* EEPROM 24AA02E48 */
#define CONFIG_SYS_I2C_EEPROM_LEN       6       /* Bytes of address */
#define DVI_EDID_I2C_BUS	2
#define DVI_EDID_I2C_ADDRESS	0x50

static int read_eeprom(void);

DECLARE_GLOBAL_DATA_PTR;

/*
 * Routine: board_init
 * Description: Early hardware init.
 */
int board_init(void)
{
	gpmc_init(); /* in SRAM or SDRAM, finish GPMC */
	/* board id for Linux */
	gd->bd->bi_arch_number = MACH_TYPE_BLUESHARK;
	/* boot param addr */
	gd->bd->bi_boot_params = (OMAP34XX_SDRC_CS0 + 0x100);

#if defined(CONFIG_STATUS_LED) && defined(STATUS_LED_BOOT)
	status_led_set (STATUS_LED_BOOT, STATUS_LED_ON);
#endif

	return 0;
}

#ifdef CONFIG_SPL_BUILD
/*
 * Routine: get_board_mem_timings
 * Description: If we use SPL then there is no x-loader nor config header
 * so we have to setup the DDR timings ourself on both banks.
 */
void get_board_mem_timings(u32 *mcfg, u32 *ctrla, u32 *ctrlb, u32 *rfr_ctrl,
		u32 *mr)
{
	int nand_mfr, nand_id;

	/*
	 * We need to identify what PoP memory is on the board so that
	 * we know what timings to use.  If we can't identify it then
	 * we know it's an xM.  To map the ID values please see nand_ids.c
	 */
	identify_nand_chip(&nand_mfr, &nand_id);

	*mr = MICRON_V_MR_165;
#ifdef DDRSIZE_256M
	/* BlueShark, 256MB DDR 200MHz */
	*mcfg = MICRON_V_MCFG_200(256 << 20);
	*ctrla = MICRON_V_ACTIMA_200;
	*ctrlb = MICRON_V_ACTIMB_200;
	*rfr_ctrl = SDP_3430_SDRC_RFR_CTRL_200MHz;
#elif DDRSIZE_512M
	/* BlueShark, 512MB DDR 200MHz */
	*mcfg = MICRON_V_MCFG_200(512 << 20);
	*ctrla = MICRON_V_ACTIMA_200;
	*ctrlb = MICRON_V_ACTIMB_200;
	*rfr_ctrl = SDP_3430_SDRC_RFR_CTRL_200MHz;
#else
	/* Assume 128MB and Micron/165MHz timings to be safe */
	*mcfg = MICRON_V_MCFG_165(128 << 20);
	*ctrla = MICRON_V_ACTIMA_165;
	*ctrlb = MICRON_V_ACTIMB_165;
	*rfr_ctrl = SDP_3430_SDRC_RFR_CTRL_165MHz;
#endif
}
#endif

/*
 * Configure DSS to display background color on DVID
 * Configure VENC to display color bar on S-Video
 */
void blueshark_display_init(void)
{
	omap3_dss_venc_config(&venc_config_std_tv, VENC_HEIGHT, VENC_WIDTH);
	omap3_dss_panel_config(&dvid_cfg);
}

/*
 * Routine: misc_init_r
 * Description: Configure board specific parts
 */
int misc_init_r(void)
{
	MUX_BBTOYS_WIFI();

	twl4030_power_init();
	twl4030_led_init(TWL4030_LED_LEDEN_LEDAON | TWL4030_LED_LEDEN_LEDBON);

	dieid_num_r();
	blueshark_display_init();
	omap3_dss_enable();

	/* Read EEPROM with EUI-48 */
	read_eeprom();

	return 0;
}

/*
 * Routine: set_muxconf_regs
 * Description: Setting up the configuration Mux registers specific to the
 *		hardware. Many pins need to be moved from protect to primary
 *		mode.
 */
void set_muxconf_regs(void)
{
	MUX_BLUESHARK();
}

#if defined(CONFIG_GENERIC_MMC) && !defined(CONFIG_SPL_BUILD)
int board_mmc_init(bd_t *bis)
{
	omap_mmc_init(0);
	return 0;
}
#endif

#ifndef CONFIG_SPL_BUILD
/*
 * This command returns the status of the user button on Atoll
 * Input - none
 * Returns - 	1 if button is held down
 *		0 if button is not held down
 */
int do_userbutton(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int     button = 0;
	int	gpio;

	/*
	 * pass address parameter as argv[0] (aka command name),
	 * and all remaining args
	 */
	gpio = 7;
	gpio_request(gpio, "");
	gpio_direction_input(gpio);
	printf("The user button is currently ");
	if (gpio_get_value(gpio))
	{
		button = 1;
		printf("PRESSED.\n");
	}
	else
	{
		button = 0;
		printf("NOT pressed.\n");
	}

	return !button;
}

/* -------------------------------------------------------------------- */

U_BOOT_CMD(
	userbutton, CONFIG_SYS_MAXARGS, 1,	do_userbutton,
	"Return the status of the USER button",
	""
);
#endif

/**
 * read_eeprom - read the EEPROM into memory
 */
static int read_eeprom(void)
{
	int ret;
	char e[CONFIG_SYS_I2C_EEPROM_LEN];
	char str[sizeof("00:00:00:00:00:00")];

	i2c_set_bus_num(EXPANSION_EEPROM_I2C_BUS);

	/* Check if expansion EEPROM is there*/
	if(i2c_probe(CONFIG_SYS_I2C_EEPROM_ADDR) == 0) {
		/* EEPROM found, read configuration data */
		ret = i2c_read(CONFIG_SYS_I2C_EEPROM_ADDR,
				CONFIG_SYS_I2C_EEPROM_OFF_ADDR,
				1, (uchar *)&e, sizeof(e));
		if (!ret) {
			/* Set Node address */
			sprintf(str,"%02x:%02x:%02x:%02x:%02x:%02x",
					e[0],e[1],e[2],e[3],e[4],e[5]);
			setenv("ethmac", str);
			printf("MAC address EUI-48 is %s\n",str);
		}
		else printf("EEPROM read failed\n");
	}
	else printf("EEPROM probe failed\n");

	/* Switch back to default bus and pin mux */
	i2c_set_bus_num(TWL4030_I2C_BUS);

	return ret;
}

