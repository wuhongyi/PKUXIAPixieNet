<!-- README.md --- 
;; 
;; Description: 
;; Author: Hongyi Wu(吴鸿毅)
;; Email: wuhongyi@qq.com 
;; Created: 四 4月 27 23:08:59 2017 (+0800)
;; Last-Updated: 五 4月 28 15:30:52 2017 (+0800)
;;           By: Hongyi Wu(吴鸿毅)
;;     Update #: 10
;; URL: http://wuhongyi.cn -->

# README

本程序专为 XIA Pixie-Net 开发。

Author: **Hongyi Wu(吴鸿毅)**
Email: wuhongyi@qq.com 

----

系统默认的是 ununtu 12.04 一些软件比较老，所以做适当的升级。


```shell
apt-get upgrade
apt-get install emacs
aot-get install git
apt-get install build-essential
apt-get install gcc g++ make libc6-dev

add-apt-repository ppa:ubuntu-toolchain-r/test
apt-get update
apt-get install gcc-4.9 g++-4.9
ln -sf /usr/bin/g++-4.9 /usr/bin/g++
ln -sf /usr/bin/gcc-4.9 /usr/bin/gcc
apt-get install cmake
```


## 恢复SD卡原始空间
为了加快镜像装载速速，实际上只格式化了2G左右的SD卡空间，我16G的SD卡还有13G多的空间都没用到，为了能够进行使用进行如下操作
```shell
fdisk /dev/mmcblk0
```
然后分别输入:  d [ENTER],2 [ENTER],n[ENTER]
[ENTER],[ENTER],[ENTER],[ENTER],w[ENTER]，
若中间出现问题详细参考Getting started with Xillinux for Zynq-7000 EPP ，
然后重启linux
开机后
```shell
resize2fs /dev/mmcblk0p2
```
并使用
```shell
df -h
```
查看最后追加的结果



未完待续。。。。





<!-- README.md ends here -->
