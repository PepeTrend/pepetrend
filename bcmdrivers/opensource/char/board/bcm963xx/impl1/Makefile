# Makefile for the bcm963xx board drivers
#


obj-y := board.o bcm63xx_flash.o bcm63xx_led.o bcm63xx_gpio.o

ifeq ($(strip $(BRCM_CHIP)),6816)
obj-y += spidevices.o
endif

ifeq ($(strip $(BRCM_CHIP)),63268)
obj-y += spidevices.o
endif


ifneq ($(strip $(CONFIG_BCM_CPLD1)),)
    obj-y += bcmcpld1.o
	EXTRA_CFLAGS += -DCONFIG_BCM_CPLD1
endif

EXTRA_CFLAGS += -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -Werror -Wfatal-errors
EXTRA_CFLAGS += -g

#<< jojopo : [JAZ-TR69-004-3] Event "7 Transfer Complete" not sent after TR-069 firmware download" for the model with NAND flash (TTS #3100) , 2012/08/31
ifeq ($(strip $(BRCM_KERNEL_ROOTFS)),jffs2)
EXTRA_CFLAGS += -DBRCM_NAND_FLASH
endif
#>> jojopo : end
ifneq ($(strip $(CONFIG_BCM_WLAN)),)
	EXTRA_CFLAGS += -DWIRELESS
endif

#//<< [CTFN-SYS-007-1] Charles Wei: add erasing backup PSI data
ifneq ($(strip $(BUILD_BACKUP_PSI)),)
EXTRA_CFLAGS += -DSUPPORT_BACKUP_PSI
endif
#//>> [CTFN-SYS-007-1] End

-include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o *.a *.s

