@echo off
rem Visual Studio C++ 컴파일러(cl) 환경을 자동으로 찾아 설정한다. build.bat / build_probe.bat 이 호출한다.
rem 1) 이미 cl 이 잡혀 있으면 아무것도 안 한다.
rem 2) vswhere 로 설치된 최신 Visual Studio(C++ 도구 포함)를 찾는다. (버전/에디션 무관)
rem 3) 그래도 없으면 흔한 설치 경로를 차례로 확인한다.
where cl >nul 2>nul && exit /b 0
set "VSROOT="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if defined VSROOT if exist "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul && exit /b 0
for %%v in (18 2022 2019 2017) do for %%e in (Community Professional Enterprise BuildTools) do (
    where cl >nul 2>nul || if exist "%ProgramFiles%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvars64.bat" call "%ProgramFiles%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvars64.bat" >nul
)
where cl >nul 2>nul && exit /b 0
echo.
echo [오류] Visual Studio C++ 컴파일러(cl)를 찾지 못했습니다.
echo        Visual Studio Installer 에서 "C++를 사용한 데스크톱 개발" 워크로드가 설치되어 있는지 확인하세요.
exit /b 1
