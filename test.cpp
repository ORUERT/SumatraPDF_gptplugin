#include <windows.h>
#include <GL/glew.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"

// 数据
static HGLRC g_GLContext; // OpenGL 渲染上下文
static HDC g_HDC;         // 设备上下文
static HWND g_hWnd;       // 主窗口句柄

// 从 imgui_impl_win32.cpp 中声明消息处理函数
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 消息处理函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 处理 ImGui 的输入消息
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE: // 处理窗口大小变化消息
        if (wParam != SIZE_MINIMIZED)
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            glViewport(0, 0, width, height); // 调整视口大小
        }
        return 0;
    case WM_SYSCOMMAND: // 处理系统命令消息
        if ((wParam & 0xfff0) == SC_KEYMENU) // 禁用 ALT 菜单键
            return 0;
        break;
    case WM_DESTROY: // 处理窗口销毁消息
        PostQuitMessage(0); // 发送退出消息
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam); // 默认处理其他消息
}

// 创建 OpenGL 上下文
void CreateOpenGLContext(HWND hWnd)
{
    PIXELFORMATDESCRIPTOR pfd = { 
        sizeof(PIXELFORMATDESCRIPTOR),   // pfd 大小
        1,                     // 版本号
        PFD_DRAW_TO_WINDOW |   // 支持窗口
        PFD_SUPPORT_OPENGL |   // 支持 OpenGL
        PFD_DOUBLEBUFFER,      // 双缓冲
        PFD_TYPE_RGBA,         // RGBA 类型
        24,                    // 24 位颜色深度
        0, 0, 0, 0, 0, 0,      // 忽略颜色位
        0,                     // 无 alpha 缓冲
        0,                     // 忽略 shift 位
        0,                     // 无累积缓冲
        0, 0, 0, 0,            // 忽略累积位
        16,                    // 16 位 z-buffer
        0,                     // 无模板缓冲
        0,                     // 无辅助缓冲
        PFD_MAIN_PLANE,        // 主层
        0,                     // 保留
        0, 0, 0                // 忽略层掩码
    }; 

    g_HDC = GetDC(hWnd); // 获取设备上下文
    int pixelFormat = ChoosePixelFormat(g_HDC, &pfd); // 选择像素格式
    SetPixelFormat(g_HDC, pixelFormat, &pfd); // 设置像素格式

    g_GLContext = wglCreateContext(g_HDC); // 创建 OpenGL 上下文
    wglMakeCurrent(g_HDC, g_GLContext); // 激活当前 OpenGL 上下文
}

// 清理 OpenGL 上下文
void CleanupOpenGLContext()
{
    wglMakeCurrent(NULL, NULL); // 取消当前 OpenGL 上下文
    wglDeleteContext(g_GLContext); // 删除 OpenGL 上下文
    ReleaseDC(GetActiveWindow(), g_HDC); // 释放设备上下文
}

// 初始化 ImGui
void InitializeImGui(HWND hwnd)
{
    // 初始化 Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark(); // 使用暗色主题

    // 初始化平台/渲染器绑定
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");
}

// 渲染 ImGui 窗口
void RenderImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 显示一个独立的 ImGui 窗口
    ImGui::Begin("Standalone ImGui Window");
    ImGui::Text("This is some useful text.");
    ImGui::End();

    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SwapBuffers(g_HDC); // 交换前后缓冲
}

// 清理 ImGui
void CleanupImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

// 自定义的消息循环
static int RunMessageLoop()
{
    MSG msg;
    HACCEL accels;
    HWND hwndDialog;
    HWND hwndAccel;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (PreTranslateMessage(msg))
        {
            continue;
        }

        // 检查是否处理加速键
        bool doAccels = ((msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) ||
                         (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST));

        if (doAccels)
        {
            accels = FindAcceleratorsForHwnd(msg.hwnd, &hwndAccel);
            if (accels)
            {
                auto didTranslate = TranslateAccelerator(hwndAccel, accels, &msg);
                if (didTranslate)
                {
                    continue;
                }
            }
        }

        hwndDialog = GetCurrentModelessDialog();
        if (hwndDialog && IsDialogMessage(hwndDialog, &msg))
        {
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
        ResetTempAllocator();

        // 渲染 ImGui 窗口
        RenderImGui();
    }

    return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // 注册窗口类
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    RegisterClassEx(&wc);
    g_hWnd = CreateWindow(wc.lpszClassName, _T("Dear ImGui Standalone Example"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    // 初始化 OpenGL
    CreateOpenGLContext(g_hWnd);
    glewInit();

    // 显示窗口
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // 初始化 ImGui
    InitializeImGui(g_hWnd);

    // 运行消息循环
    RunMessageLoop();

    // 清理
    CleanupImGui();
    CleanupOpenGLContext();
    DestroyWindow(g_hWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}
