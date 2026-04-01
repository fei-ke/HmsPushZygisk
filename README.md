# HMSPush Zygisk

一个用于为应用伪装华为设备的 Magisk/Zygisk 模块，以便于使用 [HMSPush](https://github.com/fei-ke/HmsPushZygisk.git)

### 下载

前往 [Release](https://github.com/fei-ke/HmsPushZygisk/releases) 下载

### 配置

手动编辑 `/data/adb/hmspush/app.conf` 文件（文件不存在请手动创建），或者使用 [HMSPush](https://github.com/fei-ke/HmsPush) 应用进行配置

配置示例：

```
#对该应用所有进程进行伪装
com.example.app

#对该应用指定进程进行伪装，格式：包名|进程名
com.example.app|com.example.app:push

#对该应用所有进程进行伪装，但跳过 Build 属性伪装
!com.example.app

#对该应用指定进程进行伪装，但跳过 Build 属性伪装，格式：!包名|进程名
!com.example.app|com.example.app:push
```

### 开发、构建

#### Requirement

- [NDK](https://developer.android.com/ndk/downloads) and setup `ANDROID_NDK_HOME` environment variable
- [Cargo](https://rust-lang.org/)
- [Bun](https://bun.sh/) for WebUI

在项目根目录执行，构建产物在 `build` 目录下

```shell
./build.sh
```

### 致谢

- [zygisk-api-rs](https://github.com/rmnscnce/zygisk-api-rs)

### License

[GNU General Public License v3 (GPL-3)](http://www.gnu.org/copyleft/gpl.html).
