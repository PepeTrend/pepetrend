
ifeq ($(CONFIG_MIPS_BRCM),y)

# Linux
obj-y		+= boardparms.o

ifneq ($(strip $(VOXXXLOAD)),)
obj-y		+= boardparms_voice.o
endif

EXTRA_CFLAGS += -DCONFIG_BCM9$(BRCM_CHIP) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)
# << [CTFN-SYS-021] MHTsai: Separate AFE ID with AnnexA and AnnexB.
ifeq "$(ADSL)" "ANNEX_B"
EXTRA_CFLAGS += -DADSL_ANNEXB
endif
# >> [CTFN-SYS-021] End
-include $(TOPDIR)/Rules.make

endif


