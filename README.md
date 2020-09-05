## 一些 Rust/Go 文章翻译

## 目录
### 2020
* 2020.6th [【译】我的阅读习惯](src/2020/6.My-Reading-Habits.md)
* 2020.5th [【译】数据库基础：用 Go 从零开始写一个 SQL 数据库 —— 第二部分](src/2020/5.Database-basics:-writing-a-SQL-database-from-scratch-in-Go-part2.md)
* 2020.4th [【译】用`Wasmer`进行插件开发 第二部分](src/using_wasmer_for_plugins/part2.md)
* 2020.3th [【译】Rust 中的状态机](src/2020/3.state-machines.md)
* 2020.2th [【译】数据库基础：用 Go 从零开始写一个 SQL 数据库 —— 第一部分](src/2020/2.Database-basics:-writing-a-SQL-database-from-scratch-in-Go-part1.md)

### 2019
* [【译】Rust 的 Result 类型入门](src/2019/a-primer-on-rusts-result-type.md)
* [【译】从 Rust 到不只是 Rust：PHP 语言领域](src/2019/From-Rust-to-beyond-The-PHP-galaxy.md)
* [【译】写好 CLI 程序](src/2019/Write-a-Good-CLI-Program.md)
* [【译】使用 Rust 和 WebAssembly 构建离线画图页面](src/2019/Create-Dev_s-offline-page-with-Rust-and-WebAssembly.md)
* [【译】通过 Rust 强化 sed](src/2019/racing-sed-with-rust.md)
* [【译】使用 Rust 构建你自己的 Shell](src/2019/Build_Your_Own_Shell_using_Rust.md)
* [【译】理解二进制（1）](src/2019/Understanding_Binary_Pt_1.md)
* [【译】学习写解析器组合器](src/2019/Learning-Parser-Combinators-With-Rust.md)
* [【译】用`Wasmer`进行插件开发 第一部分](src/using_wasmer_for_plugins)

## plan
- [https://blog.yoshuawuyts.com/state-machines/](https://blog.yoshuawuyts.com/state-machines/)

## 规范
### 文件名
* 文章的名称是英文的，并且特殊字符使用 `-` 替代，可以使用正则 `(:\s+|\ +)`，将特殊字符替换为 `-`

## 一些文档翻译
### PHP7 内核 [PHP Internals Book](./src/PHP-Internals-Book)
- 目前这本 PHP 内核已经被社区发起翻译计划了，基本上翻译完成，[点此查看](https://learnku.com/docs/php-internals/php7)

#### 翻译招募
* 内容篇幅多，希望有志之士一同参与翻译。有意向请提 issue 告知。谢谢！

#### 进度
* internal_types
    * zvals
        * [basic_structure](./src/PHP-Internals-Book/php7/internal_types/zvals/basic_structure.md)
    * strings
        * [zend_string](./src/PHP-Internals-Book/php7/internal_types/strings/zend_strings.md)

### [PHP-Parser-doc](./src/PHP-Parser-doc)

## 其他
* [PHP 编译器项目](https://github.com/ircmaxell/php-compiler)，可将 PHP 代码编译成可执行的二进制文件

## 参考
* https://github.com/nikic/PHP-Parser
* https://blog.ircmaxell.com/2019/04/compilers-ffi.html
