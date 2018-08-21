vf gfx
for %%F in (gfx/*.lmp) do (
    tools\lmp2tga.exe gfx/%%~nxF
)
pause