# 内核外部构建模块的过程
```bash
root@uos:/lib/modules/4.19.0-amd64-desktop# ls
build          modules.alias.bin    modules.dep      modules.order    modules.symbols.bin
kernel         modules.builtin      modules.dep.bin  modules.softdep  updates
modules.alias  modules.builtin.bin  modules.devname  modules.symbols

uos@uos:~/Desktop/test$ make -j
make -C /lib/modules/4.19.0-amd64-desktop/build M=/home/uos/Desktop/test
make[1]: 进入目录“/usr/src/linux-headers-4.19.0-amd64-desktop”
  CC [M]  /home/uos/Desktop/test/netlink_test.o
  Building modules, stage 2.
  MODPOST 1 modules
  CC      /home/uos/Desktop/test/netlink_test.mod.o
  LD [M]  /home/uos/Desktop/test/netlink_test.ko
make[1]: 离开目录“/usr/src/linux-headers-4.19.0-amd64-desktop”

uos@uos:~/Desktop/test$ sudo insmod netlink_test.ko
```


# 查询
```bash
uos@uos:~/Desktop/test$ lsmod | grep netlink_test
netlink_test           16384  0

uos@uos:~/Desktop/test$ dmesg
[  687.145420] snd_hda_codec_generic hdaudioC0D0: hda_codec_setup_stream: NID=0x2, stream=0x5, channel=0, format=0x4011
[  693.651139] snd_hda_codec_generic hdaudioC0D0: hda_codec_cleanup_stream: NID=0x2
[ 1413.986962] traps: Typora[12614] trap int3 ip:5642066f0ea4 sp:7ffc000b4120 error:0 in Typora[564204259000+5cc0000]
[ 2435.185940] test_netlink_init

uos@uos:~/Desktop/test$ gcc netlink_user.c -o netlink_user
uos@uos:~/Desktop/test$ ./netlink_user 
send kernel:hello netlink!!
from kernel:hello users!!!
```



# 卸载
```bash
uos@uos:~/Desktop/test$ sudo rmmod netlink_test 
```



# 内部构建

在内核源码drivers/char/目录下新建一个hqh_netlink_test的目录，然后在hqh_netlink_test目录下添加一个Makefile文件，内容如下：

`obj-m += netlink_test.o`

在drivers/char目录下的Makefile文件文件中添加一行：

`obj-m				+= hqh_netlink_test/`

`uos@uos:~/Desktop/git/x86-kernel$ make -j2`

编译成功后，会在hqh_netlink_test目录生成ko文件

```
uos@uos:~/Desktop/git/x86-kernel$ ls drivers/char/hqh_netlink_test/
Makefile       netlink_test.c   netlink_test.mod.c  netlink_test.o
modules.order  netlink_test.ko  netlink_test.mod.o
```

由于没有新增编译开关，故不需要使用Kconfig文件配置．所有构建操作可在<<Linux内核设计与实现第三版>> 机械工业出版社中找到．


# 参考

https://www.cnblogs.com/wenqiang/p/6306727.html

https://www.cnblogs.com/wanqieddy/archive/2011/09/21/2184257.html

http://m.blog.chinaunix.net/uid-29867135-id-4669158.html

https://www.eet-china.com/mp/a50875.html

https://www.cnblogs.com/klb561/p/9048662.html

