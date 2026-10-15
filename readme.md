更新uboot，直接将当前目录的uboot.img 传过去执行 

dd of=/dev/mtdblock0 if=uboot.img bs=512 seek=4096
