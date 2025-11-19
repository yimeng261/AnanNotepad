/*
 * QQ图片文字发送工具 - 整合版
 * 功能：监控QQ输入，将特定格式文本自动转换为图片并发送
 * 编译: gcc -o qq_image_sender.exe combined_tool.c -lgdi32 -luser32 -lpsapi -lm -O2
 */

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psapi.h>
#include <ctype.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

/* ============ 图片处理相关结构和函数 ============ */

/* 配置结构 */
typedef struct {
    char base_image_paths[12][512];
    char font_name[128];
    int text_box_left;
    int text_box_top;
    int text_box_right;
    int text_box_bottom;
    COLORREF text_color;
    int max_font_height;
} ImageConfig;

/* 图像数据结构 */
typedef struct {
    unsigned char* data;
    int width;
    int height;
    int channels;
} ImageData;

/* 加载图片配置 */
void load_image_config(ImageConfig* config) {
    strcpy(config->base_image_paths[0], "BaseImages\\base.png");
    strcpy(config->base_image_paths[1], "BaseImages\\开心.png");
    strcpy(config->base_image_paths[2], "BaseImages\\生气.png");
    strcpy(config->base_image_paths[3], "BaseImages\\无语.png");
    strcpy(config->base_image_paths[4], "BaseImages\\脸红.png");
    strcpy(config->base_image_paths[5], "BaseImages\\病娇.png");
    strcpy(config->base_image_paths[6], "BaseImages\\闭眼.png");
    strcpy(config->base_image_paths[7], "BaseImages\\难受.png");
    strcpy(config->base_image_paths[8], "BaseImages\\害怕.png");
    strcpy(config->base_image_paths[9], "BaseImages\\激动.png");
    strcpy(config->base_image_paths[10], "BaseImages\\惊讶.png");
    strcpy(config->base_image_paths[11], "BaseImages\\哭泣.png");
    
    strcpy(config->font_name, "Microsoft YaHei");
    config->text_box_left = 119;
    config->text_box_top = 450;
    config->text_box_right = 398;
    config->text_box_bottom = 625;
    config->text_color = RGB(0, 0, 0);
    config->max_font_height = 64;
}

/* 加载图片 */
ImageData* load_image(const char* filename) {
    ImageData* img = (ImageData*)malloc(sizeof(ImageData));
    if (!img) return NULL;
    
    img->data = stbi_load(filename, &img->width, &img->height, &img->channels, 4);
    if (!img->data) {
        free(img);
        return NULL;
    }
    img->channels = 4;
    return img;
}

/* 释放图片 */
void free_image(ImageData* img) {
    if (img) {
        if (img->data) stbi_image_free(img->data);
        free(img);
    }
}

/* 将图像数据转换为HBITMAP */
HBITMAP create_bitmap_from_data(ImageData* img) {
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = img->width;
    bmi.bmiHeader.biHeight = -img->height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* pBits = NULL;
    HDC hdcScreen = GetDC(NULL);
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    ReleaseDC(NULL, hdcScreen);
    
    if (!hBitmap) return NULL;
    
    unsigned char* dest = (unsigned char*)pBits;
    unsigned char* src = img->data;
    
    for (int i = 0; i < img->width * img->height; i++) {
        dest[i*4 + 0] = src[i*4 + 2];
        dest[i*4 + 1] = src[i*4 + 1];
        dest[i*4 + 2] = src[i*4 + 0];
        dest[i*4 + 3] = 255;
    }
    
    return hBitmap;
}

/* 测量文本尺寸 */
void measure_text(HDC hdc, const char* text, int font_size, SIZE* size, ImageConfig* config) {
    HFONT hFont = CreateFontA(
        font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        config->font_name
    );
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    WCHAR wtext[4096];
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 4096);
    
    RECT rect;
    rect.left = config->text_box_left;
    rect.top = config->text_box_top;
    rect.right = config->text_box_right;
    rect.bottom = config->text_box_bottom;
    
    DrawTextW(hdc, wtext, -1, &rect, DT_CALCRECT | DT_WORDBREAK | DT_CENTER);
    
    size->cx = rect.right - rect.left;
    size->cy = rect.bottom - rect.top;
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

/* 查找最佳字号 */
int find_best_font_size(HDC hdc, const char* text, ImageConfig* config) {
    int region_w = config->text_box_right - config->text_box_left;
    int region_h = config->text_box_bottom - config->text_box_top;
    
    int lo = 10;
    int hi = (region_h < config->max_font_height) ? region_h : config->max_font_height;
    int best_size = 10;
    
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        SIZE size;
        measure_text(hdc, text, mid, &size, config);
        
        if (size.cx <= region_w && size.cy <= region_h) {
            best_size = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    
    return best_size;
}

/* 在位图上绘制文本 */
int draw_text_on_bitmap(HBITMAP hBitmap, const char* text, ImageConfig* config) {
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    int font_size = find_best_font_size(hdcMem, text, config);
    
    HFONT hFont = CreateFontA(
        font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        config->font_name
    );
    
    HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, config->text_color);
    
    WCHAR wtext[4096];
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 4096);
    
    RECT rect;
    rect.left = config->text_box_left;
    rect.top = config->text_box_top;
    rect.right = config->text_box_right;
    rect.bottom = config->text_box_bottom;
    
    DrawTextW(hdcMem, wtext, -1, &rect, DT_WORDBREAK | DT_CENTER | DT_VCENTER);
    
    SelectObject(hdcMem, hOldFont);
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hFont);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    
    return 1;
}

/* 生成图片并保存到临时文件 */
char* generate_image(const char* text, int emotion_idx) {
    static char temp_file[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_file);
    strcat(temp_file, "qq_temp_image.png");
    
    ImageConfig config;
    load_image_config(&config);
    
    if (emotion_idx < 0 || emotion_idx > 11) {
        emotion_idx = 0;
    }
    
    ImageData* img = load_image(config.base_image_paths[emotion_idx]);
    if (!img) return NULL;
    
    HBITMAP hBitmap = create_bitmap_from_data(img);
    if (!hBitmap) {
        free_image(img);
        return NULL;
    }
    
    if (!draw_text_on_bitmap(hBitmap, text, &config)) {
        DeleteObject(hBitmap);
        free_image(img);
        return NULL;
    }
    
    /* 将位图转换回图像数据 */
    unsigned char* tempData = (unsigned char*)malloc(img->width * img->height * 4);
    if (tempData) {
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        SelectObject(hdcMem, hBitmap);
        
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(BITMAPINFO));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = img->width;
        bmi.bmiHeader.biHeight = -img->height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        GetDIBits(hdcMem, hBitmap, 0, img->height, tempData, &bmi, DIB_RGB_COLORS);
        
        for (int i = 0; i < img->width * img->height; i++) {
            img->data[i*4 + 0] = tempData[i*4 + 2];
            img->data[i*4 + 1] = tempData[i*4 + 1];
            img->data[i*4 + 2] = tempData[i*4 + 0];
            img->data[i*4 + 3] = 255;
        }
        
        free(tempData);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
    }
    
    /* 保存图片 */
    int result = stbi_write_png(temp_file, img->width, img->height, 4, img->data, img->width * 4);
    
    DeleteObject(hBitmap);
    free_image(img);
    
    return result ? temp_file : NULL;
}

/* 将图片复制到剪贴板 (使用DIB格式，兼容QQ) */
BOOL CopyImageToClipboard(const char* image_path) {
    /* 加载图片 */
    ImageData* img = load_image(image_path);
    if (!img) return FALSE;
    
    /* 创建BITMAPINFOHEADER和位图数据 */
    BITMAPINFOHEADER bih;
    ZeroMemory(&bih, sizeof(BITMAPINFOHEADER));
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = img->width;
    bih.biHeight = img->height;  /* 正值表示从下到上 */
    bih.biPlanes = 1;
    bih.biBitCount = 24;  /* 24位RGB，不含Alpha */
    bih.biCompression = BI_RGB;
    bih.biSizeImage = ((img->width * 3 + 3) & ~3) * img->height;  /* 行对齐到4字节 */
    
    /* 计算DIB数据大小 */
    DWORD dibSize = sizeof(BITMAPINFOHEADER) + bih.biSizeImage;
    
    /* 分配全局内存 */
    HGLOBAL hDIB = GlobalAlloc(GMEM_MOVEABLE, dibSize);
    if (!hDIB) {
        free_image(img);
        return FALSE;
    }
    
    /* 锁定内存并填充数据 */
    void* pDIB = GlobalLock(hDIB);
    if (!pDIB) {
        GlobalFree(hDIB);
        free_image(img);
        return FALSE;
    }
    
    /* 复制BITMAPINFOHEADER */
    memcpy(pDIB, &bih, sizeof(BITMAPINFOHEADER));
    
    /* 转换图像数据为24位BGR格式，并从下到上排列 */
    unsigned char* pBits = (unsigned char*)pDIB + sizeof(BITMAPINFOHEADER);
    int rowSize = (img->width * 3 + 3) & ~3;  /* 行字节数，对齐到4字节 */
    
    /* 从下到上填充（DIB标准格式） */
    for (int y = 0; y < img->height; y++) {
        int srcRow = img->height - 1 - y;  /* 源图从上到下 */
        unsigned char* destRow = pBits + y * rowSize;
        
        for (int x = 0; x < img->width; x++) {
            int srcIdx = (srcRow * img->width + x) * 4;
            int destIdx = x * 3;
            
            /* RGBA转BGR（去掉Alpha通道） */
            destRow[destIdx + 0] = img->data[srcIdx + 2];  /* B */
            destRow[destIdx + 1] = img->data[srcIdx + 1];  /* G */
            destRow[destIdx + 2] = img->data[srcIdx + 0];  /* R */
        }
    }
    
    GlobalUnlock(hDIB);
    free_image(img);
    
    /* 复制到剪贴板 */
    if (!OpenClipboard(NULL)) {
        GlobalFree(hDIB);
        return FALSE;
    }
    
    EmptyClipboard();
    SetClipboardData(CF_DIB, hDIB);
    CloseClipboard();
    
    /* 注意：hDIB由系统管理，不要手动GlobalFree */
    
    return TRUE;
}

/* ============ QQ监控相关函数 ============ */

HHOOK g_hKeyboardHook = NULL;

/* 检查是否是QQ窗口 */
BOOL IsQQWindow(HWND hwnd) {
    char className[256];
    
    if (hwnd == NULL || !IsWindow(hwnd)) return FALSE;
    
    GetClassNameA(hwnd, className, sizeof(className));
    
    if (strstr(className, "TXGuiFoundation") != NULL) {
        return TRUE;
    }
    
    if (strcmp(className, "#32770") == 0) {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hProcess != NULL) {
            char processName[MAX_PATH];
            if (GetModuleBaseNameA(hProcess, NULL, processName, sizeof(processName))) {
                for (int i = 0; processName[i]; i++) {
                    processName[i] = tolower(processName[i]);
                }
                if (strstr(processName, "qq.exe") != NULL) {
                    CloseHandle(hProcess);
                    return TRUE;
                }
            }
            CloseHandle(hProcess);
        }
    }
    
    return FALSE;
}

/* 备份剪贴板 */
BOOL BackupClipboard(char* backup, int bufferSize) {
    if (!OpenClipboard(NULL)) return FALSE;
    
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData != NULL) {
        WCHAR* pszTextW = (WCHAR*)GlobalLock(hData);
        if (pszTextW != NULL) {
            int size = WideCharToMultiByte(CP_UTF8, 0, pszTextW, -1, NULL, 0, NULL, NULL);
            if (size > 0 && size <= bufferSize) {
                WideCharToMultiByte(CP_UTF8, 0, pszTextW, -1, backup, bufferSize, NULL, NULL);
            } else {
                backup[0] = '\0';
            }
            GlobalUnlock(hData);
            CloseClipboard();
            return TRUE;
        }
        GlobalUnlock(hData);
    }
    
    backup[0] = '\0';
    CloseClipboard();
    return TRUE;
}

/* 模拟按键 */
void SendKeys(WORD vk, BOOL ctrl) {
    INPUT inputs[4];
    int inputCount = 0;
    
    if (ctrl) {
        inputs[inputCount].type = INPUT_KEYBOARD;
        inputs[inputCount].ki.wVk = VK_CONTROL;
        inputs[inputCount].ki.dwFlags = 0;
        inputs[inputCount].ki.time = 0;
        inputs[inputCount].ki.dwExtraInfo = 0;
        inputCount++;
    }
    
    inputs[inputCount].type = INPUT_KEYBOARD;
    inputs[inputCount].ki.wVk = vk;
    inputs[inputCount].ki.dwFlags = 0;
    inputs[inputCount].ki.time = 0;
    inputs[inputCount].ki.dwExtraInfo = 0;
    inputCount++;
    
    inputs[inputCount].type = INPUT_KEYBOARD;
    inputs[inputCount].ki.wVk = vk;
    inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[inputCount].ki.time = 0;
    inputs[inputCount].ki.dwExtraInfo = 0;
    inputCount++;
    
    if (ctrl) {
        inputs[inputCount].type = INPUT_KEYBOARD;
        inputs[inputCount].ki.wVk = VK_CONTROL;
        inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[inputCount].ki.time = 0;
        inputs[inputCount].ki.dwExtraInfo = 0;
        inputCount++;
    }
    
    SendInput(inputCount, inputs, sizeof(INPUT));
}

/* 清空剪贴板 */
BOOL ClearClipboard() {
    if (!OpenClipboard(NULL)) return FALSE;
    EmptyClipboard();
    CloseClipboard();
    return TRUE;
}

/* 从剪贴板获取文本 */
BOOL GetClipboardText(char* buffer, int bufferSize) {
    if (!OpenClipboard(NULL)) return FALSE;
    
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData != NULL) {
        WCHAR* pszTextW = (WCHAR*)GlobalLock(hData);
        if (pszTextW != NULL) {
            int size = WideCharToMultiByte(CP_UTF8, 0, pszTextW, -1, NULL, 0, NULL, NULL);
            if (size > 0 && size <= bufferSize) {
                WideCharToMultiByte(CP_UTF8, 0, pszTextW, -1, buffer, bufferSize, NULL, NULL);
                GlobalUnlock(hData);
                CloseClipboard();
                return TRUE;
            }
            GlobalUnlock(hData);
        }
    }
    
    buffer[0] = '\0';
    CloseClipboard();
    return FALSE;
}

/* 捕获输入内容 */
BOOL CaptureInputByClipboard(char* outputBuffer, int bufferSize) {
    char oldClipboard[8192];
    if (!BackupClipboard(oldClipboard, sizeof(oldClipboard))) {
        return FALSE;
    }
    
    ClearClipboard();
    SendKeys('A', TRUE);
    Sleep(50);
    SendKeys('C', TRUE);
    Sleep(50);
    
    BOOL success = GetClipboardText(outputBuffer, bufferSize);
    BackupClipboard(oldClipboard, sizeof(oldClipboard));
    
    return success && outputBuffer[0] != '\0';
}

/* 处理文本并生成图片 */
BOOL ProcessAndSendImage(const char* text) {
    if (text == NULL || text[0] == '\0') return FALSE;
    
    printf("\n【捕获到内容】%s\n", text);
    
    /* 解析表情编号和文本 */
    /* 格式: 文本#数字 -> #数字表示表情编号 */
    int emotion_idx = 0;
    char content[8192];
    strncpy(content, text, sizeof(content) - 1);
    content[sizeof(content) - 1] = '\0';
    
    /* 检查是否以 #数字 结尾 */
    int len = strlen(content);
    if (len >= 2) {
        int i = len - 1;
        int digitEnd = i;
        
        while (i >= 0 && isdigit((unsigned char)content[i])) {
            i--;
        }
        
        if (i >= 0 && content[i] == '#' && i < digitEnd) {
            char numberStr[32];
            int numberLen = digitEnd - i;
            strncpy(numberStr, &content[i + 1], numberLen);
            numberStr[numberLen] = '\0';
            
            emotion_idx = atoi(numberStr);
            content[i] = '\0';  /* 移除 #数字 */
            
            printf("→ 检测到表情编号: %d\n", emotion_idx);
        }
    }
    
    /* 生成图片 */
    printf("→ 正在生成图片...\n");
    char* image_path = generate_image(content, emotion_idx);
    if (!image_path) {
        printf("× 生成图片失败\n");
        return FALSE;
    }
    
    printf("✓ 图片已生成: %s\n", image_path);
    
    /* 复制图片到剪贴板 */
    printf("→ 正在复制图片到剪贴板...\n");
    if (!CopyImageToClipboard(image_path)) {
        printf("× 复制图片失败\n");
        return FALSE;
    }
    
    printf("✓ 图片已复制到剪贴板\n");
    
    /* 直接粘贴图片（文字已处于全选状态，会自动覆盖） */
    printf("→ 正在粘贴图片...\n");
    Sleep(1);
    SendKeys('V', TRUE);  /* 粘贴图片 */
    Sleep(1);           /* 等待图片加载 */
    SendKeys(VK_RETURN, FALSE);  /* 发送 */

    return TRUE;
}

/* 键盘钩子回调 */
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
        
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (pKeyboard->vkCode == VK_RETURN) {
                HWND hForeground = GetForegroundWindow();
                
                if (IsQQWindow(hForeground)) {
                    char capturedText[8192];
                    if (CaptureInputByClipboard(capturedText, sizeof(capturedText))) {
                        /* 检查是否包含 #数字 格式 */
                        int len = strlen(capturedText);
                        BOOL shouldConvert = FALSE;
                        
                        if (len >= 2) {
                            int i = len - 1;
                            while (i >= 0 && isdigit((unsigned char)capturedText[i])) {
                                i--;
                            }
                            if (i >= 0 && capturedText[i] == '#' && i < len - 1) {
                                shouldConvert = TRUE;
                            }
                        }
                        
                        if (shouldConvert) {
                            ProcessAndSendImage(capturedText);
                            return 1;  /* 阻止Enter键传递 */
                        }
                    }
                }
            }
        }
    }
    
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

/* 安装键盘钩子 */
BOOL InstallKeyboardHook() {
    g_hKeyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        KeyboardProc,
        GetModuleHandle(NULL),
        0
    );
    
    if (g_hKeyboardHook == NULL) {
        printf("错误: 安装键盘钩子失败! 错误代码: %lu\n", GetLastError());
        printf("请以管理员身份运行程序\n");
        return FALSE;
    }
    
    return TRUE;
}

/* 卸载键盘钩子 */
void UninstallKeyboardHook() {
    if (g_hKeyboardHook != NULL) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = NULL;
    }
}

/* 主函数 */
int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    
    printf("========================================\n");
    printf("  QQ图片文字发送工具\n");
    printf("========================================\n");
    printf("\n");
    printf("功能说明:\n");
    printf("  • 监听QQ窗口的输入\n");
    printf("  • 检测 #数字 格式自动转换为图片\n");
    printf("  • 支持12种表情底图（0-11）\n");
    printf("  • 自动生成并发送图片\n");
    printf("\n");
    printf("使用方法:\n");
    printf("  在QQ输入框输入: 你的文字#0\n");
    printf("  按Enter后会自动转换为图片并发送\n");
    printf("\n");
    printf("表情编号:\n");
    printf("  0-普通  1-开心  2-生气  3-无语\n");
    printf("  4-脸红  5-病娇  6-闭眼  7-难受\n");
    printf("  8-害怕  9-激动  10-惊讶 11-哭泣\n");
    printf("\n");
    printf("示例: 今天天气真好#1 (开心表情)\n");
    printf("\n");
    printf("按 Ctrl+C 退出程序\n");
    printf("========================================\n\n");
    
    /* 检查必要文件 */
    ImageConfig config;
    load_image_config(&config);
    
    BOOL foundImages = FALSE;
    for (int i = 0; i < 12; i++) {
        DWORD attrib = GetFileAttributesA(config.base_image_paths[i]);
        if (attrib != INVALID_FILE_ATTRIBUTES) {
            foundImages = TRUE;
            break;
        }
    }
    
    if (!foundImages) {
        printf("警告: 未找到底图文件，请确保 BaseImages 目录存在\n\n");
    }
    
    /* 安装键盘钩子 */
    if (!InstallKeyboardHook()) {
        printf("\n初始化失败，请以管理员身份运行！\n");
        system("pause");
        return 1;
    }
    
    printf("✓ 键盘钩子已安装\n");
    printf("✓ 监控已启动\n");
    printf("\n等待QQ窗口输入...\n\n");
    
    /* 消息循环 */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    /* 清理 */
    UninstallKeyboardHook();
    printf("\n程序已退出\n");
    
    return 0;
}

