# 可直接玩的免费 GB 游戏

这三款是作者发布的完整自制游戏，不是显示测试程序。它们分别来自作者网站、作者发布的下载包或作者源码仓库；没有改动 ROM 内容。

| 游戏文件 | 内容 | 开始方式 | 作者与来源 | 许可 |
| --- | --- | --- | --- | --- |
| `2048-Sanqui.gb` | 合并数字的益智游戏，画面变化较少 | START 开始，方向键操作 | [Sanqui / 2048-gb](https://github.com/Sanqui/2048-gb) | zlib |
| `MinesweepGB.gb` | 扫雷，有难度选择 | 在游戏菜单选择难度 | [lancekindle / minesweepGB](https://github.com/lancekindle/minesweepGB) | GPL-3.0 |
| `Tobu-Tobu-Girl.gb` | 跳跃、冲刺的平台动作游戏 | 等待开场后按 START，操作参见原版手册 | [Tangram Games](https://tangramgames.itch.io/tobutobugirl) | 代码 MIT，素材 CC BY 4.0 |

Tobu Tobu Girl 由 Tangram Games 制作，音乐由 potato-tan 创作。原始下载包和手册保留在游戏包的 `extras/` 中。MIT、CC BY、zlib、GPL 许可及扫雷的对应源代码均随游戏包提供。各游戏保留各自的许可，不统一改为 kindleboy 项目的许可。

三个 ROM 均通过当前 kindleboy 模拟核心的 360 帧启动检查，并检查了启动画面。尚未逐一验证 Kindle 真机上的完整关卡、操作组合和性能。

## 导入

准备好的文件位于 `dist/free-games/`，离线包为 `dist/kindleboy-free-games.zip`。退出 Kindle 上的模拟器，连接 USB，再运行：

```powershell
python .\scripts\install-games.py --drive H:/
```

脚本核对 ROM 校验值和 GB 头部，把三个 `.gb` 文件复制到 `roms/gb/`，把来源、许可和源代码放进 `extensions/kindleboy/games-info/`。它不会修改现有存档。若同名游戏已有不同内容，会停止并报告冲突。

安全弹出后，打开书库中的 KindleBoy，或断开 USB 后重新启动设备，在列表选择游戏。

## 马力欧、宝可梦和塞尔达

这些商业原版 ROM 不在本包内。用户提供已有的 `.gb` / `.gbc` 文件后，可以复制到相同的 `roms/gb/` 目录使用。不要把 NES 的 `.nes` 或 GBA 的 `.gba` 文件当作 GB ROM 导入。
