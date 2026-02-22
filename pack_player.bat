@echo off

svn info -r HEAD | findstr Revision > svn_revision.txt
for /f "tokens=2 delims= " %%g in (svn_revision.txt) do set SVN_REVERSION=%%g
del svn_revision.txt

pushd "%~dp0"

set ZIP="C:\Program Files\7-Zip\7z.exe"
set PACKAGE="%~dp0"\package\player_rev%SVN_REVERSION%.zip
set BIN_PATH=bin\Release
set THIRD_PARTY_PATH=thirdparty
set MEDIA_PATH=media

del /Q %PACKAGE% 2> nul

pushd %BIN_PATH%
%ZIP% a %PACKAGE% dragonfly_player.exe
popd

pushd %THIRD_PARTY_PATH%
%ZIP% a %PACKAGE% third_party_notices.txt
popd

%ZIP% -mx0 a %PACKAGE% %MEDIA_PATH%/*.df

popd

echo.
echo Added %PACKAGE%
echo.

pause
