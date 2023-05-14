# Sylar

## 介绍

[Sylar](https://github.com/sylar-yin/sylar.git) from scratch.

### 编译库
```bash
git clone https://github.com/yinghaoyu/sylar.git
cd sylar
make
make -j
```

### 使用库创建项目
```bash
sh generate.sh ${project-name} ${name-space}
cd ${project-name}
make
make -j
sh move.sh #编译完成后，move可执行文件和动态库
bin/${project-name} -s #执行
```