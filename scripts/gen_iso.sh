#!/bin/sh

set -e

mkdir -p isodir/boot/grub/
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "K4" {
    multiboot2 "/k4.kern"
    boot
}
EOF
cp kernel/k4.kern isodir/
grub-mkrescue --product-name="DURRR" isodir -o k4.iso
