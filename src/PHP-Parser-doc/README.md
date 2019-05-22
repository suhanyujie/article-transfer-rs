# 【译】目录

>* 原文地址：https://github.com/nikic/PHP-Parser/blob/master/doc/README.md
>* 原文作者：[Josh Mcguigan](https://github.com/nikic/PHP-Parser)
>* 译文出自：https://github.com/suhanyujie
>* 本文永久链接： 
>* 译者：[suhanyujie](https://github.com/suhanyujie)

> 正文开始

## 导航
* 说明
* 基础组件的使用

## 组件文档
* 了解 AST
    * 节点访问
    * 修改 AST 中的节点
    * 短路遍历
    * 交叉访问
    * 简单的查找节点 API
    * 父级引用和兄弟节点引用

* 名称解析
    * 名称解析器设置
    * 名称解析上下文

* 美化输出
    * 将 AST 转换回 PHP 代码
    * 自定义格式化
    * 代码转换后的格式化保存

* 构建 AST
    * 构建 AST 节点流

* 词法分析器
    * 词法分析器设置
    * 节点的 Token 和文件位置
    * 自定义属性

* 错误处理
    * 错误的字段信息
    * 错误恢复（错误代码的语法解析）

* 常量表达式的求值
    * 常量、属性等值的初始化
    * 处理错误和未知表达式

* JSON 表达式
    * JSON 编码和解码的 AST

* 性能
    * 禁用 XDebug
    * 对象重用
    * GC 的影响

* 常见问题
    * 父节点和兄弟节点引用
