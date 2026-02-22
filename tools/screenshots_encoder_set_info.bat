set STREAM1=D:\dev\v6\trunk\media\m1_sequencer.df
set SS1=D:\tmp\v6\Seq_Screenshot_01.df
set TITLE1="Sequencer"
set IMAGE1=D:\dev\v6\trunk\media\m1_sequencer.tga

copy %SS1% %STREAM1%
..\bin\Release\dragonfly_encoder.exe -s %STREAM1% -k title -v %TITLE1%
..\bin\Release\dragonfly_encoder.exe -s %STREAM1% -k icon -v %IMAGE1%

REM

set STREAM2=D:\dev\v6\trunk\media\m1_temple.df
set SS2=D:\tmp\v6\SunT_Screenshot_01.df
set TITLE2="Temple"
set IMAGE2=D:\dev\v6\trunk\media\m1_temple.tga

copy %SS2% %STREAM2%
..\bin\Release\dragonfly_encoder.exe -s %STREAM2% -k title -v %TITLE2%
..\bin\Release\dragonfly_encoder.exe -s %STREAM2% -k icon -v %IMAGE2%

REM

set STREAM=D:\dev\v6\trunk\media\m1_unreal_paris.df
set SS=D:\tmp\v6\unreal_paris3.df
set TITLE="Unreal Paris"
set IMAGE=D:\dev\v6\trunk\media\m1_unreal_paris.tga

copy %SS% %STREAM%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k title -v %TITLE%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k icon -v %IMAGE%

REM

set STREAM=D:\dev\v6\trunk\media\m1_elemental_ss2.df
set SS=D:\tmp\v6\Elem_Screenshot_02.df
set TITLE="Dead Castle 2"
set IMAGE=D:\dev\v6\trunk\media\m1_elemental_ss2.tga

copy %SS% %STREAM%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k title -v %TITLE%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k icon -v %IMAGE%

REM

set STREAM=D:\dev\v6\trunk\media\m1_elemental_ss4.df
set SS=D:\tmp\v6\Elem_Screenshot_04.df
set TITLE="Beast"
set IMAGE=D:\dev\v6\trunk\media\m1_elemental_ss4.tga

copy %SS% %STREAM%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k title -v %TITLE%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k icon -v %IMAGE%

REM

set STREAM=D:\dev\v6\trunk\media\m1_elemental_ss5.df
set SS=D:\tmp\v6\Elem_Screenshot_05.df
set TITLE="Den"
set IMAGE=D:\dev\v6\trunk\media\m1_elemental_ss5.tga

copy %SS% %STREAM%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k title -v %TITLE%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k icon -v %IMAGE%

REM

set STREAM=D:\dev\v6\trunk\media\m1_elemental_ss6.df
set SS=D:\tmp\v6\Elem_Screenshot_06.df
set TITLE="Burning Den"
set IMAGE=D:\dev\v6\trunk\media\m1_elemental_ss6.tga

copy %SS% %STREAM%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k title -v %TITLE%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k icon -v %IMAGE%

REM

set STREAM=D:\dev\v6\trunk\media\m1_elemental_ss7.df
set SS=D:\tmp\v6\Elem_Screenshot_07.df
set TITLE="Beast Destroy"
set IMAGE=D:\dev\v6\trunk\media\m1_elemental_ss7.tga

copy %SS% %STREAM%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k title -v %TITLE%
..\bin\Release\dragonfly_encoder.exe -s %STREAM% -k icon -v %IMAGE%

pause