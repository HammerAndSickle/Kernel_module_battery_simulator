obj-m := batt_module.o

KERNELDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

All: test_dd test_app_ui test_app_manager

test_dd: batt_module.c
	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) modules

test_app_ui : battery_UI.c
	gcc -o battui battery_UI.c

test_app_manager : power_manager.c
	gcc -o battma power_manager.c
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm -rf gyapp2
