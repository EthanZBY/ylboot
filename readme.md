# U-Boot for Yourland CodeBook

这个U-Boot代码基于Rockchip的next-dev版本。但是做了很多离线修改，现在已经无法合入上游。仅供幽兰用户学习使用。

## 1. 在幽兰本地构建方法
./build.sh

## 2. 在幽兰上直接测试

更新uboot，直接将当前目录的uboot.img dd到幽兰的NOR闪存即可： 

dd of=/dev/mtdblock0 if=uboot.img bs=512 seek=4096
或者使用目录下的uu.sh

如有任何问题，请在兰舍群提问和讨论！
非幽兰用户，请访问幽兰官网获取更多信息。
https://nanocode.cn/#/yl/index

2026.10.15
