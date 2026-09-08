# PPPPS

PPPPS 是一个对象驱动型智能图像编辑器。当前代码处于 Qt 桌面端 POC 阶段，
尚未通过立项闸门，不代表完整产品能力已经可用。

## macOS POC 构建

首次使用前，只需要确保已经安装 Homebrew 和 Xcode Command Line Tools：

```bash
xcode-select --install
```

之后使用统一入口；`make build` 会检查 `cmake`、`qtbase`、`onnxruntime`、
`ninja`、`pkgconf`，缺失时自动通过 Homebrew 安装，再配置并构建工程：

```bash
make build      # 检查/安装依赖并构建
make test       # 构建并运行测试
make run        # 构建并启动 PPPPS.app
make doctor     # 只检查环境，不安装
```

如果系统已有 OpenCV 4，可通过 `CMAKE_PREFIX_PATH` 使用它；否则预设会下载固定
提交的 OpenCV 4.12.0，仅构建 `core` 和 `imgproc`。构建产物和下载的依赖位于
`build/`，不会进入 Git。

项目进度以 [`docs/development/STATUS.md`](docs/development/STATUS.md) 和
[`docs/development/ROADMAP.md`](docs/development/ROADMAP.md) 为准。
