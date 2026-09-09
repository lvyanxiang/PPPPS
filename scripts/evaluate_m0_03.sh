#!/bin/bash

set -u -o pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
mask_model_path="${1:-$project_root/poc/models/MaskRCNN-12-int8.onnx}"
matting_model_path="${2:-$project_root/poc/models/modnet_photographic_portrait_matting.onnx}"
probe_binary="$project_root/build/macos-homebrew-debug/bin/pppps_matting_probe"
case_manifest="$project_root/poc/evaluation/m0_03_cases.csv"
corpus_manifest="$project_root/poc/evaluation/corpus_manifest.csv"
data_directory="$project_root/poc/evaluation/data"
commit="$(git -C "$project_root" rev-parse --short HEAD 2>/dev/null || echo no-git)"
result_directory="$project_root/poc-results/$(date -u +%Y%m%dT%H%M%SZ)-$commit-m0-03"
summary="$result_directory/summary.csv"

for required_file in "$mask_model_path" "$matting_model_path" "$probe_binary" "$case_manifest"; do
    if [[ ! -e "$required_file" ]]; then
        echo "错误：缺少 ${required_file}，请先运行 make build。"
        exit 2
    fi
done

mkdir -p "$data_directory" "$result_directory"
echo "case_id,evaluation_area,mask_label,model_scope,soft_pixels,soft_percent,mask_count,threshold_iou,mean_absolute_difference,inference_ms,total_ms,technical_status" > "$summary"
technical_failures=0

while IFS=, read -r case_id evaluation_area mask_label model_scope expected_observation; do
    if [[ "$case_id" == "case_id" || -z "$case_id" ]]; then
        continue
    fi

    manifest_values="$(awk -F, -v id="$case_id" '$1 == id { print $11 "," $12; exit }' "$corpus_manifest")"
    IFS=, read -r image_url image_sha <<< "$manifest_values"
    if [[ -z "$image_url" || -z "$image_sha" ]]; then
        echo "${case_id}：清单缺少下载地址或 SHA-256。"
        technical_failures=$((technical_failures + 1))
        continue
    fi

    image_path="$data_directory/$case_id.jpg"
    actual_sha=""
    if [[ -f "$image_path" ]]; then
        actual_sha="$(shasum -a 256 "$image_path" | awk '{print $1}')"
    fi
    if [[ "$actual_sha" != "$image_sha" ]]; then
        temporary="$image_path.download"
        echo "${case_id}：下载并校验测试图片"
        if ! curl --fail --location --retry 3 --continue-at - --output "$temporary" "$image_url"; then
            technical_failures=$((technical_failures + 1))
            continue
        fi
        actual_sha="$(shasum -a 256 "$temporary" | awk '{print $1}')"
        if [[ "$actual_sha" != "$image_sha" ]]; then
            echo "${case_id}：SHA-256 校验失败。"
            rm -f "$temporary"
            technical_failures=$((technical_failures + 1))
            continue
        fi
        mv "$temporary" "$image_path"
    fi

    case_directory="$result_directory/$case_id"
    if ! "$probe_binary" \
        --model "$matting_model_path" \
        --mask-model "$mask_model_path" \
        --mask-label "$mask_label" \
        --image "$image_path" \
        --output-dir "$case_directory" \
        > "$result_directory/$case_id.stdout.json"; then
        echo "${case_id}：真实推理失败。"
        technical_failures=$((technical_failures + 1))
        continue
    fi

    result_json="$case_directory/result.json"
    soft_pixels="$(sed -n 's/.*"soft_pixels": \([0-9]*\).*/\1/p' "$result_json" | head -n 1)"
    image_width="$(sed -n 's/.*"image_width": \([0-9]*\).*/\1/p' "$result_json" | head -n 1)"
    image_height="$(sed -n 's/.*"image_height": \([0-9]*\).*/\1/p' "$result_json" | head -n 1)"
    mask_count="$(sed -n 's/.*"binary_mask_count": \([0-9]*\).*/\1/p' "$result_json" | head -n 1)"
    threshold_iou="$(sed -n 's/.*"threshold_iou": \([0-9.eE+-]*\).*/\1/p' "$result_json" | head -n 1)"
    mean_difference="$(sed -n 's/.*"mean_absolute_difference": \([0-9.eE+-]*\).*/\1/p' "$result_json" | head -n 1)"
    inference_ms="$(sed -n 's/.*"inference_ms": \([0-9]*\).*/\1/p' "$result_json" | head -n 1)"
    total_ms="$(sed -n 's/.*"total_ms": \([0-9]*\).*/\1/p' "$result_json" | head -n 1)"
    soft_percent="$(awk -v soft="$soft_pixels" -v width="$image_width" -v height="$image_height" 'BEGIN { printf "%.2f", soft * 100 / (width * height) }')"
    mask_count="${mask_count:-0}"
    threshold_iou="${threshold_iou:-NA}"
    mean_difference="${mean_difference:-NA}"

    echo "$case_id,$evaluation_area,$mask_label,$model_scope,$soft_pixels,$soft_percent,$mask_count,$threshold_iou,$mean_difference,$inference_ms,$total_ms,ok" >> "$summary"
    echo "${case_id}：完成（${evaluation_area}，软透明像素 ${soft_percent}%）"
done < "$case_manifest"

echo "M0-03 真实推理完成；技术错误 ${technical_failures}。"
echo "请人工查看各案例的 alpha.png、checkerboard.png 与 binary-reference-mask.png。"
echo "结果目录：${result_directory}"
if [[ "$technical_failures" -ne 0 ]]; then
    exit 3
fi
