DRV_DIR=../atmega328p_drv

CPPFLAGS += -I..
CPPFLAGS += -I$(DRV_DIR)

include $(DRV_DIR)/Makefile.defs

TARGET = nRF24L01_test
CSRCS = \
		$(DRV_DIR)/drv/spi0.c \
		nRF24L01.c \
		nRF24L01_test.c

ifdef RELEASE
	CFLAGS +=  \
		-DASSERT_DISABLE
endif

include $(DRV_DIR)/Makefile.rules

clean:
	cd $(DRV_DIR) && make clean
	rm *.bin *.elf *.hex *.lst *.map *.o *.su *.stack_usage -f
