# Makefile file for Email

EXE = email

OBJS=email.o

LIBS= -lcms_msg $(CMS_COMMON_LIBS)

all dynamic: sanity_check $(EXE) generic_exe_install

# Remove symlinks so customers do not import them into their source control systems
clean: generic_clean
	rm -f $(INSTALL_DIR)/bin/$(EXE)

#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common



#
# GPL apps and libs are only allowed to include header files from the
# gpl and public directories
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_INCLUDE_PATHS := -I.\
                         -I$(BUILD_DIR)/userspace/gpl/include  \
                         -I$(BUILD_DIR)/userspace/gpl/include/$(OALDIR) \
                         -I$(BUILD_DIR)/userspace/public/include  \
                         -I$(BUILD_DIR)/userspace/public/include/$(OALDIR)


#
# GPL apps and libs are only allowed to link with libraries from the
# gpl and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_LIB_DIRS := /lib:/lib/gpl:/lib/public
CFLAGS += -Os -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)  \
          -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) 

# Joba Yang add ssl
LIBS += -lssl -lcrypto
CUSTOM_CFLAGS += -I$(BUILD_DIR)/userspace/public/libs/openssl/include
# Joba Yang

#
# Implicit rule will make the .c into a .o
# Implicit rule is $(CC) -c $(CPPFLAGS) $(CFLAGS)
# See Section 10.2 of Gnu Make manual
# 
$(EXE): $(OBJS)
	$(CC) -o $@ $(OBJS) -Wl,-rpath,$(CMS_LIB_RPATH) $(CMS_LIB_PATH) $(LIBS)
