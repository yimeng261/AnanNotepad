/*
 * QQ图片文字发送工具 - 整合版（内嵌资源）
 * 功能：监控QQ输入，将特定格式文本自动转换为图片并发送
 * 编译: 
 *   windres resources.rc -o resources.o
 *   gcc -o qq_image_sender.exe combined_tool.c resources.o -lgdi32 -luser32 -lpsapi -lm -O2
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

#include "resource.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

/* 定义 QuickEdit 模式标志（如果未定义） */
#ifndef ENABLE_QUICK_EDIT_MODE
#define ENABLE_QUICK_EDIT_MODE 0x0040
#endif
#ifndef ENABLE_EXTENDED_FLAGS
#define ENABLE_EXTENDED_FLAGS 0x0080
#endif

#define NUM_EMOTIONS 12

/* ============ 图片处理相关结构和函数 ============ */

/* 配置结构 */
typedef struct {
    int resource_ids[NUM_EMOTIONS];  /* 资源ID数组 */
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
    /* 设置资源ID */
    config->resource_ids[0] = IDR_IMAGE_BASE;
    config->resource_ids[1] = IDR_IMAGE_HAPPY;
    config->resource_ids[2] = IDR_IMAGE_ANGRY;
    config->resource_ids[3] = IDR_IMAGE_SPEECHLESS;
    config->resource_ids[4] = IDR_IMAGE_BLUSH;
    config->resource_ids[5] = IDR_IMAGE_YANDERE;
    config->resource_ids[6] = IDR_IMAGE_CLOSED;
    config->resource_ids[7] = IDR_IMAGE_SAD;
    config->resource_ids[8] = IDR_IMAGE_SCARED;
    config->resource_ids[9] = IDR_IMAGE_EXCITED;
    config->resource_ids[10] = IDR_IMAGE_SURPRISED;
    config->resource_ids[11] = IDR_IMAGE_CRYING;
    
    strcpy(config->font_name, "Microsoft YaHei");
    config->text_box_left = 119;
    config->text_box_top = 450;
    config->text_box_right = 398;
    config->text_box_bottom = 625;
    config->text_color = RGB(0, 0, 0);
    config->max_font_height = 64;
}

/* 从资源加载图片 */
ImageData* load_image_from_resource(int resource_id) {
    /* 查找资源 */
    HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(resource_id), RT_RCDATA);
    if (!hResource) {
        printf("错误：找不到资源 ID=%d\n", resource_id);
        return NULL;
    }
    
    /* 加载资源 */
    HGLOBAL hMemory = LoadResource(NULL, hResource);
    if (!hMemory) {
        printf("错误：无法加载资源 ID=%d\n", resource_id);
        return NULL;
    }
    
    /* 锁定资源 */
    DWORD dwSize = SizeofResource(NULL, hResource);
    LPVOID pData = LockResource(hMemory);
    if (!pData) {
        printf("错误：无法锁定资源 ID=%d\n", resource_id);
        return NULL;
    }
    
    /* 使用stb_image从内存加载图片 */
    ImageData* img = (ImageData*)malloc(sizeof(ImageData));
    if (!img) return NULL;
    
    img->data = stbi_load_from_memory((const unsigned char*)pData, dwSize, 
                                      &img->width, &img->height, &img->channels, 4);
    if (!img->data) {
        printf("stbi_load_from_memory 错误: %s\n", stbi_failure_reason());
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
        font_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,  /* FW_BOLD = 粗体 */
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

/* 检查字符是否是方括号（半角或全角） */
BOOL IsBracket(WCHAR c) {
    return (c == L'[' || c == L']' || c == L'【' || c == L'】');
}

/* 检查字符是否是左方括号 */
BOOL IsLeftBracket(WCHAR c) {
    return (c == L'[' || c == L'【');
}

/* 检查字符是否是右方括号 */
BOOL IsRightBracket(WCHAR c) {
    return (c == L']' || c == L'】');
}

/* 结构：存储文本行和每个字符的颜色 */
typedef struct {
    WCHAR* chars;
    COLORREF* colors;
    int length;
} ColoredLine;

/* 将文本按行分割，并保留颜色信息 */
int SplitTextIntoLines(HDC hdc, WCHAR* wtext, int font_size, ImageConfig* config, ColoredLine** outLines) {
    /* 创建字体 */
    HFONT hFont = CreateFontA(
        font_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        config->font_name
    );
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    int len = wcslen(wtext);
    int lineWidth = config->text_box_right - config->text_box_left;
    
    /* 分配行缓冲 */
    ColoredLine* lines = (ColoredLine*)malloc(100 * sizeof(ColoredLine));
    int lineCount = 0;
    
    /* 当前行的字符和颜色 */
    WCHAR lineBuffer[4096];
    COLORREF colorBuffer[4096];
    int linePos = 0;
    
    BOOL inBracket = FALSE;
    int currentX = 0;
    
    for (int i = 0; i <= len; i++) {
        WCHAR ch = (i < len) ? wtext[i] : L'\0';
        
        /* 检测颜色变化 */
        if (IsLeftBracket(ch)) {
            inBracket = TRUE;
        }
        
        COLORREF color = inBracket ? RGB(128, 0, 128) : config->text_color;
        
        /* 测量字符宽度 */
        SIZE sz = {0};
        if (i < len) {
            GetTextExtentPoint32W(hdc, &ch, 1, &sz);
        }
        
        /* 检查是否需要换行 */
        if (i == len || ch == L'\n' || (currentX + sz.cx > lineWidth && linePos > 0)) {
            /* 保存当前行 */
            if (linePos > 0 || i == len) {
                lines[lineCount].chars = (WCHAR*)malloc((linePos + 1) * sizeof(WCHAR));
                lines[lineCount].colors = (COLORREF*)malloc(linePos * sizeof(COLORREF));
                memcpy(lines[lineCount].chars, lineBuffer, linePos * sizeof(WCHAR));
                lines[lineCount].chars[linePos] = L'\0';
                memcpy(lines[lineCount].colors, colorBuffer, linePos * sizeof(COLORREF));
                lines[lineCount].length = linePos;
                lineCount++;
                
                linePos = 0;
                currentX = 0;
            }
            
            /* 如果是结束或换行符，继续下一个字符 */
            if (i == len || ch == L'\n') {
                if (IsRightBracket(ch)) {
                    inBracket = FALSE;
                }
                continue;
            }
        }
        
        /* 添加字符到当前行 */
        if (i < len) {
            lineBuffer[linePos] = ch;
            colorBuffer[linePos] = color;
            linePos++;
            currentX += sz.cx;
        }
        
        if (IsRightBracket(ch)) {
            inBracket = FALSE;
        }
    }
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    
    *outLines = lines;
    return lineCount;
}

/* 在位图上绘制带颜色的文本片段 */
void DrawColoredTextSegments(HDC hdc, WCHAR* wtext, int font_size, ImageConfig* config) {
    HFONT hFont = CreateFontA(
        font_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        config->font_name
    );
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    
    /* 将文本分行 */
    ColoredLine* lines = NULL;
    int lineCount = SplitTextIntoLines(hdc, wtext, font_size, config, &lines);
    
    if (lineCount == 0) {
        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        return;
    }
    
    /* 计算行高 */
    SIZE sz;
    GetTextExtentPoint32W(hdc, L"测", 1, &sz);
    int lineHeight = sz.cy;
    
    /* 计算总高度和起始Y */
    int totalHeight = lineCount * lineHeight;
    int startY = (config->text_box_top + config->text_box_bottom - totalHeight) / 2;
    
    /* 绘制每一行 */
    for (int line = 0; line < lineCount; line++) {
        /* 计算行宽度（用于居中） */
        int lineWidth = 0;
        for (int i = 0; i < lines[line].length; i++) {
            GetTextExtentPoint32W(hdc, &lines[line].chars[i], 1, &sz);
            lineWidth += sz.cx;
        }
        
        int boxWidth = config->text_box_right - config->text_box_left;
        int startX = config->text_box_left + (boxWidth - lineWidth) / 2;
        
        /* 绘制每个字符 */
        int x = startX;
        int y = startY + line * lineHeight;
        
        for (int i = 0; i < lines[line].length; i++) {
            SetTextColor(hdc, lines[line].colors[i]);
            GetTextExtentPoint32W(hdc, &lines[line].chars[i], 1, &sz);
            TextOutW(hdc, x, y, &lines[line].chars[i], 1);
            x += sz.cx;
        }
        
        /* 释放行数据 */
        free(lines[line].chars);
        free(lines[line].colors);
    }
    
    free(lines);
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

/* 在位图上绘制文本（带颜色支持） */
int draw_text_on_bitmap(HBITMAP hBitmap, const char* text, ImageConfig* config) {
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    WCHAR wtext[4096];
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 4096);
    
    int font_size = find_best_font_size(hdcMem, text, config);
    
    /* 使用复杂的分段绘制 */
    DrawColoredTextSegments(hdcMem, wtext, font_size, config);
    
    SelectObject(hdcMem, hOldBitmap);
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
    
    if (emotion_idx < 0 || emotion_idx > NUM_EMOTIONS - 1) {
        emotion_idx = 0;
    }
    
    /* 从资源加载图片 */
    ImageData* img = load_image_from_resource(config.resource_ids[emotion_idx]);
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
    /* 加载图片文件（临时文件） */
    ImageData* img = (ImageData*)malloc(sizeof(ImageData));
    if (!img) return FALSE;
    
    img->data = stbi_load(image_path, &img->width, &img->height, &img->channels, 4);
    if (!img->data) {
        free(img);
        return FALSE;
    }
    img->channels = 4;
    
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
    
    return TRUE;
}

/* ============ QQ监控相关函数 ============ */

HHOOK g_hKeyboardHook = NULL;

/* 输入缓冲区 */
#define INPUT_BUFFER_SIZE 1024
char g_inputBuffer[INPUT_BUFFER_SIZE] = {0};
int g_inputLength = 0;
HWND g_lastWindow = NULL;

/* 清空输入缓冲区 */
void ClearInputBuffer() {
    memset(g_inputBuffer, 0, INPUT_BUFFER_SIZE);
    g_inputLength = 0;
}

/* 添加字符串到输入缓冲区（支持UTF-8多字节字符） */
void AppendToInputBuffer(const char* str) {
    int len = strlen(str);
    if (g_inputLength + len < INPUT_BUFFER_SIZE - 1) {
        strcat(g_inputBuffer, str);
        g_inputLength += len;
    }
}

/* 从输入缓冲区删除最后一个字符 */
void RemoveLastCharFromBuffer() {
    if (g_inputLength > 0) {
        g_inputLength--;
        g_inputBuffer[g_inputLength] = '\0';
    }
}


BOOL IsOnlyDigits() {
    for (int i = 0; i < g_inputLength; i++) {
        if (!isdigit(g_inputBuffer[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

BOOL HasTargetFormat(const char* text) {
    int len = strlen(text);
    if (len < 2) return FALSE;
    
    int i = len - 1;
    while (i >= 0 && isdigit((unsigned char)text[i])) {
        i--;
    }
    
    return (i >= 0 && text[i] == '#' && i < len - 1);
}


/* 判断是否需要触发剪贴板获取 */
BOOL ShouldTriggerClipboard() {
    if (g_inputLength == 0 || IsOnlyDigits() || HasTargetFormat(g_inputBuffer)) {
        return TRUE;
    }
    
    return FALSE;
}



/* 检查是否是支持的聊天窗口（QQ或微信） */
BOOL IsSupportedChatWindow(HWND hwnd) {
    if (hwnd == NULL || !IsWindow(hwnd)) return FALSE;
    
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess != NULL) {
        char exePath[MAX_PATH];
        DWORD size = GetModuleFileNameExA(hProcess, NULL, exePath, MAX_PATH);
        
        if (size > 0) {
            for (DWORD i = 0; i < size; i++) {
                exePath[i] = tolower(exePath[i]);
            }
            if (
                strstr(exePath, "wechat.exe") != NULL || 
                strstr(exePath, "weixin.exe") != NULL || 
                strstr(exePath, "qq.exe") != NULL
            ) {
                CloseHandle(hProcess);
                return TRUE;
            }
        }
    }
    CloseHandle(hProcess);
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
    Sleep(10);
    SendKeys('C', TRUE);
    Sleep(10);
    
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
    char* image_path = generate_image(content, emotion_idx%NUM_EMOTIONS);
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
    Sleep(50);
    SendKeys('V', TRUE);
    Sleep(50);
    SendKeys(VK_RETURN, FALSE);

    return TRUE;
}

/* 键盘钩子回调 */
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
        HWND hForeground = GetForegroundWindow();
        
        /* 检查窗口是否切换 */
        if (hForeground != g_lastWindow) {
            ClearInputBuffer();
            g_lastWindow = hForeground;
        }
        
        /* 只在支持的聊天窗口中监听 */
        if (!IsSupportedChatWindow(hForeground)) {
            return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
        }
        
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            /* 处理退格键 */
            if (pKeyboard->vkCode == VK_BACK) {
                RemoveLastCharFromBuffer();
            }
            /* 处理Enter键 */
            else if (pKeyboard->vkCode == VK_RETURN) {
                printf("\n========================================\n");
                printf("[调试] Enter键按下\n");
                printf("当前输入缓冲: \"%s\" (长度: %d)\n", g_inputBuffer, g_inputLength);
                printf("========================================\n");
                
                /* 判断是否需要触发剪贴板获取 */
                if (ShouldTriggerClipboard()) {
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
                            ClearInputBuffer();  /* 清空缓冲区 */
                            ProcessAndSendImage(capturedText);
                            return 1;  /* 阻止Enter键传递 */
                        }
                    }
                }
                
                /* Enter后清空缓冲区 */
                ClearInputBuffer();
            }
            /* 处理普通字符输入 */
            else {
                /* 在低级键盘钩子中，需要使用GetAsyncKeyState获取实时键盘状态 */
                BYTE keyboardState[256] = {0};
                
                /* 手动设置修饰键状态 */
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                    keyboardState[VK_SHIFT] = 0x80;
                    keyboardState[VK_LSHIFT] = 0x80;
                    keyboardState[VK_RSHIFT] = 0x80;
                }
                if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                    keyboardState[VK_CONTROL] = 0x80;
                    keyboardState[VK_LCONTROL] = 0x80;
                    keyboardState[VK_RCONTROL] = 0x80;
                }
                if (GetAsyncKeyState(VK_MENU) & 0x8000) {  /* Alt键 */
                    keyboardState[VK_MENU] = 0x80;
                    keyboardState[VK_LMENU] = 0x80;
                    keyboardState[VK_RMENU] = 0x80;
                }
                if (GetAsyncKeyState(VK_CAPITAL) & 0x0001) {  /* CapsLock锁定状态 */
                    keyboardState[VK_CAPITAL] = 0x01;
                }
                
                /* 使用ToUnicode正确处理组合键和多字节字符 */
                WCHAR unicodeBuffer[10] = {0};
                int result = ToUnicode(
                    pKeyboard->vkCode,
                    pKeyboard->scanCode,
                    keyboardState,
                    unicodeBuffer,
                    10,
                    0
                );
                
                /* result > 0 表示成功获取到字符 */
                if (result > 0) {
                    /* 将Unicode转换为UTF-8 */
                    char utf8Buffer[32] = {0};
                    int utf8Len = WideCharToMultiByte(
                        CP_UTF8,
                        0,
                        unicodeBuffer,
                        result,
                        utf8Buffer,
                        sizeof(utf8Buffer) - 1,
                        NULL,
                        NULL
                    );
                    
                    if (utf8Len > 0) {
                        utf8Buffer[utf8Len] = '\0';
                        AppendToInputBuffer(utf8Buffer);
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

/* 禁用控制台的QuickEdit模式，避免选中文本时程序暂停 */
void DisableConsoleQuickEditMode() {
    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    
    if (GetConsoleMode(hConsole, &mode)) {
        /* 移除 ENABLE_QUICK_EDIT_MODE 标志 */
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        /* 保留 ENABLE_EXTENDED_FLAGS，这样才能修改 QuickEdit */
        mode |= ENABLE_EXTENDED_FLAGS;
        SetConsoleMode(hConsole, mode);
    }
}

/* 主函数 */
int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    
    /* 禁用QuickEdit模式，避免选中文本导致程序暂停 */
    DisableConsoleQuickEditMode();
    
    printf("========================================\n");
    printf("  QQ/微信图片文字发送工具\n");
    printf("========================================\n");
    printf("\n");
    printf("功能说明:\n");
    printf("  • 监听QQ和微信窗口的输入\n");
    printf("  • 检测 #数字 格式自动转换为图片\n");
    printf("  • 支持%d种表情底图（0-%d）\n", NUM_EMOTIONS, NUM_EMOTIONS - 1);
    printf("  • 自动生成并发送图片\n");
    printf("\n");
    printf("使用方法:\n");
    printf("  在QQ/微信输入框输入: 你的文字#0\n");
    printf("  按Enter后会自动转换为图片并发送\n");
    printf("\n");
    printf("表情编号:\n");
    printf("  0-普通  1-开心  2-生气  3-无语\n");
    printf("  4-脸红  5-病娇  6-闭眼  7-难受\n");
    printf("  8-害怕  9-激动  10-惊讶 11-哭泣\n");
    printf("\n");
    printf("示例: 今天天气真好#1 (开心表情)\n");
    printf("\n");
    printf("支持平台: QQ、微信\n");
    printf("按 Ctrl+C 退出程序\n");
    printf("========================================\n\n");
    
    /* 检查资源是否可用 */
    ImageConfig config;
    load_image_config(&config);
    
    printf("正在检查图片资源...\n");
    int loaded_count = 0;
    for (int i = 0; i < NUM_EMOTIONS; i++) {
        ImageData* test_img = load_image_from_resource(config.resource_ids[i]);
        if (test_img) {
            free_image(test_img);
            loaded_count++;
        }
    }
    
    if (loaded_count == 0) {
        printf("错误：未找到任何图片资源！\n");
        printf("请确保程序正确编译并包含资源文件。\n\n");
        system("pause");
        return 1;
    }
    
    printf("✓ 成功加载 %d/%d 个图片资源\n\n", loaded_count, NUM_EMOTIONS);
    
    /* 安装键盘钩子 */
    if (!InstallKeyboardHook()) {
        printf("\n初始化失败，请以管理员身份运行！\n");
        system("pause");
        return 1;
    }
    
    printf("✓ 键盘钩子已安装\n");
    printf("✓ 监控已启动\n");
    printf("\n等待QQ/微信窗口输入...\n\n");
    
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

