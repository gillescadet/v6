set STREAM=D:\dev\v6\trunk\media\m1_fight_scene.df

set SHOT1=d:\tmp\v6\fight_scene_shot1_trimmed.df
set SHOT2=d:\tmp\v6\fight_scene_shot2_v2_trimmed.df
set SHOT3=d:\tmp\v6\fight_scene_shot3_v2.df

..\bin\Release\dragonfly_encoder.exe -s %STREAM% -a %SHOT1% -a %SHOT2% -a %SHOT3%

pause