/*
 * somiq-am35.c - board file for SomIQ-AM35
 *
 * Author: Maxim Podbereznyy <lisarden@gmail.com>
 *
 * Based on ti/am3517crane/am3517crane.c
 *
 * Copyright (C) 2012 MENTOREL Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/mem.h>
#include <asm/arch/mux.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-types.h>
#include <i2c.h>
#include "am3517somiq.h"

#define TPS65910_I2C_BUS			0
#define EXPANSION_EEPROM_I2C_BUS	1
#define EXPANSION_EEPROM_I2C_ADDRESS	0x50
#define CONFIG_SYS_I2C_EEPROM_ADDR      0x50    /* EEPROM 24AA02E48 */
#define CONFIG_SYS_I2C_EEPROM_OFF_ADDR  0xFA    /* EEPROM 24AA02E48 */
#define CONFIG_SYS_I2C_EEPROM_LEN       6       /* Bytes of address */

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
	gd->bd->bi_arch_number = MACH_TYPE_SOMIQ_AM35;
	/* boot param addr */
	gd->bd->bi_boot_params = (OMAP34XX_SDRC_CS0 + 0x100);

	return 0;
}

/*
 * Routine: misc_init_r
 * Description: Init i2c, ethernet, etc... (done here so udelay works)
 */
int misc_init_r(void)
{
#ifdef CONFIG_DRIVER_OMAP34XX_I2C
	i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
#endif

	dieid_num_r();

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
	MUX_AM3517SOMIQ();
}

#if defined(CONFIG_GENERIC_MMC) && !defined(CONFIG_SPL_BUILD)
int board_mmc_init(bd_t *bis)
{
	omap_mmc_init(0);
	return 0;
}
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
			eth_setenv_enetaddr("ethaddr", e);
			setenv("ethmac", str);
			printf("MAC address EUI-48 is %s\n",str);
		}
		else printf("EEPROM read failed\n");
	}
	else printf("EEPROM probe failed\n");

	/* Switch back to default bus and pin mux */
	i2c_set_bus_num(TPS65910_I2C_BUS);

	return ret;
}
