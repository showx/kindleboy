# KT3 黑白 Game Boy 安装流程

适用目标：Kindle 第 8 代入门版（2016 / KT3，序列号前缀 `G000K9`），固件 `5.15.1.1`。底层使用现有 Kindle Linux 和设备驱动。专用模式停止阅读界面，退出后恢复；另提供按机型安装的开机启动配置。

## 1. 准备文件

`kindleboy-kt3-5.15.1.1.zip` 包含：

- `extensions/kindleboy/`：模拟器、配置、兼容旧启动器的菜单和许可文件。
- `documents/KindleBoy.sh`：现代越狱的普通启动入口。
- `documents/KindleBoy-Dedicated.sh`：专用模式入口。
- `documents/KindleBoy-Setup.sh`：安装开机配置并显示成功/失败结果。
- `roms/gb/KindleBoy-Demo.gb`：本项目生成的黑白测试程序，不是商业游戏。

将这三个目录合并到 Kindle USB 盘根目录。项目提供的 `scripts/install-device.ps1` 会检查固件、验证包内校验值、备份被覆盖的相关文件，并保留已有模拟器设置。它不读取书籍，也不修改系统分区。

未越狱的设备还需要 `dist/jailbreak/winterbreak2/` 文件夹，放到 Kindle USB 盘根目录。仅复制这些文件不会完成越狱。

## 2. 在 Kindle 上完成 WinterBreak2

已按 KindleModding 的机型和固件规则核对：KT3 的 `5.15.1.1` 在 WinterBreak2 适用范围内。

1. 保持当前固件，先阅读 [官方越狱说明](https://kindlemodding.org/jailbreaking/) 和 [WinterBreak2 指南](https://kindlemodding.org/jailbreaking/WinterBreak2/)。联网前按官方向导检查适用于这台设备的防更新要求。
2. 确认 USB 盘根目录存在 `winterbreak2/dialoger.html`，然后安全弹出并拔掉 USB。
3. 在 Kindle 连接 Wi-Fi，打开实验性浏览器。
4. 输入 `https://penguins184.xyz/wb2`，点击网页上的 **Jailbreak**，等待屏幕提示完成。不要同时进行更新或重启。
5. 记录屏幕上的成功信息或错误；若界面要求额外步骤，先按该提示核查，不能把“复制文件成功”当成“越狱完成”。

本次准备的官方包为 WinterBreak2 `v1.1.0`。页面执行时会获取官方站点的 `jb.sh`，安装内容以当时的官方脚本为准。

## 3. 首次启动与验证

现代 `hdnext` 越狱支持把 `documents/*.sh` 显示成书库中的可运行入口。参考 [官方应用安装说明](https://kindlemodding.org/jailbreaking/whats-next/installing-homebrew.html)。如果当前越狱只提示安装 hotfix，需先完成它要求的后续步骤，再使用入口。

1. 先打开书库中的 **KindleBoy** 普通入口。
2. 从游戏列表选择 `KindleBoy-Demo.gb`。应看到四级灰度图案和文字；按 A 反色，按 B 恢复。
3. 检查方向键、A/B、START/SELECT 是否响应；顶部 MENU 打开暂停菜单，CLEAN 清理残影，3X 切换三倍速。
4. 验证 SAVE STATE / LOAD STATE，再退出并重新进入，检查续玩和 `.sav`、`.st` 存档。
5. 普通模式确认正常后，再测试 **KindleBoy-Dedicated**。退出模拟器后，阅读界面恢复可能需要数十秒。

游戏文件放到 `roms/gb/`，优先使用黑白 `.gb`。界面字体目前为 ASCII，游戏文件名建议使用英文。自备游戏文件；本包没有商业游戏。

## 故障定位与恢复

- 启动失败：重新连接 USB，查看根目录 `kindleboy.log`。
- 没有可运行入口：先核对越狱是否完成，检查官方当前的后续安装要求。
- 画面残影：使用顶部 CLEAN，或暂停菜单切换 QUALITY。
- 触摸偏移或组合按键无效：保留日志中的 `fbink:` 和 `input:` 行，继续调试 KT3 的输入适配。
- 进程被强制终止后再次启动提示 session directory exists：重启可清除临时目录；不要重复开启多个实例。
- 游戏中先正常退出，再连接 USB 传输游戏或存档。

已在这台 KT3 上验证普通模式的画面、A/B 操作、存档保存/读取和退出，设备日志也显示操作成功。用户另已确认 Setup 显示 SUCCESS，重启后自动进入 Game Boy。组合按键、休眠、不同游戏与长时间使用仍需继续测试。

## 4. 专用开机模式

开机配置安装到 `/etc/upstart/kindleboy.conf`，从 Kindle 的 `framework_ready` 事件启动。它使用单独任务，每个内核启动周期只运行一次，退出游戏后可返回阅读界面。

普通模式验证后，通过 `install-device.ps1` 更新应用，安全弹出 Kindle。

1. 在书库打开 **KindleBoy Setup**。
2. 等待明确的 **SUCCESS** 提示。结果也写入书库中的 `KindleBoy-Setup-Result.txt`；若显示 FAILED，重新连接 USB 检查 `kindleboy-setup.log`。
3. 从 Kindle 设置中选择「重新启动」，测试是否自动进入 Game Boy 游戏列表，测试时断开 USB。
4. 测试游戏和 MENU → EXIT EMULATOR，确认能够返回 Kindle 界面。

安装脚本只创建项目自己的启动任务，检查其是否被系统识别，并恢复原有的根文件系统挂载状态；验证失败时回滚任务。设备上的 `kindleboy-setup.log`、`kindleboy-boot.log` 和 `kindleboy.log` 分别记录安装、开机和游戏运行过程。浏览器动作会消费一次性请求文件，后台重试不会重复安装或重开游戏。

开发用的浏览器动作只用于书库启动支持尚未正常时的诊断；使用过它后可恢复原始浏览器入口：

```powershell
python .\scripts\set-browser-action.py original --drive H:/
```

### 从 USB 关闭自启

在 USB 盘根目录创建无扩展名的 `DISABLE_KINDLEBOY` 文件，或运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\disable-autostart.ps1 -DriveRoot H:\
```

安全弹出并重启后，设备保留阅读界面。删除该关闭标记可重新允许自启。检测到 USB 传输模式或无法确认 USB 状态时，启动脚本也会跳过当前开机的游戏启动。

## 5. 日常使用

- 开机或重新启动后自动进入 Game Boy 游戏列表。
- 导入游戏：先用 MENU → EXIT EMULATOR 退出，连接 USB，把自己的 `.gb` 文件复制到 `roms/gb/`，安全弹出后从书库的 KindleBoy 入口启动，或重新启动设备。
- 存档：MENU → SAVE STATE / LOAD STATE；正常退出时也会自动保存续玩状态。存档与 ROM 放在同一目录，`.sav` 是电池存档，`.st` 是本版本的即时存档。
- 返回游戏列表：MENU → GAME SELECT。
- 返回 Kindle 界面：MENU → EXIT EMULATOR。
- 清理残影：顶部 CLEAN，或暂停菜单中的 CLEAN SCREEN。
- 游戏列表和暂停菜单可以用方向键选择，A/START 确认、B 返回；列表左右翻页。直接触摸列表和菜单也可操作。
- 按住按键时出现下沉、高亮和阴影变化，松开恢复；墨水屏上快速短按的视觉效果会短暂保留，不延长实际游戏输入。
- 临时保留阅读界面：退出模拟器后不会在同一次开机中重复自动打开；需要长期关闭自启时，使用第 4 节的 USB 关闭标记。

### 更新界面

已安装并验证自启的设备，只需退出模拟器、连接 USB，然后重新运行 `install-device.ps1 -DriveRoot H:\`。更新后安全弹出，打开书库的 KindleBoy 即可；无需再次运行 Setup。安装脚本保留配置和存档，并备份被覆盖的项目文件。
