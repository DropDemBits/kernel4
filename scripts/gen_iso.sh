#!/bin/sh

set -e

if [ $# -ne 1 ]; then
    echo "Error: No architechture specified"
    exit 1
fi

echo "Creating ISO for target $1"
mkdir -p isodir/boot/grub/
cat > isodir/boot/grub/grub.cfg << EOF
set timeout=0
menuentry "K4 (Multiboot)" {
    multiboot "/k4-$1.kern"
    module "/initrd.tar" "initrd.tar"
    boot
}
menuentry "K4 (Multiboot2)" {
    multiboot2 "/k4-$1.kern"
    module2 "/initrd.tar" "initrd.tar"
    boot
}
EOF

rm -f isodir/k4-*.kern
cp build/$1/k4-$1.kern isodir/
cp initrd/initrd/initrd.tar isodir/

rm -f k4-$1.iso
grub-mkrescue --product-name="DURRR" isodir -o k4-$1.iso
