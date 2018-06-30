#LIBDIR = $(shell pwd)/userspace/gpl/apps/libusb/.libs $(INSTALL_DIR)/lib
LIBDIR = $(INSTALL_DIR)/lib
CFLAGS += -I$(LIBDIR) 
#LDLIBS = $(INSTALL_DIR)/lib/libusb-0.1.so.4
LDLIBS = libusb-0.1.so.4.4.4
EXEC = usb_modeswitch
SRCS = usb_modeswitch.c

$(EXEC): $(SRCS)
#	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)	

#usb_modeswitch:
#	$$(INSTALL_DIR) /bin/$(EXEC)
#	$(ROMFSINST) conf /etc_ro/usb

clean:
	-rm -f $(EXEC) *.o

