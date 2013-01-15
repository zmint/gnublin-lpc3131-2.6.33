/*  pwm-module.c
 *
 *  Author:    Michael Schwarz
 *  		   Benedikt Niedermayr (niedermayr@embedded-projects.net)
 *
 * PWM module for LPC313x
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <mach/hardware.h>
#include <mach/gpio.h>
#include <linux/cdev.h>       /* Support for /sys/class */
#include <linux/device.h>

#include "lpc313x_pwm.h"

static int pwm_value = 0;

static int dev_major = 0;
static int dev_open = 0; 

static dev_t driv_number;
static struct cdev *driv_object;
static struct class *driv_class;
static struct device *driv_dev;


static struct file_operations fops = {
 .read = device_read,
 .write = device_write,
 .open = device_open,
 .release = device_release
};



// ------------------------------------ /dev functions ----------------------------------------

static int device_open(struct inode *inode, struct file *file) {
 if(dev_open) return -EBUSY;
 dev_open++;
 try_module_get(THIS_MODULE);
 return 0;
}

static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t * off) {
 char in_buffer[DEVICE_LEN]; 
 
 len = (len > DEVICE_LEN ? DEVICE_LEN : len);
 
 if(copy_from_user(in_buffer, buff, len)) return -EINVAL;

 pwm_value = (in_buffer[0] + (in_buffer[1] << 8));
 PWM_TMR_REG = pwm_value & PWM_MR_MASK;
 
 printk("[lpc313x_pwm] pwm to %d (%d%%)\n", pwm_value, pwm_value * 100 / 4095);

 return len;
}


static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t * offset) {
 if(*offset == 0) {
  *offset += 2;
 } else return 0;
 
 put_user(pwm_value & 0xff, buffer++);
 put_user((pwm_value >> 8) & 0xff, buffer++);
 return 2;
}

static int device_release(struct inode *inode, struct file *file) {
 dev_open--;
 module_put(THIS_MODULE);
 return 0;
}



int __init init_pwm(void) {
 printk("[lpc313x_pwm] pwm frequency: %u Hz\n", cgu_get_clk_freq(CGU_SB_PWM_CLK_ID) / 4096);
 
 /* enable clock for PWM */
 cgu_clk_en_dis(CGU_SB_PWM_PCLK_ID, 1);
 cgu_clk_en_dis(CGU_SB_PWM_PCLK_REGS_ID, 1);
 cgu_clk_en_dis(CGU_SB_PWM_CLK_ID, 1);
 
 /* reset to default */
 PWM_TMR_REG = PWM_TMR_DEFAULT;
 PWM_CNTL_REG = PWM_CNTL_DEFAULT;
 
	/* Driver loading with Sysfs support --BN */
    if(alloc_chrdev_region(&driv_number,0,1,"lpc313x_pwm") < 0 ) return -EIO;
		driv_object = cdev_alloc(); 

	if( driv_object==NULL)
		goto free_device_number;

	driv_object->owner = THIS_MODULE;
	driv_object->ops   = &fops;


	driv_class = class_create(THIS_MODULE, "lpc313x_pwm");
	if( IS_ERR(driv_class) ) {
		pr_err(	"[lpc313x_pwm] no sysfs support\n");
		goto free_cdev;
	}


	 driv_dev = device_create(driv_class, NULL, driv_number, NULL, "%s", "lpc313x_adc");


 	pwm_value = 0; 
 	dev_info(driv_dev, "[lpc313x_pwm] driver loaded");
 return 0;


free_cdev:
	kobject_put(&driv_object->kobj);
free_device_number:
	unregister_chrdev_region(driv_number, 1);
	return -EIO;
}

void __exit cleanup_pwm(void) {
 
 /* disable clock for PWM */
 cgu_clk_en_dis(CGU_SB_PWM_PCLK_ID, 0);
 cgu_clk_en_dis(CGU_SB_PWM_PCLK_REGS_ID, 0);
 cgu_clk_en_dis(CGU_SB_PWM_CLK_ID, 0);
 
 device_release_driver(driv_dev);
 device_destroy(driv_class,driv_number);
 class_destroy(driv_class);
 cdev_del(driv_object);
 unregister_chrdev_region(driv_number,1);

 dev_info(driv_dev, "[lpc313x_pwm] driver unloaded");
}


module_init(init_pwm);
module_exit(cleanup_pwm);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(PWM_AUTHOR);  
MODULE_DESCRIPTION(PWM_DESCRIPTION);    
 
