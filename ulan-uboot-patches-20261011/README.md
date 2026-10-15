# 幽兰 RK3588 U-Boot B3 源码补丁

本包将原始 `uboot_youxi_202508_22.tar.gz` 中的 U-Boot 源码修改为本次上机验证的 B3 源码。三份补丁共修改 12 个文件，新增 153 行、删除 22 行。应用及撤销均已在从原压缩包提取的文件上验证。

**修复白色噪点的代码在 `0003-edp-assr.patch`。** 它在链路训练前同步屏幕与 RK3588 发送器的 ASSR 设置。`0001` 和 `0002` 保留本次构建及启动时实际需要的适配；完整复现本机 B3 时一并应用。仅将花屏修复集成到已有可用分支时，可先检查并应用 `0003`。

本包是 2026-10-11 实测 B3 修改的源码导出。补丁对应此前交付的 5 MiB B3 FIT 镜像，SHA-256 为 `506be66f55a7f96ac7b9168f70789f1cd128852b11c32d5cc9589b4b5ab93a58`。本次生成补丁保持原源码和目标机固件不变。

## 一、原始源码与补丁文件

原源码包名称为 `uboot_youxi_202508_22.tar.gz`，长度为 925511680 字节，SHA-256 如下。文件实际是普通 POSIX tar，解包使用 `tar -xf`。

```text
f402c0bded35720737fb2cd51ff5d915bcad06345e2e34a4b1019f10332ccb1a
```

解包后的源码根目录为 `v2017.09-rk3588-taiyi`。U-Boot 本身没有可用 Git commit 标识，本包以压缩包 SHA 和逐文件 SHA 确定适用版本；其中 `rkbin` 的 Git 信息属于另一仓库。

| 文件 | 内容与使用条件 |
| --- | --- |
| [0001-build-adaptation.patch](patches/0001-build-adaptation.patch) | 修正 `KBUILD_CFLAGS` 拼写，改用 Python 3，按本地接口调整 DWC3 PHY 参数；用于从这份源码完成构建 |
| [0002-boot-compatibility.patch](patches/0002-boot-compatibility.patch) | 保留 B3 的早期栈地址、EFI 配置、原 NVMe 启动命令兼容、control tree 的 phandle 查找和工厂 eMMC 无 ini 的 legacy 启动 |
| [0003-edp-assr.patch](patches/0003-edp-assr.patch) | 同步屏幕与发送器 ASSR、核对读回、检查视频开启前状态、向显示调用方传递失败并清理已启用资源 |
| [ulan-uboot-b3-all.patch](ulan-uboot-b3-all.patch) | 从原始源码直接生成的完整 B3 补丁，合并上述三个部分，用于一次应用全部改动 |
| [series](series) | 三份分项补丁的应用顺序 |
| [manifest.json](manifest.json) | 原包、补丁及 12 个修改文件的修改前后 SHA-256，逐文件增删行数 |
| [validation.json](validation.json)、[patch-apply.log](patch-apply.log) | 本次补丁应用、撤销、字节一致性及空白检查的结果和命令日志 |

完整补丁与三份分项补丁是两种应用入口，选择其中一种即可。它们是普通 unified diff，使用 `patch` 或 `git apply`；使用 Git 的接收方可以在审阅后自行提交。

## 二、各部分的实际修改

### 1. 构建适配

`0001` 修改四个文件：

- `Makefile`：将遗漏的 `KBUILD_` 恢复为 `KBUILD_CFLAGS`，使导出和 `-Os` 选项作用于正确变量。
- `make.sh`：检查 Python 3，并将 `PYTHON=python3` 传给 make。
- `arch/arm/mach-rockchip/decode_bl31.py`：修改解释器入口为 Python 3。
- `drivers/usb/host/xhci-dwc3.c`：按原包实际的 `dwc3_setup_phy()`、`dwc3_shutdown_phy()` 接口，保存 `struct phy *usb_phys` 与 `num_phys` 并传递相应参数。

### 2. 启动兼容修正

`0002` 修改四个文件，保持本次 B3 实际使用的配置和启动行为：

- `include/configs/evb_rk3588.h`：把 `CONFIG_SYS_INIT_SP_ADDR` 设为 `0x00800000`，使早期 malloc、全局数据和栈避开 payload 及附加 DTB；原 4 MiB 布局在本次源码构建中发生覆盖。
- `configs/rk3588-yourland_defconfig`：将 `CONFIG_EFI_LOADER` 从 `n` 改为 `y`，与本次 B3 构建配置一致。EFI/GRUB 的完整启动效果须按对应场景另行验证。
- `common/osl.c`：同时接受工厂命令 `bootyl nvme krnl6` 和带分区的 `bootyl nvme 0:1 krnl6`。工厂 eMMC 找不到 ini 时返回 `-ENOENT`，转入已有 legacy 启动；这一分支保留 LCD Logo，避免开启 LCD 调试控制台后改变交给 Linux 的 framebuffer。
- `drivers/core/of_access.c`：`CONFIG_USING_KERNEL_DTB_V2` 下，默认查找在当前树找不到 phandle 时，继续查找原 control tree `gd->of_root_f`；显式指定树的调用保留原查找范围。该修正用于解析保存在 control tree 中的时钟、GPIO 等提供者。

### 3. eDP ASSR 修复

原始 U-Boot 的 eDP 发送器 ASSR 位关闭，历史 SWD 对照中，开启该位后电池图标正常，恢复原值后白色噪点重新出现。本次 B3 启动读回进一步确认：屏幕 ASSR 设置已为 `0x01`，发送器初值仍为 `0x50`。

`0003` 修改四个显示文件，调用顺序如下：

1. `analogix_dp_link_start()` 在写 `DP_DOWNSPREAD_CTRL` 后、训练图案 1 前调用 `analogix_dp_set_assr_mode()`。
2. `analogix_dp_set_assr_mode()` 使用已经读入的 DPCD `0x00d` bit 0 判断支持能力，通过 AUX 修改屏幕 DPCD `0x10a` bit 0，并读回核对。本地 AUX 接口以返回 0 表示成功。
3. 屏幕配置确认后，`analogix_dp_enable_assr_mode()` 按位修改发送器 `ANALOGIX_DP_LINK_POLICY` 的 bit 7，并读回核对。寄存器偏移为 `0x9d8`，本机 eDP1 地址为 `0xfded09d8`；其余位保留。
4. `analogix_dp_config_video()` 在 `analogix_dp_start_video()` 前检查发送器 ASSR 与目标状态一致。支持 ASSR 时两端开启，不支持时两端关闭。
5. AUX 失败、读回不一致或视频前状态错误沿调用链返回；`rockchip_display.c` 的 `display_enable()` 清理已启用的 connector、CRTC、panel 和 bridge 资源，`display_logo()` 返回实际显示结果。

对应源码文件为 `drivers/video/drm/analogix_dp.c`、`analogix_dp.h`、`analogix_dp_reg.c` 和 `rockchip_display.c`。实现参考 Rockchip 的 ASSR 支持修改 `fc2750785b7f24959c90ae972527f1ce47886d52` 及关闭地址修正 `4e1a12d507df3921a6e8387f61fecf29b763d765`，按本地 AUX 接口适配。

## 三、核对输入并应用补丁

依赖为 GNU tar、GNU patch、sha256sum；使用 `git apply` 时还需要 Git。操作影响新解包目录中的源码文件，成功条件为预检及应用均退出 0。采用新的解包目录保存原输入。

以下命令从同时放有原源码包和解压后 `ulan-uboot-patches-20261011/` 的工作目录执行。**按阶段人工执行并检查退出码；任一命令失败时停止后续步骤，保留输出。** `uboot-patch-work` 应为首次使用的目录。

```bash
# 核对原源码包，再核对补丁包内的文件
sha256sum -c ulan-uboot-patches-20261011/ORIGINAL-SOURCE.sha256
cd ulan-uboot-patches-20261011
sha256sum -c SHA256SUMS
cd ..

# 保留原包，在新的目录中解包并进入 U-Boot 根目录
mkdir uboot-patch-work
tar -xf uboot_youxi_202508_22.tar.gz -C uboot-patch-work
cd uboot-patch-work/v2017.09-rk3588-taiyi
```

完整复现 B3 源码时，先预检完整补丁；退出 0 后再应用。`--fuzz=0` 要求上下文完整匹配，`--forward` 防止误将已应用的补丁反向处理。

```bash
# 预检通过后，应用完整 B3 改动
patch --batch --forward --fuzz=0 --dry-run -p1 -i ../../ulan-uboot-patches-20261011/ulan-uboot-b3-all.patch
patch --batch --forward --fuzz=0 -p1 -i ../../ulan-uboot-patches-20261011/ulan-uboot-b3-all.patch
```

希望逐份审阅和应用时，在新的原始源码目录中按 `series` 的顺序应用 `0001`、`0002`、`0003`。每份采用同样的预检和应用命令，把 `-i` 的路径换成对应文件，检查通过后再继续下一份。

只集成 ASSR 修复时，在原始源码目录中使用下面两条命令。本次已经确认该补丁可以独立应用；单独应用的检查证明源码上下文匹配，其构建和板端启动还取决于接收分支的构建及启动代码。

```bash
# 预检通过后，只应用 eDP ASSR 修复
patch --batch --forward --fuzz=0 --dry-run -p1 -i ../../ulan-uboot-patches-20261011/patches/0003-edp-assr.patch
patch --batch --forward --fuzz=0 -p1 -i ../../ulan-uboot-patches-20261011/patches/0003-edp-assr.patch
```

Git 仓库中可用 `git apply --check --whitespace=error-all <补丁路径>` 预检，再用 `git apply --whitespace=error-all <补丁路径>` 应用。接收分支存在额外修改时，以预检结果判断是否需要人工移植。

本次 B3 构建使用 `rk3588-yourland`、GCC 11.4.0 的 `aarch64-linux-gnu-` 工具链、Python 3、dtc 和 `SOURCE_DATE_EPOCH=1791716400`。既有构建入口为 `make.sh rk3588-yourland ... --idblock`。本包交付源码补丁；此前的《幽兰U-Boot-ASSR源码修复与候选镜像.zip》保留完整编译、原 FIT 组件重打包和部署步骤。生成与本次 5 MiB B3 相同布局的镜像需要沿用该重打包过程。

## 四、撤销源码补丁

撤销用于恢复本次修改的源码内容。在补丁之后继续编辑过相同代码时，先保存后续编辑，检查撤销预检输出，再决定恢复方式。

完整补丁应用后，从同一个源码根目录分阶段执行：

```bash
# 核对撤销条件，退出 0 后恢复原源码内容
patch --batch --fuzz=0 --dry-run -R -p1 -i ../../ulan-uboot-patches-20261011/ulan-uboot-b3-all.patch
patch --batch --fuzz=0 -R -p1 -i ../../ulan-uboot-patches-20261011/ulan-uboot-b3-all.patch
```

分项应用时按 `0003`、`0002`、`0001` 的逆序撤销；仅应用 `0003` 时只撤销该文件。撤销源码后，下次构建使用恢复后的内容；机器上已经写入的固件需使用此前验证过的原固件恢复步骤。

## 五、本次导出验证与既有板端结果

本次直接从已核对 SHA 的原压缩包提取 12 个受影响文件，检查完整补丁与分项补丁的应用及撤销。GNU patch 使用 `--fuzz=0`，Git 预检使用 `--whitespace=error-all`。应用后的 12 个文件逐字节等于 B3 开发源码，撤销后逐字节等于原包内容；`0003` 单独应用时，其他八个文件保持原内容。命令和结果见 [应用检查日志](patch-apply.log)、[验证结果](validation.json)，逐文件 SHA 见 [版本清单](manifest.json)。

本次只导出和验证已有修改，编译及板端证据沿用此前同一份 B3：电池图标正常；追加 7 次重启均进入原 NVMe/GDM，画面抽查正常，其中 5 次取得完整 ASSR 读回。断电冷启动仍待验证。完整条件、屏摄和串口证据保存在此前交付的镜像包及《修复与验证说明》中。
