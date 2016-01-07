/*
 * pwmpi.c
 *
 * Pwm driver for the raspberry pi
 *
 * Copyright 2014 Raphael Leroux (indri.apollo@gmail.com)
 *
 * Based on pwmcdev.c by Thomas More Campus De Nayer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define REG_PWM_ENABLE 0x2020C000					//register address for enabling the pwm
#define REG_PWM_DUTY 0x2020C014						//register address for setting duty in ns
#define REG_PWM_PERIOD 0x2020C010					//register address for setting period in ns
#define PWMCLK_CNTL 0x201010A0						//register address for enabling/disabling the clock
#define VAL_PWMCLK_CNTL_OFF 0x5A000000 | (1 << 5)	//value to write in the register to disable clock
#define VAL_PWMCLK_CNTL_ON 0x5A000000 | 0x11 		//value to write in register to enable clock
#define PWMCLK_DIV 0x201010A4						//
#define VAL_PWM_ENABLE 0x00000001					//value to write in register to enable the pwm
#define VAL_PWM_DISABLE 0xfffffffe					//value to write in register to disable the pwm
#define VAL_PWMCLK_DIV 0x5A000000 | (19 << 12)		//Value to set the clock to ~ 1Mhz
#define FUNC_SLCT_HEAT_PWM 0x20200004				//register address for setting i/o function
#define MASK_FUNC_SLCT_HEAT_PWM 0x03F00000			//Mask for bits in FUNC_SLCT_HEAT
#define VAL_FUNC_SLCT_HEAT_PWM 0x02200000			//Value to set GPIO17 as output and GPIO18 as PWM0
#define MASK_CTL_PWM 0x000000FF            			//
#define VAL_CTL_PWM 0x00000081            			//Value to enable i/o function as pwm

volatile unsigned long *ptrPWMCLK_CNTL;
volatile unsigned long *ptrPWMCLK_DIV;
volatile unsigned long *ptrREG_PWM_ENABLE;
volatile unsigned long *ptrREG_PWM_DUTY;
volatile unsigned long *ptrREG_PWM_PERIOD;
volatile unsigned long *ptrFUNC_SLCT_HEAT_PWM;

struct pi_pwm_chip {
	struct pwm_chip chip;
	struct device *dev;
};

static inline struct pi_pwm_chip *to_pi_pwm_chip(struct pwm_chip *chip){
	return container_of(chip, struct pi_pwm_chip, chip);
}

static int pi_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm, int duty_ns, int period_ns){
	//printk(KERN_INFO "[pwmpi] config");
	struct pi_pwm_chip *pc = to_pi_pwm_chip(chip);
	u32 period32=period_ns;
	u32 duty32=duty_ns;
	mdelay(1);
	iowrite32(duty32, ptrREG_PWM_DUTY);
	mdelay(1);
	iowrite32(period32, ptrREG_PWM_PERIOD);
	printk(KERN_INFO "[pwmpi] duty:%d period:%d\n",ioread32(ptrREG_PWM_DUTY),ioread32(ptrREG_PWM_PERIOD));
	return 0;
}

static int pi_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm){
	struct pi_pwm_chip *pc = to_pi_pwm_chip(chip);
	printk(KERN_INFO "[pwmpi] pwm enabled\n");
	mdelay(1);
	iowrite32(ioread32(ptrREG_PWM_ENABLE) | VAL_PWM_ENABLE, ptrREG_PWM_ENABLE);
	return 0;
}

static void pi_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm){
	struct pi_pwm_chip *pc = to_pi_pwm_chip(chip);
	printk(KERN_INFO "[pwmpi] pwm disabled\n");
	mdelay(1);
	iowrite32(ioread32(ptrREG_PWM_ENABLE) & VAL_PWM_DISABLE, ptrREG_PWM_ENABLE);
}

static const struct pwm_ops pi_pwm_ops = {
	.config = pi_pwm_config,
	.enable = pi_pwm_enable,
	.disable = pi_pwm_disable,
	.owner = THIS_MODULE,
};

static int pi_pwm_probe(struct platform_device *pdev){
	struct pi_pwm_chip *pwm;
	int ret;
	//printk(KERN_INFO "[pwmpi] kzalloc ...");
	pwm = devm_kzalloc(&pdev->dev, sizeof(*pwm), GFP_KERNEL);
	if (!pwm) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	pwm->dev = &pdev->dev;
	platform_set_drvdata(pdev, pwm);

	pwm->chip.dev = &pdev->dev;
	pwm->chip.ops = &pi_pwm_ops;
	pwm->chip.npwm = 1;			//specify nr of pwm
	//printk(KERN_INFO "[pwmpi] add ...");
	ret = pwmchip_add(&pwm->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		return ret;
	}

    printk(KERN_INFO "[pwmpi] IOREMAP\n"); 

	ptrREG_PWM_ENABLE = ioremap_nocache(REG_PWM_ENABLE,4);
	if(ptrREG_PWM_ENABLE == NULL){
		printk(KERN_INFO "[pwmpi] ioremap REG_PWM_ENABLE failed\n");
	}
	ptrREG_PWM_DUTY = ioremap_nocache(REG_PWM_DUTY,4);
	if(ptrREG_PWM_DUTY == NULL){
		printk(KERN_INFO "[pwmpi] ioremap REG_PWM_DUTY failed\n");
	}
	
	ptrREG_PWM_PERIOD =(unsigned long)ioremap_nocache(REG_PWM_PERIOD,4);
	if(ptrREG_PWM_PERIOD == NULL){
		printk(KERN_INFO "[pwmpi] ioremap REG_PWM_PERIOD\n");
	}
	
	ptrPWMCLK_CNTL =(unsigned long)ioremap_nocache(PWMCLK_CNTL,4);
	if(ptrPWMCLK_CNTL == NULL){
		printk(KERN_INFO "[pwmpi] ioremap PWMCLK_CNTL failed\n");
	}

	ptrPWMCLK_DIV =(unsigned long)ioremap_nocache(PWMCLK_DIV,4);
	if(ptrPWMCLK_DIV == NULL){
		printk(KERN_INFO "[pwmpi] ioremap PWMCLK_DIV failed\n");
	}

	ptrFUNC_SLCT_HEAT_PWM = ioremap_nocache(FUNC_SLCT_HEAT_PWM,4);
	if(ptrFUNC_SLCT_HEAT_PWM == NULL){
		printk(KERN_INFO "[pwmpi] ioremap FUNC_SLCT_HEAT_PWM failed\n"); 
	}

	printk(KERN_INFO "[pwmpi] IOREMAP done");
	mdelay(1);
	iowrite32((ioread32(ptrFUNC_SLCT_HEAT_PWM) & ~MASK_FUNC_SLCT_HEAT_PWM) | VAL_FUNC_SLCT_HEAT_PWM, ptrFUNC_SLCT_HEAT_PWM);	//Set function to i/o
	mdelay(1);
	iowrite32(VAL_PWMCLK_CNTL_OFF, ptrPWMCLK_CNTL);	//First disable clock
	mdelay(1);
	iowrite32(VAL_PWMCLK_DIV, ptrPWMCLK_DIV);		//clock set to 1Mhz.
	mdelay(1);
	iowrite32(VAL_PWMCLK_CNTL_ON, ptrPWMCLK_CNTL);	//Restart the clock
	mdelay(1);
	iowrite32(ioread32(ptrREG_PWM_ENABLE) & ~MASK_CTL_PWM, ptrREG_PWM_ENABLE);
    mdelay(1);
    iowrite32(ioread32(ptrREG_PWM_ENABLE) | VAL_CTL_PWM, ptrREG_PWM_ENABLE);    //Enable pwm
	printk(KERN_INFO "[pwmpi] clock set");
	//printk("alt:%x",ioread32(ptrFUNC_SLCT_HEAT_PWM));

	return 0;
}

static int pi_pwm_remove(struct platform_device *pdev){
	struct pi_pwm_chip *pc = platform_get_drvdata(pdev);
	if (WARN_ON(!pc))
		return -ENODEV;
	printk(KERN_INFO "[pwmpi] pwm removed");
	return pwmchip_remove(&pc->chip);
}


static struct platform_driver pi_pwm_driver = {
	.driver = {
		.name = "bcm2708_pwm",		//driver name set in bcm2708.c
		.owner = THIS_MODULE,
	},
	.probe = pi_pwm_probe,
	.remove = pi_pwm_remove,
};

module_platform_driver(pi_pwm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raphael Leroux");

