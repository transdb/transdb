FOR /F "tokens=*" %%i IN ('call "C:\Program Files\SlikSvn\bin\svnversion.exe"') DO echo #define SVNVERSION "%%i" > "C:\Users\administrator.VPS4U\Desktop\TransStorageServerSVN\trunk\src\console\svnversion.h"