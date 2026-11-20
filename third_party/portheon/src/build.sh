#!/bin/sh
# Cross-compile with llvm-mingw (https://github.com/mstorsjo/llvm-mingw); on Windows run the same commands.
# portheon.c includes lang.c (with lang_data.inc), detect.c, update.c, checks.c, extras.c, pad.c, ui.c (with
# ui_items.c, ui_screens.c and ui_input.c) and online.c.
x86_64-w64-mingw32-windres app.rc -O coff -o app.res
x86_64-w64-mingw32-clang -O2 -municode -mwindows -Wall -o PORTHEON.exe portheon.c app.res \
  -lcomctl32 -lshell32 -lshlwapi -luxtheme -lgdiplus -lole32 -loleaut32 -ldwmapi -lgdi32 -luser32 -ladvapi32 -lmsimg32 -luuid -lwinhttp \
  -lbcrypt -liphlpapi -lws2_32 -lwinmm -lcomdlg32 -s
