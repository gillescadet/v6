@echo off
echo V6 Project Maker

del bin\make_project.* 2> nul
cl source\v6\doc\make_project.cpp /nologo /Fobin\make_project /Febin\make_project > "bin\make_project.log"
if not exist "bin\make_project.exe" goto compilation_error

call "bin\make_project.exe" v6 2015

echo Done
goto end

:compilation_error
echo Compilation error
more "bin\make_project.log"
goto end

:end