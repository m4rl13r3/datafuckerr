@echo off
chcp 65001 >nul
setlocal
where py >nul 2>nul
if %errorlevel% equ 0 (
    py -3 -c "import sys, PySide6; raise SystemExit(sys.version_info ^< (3, 10))" >nul 2>nul
    if errorlevel 1 (
        echo Erreur : datafuckerr exige Python 3.10 ou supérieur et PySide6. 1>&2
        echo Installez l’interface avec : py -3 -m pip install -r "%~dp0requirements.txt" 1>&2
        exit /b 1
    )
    py -3 "%~dp0datafuckerr_qt.py" %*
) else (
    python -c "import sys, PySide6; raise SystemExit(sys.version_info ^< (3, 10))" >nul 2>nul
    if errorlevel 1 (
        echo Erreur : datafuckerr exige Python 3.10 ou supérieur et PySide6. 1>&2
        echo Installez l’interface avec : python -m pip install -r "%~dp0requirements.txt" 1>&2
        exit /b 1
    )
    python "%~dp0datafuckerr_qt.py" %*
)
endlocal
