@echo off
rem FinalExam01 전략 시뮬레이터 빌드 (Visual Studio x64 Native Tools 프롬프트에서 실행). 일반 상대 풀만 사용.
rem 이전 조 코드(old0~old5)까지 쓰려면 WSL/리눅스에서 build.sh 를 쓰세요. (주의: 이 .bat 은 작성 환경에서 직접 실행해 보지 못했습니다)
setlocal
cd /d "%~dp0"
if not exist build mkdir build
set CORE=..\FinalExam01
cl /nologo /O2 /W3 /utf-8 /std:c17 /D_CRT_SECURE_NO_WARNINGS /I%CORE% /Fobuild\ /Fe:build\sim.exe sim.c opponents.c %CORE%\game.c %CORE%\snapshot.c %CORE%\diagnostics.c %CORE%\player4.c
if errorlevel 1 exit /b 1
echo 빌드 완료: build\sim.exe   (실행 예: build\sim.exe --games 50 --seed 1)
