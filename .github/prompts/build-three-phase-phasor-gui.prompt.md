---
name: Build Three-Phase Phasor GUI
description: Create or extend a native C++ Win32 three-phase phasor GUI built with CMake.
argument-hint: Optional behavior or constraints
agent: agent
---

Build or extend a native Windows desktop application that displays and edits a three-phase phasor diagram. Use C++17, CMake, and the Win32 API/GDI, with no external GUI dependency unless the existing project already requires one.

- Inspect workspace instructions, source files, and build configuration first. Keep compatible existing project conventions, but retain C++ and CMake.
- Put the application CMake project file at `source/CMakeLists.txt`, with its C++ entry point and related sources alongside it. Keep executable source paths relative to that CMake file. Use an out-of-source build directory.
- For a new project, create a Unicode Win32 GUI executable, target C++17, link `user32` and `gdi32`, and enable useful MSVC warnings. Prevent Windows `min`/`max` macros from interfering with standard-library calls.
- Provide editable magnitude and phase-angle fields for phases A, B, and C. Use degrees, with 0 degrees pointing right and positive angles counterclockwise. Start values at zero. Validate magnitudes in the range 0 to 1e12 and phase angles in the range -360000 to 360000 degrees; invalid or incomplete input must show a clear status and must not leave stale vectors visible.
- Add a separate diagram-rotation input in degrees, initially zero. The displayed angle for each vector is its entered phase angle plus the diagram rotation, normalized modulo 360 degrees. Editing any value updates the plot immediately.
- Color phase A red, phase B yellow, and phase C blue. Use each phase color consistently for its input label, vector, endpoint, and plot label.
- Add a `Balance Phases` button for the positive ABC sequence. When clicked with valid inputs, use phase A's magnitude and angle as the reference, set all three magnitudes equal to phase A, set phase B to A minus 120 degrees and phase C to A plus 120 degrees (wrap the resulting input angles into the supported range), and leave diagram rotation unchanged. Show a concise confirmation in the status area.
- Draw labeled, arrowed vectors on a readable, autoscaled complex-plane plot. Explicitly invalidate/redraw the plot when it is resized, including when the main window is maximized.
- Keep controls usable at the minimum window size and follow existing code style. For validation, configure CMake from `source` into a fresh build directory if an existing cache points to another source path; do not delete or overwrite that cache. Build the Release target with the available Windows C++ toolchain.
- Report the changed files, exact configure/build commands, executable location, and any requirements that were not verified.

Additional request: ${input:request:Optional behavior or constraints}