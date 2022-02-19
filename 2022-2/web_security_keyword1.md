# 白帽子讲web安全读后感1

本文实际上算是[白帽子讲web安全](https://weread.qq.com/web/reader/7c4327b05cfd497c4eaa52f)的回顾,总结了一些我觉得比较重要的关键词,方便以后复习.具体内容可以参考书

### 浏览器安全
本文对应第2章节,主要的内容是[同源策略](https://blog.csdn.net/u011822516/article/details/122695343).当然也可以参考[阮一峰老师的文章](https://www.ruanyifeng.com/blog/2016/04/same-origin-policy.html)

终止就是要保证协议,域名和端口的一致,如果看到提示cros错误就要考虑这个问题.

另外现在app开发喜欢采用浏览器套壳的策略,同时还有h5小程序开发,总之前端技术大有可为.

### 跨站脚本攻击(XSS)


本文是第三章
跨站脚本攻击(Cross Site Script，简称 XSS)，利用网页开发时留下的漏洞，通过巧妙的方法注入恶意指令代码到网页，使用户加载并执行攻击者恶意制造的网页程序.攻击方法多种多样,主要是在js层面,早年间还有一些flash漏洞.不过现代应该会有一些利用h5的漏洞存在

本书中给的几个案例都是比较过时的思路,比如js的标签里面注入一段脚本.

或者通过css中添加脚本(百度的那个例子)

实际上这几年有一个案例我印象深刻.就是前端的工具链模式下(所谓现代开发方式),开发者几乎不会检查引入的文件,因此可以通过上传恶意代码,通过审核,或者开发者自己上传恶意代码(这个也出现过案例).

### 跨站点请求伪造(CSRF)

关键词:
1. (Cross-site request forgery,简称 CSRF)， 是一种挟制用户在当前已登录的 Web 应用程序上执行非本意的操作的攻击方法
2. 防御方法
- 增加验证码(简单有效)
- 检查请求来源是否合法
- 增加随机 token

实际上就是非法访问接口.这个请求可以和第四章内容结合,首先通过跨域脚本攻击注入脚本,然后在进行访问

本书中提到一个有趣的例子

>2008年9月，国内的安全组织80sec公布了一个百度的CSRF Worm。漏洞出现在百度用户中心的发送短消息功能中：这个解耦只需要修改参数sn，即可对指定的用户发送短消息。而百度的另外一个接口则能查询出某个用户的所有好友

>将两者结合起来，可以组成一个CSRF Worm——让一个百度用户查看恶意页面后，将给他的所有好友发送一条短消息，然后这条短消息中又包含一张图片，其地址再次指向CSRF页面，使得这些好友再次将消息发给他们的好友，这个Worm因此得以传播。

实际上这个案例告诉我们在设计接口的时候最好考虑限制用户的资源访问和权限,如果限制当前用户发送消息的权限,就能很好的限制漏洞的传播.

这个蠕虫很好地展示了CSRF的破坏性——即使没有XSS漏洞，仅仅依靠CSRF，也是能够发起大规模蠕虫攻击的。
防御手段
- 验证码
  验证码被认为是对抗CSRF攻击最简洁而有效的防御方法。验证码只能作为防御CSRF的一种辅助手段，而不能作为最主要的解决方案。

- Referer
  CheckReferer Check在互联网中最常见的应用就是“防止图片盗链”。同理，RefererCheck也可以被用于检查请求是否来自合法的“源”。常见的互联网应用，页面与页面之间都具有一定的逻辑关系，这就使得每个正常请求的Referer具有一定的规律。比如一个“论坛发帖”的操作，在正常情况下需要先登录到用户后台，或者访问有发帖功能的页面。在提交“发帖”的表单时，Referer的值必然是发帖表单所在的页面。如果Referer的值不是这个页面，甚至不是发帖网站的域，则极有可能是CSRF攻击。

- token
  
  token这个基本是惯例了,那么token的使用原则有那些?

  原文提到的随机性这个不必说,我们现在的token都是框架生成的.
  Token如果出现在某个页面的URL中，则可能会通过Referer的方式泄露.这个要小心谨慎,避免出现访问的过程中有token

  还有一些其他的途径可能导致Token泄露。比如XSS漏洞或者一些跨域漏洞，都可能让攻击者窃取到Token的值。这也证明了一点,csrf攻击本身就和xss漏洞和跨域漏洞关系很深

### 点击劫持（ClickJacking)
本文是第五章
实际上本文还是比较前端的内容
点击劫持是一种视觉上的欺骗手段。攻击者使用一个透明的、不可见的iframe，覆盖在一个网页上，然后诱使用户在该网页上进行操作，此时用户将在不知情的情况下点击透明的iframe页面。通过调整iframe页面的位置，可以诱使用户恰好点击在iframe页面的一些功能性按钮上。

ClickJacking是一种视觉上的欺骗，那么如何防御它呢？针对传统的ClickJacking，一般是通过禁止跨域的iframe来防范。

### h5安全

这个就是书里面提到的一些漏洞,当然我不是前端不是很了解这些内容就是了.不过大体上依旧是之前提到的那几个内容的在h5上面的翻新

### 注入攻击总结

注入攻击是应用违背了“数据与代码分离原则”导致的结果。它有两个条件：一是用户能够控制数据的输入；二是代码拼凑了用户输入的数据，把数据当做代码执行了。在对抗注入攻击时，只需要牢记“数据与代码分离原则”，在“拼凑”发生的地方进行安全检查，就能避免此类问题。

### sql注入

 SQL 注入

输入的字符串中注入 SQL 指令，若程序当中忽略了字符检查，导致恶意指令被执行而遭到破坏或入侵.这个我认为对于老的代码可能用的就比较多了,一般新生的框架,使用链式访问都可以有效避免注入.只有那些要自己写的嵌入的组合代码就非常容易出现注入的问题.

#### sql注入技巧

1. 盲注
所谓“盲注”，就是在服务器没有错误回显时完成的注入攻击。服务器没有错误回显，对于攻击者来说缺少了非常重要的“调试信息”，所以攻击者必须找到一个方法来验证注入的SQL语句是否得到执行。最常见的盲注验证方法是，构造简单的条件语句，根据返回页面是否发生变化，来判断SQL语句是否得到执行。

2. time attack

Timing Attack，来判断漏洞的存在,简单的就是使用某些函数来拖慢访问时间,如果函数生效,时间就会变长。在MySQL中，有一个BENCHMARK()函数，它是用于测试函数性能的。

3. 工具

sqlmap.py就是一个非常好的自动化注入工具

4. 命令执行

在MySQL中，除了可以通过导出webshell间接地执行命令外，还可以利用“用户自定义函数”的技巧，即UDF（User-Defined Functions）来执行命令。在流行的数据库中，一般都支持从本地文件系统中导入一个共享库文件作为自定义函数。

在MySQL 4的服务器上，Marco Ivaldi公布了如下的代码，可以通过UDF执行系统命令。尤其是当运行mysql进程的用户为root时，将直接获得root权限。

实际上调用命令都是非常危险的行为,尤其是使用root用户,包括php里面使用这个调用linux命令也是大忌.

5. 攻击存储过程

 存储过程为数据库提供了强大的功能，它与UDF很像，但存储过程必须使用CALL或者EXECUTE来执行。在MS SQL Server和Oracle数据库中，都有大量内置的存储过程。在注入攻击的过程中，存储过程将为攻击者提供很大的便利。在MS SQL Server中，存储过程“xp_cmdshell”可谓是臭名昭著了，无数的黑客教程在讲到注入SQL Server时都是使用它执行系统命令.

 这个命令和我上文提到的要小心调用命令是一样的逻辑

 6. 字符集攻击
书里面提到了给予宽字符集合的攻击,我觉得当代应该都用utf-8编码了吧,还有人用gbk的老旧编码么?
主要原理是编码和转义符号造成的漏洞,对于给予转义过滤的攻击.

7. SQL CoIumn Truncation
   这个倒是很有趣,基本上就是某一个mysql模式下,对于超长的insert语句,是可以插入成功的,只是报一个警告()
书中提到的例子是这样的,采用用户名限制长度,然后输入一个同名用户名+一串空格,这样就会有出现相同用户名的客户,导致可能出现越权访问问题.



#### SQL 注入防御

- 使用预编译语句绑定变量(最佳方式)
当然框架如果正常使用,都是会采用预编译绑定

- 使用安全的存储过程(也可能存在注入问题)
- 检查输入数据的数据类型(可对抗注入)
- 数据库最小权限原则
  
  避免Web应用直接使用root、dbowner等高权限账户直接连接数据库。如果有多个不同的应用在使用同一个数据库，则也应该为每个应用分配不同的账户。Web应用使用的数据库账户，不应该有创建自定义函数、操作本地文件的权限。



### 代码注入

代码注入比较特别一点。代码注入与命令注入往往都是由一些不安全的函数或者方法引起的，其中的典型代表就是php eval().
远程调用命令和文件(php里面的include),都会有类似的漏洞,最近闹得沸沸扬扬的javalog漏洞也是在低版本jdk下面,会远程调用命令,导致注入成功.

代码注入往往是由于不安全的编程习惯所造成的，危险函数应该尽量避免在开发中使用，可以在开发规范中明确指出哪些函数是禁止使用的。这些危险函数一般在开发语言的官方文档中可以找到一些建议。


###  CRLF注入

CRLF实际上是两个字符：CR是Carriage Return (ASCII 13, \r), LF是Line Feed(ASCII 10,\n)。\r\n这两个字符是用于表示换行的，其十六进制编码分别为0x0d、0x0a。CRLF常被用做不同语义之间的分隔符。因此通过“注入CRLF字符”，就有可能改变原有的语义。

在HTTP协议中，HTTP头是通过“\r\n”来分隔的。因此如果服务器端没有过滤“\r\n”，而又把用户输入的数据放在HTTP头中，则有可能导致安全隐患。这种在HTTP头中的CRLF注入，又可以称为“Http Response Splitting”。