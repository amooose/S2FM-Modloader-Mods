# S2FM Modloader / Mods

## S2FM Modloader
This modloader is simple,  

We do a little bit of DLL Proxy Hijacking using a custom p4lib.dll (the mod loader), the .dll gets loaded by SFM, which then injects all .dlls present in the /mods/ folder.

## S2FM Camera Fix
This fixes the bug of the camera accelerating to a random angle by consuming any large movement camera adjustments in the first few hundred milliseconds upon each time the viewport becomes active.

### Installation
1. In `\Half-Life Alyx\game\bin\win64\` rename `p4lib.dll` to `p4lib_real.dll`
2. Extract the contents of the release.zip folder into `\Half-Life Alyx\game\bin\win64\`

(Make sure you rename the original `p4lib.dll` to `p4lib_real.dll` **before extracting**)  

Your HL Alyx folder should look like this:  
`Half-Life Alyx\game\bin\win64\mods` (containing s2fmCameraPatch.dll, etc)  
`Half-Life Alyx\game\bin\win64\p4lib.dll`  
`Half-Life Alyx\game\bin\win64\p4lib_real.dll`  
