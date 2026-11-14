@echo off
rem ===========================================================================
rem  Rebuild.bat - double-click AFTER closing Unreal.
rem  Compiles the game C++ code (new classes need this; Live Coding cannot add them),
rem  writes the log to Saved\rebuild_log.txt (so Claude can read errors), then opens
rem  the project.
rem ===========================================================================
cd /d "%~dp0.."
if not exist Saved mkdir Saved
echo Building Grapple Runner (1-5 minutes)...
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" GrappleRunnerEditor Win64 Development -Project="%CD%\GrappleRunner.uproject" -WaitMutex > "Saved\rebuild_log.txt" 2>&1
if errorlevel 1 (
  findstr /c:"Live Coding is active" "Saved\rebuild_log.txt" >nul && echo. && echo UNREAL IS STILL OPEN somewhere. Close every Unreal window (check the taskbar and Task Manager for UnrealEditor), then run this again. && pause && exit /b 1
  type "Saved\rebuild_log.txt" | findstr /i "error"
  echo.
  echo BUILD FAILED. Tell Claude "build failed" - the log is in Saved\rebuild_log.txt
  pause
  exit /b 1
)
echo Built. Opening Unreal...
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%CD%\GrappleRunner.uproject"
