set STREAM=D:\dev\v6\trunk\media\m1_fight_scene.df

set TITLE="Fight Scene"
set IMAGE=D:\dev\v6\trunk\media\m1_fight_scene.tga
set CUTS=1731

..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k title -v %TITLE%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k cuts -v %CUTS%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k icon -v %IMAGE%

pause