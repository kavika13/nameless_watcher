@ECHO OFF

SET CommonCompilerFlags=-Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -d2Zi+ -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4127 -FC -Z7
SET CommonCompilerFlags=-DNAMELESS_WATCHER_INTERNAL=1 -DNAMELESS_WATCHER_SLOW=1 -DNAMELESS_WATCHER_WIN32=1 %CommonCompilerFlags%
SET CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib opengl32.lib

REM TODO - can we just build both with one exe?

IF NOT EXIST ..\build MKDIR ..\build
PUSHD ..\build

DEL *.pdb > NUL 2> NUL

REM 32-bit build
REM cl %CommonCompilerFlags% ..\src\win32_watcher.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
REM Optimization switches /wO2
ECHO WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% -Fewatcher.dll ..\src\main.cpp -Fmwatcher.map -LD /link -incremental:no -opt:ref -PDB:watcher_%random%.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender
SET LastError=%ERRORLEVEL%
DEL lock.tmp
cl %CommonCompilerFlags% -Fewin32_watcher.exe ..\src\win32_watcher.cpp -Fmwin32_watcher.map /link %CommonLinkerFlags%
POPD
