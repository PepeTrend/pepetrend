
all: lib install

lib:
	cp src/libbcmssl_$(PROFILE).so src/libbcmssl.so
	cp ctaocrypt/libbcmcrypto_$(PROFILE).so ctaocrypt/libbcmcrypto.so

install: lib
	install -m 755 src/libbcmssl.so $(INSTALL_DIR)/lib/private
	install -m 755 ctaocrypt/libbcmcrypto.so $(INSTALL_DIR)/lib/private
	$(STRIP) $(INSTALL_DIR)/lib/private/libbcmssl.so
	$(STRIP) $(INSTALL_DIR)/lib/private/libbcmcrypto.so

clean:
	-rm src/libbcmssl.so ctaocrypt/libbcmcrypto.so $(INSTALL_DIR)/lib/private/libbcmssl.so $(INSTALL_DIR)/lib/private/libbcmcrypto.so


