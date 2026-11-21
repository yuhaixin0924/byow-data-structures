# Windows

## C++ 原生版

源码构建需要 CMake 3.24+、Git 和 C++17 编译器。在仓库根目录配置并构建：

```sh
cmake -S . -B build-native -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build-native --config Release
cmake --install build-native --prefix platforms/Windows/Native
```

首次配置会获取 SFML 3.0.2。发行目录内含 `byow_native.exe`、SFML 与 MinGW DLL、字体和许可文件。安装后双击 `启动游戏.bat` 或 `Native/byow_native.exe` 即可运行，无需 Java、Python 或 IDE。

首页可查看玩法帮助、选择角色或进入四季迷宫。每关在「出发」后开始倒计时，拾取时钟延长 30 秒。收集四朵花并抵达出口后，选关地图的花朵会恢复颜色，进度保存到当前 Windows 用户。游玩时按 Tab 切换全图与跟随镜头，按 M 切换声音。

## Java 兼容版

如果本机目录中已有 Windows 免安装包，可双击 `BYOW.exe` 运行；包内含 Java runtime 和 Windows C++ 引擎，无须另装 Java 或 Python。发行包不纳入 Git，因此新克隆的仓库默认没有该 exe。

未安装原生版时，`启动游戏.bat` 会回退到 Java 兼容版。也可以用 `启动游戏.bat --java` 主动选择兼容版。脚本会优先运行已有的 `BYOW.exe`，否则调用仓库根目录的 `run.py`；从源码运行需要 Python 3.10+、JDK 17+ 和 C++17 编译器。

免安装包是 Windows x86_64 构建产物。重新生成或更新包时，应在 Windows 上使用 JDK 17 的 `jpackage`，并将整个应用目录放在此处；大型 runtime 和二进制不纳入 Git。
