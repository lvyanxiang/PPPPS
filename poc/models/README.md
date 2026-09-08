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

