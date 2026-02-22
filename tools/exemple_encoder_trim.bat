set STREAM=d:\tmp\v6\fight_scene_shot1.df

set SHOT=d:\tmp\v6\fight_scene_shot1_trimmed.df

bin\Release\dragonfly_encoder.exe -s %STREAM% -i %SHOT% -f 0 -l 1
