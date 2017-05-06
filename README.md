<!-- README.md --- 
;; 
;; Description: 
;; Author: Hongyi Wu(吴鸿毅)
;; Email: wuhongyi@qq.com 
;; Created: 四 4月 27 23:08:59 2017 (+0800)
;; Last-Updated: Sat May  6 14:00:34 2017 (+0000)
;;           By: Hongyi Wu(吴鸿毅)
;;     Update #: 18
;; URL: http://wuhongyi.cn -->

# README

本程序专为 XIA Pixie-Net 开发。

Author: **Hongyi Wu(吴鸿毅)**
Email: wuhongyi@qq.com 

----

系统默认的是 ununtu 12.04 一些软件比较老，所以做适当的升级。


```shell
apt-get install nfs-common

apt-get upgrade
apt-get install emacs
aot-get install git
apt-get install build-essential
apt-get install gcc g++ make libc6-dev
apt-get install iotop

add-apt-repository ppa:ubuntu-toolchain-r/test
apt-get update
apt-get install gcc-4.9 g++-4.9
ln -sf /usr/bin/g++-4.9 /usr/bin/g++
ln -sf /usr/bin/gcc-4.9 /usr/bin/gcc
apt-get install cmake
```

## 挂载NFS文件共享

NFS服务器的配置相对比较简单，只需要在相应的配置文件中进行设置，然后启动NFS服务器即可。

NFS的常用目录   
- /etc/exports             NFS服务的主要配置文件
- /usr/sbin/exportfs       NFS服务的管理命令
- /usr/sbin/showmount      客户端的查看命令
- /var/lib/nfs/etab        记录NFS分享出来的目录的完整权限设定值
- /var/lib/nfs/xtab        记录曾经登录过的客户端信息

NFS服务的配置文件为 /etc/exports，/etc/exports文件内容格式：
```shell
<输出目录> [客户端1 选项（访问权限,用户映射,其他）] [客户端2 选项（访问权限,用户映射,其他）]
```
输出目录：输出目录是指NFS系统中需要共享给客户机使用的目录；   
客户端：客户端是指网络中可以访问这个NFS输出目录的计算机   
客户端常用的指定方式   
- 指定ip地址的主机：192.168.0.200
- 指定子网中的所有主机：192.168.0.0/24 192.168.0.0/255.255.255.0
- 指定域名的主机：david.bsmart.cn
- 指定域中的所有主机：*.bsmart.cn
- 所有主机：*
选项：选项用来设置输出目录的访问权限、用户映射等。  
NFS主要有3类选项：   
- 访问权限选项
   - 设置输出目录只读：ro
   - 设置输出目录读写：rw
- 用户映射选项
   - all_squash：将远程访问的所有普通用户及所属组都映射为匿名用户或用户组（nfsnobody）；
   - no_all_squash：与all_squash取反（默认设置）；
   - root_squash：将root用户及所属组都映射为匿名用户或用户组（默认设置）；
   - no_root_squash：与rootsquash取反；
   - anonuid=xxx：将远程访问的所有用户都映射为匿名用户，并指定该用户为本地用户（UID=xxx）；
   - anongid=xxx：将远程访问的所有用户组都映射为匿名用户组账户，并指定该匿名用户组账户为本地用户组账户（GID=xxx）；
- 其它选项
   - secure：限制客户端只能从小于1024的tcp/ip端口连接nfs服务器（默认设置）；
   - insecure：允许客户端从大于1024的tcp/ip端口连接服务器；
   - sync：将数据同步写入内存缓冲区与磁盘中，效率低，但可以保证数据的一致性；
   - async：将数据先保存在内存缓冲区中，必要时才写入磁盘；
   - wdelay：检查是否有相关的写操作，如果有则将这些写操作一起执行，这样可以提高效率（默认设置）；
   - no_wdelay：若有写操作则立即执行，应与sync配合使用；
   - subtree：若输出目录是一个子目录，则nfs服务器将检查其父目录的权限(默认设置)；
   - no_subtree：即使输出目录是一个子目录，nfs服务器也不检查其父目录的权限，这样可以提高效率；

```shell
# 示例 将NFS Server的/home/wuhongyi共享给所有主机，权限读写。
/home/wuhongyi (rw,no_root_squash)
```

为了使NFS服务器能正常工作，需要启动rpcbind和nfs两个服务，并且rpcbind一定要先于nfs启动。   
```shell
service rpcbind start
service nfs start
```

查询NFS服务器状态   
```shell
service rpcbind status
service nfs status  
```

停止NFS服务器   
要停止NFS运行时，需要先停止nfs服务再停止rpcbind服务，对于系统中有其他服务(如NIS)需要使用时，不需要停止rpcbind服务
```shell
service nfs stop
service rpcbind stop
```

客户端操作  
```shell
# 挂载到本地/mnt/data文件夹
mount -t nfs -o v3  162.105.147.50:/home/wuhongyi /mnt/data
# 出现了“mount.nfs: access denied by server while mounting...”的错误，则加入参数 -o v3 / -o v2 / -o v4 中的一组试试

# 取消挂载
umount /mnt/data
```


## 挂载USB  
```shell
# 挂载到本地/mnt/data文件夹
mount /dev/sda1 /mnt/data

# 取消挂载
umount /mnt/data
```




## 恢复SD卡原始空间
为了加快镜像装载速度，实际上只格式化了16G左右的SD卡空间，我32G的SD卡还有16G多的空间都没用到，为了能够进行使用进行如下操作
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


## 运行要求

```shell
cd /var/www/PKUXIAPixieNet
# /var/www 目录是网页服务可直接查看的目录。因此将我们的程序包PKUXIAPixieNet放在该目录下

chmod 777 /dev/uio0
#授权，让cgi可直接通过网页交互读取FPGA寄存器上数据
```


## 传输速率

内置micro SD卡一方面容量太小，一方面写入速度也仅有20MB/s左右，USB2.0数据传输在30MB/s左右，因此应该利用其千兆网卡功能（远程电脑也得千兆网卡、配上千兆交换机、千兆网线），通过挂载远程硬盘到本地，实现数据写入远程电脑硬盘，这样估计传输能比以上两种方式大不少。


未完待续。。。。





<!-- README.md ends here -->
