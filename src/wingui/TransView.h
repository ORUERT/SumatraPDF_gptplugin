// struct EditAnnotationsWindow;
#include "imgui.h"
struct TransWindow;
//struct Wnd;
struct MainWindow; // 前向声明  
// struct LayoutBase; // 前向声明  
// struct Static; // 前向声明  
// struct Chat; // 前向声明  
// struct Edit; // 前向声明  
// struct Button; // 前向声明
// zzz:Wnd{};
struct GLFWwindow;
struct TransWindow;
struct ChatMessage;

struct TransWindow {
    TransWindow();
    ~TransWindow();
    HGLRC g_GLContext;
    WNDCLASSEX wc;
    HDC g_HDC;
    HWND hwnd;
    ImVec2 imguiWindowSize; // 定义 imguiWindowSize
    ImVec2 imguiWindowPos;
    bool showImGuiWindow; // 控制 ImGui 窗口显示与否
    bool buttonEnabled;
    ImFont* fontNormal;
    ImFont* fontSmall;
    // char* chatgptDomain;
    // char* chatgptAPI;
    Vec<ChatMessage*> messages;
    MainWindow * win;

    char inputBuffer[1024];
    std::function<void(const char*)> onSendMessage;

    void CreateOpenGLContext(HWND hWnd);
    void CleanupOpenGLContext();
    void InitializeImGui(HWND hwnd);
    void RenderImGui();
    void CleanupImGui();
    void UpdateImGuiWindowSize(int width, int height);
    void HideWindow(); // 隐藏 Win32 窗口
    void SetImGuiWindowPosition(float x, float y); // 新增方法设置位置
    void SubmitMessage(const char * suffixStr);
    void AddChatMessage(char *content,int role,time_t timestamp);
    void SetAlwaysOnTop(bool alwaysOnTop);
};


void ShowNewWindow(MainWindow* win);
void AppendTextToChatWindow(const char* s);
