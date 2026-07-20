$ErrorActionPreference = 'Continue'
& "C:\unreal engine\UE_5.8\Engine\Build\BatchFiles\Build.bat" MYPROJ2Editor Win64 Development -Project="c:\Users\LucasZou\Documents\Unreal Projects\MYPROJ2\MYPROJ2.uproject" -waitmutex
Write-Host ""
Write-Host "Exit code: $LASTEXITCODE" -ForegroundColor $(if ($LASTEXITCODE -eq 0) { 'Green' } else { 'Red' })
