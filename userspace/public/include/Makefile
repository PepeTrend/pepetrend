#***********************************************************************
#
#  Copyright (c) 2008  Broadcom Corporation
#  All Rights Reserved
#
# 
# 
# This program is free software; you can redistribute it and/or modify 
# it under the terms of the GNU General Public License, version 2, as published by  
# the Free Software Foundation (the "GPL"). 
# 
#
# 
# This program is distributed in the hope that it will be useful,  
# but WITHOUT ANY WARRANTY; without even the implied warranty of  
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
# GNU General Public License for more details. 
#  
# 
#  
#   
# 
# A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by 
# writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
# Boston, MA 02111-1307, USA. 
#
#
#***********************************************************************/

VERSION_FILE=cms_version.h
COLORS_FILE=bcm_colors.h
ASSERT_FILE=bcm_assert.h
SWVERSION_FILE=bcm_swversion.h

all install: $(VERSION_FILE) $(COLORS_FILE) $(ASSERT_FILE) $(SWVERSION_FILE)

clean: force $(VERSION_FILE)

force:
	rm -f $(VERSION_FILE) $(COLORS_FILE) linux/$(COLORS_FILE)
	rm -f $(ASSERT_FILE) linux/$(ASSERT_FILE)
	rm -f $(SWVERSION_FILE)

modsw_clean: generic_modsw_base_clean


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



#
# We only need one rule in this Makefile, which is the one for dynamically
# generating the version.h file.
#
# << [CTFN-SYS-006] Jimmy Wu, for comtrend version, 2008/05/16
#cms_version.h: $(BUILD_DIR)/version.make
cms_version.h: $(BUILD_DIR)/version.make $(TARGETS_DIR)/$(PROFILE)/$(PROFILE)
	@echo "generating $(VERSION_FILE)"
	@echo "/* this is an automatically generated file.  Do not modify. */" > $(VERSION_FILE)
	@echo "/* modify version.make at the top. */" >> $(VERSION_FILE)
	@echo "#ifndef _CMS_VERSION_H_" >> $(VERSION_FILE)
	@echo "#define _CMS_VERSION_H_" >> $(VERSION_FILE)
	@echo -n "#define CMS_RELEASE_VERSION \"" >> $(VERSION_FILE)
	# << [CTFN-SYS-006] Jimmy Wu, for comtrend version, 2008/05/16
ifeq ($(strip $(COMTREND_RELEASE_VERSION)),)
	@echo -n $(BRCM_VERSION) >> $(VERSION_FILE)
	@echo -n "." >> $(VERSION_FILE)
	@echo -n $(BRCM_RELEASE) >> $(VERSION_FILE)
	@echo -n "L." >> $(VERSION_FILE)
	@echo -n $(BRCM_EXTRAVERSION) >> $(VERSION_FILE)
else
	@echo -n $(CTCONFIG_SWHW_VERSION) >> $(VERSION_FILE)
	@echo -n "-" >> $(VERSION_FILE)
	@echo -n $(BRCM_MAINVERSION)$(CTCONFIG_CUSTOMER_ABBREVIATION) >> $(VERSION_FILE)
	@echo -n "-" >> $(VERSION_FILE)
	@echo -n $(CTCONFIG_RELEASE_VERSION) >> $(VERSION_FILE)
endif
	# >> [CTFN-SYS-006] End
	@echo "\"" >> $(VERSION_FILE)
	@echo "#endif" >> $(VERSION_FILE)

bcm_colors.h:
	@echo "Making symlink to $(COLORS_FILE)"
	@ln -s ../../../kernel/linux/include/linux/bcm_colors.h .
	@(cd linux; ln -s ../bcm_colors.h .)

bcm_assert.h:
	@echo "Making symlink to $(ASSERT_FILE)"
	@ln -s ../../../kernel/linux/include/linux/bcm_assert.h .
	@(cd linux; ln -s ../bcm_assert.h .)
	
$(SWVERSION_FILE):
	@echo "Making symlink to $@"
	@ln -s ../../../kernel/linux/include/linux/bcm_swversion.h .
