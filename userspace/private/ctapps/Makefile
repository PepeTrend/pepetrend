#***********************************************************************
#
#  Copyright (c) 2011  Comtrend Corporation
#  All Rights Reserved
#
#  Author : Jimmy Wu
#
#***********************************************************************/


#
#
# To add an app, add it to APP_DIRS, and write the target rule in the
# last section of this Makefile
#
# [CTFN-SYS-001] Jimmy Wu : for ct_polling, 2008/05/21
#
APP_DIRS = ct_polling

# << [CTFN-3G-001] MHTsai: Support 3G feature, 2010/08/09
# APP_DIRS = ct_polling at_test ct_hotplug_monitor ct_atcpoll ct_3gbackup
ifneq ($(strip $(CTCONFIG_3G_FEATURE)),)
APP_DIRS += at_test ct_hotplug_monitor ct_atcpoll ct_3gbackup
endif
# >> [CTFN-3G-001] End


#
# We need to declare these directory names as phony targets,
# otherwise make will think the directories themselves are
# the targets and will never execute the cmds.
# See GNU Make, 4.6 
#
.PHONY : $(APP_DIRS)
all : $(APP_DIRS)


#
# Cleaning rules
# See example in GNU Make, 4.6
#
clean: 
	for dir in $(APP_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done



#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
# You do not need to modify this part.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common

ifneq ($(strip $(BUILD_VODSL)),)
include $(BUILD_DIR)/make.voice
endif

export BUILD_DIR



##############################################################
# 
# Rules for inidividual apps
# apps can be compiled or not depending on the profile (which
# is modified via make menuconfig)
#
##############################################################

#
# [CTFN-SYS-001] Jimmy Wu : for ct_polling, 2008/05/17
#
ct_polling: sanity_check
	$(MAKE) -C ct_polling

#
# << [CTFN-3G-001] MHTsai: Support 3G feature,  add ct_atcpoll, 2010/08/09
#
ct_atcpoll: sanity_check
	$(MAKE) -C ct_atcpoll

#
# Dylan Shih : for at_test
#
at_test: sanity_check
	$(MAKE) -C at_test

#
# Dylan Shih : for ct_hotplug_mointor
#
ct_hotplug_monitor: sanity_check
	$(MAKE) -C ct_hotplug_monitor clean
	$(MAKE) -C ct_hotplug_monitor;

#
## Dylan Shih : for ct_3gbackup
#
ct_3gbackup: sanity_check
	$(MAKE) -C ct_3gbackup clean
	$(MAKE) -C ct_3gbackup;
# >> [CTFN-3G-001] End