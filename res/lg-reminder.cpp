/*
lg-reminder
在 Windows 通知弹窗提醒洛谷私信
==================================================
@version 0.4
@author Gary0
@license MIT
本脚本由洛谷 @Gary0 开发
感谢洛谷 @PenaltyKing 提供的思路及建议
==================================================
本脚本不会盗取您的 cookie
使用了 AI 辅助开发，计划增加犇犇提醒和通知提醒等功能
==================================================
*/
#define lg_reminder_version "0.4"
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
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
using namespace std;

struct Msg { int id, uid; string name, time, con; bool is_new; };

string now()
{
	SYSTEMTIME st; GetLocalTime(&st);
	char b[64]; sprintf(b, "%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	return b;
}

string gbk(string u)
{
	if (u.empty()) return "";
	int wl = MultiByteToWideChar(CP_UTF8, 0, u.c_str(), -1, NULL, 0);
	if (!wl) return u;
	wchar_t *ws = new wchar_t[wl];
	MultiByteToWideChar(CP_UTF8, 0, u.c_str(), -1, ws, wl);
	int gl = WideCharToMultiByte(CP_ACP, 0, ws, -1, NULL, 0, NULL, NULL);
	if (!gl) { delete[] ws; return u; }
	char *gs = new char[gl];
	WideCharToMultiByte(CP_ACP, 0, ws, -1, gs, gl, NULL, NULL);
	string r(gs); delete[] ws; delete[] gs; return r;
}

string dec(string s)
{
	string r;
	for (size_t i = 0; i < s.length(); i++)
		if (s[i] == '\\' && i + 5 < s.length() && s[i + 1] == 'u')
		{
			char h[5] = {s[i + 2], s[i + 3], s[i + 4], s[i + 5], 0};
			int c = stoi(h, nullptr, 16);
			if (c < 0x80) r += (char)c;
			else if (c < 0x800) r += (char)(0xC0 | (c >> 6)), r += (char)(0x80 | (c & 0x3F));
			else r += (char)(0xE0 | (c >> 12)), r += (char)(0x80 | ((c >> 6) & 0x3F)), r += (char)(0x80 | (c & 0x3F));
			i += 5;
		}
		else r += s[i];
	for (char c : r) if ((unsigned char)c >= 0x80) return gbk(r);
	return r;
}

string ext(string h)
{
	string p = "window._feInjection = JSON.parse(decodeURIComponent(\"";
	size_t l = h.find(p); if (l == string::npos) return "";
	l += p.length(); size_t r = h.find("\"));", l);
	return (r == string::npos) ? "" : h.substr(l, r - l);
}

string url(string e)
{
	string d;
	for (size_t i = 0; i < e.length(); i++)
		if (e[i] == '%' && i + 2 < e.length())
		{
			int v; sscanf(e.substr(i + 1, 2).c_str(), "%x", &v);
			d += (char)v, i += 2;
		}
		else d += e[i];
	return d;
}

vector<Msg> parse(string j)
{
	vector<Msg> v;
	string s1 = "\"latestMessages\":{\"result\":[", s2 = "\"result\":[";
	size_t st = j.find(s1); if (st == string::npos) { st = j.find(s2); if (st == string::npos) return v; st += s2.length(); }
	else st += s1.length();
	size_t p = st;
	while (1)
	{
		p = j.find("{", p); if (p == string::npos || p > j.find("]", st)) break;
		Msg m = {0, 0, "", "", "", false};
		int bc = 1; size_t ed = p + 1;
		while (ed < j.length() && bc > 0) { if (j[ed] == '{') bc++; else if (j[ed] == '}') bc--; ed++; }
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
				char buf[16]; strftime(buf, 16, "%m-%d %H:%M", ti);
				m.time = buf;
			}
		}
		
		size_t fsender = o.find("\"sender\":");
		if (fsender != string::npos)
		{
			size_t np = o.find("\"name\":\"", fsender);
			if (np != string::npos)
			{
				np += 8; size_t ne = o.find("\"", np);
				if (ne != string::npos) m.name = dec(o.substr(np, ne - np));
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
				cs++; size_t ce = cs; bool esc = 0;
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
					if (m.con.length() > 30) m.con = m.con.substr(0, 27) + "...";
				}
			}
		}
		
		if (m.id && !m.name.empty()) v.push_back(m);
		p = ed;
	}
	return v;
}

void noti(vector<Msg> v, string us)
{
	if (v.empty()) return;
	for (auto &m : v) if (m.name != us)
	{
		string t = "洛谷新私信 - 来自 " + m.name, c = m.con.empty() ? "您有一条新消息" : m.con;
		NOTIFYICONDATAA n = {};
		n.cbSize = sizeof(NOTIFYICONDATAA);
		n.hWnd = GetConsoleWindow();
		n.uID = m.id;
		n.uFlags = NIF_INFO | NIF_ICON | NIF_TIP;
		n.dwInfoFlags = NIIF_INFO | NIIF_LARGE_ICON;
		n.uTimeout = 5000;
		strncpy(n.szInfoTitle, t.c_str(), sizeof(n.szInfoTitle) - 1);
		strncpy(n.szInfo, c.c_str(), sizeof(n.szInfo) - 1);
		strncpy(n.szTip, "lg-reminder", sizeof(n.szTip) - 1);
		HICON h = LoadIcon(NULL, IDI_INFORMATION);
		if (h) n.hIcon = h, n.uFlags |= NIF_ICON;
		Shell_NotifyIconA(NIM_ADD, &n);
		Sleep(2000), Shell_NotifyIconA(NIM_DELETE, &n), Sleep(500);
	}
}

bool http(string ck, string &r)
{
	HINTERNET s = NULL, c = NULL, q = NULL;
	s = WinHttpOpen(L"lg-reminder/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (s) c = WinHttpConnect(s, L"www.luogu.com.cn", INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (c) q = WinHttpOpenRequest(c, L"GET", L"/chat", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (q)
	{
		int l = MultiByteToWideChar(CP_UTF8, 0, ck.c_str(), -1, NULL, 0);
		wchar_t *w = new wchar_t[l];
		MultiByteToWideChar(CP_UTF8, 0, ck.c_str(), -1, w, l);
		wstring h = L"Cookie: "; h += w; h += L"\r\nUser-Agent: Mozilla/5.0\r\nAccept: text/html\r\nAccept-Language: zh-CN\r\n";
		delete[] w;
		WinHttpAddRequestHeaders(q, h.c_str(), h.length(), WINHTTP_ADDREQ_FLAG_ADD);
		if (!WinHttpSendRequest(q, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) goto end;
		if (!WinHttpReceiveResponse(q, NULL)) goto end;
		DWORD sz = 0, d = 0; char *b;
		do
		{
			if (!WinHttpQueryDataAvailable(q, &sz) || !sz) break;
			b = new char[sz + 1]; ZeroMemory(b, sz + 1);
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

bool cfg(string &c, string &u, int &t)
{
	ifstream f("config.txt");
	if (!f.is_open())
	{
		ofstream o("config.txt");
		if (o.is_open())
		{
			o << "# lg-reminder 配置\n\n";
			o << "cookie=你的完整cookie\n\n";
			o << "# 用户名\nusername=你的洛谷用户名\n\n";
			o << "# 轮询间隔\ninterval=15";
			o.close();
			o.close();
		}
		return false;
	}
	string l;
	while (getline(f, l))
	{
		if (l.empty() || l[0] == '#') continue;
		size_t p = l.find('='); if (p == string::npos) continue;
		string k = l.substr(0, p), v = l.substr(p + 1);
		if (k == "cookie") c = v;
		else if (k == "username") u = v;
		else if (k == "interval") t = stoi(v);
	}
	f.close();
	return !c.empty() && c.find("你的") == string::npos;
}

void save(vector<int> v)
{
	ofstream f("data.txt");
	if (f.is_open()) { for (int i : v) f << i << "\n"; f.close(); }
}

vector<int> load()
{
	vector<int> v; ifstream f("data.txt");
	if (f.is_open()) { int x; while (f >> x) v.push_back(x); f.close(); }
	return v;
}

vector<Msg> findnew(vector<Msg> cur, vector<int> lst)
{
	if (lst.empty()) return cur;
	vector<Msg> n;
	for (auto &m : cur)
	{
		bool ok = 0;
		for (int i : lst) if (m.id == i) { ok = 1; break; }
		if (!ok) n.push_back(m);
	}
	return n;
}

int main()
{
	SetConsoleOutputCP(CP_ACP), SetConsoleCP(CP_ACP), SetConsoleTitleA("lg-reminder");
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO ci;
	GetConsoleCursorInfo(h, &ci); ci.bVisible = 0; SetConsoleCursorInfo(h, &ci);
	
	SetConsoleTextAttribute(h, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	cout << "lg-reminder (v." << lg_reminder_version << " by " << lg_reminder_author << ")\n";
	cout << "==================================================\n\n";
	SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	
	string ck, us; int itv = 30;
	if (!cfg(ck, us, itv))
	{
		cout << "错误：配置加载失败\n\n";
		cout << "请编辑 config.txt 文件，填入您的 cookie\n\n";
		cout << "如何获取 cookie：\n";
		cout << "1. 在浏览器中登录 luogu.com.cn\n";
		cout << "2. 按 F12 打开开发者工具\n";
		cout << "3. 切换到“网络”标签，刷新页面\n";
		cout << "4. 点击任意请求，在 Request Headers 中找到“Cookie”\n";
		cout << "5. 复制 cookie 内容到 config.txt\n";
		cout << "\n按任意键退出..." << endl;
		system("pause > nul"); return 1;
	}
	
	vector<int> lst = load();
	cout << "启动信息:\n  轮询间隔: " << itv << " 秒\n  历史消息: " << lst.size() << " 条\n\n开始监听...\n按 Ctrl+C 退出\n==================================================\n";
	
	int cnt = 0; bool fr = lst.empty();
	while (1)
	{
		cnt++; string htm;
		cout << "[" << now() << "] 第 " << cnt << " 次检查... "; cout.flush();
		
		if (http(ck, htm))
		{
			string e = ext(htm);
			if (!e.empty())
			{
				string j = url(e);
				vector<Msg> v = parse(j);
				if (!v.empty())
				{
					cout << "发现 " << v.size() << " 条消息";
					vector<int> ids;
					for (auto &m : v) ids.push_back(m.id);
					vector<Msg> nw = findnew(v, lst);
					if (!nw.empty())
					{
						if (fr) cout << " [首次运行，记录 " << nw.size() << " 条]", fr = 0;
						else
						{
							cout << " [新消息: " << nw.size() << " 条]\n  └─ 来自:";
							for (auto &m : nw) cout << " " << m.name;
							noti(nw, us);
						}
						lst = ids, save(ids);
					}
					cout << endl;
				}
				else cout << "错误：解析失败" << endl;
			}
			else cout << "无消息数据" << endl;
		}
		else
		{
			SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_INTENSITY);
			cout << "错误：请求失败" << endl;
			SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
		}
		this_thread::sleep_for(chrono::seconds(itv));
	}
	return 0;
}