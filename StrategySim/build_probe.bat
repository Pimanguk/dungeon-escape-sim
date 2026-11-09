@echo off
rem 코어 버그 탐침(probe.exe) 빌드. 실제 FinalExamGameCore의 game.c / snapshot.c / player0.c 를 그대로 링크한다.
rem 호출하는 곳: run_menu.ps1 의 메뉴 d.  실행: probe.exe --games 2000 --seed 5
setlocal
cd /d "%~dp0"
where cl >nul 2>nul || call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist obj mkdir obj
set CORE=..\FinalExamGameCore
cl /nologo /O2 /W3 /sdl /utf-8 /std:c17 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /I%CORE% /Foobj\ /Fe:probe.exe bugprobe.c sim_io.c %CORE%\game.c %CORE%\snapshot.c %CORE%\player0.c
