EXE = ct_polling
dynamic: generic_exe_install

clean:
	rm -f ct_polling


ct_polling:
	cp ct_polling_$(PROFILE)_save ct_polling

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
