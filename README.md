
pwmpi : basic pwm driver for the raspberry pi
=============================================

For a better and up-to-date driver visit https://github.com/btanghe/rpi-PWM
---------------------------------------------------------------------------


          
This driver lets you set and control pwm on gpio18.

How to use :
------------

After loading the module, you must first export the chip to be able to use it :
```
  sudo insmod pwmpi.ko

  cd /sys/class/pwmchip0
  echo 0 | sudo tee export
```
Now you can cd to the chip and control it :
```
  cd pwm0
  echo 1000 | sudo tee period     //set the period to 1000 ns
  echo 500 | sudo tee duty_cycle  //set the duty cycle to 500 ns (50%)
  echo 1 | sudo tee enable        //enable pwm (set to 0 to disable)
```
Before removing the module, be sure to unexport the chip to avoid unwanted errors :
```
  cd ..                           //back to /sys/class/pwmchip0
  echo unexport 0 | sudo tee unexport
  sudo rmmod pwmpi
``` 
You can always check the kernel logs with 'dmesg'.

Compiling the raspberry pi kernel and modules
---------------------------------------------

Through the following lines will be explained how to cross compile the raspberry pi kernel and modules.

This guide assumes you're running a debian-based linux distribution and Raspbian on the Pi.
If not simply download the packages for your favorite distro.

First install these packages :
```
  sudo apt-get install build-essentials git-core libncurses5-dev gcc-4.8-arm-linux-gnueabihf
```  
Create a symlink for the cross compiler :
```
  sudo ln -s /usr/bin/arm-linux-gnueabihf-gcc-4.8 /usr/bin/arm-linux-gnueabihf-gcc
``` 
Make a directory for the source and tools, then clone them with git :
```
  mkdir pi
  cd pi
  git clone -b rpi-3.13.y --single-branch https://github.com/raspberrypi/linux
  git clone https://github.com/raspberrypi/tools.git
  git clone https://github.com/indriApollo/pwmpi.git
  cd linux
```  
note : the driver needs kernel >= 3.11.x to work.  
Generate the .config file from the pre-packaged raspberry pi one :
```
  make ARCH=arm CROSS_COMPILE=/usr/bin/arm-linux-gnueabihf- bcmrpi_cutdown_defconfig
```  
Run menuconfig to add the pwm subsystem with sysfs support in your kernel :
```
  make ARCH=arm CROSS_COMPILE=/usr/bin/arm-linux-gnueabihf- menuconfig
```  
Navigate now to 'Device Drivers' and look at the bottom for 'Pulse-Width Modulation (PWM) Support'.
Select it with space, save and exit.
You have to specify the pwm device in bcm2708.c to allow the driver to control it :
```
  gedit arch/arm/mach-bcm2708/bcm2708.c
```  
note : you can replace 'gedit' with wathever text-editor you use.  
Now add these lines under 'static struct resource bcm2708_spi_resources[] = { some code };' :
```
  static struct platform_device bcm2708_pwm = {
    .name = "bcm2708_pwm",
  };
```
Also add this line under 'void __init bcm2708_init(void){' :
```
  bcm_register_device(&bcm2708_pwm);
```  
Save and exit gedit.
Now you can start compiling the kernel.
You can speed up the compilation process by enabling parallel make with the ­j flag. The
recommended use is ‘processor cores + 1′, e.g. 5 if you have a quad core processor :
```
  make ARCH=arm CROSS_COMPILE=/usr/bin/arm-linux-gnueabihf- -k -j5
``` 
Assuming the compilation was sucessful, create a directory for the modules :
```
  mkdir ../modules
```  
Then compile and ‘install’ the loadable modules to the temp directory :
```  
  make modules_install ARCH=arm CROSS_COMPILE=/usr/bin/arm-linux-gnueabihf- INSTALL_MOD_PATH=../modules/
```  
Now we need to use imagetool-­uncompressed.py from the tools repo to get the kernel ready for the Pi :
```
  cd ../tools/mkimage/
  ./imagetool-uncompressed.py ../../linux/arch/arm/boot/Image
```  
This creates a kernel.img in the current directory. Plug in the SD card of the existing Raspbian image that you
wish to install the new kernel on. Delete the existing kernel.img and replace it with the new one,
substituting “boot­-partition­uuid” with the identifier of the partion as it is mounted in Ubuntu.
```
  sudo rm /media/boot­-partition­uuid/kernel.img
  sudo mv kernel.img /media/boot­-partition­uuid/
```
Next, remove the existing /lib/modules and lib/firmware directories, substituting “rootfs­partition­uuid” with
the identifier of the root filesystem partion mounted in Ubuntu.
```
  sudo rm ­-rf /media/rootfs-­partition­uuid/lib/modules/
  sudo rm ­-rf /media/rootfs-­partition­uuid/lib/firmware/
```
Go to the destination directory of the previous make modules_install, and copy the new modules and
firmware in their place:
```
  cd ../../modules/
  sudo cp ­-a lib/modules/ /media/rootfs-­partition­uuid/lib/
  sudo cp ­-a lib/firmware/ /media/rootfs-­partition­uuid/lib/
```  
Get now to the pwmpi directory and make the module :
```  
  cd ../pwmpi
  make
```  
If no errors showed up copy pwmpi.ko to the pi home directory :
```
  sudo cp pwmpi.ko /media/rootfs-partitionuuid/home/pi
  sync
```
note : be sure to sync all buffers content to the SD card.
That’s it! Exject the SD card, and boot the new kernel on the raspberry pi!

Tutorial based on http://mitchtech.net/raspberry-pi-kernel-compile/ and updated.

Many thanks to Bart Tanghe and Bjorn van Tilt for their precious collaboration !

---------------
-------
---
-
