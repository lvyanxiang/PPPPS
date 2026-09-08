#!/usr/bin/env python3
"""Report PPPPS milestone progress from the repository source-of-truth docs."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


CHECKBOX_RE = re.compile(r"^\s*-\s+\[([ xX])\]\s+(.*)$")
SECTION_RE = re.compile(r"^##\s+(.+)$")
STATUS_FIELD_RE = re.compile(
    r"^-\s+(里程碑|当前节点|下一任务|最近完成)：\s*(.*)$"
)


def find_repo_root() -> Path:
    here = Path(__file__).resolve()
    for candidate in here.parents:
        if (candidate / "docs/development/STATUS.md").is_file() and (
            candidate / "docs/development/ROADMAP.md"
        ).is_file():
            return candidate
    raise SystemExit("无法定位包含 docs/development 的 PPPPS 仓库根目录")


def read_status(path: Path) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        match = STATUS_FIELD_RE.match(line.strip())
        if match:
            fields[match.group(1)] = match.group(2).strip()
    return fields


def read_roadmap(path: Path) -> tuple[dict[str, dict[str, object]], list[dict[str, object]]]:
    milestones: dict[str, dict[str, object]] = {}
    tasks: list[dict[str, object]] = []
    current = "未分组"

    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        heading = SECTION_RE.match(line)
        if heading:
            current = heading.group(1).strip()
            continue

        checkbox = CHECKBOX_RE.match(line)
        if not checkbox:
            continue

        done = checkbox.group(1).lower() == "x"
        task = {"milestone": current, "done": done, "text": checkbox.group(2).strip(), "line": number}
        tasks.append(task)
        summary = milestones.setdefault(
            current, {"done": 0, "total": 0, "first_open": None}
        )
        summary["total"] = int(summary["total"]) + 1
        if done:
            summary["done"] = int(summary["done"]) + 1
        elif summary["first_open"] is None:
            summary["first_open"] = task

    return milestones, tasks


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="以 JSON 输出")
    args = parser.parse_args()

    root = find_repo_root()
    status = read_status(root / "docs/development/STATUS.md")
    milestones, tasks = read_roadmap(root / "docs/development/ROADMAP.md")
    total = len(tasks)
    done = sum(1 for task in tasks if task["done"])
    active_code_match = re.search(r"\bM\d+\b", status.get("里程碑", ""))
    active_code = active_code_match.group(0) if active_code_match else None
    active_heading = next(
        (heading for heading in milestones if active_code and heading.startswith(active_code)),
        None,
    )
    active_summary = milestones.get(active_heading) if active_heading else None

    payload = {
        "repository": str(root),
        "status": status,
        "overall": {"done": done, "total": total},
        "active_milestone": active_heading,
        "active_progress": active_summary,
        "milestones": milestones,
    }

    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
        return

    print(f"仓库：{root}")
    for field in ("里程碑", "当前节点", "下一任务", "最近完成"):
        print(f"{field}：{status.get(field, '未记录')}")
    print(f"总进度：{done}/{total}")
    if active_heading and active_summary:
        print(
            f"当前里程碑进度：{active_summary['done']}/{active_summary['total']} "
            f"({active_heading})"
        )
        first_open = active_summary["first_open"]
        if first_open:
            print(f"首个未完成项（ROADMAP.md:{first_open['line']}）：{first_open['text']}")
    print("各里程碑：")
    for heading, summary in milestones.items():
        print(f"- {heading}: {summary['done']}/{summary['total']}")


if __name__ == "__main__":
    main()
