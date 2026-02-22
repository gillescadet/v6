set STREAM=D:\dev\v6\trunk\media\m1_infiltrator_station.df

set TITLE="Infiltrator Station"
set IMAGE=D:\dev\v6\trunk\media\m1_infiltrator_station.tga
rem set CUTS=1731

..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k title -v %TITLE%
rem ..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k cuts -v %CUTS%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k icon -v %IMAGE%

pause