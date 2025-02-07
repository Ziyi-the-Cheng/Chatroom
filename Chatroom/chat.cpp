#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include "Client.h"
#include <vector>
#include <map>
#include <set>

#pragma comment(lib,"d3d11.lib")

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    
    // 变量定义
    static bool isConnected = false;    // 用于控制是否连接
    static std::string username = "";    // 存储用户名
    static std::vector<std::string> activeUsers; // 存储已经加入聊天室的用户

    static std::vector<std::string> messages;
    std::string message;
    std::thread cli = std::thread(client, std::ref(message));

    static std::map<std::string, std::vector<std::string>> privateMessages; // 每个用户都有独立的消息列表
    static std::set<std::string> openChatWindows; // 跟踪已打开的私聊窗口


    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        
        
        {
            // 新建窗口用于输入用户名和连接按钮
            if (!isConnected) {
                ImGui::Begin("Login", nullptr, ImGuiWindowFlags_NoCollapse);
                ImGui::Text("Enter your username:");

                // 输入框
                static char inputText[256] = "";
                ImGui::InputText("##username", inputText, IM_ARRAYSIZE(inputText));

                // 连接按钮
                if (strlen(inputText) > 0) {
                    if (ImGui::Button("Connect", ImVec2(100, 30))) {
                        if (isConnected && cli.joinable()) {
                            cli.detach();
                        }
                        username = inputText;  // 保存用户名
                        isConnected = true;  // 连接成功
                        activeUsers.push_back(inputText); // 将用户名添加到活跃用户列表
                        inputText[0] = '\0';  // 清空输入框
                        SendMessageToServer(client_socket, username);
                    }
                }
                else {
                    ImGui::BeginDisabled();
                    ImGui::Button("Connect", ImVec2(100, 30));
                    ImGui::EndDisabled();
                }

                ImGui::End();
            }
            else {
                // 显示聊天室窗口
                ImGui::Begin("Chat Room", nullptr, ImGuiWindowFlags_NoCollapse);

                // 更新消息
                if (message.size() > 0) {
                    if (message.front() == '+') {
                        std::cout << "a name added" << "\n";
                        message.erase(0, 1);
                        activeUsers.push_back(message);
                        message = message + " joined the channel!";
                        messages.push_back(message);
                    }
                    else if (message.front() == '-') {
                        std::cout << "a name delete" << "\n";
                        message.erase(0, 1);
                        auto it = std::find(activeUsers.begin(), activeUsers.end(), message);
                        activeUsers.erase(it);
                        message = message + " is disconnected!";
                        messages.push_back(message);
                    }
                    else if (message.front() == '#') {
                        std::cout << "recieved a DM";
                        size_t start = message.find('#') + 1;   // 找到 '#' 后面的第一个字符
                        size_t end = message.find(':');         // 找到 ':'
                        std::string sender = message.substr(start, end - start);  // 提取 'neil'
                        std::string content = message.substr(end + 1);
                        std::string output = sender + ": " + content;
                        if (openChatWindows.find(sender) == openChatWindows.end()) {
                            openChatWindows.insert(sender); // 打开私聊窗口
                        }
                        privateMessages[sender].push_back(output);
                    }
                    else {
                        std::cout << "a message" << "\n";
                        messages.push_back(message);
                    }
                }
                message.clear();

                // 左侧用户列表，显示活跃用户
                ImGui::BeginChild("UserList", ImVec2(200, 0), true);
                ImGui::Text("Users:");
                ImGui::Separator();
                for (const auto& user : activeUsers) {
                    if (ImGui::Selectable(user.c_str())) {
                        openChatWindows.insert(user); // 打开私聊窗口
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();

                // 右侧聊天窗口
                ImGui::BeginChild("ChatWindow", ImVec2(0, -60), true);
                ImGui::Text("Chat Messages:");
                ImGui::Separator();

                for (const auto& message : messages) {
                    ImGui::TextWrapped("%s", message.c_str());
                }

                ImGui::EndChild();

                // 处理私聊窗口
                for (auto it = openChatWindows.begin(); it != openChatWindows.end(); ) {
                    bool isOpen = true;
                    ImGui::Begin(((*it) + " (Private Chat)").c_str(), &isOpen, ImGuiWindowFlags_NoCollapse);

                    // 私聊消息显示
                    ImGui::BeginChild("PrivateChatWindow", ImVec2(0, -60), true);
                    for (const auto& privateMsg : privateMessages[*it]) {
                        ImGui::TextWrapped("%s", privateMsg.c_str());
                    }
                    ImGui::EndChild();

                    // 私聊消息输入框和发送按钮
                    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 60);
                    ImGui::Separator();
                    static char privateInputText[256] = "";
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 110);
                    if (ImGui::InputText(("##input" + *it).c_str(), privateInputText, IM_ARRAYSIZE(privateInputText), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        if (strlen(privateInputText) > 0) {
                            privateMessages[*it].push_back("You: " + std::string(privateInputText));
                            SendPrivateMessageToServer(client_socket, *it, privateInputText);
                            privateInputText[0] = '\0';  // 清空输入框
                        }
                    }
                    ImGui::SameLine();

                    if (ImGui::Button(("Send##" + *it).c_str(), ImVec2(100, 30))) {
                        if (strlen(privateInputText) > 0) {
                            privateMessages[*it].push_back("You: " + std::string(privateInputText));
                            SendPrivateMessageToServer(client_socket, *it, privateInputText);
                            privateInputText[0] = '\0';  // 清空输入框
                        }
                    }

                    ImGui::End();

                    // 如果窗口被关闭，移除该私聊窗口
                    if (!isOpen) {
                        it = openChatWindows.erase(it);
                    }
                    else {
                        ++it;
                    }
                }



                // 聊天输入框和按钮在聊天窗口下方
                ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 60);
                ImGui::Separator();
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 220);
                static char inputText[256] = "";
                if (ImGui::InputText("##input", inputText, IM_ARRAYSIZE(inputText), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (strlen(inputText) > 0) {
                        messages.push_back(username + ": " + std::string(inputText));
                        SendMessageToServer(client_socket, inputText);
                        inputText[0] = '\0';  // 清空输入框
                    }
                }
                ImGui::SameLine();

                // 发送按钮
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
                if (ImGui::Button("Send", ImVec2(100, 30))) {
                    if (strlen(inputText) > 0) {
                        messages.push_back(username + ": " + std::string(inputText));
                        SendMessageToServer(client_socket, inputText);
                        inputText[0] = '\0';  // 清空输入框
                    }
                }
                ImGui::PopStyleColor(3);

                // 断开连接按钮放在底部右侧
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button("Disconnect", ImVec2(100, 30))) {
                    std::string dmessage = "-" + username;
                    SendMessageToServer(client_socket, dmessage);
                    // 回到登录界面，重置状态
                    isConnected = false;
                    activeUsers.clear();
                    messages.clear();
                    username.clear();
                    message.clear();
                    done = true;
                }
                ImGui::PopStyleColor(3);


                ImGui::End();
            }

        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
