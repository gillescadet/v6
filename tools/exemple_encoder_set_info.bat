set STREAM=d:\tmp\v6\fight_scene_movie.df

set TITLE="Fight Scene"
set IMAGE=d:\tmp\v6\fight_scene_movie.tga

bin\Release\dragonfly_encoder.exe -s %STREAM% -k title -v %TITLE%
bin\Release\dragonfly_encoder.exe -s %STREAM% -k icon -v %IMAGE%