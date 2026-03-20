/*
lg-reminder
在 Windows 通知弹窗提醒洛谷私信
==================================================
@version win.0.6
@author Gary0
@license MIT
本脚本由洛谷 @Gary0 开发
感谢洛谷 @PenaltyKing 提供的思路及建议
==================================================
新增功能：
- 系统托盘图标和菜单
- 后台运行（可隐藏控制台）
==================================================
*/

#define lg_reminder_version "win.0.6"
#define lg_reminder_author "Gary0"

#include <iostream>
#include <string>
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <shlobj.h>
#include <commctrl.h>
#include <cstring>
#include <cctype>
#include <vector>
#include <regex>
#include <iomanip>
#include <atomic>
#include <mutex>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

using namespace std;

// 全局变量
atomic<bool> g_running(true);
atomic<bool> g_checking(false);
atomic<int> g_check_interval(30);
HWND g_hwnd = NULL;
NOTIFYICONDATAA g_nid = {};
string g_cookie, g_username;
vector<int> g_history_ids;
mutex g_history_mutex;
bool g_console_visible = true;

// 菜单项ID
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SHOW 1002
#define ID_TRAY_HIDE 1003
#define ID_TRAY_ABOUT 1004
#define ID_TRAY_SETTINGS 1005

struct Msg { 
    int id, uid; 
    string name, time, con; 
    bool is_new; 
};

// 前置声明
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateTrayIcon(HWND hwnd);
void RemoveTrayIcon();
void ShowContextMenu(HWND hwnd);
void ShowAboutDialog(HWND hwnd);
void ShowSettingsDialog(HWND hwnd);
void CheckMessages();
void SaveHistory(const vector<int>& ids);
vector<int> LoadHistory();
void ToggleConsole();
void HideConsole();
void ShowConsole();

string now() {
    SYSTEMTIME st; 
    GetLocalTime(&st);
    char b[64]; 
    sprintf_s(b, sizeof(b), "%04d-%02d-%02d %02d:%02d:%02d", 
              st.wYear, st.wMonth, st.wDay, 
              st.wHour, st.wMinute, st.wSecond);
    return b;
}

void SetConsoleUTF8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

string utf8_to_system(const string &utf8_str) {
    if (utf8_str.empty())
        return "";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, NULL, 0);
    if (!wlen)
        return utf8_str;
    wchar_t *wstr = new wchar_t[wlen];
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, wstr, wlen);
    int glen = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (!glen) {
        delete[] wstr;
        return utf8_str;
    }
    char *gstr = new char[glen];
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, gstr, glen, NULL, NULL);
    string result(gstr);
    delete[] wstr;
    delete[] gstr;
    return result;
}

string dec(string s) {
    string r;
    for (size_t i = 0; i < s.length(); i++) {
        if (s[i] == '\\' && i + 5 < s.length() && s[i + 1] == 'u') {
            char h[5] = {s[i + 2], s[i + 3], s[i + 4], s[i + 5], 0};
            int c = stoi(h, nullptr, 16);
            if (c < 0x80) r += (char)c;
            else if (c < 0x800) {
                r += (char)(0xC0 | (c >> 6));
                r += (char)(0x80 | (c & 0x3F));
            } else {
                r += (char)(0xE0 | (c >> 12));
                r += (char)(0x80 | ((c >> 6) & 0x3F));
                r += (char)(0x80 | (c & 0x3F));
            }
            i += 5;
        } else {
            r += s[i];
        }
    }
    return r;
}

string ext(string h) {
    string p = "window._feInjection = JSON.parse(decodeURIComponent(\"";
    size_t l = h.find(p); 
    if (l == string::npos) return "";
    l += p.length(); 
    size_t r = h.find("\"));", l);
    return (r == string::npos) ? "" : h.substr(l, r - l);
}

string url(string e) {
    string d;
    for (size_t i = 0; i < e.length(); i++) {
        if (e[i] == '%' && i + 2 < e.length()) {
            int v; 
            sscanf_s(e.substr(i + 1, 2).c_str(), "%x", &v);
            d += (char)v, i += 2;
        }
        else d += e[i];
    }
    return d;
}

vector<Msg> parse(string j) {
    vector<Msg> v;
    string s1 = "\"latestMessages\":{\"result\":[", s2 = "\"result\":[";
    size_t st = j.find(s1); 
    if (st == string::npos) { 
        st = j.find(s2); 
        if (st == string::npos) return v; 
        st += s2.length();
    }
    else st += s1.length();
    
    size_t p = st;
    while (1) {
        p = j.find("{", p); 
        if (p == string::npos || p > j.find("]", st)) break;
        
        Msg m = {0, 0, "", "", "", false};
        int bc = 1; 
        size_t ed = p + 1;
        while (ed < j.length() && bc > 0) { 
            if (j[ed] == '{') bc++; 
            else if (j[ed] == '}') bc--; 
            ed++; 
        }
        string o = j.substr(p, ed - p);
        
        size_t fid = o.find("\"id\":");
        if (fid != string::npos) {
            size_t a = o.find_first_of("0123456789", fid + 5);
            size_t b = o.find_first_not_of("0123456789", a);
            if (a != string::npos && b != string::npos)
                m.id = atoi(o.substr(a, b - a).c_str());
        }
        
        size_t ftime = o.find("\"time\":");
        if (ftime != string::npos) {
            size_t a = o.find_first_of("0123456789", ftime + 6);
            size_t b = o.find_first_not_of("0123456789", a);
            if (a != string::npos && b != string::npos) {
                time_t t = atol(o.substr(a, b - a).c_str());
                struct tm *ti = localtime(&t);
                char buf[16]; 
                strftime(buf, 16, "%m-%d %H:%M", ti);
                m.time = buf;
            }
        }
        
        size_t fsender = o.find("\"sender\":");
        if (fsender != string::npos) {
            size_t np = o.find("\"name\":\"", fsender);
            if (np != string::npos) {
                np += 8; 
                size_t ne = o.find("\"", np);
                if (ne != string::npos) 
                    m.name = dec(o.substr(np, ne - np));
            }
            size_t up = o.find("\"uid\":", fsender);
            if (up != string::npos) {
                size_t a = o.find_first_of("0123456789", up + 5);
                size_t b = o.find_first_not_of("0123456789", a);
                if (a != string::npos && b != string::npos)
                    m.uid = atoi(o.substr(a, b - a).c_str());
            }
        }
        
        size_t fcon = o.find("\"content\":");
        if (fcon != string::npos) {
            size_t cs = o.find("\"", fcon + 9);
            if (cs != string::npos) {
                cs++; 
                size_t ce = cs; 
                bool esc = 0;
                while (ce < o.length()) {
                    if (o[ce] == '\\' && !esc) esc = 1;
                    else if (o[ce] == '"' && !esc) break;
                    else esc = 0;
                    ce++;
                }
                if (ce < o.length()) {
                    string rc = o.substr(cs, ce - cs);
                    m.con = dec(rc);
                    if (m.con.length() > 30) 
                        m.con = m.con.substr(0, 27) + "...";
                }
            }
        }
        
        if (m.id && !m.name.empty()) 
            v.push_back(m);
        p = ed;
    }
    return v;
}

void noti(vector<Msg> v, string us) {
    if (v.empty()) return;
    for (auto &m : v) {
        string name_sys = utf8_to_system(m.name);
        string us_sys = utf8_to_system(us);
        
        if (name_sys != us_sys) {
            string t = utf8_to_system("洛谷新私信 - 来自 ") + name_sys;
            string c = m.con.empty() ? utf8_to_system("您有一条新消息") : utf8_to_system(m.con);
            
            NOTIFYICONDATAA n = {};
            n.cbSize = sizeof(NOTIFYICONDATAA);
            n.hWnd = g_hwnd;
            n.uID = m.id;
            n.uFlags = NIF_INFO | NIF_ICON | NIF_TIP;
            n.dwInfoFlags = NIIF_INFO | NIIF_LARGE_ICON;
            n.uTimeout = 5000;
            strncpy_s(n.szInfoTitle, t.c_str(), sizeof(n.szInfoTitle) - 1);
            strncpy_s(n.szInfo, c.c_str(), sizeof(n.szInfo) - 1);
            strncpy_s(n.szTip, "lg-reminder", sizeof(n.szTip) - 1);
            HICON h = LoadIcon(NULL, IDI_INFORMATION);
            if (h) n.hIcon = h, n.uFlags |= NIF_ICON;
            Shell_NotifyIconA(NIM_ADD, &n);
            Sleep(2000);
            Shell_NotifyIconA(NIM_DELETE, &n);
            Sleep(500);
        }
    }
}

bool http(string ck, string &r) {
    HINTERNET s = NULL, c = NULL, q = NULL;
    s = WinHttpOpen(L"lg-reminder/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (s) c = WinHttpConnect(s, L"www.luogu.com.cn", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (c) q = WinHttpOpenRequest(c, L"GET", L"/chat", NULL, WINHTTP_NO_REFERER, 
                                    WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (q) {
        int l = MultiByteToWideChar(CP_UTF8, 0, ck.c_str(), -1, NULL, 0);
        wchar_t *w = new wchar_t[l];
        MultiByteToWideChar(CP_UTF8, 0, ck.c_str(), -1, w, l);
        wstring h = L"Cookie: "; h += w; 
        h += L"\r\nUser-Agent: Mozilla/5.0\r\nAccept: text/html\r\nAccept-Language: zh-CN\r\n";
        delete[] w;
        WinHttpAddRequestHeaders(q, h.c_str(), h.length(), WINHTTP_ADDREQ_FLAG_ADD);
        if (!WinHttpSendRequest(q, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) goto end;
        if (!WinHttpReceiveResponse(q, NULL)) goto end;
        DWORD sz = 0, d = 0; char *b;
        do {
            if (!WinHttpQueryDataAvailable(q, &sz) || !sz) break;
            b = new char[sz + 1]; 
            ZeroMemory(b, sz + 1);
            if (WinHttpReadData(q, b, sz, &d)) r.append(b, d);
            delete[] b;
        } while (sz > 0);
    }
    end:
    if (q) WinHttpCloseHandle(q);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return true;
}

bool cfg(string &c, string &u, int &t) {
    ifstream f("config.txt");
    if (!f.is_open()) {
        ofstream o("config.txt");
        if (o.is_open()) {
            string config =
                "# lg-reminder 配置\n\n"
                "cookie=你的完整cookie\n\n"
                "# 用户名\nusername=你的洛谷用户名\n\n"
                "# 轮询间隔\ninterval=15";
            o << utf8_to_system(config);
            o.close();
        }
        return false;
    }
    string content;
    char buffer[4096];
    while (f.read(buffer, sizeof(buffer))) {
        content.append(buffer, f.gcount());
    }
    content.append(buffer, f.gcount());
    f.close();

    if (content.size() >= 3 &&
        (unsigned char)content[0] == 0xEF &&
        (unsigned char)content[1] == 0xBB &&
        (unsigned char)content[2] == 0xBF) {
        content = content.substr(3);
    }

    stringstream ss(content);
    string line;
    while (getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#')
            continue;
        size_t p = line.find('=');
        if (p == string::npos)
            continue;

        string k = line.substr(0, p);
        string v = line.substr(p + 1);

        if (k == "cookie")
            c = v;
        else if (k == "username")
            u = v;
        else if (k == "interval")
            t = stoi(v);
    }
    return !c.empty() && c.find("你的") == string::npos;
}

void SaveHistory(const vector<int>& v) {
    ofstream f("data.txt", ios::binary);
    if (f.is_open()) {
        for (int i : v) {
            f << i << "\n";
        }
        f.close();
    }
}

vector<int> LoadHistory() {
    vector<int> v;
    ifstream f("data.txt", ios::binary);
    if (f.is_open()) {
        string line;
        while (getline(f, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                v.push_back(stoi(line));
            }
        }
        f.close();
    }
    return v;
}

vector<Msg> findnew(vector<Msg> cur, vector<int> lst) {
    if (lst.empty()) return cur;
    vector<Msg> n;
    for (auto &m : cur) {
        bool ok = false;
        for (int i : lst) {
            if (m.id == i) {
                ok = true;
                break;
            }
        }
        if (!ok) n.push_back(m);
    }
    return n;
}

void CheckMessages() {
    if (g_checking) return;
    g_checking = true;
    
    string htm;
    if (http(g_cookie, htm)) {
        string e = ext(htm);
        if (!e.empty()) {
            string j = url(e);
            vector<Msg> v = parse(j);
            if (!v.empty()) {
                vector<int> ids;
                for (auto &m : v) ids.push_back(m.id);
                
                lock_guard<mutex> lock(g_history_mutex);
                vector<Msg> nw = findnew(v, g_history_ids);
                if (!nw.empty()) {
                    // 输出到控制台（如果可见）
                    if (g_console_visible) {
                        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
                        SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                        cout << "[" << now() << "] 发现新消息:";
                        for (auto &m : nw) cout << " " << m.name;
                        cout << endl;
                    }
                    noti(nw, g_username);
                    g_history_ids = ids;
                    SaveHistory(ids);
                }
            }
        }
    }
    g_checking = false;
}

void CheckMessagesLoop() {
    while (g_running) {
        CheckMessages();
        this_thread::sleep_for(chrono::seconds(g_check_interval));
    }
}

// 窗口过程
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateTrayIcon(hwnd);
            break;
            
        case WM_DESTROY:
            RemoveTrayIcon();
            PostQuitMessage(0);
            break;
            
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                ShowContextMenu(hwnd);
            }
            break;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_TRAY_SHOW:
                    ShowConsole();
                    break;
                case ID_TRAY_HIDE:
                    HideConsole();
                    break;
                case ID_TRAY_ABOUT:
                    ShowAboutDialog(hwnd);
                    break;
                case ID_TRAY_SETTINGS:
                    ShowSettingsDialog(hwnd);
                    break;
                case ID_TRAY_EXIT:
                    g_running = false;
                    DestroyWindow(hwnd);
                    break;
            }
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateTrayIcon(HWND hwnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATAA);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_INFORMATION);
    strncpy_s(g_nid.szTip, "lg-reminder - 洛谷私信提醒", sizeof(g_nid.szTip) - 1);
    Shell_NotifyIconA(NIM_ADD, &g_nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIconA(NIM_DELETE, &g_nid);
}

void ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    
    InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_SHOW, "显示控制台");
    InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_HIDE, "隐藏控制台");
    InsertMenuA(hMenu, -1, MF_SEPARATOR, 0, NULL);
    InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_SETTINGS, "设置");
    InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_ABOUT, "关于");
    InsertMenuA(hMenu, -1, MF_SEPARATOR, 0, NULL);
    InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_EXIT, "退出");
    
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

void ShowAboutDialog(HWND hwnd) {
    string msg = "lg-reminder " + string(lg_reminder_version) + 
                 "\n作者: " + string(lg_reminder_author) +
                 "\n\n洛谷私信提醒工具\n运行于系统托盘\n\n感谢使用！";
    MessageBoxA(hwnd, msg.c_str(), "关于 lg-reminder", MB_OK | MB_ICONINFORMATION);
}

void ShowSettingsDialog(HWND hwnd) {
    // 简单的设置对话框
    string current_interval = to_string(g_check_interval);
    char buffer[10];
    strcpy_s(buffer, current_interval.c_str());
    
    string prompt = "轮询间隔（秒）:\n当前: " + current_interval;
    if (MessageBoxA(hwnd, "是否修改轮询间隔？", "设置", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        // 这里可以实现更复杂的输入对话框，简化处理
        // 实际使用中可以读取配置文件重新加载
    }
}

void HideConsole() {
    if (g_console_visible) {
        ShowWindow(GetConsoleWindow(), SW_HIDE);
        g_console_visible = false;
        
        // 更新托盘图标提示
        if (g_nid.hWnd) {
            strncpy_s(g_nid.szTip, "lg-reminder - 后台运行中", sizeof(g_nid.szTip) - 1);
            Shell_NotifyIconA(NIM_MODIFY, &g_nid);
        }
    }
}

void ShowConsole() {
    if (!g_console_visible) {
        ShowWindow(GetConsoleWindow(), SW_SHOW);
        g_console_visible = true;
        
        // 更新托盘图标提示
        if (g_nid.hWnd) {
            strncpy_s(g_nid.szTip, "lg-reminder - 洛谷私信提醒", sizeof(g_nid.szTip) - 1);
            Shell_NotifyIconA(NIM_MODIFY, &g_nid);
        }
        
        // 输出当前状态
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(h, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        cout << "\n==================================================\n";
        cout << "控制台已显示，程序继续运行中...\n";
        cout << "轮询间隔: " << g_check_interval << " 秒\n";
        cout << "历史消息: " << g_history_ids.size() << " 条\n";
        cout << "==================================================\n" << endl;
        SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
}

int main() {
    SetConsoleUTF8();
    SetConsoleTitleA("lg-reminder");
    
    // 获取控制台句柄
    g_console_handle = GetConsoleWindow();
    
    // 读取配置
    if (!cfg(g_cookie, g_username, g_check_interval)) {
        cout << "错误：配置加载失败\n\n";
        cout << "请编辑 config.txt 文件，填入您的 cookie\n\n";
        cout << "如何获取 cookie：\n";
        cout << "1. 在浏览器中登录洛谷并进入私信页面\n";
        cout << "2. 按 F12 打开开发者工具\n";
        cout << "3. 切换到\"网络\"标签，刷新页面\n";
        cout << "4. 点进名称是\"chat\"的请求，往下翻，在 Request Headers 中找到\"Cookie\"\n";
        cout << "5. 复制完整 cookie 内容到 config.txt\n";
        cout << "\n按任意键退出..." << endl;
        system("pause > nul");
        return 1;
    }
    
    // 加载历史记录
    g_history_ids = LoadHistory();
    
    // 显示启动信息
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(h, &ci);
    ci.bVisible = 0;
    SetConsoleCursorInfo(h, &ci);
    
    SetConsoleTextAttribute(h, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    cout << "lg-reminder (" << lg_reminder_version << " by " << lg_reminder_author << ")\n";
    cout << "==================================================\n\n";
    SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    cout << "启动信息:\n  轮询间隔: " << g_check_interval << " 秒\n";
    cout << "  历史消息: " << g_history_ids.size() << " 条\n\n";
    cout << "程序已启动，可在系统托盘找到图标\n";
    cout << "右键托盘图标可显示/隐藏控制台或退出程序\n";
    cout << "==================================================\n\n";
    
    // 创建隐藏窗口用于接收托盘消息
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "LGReminderClass";
    RegisterClassA(&wc);
    
    g_hwnd = CreateWindowA("LGReminderClass", "lg-reminder", WS_OVERLAPPEDWINDOW,
                           0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);
    
    if (!g_hwnd) {
        cout << "错误：无法创建窗口" << endl;
        return 1;
    }
    
    // 启动消息检查线程
    thread check_thread(CheckMessagesLoop);
    check_thread.detach();
    
    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) && g_running) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // 清理
    RemoveTrayIcon();
    cout << "程序已退出" << endl;
    
    return 0;
}
