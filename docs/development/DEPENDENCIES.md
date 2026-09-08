# PPPPS 依赖登记

> 当前登记用于 M0 POC 技术与分发风险评估，不构成法律意见。正式发布前必须锁定
> 实际发布版本、动态库清单、许可证文本、Notice、源代码提供方式和安装包行为。

| 依赖 | M0 固定版本/来源 | 用途 | 许可证初步结论 | 分发与回退约束 |
| --- | --- | --- | --- | --- |
| Qt Base | Homebrew `qtbase 6.11.2`；[Qt 官方](https://www.qt.io/development/open-source-lgpl-obligations) | Core、Gui、Widgets、Test | 开源路径以 LGPLv3 为主，部分模块可能仅 GPL；也可购买商业许可 | POC 使用动态库；商业闭源发布前完成 LGPL 合规设计或采购商业许可，不引入仅 GPL 模块 |
| OpenCV | `4.12.0`，提交 `49486f61fb25722cbcf586b7f4320921d46fb38e`；[OpenCV 官方许可](https://opencv.org/license/) | 本地颜色转换、滤波及后续传统视觉算子 | 4.5.0 及以上为 Apache-2.0 | POC 只构建 `core`、`imgproc`；系统安装缺失时由 CMake 下载固定提交；最终安装包登记其传递依赖 |
| ONNX Runtime | Homebrew `onnxruntime 1.29.0`；[Microsoft 官方仓库许可](https://github.com/microsoft/onnxruntime/blob/main/LICENSE) | 本地模型推理与 CPU 回退 | MIT | M0-01 只验证运行时、CPU 张量和后端枚举；真实模型推理从 M0-02 开始，模型许可证单独登记 |

## M0-01 边界

- Qt 图片插件实际解码临时生成的 JPEG/PNG。
- OpenCV 实际执行 RGBA→BGR、3×3 Gaussian Blur 和通道均值计算。
- ONNX Runtime 实际创建环境、SessionOptions、CPU Tensor 并枚举执行后端。
- 本节点没有加载模型，不能作为“模型推理已经通过”的证据。
- macOS/Apple Silicon 以外的平台和 GPU/NPU 后端仍是未验证状态。

