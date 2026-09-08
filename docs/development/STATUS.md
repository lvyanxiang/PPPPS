# PPPPS 项目状态

## 当前

- 里程碑：M0 — Qt 桌面端 POC 与立项闸门
- 当前节点：M0.1 — 人物抠图与 Alpha Matting 对比
- 下一任务：完成 M0-03，对比实例分割人物 Mask 与 Image/Alpha Matting；分别验证头发、动物毛发、婚纱/玻璃半透明边缘，并给出手工修正可行性结论。
- 最近完成：M0-02；在 Qt 桌面端接入真实 Mask R-CNN INT8，通过 ONNX Runtime CPU 打通自动对象检测、Mask、悬停轮廓和点击命中；13 个固定点击探针命中 9 个（69.2%），明确记录服装类别缺口及近景马、小密集瓶、透明瓶失败。

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
- M0-02 使用 ONNX Model Zoo `MaskRCNN-12-int8.onnx`，模型与测试图片由 Makefile 下载并按 SHA-256 校验，不进入 Git；当前固定 score 阈值 0.70、mask 阈值 0.50，执行后端为 CPU。
- Mask R-CNN 只覆盖 COCO 80 类，不能把 M0-02 的真实链路通过误解为全部产品对象通过；服装、透明物体和小密集对象需要后续模型组合。模型卡 MIT 与镜像 Apache-2.0 元数据不一致，正式发布前需人工复核。

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
- M0-02 真实推理：`make test` 的 5 个 CTest 全部通过；真实 OI-07 图片经 `MaskRCNN-12-int8.onnx` 输出 `cat` box、score 和完整分辨率 Mask，中心点击命中，非 stub/固定结果。
- M0-02 交互：自动化验证 Mask 像素命中、重叠对象优先选择较小实例，以及 Qt 画布鼠标悬停/点击信号；人工查看 OI-07 叠加图，主体轮廓位置正确。
- M0-02 固定探针：`make evaluate-m0-02` 可复现执行 13 次点击、覆盖 10 类对象，9/13 命中且无技术错误；人物、猫、狗、鸟、普通场景马、牛、摩托车、书、手机命中，服装、极近景马、密集小瓶、透明瓶失败。冷启动纯推理 P50/P95 为 484/617 ms，全链路 P50/P95 为 983/1111 ms；完整证据见 `M0_02_RESULTS.md`。
- 进度脚本：M0-02 勾选后当前开发任务进度为 3/193，下一项定位到 M0-03。

## 备注

- 当前 Qt POC 已加载真实实例分割模型并可悬停/点击对象；69.2% 探针结果低于最终普通对象 90% GO 阈值，后续节点必须继续处理模型覆盖与边缘质量，不能提前给出 M0 `GO`。
- `docs/development/ROADMAP.md` 是范围与勾选状态的唯一事实来源；本文件只保留恢复开发所需的短指针。
