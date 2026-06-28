@echo off
set QT_BINS=%1
set DEST_DIR=%2
copy /y "%QT_BINS%\libssl-1_1-x64.dll" "%DEST_DIR%\" >nul 2>&1
copy /y "%QT_BINS%\libcrypto-1_1-x64.dll" "%DEST_DIR%\" >nul 2>&1
echo Copied OpenSSL 1.1.x DLLs for Qt5 HTTPS support
