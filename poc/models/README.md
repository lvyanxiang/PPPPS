# M0 POC 模型

模型二进制不进入 Git。运行 `make build` 或 `make models` 时会把固定文件下载到本目录，
并在使用前校验 SHA-256。

## MaskRCNN-12-int8

- 用途：M0-02 自动对象检测与实例 Mask。
- 模型：Mask R-CNN R-50-FPN，ONNX opset 12，INT8。
- 下载：`https://huggingface.co/onnxmodelzoo/MaskRCNN-12-int8/resolve/main/MaskRCNN-12-int8.onnx`
- 固定 SHA-256：`4409935e855719fd6cd986f7ec2a3de840d0bd9c9cf7a0cba84ce95377f5b476`
- 文件大小：45,769,352 bytes。
- 上游说明：ONNX Model Zoo 的 Mask R-CNN 模型卡；输入为 BGR CHW，短边缩放到
  800、长边不超过 1333、减均值后补齐到 32 的倍数；输出为 boxes、labels、scores、
  28×28 instance masks。
- 类别边界：只覆盖 COCO 80 类，不包含服装、天空等产品语义类别。
- 许可证初步结论：具体模型卡写 MIT；Hugging Face 镜像元数据写 Apache-2.0，存在
  元数据不一致。两者都是宽松许可证，但正式发布前仍需保留上游许可证文本并人工复核
  模型权重、训练数据与镜像声明。
- 当前执行后端：`CPUExecutionProvider`。INT8 版本比 fp32 小，且上游报告精度下降较小；
  Apple Silicon 上的实际速度和命中证据见 `docs/development/M0_02_RESULTS.md`。

## MODNet photographic portrait matting

- 用途：M0-03 人物精细抠图与连续 Alpha。
- 下载：MODNet 官方仓库 ONNX 文档所列的 official Image Matting Model。
- 固定 SHA-256：`07c308cf0fc7e6e8b2065a12ed7fc07e1de8febb7dc7839d7b7f15dd66584df9`。
- 文件大小：25,888,640 bytes。
- 输入输出：RGB NCHW、归一化到 `[-1, 1]`；输出单通道连续 Alpha。
- 许可证初步结论：官方仓库声明其中代码、模型和 demo（`doc/gif` 除外）为 Apache-2.0；
  ONNX 导出由社区贡献并收录在官方仓库。正式发布仍需随包保留许可证与来源记录。
- 能力边界：这是人物模型。动物简单样例的正向结果不能替代动物专用评测；玻璃透明瓶
  已在 M0-03 明确失败。
- 当前执行后端：`CPUExecutionProvider`；Apple Silicon 的证据见
  `docs/development/M0_03_RESULTS.md`。
