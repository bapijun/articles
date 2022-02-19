## mysql:EXPLAIN  SHOW WARNINGS一起使用得到实际执行的sql

之前写一个where in子查询,看到了这一篇[深入理解MySql子查询IN的执行和优化](https://www.cnblogs.com/wxw16/p/6105624.html)文章,里面提到了这一句,让我觉得很有趣

>通过EXPLAIN EXTENDED 和 SHOW WARNINGS命令，看到如下结果：

>select `northwind`.`driver`.`driver_id` AS `driver_id` from `northwind`.`driver` where <in_optimizer>(`northwind`.`driver`.`driver_id`,<exists>(select 1 from `northwind`.`user` where ((`northwind`.`user`.`uid` = `northwind`.`driver`.`driver_id`) and (<cache>(`northwind`.`driver`.`driver_id`) = `northwind`.`driver`.`driver_id`))))

>可以看出无论是独立子查询还是相关子查询，MySql 5.5之前的优化器都是将IN转换成EXISTS语句。如果子查询和外部查询分别返回M和N行，那么该子查询被扫描为O(N+N*M)，而不是O(N+M)。这也就是为什么IN慢的原因。

很好,我虽然之前大量使用explain调优,但是还是第一次知道这一种用法特地查了一下

> The EXPLAIN statement produces extra (“extended”) information that is not part of EXPLAIN output but can be viewed by issuing a SHOW WARNINGS statement following EXPLAIN.

官方原文说法是说explain 可以给出额外的解释,但是需要 SHOW WARNINGS使用. 注意不同版本的mysql这样的使用可能有不一致.我看到有的博客说explain extended会多一个fliter,我查了一下,在我的版本(5.7)中并没有这个.文档里面就说在5.7之后不再需要extended,这个词了,直接使用就可以得到实际上优化器优化后的sql.



这里我试了一下,要注意这两个要接在一起.

mysql> EXPLAIN
       SELECT t1.a, t1.a IN (SELECT t2.a FROM t2) FROM t1\G
*************************** 1. row ***************************
           id: 1
  select_type: PRIMARY
        table: t1
         type: index
possible_keys: NULL
          key: PRIMARY
      key_len: 4
          ref: NULL
         rows: 4
     filtered: 100.00
        Extra: Using index
*************************** 2. row ***************************
           id: 2
  select_type: SUBQUERY
        table: t2
         type: index
possible_keys: a
          key: a
      key_len: 5
          ref: NULL
         rows: 3
     filtered: 100.00
        Extra: Using index
2 rows in set, 1 warning (0.00 sec)

mysql> SHOW WARNINGS\G
*************************** 1. row ***************************
  Level: Note
   Code: 1003
Message: /* select#1 */ select `test`.`t1`.`a` AS `a`,
         <in_optimizer>(`test`.`t1`.`a`,`test`.`t1`.`a` in
         ( <materialize> (/* select#2 */ select `test`.`t2`.`a`
         from `test`.`t2` where 1 having 1 ),
         <primary_index_lookup>(`test`.`t1`.`a` in
         <temporary table> on <auto_key>
         where ((`test`.`t1`.`a` = `materialized-subquery`.`a`))))) AS `t1.a
         IN (SELECT t2.a FROM t2)` from `test`.`t1`
1 row in set (0.00 sec)