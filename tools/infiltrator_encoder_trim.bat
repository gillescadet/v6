set STREAM=d:\tmp\v6\Inf_02_45fps_trimmed.df

set SHOT=d:\tmp\v6\Inf_02_45fps.df

..\bin\Release\dragonfly_encoder.exe -s %STREAM% -i %SHOT% -f 0 -l 1

pause