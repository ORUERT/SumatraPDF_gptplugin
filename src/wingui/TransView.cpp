extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}
#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include <windows.h>
// #include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>
#include <json.hpp>
#include <cstdio>
#include <codecvt>
#include<curl/curl.h>
#include <GLEW/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 
#include <fstream>  
#include <ctime>  
//#include<curl/easy.h>
#include <time.h>

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
// #include "imgui.h"
// #include "imgui_impl_glfw.h"
// #include "imgui_impl_opengl3.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "Annotation.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "EngineMupdf.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "GlobalPrefs.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "Toolbar.h"

#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Canvas.h"
#include "Commands.h"
#include "TransView.h"
#include "utils/Log.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLEW/glew.h>
#include <GLFW/glfw3.h>
#include <windows.h>
#include <tchar.h>
#include <iostream>
#include "imgui_markdown.h"
#include "utf8.h"
#include <thread>
// #include "SumatraPDF.h"
#include "GlobalPrefs.h"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Normaliz.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
// 回调函数，用于接收响应数据
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void SuccessCallback(const std::string& response) {
    auto jsonResponse = nlohmann::json::parse(response);
    std::string content = jsonResponse["choices"][0]["message"]["content"];
    logf("success %s",content.c_str());
    gTransWindow.AddChatMessage(const_cast<char*>(content.c_str()), 1, jsonResponse["created"].get<time_t>());
}

void FailureCallback() {
    time_t currentTime;
    time(&currentTime);
    char tempStr[] = "connect error or domain/apikey error.please check option settings";
    logf("failed %s",tempStr);
    gTransWindow.AddChatMessage(tempStr,3,currentTime);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
        case WM_CREATE:
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED)
            {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                glViewport(0, 0, width, height);
                gTransWindow.UpdateImGuiWindowSize(width, height); // 更新 ImGui 子窗口大小
                
            }
            return 0;
        case WM_MOVE:
            {
                int xPos = (int)(short)LOWORD(lParam);
                int yPos = (int)(short)HIWORD(lParam);
                gTransWindow.imguiWindowPos = ImVec2(xPos,yPos);
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;
        case WM_CLOSE:
            // Hide the window instead of closing it
            ShowWindow(hWnd, SW_HIDE);
            // tab->lastEditAnnotsWindowPos = WindowRect(hwnd);
            // auto cr = ClientRect(hwnd);
            // tab->lastEditAnnotsWindowPos.dx = cr.dx;
            // tab->lastEditAnnotsWindowPos.dy = cr.dy;
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void TransWindow::SetAlwaysOnTop(bool alwaysOnTop) {
    // 设置窗口的显示层级
    SetWindowPos(hwnd,
                 alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

TransWindow::TransWindow(){
    showImGuiWindow = true;
    buttonEnabled = true;
    // 注册窗口类
    wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    RegisterClassEx(&wc);
    hwnd = CreateWindow(wc.lpszClassName, _T("Dear ImGui Standalone Example"), WS_OVERLAPPEDWINDOW, 100, 100, 500, 900, NULL, NULL, wc.hInstance, NULL);
    CreateOpenGLContext(hwnd);
    glewInit();

    InitializeImGui(hwnd);
    SetImGuiWindowPosition(0.0f, 0.0f);
    SetAlwaysOnTop(true);
}

TransWindow::~TransWindow(){
    // Cleanup
    CleanupImGui();
    CleanupOpenGLContext();
    DestroyWindow(gTransWindow.hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
}

void TransWindow::SetImGuiWindowPosition(float x, float y) {
    imguiWindowPos = ImVec2(x, y);
}

void TransWindow::CreateOpenGLContext(HWND hWnd)
{
    PIXELFORMATDESCRIPTOR pfd = { 
        sizeof(PIXELFORMATDESCRIPTOR),   // size of this pfd
        1,                     // version number
        PFD_DRAW_TO_WINDOW |   // support window
        PFD_SUPPORT_OPENGL |   // support OpenGL
        PFD_DOUBLEBUFFER,      // double buffered
        PFD_TYPE_RGBA,         // RGBA type
        24,                    // 24-bit color depth
        0, 0, 0, 0, 0, 0,      // color bits ignored
        0,                     // no alpha buffer
        0,                     // shift bit ignored
        0,                     // no accumulation buffer
        0, 0, 0, 0,            // accum bits ignored
        16,                    // 16-bit z-buffer
        0,                     // no stencil buffer
        0,                     // no auxiliary buffer
        PFD_MAIN_PLANE,        // main layer
        0,                     // reserved
        0, 0, 0                // layer masks ignored
    }; 

    g_HDC = GetDC(hWnd);
    int pixelFormat = ChoosePixelFormat(g_HDC, &pfd);
    SetPixelFormat(g_HDC, pixelFormat, &pfd);

    g_GLContext = wglCreateContext(g_HDC);
    wglMakeCurrent(g_HDC, g_GLContext);
}

void TransWindow::UpdateImGuiWindowSize(int width, int height)
{
    imguiWindowSize = ImVec2(width, height);
}

void TransWindow::CleanupOpenGLContext()
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(g_GLContext);
    ReleaseDC(GetActiveWindow(), g_HDC);
}

void PerformRequest(const std::string& question,const std::string& domain, const std::string& apiKey) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    // 初始化libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // 设置URL
        curl_easy_setopt(curl, CURLOPT_URL, domain.c_str());

        // 设置HTTP头
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // 创建JSON数据
        nlohmann::json jres;
        jres["model"] = "gpt-3.5-turbo";
        jres["messages"] = nlohmann::json::array();
        nlohmann::json message1 = {
            {"role", "user"},
            {"content", question}
        };
        jres["messages"].push_back(message1);
        std::string jsonData = jres.dump();

        // 设置POST数据
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());

        // 设置回调函数接收响应数据
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // 设置超时
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 设置超时时间为10秒

        // 执行请求
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            FailureCallback();
        } else {
            SuccessCallback(readBuffer);
        }

        // 清理
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    // 恢复按钮状态
    gTransWindow.buttonEnabled = true;
}

bool isNonEmptyString(const std::string& str) {
    return !str.empty() && std::any_of(str.begin(), str.end(), [](unsigned char c) {
        return !std::isspace(c);
    });
}

void SendMessageCallback1(const char* message) {

    auto question = std::string(message);
    std::string apiKey = gGlobalPrefs->chatgptapi?std::string(gGlobalPrefs->chatgptapi):std::string("");
    std::string domain = gGlobalPrefs->chatgptdomin?std::string(gGlobalPrefs->chatgptdomin):std::string("");
    if((!isNonEmptyString(apiKey))||(!isNonEmptyString(domain))){
        ShowOptionsDialog(gTransWindow.win);
    }
    // 禁用按钮
    gTransWindow.buttonEnabled = false;

    // 创建一个新线程来执行请求
    std::thread requestThread(PerformRequest, question,domain, apiKey);

    // 分离线程，以便它在后台运行
    requestThread.detach();
}

void TransWindow::InitializeImGui(HWND hwnd)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");
        // 设置初始窗口位置和尺寸
    gTransWindow.onSendMessage = SendMessageCallback1; // 设置回调函数

    ImFontConfig font_cfg;
    font_cfg.MergeMode = false;
    font_cfg.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("../fonts/MSYH.ttc", 18.0f, &font_cfg, io.Fonts->GetGlyphRangesChineseFull());
    fontNormal = io.Fonts->AddFontFromFileTTF("../fonts/MSYH.ttc", 18.0f, &font_cfg, io.Fonts->GetGlyphRangesChineseFull()); // Normal font size
    fontSmall = io.Fonts->AddFontFromFileTTF("../fonts/MSYH.ttc", 12.0f, &font_cfg, io.Fonts->GetGlyphRangesChineseFull());  // Smaller font size
}

void TransWindow::SubmitMessage(const char * prefixStr) {
    if (inputBuffer[0] != '\0') {
        time_t currentTime;
        time(&currentTime);
        AddChatMessage(inputBuffer,0,currentTime);
        std::string tempBuffer = std::string(prefixStr)+"\""+std::string(inputBuffer)+"\"";
        inputBuffer[0] = '\0';  // 清空输入框
        if (onSendMessage) {
            onSendMessage(tempBuffer.c_str());
        }
    }
}

void RenderMarkdownText(char * markdown_text) {
    // 配置 Markdown 渲染设置
    ImGui::MarkdownConfig mdConfig;
    mdConfig.linkCallback = nullptr; // 链接回调函数
    mdConfig.imageCallback = nullptr; // 图片回调函数
    mdConfig.tooltipCallback = nullptr; // 工具提示回调函数
    mdConfig.linkIcon = ""; // 链接图标
    mdConfig.headingFormats[0] = { ImGui::GetFont(), true }; // 一级标题格式
    mdConfig.headingFormats[1] = { ImGui::GetFont(), true }; // 二级标题格式
    mdConfig.headingFormats[2] = { ImGui::GetFont(), false }; // 三级标题格式

    // 渲染 Markdown 内容
    ImGui::Markdown(markdown_text, strlen(markdown_text), mdConfig);
}

void TransWindow::RenderImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    // SetImGuiWindowPosition(500,900);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove| ImGuiWindowFlags_NoResize |ImGuiWindowFlags_NoTitleBar ;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always); // 位置 (x, y)
    ImGui::SetNextWindowSize(imguiWindowSize, ImGuiCond_Always);
    // 1. Show a simple window


    ImGui::Begin("Standalone ImGui Window",nullptr,window_flags);
    // 显示聊天消息
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // 设置背景颜色为黑色
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // 设置文本颜色为白色

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true, ImGuiWindowFlags_HorizontalScrollbar|ImGuiWindowFlags_NoMove);
    static std::string chatContent;
    chatContent.clear();
    for(int i = 0 ; i < messages.len ; i ++) {
        auto msg = messages.at(i);
        ImGui::PushFont(fontSmall);
        if (msg->role!=0) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Server");
        } else {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Client");
        }
        ImGui::PopFont();

        RenderMarkdownText(msg->content);


        ImGui::Separator(); // 分隔每条消息

    }
    ImGui::EndChild();
    
    ImGui::PopStyleColor(2); // 恢复颜色设置

    float chatWidth = ImGui::GetContentRegionAvail().x;

    // 输入框
    ImGui::PushItemWidth(chatWidth);
    if (ImGui::InputText("##input", inputBuffer, IM_ARRAYSIZE(inputBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        SubmitMessage("");
    }
    ImGui::PopItemWidth();
    // 发送按钮
    float totalWidth = ImGui::GetContentRegionAvail().x;
    float buttonWidth = (totalWidth - ImGui::GetStyle().ItemSpacing.x) / 2;
    ImGui::BeginDisabled(!buttonEnabled);
    if (ImGui::Button("Send",ImVec2(buttonWidth, 0))) {
        SubmitMessage("");
    }
    ImGui::EndDisabled(); // 恢复按钮
    // float cursorPosX = chatWidth/2;

    // 右对齐按钮
    ImGui::SameLine(); // 保证在同一行
    // ImGui::SetCursorPosX(cursorPosX); // 设置光标位置以实现右对齐
    ImGui::BeginDisabled(!buttonEnabled);
    if (ImGui::Button("Translate",ImVec2(buttonWidth, 0))) {
        const char * tempstr = "zh-en translation of ";
        SubmitMessage(tempstr);
    }
    ImGui::EndDisabled(); // 恢复按钮
    ImGui::End();
    

    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SwapBuffers(g_HDC);
}

void TransWindow::CleanupImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void TransWindow::AddChatMessage(char *content,int role,time_t timestamp) {
    messages.Append(new ChatMessage(content,role,timestamp));
}

void ShowNewWindow(MainWindow* win){
    if(gTransWindow.win == nullptr){
        gTransWindow.win = win;
    }
    ShowWindow(gTransWindow.hwnd, SW_SHOWDEFAULT);
    UpdateWindow(gTransWindow.hwnd);
}

void AppendTextToChatWindow(const char* s){
    strncpy(gTransWindow.inputBuffer, s, sizeof(gTransWindow.inputBuffer) - 1);
}
