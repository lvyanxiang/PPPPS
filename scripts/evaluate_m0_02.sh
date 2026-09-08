#!/bin/bash

set -u -o pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
model_path="${1:-$project_root/poc/models/MaskRCNN-12-int8.onnx}"
probe_binary="$project_root/build/macos-homebrew-debug/bin/pppps_segmentation_probe"
probe_manifest="$project_root/poc/evaluation/m0_02_click_probes.csv"
corpus_manifest="$project_root/poc/evaluation/corpus_manifest.csv"
data_directory="$project_root/poc/evaluation/data"
commit="$(git -C "$project_root" rev-parse --short HEAD 2>/dev/null || echo no-git)"
result_directory="$project_root/poc-results/$(date -u +%Y%m%dT%H%M%SZ)-$commit-m0-02"
summary="$result_directory/summary.csv"

if [[ ! -f "$model_path" ]]; then
    echo "错误：找不到模型 ${model_path}，请先运行 make models。"
    exit 2
fi
if [[ ! -x "$probe_binary" ]]; then
    echo "错误：找不到推理工具 ${probe_binary}，请先运行 make build。"
    exit 2
fi

mkdir -p "$data_directory" "$result_directory"
echo "probe_id,case_id,object_type_zh,expected_model_label,hit,actual_label,inference_ms,total_ms,technical_status" > "$summary"

hits=0
misses=0
technical_failures=0

while IFS=, read -r probe_id case_id image_id object_type expected_label probe_x probe_y difficulty ground_truth; do
    if [[ "$probe_id" == "probe_id" || -z "$probe_id" ]]; then
        continue
    fi

    manifest_values="$(awk -F, -v id="$case_id" '$1 == id { print $11 "," $12; exit }' "$corpus_manifest")"
    IFS=, read -r image_url image_sha <<< "$manifest_values"
    if [[ -z "$image_url" || -z "$image_sha" ]]; then
        echo "${probe_id}：清单中缺少 ${case_id} 的下载地址或 SHA-256。"
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
        echo "${probe_id}：下载并校验 ${case_id}"
        if ! curl --fail --location --retry 3 --continue-at - --output "$temporary" "$image_url"; then
            technical_failures=$((technical_failures + 1))
            continue
        fi
        actual_sha="$(shasum -a 256 "$temporary" | awk '{print $1}')"
        if [[ "$actual_sha" != "$image_sha" ]]; then
            echo "${probe_id}：${case_id} SHA-256 校验失败。"
            rm -f "$temporary"
            technical_failures=$((technical_failures + 1))
            continue
        fi
        mv "$temporary" "$image_path"
    fi

    case_directory="$result_directory/$probe_id"
    command=(
        "$probe_binary"
        --model "$model_path"
        --image "$image_path"
        --probe-x "$probe_x"
        --probe-y "$probe_y"
        --output-dir "$case_directory"
    )
    if [[ -n "$expected_label" ]]; then
        command+=(--expect-label "$expected_label")
    fi

    "${command[@]}" > "$result_directory/$probe_id.stdout.json"
    exit_code=$?
    if [[ "$exit_code" -ne 0 && "$exit_code" -ne 4 ]]; then
        echo "${probe_id}：推理工具执行失败，退出码 ${exit_code}。"
        technical_failures=$((technical_failures + 1))
        continue
    fi

    result_json="$case_directory/result.json"
    hit="$(sed -n -e 's/.*"hit": true.*/true/p' -e 's/.*"hit": false.*/false/p' "$result_json" | head -n 1)"
    actual_label="$(sed -n 's/.*"actual_label": "\([^"]*\)".*/\1/p' "$result_json" | head -n 1)"
    inference_ms="$(sed -n 's/.*"inference_ms": \([0-9]*\).*/\1/p' "$result_json" | head -n 1)"
    total_ms="$(sed -n 's/.*"total_ms": \([0-9]*\).*/\1/p' "$result_json" | head -n 1)"
    if [[ -n "$expected_label" && "$actual_label" != "$expected_label" ]]; then
        hit=false
    fi

    if [[ "$hit" == "true" ]]; then
        hits=$((hits + 1))
        status=hit
    else
        misses=$((misses + 1))
        status=miss
    fi
    echo "$probe_id,$case_id,$object_type,$expected_label,$hit,$actual_label,$inference_ms,$total_ms,$status" >> "$summary"
    echo "${probe_id}：${status}（${object_type}${actual_label:+ → $actual_label}）"
done < "$probe_manifest"

echo "M0-02 探针完成：命中 ${hits}，未命中 ${misses}，技术错误 ${technical_failures}。"
echo "结果目录：${result_directory}"
if [[ "$technical_failures" -ne 0 ]]; then
    exit 3
fi
