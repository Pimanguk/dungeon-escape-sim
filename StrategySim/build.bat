@echo off
rem 시뮬레이터 빌드 (FinalExam01 기준): FinalExam01 코어(game.c, snapshot.c, display.c, player_names.c)를 그대로 링크한다.
rem  - ours  = FinalExam01\player4.c (우리 팀 P4 제출 파일 그대로)
rem  - 상대   = 교수님 성향 AI FinalExam00\player1~5.c (함수 이름만 profN 으로 바꾼 복사본) + opponents.c 의 전략들 + custom\*.c (oldp4.c = 우리 옛 P4)
rem  - FinalExam01\player0,1,2,3,5.c 는 이름표(player_names.c) 때문에 링크만 한다.
rem 프로젝트와 같은 /sdl, _CRT_SECURE_NO_WARNINGS 를 쓴다.
setlocal
cd /d "%~dp0"
call "%~dp0vsenv.bat" || exit /b 1
if not exist obj mkdir obj
set CORE=..\FinalExam01
rem optional: set OUT=other.exe (when sim.exe is running)
if "%OUT%"=="" set OUT=sim.exe
rem 성향 AI 는 obj 로 복사하면서 playerN_choose_command 이름을 profN_choose_command 로 바꾼다(FinalExam01 의 playerN 과 이름이 겹치지 않게).
rem sample_ai.h 도 obj 로 같이 복사한다: obj 에서 컴파일하면 FinalExam00 의 옛 player_api.h 대신 /I 로 준 FinalExam01 헤더를 쓴다(따옴표 include 는 파일 옆 폴더부터 찾는다).
copy /y ..\FinalExam00\sample_ai.h obj\sample_ai.h >nul
for %%i in (1 2 3 4 5) do powershell -NoProfile -Command "(Get-Content -Raw -Encoding UTF8 '..\FinalExam00\player%%i.c') -replace 'player%%i_choose_command','prof%%i_choose_command' | Set-Content -Encoding UTF8 'obj\prof%%i_build.c'"
cl /nologo /O2 /W3 /sdl /utf-8 /std:c17 /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /I%CORE% %* /Foobj\ /Fe:%OUT% sim.c opponents.c sim_io.c ^
  %CORE%\game.c %CORE%\snapshot.c %CORE%\display.c %CORE%\player_names.c ^
  %CORE%\player0.c %CORE%\player1.c %CORE%\player2.c %CORE%\player3.c %CORE%\player4.c %CORE%\player5.c ^
  obj\prof1_build.c obj\prof2_build.c obj\prof3_build.c obj\prof4_build.c obj\prof5_build.c custom\*.c
