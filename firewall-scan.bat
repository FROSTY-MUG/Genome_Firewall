@echo off
setlocal
if "%~1"=="" (
    echo Usage: firewall-scan.bat ^<genome.fasta^>
    exit /b 1
)
set GENOME=%~1
echo [*] Launching Genome Firewall Scan on %GENOME% ...
echo [*] Using trained CARD-based AMR database (455 top markers)
genome_firewall.exe --input "%GENOME%" --db markers_card_top.csv --fuzzy --async
endlocal
