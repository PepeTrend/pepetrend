#
# Makefile for USB Core files and filesystem
#

usbcore-objs	:= usb.o hub.o hcd.o urb.o message.o driver.o \
			config.o file.o buffer.o sysfs.o endpoint.o \
			devio.o notify.o generic.o quirks.o
# << [JAZ-026] Joba Yang : Support USB LED per Jazztel's specific  
EXTRA_CFLAGS += -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD) -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)
# >> [JAZ-026] End

ifeq ($(CONFIG_PCI),y)
	usbcore-objs	+= hcd-pci.o
endif

ifeq ($(CONFIG_USB_DEVICEFS),y)
	usbcore-objs	+= inode.o devices.o
endif

obj-$(CONFIG_USB)	+= usbcore.o

ifeq ($(CONFIG_USB_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
