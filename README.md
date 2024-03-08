# Raptor's Integration for OpenFPGA

Adds features like a Verilog reader to VPR, a Pin assignment checker, encryption of Architecture file...

[![Build Compatibility](https://github.com/RapidSilicon/OpenFPGA_RS2/actions/workflows/build.yml/badge.svg)](https://github.com/RapidSilicon/OpenFPGA_RS2/actions/workflows/build.yml)

Version: see [`VERSION.md`](VERSION.md)

## Compilation

Please ensure checkout all the submodules using

```
make checkout
```

Then, you can compile the codebase using

```
make release
```

## How to update .diff patches

```
Suppose you want to update the patch for base/SetupVPR.cpp.

1. Type 'make compile'. It will apply the existing patch include/base_fix/DIFF/SetupVPR_cpp.diff
OpenFPGA/vtr-verilog-to-routing/vpr/src/base/SetupVPR.cpp will be modified.

2. Temporarily disable patching this file in CMakeLists.txt:
#apply_patch(${DIFF_FILE}  ${TARGET_DIR}  "base/SetupVPR.cpp")

3. Go to VPR source code and edit SetupVPR.cpp - add your new tweaks.
cd OpenFPGA/vtr-verilog-to-routing/vpr/src/base; <your_editor> SetupVPR.cpp

4. Compile and test VPR.

5. Generate new diff by 'git diff SetupVPR.cpp > SetupVPR_cpp.diff'

6. Copy or move the new diff to include/base_fix/DIFF/SetupVPR_cpp.diff

7. Re-enable patch application in CMakeLists.txt.
This can be done by revering this file by 'git checkout CMakeLists.txt'

8. Check in the updated SetupVPR_cpp.diff
git add include/base_fix/DIFF/SetupVPR_cpp.diff
git commit
git push
```
