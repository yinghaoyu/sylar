# Sylar

## 介绍

[Sylar](https://github.com/sylar-yin/sylar.git) 是一个 C++ 高性能分布式服务器框架。

## 编译环境

原项目的编译环境在 CentOS 7.6 下，现在编译环境换成了 Ubuntu 20.04，需要的依赖包有：

```bash
sudo apt update

sudo apt install mysql-server libmysqlclient-dev libboost-all-dev libssl-dev zlib1g-dev libsqlite3-dev libtinyxml2-dev protobuf-compiler libprotobuf-dev libtbb-dev librdkafka-dev

git clone https://github.com/vipshop/hiredis-vip.git
cd hiredis-vip
make
sudo make install

// 下载最新的jsoncpp，为了支持 int64_t
git clone https://github.com/open-source-parsers/jsoncpp
mkdir build
cmake ..
make
sudo make install
```

## 编译库
```bash
git clone https://github.com/yinghaoyu/sylar.git
cd sylar
make
make -j
```

## 使用库创建项目
```bash
sh generate.sh ${project-name} ${name-space}
cd ${project-name}
make
make -j
sh move.sh #编译完成后，move可执行文件和动态库
bin/${project-name} -s #执行
```
