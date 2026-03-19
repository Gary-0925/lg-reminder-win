/*
lg-reminder
在 Windows 通知弹窗提醒洛谷私信
==================================================
@version 0.3
@author Gary0
@license MIT
本脚本由洛谷 @Gary0 开发
感谢洛谷 @PenaltyKing 提供的思路及建议
==================================================
本脚本不会盗取您的 cookie
使用了 AI 辅助开发，计划增加犇犇提醒和通知提醒等功能
==================================================
*/
#define lg_reminder_version "0.3"
#define lg_reminder_author "Gary0"

#include <bits/stdc++.h>
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <commctrl.h>
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

string dec(string s)
{
	string r;
	for (size_t i = 0; i < s.length(); i++)
		if (s[i] == '\\' && i + 5 < s.length() && s[i + 1] == 'u')
		{
			char h[5] = {s[i + 2], s[i + 3], s[i + 4], s[i + 5], 0};
			int c = stoi(h, nullptr, 16);
			if (c >= 0x4e00 && c <= 0x9fff)
				if (c <= 0x7ff) r += (char)(0xc0 | (c >> 6)), r += (char)(0x80 | (c & 0x3f));
				else r += (char)(0xe0 | (c >> 12)), r += (char)(0x80 | ((c >> 6) & 0x3f)), r += (char)(0x80 | (c & 0x3f));
			else r += '?';
			i += 5;
		}
		else r += s[i];
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
	size_t p = j.find("\"result\":["); if (p == string::npos) return v;
	p += 10;
	while (p < j.length() && j[p] != ']')
	{
		Msg m = {0, 0, "", "", "", false};
		size_t x;
		#define find_word(k) if ((x = j.find(k, p)) != string::npos && x - p < 500)
		
		find_word("\"id\":") { size_t a = j.find_first_of("0123456789", j.find(":", x)), b = j.find_first_not_of("0123456789", a); if (a != string::npos && b != string::npos) m.id = stoi(j.substr(a, b - a)); }
		find_word("\"name\":") { size_t a = j.find("\"", j.find(":", x) + 1), b = j.find("\"", a + 1); if (a != string::npos && b != string::npos) m.name = j.substr(a + 1, b - a - 1); }
		find_word("\"uid\":") { size_t a = j.find_first_of("0123456789", j.find(":", x)), b = j.find_first_not_of("0123456789", a); if (a != string::npos && b != string::npos) m.uid = stoi(j.substr(a, b - a)); }
		find_word("\"content\":") { size_t a = j.find("\"", j.find(":", x) + 1), b = j.find("\"", a + 1); if (a != string::npos && b != string::npos) { m.con = dec(j.substr(a + 1, b - a - 1)); if (m.con.length() > 30) m.con = m.con.substr(0, 27) + "..."; } }
		find_word("\"time\":") { size_t a = j.find_first_of("0123456789", j.find(":", x)), b = j.find_first_not_of("0123456789", a); if (a != string::npos && b != string::npos) { time_t t = stol(j.substr(a, b - a)); struct tm *ti = localtime(&t); char buf[16]; strftime(buf, 16, "%m-%d %H:%M", ti); m.time = buf; } }
		#undef find_word
		if (m.id && !m.name.empty()) v.push_back(m);
		p = j.find("},", p); if (p != string::npos) p += 2; else break;
	}
	return v;
}

void noti(vector<Msg> v, string us)
{
	if (v.empty()) return;
	for (auto &m : v)
		if (m.name != us)
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
			Sleep(6000), Shell_NotifyIconA(NIM_DELETE, &n), Sleep(500);
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
			o << "# lg-reminder config\n\n";
			o << "cookie=你的完整cookie\n\n";
			o << "# 用户名\nusername=你的洛谷用户名\n\n";
			o << "# 轮询间隔\ninterval=15";
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
	SetConsoleTitleA("lg-reminder");
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO ci;
	GetConsoleCursorInfo(h, &ci); ci.bVisible = 0; SetConsoleCursorInfo(h, &ci);
	
	SetConsoleTextAttribute(h, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	cout << "lg-reminder (v." << lg_reminder_version << " by " << lg_reminder_author << ")\n";
	cout << "==================================================\n\n";
	SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	
	string ck, us; int itv = 30;
	if (!cfg(ck, us, itv))
	{;
		cerr << "错误：配置加载失败\n\n";
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