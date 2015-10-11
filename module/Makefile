OUTPUT_FILE = cyclonev_comm
TOOL=arm-linux-gnueabihf-
LINUX_SOURCE=/home/anes/work/cyclone/linux-socfpga

CPUFLAG	=-mcpu=cortex-a9 -march=armv7-a
INCLUDE=-I. -I/include/

obj-m := $(OUTPUT_FILE).o
$(OUTPUT_FILE)-objs := module.o

#CFLAGS    = -O2 -Werror -Wall $(CPUFLAG) $(INCLUDE)

all:
	make -C $(LINUX_SOURCE) ARCH=arm CROSS_COMPILE=$(TOOL) M=$(shell pwd) modules
	$(call echo_success, $(OUTPUT_FILE).ko, $(shell pwd))

clean: 
	make -C $(LINUX_SOURCE) ARCH=arm CROSS_COMPILE=$(TOOL) M=$(shell pwd) clean

.PHONY: defines all clean
