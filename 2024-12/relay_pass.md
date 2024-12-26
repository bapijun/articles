
# relay pass 的关键概念

最近刚好在写tvm的一个量化相关的内容,这里因为需要手动实现一个pass,所以整理了一下relay有关的一些概念

## pass

pass 就是优化

## pass的类型

## 如何添加pass

https://tvm.hyper.ai/docs/dev/how_to/relay_add_pass/

先注册一个pass

用 Pass Manager 注册 Pass

然后编写对应的ast的 visit

表达式访问器（Expression Visitors）h和表达式修改器的区别是什么

我这里应该是需要一个表达式修改器

## visitExpr

用于遍历 Relay 程序的基类是 ExprFunctor。它提供的公共接口是一个 VisitExpr 方法，该方法接收一个表达式以及零个或多个参数，并返回某种类型的实例。扩展此类时，可以通过覆盖每种表达式类型的 VisitExpr_ 实现，来定义 AST 遍历模式。

VisitExpr 和 VisitExpr_ 之间的关系与调度有关。每个 VisitExpr_ 定义都针对特定类型的表达式，但用户无法每次都得知要访问的节点类型。为了解决这个问题，ExprFunctor 提供了一个 VisitExpr 函数，将给定表达式路由转换为 VisitExpr_ 实例进而解决问题。尽管 C++ 已经提供了动态调度，但 ExprFunctor 定义了自己的虚表供 VisitExp 使用。通过定义虚表可以更好地控制调度。例如，定义一个在每次访问之前都打印 "Here" 的 PrintVisitor 遍历器，可以覆盖 VisitExpr：