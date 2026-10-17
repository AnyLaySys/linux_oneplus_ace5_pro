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

脚本不下载 Ubuntu、不制作 rootfs 或 Android boot 镜像，也不连接或刷写手机。启动镜像的打包和用户空间部署由内核仓库之外的流程负责。

## 设备源码

| 路径 | 内容 |
| --- | --- |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer.dts` | 正式设备树入口、硬件描述与内存保留 |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer-bringup.dtsi` | 当前通过启动验证的外设启用范围 |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer-pmics.dtsi` | 板级 PMIC 配置 |
| `arch/arm64/boot/dts/qcom/sm8750-oneplus-hummer-regulators.dtsi` | 电源约束 |
| `arch/arm64/configs/oneplus_ace5_pro_defconfig` | 设备构建配置 |

内核改动涉及 PSCI、RPMh、SM8750 interconnect、UFS/SCSI 只读保护、ramoops 与启动诊断。上游基线提交为 `5fce161649b4d779d1b76d9fcd52dc77779774b8`。

原 M3/M4/M6 设备树的有效配置已合并为上述文件。整理后的 DTB 与通过 M7-UBUNTU-038 实机测试的 DTB 字节一致。功能配置保持原样，仅将内核版本后缀统一为 `-oneplus-ace5-pro` 并关闭自动 Git 后缀。

## 当前支持范围

源配置已在实机验证 Linux PID1、SCM/TLMM/SMMU/UFS 驱动绑定和 UFS 只读枚举，并用于 Ubuntu RAM 用户空间启动。整理后的重编译产物仍需另行实机验收。

TLMM 仅开放 UFS reset 使用的 GPIO 215。固件内存保留和 RPMh 初始供电请求保留属于当前启动配置。UFS 只读模式通过 `sd_mod.hummer_read_only=1 ufshcd_core.hummer_read_only=1 ufshcd_core.use_mcq_mode=0` 启用；默认编译流程不会加载模块或操作存储设备。

GPT/分区内容比对、重复读取可靠性、其他 GPIO 与持久化 Ubuntu 安装尚未完成验收。保留的完整 defconfig 包含其他平台的驱动选项，编译成功不代表这些功能已在本机可用。

## 设备适配情况

✅ 已适配，⚠️ 部分适配或未验证，❌ 未适配。

| 功能 | 说明 | 状态 |
| --- | --- | :---: |
| CPU |  | ✅ |
| GPU |  | ⚠️ |
| UFS 存储 |  | ✅ |
| 显示输出 |  | ⚠️ |
| 触控 | S3910 | ✅ |
| 侧键 |  | ⚠️ |
| 三段式静音拨键 |  | ⚠️ |
| 指纹识别 |  | ❌ |
| 触觉反馈 |  | ⚠️ |
| 接近传感器 |  | ⚠️ |
| 环境光传感器 |  | ⚠️ |
| 加速度计 |  | ⚠️ |
| 陀螺仪 |  | ⚠️ |
| 磁力计 |  | ⚠️ |
| GNSS |  | ❌ |
| WLAN |  | ✅ |
| 蓝牙 |  | ✅ |
| NFC |  | ❌ |
| USB 主机模式 |  | ⚠️ |
| USB 设备模式 |  | ⚠️ |
| USB PD |  | ⚠️ |
| 电池计量 | 电量显示偏差 | ⚠️ |
| 充电控制 |  | ⚠️ |
| 热管理 |  | ⚠️ |
| 摄像头 |  | ❌ |
| 相机闪光灯 |  | ⚠️ |
| 扬声器输出 |  | ❌ |
| 麦克风输入 |  | ❌ |

## 提交范围

`out/` 已加入 `.gitignore`。提交源码、配置、文档和 `build.py`；编译产物、模块文件和构建日志不进入源码提交。上游授权文件和已有注释保留，新增设备代码不添加解释性注释。
