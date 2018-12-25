代码优化技巧2:循环优化

---
本文算不上什么很有独创性的东西,在很多人的文章和书籍中都有看到,比如深入理解操作系统在的5/4,韩天峰老师的谋篇<php需要聪明人的语言>也提到过一些相关内容.

---

在大多数的可以允许过程化编程的语言中,循环永远是经常用到的结构.但是每一次使用循环我们都要尤其小心谨慎,避免循环出现的低效率.我们在这里使用一个有趣的例子.

### 避免在循环条件中使用函数

考虑下面一个循环
```
string v;
long i;
for (i = 0; i <= getlength(v); i++) {
	data_v val;
	get_vec_element(v, i);
	val->i += v;
}
```

我们在循环体里面访问函数获取v的长度.很容易我们就看出这是低效率的.我们可以吧获取长度放到外面来.

```
string v;
long i;
long length =  getlength(v);
for (i = 0; i <= length; i++) {
	data_v val;
	get_vec_element(v, i);
	val->i += v;
}
```

很容易的有些人会以为编译器会足够的只能回自动优化成第二类代码.实际上并非如此.因为编译器是无法知道函数是否会变v产生额外影响.即便是采用内联函数优化,编译器也没如此强大的分析能力.

在极端的情况下,比如吧字符串全部大写的函数,每一次循环都访问strlen,这样函数的时间复杂度会变从线性转化为二次.复杂度斗争.

### 循环展开

循环展开是一种优化方法,就是增加每一次迭代计算的数目,减少迭代的次数从而达到优化代码的目的.

```
void combine5(double data[],int length)
{
    double sum = 0.0;
    for(int i=0;i<length;i++)
    {
        sum *= data[i];
    }
    cout<<sum<<endl;
}
void combine6(double data[],int length)
{
    double sum = 0.0;
    int limit = length-1;
    int i;
    for(i=0;i<limit;i+=2)
    {
        sum = sum*data[i]*data[i+1];
    }
    for(;i<length;i++)
    {
        sum *= data[i];
    }
    cout<<sum<<endl;
}
void combine7(double data[],int length)
{
    double sum1=0.0,sum2=0.0;
    int limit = length-1;
    int i;
    for(i=0;i<limit;i+=2)
    {
        sum1 *= data[i];   //  合并下标为偶数的值，  0按偶数算
        sum2 *= data[i+1];  //  合并下标为奇数的值
    }
    double sum = sum1*sum2;
    for(;i<length;i++)
    {
        sum *= data[i];
    }
    cout<<sum<<endl;
}

```

  combine5只是做了一些简单的优化，combine6进行了循环展开，combine7既循环展开又多路并行.由于cpu流水线的存在.cpu可以大幅度优化代码的效率.

  有趣的是编译器是很容易执行循环展开的,只要把gcc的优化等级调高到o3就可以直接使用循环展开优化.

  ### 迭代器模式

  我再次强烈推荐使用迭代器模式.比如php中最常见的foreach就是迭代器模式的体现.使用迭代器模式有几个好处

  - 避免手动写循环条件造成低效率
  - 减少内存占用.由于迭代器模式下不会访问整个数组/容器.从而减少内存占用.
  - 更少的出错.毋庸置疑使用迭代器可以避免手动写循环导致的错误
