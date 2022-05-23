# 生成器思想与协程

最近在学习python,刚好看到生成器(generator),突然脑子灵光一闪,想到多年前看php文档的时候也花了时间去研究php的生成器与协程.于是就把之前写的东西总结一下,放下来.

### 生成器
看到这个名字,难免会和设计模式的生成器模式弄混.其实看英语会比较明显,一个是Builder Pattern,一个是generator.在某种程度上说这也是英文专业名词翻译的大锅了.笑,这也是为什么说程序员要学习英语的一个例证了.

生成器的实现其实很简单,就是处理大数据集合,不是使用数组一次性放到一个数组中,而是利用迭代器的思想,用算法的思想生成.只保存i的状态,然后用算法生成当前的数.

以php为例

```
<?php
function gen_one_to_ten_thousand() {
    for ($i = 1; $i <= 10000; $i++) {
        //注意变量$i的值在不同的yield之间是保持传递的。
        yield $i;
    }
}

$generator = gen_one_to_ten_thousand();
foreach ($generator as $value) {
    echo "$value\n";
}
```

用python的语法的话就是

```
def gen_one_to_ten_thousand():
    for i in range(1, 10):
        yield i


for i in gen_one_to_ten_thousand():
    print(i)

```

(迭代)生成器也是一个函数,不同的是这个函数的返回值是依次返回,而不是只返回一个单独的值.或者,换句话说,生成器使你能更方便的实现了迭代器接口.

里面的代码并没有真正的执行，而是返回了一个生成器对象$generator = Generator Object( )，$generator instanceof Iterator说明Generator实现了Iterator接口，可以用foreach进行遍历，每次遍历都会隐式调用current()、next()、key()、valid()等方法。所以 这里就不是原本的函数模式,不可以按照之前函数的逻辑.即函数,栈,恢复状态这样的逻辑来理解.

```
//这里之所以放出这个代码 是为了提醒读者 生成器有哪些可以被调用的方法 同样的python里面一样可以对生成器调用next方法
final class Generator implements Iterator {
/* 方法 */
public current(): mixed
public getReturn(): mixed
public key(): mixed
public next(): void
public rewind(): void
public send(mixed $value): mixed
public throw(Throwable $exception): mixed
public valid(): bool
public __wakeup(): void
}
```

当然,也可以不同通过生成器来实现这个功能,而是可以通过继承Iterator接口实现.但通过使用生成器实现起来会更方便,不用再去实现iterator接口中的5个方法了.实际上这个方法是对于设计模式中的iterator的更缩略实现,实际上由于定义的问题,在相关的文档中没有很好的解释这个问题


 调用迭代器的方法一次, 其中的代码运行一次.例如, 如果你调用$range->rewind(), 那么xrange()里的代码就会运行到控制流第一次出现yield的地方. 而函数内传递给yield语句的返回值可以通过$range->current()获取.

### 协程
协程的出现其实和生成器的关系不大,其实更像是编程语言级别下对于多任务的思考.当然从语言学的角度上说确实是先出现协程的思想,之后出现多进程切换,多线程切换.因此如果在编写代码的时候并不会考虑多线程/任务的情况下,协程对于读者而言只是一个名词而已.

我在编写这边写之前那篇ppt到这篇博客之前数年,写多线程的代码屈指可数,笑嘻了

#### 协程2
协程的支持是在迭代生成器的基础上, 增加了可以回送数据给生成器的功能(调用者发送数据给被调用的生成器函数). 这就把生成器到调用者的单向通信转变为两者之间的双向通信.

传递数据的功能是通过迭代器的send()方法实现的. 下面的logger()协程是这种通信如何运行的例子：
```

<?php
function logger($fileName) {
    $fileHandle = fopen($fileName, 'a');
    while (true) {
        fwrite($fileHandle, yield . "\n");
    }
}
$logger = logger(__DIR__ . '/log');
$logger->send('Foo');
$logger->send('Bar')
?>

```

协程是非常强大的概念,不过却应用的很稀少而且常常十分复杂.要给出一些简单而真实的例子很难.
 在这篇文章里,我决定去做的是使用协程实现多任务协作.我们要解决的问题是你想并发地运行多任务(或者“程序”）.不过我们都知道CPU在一个时刻只能运行一个任务（不考虑多核的情况）.因此处理器需要在不同的任务之间进行切换,而且总是让每个任务运行 “一小会儿”.

多任务协作这个术语中的“协作”很好的说明了如何进行这种切换的：它要求当前正在运行的任务自动把控制传回给调度器,这样就可以运行其他任务了. 这与“抢占”多任务相反, 抢占多任务是这样的：调度器可以中断运行了一段时间的任务, 不管它喜欢还是不喜欢. 协作多任务在Windows的早期版本(windows95)和Mac OS中有使用, 不过它们后来都切换到使用抢先多任务了. 理由相当明确：如果你依靠程序自动交出控制的话, 那么一些恶意的程序将很容易占用整个CPU, 不与其他任务共享.

现在你应当明白协程和任务调度之间的关系：yield指令提供了任务中断自身的一种方法, 然后把控制交回给任务调度器. 因此协程可以运行多个其他任务. 更进一步来说, yield还可以用来在任务和调度器之间进行通信.

关于协程的思考,可以参考Laruence巨巨的这一篇文章[在PHP中使用协程实现多任务调度](https://www.laruence.com/2015/05/28/3038.html)