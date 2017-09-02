#!bin/bash
make TARGET_ARCH=i686 -C kernel
cp kernel/k4.kern isodir/
/home/user/opt/grub/bin/os-grub-mkrescue -o k4.iso --product-name="Product Name" isodir
