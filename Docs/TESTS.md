# Grapple Runner: Test Log

<!--
HOW TO READ THIS (no coding experience needed)
This file is the lab notebook for the game. Each test follows the scientific method:
  Question    - what we want to find out
  Hypothesis  - what we expect to happen, and why, written BEFORE running the test
  Method      - exact steps, so anyone can repeat the test
  Variables   - what we changed (independent), what we measured (dependent),
                and what we kept the same (controlled)
  Results     - what actually happened, with numbers where possible
  Evidence    - screenshots, recordings or logs in Docs/media/tests/
  Conclusion  - did the result support the hypothesis? what changes next?
A test PASSES when the result matches the acceptance criteria written in advance.
-->

## Test environment (applies to every test unless stated otherwise)

| Item | Value |
|---|---|
| Engine | Unreal Engine 5.8.4 (VR template, forward shading) |
| Compiler | MSVC 19.44.35229 (Visual Studio 2022 toolset 14.44.35207), Windows SDK 10.0.22621.0 |
| CPU / RAM | 12 physical / 20 logical cores, 31.8 GB RAM |
| Level | `L_FloatingIslands_2` (Brushify Floating Islands, 14 landing islands, tagged grapple anchors) |
| Mode | Desktop play-in-editor (no headset available 8 to 22 Nov 2026) |

## Summary

| ID | Date | Test | Result |
|---|---|---|---|
| T-01 | 2026-11-08 | C++ grapple module compiles and loads in UE 5.8 | PASS |

---

## T-01: C++ module build

**Question.** Does the hand-written C++ grapple code (`GrappleRig.cpp`, about 600 lines) compile against Unreal Engine 5.8 and load into the editor?

**Hypothesis.** It will compile with no errors once a compiler version that UE 5.8 accepts is installed. (The first attempt on 7 Nov failed before compiling any code: UE 5.8 refuses MSVC 14.44.0 to 14.44.35210 because of a known compiler bug, and 14.36 was too old.)

**Acceptance criteria.** Unreal Build Tool reports `Result: Succeeded` with 0 errors, and the editor opens the level with the `GrappleRunner` module loaded.

**Method.**
1. Updated the Visual Studio installs (added MSVC 14.42, and Visual Studio 2026 with MSVC 14.51). Unreal Build Tool then chose the updated 14.44 compiler, build 35229, which is past the banned range.
2. Opened the project from the Epic Games Launcher (Library, then My Projects, then GrappleRunner).
3. Unreal reported "Missing GrappleRunner Modules, rebuild them now?" Answered Yes.
4. Read the build section of `Saved/Logs/GrappleRunner.log`.

**Variables.**
- Independent: compiler toolset version.
- Dependent: build result, error and warning count, build time.
- Controlled: same source code (git commit `0dc77be`), engine version and machine.

**Results.**

| Measure | Value |
|---|---|
| Build actions | 9 (6 compile, 2 link, 1 metadata) |
| Errors | 0 |
| Compiler warnings | 0 |
| Total build time | 89.2 s (73.0 s in Unreal Build Accelerator) |
| Output | `UnrealEditor-GrappleRunner.dll` (227 KB) |
| Editor startup map check | 0 errors, 0 warnings |

**Evidence.**
- Build log excerpt: [`media/tests/T01_compile_log.txt`](media/tests/T01_compile_log.txt)
- Editor open on the level after the build: [`media/tests/T01_editor_loaded_after_compile.jpg`](media/tests/T01_editor_loaded_after_compile.jpg)

**Conclusion.** PASS. The hypothesis was supported: the code itself had no errors, and the 7 Nov failure was caused only by the toolchain. Lesson recorded: check the engine's supported compiler range before the first build (UE 5.8 needs MSVC 14.38 or newer, excluding 14.44.0 to 14.44.35210).
