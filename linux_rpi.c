/*
 * This file is part of the flashrom project.
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
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
 */

#include <stdlib.h>
#include <bcm2835.h>
#include "flash.h"
#include "programmer.h"
#include "hwaccess.h"

static const uint8_t pin_ce = 1;
static const uint8_t pin_we = 14;
static const uint8_t pin_oe = 8;

static const uint8_t pin_data[] = { 6, 13, 19, 26, 21, 20, 16, 12 };
static const uint8_t pin_addr[] = { 5, 0, 11, 9, 10, 22, 27, 17, 23, 24, 7, 25, 4, 18, 15, 3, 2 };

static void rpi_chip_writeb(const struct flashctx *flash, uint8_t val,
				chipaddr addr);
static uint8_t rpi_chip_readb(const struct flashctx *flash,
				  const chipaddr addr);
static const struct par_master par_master_rpi = {
		.chip_readb		= rpi_chip_readb,
		.chip_readw		= fallback_chip_readw,
		.chip_readl		= fallback_chip_readl,
		.chip_readn		= fallback_chip_readn,
		.chip_writeb		= rpi_chip_writeb,
		.chip_writew		= fallback_chip_writew,
		.chip_writel		= fallback_chip_writel,
		.chip_writen		= fallback_chip_writen,
};

static int rpi_shutdown(void *data)
{
	bcm2835_close();
	return 0;
}

int rpi_init(void)
{
	if (!bcm2835_init())
		return 1;

	bcm2835_gpio_set_multi((1 << pin_ce) | (1 << pin_oe) | (1 << pin_we));
	bcm2835_gpio_fsel(pin_ce, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(pin_oe, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(pin_we, BCM2835_GPIO_FSEL_OUTP);

	for (int i = 0; i < 17; i++)
		bcm2835_gpio_fsel(pin_addr[i], BCM2835_GPIO_FSEL_OUTP);

	if (register_shutdown(rpi_shutdown, NULL))
		return 1;

	register_par_master(&par_master_rpi, BUS_PARALLEL);

	return 0;
}

static void rpi_set_addr(chipaddr addr)
{
	uint32_t mask_set = 0, mask_clr = 0;

	for (int i = 0; i < 17; i++) {
		if ((addr >> i) & 1)
			mask_set |= 1 << pin_addr[i];
		else
			mask_clr |= 1 << pin_addr[i];
	}

	bcm2835_gpio_set_multi(mask_set);
	bcm2835_gpio_clr_multi(mask_clr);
}

static void rpi_chip_writeb(const struct flashctx *flash, uint8_t val,
				chipaddr addr)
{
	rpi_set_addr(addr);

	for (int i = 0; i < 8; i++)
		bcm2835_gpio_fsel(pin_data[i], BCM2835_GPIO_FSEL_OUTP);

	uint32_t mask_set = 0, mask_clr = 0;

	for (int i = 0; i < 8; i++) {
		if ((val >> i) & 1)
			mask_set |= 1 << pin_data[i];
		else
			mask_clr |= 1 << pin_data[i];
	}

	bcm2835_gpio_set_multi(mask_set);
	bcm2835_gpio_clr_multi(mask_clr);

	bcm2835_gpio_clr(pin_ce);
	bcm2835_gpio_clr(pin_we);
	bcm2835_gpio_set(pin_we);
	bcm2835_gpio_set(pin_ce);
}

static uint8_t rpi_chip_readb(const struct flashctx *flash,
				  const chipaddr addr)
{
	rpi_set_addr(addr);

	for (int i = 0; i < 8; i++)
		bcm2835_gpio_fsel(pin_data[i], BCM2835_GPIO_FSEL_INPT);

	bcm2835_gpio_clr(pin_ce);
	bcm2835_gpio_clr(pin_oe);

        uint8_t val = 0;

	for (int i = 0; i < 8; i++) {
		val |= bcm2835_gpio_lev(pin_data[i]) << i;
	}

	bcm2835_gpio_set(pin_oe);
	bcm2835_gpio_set(pin_ce);

	return val;
}
