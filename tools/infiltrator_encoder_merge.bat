set STREAM=D:\dev\v6\trunk\media\m1_infiltrator_station.df

set SHOT0=d:\tmp\v6\Inf_00_45fps.df
set SHOT1=d:\tmp\v6\Inf_01_45fps.df
set SHOT2=d:\tmp\v6\Inf_02_45fps_trimmed.df
set SHOT3=d:\tmp\v6\Inf_03_45fps.df

..\bin\Release\dragonfly_encoder.exe -s %STREAM% -a %SHOT0% -a %SHOT1% -a %SHOT2% -a %SHOT3%

pause