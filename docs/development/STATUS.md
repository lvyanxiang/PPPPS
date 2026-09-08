# PPPPS 项目状态

## 当前

- 里程碑：M0 — Qt 桌面端 POC 与立项闸门
- 当前节点：M0.1 — 对象识别与 Mask 真实链路验证
- 下一任务：完成 M0-02，接入许可可接受的真实检测/分割模型，打通“打开图片 → 自动检测主要对象 → 悬停轮廓 → 点击命中 → 获得 Mask”，并记录按对象类型的命中率、耗时和错误。
- 最近完成：M0-01；建立 Qt 6.11.2 + C++20 + CMake POC，实际验证 JPEG/PNG、OpenCV 4.12.0、ONNX Runtime 1.29.0 CPU 路径、后台进度/取消/错误和自动化测试。

## 决策

- 产品定位是“对象驱动型智能图像编辑器”，不是简化版 Photoshop。
- 桌面端使用 Qt 6 + C++20 + CMake；默认以 Qt Widgets 搭建专业桌面外壳，以自定义渲染视口承载画布。若 POC 证据表明 QML 更合适，再记录迁移决定。
- OpenCV 承担成熟传统图像处理，ONNX Runtime 承担本地模型推理；Skia/GPU 后端在 M0 性能数据后通过渲染抽象接入，不在 POC 骨架中无条件绑定。
- 原图保持不可变；编辑以文档、语义对象、Mask、参数和命令记录，默认非破坏、可撤销。
- M0 未记录 `GO` 前，不进入 M1 正式引擎开发；`CONDITIONAL GO` 只能执行评审中明确列出的补充 POC。
- 开发进度文档使用中文，项目技能本身使用便于代理执行的英文。
- POC 数据与结果不进入 Git；许可证冲突的 AM-2K 在人工复核前阻塞，Transparent-460 仅限隔离的非商业本地研究，不能成为产品发布或商业训练依赖。
- M0-01 只安装 Qt Base 与 ONNX Runtime；OpenCV 以固定提交按 `core/imgproc` 最小构建。Qt 自动代码生成只作用于 PPPPS 目标，避免污染第三方 CMake 目标。
- M0-01 的 ONNX Runtime 证据仅覆盖环境、SessionOptions、CPU Tensor 和执行后端枚举；没有模型文件，不能视为真实推理通过。
- macOS 开发入口统一为根目录 `Makefile`：`make build` 自动检查并安装 Homebrew 依赖，`make test` 验证，`make run` 启动；Homebrew 与 Xcode Command Line Tools 仍需预先安装。

## 验证

- 里程碑覆盖审计：193 个任务编号均存在且唯一；已把立项文档中的核心功能、完整能力拆解、八项 POC、明确延后项和生态项映射到 M0–M10 与横切风险组。
- 技能校验：`quick_validate.py` 通过，`agents/openai.yaml` 可正常解析。
- 进度脚本：文本与 JSON 输出均通过，能够定位 M0、M0.1 和首个未完成项 M0-02；当前开发任务进度为 2/193。
- M0-00 清单：CSV 结构和状态值可解析，45 条记录代表 65 个案例；41 个具体文件均有 64 位十六进制 SHA-256，案例 ID 无重复。
- M0-00 取样：实际下载并目视检查 24 个 Open Images、10 个 TextOCR 与 7 个 P3M Demo 文件；未把临时图片或可能受限数据提交到仓库。
- 基线硬件：macOS 26.3、Apple M4（10 CPU/10 GPU）、24GB、Metal 4；Windows、Intel、AMD、NVIDIA 仍明确为未验证。
- M0-01 配置与构建：`cmake --preset macos-homebrew-debug` 和 `cmake --build --preset macos-homebrew-debug --parallel 8` 通过；产物为 arm64 Mach-O Qt Widgets `.app`。
- M0-01 核心链路：临时生成的 JPEG/PNG 均由 Qt 解码；OpenCV 实际执行颜色转换、Gaussian Blur 与均值；ONNX Runtime 1.29.0 实际创建 CPU Tensor 并确认 `CPUExecutionProvider`。
- M0-01 并发与错误：自动化覆盖后台成功、后台失败、进度信号、同步取消与后台取消；`ctest --preset macos-homebrew-debug --output-on-failure --repeat until-fail:10` 连续通过，两项 CTest、零失败。
- M0-01 依赖与分发边界记录于 `docs/development/DEPENDENCIES.md`；本机安装 Qt Base 6.11.2、ONNX Runtime 1.29.0、Ninja 1.13.2、pkgconf 3.0.7。
- macOS 构建入口：`make doctor`、`make build`、`make test` 均通过；动态计算 Homebrew 前缀，不再要求 Makefile 假定 Apple Silicon 固定路径。

## 备注

- 当前仓库已具备可运行的 Qt POC 骨架，但尚未加载任何模型；M0-02 才开始真实检测与分割推理。
- `docs/development/ROADMAP.md` 是范围与勾选状态的唯一事实来源；本文件只保留恢复开发所需的短指针。
