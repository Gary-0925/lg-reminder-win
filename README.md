[![最新版本](https://img.shields.io/github/v/release/Gary-0925/lg-reminder?sort=date&style=flat-square&label=%E6%9C%80%E6%96%B0%E7%89%88%E6%9C%AC&color=%23aa99dd)](https://github.com/Gary-0925/lg-reminder/releases/latest)[![最新版本发布时间](https://img.shields.io/github/release-date/Gary-0925/lg-reminder?style=flat-square&label=%20&color=%23aa99dd)](https://github.com/Gary-0925/lg-reminder/releases/latest)
[![最近更新](https://img.shields.io/github/last-commit/Gary-0925/lg-reminder?style=flat-square&label=%E6%9C%80%E8%BF%91%E6%9B%B4%E6%96%B0
)](https://github.com/Gary-0925/lg-reminder/commit/main)
[![license](https://img.shields.io/github/license/Gary-0925/lg-reminder.svg?style=flat-square)](https://github.com/Gary-0925/lg-reminder/blob/main/LICENSE)

---

省流：在Windows弹窗显示洛谷私信的工具可以到[这里](https://github.com/Gary-0925/lg-reminder/releases/latest)下载运行后按说明填写配置再运行就能用了。

~~明目张胆要 star。QAQ~~

---

## 总览

不知道《洛谷网红》有没有这样的困扰：洛谷私信太多，读不过来，容易漏消息，难以及时看到消息。~~其实我猜大概没有。~~

在 @[PenaltyKing](https://www.luogu.com.cn/user/976095) 的建议下，我写了一个在 Windows 通知中弹窗显示洛谷私信的工具。

该程序会在后台无窗口运行，并在系统托盘中显示，监听到新的洛谷私信时在通知中提示，点击通知后跳转到私信。

可以前往这里下载： https://github.com/Gary-0925/lg-reminder/releases/latest 。  
项目地址： https://github.com/Gary-0925/lg-reminder 。  
洛谷地址： https://www.luogu.com.cn/article/ftltn0ld 。  

如果你只想使用程序，请点击上面的链接，点击下载 `lg-reminder.windows.zip`，**下载后一定要先解压再运行**。

## 使用方法

运行一次 `lg-reminder.exe`，之后关掉程序，现在你放程序的目录下应该自动生成了一个 `config.txt`，内容应该是：

```txt
# lg-reminder 配置

# 你的洛谷cookie
cookie=你的完整cookie

# 你的洛谷用户id
uid=你的uid

# 轮询间隔（秒）
interval=10
```

- 在 `cookie=` 后面填写洛谷 cookie。

> ### cookie 获取方法
>
> 1. 在浏览器中登录洛谷并进入私信页面 
> 2. 按 F12 打开开发者工具
> 3. 切换到“网络”标签，刷新页面
> 4. 点进名称是“chat”的请求，往下翻，在 Request Headers 中复制“Cookie”
> 5. 将 cookie（注意是完整 cookie，不是只包含 __client_id）填入 config.txt
> 
> ![](https://cdn.luogu.com.cn/upload/image_hosting/1m9kqkci.png)
>
> 注意是完整 cookie，格式大概形如：
>
> ```
> code=1; uid=1202669; notice=14695742; version=1.14.1; engine=bing; __client_id=*********************; _uid=1202669;
> ```
>
> 本人（开发者）郑重承诺，不会利用您输入的 cookie 信息对您的账号进行除读取私信消息以外的任何操作，也不会查取您的私信信息或将其发送给第三方。
>
> **不要把你的 cookie 分享给别人，否则账号被盗概不负责。**

- 在 `uid=` 后面填写洛谷 uid（个人主页 URL 中的数字）。

- 在 `interval=` 后面填写你希望设定的轮询间隔（秒），建议不要短于 15，否则可能导致 IP 被封禁。

现在配置就填完了，我的配置是：

```txt
# lg-reminder 配置

# 你的洛谷cookie
cookie=code=1; uid=1202669; notice=14695742; version=1.14.1; engine=bing; __client_id=*********************; _uid=1202669;

# 你的洛谷用户id
uid=1202669

# 轮询间隔（秒）
interval=10
```

供参考。

现在运行程序，如果显示 `开始监听...` ，那么恭喜你，你已经配置完成了。
