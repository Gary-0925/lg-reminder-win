省流：在Windows弹窗显示洛谷私信的工具可以到[这里](https://github.com/Gary-0925/lg-reminder-win/tags)下载运行后按说明填写配置再运行就能用了。

---

## 总览

不知道《洛谷网红》有没有这样的困扰：洛谷私信太多，读不过来，容易漏消息，难以及时看到消息。~~其实我猜大概没有。~~

在 @[PenaltyKing](/user/976095) 的建议下，我写了一个在 Windows 通知中弹窗显示洛谷私信的工具。

可以前往这里下载： https://github.com/Gary-0925/lg-reminder-win/tags 。  
项目地址： https://github.com/Gary-0925/lg-reminder-win 。

如果你只想使用脚本，请点击上面的链接，进入最新版本，并点击 `lg-reminder.exe`。

## 使用方法

运行一次 `lg-reminder.exe`，之后关掉程序，现在你放程序的目录下应该自动生成了一个 `config.txt`，内容应该是：

```txt
# lg-reminder config

cookie=你的完整cookie

# 用户名
username=你的洛谷用户名

# 轮询间隔
interval=15
```

> 在 `cookie=` 后面填写洛谷 cookie。
> 
> > :::info[cookie 获取方法]
> > 1. 在浏览器中登录 luogu.com.cn
> > 2. 按 F12 打开开发者工具
> > 3. 切换到“网络”标签，刷新页面
> > 4. 点击任意请求，在 Request Headers 中找到“Cookie”
> > 5. 复制 cookie 内容到 config.txt
> > :::
> > 
> > 本人（开发者）郑重承诺，不会利用您输入的 cookie 信息对您的账号进行除读取私信消息以外的任何操作，也不会查取您的私信信息或将其发送给第三方。
> > 
> > **不要把你的 cookie 分享给别人，否则账号被盗概不负责。**
> 
> 在 `username=` 后面填写洛谷用户名。
> 
> 在 `interval=` 后面填写你希望设定的轮询间隔，建议不要短于 15，否则可能导致 IP 被封禁。

现在运行程序，如果显示 `开始监听...` ，那么恭喜你，你已经配置完成了。
