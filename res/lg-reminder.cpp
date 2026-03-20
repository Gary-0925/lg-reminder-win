/*
lg-reminder
在 Windows 通知弹窗提醒洛谷私信（美化版）
==================================================
@version v.1.1.0
@author Gary0
@license MIT
==================================================
*/

#define lg_reminder_version "v.1.1.0"
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
#include <sstream>
#include <shellapi.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace std;

// 自定义消息常量
#define WM_TRAYICON (WM_USER + 1)

// 全局变量
atomic<bool> g_running(true);
atomic<bool> g_checking(false);
int g_check_interval = 30;
HWND g_hwnd = NULL;
NOTIFYICONDATAA g_nid = {};
string g_cookie, g_username;
vector<int> g_history_ids;
mutex g_history_mutex;
HICON g_tray_icon_normal = NULL;
HICON g_tray_icon_newmsg = NULL;

// 菜单项ID
#define ID_TRAY_EXIT 1001
#define ID_TRAY_ABOUT 1003
#define ID_TRAY_SETTINGS 1004
#define ID_TRAY_SHOW_LOG 1005
#define ID_TRAY_CHECK 1006

struct Msg { 
    int id, uid; 
    string name, time, con; 
    bool is_new; 
};

string utf8_to_system(const string &utf8_str);
void WriteLog(const string& msg);
void ShowBeautifiedNotification(const string& title, const string& content, bool is_error = false);
void UpdateTrayIcon(bool has_new_msg = false);
HICON GetTrayIcon(bool has_new_msg = false);
void noti(vector<Msg> v, string us);

// 创建自定义图标（使用 GDI+ 或简单图形）
HICON CreateCustomIcon(bool has_new_msg = false) {
    // 创建 32x32 的位图
    int width = 32;
    int height = 32;
    HDC hdc = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);
    
    // 填充背景
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
    RECT rect = {0, 0, width, height};
    FillRect(hdcMem, &rect, hBrush);
    DeleteObject(hBrush);
    
    // 绘制图标内容
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 120, 215));
    HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPen);
    
    // 绘制信封形状
    if (has_new_msg) {
        // 新消息：红色信封
        SelectObject(hdcMem, CreateSolidBrush(RGB(255, 80, 80)));
    } else {
        // 普通：蓝色信封
        SelectObject(hdcMem, CreateSolidBrush(RGB(0, 120, 215)));
    }
    
    // 绘制信封主体
    Rectangle(hdcMem, 8, 12, 24, 28);
    
    // 绘制信封盖
    POINT points[] = {{8, 12}, {16, 18}, {24, 12}};
    Polygon(hdcMem, points, 3);
    
    // 绘制消息点（有新消息时显示红点）
    if (has_new_msg) {
        HBRUSH hRedBrush = CreateSolidBrush(RGB(255, 0, 0));
        SelectObject(hdcMem, hRedBrush);
        Ellipse(hdcMem, 24, 8, 30, 14);
        DeleteObject(hRedBrush);
    }
    
    // 清理 GDI 对象
    SelectObject(hdcMem, hOldPen);
    DeleteObject(hPen);
    
    // 创建图标
    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmColor = hbm;
    ii.hbmMask = hbm;
    
    HICON hIcon = CreateIconIndirect(&ii);
    
    // 清理
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdc);
    
    return hIcon;
}

// 创建简单的图标（使用系统图标）
HICON GetTrayIcon(bool has_new_msg = false) {
    if (has_new_msg) {
        // 尝试加载自定义图标或使用系统图标
        HICON hIcon = (HICON)LoadImage(NULL, IDI_WARNING, IMAGE_ICON, 16, 16, LR_SHARED);
        return hIcon ? hIcon : LoadIcon(NULL, IDI_WARNING);
    } else {
        return LoadIcon(NULL, IDI_INFORMATION);
    }
}

// 更新托盘图标
void UpdateTrayIcon(bool has_new_msg = false) {
    if (g_nid.hWnd) {
        g_nid.uFlags = NIF_ICON;
        g_nid.hIcon = GetTrayIcon(has_new_msg);
        Shell_NotifyIconA(NIM_MODIFY, &g_nid);
    }
}

// 日志输出（写入文件）
void WriteLog(const string& msg) {
    ofstream log("lg-reminder.log", ios::app);
    if (log.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timebuf[64];
        sprintf_s(timebuf, "[%04d-%02d-%02d %02d:%02d:%02d] ",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        log << timebuf << msg << endl;
        log.close();
    }
}

// 美化版通知
void ShowBeautifiedNotification(const string& title, const string& content, bool is_error = false) {
    NOTIFYICONDATAA n = {};
    n.cbSize = sizeof(NOTIFYICONDATAA);
    n.hWnd = g_hwnd;
    n.uID = 1;
    n.uFlags = NIF_INFO | NIF_ICON | NIF_TIP;
    
    // 设置通知样式
    if (is_error) {
        n.dwInfoFlags = NIIF_ERROR | NIIF_LARGE_ICON;
        n.hIcon = LoadIcon(NULL, IDI_ERROR);
    } else {
        n.dwInfoFlags = NIIF_INFO | NIIF_LARGE_ICON;
        n.hIcon = GetTrayIcon(true);
    }
    
    n.uTimeout = 8000;  // 8秒显示时间
    strncpy_s(n.szInfoTitle, title.c_str(), sizeof(n.szInfoTitle) - 1);
    strncpy_s(n.szInfo, content.c_str(), sizeof(n.szInfo) - 1);
    strncpy_s(n.szTip, "lg-reminder", sizeof(n.szTip) - 1);
    
    Shell_NotifyIconA(NIM_ADD, &n);
    Sleep(3000);
    Shell_NotifyIconA(NIM_DELETE, &n);
}

// 美化版消息通知
void noti(vector<Msg> v, string us) {
    if (v.empty()) return;
    
    // 更新托盘图标为新消息状态
    UpdateTrayIcon(true);
    
    for (auto &m : v) {
        string name_sys = utf8_to_system(m.name);
        string us_sys = utf8_to_system(us);
        
        if (name_sys != us_sys) {
            // 构建美观的通知内容
            string title = "📬 " + utf8_to_system("洛谷新私信");
            string content = "📝 " + utf8_to_system("来自: ") + name_sys + "\n";
            
            if (!m.con.empty()) {
                content += "💬 " + m.con + "\n";
            }
            content += "🕐 " + utf8_to_system("时间: ") + m.time;
            
            // 使用美化版通知
            ShowBeautifiedNotification(title, content, false);
            
            // 播放提示音（可选）
            Beep(800, 200);
            Sleep(100);
            Beep(1000, 200);
        }
    }
    
    // 5秒后恢复普通图标
    this_thread::sleep_for(chrono::seconds(5));
    UpdateTrayIcon(false);
}

// 前置声明
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateTrayIcon(HWND hwnd);
void RemoveTrayIcon();
void ShowContextMenu(HWND hwnd);
void ShowAboutDialog(HWND hwnd);
void ShowSettingsDialog(HWND hwnd);
void ShowLogDialog(HWND hwnd);
void CheckMessages();
void SaveHistory(const vector<int>& ids);
vector<int> LoadHistory();

string now() {
    SYSTEMTIME st; 
    GetLocalTime(&st);
    char b[64]; 
    sprintf_s(b, sizeof(b), "%04d-%02d-%02d %02d:%02d:%02d", 
              st.wYear, st.wMonth, st.wDay, 
              st.wHour, st.wMinute, st.wSecond);
    return b;
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
                    if (m.con.length() > 50) 
                        m.con = m.con.substr(0, 47) + "...";
                }
            }
        }
        
        if (m.id && !m.name.empty()) 
            v.push_back(m);
        p = ed;
    }
    return v;
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
        WinHttpAddRequestHeaders(q, h.c_str(), (DWORD)h.length(), WINHTTP_ADDREQ_FLAG_ADD);
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
                "# 轮询间隔（秒）\ninterval=10";
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
                    // 记录到日志
                    string logmsg = "发现新消息: ";
                    for (auto &m : nw) {
                        logmsg += m.name + " ";
                    }
                    WriteLog(logmsg);
                    
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
    WriteLog("程序启动，开始监听消息");
    while (g_running) {
        CheckMessages();
        this_thread::sleep_for(chrono::seconds(g_check_interval));
    }
    WriteLog("程序退出");
}

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
                case ID_TRAY_CHECK:
                    // 手动检查
                    thread([]() {
                        CheckMessages();
                    }).detach();
                    ShowBeautifiedNotification("手动检查", "正在检查新消息...", false);
                    break;
                case ID_TRAY_ABOUT:
                    ShowAboutDialog(hwnd);
                    break;
                case ID_TRAY_SETTINGS:
                    ShowSettingsDialog(hwnd);
                    break;
                case ID_TRAY_SHOW_LOG:
                    ShowLogDialog(hwnd);
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
    g_nid.hIcon = GetTrayIcon(false);
    strncpy_s(g_nid.szTip, "lg-reminder - 洛谷私信提醒\n右键查看菜单", sizeof(g_nid.szTip) - 1);
    Shell_NotifyIconA(NIM_ADD, &g_nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIconA(NIM_DELETE, &g_nid);
}

void ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    
    string str_check = utf8_to_system("🔍 手动检查");
    string str_show_log = utf8_to_system("📋 查看日志");
    string str_settings = utf8_to_system("⚙️ 设置");
    string str_about = utf8_to_system("ℹ️ 关于");
    string str_exit = utf8_to_system("❌ 退出");
    
    InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_CHECK, str_check.c_str());
    InsertMenuA(hMenu, -1, MF_SEPARATOR, 0, NULL);
    InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_SHOW_LOG, str_show_log.c_str());
    InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_SETTINGS, str_settings.c_str());
    InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_ABOUT, str_about.c_str());
    InsertMenuA(hMenu, -1, MF_SEPARATOR, 0, NULL);
    InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_EXIT, str_exit.c_str());
    
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

void ShowAboutDialog(HWND hwnd) {
    string msg = 
        "╔══════════════════════════════════╗\n"
        "║     lg-reminder " + string(lg_reminder_version) + "      ║\n"
        "║     By " + string(lg_reminder_author) + "              ║\n"
        "║     License: MIT                ║\n"
        "╚══════════════════════════════════╝\n\n"
        "📬 洛谷私信提醒工具\n"
        "🎯 功能：\n"
        "   • 实时监控洛谷私信\n"
        "   • 系统托盘运行\n"
        "   • 美观通知提醒\n"
        "   • 日志记录\n\n"
        "💡 提示：程序完全后台运行，无窗口\n"
        "🖱️ 右键托盘图标可进行设置\n\n"
        "感谢使用！❤️";
    
    string title = "关于 lg-reminder";
    
    string msg_sys = utf8_to_system(msg);
    string title_sys = utf8_to_system(title);
    
    MessageBoxA(hwnd, msg_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONINFORMATION);
}

void ShowSettingsDialog(HWND hwnd) {
    string info = "⚙️ 设置说明\n\n"
                  "当前轮询间隔: " + to_string(g_check_interval) + " 秒\n\n"
                  "📝 修改方法：\n"
                  "1. 关闭程序\n"
                  "2. 编辑 config.txt 文件\n"
                  "3. 修改 interval= 的值\n"
                  "4. 重新启动程序\n\n"
                  "💡 建议间隔：10-60 秒";
    
    string title = "设置";
    
    string info_sys = utf8_to_system(info);
    string title_sys = utf8_to_system(title);
    
    MessageBoxA(hwnd, info_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONINFORMATION);
}

void ShowLogDialog(HWND hwnd) {
    ifstream log("lg-reminder.log");
    string content;
    if (log.is_open()) {
        string line;
        int line_count = 0;
        while (getline(log, line) && line_count < 50) {  // 只显示最近50条
            content += line + "\n";
            line_count++;
        }
        log.close();
        
        if (line_count >= 50) {
            content += "\n... (仅显示最近50条日志)";
        }
    }
    
    if (content.empty()) {
        content = "暂无日志";
    }
    
    string title = "📋 运行日志 (最近50条)";
    string content_sys = utf8_to_system(content);
    string title_sys = utf8_to_system(title);
    
    MessageBoxA(hwnd, content_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONINFORMATION);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 读取配置
    if (!cfg(g_cookie, g_username, g_check_interval)) {
        // 配置错误时显示消息框
        string error_msg = 
            "❌ 错误：配置加载失败\n\n"
            "请编辑 config.txt 文件，填入您的 cookie\n\n"
            "📖 如何获取 cookie：\n"
            "1. 在浏览器中登录洛谷并进入私信页面\n"
            "2. 按 F12 打开开发者工具\n"
            "3. 切换到\"网络\"标签，刷新页面\n"
            "4. 点进名称是\"chat\"的请求\n"
            "5. 在 Request Headers 中找到\"Cookie\"\n"
            "6. 复制完整 cookie 内容到 config.txt\n\n"
            "💡 注意：cookie 通常很长，请完整复制";
        
        string error_sys = utf8_to_system(error_msg);
        string title_sys = utf8_to_system("配置错误");
        
        MessageBoxA(NULL, error_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // 加载历史记录
    g_history_ids = LoadHistory();
    
    // 创建隐藏窗口
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "LGReminderClass";
    RegisterClassA(&wc);
    
    g_hwnd = CreateWindowA("LGReminderClass", "lg-reminder", WS_OVERLAPPEDWINDOW,
                           0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    
    if (!g_hwnd) {
        MessageBoxA(NULL, "无法创建窗口", "错误", MB_OK);
        return 1;
    }
    
    // 启动消息检查线程
    thread check_thread(CheckMessagesLoop);
    check_thread.detach();
    
    // 显示启动提示
    string start_msg = 
        "✅ lg-reminder " + string(lg_reminder_version) + " 已启动\n\n"
        "📊 运行状态：\n"
        "   • 轮询间隔: " + to_string(g_check_interval) + " 秒\n"
        "   • 历史消息: " + to_string(g_history_ids.size()) + " 条\n\n"
        "🖥️ 程序已在后台运行\n"
        "🖱️ 可在系统托盘找到图标\n\n"
        "💡 提示：右键托盘图标可进行设置";
    
    string start_sys = utf8_to_system(start_msg);
    string title_sys = utf8_to_system("启动成功");
    
    MessageBoxA(NULL, start_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONINFORMATION);
    
    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) && g_running) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // 清理
    RemoveTrayIcon();
    return 0;
}
