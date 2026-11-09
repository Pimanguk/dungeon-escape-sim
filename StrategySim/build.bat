@echo off
rem 시뮬레이터 빌드: 실제 FinalExamGameCore\player0.c 를 그대로 링크한다. (프로젝트와 같은 /sdl, _CRT_SECURE_NO_WARNINGS)
setlocal
cd /d "%~dp0"
where cl >nul 2>nul || call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist obj mkdir obj
set CORE=..\FinalExamGameCore
rem optional: set OUT=other.exe (when sim.exe is running), set P0SRC=strategy\player0_v1.c (old version)
if "%OUT%"=="" set OUT=sim.exe
if "%P0SRC%"=="" set P0SRC=%CORE%\player0.c
cl /nologo /O2 /W3 /sdl /utf-8 /std:c17 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /I%CORE% %* /Foobj\ /Fe:%OUT% sim.c opponents.c sim_io.c ^
  %CORE%\game.c %CORE%\snapshot.c %CORE%\display.c %P0SRC% %CORE%\player1.c custom\*.c
