# POC Evaluation Corpus

`corpus_manifest.csv` 是 M0 V0.1 的固定案例清单。测试图片、数据集、模型和结果不进
Git；把本地输入放在 `poc/evaluation/data/`，把运行结果放在 `poc-results/`。

执行规则：

1. 先按清单的 `download_url` 获取具体文件，校验 `sha256`；
2. 数据集范围项按 `source_id_or_rule` 确定性选样，随后把逐文件路径与 SHA-256
   补入新版本清单；
3. `blocked_license_review` 不得下载，`noncommercial_only` 只能用于隔离的本地
   非商业研究；
4. `license_review` 可以用于本地取样，但在逐图归属复核前不得提交、发布或训练
   可分发模型；
5. 评测必须引用 `case_id`，不能用临时文件名代替稳定身份。

完整指标、阈值、硬件和结果格式见
`docs/development/POC_EVALUATION_PLAN.md`。

M0-02 的 13 个固定点击点在 `m0_02_click_probes.csv`；运行
`make evaluate-m0-02` 会下载并校验对应图片、执行真实 Mask R-CNN 推理，并把汇总、
JSON、逐对象 Mask 和轮廓叠加图写入被忽略的 `poc-results/`。
