@echo off
setlocal
if "%~1"=="" (
    echo ========================================================================
    echo             GENOME FIREWALL v2.0 - FAST SCAN LAUNCHER
    echo ========================================================================
    echo Usage: firewall-scan.bat ^<genome.fasta^> [markers.csv]
    echo.
    echo Examples:
    echo   firewall-scan.bat data\sample_real.fasta
    echo   firewall-scan.bat data\sample.fasta data\markers.csv
    exit /b 1
)

set GENOME=%~1
set DB=%~2
if "%DB%"=="" set DB=data\markers_card_top.csv

echo [*] Launching Genome Firewall Scan on %GENOME% ...
echo [*] Database: %DB%
genome_firewall.exe --input "%GENOME%" --db "%DB%" --fuzzy --async
endlocal
