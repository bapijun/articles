避免在循环中访问数据库,一次对于PHP代码的优化经历
===

这次是在上班过程中发生的事件,我隔壁的同事正在写一个功能.这个功能简单的描述是这样的:从数据库几个表中获取数据,导入到excel,类似的功能,实际上之间我们之前已经完成过不少,然而这一次却遇到的问题.那就是代码运行的太慢了.跑了三十几秒代码自己断开了

### 代码

原来的代码段很长,所以我做了简化 省略号表示还有其他代码,实际上原来的代码更为复杂有更多以的循环和嵌套.
```
...
$row = $db->select('game_team',array('id','name'), $where);
if($row != NULL){
    foreach($row as $k => $v) {

        $teamappl = $db->select('game_team_apply', array('name','code','nation','sex'), array('AND' => array("game_team_id" => $v['id'], "type" => "1")));
        if ($teamappl != NULL) {
            foreach ($teamappl as $k1 => $vb) {
                $item_id_array = $db->select('gameitem', 'id', array('game_id' => $_GET['gid']));
                if (!empty($item_id_array)) {
                    foreach ($item_id_array as $k3 => $v3) {

                        $ifitemname = $db->get('game_item_eroll', 'id', array('AND' => array('gameitem_id' => $v3, 'apply_id' => $vb['id'])));
                        ...
                        $game_type = $db->get('gameitem', 'type', array('AND' => array('id' => $v3)));
                        if (!empty($game_type) && $game_type == 2) {
                        ...
                        }

                    }
                }
                $content[] = $tmp;
                unset($tmp);
            }
        }
    }

}

create_xls($content,$downloadfilename);
```
我的同事经过一系列的调试,加断点,查看日志,终于确认清楚的,数据库访问拉慢了代码,导致最后代码出发PHP设置的运行时间限制.现在问题变为了如何优化这一段代码加快速度.

### 索引
同事首先想到原来的数据表是没有索引的,这时候可以考虑在数据表上添加索引加速访问.关于索引的内容可以参考我之间关于索引的文章,在这里由于我不想贴出原来的数据表,所以就略过这一个环节.总之添加索引之后代码总算是能跑了,数据能够下载下来.虽然依旧很慢,但是总算没有断掉.可喜可贺.可喜可贺

### 代码分析与修改

我因为本来的工作因为意外空了出来,所以过来协助,修改代码.因此我开始进一步分析,这一块代码变慢的原因.很明显,这里有三层循环,每一层都会访问数据库,尤其是最后一层
```
$db->get('game_item_eroll', 'id', array('AND' => array('gameitem_id' => $v3, 'apply_id' => $vb['id'])));
...
$game_type = $db->get('gameitem', 'type', array('AND' => array('id' => $v3)));
```
这是本篇文章的核心,也是文眼.我们在平时写代码的过程中要避免在循环体内部访问数据库,虽然我们经常说MySQL的每一次连接都很轻,但是也扛不住.本次访问中通过打断点,我得知,在正式环境下,仅仅这个get语句就要访问MySQL,在正式环境需要大几万次的访问(这也是估测),如此庞大的访问量,必然会大大拉慢数据库.因此减少数据库访问是本次优化的核心.<br>
首先我们看这一句,每一次二重循环都会访问
```
$item_id_array = $db->select('gameitem', 'id', array('game_id' => $_GET['gid']));
```
很显然我们可以吧代码拖到外面,使用变量访问,这样我们减少一小部分的访问.
之后我们把一二重循环的访问合并,组成一个连接访问.

```
$item_id_array = $db->select('gameitem', 'id', array('game_id' => $_GET['gid']));
if (!empty($item_id_array)) {
    ...

    $teamappl = $db->query('select tb_team_apply.name, code, nation, sex  from  tb_team_apply left join game_team on tb_team_apply.game_team_id = game_team.id where type=1;')>fetchAll();;


    if ($teamappl != NULL) {
        foreach ($teamappl as $k1 => $vb) {

                foreach ($item_id_array as $k3 => $v3) {

                    $ifitemname = $db->get('game_item_eroll', 'id', array('AND' => array('gameitem_id' => $v3, 'apply_id' => $vb['id'])));
                    ...
                    $game_type = $db->get('gameitem', 'type', array('AND' => array('id' => $v3)));
                    if (!empty($game_type) && $game_type == 2) {
                    ...
                    }


            }
            $content[] = $tmp;
            unset($tmp);
        }

    }
}
```
优化到这里,访问数据库的数量级已经降低到可以容忍的地步(其实只是降低了两个数量级),当然我们还可以进一步优化,比如将left join 的代码通过explain优化,将$game_type的方法提取到外部.甚至是直接吧最里面那一层访问提取出来(这也是最困难的一步,也是治本的一步,我在另一个项目中首次尝试吧循环内部的数据防访问通过连接的方式提取到外部,大大提高访问的速度).实际上的核心实在访问交给了数据库引擎,让引擎使用索引和连接等各类优化技术.

### 最后的办法
这时候同事已经决定放弃修改这个代码,通过添加一个额外的统计表,通过定时脚本的方式,存入数据,再从这个表读取统计数据,这个思路总体算是一个治本的方法.在<高性能MySQL>也推荐过这样的办法,通过统计表来提升某一类的统计的效率
