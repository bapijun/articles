## python函数装饰器和php函数装饰器

python里面有一类特别有趣的语法糖名叫装饰器(Decorator).也就是给定的特定的语法结构允许对函数进行包装,添加特定的功能.

如果只看名字,可能会觉得和其他面向对象语言中的装饰器模式相似.可以参考[装饰器模式（装饰设计模式）详解](http://c.biancheng.net/view/1366.html)

### 装饰器的讨论

装饰器在python3.0之前并不是一个语法糖( syntax candy).一开始,他是因为python的语法特性而被人借用的常见开发方式.原因在于python糅杂了函数式编程的语法.允许吧函数作为参数参数和返回值使用,同时允许使用闭包的存在是的装饰器成为可能.

参考下面的代码

```
from functools import reduce

def sum(array):
    return reduce(lambda x,y:x+y,array)
```


这里我们定义了函数sum,用reduce函数,对一组输入参数进行叠加计算.
现在我们需要对这个函数进行装饰,添加一定的功能.这里是借鉴了装饰器的模式,也就是针对增加一些职责（即增加其额外功能）,只不过这里的对象变成了函数.

### 带有参数的装饰器

```
# 定义装饰器
def decorator_print(func):
    #准备包装的函数 使用可变参数
    def wrapper(*arg):
        #装饰器中可以添加额外功能的区域
        print(arg)
        return func(*arg)
    return wrapper
```
这里定义了装饰器,在wrapper中是定义的装饰器的核心语法,在return之前我们可以做我们想要添加的额外功能.我们在这里答应了参数.


### 语法分析

正如我们在第一段中说的那样,python语法中支持函数作为参数和返回值,同时允许在函数内定义函数.我们在这里定义了一个函数decorator_print,他使用func作为参数引入函数中,之后我们在函数内有定义了函数wrapper,在内部打印了自己的参数,然后调用了外部的func的函数.
最后在外部的函数中调用了内部的wrapper.

上面这一段看着很混乱对吧,但是如果你有一些其他语言的基础,我们会在下面的解释中引用一点通用语言的概念.

首先是作用域
>作用域是程序中定义的变量所存在的区域，超过该区域变量就不能被访问。

python的作用域可以参考如下文章[Python 变量作用域](https://blog.csdn.net/cc7756789w/article/details/46635383)
>局部作用域：这是在一个函数内部定义的变量；
闭包作用域：这是在一个函数外部另一个函数内部定义的变量；
全局作用域：在所有函数外定义的变量。

>对于各作用域中对变量的访问与修改：
各个局部作用域是相互独立的，其中的变量是无法相互访问的，但是可以访问全局作用域里的变量，但是对于全局变量的修改，需要在局部作用域里用关键字global提前声明，以表明该变量是全局变量；
在一个闭包函数形成的局部作用域中，可以访问其上级的闭包作用域里的变量，也可以修改其上级闭包作用域里的变量，但是修改的话，需要在局部作用域里用关键字nonlocal提前声明，以表明该变量不是局部变量，而是闭包作用域里的变量。

这里我们看到三个用必要分析的作用域func, arg, wrapper.
首先是func,他是外部函数decorator_print的一部分,因此在内部的函数中使用.
然后是arg,他作为内部函数定义的东西,自然可以用在第七行.然后是wapper,我们在函数内定义它,然后作为返回值返回.同样的也是func,他也是返回值.

如果你对c语言的函数指针熟悉的话,就可以很容易明白这里是在包装了两层的函数.
wrapper(func(arg)); 类似于这样的用法.然后我们在wrapper中定义了额外的功能
现在我们在代码中使用它
```
# 定义装饰器
def decorator_print(func):
    #准备包装的函数 使用可变参数
    def wrapper(*arg):
        #装饰器中可以添加额外功能的区域
        print(arg)
        return func(*arg)
    return wrapper

def sum(array):
    return reduce(lambda x,y:x+y,array)

data = [1,3,5,7,9]
sum = decorator_print(sum)
print(sum(data))
```
在这里我们吧sum函数作为参数引入decorator_print中,从而实现了对装饰器的使用

### 语法糖
可以说当装饰器的写法被人发明出来之后迅速流行起来,相比于对面对象中的装饰器模式的复杂,python里面的函数装饰器,确实更适合pythoner,于是在3.0中一个新的语法糖被引入.

```
from functools import reduce

# 定义装饰器
def decorator_print(func):
    #准备包装的函数 使用可变参数
    def wrapper(*arg):
        #包装的目的是打印参数
        print(arg)
        return func(*arg)
    return wrapper

@decorator_print
def sum(array):
    return reduce(lambda x,y:x+y,array)

data = [1,3,5,7,9]

print(sum(data))
```
注意通过@decorator_print的使用我们避免了定义sum = decorator_print(sum)这样的语法.从而加快的书写速度,这在某些复杂的装饰器中可以大大加快理解和写代码的速度.

### 借鉴python装饰器使用的php函数装饰器
实际上在其他语言中,比如在php 中因为在Callable 类型(你可以理解为函数参数)使用参数了,导致我们可以实现类似于python函数装饰器类似的功能.
同时需要使用call_user_func使用函数的语法

```
<?php
function sum($arg)
{
    $i = 0;
    if ($arg) {
        foreach ($arg as $a) {
            $i += $a;
        }
    }
    print($i);
}
function decorator_print( $wrapper, $arg )
{
    var_dump($arg);
    call_user_func($wrapper, $arg );
}

$arg = [1, 2, 3];
decorator_print( 'sum', $arg );

```

### C语言
同样的在函数指针大量存在使用C语言系中我们同样可以使用类似的语法.这里就不献丑了,我在系统级别的众多语法中看到大量的用法.这也是因为c语言本身就支持函数指针作为参数和返回值使用.当然主要的问题在于早期c的函数指针会限制函数指针本身的参数,这就导致他的使用可能没有那么全面,不像python和php支持可变参数.
