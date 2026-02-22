@echo off
echo V6 Project Maker

set VERSION_REV=0

@rem ---- INIT ---
@cd /D %~dp0

del bin\make_project.* 2> nul
cl source\v6\doc\make_project.cpp /nologo /Fobin\make_project /Febin\make_project > "bin\make_project.log"
if not exist "bin\make_project.exe" goto compilation_error

if not exist project\vs2015 mkdir project\vs2015

svn info -r HEAD | findstr Revision > svn_revision.txt
for /f "tokens=2 delims= " %%g in (svn_revision.txt) do set VERSION_REV=%%g
del svn_revision.txt

call "bin\make_project.exe" dragonfly 2015 %VERSION_REV%
start project\vs2015\dragonfly_vs2015.sln

echo Done
goto end

:compilation_error
echo Compilation error
more "bin\make_project.log"
goto end

:end