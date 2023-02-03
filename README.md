# 说明

This is a demo for netlink.

# 使用方式

gcc netlink_test.c -o netlink_test

# 运行

关闭无线网卡wlan0

sudo ./netlink_test wlan0 0

# 参考文档

* [通过netlink协议控制或查询网卡信息
](https://github.com/dev-nono/nettool/blob/3cf0d750ef15c1d1289dd3c7e65fdf3ffaa0e2fe/libnettool/src/ip_link.c)

* [linux netlink通信机制
](https://www.cnblogs.com/wenqiang/p/6306727.html)

* [netlink报文格式介绍
](https://www.infradead.org/~tgr/libnl/doc/route.html)

https://www.linuxjournal.com/article/7356
