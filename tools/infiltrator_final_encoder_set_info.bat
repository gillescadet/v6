set STREAM=d:\tmp\v6\infiltrator_final_trimmed.df

set TITLE="Infiltrator Boss"
set IMAGE=d:\tmp\v6\infiltrator_final.tga
set CUTS=142,223

..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k title -v %TITLE%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k cuts -v %CUTS%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k icon -v %IMAGE%

pause