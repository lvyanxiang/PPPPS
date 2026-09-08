SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := build

PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
PRESET ?= macos-homebrew-debug
BREW_FORMULAE := cmake qtbase onnxruntime ninja pkgconf
APP_BUNDLE := $(PROJECT_ROOT)/build/macos-homebrew-debug/bin/PPPPS.app

.PHONY: help doctor check-platform check-xcode check-brew bootstrap configure build test run

help:
	@echo "PPPPS macOS 开发命令"
	@echo "  make doctor     只检查开发环境"
	@echo "  make bootstrap  检查并安装缺失的 Homebrew 依赖"
	@echo "  make build      准备环境、配置并构建项目（默认）"
	@echo "  make test       构建并运行自动化测试"
	@echo "  make run        构建并启动 PPPPS.app"

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

configure: bootstrap
	@qt_prefix="$$(brew --prefix qtbase)"; \
	ort_prefix="$$(brew --prefix onnxruntime)"; \
	cmake --preset "$(PRESET)" -DCMAKE_PREFIX_PATH="$$qt_prefix;$$ort_prefix"

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

