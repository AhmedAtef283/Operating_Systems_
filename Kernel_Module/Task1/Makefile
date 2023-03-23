obj-m += task1.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

test:
	sudo insmod task1.ko
	sleep 3s
	cat /proc/jiffies
	sudo rmmod task1.ko