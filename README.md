# Linux for OnePlus Ace 5 Pro

OnePlus Ace 5 Pro / PKR110 (`hummer`, Qualcomm SM8750) 的 Linux 内核适配源码，基于 Linux v7.2.9。仓库保留完整上游源码树。

## 编译

在 Linux 主机上安装构建依赖。Ubuntu/Debian 示例：

```bash
sudo apt install build-essential clang llvm lld bc bison flex libssl-dev libelf-dev python3
```

需要 Python 3.9 或更新版本。已使用 Ubuntu Clang/LLD 22.1.8 验证。仓库根目录只有一个新增构建入口：

```bash
python3 build.py
```

脚本调用上游 Makefile，使用 `ARCH=arm64 LLVM=1`，默认通过 `O=out/kernel` 分离源码和产物。编译内核、hummer DTB，以及配置中启用的可加载模块：

```text
out/kernel/arch/arm64/boot/Image
out/kernel/arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer.dtb
out/kernel/drivers/…/*.ko
out/kernel/net/…/*.ko
out/kernel/modules.order
out/kernel/Module.symvers
out/kernel/.config
```

`modules.order` 是本次构建的模块清单。`.ko` 文件保留在上游构建系统对应的子目录中。

```bash
python3 build.py -j 8
python3 build.py --configure
python3 build.py --out out/debug
```

已有输出配置默认保留；`--configure` 会恢复设备 defconfig。`--out` 可选择本仓库 `out/` 下的输出目录。默认固定构建用户、主机、时间和构建序号；需要修改时可设置相应的 `KBUILD_BUILD_*` 环境变量。
## 设备源码

| 路径 | 内容 |
| --- | --- |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer.dts` | 正式设备树入口、硬件描述与内存保留 |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer-bringup.dtsi` | 当前通过启动验证的外设启用范围 |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer-pmics.dtsi` | 板级 PMIC 配置 |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer-regulators.dtsi` | 电源约束 |
| `arch/arm64/configs/oneplus_ace5_pro_defconfig` | 设备构建配置 |

内核改动涉及 PSCI、RPMh、SM8750 interconnect、UFS/SCSI 只读保护、ramoops 与启动诊断。上游基线提交为 `5fce161649b4d779d1b76d9fcd52dc77779774b8`。

## 设备适配情况
| 功能 | 状态 |
| :--- | :---: |
| CPU | ✅ |
| GPU | ✅ |
| UFS | ✅ |
| 触控 | ✅ |
| WLAN | ✅ |
| 蓝牙 | ✅ |
| USB | ✅ |
| 电池 | ✅ |
| 摄像头 | ❌ |
| 闪光灯 | ✅ |
| 扬声器 | ✅ |
| 麦克风 | ✅ |