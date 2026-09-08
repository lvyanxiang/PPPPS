---
name: pppps-project
description: Resume, plan, implement, verify, and report the PPPPS Qt desktop image-editor project across sessions. Use when Codex is asked for project progress, to continue development, to work on a roadmap item, or to update milestone state; treat docs/development/STATUS.md and ROADMAP.md as the execution source of truth and keep M0 POC gated before full development.
---

# PPPPS Project

Use this project-local skill as the single entry point for continuing the
object-driven intelligent image editor. Repository documents, not previous
conversation, are the source of truth.

## Context recovery

For a progress request or before substantial implementation:

1. Run `python3 .codex/skills/pppps-project/scripts/project_status.py` from the
   repository root.
2. Read `docs/development/STATUS.md` completely.
3. Read the current milestone section in `docs/development/ROADMAP.md` and any
   directly named source files.
4. Read the relevant document under `docs/project-initiation/` only when the
   task needs its rationale, product boundary, risk details, or full feature
   definition. Do not reload all initiation documents routinely.

Use `STATUS.md` as the short resume pointer and `ROADMAP.md` as the authoritative
scope, task order, checkbox state, and acceptance-gate record. If they disagree,
verify the repository and make them consistent before continuing.

## Continue-development loop

When the user says to continue without selecting a task:

1. Use the milestone and current node from `STATUS.md`.
2. Select its first unfinished roadmap item, unless `Next task` identifies a
   narrower task in that same milestone.
3. Read the smallest useful set of implementation files.
4. Implement one coherent, verifiable slice. Keep unrelated future milestone
   work out of the change.
5. Run focused tests plus the relevant build or benchmark.
6. Mark a roadmap checkbox `[x]` only when the deliverable exists and a
   reasonable verification has run.
7. Rewrite `STATUS.md` with milestone, current node, next task, last completed,
   durable decisions, verification commands/results, and active blockers.

If no executable task is selected, report the confirmed state and ask for a
priority instead of inventing work. If validation cannot run, keep the task
unchecked and record why.

## M0 POC gate

M0 is the first implementation milestone. Do not start M1 or productize the
full feature set until `docs/development/POC_REPORT.md` records a reviewed `GO`.
A `CONDITIONAL GO` authorizes only the additional POC work named in that report.
A `NO-GO` stops implementation and returns to the interaction/product model.

POC demonstrations must execute real detection, segmentation, editing, and
history paths. Label a stub or fixture clearly; never use it as evidence that a
model, performance target, or hardware path works. Record the test corpus,
asset/model licenses, hardware, build type, metrics, failures, and reproduction
steps so a later session can evaluate the same claim.

## Qt and core constraints

- Use Qt 6, C++20, and CMake. Default to Qt Widgets for the professional desktop
  shell and a custom render viewport; adopt QML only after an explicit recorded
  decision supported by a concrete need.
- Keep UI code separate from document, history, image, AI, rendering, storage,
  and platform adapters. Core behavior must be testable without opening a GUI.
- Use OpenCV for mature image primitives and ONNX Runtime for local inference.
  Keep model/provider selection behind interfaces and retain a CPU fallback.
- Start POC rendering behind a replaceable interface. Choose Skia or platform
  GPU backends from measured M0 evidence rather than coupling document logic to
  one renderer.
- Keep source pixels immutable. Persist assets, semantic objects, masks,
  transforms, adjustments, effects, and commands so edits remain
  non-destructive, serializable, replayable, and undoable.
- Never block the Qt GUI thread with decoding, inference, full-resolution
  rendering, export, or disk-heavy work. Provide cancellation, progress, and a
  safe result handoff.
- Treat 4K images, 20 semantic objects, bounded memory, 50-step Undo/Redo, and
  Windows/macOS plus Intel/AMD/NVIDIA compatibility as explicit M0 evidence,
  not aspirational notes.
- Before adding a model, cloud service, font, sample asset, or major library,
  record its source, license, distribution constraints, size, hardware needs,
  and fallback path.

## Validation

Match validation to the change. Prefer:

- CMake configure and build for the active platform;
- `ctest --test-dir <build-dir> --output-on-failure` for core behavior;
- focused Qt UI/integration tests for interactions;
- pixel-diff or Golden tests for masks/compositing;
- release-build benchmarks with recorded hardware for performance claims;
- save/reopen and undo/redo round trips for document changes.

Do not claim cross-platform or GPU compatibility from a single development
machine. Record verified cells and unknown cells separately.

## Documentation language and scope

Write milestone, status, POC report, decisions, and user-facing progress in
Chinese. Code identifiers, commit-friendly technical names, and this skill may
use English. Keep `STATUS.md` concise; it is a resume point, not a changelog.

All researched features already have a milestone in `ROADMAP.md`. New or
changed requirements must receive a stable ID and milestone before
implementation. Deferring a feature changes its order; it does not silently
remove it from total scope.
