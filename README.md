# PPPPS

PPPPS 是一个对象驱动型智能图像编辑器。当前代码处于 Qt 桌面端 POC 阶段，
尚未通过立项闸门，不代表完整产品能力已经可用。

## macOS POC 构建

首次使用前，只需要确保已经安装 Homebrew 和 Xcode Command Line Tools：

```bash
xcode-select --install
```

之后使用统一入口；`make build` 会检查 `cmake`、`qtbase`、`onnxruntime`、
`ninja`、`pkgconf`，缺失时自动通过 Homebrew 安装；还会下载并校验约 46 MB 的
Mask R-CNN POC 模型，再配置并构建工程：

```bash
make build      # 检查/安装依赖并构建
make test       # 构建并运行测试
make run        # 构建并启动 PPPPS.app
make doctor     # 只检查环境，不安装
make models     # 只准备并校验模型
make evaluate-m0-02  # 导出一次真实推理的 JSON、Mask 和叠加图
```

如果系统已有 OpenCV 4，可通过 `CMAKE_PREFIX_PATH` 使用它；否则预设会下载固定
提交的 OpenCV 4.12.0，仅构建 `core` 和 `imgproc`。构建产物位于 `build/`，模型位于
`poc/models/`，测试图片位于 `poc/evaluation/data/`；这些下载内容都不会进入 Git。

当前桌面 POC 已支持真实的“打开图片 → 自动识别对象 → 悬停高亮轮廓 → 点击获得
Mask”。模型只覆盖 COCO 80 类，实际命中率和已知失败见
[`docs/development/M0_02_RESULTS.md`](docs/development/M0_02_RESULTS.md)。

项目进度以 [`docs/development/STATUS.md`](docs/development/STATUS.md) 和
[`docs/development/ROADMAP.md`](docs/development/ROADMAP.md) 为准。
