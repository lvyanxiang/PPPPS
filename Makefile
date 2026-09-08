SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := build

PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
PRESET ?= macos-homebrew-debug
BREW_FORMULAE := cmake qtbase onnxruntime ninja pkgconf
APP_BUNDLE := $(PROJECT_ROOT)/build/macos-homebrew-debug/bin/PPPPS.app
MODEL_DIR := $(PROJECT_ROOT)/poc/models
MODEL_PATH := $(MODEL_DIR)/MaskRCNN-12-int8.onnx
MODEL_URL := https://huggingface.co/onnxmodelzoo/MaskRCNN-12-int8/resolve/main/MaskRCNN-12-int8.onnx
MODEL_SHA256 := 4409935e855719fd6cd986f7ec2a3de840d0bd9c9cf7a0cba84ce95377f5b476
POC_DATA_DIR := $(PROJECT_ROOT)/poc/evaluation/data
POC_IMAGE_PATH := $(POC_DATA_DIR)/OI-07.jpg
POC_IMAGE_URL := https://s3.amazonaws.com/open-images-dataset/validation/f9d2cc52946b9637.jpg
POC_IMAGE_SHA256 := 36f8e3668261cce538133e10aecb3f7afe4714be6500d741f4b04035b1789f5b

.PHONY: help doctor check-platform check-xcode check-brew bootstrap models poc-data configure build test run evaluate-m0-02

help:
	@echo "PPPPS macOS 开发命令"
	@echo "  make doctor     只检查开发环境"
	@echo "  make bootstrap  检查并安装缺失的 Homebrew 依赖"
	@echo "  make models     下载并校验固定版本的 POC 模型"
	@echo "  make build      准备环境和模型、配置并构建项目（默认）"
	@echo "  make test       构建并运行单元、界面和真实模型测试"
	@echo "  make run        构建并启动 PPPPS.app"
	@echo "  make evaluate-m0-02  导出一次真实推理的 JSON、Mask 和叠加图"

check-platform:
	@if [[ "$$(uname -s)" != "Darwin" ]]; then \
		echo "错误：当前 Makefile 只支持 macOS。"; \
		exit 1; \
	fi

check-xcode: check-platform
	@if ! command -v xcode-select >/dev/null 2>&1 || ! xcode-select -p >/dev/null 2>&1; then \
		echo "错误：未安装 Xcode Command Line Tools。"; \
		echo "请先运行：xcode-select --install"; \
		exit 1; \
	fi
	@echo "✓ Xcode Command Line Tools"

check-brew: check-platform
	@if ! command -v brew >/dev/null 2>&1; then \
		echo "错误：未安装 Homebrew。"; \
		echo "请先按照 https://brew.sh/ 的官方说明安装 Homebrew，然后重新执行 make build。"; \
		exit 1; \
	fi
	@echo "✓ Homebrew $$(brew --version | head -n 1)"

doctor: check-xcode check-brew
	@missing=0; \
	for formula in $(BREW_FORMULAE); do \
		if brew list --versions "$$formula" >/dev/null 2>&1; then \
			echo "✓ $$formula $$(brew list --versions "$$formula" | cut -d' ' -f2-)"; \
		else \
			echo "✗ $$formula 未安装"; \
			missing=1; \
		fi; \
	done; \
	if [[ "$$missing" -ne 0 ]]; then \
		echo "环境不完整：运行 make bootstrap 可自动安装缺失依赖。"; \
		exit 1; \
	fi

bootstrap: check-xcode check-brew
	@missing=(); \
	for formula in $(BREW_FORMULAE); do \
		if brew list --versions "$$formula" >/dev/null 2>&1; then \
			echo "✓ $$formula 已安装"; \
		else \
			echo "→ $$formula 缺失，加入安装列表"; \
			missing+=("$$formula"); \
		fi; \
	done; \
	if [[ "$${#missing[@]}" -gt 0 ]]; then \
		echo "正在通过 Homebrew 安装：$${missing[*]}"; \
		HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1 brew install "$${missing[@]}"; \
	else \
		echo "所有 Homebrew 依赖均已就绪。"; \
	fi

models:
	@mkdir -p "$(MODEL_DIR)"; \
	actual=""; \
	if [[ -f "$(MODEL_PATH)" ]]; then \
		actual="$$(shasum -a 256 "$(MODEL_PATH)" | awk '{print $$1}')"; \
	fi; \
	if [[ "$$actual" == "$(MODEL_SHA256)" ]]; then \
		echo "✓ MaskRCNN-12-int8.onnx 已下载且校验通过"; \
	else \
		temporary="$(MODEL_PATH).download"; \
		echo "→ 下载 Mask R-CNN int8 POC 模型（约 46 MB）"; \
		curl --fail --location --retry 3 --continue-at - --output "$$temporary" "$(MODEL_URL)"; \
		actual="$$(shasum -a 256 "$$temporary" | awk '{print $$1}')"; \
		if [[ "$$actual" != "$(MODEL_SHA256)" ]]; then \
			echo "错误：模型 SHA-256 校验失败，实际为 $$actual"; \
			rm -f "$$temporary"; \
			exit 1; \
		fi; \
		mv "$$temporary" "$(MODEL_PATH)"; \
		echo "✓ 模型下载与 SHA-256 校验完成"; \
	fi

poc-data:
	@mkdir -p "$(POC_DATA_DIR)"; \
	actual=""; \
	if [[ -f "$(POC_IMAGE_PATH)" ]]; then \
		actual="$$(shasum -a 256 "$(POC_IMAGE_PATH)" | awk '{print $$1}')"; \
	fi; \
	if [[ "$$actual" == "$(POC_IMAGE_SHA256)" ]]; then \
		echo "✓ OI-07 真实模型测试图片已校验"; \
	else \
		temporary="$(POC_IMAGE_PATH).download"; \
		echo "→ 下载 Open Images OI-07 真实模型测试图片"; \
		curl --fail --location --retry 3 --output "$$temporary" "$(POC_IMAGE_URL)"; \
		actual="$$(shasum -a 256 "$$temporary" | awk '{print $$1}')"; \
		if [[ "$$actual" != "$(POC_IMAGE_SHA256)" ]]; then \
			echo "错误：OI-07 SHA-256 校验失败，实际为 $$actual"; \
			rm -f "$$temporary"; \
			exit 1; \
		fi; \
		mv "$$temporary" "$(POC_IMAGE_PATH)"; \
		echo "✓ OI-07 下载与 SHA-256 校验完成"; \
	fi

configure: bootstrap models poc-data
	@qt_prefix="$$(brew --prefix qtbase)"; \
	ort_prefix="$$(brew --prefix onnxruntime)"; \
	cmake --preset "$(PRESET)" \
		-DCMAKE_PREFIX_PATH="$$qt_prefix;$$ort_prefix" \
		-DPPPPS_MASK_RCNN_MODEL="$(MODEL_PATH)"

build: configure
	@cmake --build --preset "$(PRESET)" --parallel

test: build
	@ctest --preset "$(PRESET)" --output-on-failure

run: build
	@if [[ ! -d "$(APP_BUNDLE)" ]]; then \
		echo "错误：未找到应用 $(APP_BUNDLE)"; \
		exit 1; \
	fi
	@open "$(APP_BUNDLE)"

evaluate-m0-02: test
	@bash "$(PROJECT_ROOT)/scripts/evaluate_m0_02.sh" "$(MODEL_PATH)"
