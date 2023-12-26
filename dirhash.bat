@echo off
for /R . %%f in (*.*) do (
    echo | set/p="%%~nxf - "
    certutil -hashfile "%%f" MD5 | findstr /V ":"
)
