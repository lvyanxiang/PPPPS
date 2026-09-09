# PPPPS 依赖登记

> 当前登记用于 M0 POC 技术与分发风险评估，不构成法律意见。正式发布前必须锁定
> 实际发布版本、动态库清单、许可证文本、Notice、源代码提供方式和安装包行为。

| 依赖 | M0 固定版本/来源 | 用途 | 许可证初步结论 | 分发与回退约束 |
| --- | --- | --- | --- | --- |
| Qt Base | Homebrew `qtbase 6.11.2`；[Qt 官方](https://www.qt.io/development/open-source-lgpl-obligations) | Core、Gui、Widgets、Test | 开源路径以 LGPLv3 为主，部分模块可能仅 GPL；也可购买商业许可 | POC 使用动态库；商业闭源发布前完成 LGPL 合规设计或采购商业许可，不引入仅 GPL 模块 |
| OpenCV | `4.12.0`，提交 `49486f61fb25722cbcf586b7f4320921d46fb38e`；[OpenCV 官方许可](https://opencv.org/license/) | 本地颜色转换、滤波及后续传统视觉算子 | 4.5.0 及以上为 Apache-2.0 | POC 只构建 `core`、`imgproc`；系统安装缺失时由 CMake 下载固定提交；最终安装包登记其传递依赖 |
| ONNX Runtime | Homebrew `onnxruntime 1.29.0`；[Microsoft 官方仓库许可](https://github.com/microsoft/onnxruntime/blob/main/LICENSE) | 本地模型推理与 CPU 回退 | MIT | M0-01 只验证运行时、CPU 张量和后端枚举；真实模型推理从 M0-02 开始，模型许可证单独登记 |
| Mask R-CNN R-50-FPN INT8 | ONNX Model Zoo `MaskRCNN-12-int8.onnx`；SHA-256 `4409935e…f5b476` | M0-02 COCO 80 类对象检测与实例 Mask | 具体模型卡写 MIT；Hugging Face 镜像元数据写 Apache-2.0，待发布前人工复核 | 二进制不进 Git；`make models` 下载并校验；只覆盖 COCO 80 类，不能覆盖服装/天空等完整产品语义；M0 当前只验证 CPU |
| MODNet photographic portrait matting | MODNet 官方仓库所列 ONNX；SHA-256 `07c308cf…6584df9` | M0-03 人物连续 Alpha | 官方仓库声明代码、模型与 demo（GIF 除外）为 Apache-2.0 | 二进制不进 Git；`make models` 下载并校验；仅把人物视为承诺范围，透明玻璃需专用模型 |

## M0-01 边界

- Qt 图片插件实际解码临时生成的 JPEG/PNG。
- OpenCV 实际执行 RGBA→BGR、3×3 Gaussian Blur 和通道均值计算。
- ONNX Runtime 实际创建环境、SessionOptions、CPU Tensor 并枚举执行后端。
- 本节点没有加载模型，不能作为“模型推理已经通过”的证据。
- macOS/Apple Silicon 以外的平台和 GPU/NPU 后端仍是未验证状态。

## M0-02 边界

- 模型下载地址、完整 SHA-256、文件大小、输入输出约定和许可证差异记录于
  `poc/models/README.md`。
- 实际加载 INT8 ONNX 权重并输出 boxes、labels、scores 和 instance masks，不是模拟框
  或固定 Mask。
- 当前 Session 明确走 `CPUExecutionProvider`；Core ML、DirectML、CUDA 等后端尚未验证。
- 13 个固定点击探针命中 9 个；COCO 类别缺口与透明/小密集对象失败不能视为通过，详见
  `docs/development/M0_02_RESULTS.md`。

## M0-03 边界

- MODNet 模型二进制、输入输出约定、SHA-256 与许可来源记录于 `poc/models/README.md`。
- Qt 后台线程经 ONNX Runtime CPU 输出真实连续 Alpha；没有用二值 Mask 羽化冒充 Matting。
- 人物/头发路径可行，简单猫狗样例仅作超范围观察，透明玻璃瓶失败；完整证据见
  `docs/development/M0_03_RESULTS.md`。
- M0-03 没有使用许可冲突的 AM-2K，也没有把仅限非商业研究的 Transparent-460 变成
  产品依赖。没有真值时不报告伪造的 Matting 准确率。
