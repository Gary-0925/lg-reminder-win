/*
lg-reminder
在 Windows 通知弹窗提醒洛谷私信
==================================================
@version v.1.3
@author Gary0
@license MIT
Copyright 2026 (c) Gary0
本脚本由洛谷 @Gary0 开发
感谢洛谷 @PenaltyKing 提供的思路及建议
==================================================
正式版发布，完全后台运行
本脚本不会盗取您的 cookie
使用了 AI 辅助开发，计划增加犇犇提醒和通知提醒等功能
==================================================
*/

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
#include <map>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

#define lg_reminder_version "v.1.3"
#define lg_reminder_author "Gary0"
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_ABOUT 1003
#define ID_TRAY_SETTINGS 1004
#define ID_TRAY_LOG 1005
#define ID_TRAY_GITHUB 1006
#define ID_TRAY_README 1007
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (WM_USER + 1)
#endif
#define MUTEX_NAME "Global\\lg-reminder-mutex"

typedef long long LL;
using namespace std;

struct Msg
{
	int id, uid;
	string name, time, con;
	bool is_new;
};

struct NOTIFY_DATA
{
	NOTIFYICONDATAA nid;
	HICON app_icon;
};

int n, m;
int g_check_interval = 30, g_uid;
HWND g_hwnd = NULL;
NOTIFY_DATA g_ntf = {};
atomic<bool> g_running(true);
atomic<bool> g_checking(false);
string g_cookie;
vector<int> g_history_ids;
mutex g_history_mutex;

string now();
string utf8_to_system(const string &utf8_str);
string dec(string s);
string ext(string h);
string url(string e);
vector<Msg> parse(string j);
void noti(vector<Msg> v);
bool http(string ck, string &r);
bool cfg(string &c, string &u, int &t);
void SaveHistory(const vector<int>& v);
vector<int> LoadHistory();
vector<Msg> findnew(vector<Msg> cur, vector<int> lst);
void CheckMessages();
void CheckMessagesLoop();
HICON GetAppIcon();
void WriteLog(const string& msg);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateTrayIcon(HWND hwnd);
void RemoveTrayIcon();
void ShowContextMenu(HWND hwnd);
void ShowAboutDialog(HWND hwnd);
void ShowCfgDialog(HWND hwnd);
void ShowLogDialog(HWND hwnd);

string now()
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	char buf[64];
	sprintf_s(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	return buf;
}

void WriteLog(const string& msg)
{
	ofstream log("lg-reminder.log", ios::app);
	if (log.is_open())
	{
		log << "[" << now() << "] " << msg << endl;
		log.close();
	}
}

HICON GetAppIcon()
{
	HICON hIcon = NULL;
	char exePath[MAX_PATH];
	
	hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 16, 16, 0);
	if (hIcon) return hIcon;
	
	GetModuleFileNameA(NULL, exePath, MAX_PATH);
	hIcon = (HICON)LoadImageA(NULL, exePath, IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
	if (hIcon) return hIcon;
	
	return LoadIcon(NULL, IDI_INFORMATION);
}

string utf8_to_system(const string &utf8_str)
{
	if (utf8_str.empty()) return "";
	
	int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, NULL, 0);
	if (!wlen) return utf8_str;
	
	wchar_t *wstr = new wchar_t[wlen];
	MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, wstr, wlen);
	
	int glen = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
	if (!glen)
	{
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

string dec(string s)
{
	string r;
	for (size_t i = 0; i < s.length(); i++)
	{
		if (s[i] == '\\' && i + 5 < s.length() && s[i + 1] == 'u')
		{
			char h[5] = {s[i + 2], s[i + 3], s[i + 4], s[i + 5], 0};
			int c = stoi(h, nullptr, 16);
			if (c < 0x80) r += (char)c;
			else if (c < 0x800)
			{
				r += (char)(0xC0 | (c >> 6));
				r += (char)(0x80 | (c & 0x3F));
			}
			else
			{
				r += (char)(0xE0 | (c >> 12));
				r += (char)(0x80 | ((c >> 6) & 0x3F));
				r += (char)(0x80 | (c & 0x3F));
			}
			i += 5;
		}
		else r += s[i];
	}
	return r;
}

string ext(string h)
{
	string p = "window._feInjection = JSON.parse(decodeURIComponent(\"";
	size_t l = h.find(p);
	if (l == string::npos) return "";
	l += p.length();
	size_t r = h.find("\"));", l);
	return (r == string::npos) ? "" : h.substr(l, r - l);
}

string url(string e)
{
	string d;
	for (size_t i = 0; i < e.length(); i++)
	{
		if (e[i] == '%' && i + 2 < e.length())
		{
			int v;
			sscanf_s(e.substr(i + 1, 2).c_str(), "%x", &v);
			d += (char)v, i += 2;
		}
		else d += e[i];
	}
	return d;
}

vector<Msg> parse(string j)
{
	vector<Msg> v;
	string s1 = "\"latestMessages\":{\"result\":[", s2 = "\"result\":[";
	size_t st = j.find(s1);
	
	if (st == string::npos)
	{
		st = j.find(s2);
		if (st == string::npos) return v;
		st += s2.length();
	}
	else st += s1.length();
	
	size_t p = st;
	while (1)
	{
		p = j.find("{", p);
		if (p == string::npos || p > j.find("]", st)) break;
		
		Msg m = {0, 0, "", "", "", false};
		int bc = 1;
		size_t ed = p + 1;
		while (ed < j.length() && bc > 0)
		{
			if (j[ed] == '{') bc++;
			else if (j[ed] == '}') bc--;
			ed++;
		}
		string o = j.substr(p, ed - p);
		
		size_t fid = o.find("\"id\":");
		if (fid != string::npos)
		{
			size_t a = o.find_first_of("0123456789", fid + 5);
			size_t b = o.find_first_not_of("0123456789", a);
			if (a != string::npos && b != string::npos)
				m.id = atoi(o.substr(a, b - a).c_str());
		}
		
		size_t ftime = o.find("\"time\":");
		if (ftime != string::npos)
		{
			size_t a = o.find_first_of("0123456789", ftime + 6);
			size_t b = o.find_first_not_of("0123456789", a);
			if (a != string::npos && b != string::npos)
			{
				time_t t = atol(o.substr(a, b - a).c_str());
				struct tm *ti = localtime(&t);
				char buf[16];
				strftime(buf, 16, "%m-%d %H:%M", ti);
				m.time = buf;
			}
		}
		
		size_t fsender = o.find("\"sender\":");
		if (fsender != string::npos)
		{
			size_t np = o.find("\"name\":\"", fsender);
			if (np != string::npos)
			{
				np += 8;
				size_t ne = o.find("\"", np);
				if (ne != string::npos)
					m.name = dec(o.substr(np, ne - np));
			}
			size_t up = o.find("\"uid\":", fsender);
			if (up != string::npos)
			{
				size_t a = o.find_first_of("0123456789", up + 5);
				size_t b = o.find_first_not_of("0123456789", a);
				if (a != string::npos && b != string::npos)
					m.uid = atoi(o.substr(a, b - a).c_str());
			}
		}
		
		size_t fcon = o.find("\"content\":");
		if (fcon != string::npos)
		{
			size_t cs = o.find("\"", fcon + 9);
			if (cs != string::npos)
			{
				cs++;
				size_t ce = cs;
				bool esc = 0;
				while (ce < o.length())
				{
					if (o[ce] == '\\' && !esc) esc = 1;
					else if (o[ce] == '"' && !esc) break;
					else esc = 0;
					ce++;
				}
				if (ce < o.length())
				{
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

map<int, int> idtoname;

void noti(vector<Msg> v)
{
	if (v.empty()) return;
	
	for (auto &m : v)
	{
		string name_sys = utf8_to_system(m.name);
		string t = utf8_to_system("洛谷新私信 - 来自 ") + name_sys;
		string c = m.con.empty() ? utf8_to_system("您有一条新消息") : utf8_to_system(m.con);

		idtoname[m.id] = m.uid;

		NOTIFYICONDATAA n = {};
		n.cbSize = sizeof(NOTIFYICONDATAA);
		n.hWnd = g_hwnd;
		n.uID = m.id;
		n.uFlags = NIF_INFO | NIF_ICON | NIF_TIP | NIF_MESSAGE;
		n.dwInfoFlags = NIIF_INFO | NIIF_LARGE_ICON;
		n.uTimeout = 5000;
		n.uCallbackMessage = WM_TRAYICON;
		strncpy_s(n.szInfoTitle, t.c_str(), sizeof(n.szInfoTitle) - 1);
		strncpy_s(n.szInfo, c.c_str(), sizeof(n.szInfo) - 1);

		n.hIcon = g_ntf.app_icon;
		if (n.hIcon) n.uFlags |= NIF_ICON;
		
		Shell_NotifyIconA(NIM_ADD, &n);
		Shell_NotifyIconA(NIM_SETVERSION, &n);
		Sleep(3000);
		Shell_NotifyIconA(NIM_DELETE, &n);
		Sleep(500);
	}
}

bool http(string ck, string &r)
{
	HINTERNET s = NULL, c = NULL, q = NULL;
	
	s = WinHttpOpen(L"lg-reminder/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
					WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (s) c = WinHttpConnect(s, L"www.luogu.com.cn", INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (c) q = WinHttpOpenRequest(c, L"GET", L"/chat", NULL, WINHTTP_NO_REFERER,
								  WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (q)
	{
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
		
		DWORD sz = 0, d = 0;
		char *b;
		do
		{
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

bool cfg(string &c, int &u, int &t)
{
	ifstream f("config.txt");
	if (!f.is_open())
	{
		ofstream o("config.txt");
		if (o.is_open())
		{
			string config =
				"# lg-reminder 配置\n\n"
				"# 你的洛谷cookie\ncookie=你的完整cookie\n\n"
				"# 你的洛谷用户id\nuid=你的uid\n\n"
				"# 轮询间隔（秒）\ninterval=10";
			o << utf8_to_system(config);
			o.close();
		}
		return false;
	}
	
	string content;
	char buffer[4096];
	while (f.read(buffer, sizeof(buffer)))
		content.append(buffer, f.gcount());
	content.append(buffer, f.gcount());
	f.close();
	
	if (content.size() >= 3 &&
			(unsigned char)content[0] == 0xEF &&
			(unsigned char)content[1] == 0xBB &&
			(unsigned char)content[2] == 0xBF)
		content = content.substr(3);
	
	stringstream ss(content);
	string line;
	while (getline(ss, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty() || line[0] == '#')
			continue;
		
		size_t p = line.find('=');
		if (p == string::npos)
			continue;
		
		string k = line.substr(0, p);
		string v = line.substr(p + 1);
		
		if (k == "cookie")
			c = v;
		else if (k == "uid")
			u = stoi(v);
		else if (k == "interval")
			t = stoi(v);
	}
	
	return !c.empty() && c.find(utf8_to_system("你的")) == string::npos;
}

void SaveHistory(const vector<int>& v)
{
	ofstream f("data.txt", ios::binary);
	if (f.is_open())
	{
		for (int i : v)
			f << i << "\n";
		f.close();
	}
}

vector<int> LoadHistory()
{
	vector<int> v;
	ifstream f("data.txt", ios::binary);
	if (f.is_open())
	{
		string line;
		while (getline(f, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (!line.empty())
				v.push_back(stoi(line));
		}
		f.close();
	}
	return v;
}

vector<Msg> findnew(vector<Msg> cur, vector<int> lst)
{
	vector<Msg> n;
	for (auto &m : cur)
	{
		bool ok = (m.uid == g_uid);
		for (int i : lst)
		{
			if (m.id == i)
			{
				ok = true;
				break;
			}
		}
		if (!ok) n.push_back(m);
	}
	return n;
}

void CheckMessages()
{
	if (g_checking || !g_running) return;
	g_checking = true;
	WriteLog("监听消息...");
	
	string htm;
	if (http(g_cookie, htm))
	{
		string e = ext(htm);
		if (!e.empty())
		{
			string j = url(e);
			vector<Msg> v = parse(j);
			if (!v.empty())
			{
				vector<int> ids;
				for (auto &m : v) ids.push_back(m.id);
				
				lock_guard<mutex> lock(g_history_mutex);
				vector<Msg> nw = findnew(v, g_history_ids);
				if (!nw.empty())
				{
					string logmsg = "新消息: ";
					for (auto &m : nw)
						logmsg += m.name + " ";
					WriteLog(logmsg);
					
					if (!g_history_ids.empty()) noti(nw);
					g_history_ids = ids;
					SaveHistory(ids);
				}
			}
		}
		else
		{
			string error_msg = "错误：无消息数据，请检查配置或网络";
			string error_sys = utf8_to_system(error_msg);
			string title_sys = utf8_to_system("错误");
			MessageBoxA(NULL, error_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONERROR);
		}
	}
	g_checking = false;
}

void CheckMessagesLoop()
{
	WriteLog("启动");
	while (g_running)
	{
		CheckMessages();
		this_thread::sleep_for(chrono::seconds(g_check_interval));
	}
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		CreateTrayIcon(hwnd);
		break;
		
	case WM_DESTROY:
		RemoveTrayIcon();
		PostQuitMessage(0);
		break;
		
	case WM_TRAYICON:
		if (lParam == WM_LBUTTONUP || lParam == NIN_SELECT || lParam == 1029)
		{
			ShellExecuteA(NULL, "open", ("https://www.luogu.com.cn/chat?uid=" + to_string(idtoname[wParam])).c_str(), NULL, NULL, SW_SHOW);
		}
		else if (lParam == WM_RBUTTONUP)
		{
			ShowContextMenu(hwnd);
		}
		break;
		
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_TRAY_ABOUT:
			ShowAboutDialog(hwnd);
			break;
		case ID_TRAY_SETTINGS:
			ShowCfgDialog(hwnd);
			break;
		case ID_TRAY_LOG:
			ShowLogDialog(hwnd);
			break;
		case ID_TRAY_EXIT:
			g_running = false;
			DestroyWindow(hwnd);
			break;
		case ID_TRAY_README:
			ShellExecuteA(NULL, "open", "https://github.com/Gary-0925/lg-reminder/blob/main/README.md", NULL, NULL, SW_SHOW);
			break;
		case ID_TRAY_GITHUB:
			ShellExecuteA(NULL, "open", "https://github.com/Gary-0925/lg-reminder/tags", NULL, NULL, SW_SHOW);
			break;
		}
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateTrayIcon(HWND hwnd)
{
	g_ntf.nid.cbSize = sizeof(NOTIFYICONDATAA);
	g_ntf.nid.hWnd = hwnd;
	g_ntf.nid.uID = 1;
	g_ntf.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_ntf.nid.uCallbackMessage = WM_TRAYICON;
	
	g_ntf.app_icon = GetAppIcon();
	g_ntf.nid.hIcon = g_ntf.app_icon;
	
	strncpy_s(g_ntf.nid.szTip, utf8_to_system(string("lg-reminder - 洛谷私信提醒")).c_str(), sizeof(g_ntf.nid.szTip) - 1);
	Shell_NotifyIconA(NIM_ADD, &g_ntf.nid);
}

void RemoveTrayIcon()
{
	Shell_NotifyIconA(NIM_DELETE, &g_ntf.nid);
	if (g_ntf.app_icon)
		DestroyIcon(g_ntf.app_icon);
}

void ShowContextMenu(HWND hwnd)
{
	HMENU hMenu = CreatePopupMenu();
	
	string str_readme = utf8_to_system("使用说明");
	string str_github = utf8_to_system("手动更新");
	string str_show_log = utf8_to_system("打开日志");
	string str_settings = utf8_to_system("打开配置");
	string str_about = utf8_to_system("关于");
	string str_exit = utf8_to_system("退出");
	
	InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_README, str_readme.c_str());
	InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_GITHUB, str_github.c_str());
	InsertMenuA(hMenu, -1, MF_SEPARATOR, 0, NULL);
	InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_SETTINGS, str_settings.c_str());
	InsertMenuA(hMenu, -1, MF_BYPOSITION, ID_TRAY_LOG, str_show_log.c_str());
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

void ShowAboutDialog(HWND hwnd)
{
	string msg = "lg-reminder\n\n"
				 "@version " + string(lg_reminder_version) + "\n"
				 "@author " + string(lg_reminder_author) + "\n"
				 "@license MIT\n"
				 "Copyright 2026 (c) " + string(lg_reminder_author) + "\n"
				 "本脚本由洛谷 @" + string(lg_reminder_author) + " 开发\n"
				 "感谢洛谷 @PenaltyKing 提供的思路及建议\n\n"
				 "在 Windows 通知弹窗提醒洛谷私信\n感谢使用！";
	string title = "关于 lg-reminder";
	
	string msg_sys = utf8_to_system(msg);
	string title_sys = utf8_to_system(title);
	
	MessageBoxA(hwnd, msg_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONINFORMATION);
}

void ShowLogDialog(HWND hwnd)
{
	char logPath[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, logPath);
	strcat_s(logPath, "\\lg-reminder.log");
	
	if (GetFileAttributesA(logPath) == INVALID_FILE_ATTRIBUTES)
	{
		ofstream log(logPath);
		log.close();
	}
	
	ShellExecuteA(hwnd, "open", logPath, NULL, NULL, SW_SHOW);
}

void ShowCfgDialog(HWND hwnd)
{
	char cfgPath[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, cfgPath);
	strcat_s(cfgPath, "\\config.txt");
	
	if (GetFileAttributesA(cfgPath) == INVALID_FILE_ATTRIBUTES)
	{
		ofstream cfg(cfgPath);
		cfg.close();
	}
	
	ShellExecuteA(hwnd, "open", cfgPath, NULL, NULL, SW_SHOW);

	g_running = false;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	HANDLE hMutex = CreateMutexA(NULL, TRUE, MUTEX_NAME);
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(hMutex);
		return 0;
	}

	if (!cfg(g_cookie, g_uid, g_check_interval))
	{
		string error_msg = "错误：配置加载失败\n\n请编辑 config.txt 文件，填入您的 cookie\n\n"
						   "如何获取 cookie：\n"
						   "1. 在浏览器中登录洛谷并进入私信页面\n"
						   "2. 按 F12 打开开发者工具\n"
						   "3. 切换到\"网络\"标签，刷新页面\n"
						   "4. 点进名称是\"chat\"的请求，往下翻，在 Request Headers 中复制\"Cookie\"\n"
						   "5. 将 cookie（注意是完整 cookie，不是只包含 __client_id）填入 config.txt";
		
		string error_sys = utf8_to_system(error_msg);
		string title_sys = utf8_to_system("错误");
		
		MessageBoxA(NULL, error_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONERROR);
		CloseHandle(hMutex);
		return 1;
	}
	
	g_history_ids = LoadHistory();
	
	WNDCLASSA wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "LGReminderClass";
	RegisterClassA(&wc);
	
	g_hwnd = CreateWindowA("LGReminderClass", "lg-reminder", WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
	
	if (!g_hwnd)
	{
		MessageBoxA(NULL, "无法创建窗口", "错误", MB_OK);
		return 1;
	}
	
	thread check_thread(CheckMessagesLoop);
	check_thread.detach();
	
	string start_msg = "lg-reminder " + string(lg_reminder_version) + " 开始监听...\n\n"
					   "轮询间隔: " + to_string(g_check_interval) + " 秒\n"
					   "历史消息: " + to_string(g_history_ids.size()) + " 条\n\n"
					   "程序已在后台运行，可在系统托盘找到图标";
	
	string start_sys = utf8_to_system(start_msg);
	string title_sys = utf8_to_system("lg-reminder");
	
	MessageBoxA(NULL, start_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONINFORMATION);
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) && g_running)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	WriteLog("退出");
	RemoveTrayIcon();
	CloseHandle(hMutex);
	return 0;
}
