@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "C:\Users\anton\Desktop\PhoenixII-Clone"

echo [1/3] Compiling Standalone Executable (PhoenixII_papielguapo.exe)...
cl.exe /nologo /O2 /EHsc /std:c++17 PhoenixStandalone.cpp shell32.lib user32.lib /Fe:PhoenixII_papielguapo.exe
if errorlevel 1 exit /b 1

echo [2/3] Compiling 64-bit VST3 Plug-In Binary (PhoenixII_papielguapo.vst3)...
if not exist "PhoenixII_papielguapo.vst3\Contents\x86_64-win" mkdir "PhoenixII_papielguapo.vst3\Contents\x86_64-win"
if not exist "PhoenixII_papielguapo.vst3\Contents\Resources" mkdir "PhoenixII_papielguapo.vst3\Contents\Resources"
cl.exe /nologo /O2 /EHsc /std:c++17 /LD PhoenixVST3.cpp /Fe:"PhoenixII_papielguapo.vst3\Contents\x86_64-win\PhoenixII_papielguapo.vst3"
if errorlevel 1 exit /b 1

copy /y "PhoenixVST3.dll" "PhoenixII_papielguapo_DSP.dll" >nul 2>nul
del /q *.obj *.exp *.lib >nul 2>nul

echo [3/3] Running Bit-Exact DSP Verification...
PhoenixII_papielguapo.exe --verify
