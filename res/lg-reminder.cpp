/*
lg-reminder
在 Windows 通知弹窗提醒洛谷私信
https://github.com/Gary-0925/lg-reminder/
==================================================
@version v.1.4
@author Gary0
@license MIT
Copyright 2026 (c) Gary0
本脚本由洛谷 @Gary0 开发
感谢洛谷 @PenaltyKing 提供的思路及建议
==================================================
正式版发布，完全后台运行
本程序不会盗取您的 cookie
使用了 AI 辅助开发，计划增加犇犇提醒和通知提醒等功能
==================================================
*/

#include <iostream>
#include <string>
#include <map>
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

#define LG_REMINDER_VERSION "v.1.4"
#define LG_REMINDER_AUTHOR "Gary0"
#define WM_TRAY_ICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_ABOUT 1003
#define ID_TRAY_SETTINGS 1004
#define ID_TRAY_LOG 1005
#define ID_TRAY_GITHUB 1006
#define ID_TRAY_README 1007
#define MUTEX_NAME "Global\\lg-reminder-mutex"
#define MAX_PATH_LEN 4096
#define NOTIFY_TIMEOUT 3000

typedef long long ll;
using namespace std;

// 消息结构体
struct msg_t
{
	int id, uid;
	string name, time, content;
	bool is_new;
};

// 托盘通知数据结构
struct notify_data_t
{
	NOTIFYICONDATAA nid;
	HICON app_icon;
};

// 全局变量
int g_check_interval = 30;
int g_uid = 0;
HWND g_hwnd = NULL;
notify_data_t g_notify = {};
atomic<bool> g_running(true);
atomic<bool> g_checking(false);
string g_cookie;
vector<int> g_history_ids;
mutex g_history_mutex;
map<int, int> g_id_to_uid;

// 工具函数
string get_current_time();
string utf8_to_system(const string &s);
string decode_unicode(string s);
string extract_json(string html);
string url_decode(string e);
string read_file(const string &filename);
void write_file(const string &filename, const string &content);
bool file_exists(const string &filename);
void show_error_message(const string &msg);
void show_info_message(const string &msg);
void open_url(const string &url);
void open_file(const string &filename);

// 业务函数
vector<msg_t> parse_messages(string json);
void send_notification(vector<msg_t> msgs);
bool http_request(string cookie, string &response);
bool load_config(string &cookie, int &uid, int &interval);
void save_history(const vector<int>& ids);
vector<int> load_history();
vector<msg_t> find_new_messages(vector<msg_t> current, vector<int> history);
void check_messages();
void check_messages_loop();
HICON get_app_icon();
void write_log(const string& msg);
void init_window_class(HINSTANCE hInstance);
void create_tray_icon(HWND hwnd);
void remove_tray_icon();
void show_context_menu(HWND hwnd);
void show_about_dialog(HWND hwnd);
void show_config_dialog(HWND hwnd);
void show_log_dialog(HWND hwnd);
void cleanup_and_exit(HANDLE mutex, int exit_code);
LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// 获取当前时间字符串
string get_current_time()
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	char buf[64];
	sprintf_s(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
			  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	return buf;
}

// 写入日志
void write_log(const string& msg)
{
	ofstream log("lg-reminder.log", ios::app);
	if (log.is_open())
	{
		log << "[" << get_current_time() << "] " << msg << endl;
		log.close();
	}
}

// 读取文件内容
string read_file(const string &filename)
{
	ifstream f(filename, ios::binary);
	if (!f.is_open()) return "";
	
	string content;
	char buffer[MAX_PATH_LEN];
	while (f.read(buffer, sizeof(buffer)))
		content.append(buffer, f.gcount());
	content.append(buffer, f.gcount());
	f.close();
	
	// 去除BOM
	if (content.size() >= 3 &&
		(unsigned char)content[0] == 0xEF &&
		(unsigned char)content[1] == 0xBB &&
		(unsigned char)content[2] == 0xBF)
		content = content.substr(3);
	
	return content;
}

// 写入文件
void write_file(const string &filename, const string &content)
{
	ofstream f(filename, ios::binary);
	if (f.is_open())
	{
		f << content;
		f.close();
	}
}

// 检查文件是否存在
bool file_exists(const string &filename)
{
	return GetFileAttributesA(filename.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// 显示错误消息
void show_error_message(const string &msg)
{
	string msg_sys = utf8_to_system(msg);
	string title_sys = utf8_to_system("错误");
	MessageBoxA(NULL, msg_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONERROR);
}

// 显示信息消息
void show_info_message(const string &msg)
{
	string msg_sys = utf8_to_system(msg);
	string title_sys = utf8_to_system("lg-reminder");
	MessageBoxA(NULL, msg_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONINFORMATION);
}

// 打开URL
void open_url(const string &url)
{
	ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOW);
}

// 打开文件
void open_file(const string &filename)
{
	ShellExecuteA(NULL, "open", filename.c_str(), NULL, NULL, SW_SHOW);
}

// 获取应用图标
HICON get_app_icon()
{
	HICON icon = NULL;
	char exe_path[MAX_PATH];
	
	icon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1),
							IMAGE_ICON, 16, 16, 0);
	if (icon) return icon;
	
	GetModuleFileNameA(NULL, exe_path, MAX_PATH);
	icon = (HICON)LoadImageA(NULL, exe_path, IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
	if (icon) return icon;
	
	return LoadIcon(NULL, IDI_INFORMATION);
}

// UTF8转系统编码
string utf8_to_system(const string &s)
{
	if (s.empty()) return "";
	
	int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
	if (!wlen) return s;
	
	wchar_t *wstr = new wchar_t[wlen];
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wstr, wlen);
	
	int glen = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
	if (!glen)
	{
		delete[] wstr;
		return s;
	}
	
	char *gstr = new char[glen];
	WideCharToMultiByte(CP_ACP, 0, wstr, -1, gstr, glen, NULL, NULL);
	string result(gstr);
	
	delete[] wstr;
	delete[] gstr;
	return result;
}

// 解码Unicode字符串
string decode_unicode(string s)
{
	string result;
	for (size_t i = 0; i < s.length(); i++)
	{
		if (s[i] == '\\' && i + 5 < s.length() && s[i + 1] == 'u')
		{
			char hex[5] = {s[i + 2], s[i + 3], s[i + 4], s[i + 5], 0};
			int code = stoi(hex, nullptr, 16);
			if (code < 0x80) result += (char)code;
			else if (code < 0x800)
			{
				result += (char)(0xC0 | (code >> 6));
				result += (char)(0x80 | (code & 0x3F));
			}
			else
			{
				result += (char)(0xE0 | (code >> 12));
				result += (char)(0x80 | ((code >> 6) & 0x3F));
				result += (char)(0x80 | (code & 0x3F));
			}
			i += 5;
		}
		else result += s[i];
	}
	return result;
}

// 提取JSON数据
string extract_json(string html)
{
	string pattern = "window._feInjection = JSON.parse(decodeURIComponent(\"";
	size_t start = html.find(pattern);
	if (start == string::npos) return "";
	start += pattern.length();
	size_t end = html.find("\"));", start);
	return (end == string::npos) ? "" : html.substr(start, end - start);
}

// URL解码
string url_decode(string e)
{
	string result;
	for (size_t i = 0; i < e.length(); i++)
	{
		if (e[i] == '%' && i + 2 < e.length())
		{
			int val;
			sscanf_s(e.substr(i + 1, 2).c_str(), "%x", &val);
			result += (char)val;
			i += 2;
		}
		else result += e[i];
	}
	return result;
}

// 解析消息JSON
vector<msg_t> parse_messages(string json)
{
	vector<msg_t> msgs;
	string s1 = "\"latestMessages\":{\"result\":[";
	string s2 = "\"result\":[";
	size_t start = json.find(s1);
	
	if (start == string::npos)
	{
		start = json.find(s2);
		if (start == string::npos) return msgs;
		start += s2.length();
	}
	else start += s1.length();
	
	size_t pos = start;
	while (1)
	{
		pos = json.find("{", pos);
		if (pos == string::npos || pos > json.find("]", start)) break;
		
		msg_t m = {0, 0, "", "", "", false};
		int brace_count = 1;
		size_t end = pos + 1;
		while (end < json.length() && brace_count > 0)
		{
			if (json[end] == '{') brace_count++;
			else if (json[end] == '}') brace_count--;
			end++;
		}
		string obj = json.substr(pos, end - pos);
		
		// 解析id
		size_t id_pos = obj.find("\"id\":");
		if (id_pos != string::npos)
		{
			size_t a = obj.find_first_of("0123456789", id_pos + 5);
			size_t b = obj.find_first_not_of("0123456789", a);
			if (a != string::npos && b != string::npos)
				m.id = atoi(obj.substr(a, b - a).c_str());
		}
		
		// 解析时间
		size_t time_pos = obj.find("\"time\":");
		if (time_pos != string::npos)
		{
			size_t a = obj.find_first_of("0123456789", time_pos + 6);
			size_t b = obj.find_first_not_of("0123456789", a);
			if (a != string::npos && b != string::npos)
			{
				time_t t = atol(obj.substr(a, b - a).c_str());
				struct tm *tm_info = localtime(&t);
				char buf[16];
				strftime(buf, 16, "%m-%d %H:%M", tm_info);
				m.time = buf;
			}
		}
		
		// 解析发送者
		size_t sender_pos = obj.find("\"sender\":");
		if (sender_pos != string::npos)
		{
			size_t name_pos = obj.find("\"name\":\"", sender_pos);
			if (name_pos != string::npos)
			{
				name_pos += 8;
				size_t name_end = obj.find("\"", name_pos);
				if (name_end != string::npos)
					m.name = decode_unicode(obj.substr(name_pos, name_end - name_pos));
			}
			size_t uid_pos = obj.find("\"uid\":", sender_pos);
			if (uid_pos != string::npos)
			{
				size_t a = obj.find_first_of("0123456789", uid_pos + 5);
				size_t b = obj.find_first_not_of("0123456789", a);
				if (a != string::npos && b != string::npos)
					m.uid = atoi(obj.substr(a, b - a).c_str());
			}
		}
		
		// 解析内容
		size_t content_pos = obj.find("\"content\":");
		if (content_pos != string::npos)
		{
			size_t cs = obj.find("\"", content_pos + 9);
			if (cs != string::npos)
			{
				cs++;
				size_t ce = cs;
				bool escaped = false;
				while (ce < obj.length())
				{
					if (obj[ce] == '\\' && !escaped) escaped = true;
					else if (obj[ce] == '"' && !escaped) break;
					else escaped = false;
					ce++;
				}
				if (ce < obj.length())
				{
					string raw = obj.substr(cs, ce - cs);
					m.content = decode_unicode(raw);
					if (m.content.length() > 30)
						m.content = m.content.substr(0, 27) + "...";
				}
			}
		}
		
		if (m.id && !m.name.empty())
			msgs.push_back(m);
		pos = end;
	}
	return msgs;
}

// 发送通知
void send_notification(vector<msg_t> msgs)
{
	if (msgs.empty()) return;
	
	for (auto &m : msgs)
	{
		string name_sys = utf8_to_system(m.name);
		string title = utf8_to_system("洛谷新私信 - 来自 ") + name_sys;
		string content = m.content.empty() ? utf8_to_system("您有一条新消息") : utf8_to_system(m.content);
		
		g_id_to_uid[m.id] = m.uid;
		
		NOTIFYICONDATAA n = {};
		n.cbSize = sizeof(NOTIFYICONDATAA);
		n.hWnd = g_hwnd;
		n.uID = m.id;
		n.uFlags = NIF_INFO | NIF_ICON | NIF_TIP | NIF_MESSAGE;
		n.dwInfoFlags = NIIF_INFO | NIIF_LARGE_ICON;
		n.uTimeout = 5000;
		n.uCallbackMessage = WM_TRAY_ICON;
		strncpy_s(n.szInfoTitle, title.c_str(), sizeof(n.szInfoTitle) - 1);
		strncpy_s(n.szInfo, content.c_str(), sizeof(n.szInfo) - 1);
		strncpy_s(n.szTip, "lg-reminder", sizeof(n.szTip) - 1);
		
		n.hIcon = g_notify.app_icon;
		if (n.hIcon) n.uFlags |= NIF_ICON;
		
		Shell_NotifyIconA(NIM_ADD, &n);
		Shell_NotifyIconA(NIM_SETVERSION, &n);
		Sleep(NOTIFY_TIMEOUT);
		Shell_NotifyIconA(NIM_DELETE, &n);
		Sleep(500);
	}
}

// HTTP请求
bool http_request(string cookie, string &response)
{
	HINTERNET session = NULL, connect = NULL, request = NULL;
	
	session = WinHttpOpen(L"lg-reminder/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (session) 
		connect = WinHttpConnect(session, L"www.luogu.com.cn", INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (connect) 
		request = WinHttpOpenRequest(connect, L"GET", L"/chat", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (request)
	{
		int len = MultiByteToWideChar(CP_UTF8, 0, cookie.c_str(), -1, NULL, 0);
		wchar_t *w_cookie = new wchar_t[len];
		MultiByteToWideChar(CP_UTF8, 0, cookie.c_str(), -1, w_cookie, len);
		wstring headers = L"Cookie: ";
		headers += w_cookie;
		headers += L"\r\nUser-Agent: Mozilla/5.0\r\nAccept: text/html\r\nAccept-Language: zh-CN\r\n";
		delete[] w_cookie;
		
		WinHttpAddRequestHeaders(request, headers.c_str(), (DWORD)headers.length(), WINHTTP_ADDREQ_FLAG_ADD);
		
		if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) goto cleanup;
		if (!WinHttpReceiveResponse(request, NULL)) goto cleanup;
		
		DWORD size = 0, read = 0;
		char *buffer;
		do
		{
			if (!WinHttpQueryDataAvailable(request, &size) || !size) break;
			buffer = new char[size + 1];
			ZeroMemory(buffer, size + 1);
			if (WinHttpReadData(request, buffer, size, &read))
				response.append(buffer, read);
			delete[] buffer;
		} while (size > 0);
	}
	
	cleanup: 
	if (request) WinHttpCloseHandle(request);
	if (connect) WinHttpCloseHandle(connect);
	if (session) WinHttpCloseHandle(session);
	return true;
}

// 加载配置
bool load_config(string &cookie, int &uid, int &interval)
{
	string content = read_file("config.txt");
	if (content.empty())
	{
		string default_config =
			"# lg-reminder 配置\n\n"
			"# 你的洛谷cookie\ncookie=你的完整cookie\n\n"
			"# 你的洛谷用户id\nuid=你的uid\n\n"
			"# 轮询间隔（秒）\ninterval=10";
		write_file("config.txt", utf8_to_system(default_config));
		return false;
	}
	
	stringstream ss(content);
	string line;
	while (getline(ss, line))
	{
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty() || line[0] == '#') continue;
		
		size_t pos = line.find('=');
		if (pos == string::npos) continue;
		
		string key = line.substr(0, pos);
		string value = line.substr(pos + 1);
		
		if (key == "cookie") cookie = value;
		else if (key == "uid") uid = stoi(value);
		else if (key == "interval") interval = stoi(value);
	}
	
	return !cookie.empty() && cookie.find(utf8_to_system("你的")) == string::npos;
}

// 保存历史记录
void save_history(const vector<int>& ids)
{
	stringstream ss;
	for (int id : ids)
		ss << id << "\n";
	write_file("lg-reminder.dat", ss.str());
}

// 加载历史记录
vector<int> load_history()
{
	vector<int> ids;
	string content = read_file("lg-reminder.dat");
	if (content.empty()) return ids;
	
	stringstream ss(content);
	string line;
	while (getline(ss, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (!line.empty())
			ids.push_back(stoi(line));
	}
	return ids;
}

// 查找新消息
vector<msg_t> find_new_messages(vector<msg_t> current, vector<int> history)
{
	vector<msg_t> new_msgs;
	for (auto &m : current)
	{
		bool is_old = (m.uid == g_uid);
		for (int id : history)
		{
			if (m.id == id)
			{
				is_old = true;
				break;
			}
		}
		if (!is_old) new_msgs.push_back(m);
	}
	return new_msgs;
}

// 检查消息
void check_messages()
{
	if (g_checking || !g_running) return;
	g_checking = true;
	write_log("监听消息...");
	
	string html;
	if (http_request(g_cookie, html))
	{
		string encoded = extract_json(html);
		if (!encoded.empty())
		{
			string json = url_decode(encoded);
			vector<msg_t> msgs = parse_messages(json);
			if (!msgs.empty())
			{
				vector<int> ids;
				for (auto &m : msgs) ids.push_back(m.id);
				
				lock_guard<mutex> lock(g_history_mutex);
				vector<msg_t> new_msgs = find_new_messages(msgs, g_history_ids);
				if (!new_msgs.empty())
				{
					string log_msg = "新消息: ";
					for (auto &m : new_msgs)
						log_msg += m.name + " ";
					write_log(log_msg);
					
					if (!g_history_ids.empty()) 
						send_notification(new_msgs);
					g_history_ids = ids;
					save_history(ids);
				}
			}
		}
		else
			show_error_message("错误：无消息数据，请检查配置或网络");
	}
	g_checking = false;
}

// 消息检查循环
void check_messages_loop()
{
	write_log("启动");
	while (g_running)
	{
		check_messages();
		this_thread::sleep_for(chrono::seconds(g_check_interval));
	}
}

// 初始化窗口类
void init_window_class(HINSTANCE hInstance)
{
	WNDCLASSA wc = {};
	wc.lpfnWndProc = window_proc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "LGReminderClass";
	RegisterClassA(&wc);
}

// 创建托盘图标
void create_tray_icon(HWND hwnd)
{
	g_notify.nid.cbSize = sizeof(NOTIFYICONDATAA);
	g_notify.nid.hWnd = hwnd;
	g_notify.nid.uID = 1;
	g_notify.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_notify.nid.uCallbackMessage = WM_TRAY_ICON;
	
	g_notify.app_icon = get_app_icon();
	g_notify.nid.hIcon = g_notify.app_icon;
	
	string tip = utf8_to_system("lg-reminder - 洛谷私信提醒");
	strncpy_s(g_notify.nid.szTip, tip.c_str(), sizeof(g_notify.nid.szTip) - 1);
	Shell_NotifyIconA(NIM_ADD, &g_notify.nid);
}

// 移除托盘图标
void remove_tray_icon()
{
	Shell_NotifyIconA(NIM_DELETE, &g_notify.nid);
	if (g_notify.app_icon)
		DestroyIcon(g_notify.app_icon);
}

// 显示右键菜单
void show_context_menu(HWND hwnd)
{
	HMENU menu = CreatePopupMenu();
	
	string str_readme = utf8_to_system("使用说明");
	string str_github = utf8_to_system("手动更新");
	string str_log = utf8_to_system("打开日志");
	string str_settings = utf8_to_system("打开配置");
	string str_about = utf8_to_system("关于");
	string str_exit = utf8_to_system("退出");
	
	InsertMenuA(menu, -1, MF_BYPOSITION, ID_TRAY_README, str_readme.c_str());
	InsertMenuA(menu, -1, MF_BYPOSITION, ID_TRAY_GITHUB, str_github.c_str());
	InsertMenuA(menu, -1, MF_SEPARATOR, 0, NULL);
	InsertMenuA(menu, -1, MF_BYPOSITION, ID_TRAY_SETTINGS, str_settings.c_str());
	InsertMenuA(menu, -1, MF_BYPOSITION, ID_TRAY_LOG, str_log.c_str());
	InsertMenuA(menu, -1, MF_BYPOSITION, ID_TRAY_ABOUT, str_about.c_str());
	InsertMenuA(menu, -1, MF_SEPARATOR, 0, NULL);
	InsertMenuA(menu, -1, MF_BYPOSITION, ID_TRAY_EXIT, str_exit.c_str());
	
	POINT pt;
	GetCursorPos(&pt);
	SetForegroundWindow(hwnd);
	TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
	PostMessage(hwnd, WM_NULL, 0, 0);
	DestroyMenu(menu);
}

// 显示关于对话框
void show_about_dialog(HWND hwnd)
{
	string msg = "lg-reminder\n\n"
				 "@version " + string(LG_REMINDER_VERSION) + "\n"
				 "@author " + string(LG_REMINDER_AUTHOR) + "\n"
				 "@license MIT\n"
				 "Copyright 2026 (c) " + string(LG_REMINDER_AUTHOR) + "\n"
				 "本脚本由洛谷 @" + string(LG_REMINDER_AUTHOR) + " 开发\n"
				 "感谢洛谷 @PenaltyKing 提供的思路及建议\n\n"
				 "在 Windows 通知弹窗提醒洛谷私信\n"
				 "感谢使用！";
	
	string msg_sys = utf8_to_system(msg);
	string title_sys = utf8_to_system("关于 lg-reminder");
	MessageBoxA(hwnd, msg_sys.c_str(), title_sys.c_str(), MB_OK | MB_ICONINFORMATION);
}

// 显示日志
void show_log_dialog(HWND hwnd)
{
	char log_path[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, log_path);
	strcat_s(log_path, "\\lg-reminder.log");
	
	if (!file_exists(log_path))
	{
		ofstream log(log_path);
		log.close();
	}
	
	open_file(log_path);
}

// 显示配置对话框
void show_config_dialog(HWND hwnd)
{
	char cfg_path[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, cfg_path);
	strcat_s(cfg_path, "\\config.txt");
	
	if (!file_exists(cfg_path))
	{
		ofstream cfg(cfg_path);
		cfg.close();
	}
	
	open_file(cfg_path);
	g_running = false;
}

// 清理并退出
void cleanup_and_exit(HANDLE mutex, int exit_code)
{
	if (g_hwnd)
	{
		remove_tray_icon();
		DestroyWindow(g_hwnd);
	}
	if (mutex)
	{
		ReleaseMutex(mutex);
		CloseHandle(mutex);
	}
	exit(exit_code);
}

// 窗口过程
LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		create_tray_icon(hwnd);
		break;
		
	case WM_DESTROY:
		remove_tray_icon();
		PostQuitMessage(0);
		break;
		
	case WM_TRAY_ICON:
		if (lParam == WM_LBUTTONUP || lParam == NIN_SELECT || lParam == 1029)
		{
			string url = "https://www.luogu.com.cn/chat?uid=" + to_string(g_id_to_uid[wParam]);
			open_url(url);
		}
		else if (lParam == WM_RBUTTONUP)
			show_context_menu(hwnd);
		break;
		
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_TRAY_ABOUT:
			show_about_dialog(hwnd);
			break;
		case ID_TRAY_SETTINGS:
			show_config_dialog(hwnd);
			break;
		case ID_TRAY_LOG:
			show_log_dialog(hwnd);
			break;
		case ID_TRAY_EXIT:
			g_running = false;
			DestroyWindow(hwnd);
			break;
		case ID_TRAY_README:
			open_url("https://github.com/Gary-0925/lg-reminder/blob/main/README.md");
			break;
		case ID_TRAY_GITHUB:
			open_url("https://github.com/Gary-0925/lg-reminder/tags");
			break;
		}
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// 单实例检查
	HANDLE mutex = CreateMutexA(NULL, TRUE, MUTEX_NAME);
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(mutex);
		return 0;
	}
	
	try
	{
		// 加载配置
		if (!load_config(g_cookie, g_uid, g_check_interval))
		{
			string error_msg = "错误：配置加载失败\n\n"
							   "请编辑 config.txt 文件，填入您的 cookie\n\n"
							   "如何获取 cookie：\n"
							   "1. 在浏览器中登录洛谷并进入私信页面\n"
							   "2. 按 F12 打开开发者工具\n"
							   "3. 切换到\"网络\"标签，刷新页面\n"
							   "4. 点进名称是\"chat\"的请求，往下翻，在 Request Headers 中复制\"Cookie\"\n"
							   "5. 将 cookie（注意是完整 cookie，不是只包含 __client_id）填入 config.txt";
			show_error_message(error_msg);
			write_log("配置异常退出");
			cleanup_and_exit(mutex, 1);
		}
		
		// 加载历史
		g_history_ids = load_history();
		
		// 创建窗口
		init_window_class(hInstance);
		g_hwnd = CreateWindowA("LGReminderClass", "lg-reminder", WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
		
		if (!g_hwnd)
		{
			show_error_message("无法创建窗口");
			cleanup_and_exit(mutex, 1);
		}
		
		// 启动检查线程
		thread check_thread(check_messages_loop);
		check_thread.detach();
		
		// 显示启动提示
		string start_msg = "lg-reminder " + string(LG_REMINDER_VERSION) + " 开始监听...\n\n"
						   "轮询间隔: " + to_string(g_check_interval) + " 秒\n"
						   "历史消息: " + to_string(g_history_ids.size()) + " 条\n\n"
						   "程序已在后台运行，可在系统托盘找到图标";
		show_info_message(start_msg);
		
		// 消息循环
		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0) && g_running)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		
		write_log("退出");
		cleanup_and_exit(mutex, 0);
	}
	catch (...)
	{
		write_log("异常退出");
		cleanup_and_exit(mutex, 1);
	}
	
	return 0;
}
