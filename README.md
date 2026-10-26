# Linux for OnePlus Ace 5 Pro

OnePlus Ace 5 Pro / PKR110 (`hummer`, Qualcomm SM8750) 的 Linux 内核适配源码，基于 Linux v7.2.9。仓库保留完整上游源码树。

## 编译

在 Linux 主机上安装构建依赖。Ubuntu/Debian 示例：

```bash
sudo apt install build-essential clang llvm lld bc bison flex libssl-dev libelf-dev python3
```

构建脚本以 `LLVM=1 LLVM_IAS=1` 调用上游 Makefile，需要 PATH 中可用的 `clang`、`ld.lld` 及 `llvm-*` 工具链。已使用 Ubuntu Clang/LLD 22.1.8 验证。脚本本身使用 `Path.is_relative_to`，需要 Python 3.9 或更新版本。

仓库根目录只有一个构建入口：

```bash
python3 build.py
```

默认通过 `O=out/kernel` 分离源码和产物，编译内核、hummer DTB，以及配置中启用的可加载模块：

```text
out/kernel/arch/arm64/boot/Image
out/kernel/arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer.dtb
out/kernel/modules.order
out/kernel/Module.symvers
out/kernel/.config
```

`modules.order` 是本次构建的模块清单。`.ko` 文件按上游构建系统的目录结构保留在输出树内，顶层分布为 `drivers/`（461）、`net/`（51）、`crypto/`（21）、`lib/`（9）、`fs/`（5）、`sound/`（3）、`arch/`（2）。

```bash
python3 build.py -j 8
python3 build.py --out out/debug
```

`--out` 必须位于本仓库 `out/` 目录内。脚本先执行 `olddefconfig`，因此**已有输出目录的 `.config` 会被保留**；脚本不会应用设备 defconfig。全新输出目录下 `olddefconfig` 会回落到上游通用 `arch/arm64/configs/defconfig`，得到的并不是本设备配置。要从未配置状态开始时，需要先手动应用设备配置：

```bash
make O=out/kernel ARCH=arm64 LLVM=1 LLVM_IAS=1 LOCALVERSION= oneplus_ace5_pro_defconfig
python3 build.py
```

默认固定构建用户 `hummer`、主机 `builder`、时间戳 `2026-10-09 00:00:00 +0000` 和构建序号 `1`（均为 `setdefault`，已有同名环境变量时不覆盖，需要修改时设置相应的 `KBUILD_BUILD_*`）。

脚本不下载软件包、不制作 rootfs 或 Android boot 镜像，也不连接或刷写手机。

## 设备源码

| 路径 | 内容 |
| --- | --- |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer.dts` | 正式设备树入口、硬件描述与内存保留 |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer-bringup.dtsi` | 当前通过启动验证的外设启用范围 |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer-display.dtsi` | 面板与显示链路 |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer-pmics.dtsi` | 板级 PMIC 配置 |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer-regulators.dtsi` | 电源约束 |
| `arch/arm64/boot/dts/qcom/oplus_chg/` | 充电相关设备树片段 |
| `arch/arm64/configs/oneplus_ace5_pro_defconfig` | 设备构建配置 |

相对压缩后的历史起点共有 284 个新增文件、64 个修改文件，主要集中在设备树、OPLUS 充电与电量计、ath12k、MSM 显示与面板、S3910 触控。

内核改动涉及 PSCI 复位路径、RPMh、SM8750 interconnect、pmic_glink、ath12k WCN7861 上电与 QMI、MSM 显示/DSI/面板、OPLUS 充电与电量计、ramoops 持久化与启动诊断。上游基线提交为 `5fce161649b4d779d1b76d9fcd52dc77779774b8`（该提交不在本仓库历史中，仓库历史经过压缩，无法在此校验）。

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
