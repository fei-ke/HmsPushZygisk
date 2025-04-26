# HMSPush Zygisk

一个用于为应用伪装华为设备的 Magisk/Zygisk 模块，以便于使用 [HMSPush](https://github.com/fei-ke/HmsPushZygisk.git)

### 下载
前往 [Release](https://github.com/fei-ke/HmsPushZygisk/releases) 下载

### 配置
手动编辑 `/data/misc/hmspush/app.conf` 文件，或者使用 [HMSPush](https://github.com/fei-ke/HmsPushZygisk.git) 应用进行配置

配置示例：
```
#对该应用所有进程进行伪装
com.example.app

#对该应用指定进程进行伪装，格式：包名|进程名
com.example.app|com.example.app:push
```


### 构建
配置 ndk 路径后在项目根目录执行，构建产物在 `build` 目录下
```shell
./build.sh
```

### License
[GNU General Public License v3 (GPL-3)](http://www.gnu.org/copyleft/gpl.html).
