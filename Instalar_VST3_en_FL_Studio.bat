@echo off
title Instalador VST3 para FL Studio - Phoenix II (by papielguapo)
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Solicitando permisos de Administrador para copiar a C:\Program Files\Common Files\VST3...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo ======================================================================
echo  Instalando Phoenix II (by papielguapo) en Common Files\VST3...
echo ======================================================================
copy /y "%~dp0PAPIELGUAPO - Phoenix II.vst3" "C:\Program Files\Common Files\VST3\PhoenixII_papielguapo.vst3"
copy /y "%~dp0PAPIELGUAPO - Phoenix II.vst3" "C:\Program Files\Common Files\VST3\PAPIELGUAPO - Phoenix II.vst3"
echo.
echo [OK] Plugin VST3 (9.71 MB - JUCE Native GUI + 7 Skins + 1:1 DSP) instalado correctamente!
echo Ya puedes cargarlo en FL Studio.
pause
