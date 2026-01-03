@echo off
set EXE_NAME=arena.exe
set LOG_FILE=resultat_match.txt
set INPUT_TMP=inputs.tmp

REM --- 1. COMPILATION ---
echo [1/3] Compilation (Optimisee)...
g++ -std=c++17 -O3 -I src src/main.cpp -o %EXE_NAME%

if %errorlevel% neq 0 (
    echo ERREUR DE COMPILATION.
    exit /b 1
)

REM --- 2. MENU INTERACTIF (Le terminal te parle) ---
echo.
echo ==========================================
echo      CONFIGURATION DU MATCH
echo ==========================================
echo 1. Humain vs IA
echo 2. IA vs IA (1 Match - Detail)
echo 3. IA vs IA (Benchmark 100 Matchs)
echo.

set /p MODE=">> Quel Mode choisissez-vous ? (1-3) : "

REM Si c'est Humain (Mode 1), on ne peut pas rediriger vers un fichier 
REM sinon tu ne verras pas le plateau pour jouer !
if "%MODE%"=="1" goto run_human

REM Si c'est IA vs IA (Mode 2 ou 3), on demande les versions
echo.
set /p V1=">> Version de l'IA Joueur 1 (ex: 5) : "
set /p V2=">> Version de l'IA Joueur 2 (ex: 6) : "

REM --- 3. CREATION DES ENTREES AUTOMATIQUES ---
REM On ecrit tes reponses dans un fichier temporaire
REM Le format correspond a ce que ton C++ attend (Mode, puis V1, puis V2)
(
echo %MODE%
echo %V1%
echo %V2%
) > %INPUT_TMP%

REM --- 4. LANCEMENT AVEC SAUVEGARDE ---
echo.
echo [2/3] Calcul en cours...
echo Les IA jouent, veuillez patienter...

REM L'astuce est la : on injecte le fichier inputs.tmp (<) et on sauve la sortie (>)
%EXE_NAME% < %INPUT_TMP% > %LOG_FILE%

echo.
echo [3/3] Termine !
echo ==========================================
echo Resultats sauvegardes dans %LOG_FILE%
echo Apercu des resultats finaux :
echo ==========================================
powershell -Command "Get-Content %LOG_FILE% -Tail 15"

REM Nettoyage du fichier temporaire
del %INPUT_TMP%
goto end

:run_human
echo.
echo [Lancement Mode Humain]
echo Pas de sauvegarde fichier pour le mode manuel.
%EXE_NAME%
goto end

:end
pause