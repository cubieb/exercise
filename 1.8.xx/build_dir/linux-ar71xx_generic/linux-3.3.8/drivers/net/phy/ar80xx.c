/*
 * ar80xx.c: ar80xx(ar8031/ar8033/ar8035) PHY driver
 *
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
 * Copyright (C) 2009 Felix Fietkau <nbd@openwrt.org>
 * Copyright (C) 2011-2012 Gabor Juhos <juhosg@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/if.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>
#include <linux/bitops.h>
#include <net/genetlink.h>
#include <linux/delay.h>
#include <linux/phy.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/lockdep.h>
#include "ar80xx.h"
#include <asm/mach-ath79/ar71xx_regs.h>

static int
ar8033_config_init(struct phy_device *pdev)
{
	u32 v;
	v = phy_read(pdev, AR80XX_REG_CHIP_CONFIG);
	phy_write(pdev, AR80XX_REG_CHIP_CONFIG, AR80XX_BT_BX_REG_SEL | v);

	pdev->autoneg = AUTONEG_ENABLE;

	return 0;
}

static int
ar8033_read_status(struct phy_device *pdev)
{
	void __iomem *base;

	genphy_read_status(pdev);

	base = ioremap(AR71XX_PLL_BASE, AR71XX_PLL_SIZE);

	if (pdev->speed == SPEED_1000) {
		__raw_writel( ETH_SGMII_GIGE_SET(1) | ETH_SGMII_CLK_SEL_SET(1),
			base + ETH_SGMII_ADDRESS_OFFSET);
	} else if (pdev->speed == SPEED_100) {
		__raw_writel( ETH_SGMII_PHASE0_COUNT_SET(1) | ETH_SGMII_PHASE1_COUNT_SET(1),
			base + ETH_SGMII_ADDRESS_OFFSET);
	} else {
		__raw_writel( ETH_SGMII_PHASE0_COUNT_SET(19) | ETH_SGMII_PHASE1_COUNT_SET(19),
			base + ETH_SGMII_ADDRESS_OFFSET);
	}
	iounmap(base);

	return 0;
}

static int
ar8033_config_aneg(struct phy_device *pdev)
{
	u32 v;

	v = phy_read(pdev, MII_BMCR);
	phy_write(pdev, MII_BMCR, v | AR80XX_AUTO_NEGO);

	v = genphy_config_aneg(pdev);
	if (v < 0) {
		printk("%s, Error: 0x%x\n", __func__, v);
		return v;
	}

	return 0;
}

static int
ar8033_probe(struct phy_device *pdev)
{
	return 0;
}

static void
ar8033_remove(struct phy_device *pdev)
{
}
/*Ops For ar8035*/
static void ar8035_tx_delay_enable(struct phy_device *pdev)
{
    u32 v;
    phy_write(pdev, 0x1d, 0x05);
    v = phy_read(pdev,0x1e);
    phy_write(pdev, 0x1e,v|(1<<8));
}
static void ar8035_tx_delay_disable(struct phy_device *pdev)
{
    u32 v;
    phy_write(pdev, 0x1d, 0x05);
    v = phy_read(pdev,0x1e);
    phy_write(pdev, 0x1e,v&(~(1<<8)));
}
static void ar8035_rx_delay_disable(struct phy_device *pdev)
{
    u32 v;
    phy_write(pdev, 0x1d, 0x00);
    v = phy_read(pdev,0x1e);
    phy_write(pdev, 0x1e,v&(~(1<<15)));
}
static void ar8035_rx_delay_enable(struct phy_device *pdev)
{
    u32 v;
    phy_write(pdev, 0x1d, 0x00);
    v = phy_read(pdev,0x1e);
    phy_write(pdev, 0x1e,v|(1<<15)); 
}
static void ar8035_disable_eee_operation(struct phy_device *pdev)
{
    /* 
    Disable EEE operation
      MMD7 �� EEE advertisement register: Device Address = 7; Offset = 0x3C 
      MMD access control register: Offset: 0x0D
      MMD access data register: Offset: 0x0E
    */
    u32 v;
    phy_write(pdev, 0x0d, 0x7);
    phy_write(pdev, 0x0e, 0x3c);
    phy_write(pdev, 0x0d, 0x4007);
    v = phy_read(pdev,0x0e);
    phy_write(pdev,0x0e,v&(~(0x3<<1)));
    printk("%s: EEE operation disabled.\n", __func__);        
}

static int
ar8035_config_init(struct phy_device *pdev)
{
	pdev->autoneg = AUTONEG_ENABLE;
       ar8035_disable_eee_operation(pdev);    
	return 0;
}

int g_speed=-1;

static int
ar8035_read_status(struct phy_device *pdev)
{
    void __iomem *base;
    void __iomem *cfg_base; 
 
    if(0 != genphy_read_status(pdev)) return 0;

    if(g_speed ==pdev->speed) return 0;
    
    base = ioremap(AR71XX_PLL_BASE, AR71XX_PLL_SIZE);
    cfg_base = ioremap(AR71XX_MII_BASE,AR71XX_MII_SIZE); 

    if (pdev->speed == SPEED_1000) {  
        __raw_writel(ETH_CFG_ETH_RXDV_DELAY_SET(3) |ETH_CFG_ETH_RXD_DELAY_SET(3)|
            ETH_CFG_RGMII_GE0_SET(1),
            cfg_base + ATH_ETH_CFG_OFFSET);

        __raw_writel( ETH_XMII_TX_INVERT_SET(1) |ETH_XMII_RX_DELAY_SET(2) |
            ETH_XMII_TX_DELAY_SET(1)|ETH_XMII_GIGE_SET(1),
            base + ETH_XMII_ADDRESS_OFFSET);
        printk("1000M set tx clock delay disable,rx clock delay enable\n");
        ar8035_tx_delay_disable(pdev);
        ar8035_rx_delay_enable(pdev); 
    } else if (pdev->speed == SPEED_100) {
    #if 0
        __raw_writel(ETH_CFG_RGMII_GE0_SET(1),cfg_base + ATH_ETH_CFG_OFFSET);

        __raw_writel( ETH_XMII_TX_INVERT_SET(1) |ETH_SGMII_PHASE0_COUNT_SET(1) |
            ETH_SGMII_PHASE1_COUNT_SET(1),
            base + ETH_XMII_ADDRESS_OFFSET);
    #endif
        __raw_writel(ETH_CFG_ETH_RXDV_DELAY_SET(3) |ETH_CFG_ETH_RXD_DELAY_SET(3)|
            ETH_CFG_RGMII_GE0_SET(1),
            cfg_base + ATH_ETH_CFG_OFFSET);

        __raw_writel( ETH_XMII_TX_INVERT_SET(1) |ETH_XMII_RX_DELAY_SET(1) |
            ETH_XMII_TX_DELAY_SET(1)|ETH_SGMII_PHASE0_COUNT_SET(1) |
            ETH_SGMII_PHASE1_COUNT_SET(1),
            base + ETH_XMII_ADDRESS_OFFSET);

        printk("100M set tx clock delay enable,rx clock delay enable\n");
        ar8035_tx_delay_enable(pdev);
        ar8035_rx_delay_enable(pdev); 
    }else{
       __raw_writel(ETH_CFG_RGMII_GE0_SET(1),cfg_base + ATH_ETH_CFG_OFFSET);

        __raw_writel( ETH_XMII_TX_INVERT_SET(1) |ETH_SGMII_PHASE0_COUNT_SET(19) |
        ETH_SGMII_PHASE1_COUNT_SET(19),
        base + ETH_XMII_ADDRESS_OFFSET);

        printk("10M set tx clock delay enable,rx clock delay enable\n");
        ar8035_tx_delay_enable(pdev);
        ar8035_rx_delay_enable(pdev);  
    }
    iounmap(base);
    iounmap(cfg_base);
    g_speed = pdev->speed;
    return 0;
}

static int
ar8035_config_aneg(struct phy_device *pdev)
{
	u32 v;

	v = phy_read(pdev, MII_BMCR);
	phy_write(pdev, MII_BMCR, v | AR80XX_AUTO_NEGO);

	v = genphy_config_aneg(pdev);
	if (v < 0) {
		printk("%s, Error: 0x%x\n", __func__, v);
		return v;
	}

	return 0;
}

static int
ar8035_probe(struct phy_device *pdev)
{
	return 0;
}

static void
ar8035_remove(struct phy_device *pdev)
{
}

static struct phy_driver ar80xx_phy_drivers[] = {
    {
	.phy_id		= AR80XX_PHY_ID_AR8033,
	.name		= "Qualcomm Atheros AR8033 PHY",
	.phy_id_mask	= AR80XX_PHY_ID_MASK,
	.features	= PHY_GBIT_FEATURES,
	.probe          = ar8033_probe,
	.remove         = ar8033_remove,
	.config_init	= &ar8033_config_init,
	.config_aneg	= &ar8033_config_aneg,
	.read_status    = &ar8033_read_status,
	.driver		= { .owner = THIS_MODULE },
    },
    {
	.phy_id		= AR80XX_PHY_ID_AR8035,
	.name		= "Qualcomm Atheros AR8035 PHY",
	.phy_id_mask	= AR80XX_PHY_ID_MASK,
	.features	= PHY_GBIT_FEATURES,
	.probe          = ar8035_probe,
	.remove         = ar8035_remove,
	.config_init	= &ar8035_config_init,
	.config_aneg	= &ar8035_config_aneg,
	.read_status    = &ar8035_read_status,
	.driver		= { .owner = THIS_MODULE },
    },
};

int __init
ar80xx_phy_init(void)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(ar80xx_phy_drivers); i++) {
		ret = phy_driver_register(&ar80xx_phy_drivers[i]);
		if (ret) {
			while (i-- > 0)
				phy_driver_unregister(&ar80xx_phy_drivers[i]);
			return ret;
		}
	}
	return 0;
}

void __exit
ar80xx_phy_exit(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ar80xx_phy_drivers); i++)
		phy_driver_unregister(&ar80xx_phy_drivers[i]);
}

module_init(ar80xx_phy_init);
module_exit(ar80xx_phy_exit);
MODULE_LICENSE("GPL");

