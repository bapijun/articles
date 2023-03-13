# windows下nvvp的基础使用1

本来先写nsignt的使用方式,不过折腾了一会发现没弄得那么明白.先用着nvvp好了,毕竟只是先看书配合着写点简单的cuda代码而已

## 安装建议

在windows下安装cuda的话,也就那回事,自己可以参考一下搜索引擎

(win10安装cuda)
https://blog.csdn.net/RunAtWorld/article/details/124282176

安装英伟达的套装和然后把对应的路径放到系统路径里面,唯一的问题是需要自己手动安装c1.exe,也就是windows下对应的C ++编译器,我的思路是自己下载virsual studio,然后选择安装MSVC,然后把里面的对应的c1路径放到系统路径下面
我这边是这样的版本:
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x64

当然你也可以自己单独安装MSVC,然后加入系统路径

## 使用方式

首先先用nvcc编译程序,可以运行后
nvvp exe程序即可

几个值得注意的点

![1](/2023-2/nvvp/1.png)

首先是这个分析很好用

![2](/2023-2/nvvp/2.png)

常见的度量事件和参数,nvprof里面常见的那些
