@echo off
chcp 65001 >nul
echo ===================================
echo  QQ图片文字发送工具 - 编译脚本
echo ===================================
echo.

REM 检查是否安装了GCC
where gcc >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 未找到GCC编译器
    echo 请安装 MinGW-w64: https://winlibs.com/
    pause
    exit /b 1
)

echo [1/3] 检测编译环境
gcc --version | findstr "gcc"
echo.

echo [2/3] 检查 STB 库文件...
if not exist "stb_image.h" (
    echo [错误] 缺少 stb_image.h
    echo 请先运行 build_stb_tool.bat 下载库文件
    pause
    exit /b 1
)
if not exist "stb_image_write.h" (
    echo [错误] 缺少 stb_image_write.h  
    echo 请先运行 build_stb_tool.bat 下载库文件
    pause
    exit /b 1
)
echo STB库文件已就绪
echo.

echo [3/3] 编译程序...
gcc -o qq_image_sender.exe combined_tool.c -lgdi32 -luser32 -lpsapi -lm -O2
if %errorlevel% neq 0 (
    echo [错误] 编译失败
    pause
    exit /b 1
)

echo.
echo ===================================
echo  编译成功！
echo ===================================
echo.
echo 可执行文件: qq_image_sender.exe
echo.
echo 使用方法:
echo   1. 以管理员身份运行 qq_image_sender.exe
echo   2. 在QQ输入框输入: 你的文字#表情编号
echo   3. 按Enter会自动转换为图片并发送
echo.
echo 表情编号:
echo   0-普通  1-开心  2-生气  3-无语
echo   4-脸红  5-病娇  6-闭眼  7-难受
echo   8-害怕  9-激动  10-惊讶 11-哭泣
echo.
echo 示例:
echo   今天天气真好#1  （使用开心表情）
echo   什么情况#10      （使用惊讶表情）
echo.
pause

