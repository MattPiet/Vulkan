@echo off
setlocal EnableExtensions

REM Compile the four DLSS shader variants when launched normally.
REM If one or more shader source files are dropped onto this BAT, compile
REM those files instead, creating <source filename>.spv beside each source.

if not defined VULKAN_SDK (
    echo ERROR: VULKAN_SDK is not set.
    echo Install the Vulkan SDK or set VULKAN_SDK to its installation directory.
    echo Example: C:\VulkanSDK\1.4.357.0
    pause
    exit /b 1
)

set "GLSLC=%VULKAN_SDK%\Bin\glslc.exe"
if not exist "%GLSLC%" (
    echo ERROR: Could not find "%GLSLC%".
    echo Check the VULKAN_SDK environment variable.
    pause
    exit /b 1
)

if not "%~1"=="" goto :compile_dropped_files

set "BASE=%~dp0"
echo Compiling the four DLSS motion-vector shaders...
call :CompileFile "%BASE%shaders\multiPhong_dlss.vert"
if errorlevel 1 goto :failed
call :CompileFile "%BASE%shaders\multiPhong_dlss.frag"
if errorlevel 1 goto :failed
call :CompileFile "%BASE%shaders\skybox_dlss.vert"
if errorlevel 1 goto :failed
call :CompileFile "%BASE%shaders\skybox_dlss.frag"
if errorlevel 1 goto :failed

echo.
echo All DLSS motion-vector shaders compiled successfully.
pause
exit /b 0

:compile_dropped_files
echo Compiling dropped shader file(s)...
:next_file
if "%~1"=="" goto :all_dropped_done
call :CompileFile "%~1"
if errorlevel 1 goto :failed
shift
goto :next_file

:all_dropped_done
echo.
echo All dropped shader file(s) compiled successfully.
pause
exit /b 0

:CompileFile
set "INPUT=%~1"
if not exist "%INPUT%" (
    echo ERROR: Source file not found: "%INPUT%"
    exit /b 1
)
set "OUTPUT=%INPUT%.spv"
echo.
echo glslc "%INPUT%" -o "%OUTPUT%"
"%GLSLC%" "%INPUT%" -o "%OUTPUT%"
if errorlevel 1 (
    echo ERROR: Shader compilation failed: "%INPUT%"
    exit /b 1
)
echo Created: "%OUTPUT%"
exit /b 0

:failed
echo.
echo Shader compilation failed. Check the messages above.
pause
exit /b 1
