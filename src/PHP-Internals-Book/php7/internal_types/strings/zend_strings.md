# 【译】PHP 内核 — 字符串管理
>* （Strings management: zend_string 译文）

>* 原文地址：http://www.phpinternalsbook.com/php7/internal_types/strings/zend_strings.html
>* 原文仓库：https://github.com/phpinternalsbook/PHP-Internals-Book
>* 原文作者：[phpinternalsbook](https://github.com/phpinternalsbook/PHP-Internals-Book)
>* 译文出自：https://github.com/suhanyujie
>* 本文永久链接：
>* 译者：[suhanyujie](https://github.com/suhanyujie)
>* 翻译不当之处，还请指出，谢谢！

# 字符串管理：zend_string
任何程序都需要管理字符串。在这里，我们将详细介绍一个适合 PHP 需要的定制解决方案：`zend_string`。每次 PHP 需要处理字符串时，都会使用 zend_string 结构体。这个结构体是 C 语言的 `char *` 类型包装后的结果。

它增加了内存管理功能，这样的话，同一个字符串可以在多个地方共享，而不需要复制它。另外，有些字符串是 “interned” 的，它们被持久化的分配内存并由内存管理器进行特殊管理的，这样可以实现在多个请求之间进行复用。那些字符串在后续会被 [Zend 内存管理器](http://www.phpinternalsbook.com/php7/memory_management/zend_memory_manager.html)持久化分配。

## 结构体和访问宏
这里简单地展示 `zend_string` 结构体：

```c
struct _zend_string {
        zend_refcounted_h gc;
        zend_ulong        h;
        size_t            len;
        char              val[1];
};
```

如你看到的，这个结构体嵌入了一个 `zend_refcounted_h` 头。这样做是为了内存管理和引用计数。由于字符串很可能用作哈希表探测的键，所以它将哈希值嵌入为 `h` 成员。这是一个无符号的 `zend_ulong`。这个数字只在 `zend_string` 需要计算哈希时使用，特别是跟 [HashTables: zend_array](http://www.phpinternalsbook.com/php7/internal_types/hashtables.html) 一起使用时；这是非常有可能的。

正如你知道的那样，字符串知道它自己的长度是 `len` 字段值，通过它可以支持“二进制字符串”。二进制字符串是一个字符串中包含了一个或多个 `NUL` 字符（\0）。当它传递给 libc 函数时，这些字符串会被截断，或者它们的长度不会以正确的方式计算。那么，在 `zend_string` 中，字符串的长度是已知的。请注意，长度的计算是计算 ASCII 字符，不包含结束时的 `NUL`，但包含中间的若干 `NUL`。例如，字符串 “foo” 在 `zend_string` 中存储为 “foo\0”，其长度为 3。此外，字符串 “foo\0bar” 将存储为 “foo\0bar\0”，长度是 7。

最后，字符存储到 `char[1]` 字段中。它不是 `char *`，而是 `char[1]`。为什么呢？这是一种内存优化，叫做 “C struct hack”（你可以去搜索一下这个术语）。基本上，它允许 zend 引擎为 `zend_string` 结构体类型以及要存储的字符分配内存空间时，只需用一个 C 指针。这样可以优化内存访问，因为此处分配的内存是一个连续的块，而非在内存中分配两个块（一个用于 `zend_string *`，一个用于 `char *`）。

一定要记住这个 struct hack，因为 C 中 `zend_string` 结构体的内存布局看起来和 C 中 char 是很相似的，在使用 C 调试器调试（调试字符串）时可以感觉到/看到是这样。这种 hack 可以让你通过对应的 API 操作完全地管理 `zend_string` 结构体。

![](./images/zend_string_memory_layout.png)

## 使用 zend_string API
### 简单的使用场景
就像使用 [Zvals](http://www.phpinternalsbook.com/php7/internal_types/zvals.html) 一样，你不需要手动操作 `zend_string` 内部的字段，而是使用宏来实现这一点。还存在一些宏来触发字符串上的操作。这些不是函数，而是宏，都被定义在 [Zend/zend_string.h](https://github.com/php/php-src/blob/PHP-7.0/Zend/zend_string.h) 头文件中：

```c
zend_string *str;

str = zend_string_init("foo", strlen("foo"), 0);
php_printf("This is my string: %s\n", ZSTR_VAL(str));
php_printf("It is %zd char long\n", ZSTR_LEN(str));

zend_string_release(str);
```

上面的简单示例展示了基本的字符串管理。函数 `zend_string_init()`（它实际上是一个宏，但是我们可以传递详细的参数）应该返回给你一个类似于 `char *` 的 C 字符串类型，以及它的长度。最后一个参数类型是 int —— 值应该是 0  或者 1。如果传递 0，则要求引擎使用 Zend 内存管理器使用“请求绑定”的堆分配方式分配内存。这种分配将在当前请求结束时销毁。如果你不销毁，在调试构建时，引擎将会告诉你：你刚刚创建的内存泄露了。如果你使用 1，那么你将会使用我们所说的“持久化”分配，即引擎将使用传统的 C 的 `malloc()` 调用，并且不会跟踪内存的分配。

>* 如果你需要知道更多关于内存管理的信息，可以阅读[详细章节](http://www.phpinternalsbook.com/php7/memory_management.html)

然后，我们要显示字符串。我们使用 `ZSTR_VAL()` 宏访问字符数组。`ZSTR_LEN()` 允许访问字符串长度信息。`zend_string` 相关的宏都是以 `ZSTR_**()` 开头的宏，注意这跟 `Z_STR**()` 宏可不一样！

>* 长度使用 `size_t` 类型存储。因此，要显示它，可以使用 `printf()` 配合 “%zd” 格式。如果不这样的话，可能会导致应用程序崩溃或者产生安全问题。有关 `printf()` 格式化打印的详细介绍，可以访问[此链接](http://www.cplusplus.com/reference/cstdio/printf/)

最后，我们使用 `zend_string_release()` 来释放字符串。这个版本是强制性的。这是关于内存管理的部分。“释放”是一个简单的操作：减少字符串的引用计数，如果它是零，API 将为你释放字符串的内存。如果忘记释放字符串，很可能造成内存泄露。 

>* 你必须始终考虑用 C 进行内存管理。如果你进行了内存分配 —— 无论是直接使用了 `malloc()`，还是使用一个 API 进行分配 —— 你都必须在一定的时候使用 `free()`。如果做不到这一点，就会造成内存泄露，这样的程序就是一个不能被安全地使用的糟糕程序。

### 使用 hash
如果需要访问字符串的哈希值，请使用 `ZSTR_H()`。但是，当你创建 `zend_string` 时，不会自动计算好对应的哈希值。当该字符串跟 HashTable API 一起使用时，哈希值才会计算。如果你想强制计算哈希值，请使用 `ZSTR_HASH()` 或者 `zend_string_hash_val()`。一旦哈希值被计算出来，它就会保存起来，并且再也不会计算了。如果因为某种原因，你需要重新计算 —— 例如你改变了字符串的值，这时候可以使用 `zend_string_forget_hash_val()` 重新计算哈希值：

```c
zend_string *str;

str = zend_string_init("foo", strlen("foo"), 0);
php_printf("This is my string: %s\n", ZSTR_VAL(str));
php_printf("It is %zd char long\n", ZSTR_LEN(str));

zend_string_hash_val(str);
php_printf("The string hash is %lu\n", ZSTR_H(str));

zend_string_forget_hash_val(str);
php_printf("The string hash is now cleared back to 0!");

zend_string_release(str);
```

### 字符串拷贝和内存管理
`zend_string` API 有一个非常好的特性是，它允许通过一小部分的简单声明来“拥有”一个字符串。然后引擎将不会在内存中拷贝字符串，而是简单的增加引用计数（`zend_refcounted_h` 的一部分）。这允许在代码的很多地方共享一块内存。

这样每当我们谈到“拷贝”一个 `zend_string` 时，实际上可能我们不会复制内存中的任何内容。如果需要 —— 确实可以这样做 —— 接下来我们谈谈字符串的拷贝。开始：

```c
zend_string *foo, *bar, *bar2, *baz;

foo = zend_string_init("foo", strlen("foo"), 0); /* creates the "foo" string in foo */
bar = zend_string_init("bar", strlen("bar"), 0); /* creates the "bar" string in bar */

/* 创建 bar2 并将 bar 中的 "bar" 字符串 共享给 bar2
而且增加 "bar" 字符串的引用计数为 2*/
bar2 = zend_string_copy(bar);

php_printf("We just copied two strings\n");
php_printf("See : bar content : %s, bar2 content : %s\n", ZSTR_VAL(bar), ZSTR_VAL(bar2));

/* 在内存中复制 "bar" 字符串，创建了 baz 变量并使它是一个独立的 "bar" 字符串 */
baz = zend_string_dup(bar, 0);

php_printf("We just duplicated 'bar' in 'baz'\n");
php_printf("Now we are free to change 'baz' without fearing to change 'bar'\n");

/* 更改第二个 "bar" 字符串的最后一个字符，变成 "baz" */
ZSTR_VAL(baz)[ZSTR_LEN(baz) - 1] = 'z';

/* 当字符串值被改变，丢弃旧值的 hash（如果已经计算了），这样需要重新计算它的 hash */
zend_string_forget_hash_val(baz);

php_printf("'baz' content is now %s\n", ZSTR_VAL(baz));

zend_string_release(foo);  /* destroys (frees) the "foo" string */
zend_string_release(bar);  /* decrements the refcount of the "bar" string to one */
zend_string_release(bar2); /* destroys (frees) the "bar" string both in bar and bar2 vars */
zend_string_release(baz);  /* destroys (frees) the "baz" string */
```

我们从分配 “foo” 和 “bar” 开始。然后我们创建了 `bar2` 字符串作为 `bar` 的副本。在这里，我们都需要记住：`bar` 和 `bar2` 指向相同的内存区域中的 C 字符串，更改一个也将会更改另一个。这是 `zend_string_copy()` 的行为：它只是增加了所拥有的 C 字符串的引用计数。

如果我们想分离开这个字符串 —— 也就是说我们想在内存中有两个独立的字符串副本 —— 我们需要使用 `zend_string_dup()` 拷贝。然后我们将 `bar2` 变量字符串复制到 `baz` 变量中。现在，`baz` 变量嵌入了它自己的字符串副本，并且可以在不影响 `bar2` 的情况下更改它。这就是我们要做的：我们把 ‘bar’ 中的最后一个 ‘r’ 换成 ‘z’。然后我们显示它，并释放所有字符串的内存。

注意，我们忘记了哈希值（如果之前计算过，就不需要考虑这个细节）。这是一个值得记住的好习惯。如之前所述，如果 `zend_string` 用作 HashTable 的一部分，则可以使用 hash。这是开发中非常常见的操作，更改字符串值也需要重新计算哈希值。忘记这样一个细节将会导致错误，可能就会需要花一些时间来跟踪排查了。

### 字符串的操作
`zend_string` API 还允许其他的操作，比如扩展或缩减字符串、更改字符串大小写或者比较字符串。目前还没有能用的连接操作，但是这也非常容易实现：

```c
zend_string *FOO, *bar, *foobar, *foo_lc;

FOO = zend_string_init("FOO", strlen("FOO"), 0);
bar = zend_string_init("bar", strlen("bar"), 0);

/* 和 C 字符串字面量比较 zend_string */
if (!zend_string_equals_literal(FOO, "foobar")) {
    foobar = zend_string_copy(FOO);

    /* realloc() 将 C 字符串分配到一个更大的 buffer 中 */
    foobar = zend_string_extend(foobar, strlen("foobar"), 0);

    /* 在重新分配了足够大的内存后连接 "bar" 和 "FOO"  */
    memcpy(ZSTR_VAL(foobar) + ZSTR_LEN(FOO), ZSTR_VAL(bar), ZSTR_LEN(bar));
}

php_printf("This is my new string: %s\n", ZSTR_VAL(foobar));

/* 比较两个 zend_string  */
if (!zend_string_equals(FOO, foobar)) {
    /* 复制一个字符串并将其转为小写 */
    foo_lc = zend_string_tolower(foo);
}

php_printf("This is FOO in lower-case: %s\n", ZSTR_VAL(foo_lc));

/* 释放内存 */
zend_string_release(FOO);
zend_string_release(bar);
zend_string_release(foobar);
zend_string_release(foo_lc);
```

### 通过 zval 访问 zend_string
现在你已经知道了如何管理和操作 `zend_string`，让我们看看他们与 zval 容器之间的交互。

>* 你需要熟悉 zval，如果不熟悉，请阅读 [Zvals](http://www.phpinternalsbook.com/php7/internal_types/zvals.html) 专用章节。

使用宏你将可以将 `zend_string` 存储到 `zval` 中，或从 `zval` 某种读取 `zend_string`：

```c
zval myval;
zend_string *hello, *world;

zend_string_init(hello, "hello", strlen("hello"), 0);

/* 将字符串存入 zval */
ZVAL_STR(&myval, hello);

/* 从 zval 中的 zend_string 读取 C 字符串 */
php_printf("The string is %s", Z_STRVAL(myval));

zend_string_init(world, "world", strlen("world"), 0);

/* 更改 myval 中的 zend_string：使用另一个 zend_string 替换它  */
Z_STR(myval) = world;

/* ... */
```

你必须记住，以 `ZSTR_***(s)` 开头的宏都是作用于 `zend_string` 的。

- ZSTR_VAL()
- ZSTR_LEN()
- ZSTR_HASH()
- …

所有以 `Z_STR**(z)` 开头的宏都是作用于嵌入 `zval` 中的 `zend_string`。

- Z_STRVAL()
- Z_STRLEN()
- Z_STRHASH()
- …

还有一些其他你可能用不上的宏。

### PHP 的历史和 C 中的典型的字符串
简单介绍一下经典的 C 字符串。在 C 语言中，字符串是字符数组（`char foo[]`）或者字符指针（`char *`）。它们的长度是不确定的，这就是它们会被 NUL 结束的原因（知道字符串的开头和结尾，就能知道它们的长度）。

在 PHP 7 之前，`zend_string` 结构体根本不存在。之前的传统是将 `char * / int` 成对使用。你可能还能找到一些 PHP 源码中还在使用 `char * / int` 而非 `zend_string`。你还可以找到 API 工具来将 `zend_string` 和 `char * / int` 相互转换。

只要有可能：就使用 `zend_string`。有些罕见的地方不使用 `zend_string` 是因为那个地方无所谓使用 `zend_string`，但是 PHP 源码中，你会发现很多地方都是对 `zend_string` 的引用。

### 内部的 zend_string
这里简单介绍一下 interned strings。在扩展开发中很少需要用到这样的概念。Interned 字符串也时常与 OPCache 扩展交互。

Interned 字符串是去重复的字符串。当与 OPCache 一起使用时，它们还会从一个请求重复使用到另一个请求中。

假设你想创建字符串 “foo”。你要做的只是简单地创建一个新的字符串 “foo”：

```c
zend_string *foo;
foo = zend_string_init("foo", strlen("foo"), 0);

/* ... */
```

但问题来了：在你需要使用它之前，那个字符串不是已经被创建了吗？当你需要一个字符串时，你代码在 PHP 生命周期中的某个时刻执行，这意味着在你之前的一些代码可能需要完全一样的字符串（也就是示例中的 “foo”）。

Interned 字符串是会要求引擎探测已经存储了的 interned 字符串，并重用已经分配的指针（如果它能找到你的字符串）。如果找不到：创建一个新的字符串并将其标识为 “interned” 字符串。这样它可用于 PHP 源码以外的场景（其他扩展，引擎本身，等等）

这里有一个示例：

```c
zend_string *foo;
foo = zend_string_init("foo", strlen("foo"), 0);

foo = zend_new_interned_string(foo);

php_printf("This string is interned : %s", ZSTR_VAL(foo));

zend_string_release(foo);
```

在上面的代码中，我们创建了一个新的非常经典的 `zend_string`。然后我们将创建的 `zend_string` 传递给 `zend_new_interned_string()`。这个函数在引擎的字符串缓冲区中查找相同的字符串片段（这里是 “foo”）。如果找到了它（意味着之前已经创建过这样的字符串），它就会释放你创建的字符串（可能是释放吧），并将其替换为来自 interned 字符缓冲区的字符串。如果它没有找到：就将 “foo” 添加到 interned 字符串缓冲区中，以便之后使用或者 PHP 的其它地方使用。

你必须注意一下这里的内存分配。Interned 字符串总是将 refcount 设为一，因为它们不需要被再次引用计数，因为它们将与 Interned 字符串缓冲区共享，因此它们不能被销毁。

示例:

```c
zend_string *foo, *foo2;

foo  = zend_string_init("foo", strlen("foo"), 0);
foo2 = zend_string_copy(foo); /* increments refcount of foo */

 /* refcount falls back to 1, even if the string is now
  * used at three different places */
foo = zend_new_interned_string(foo);

/* 当 foo 是 interned 字符串，这里什么也不会做 */
zend_string_release(foo);

/* 当 foo2 是 interned 字符串，这里什么也不会做 */
zend_string_release(foo2);

/* 进程结束后，PHP 将清楚 interned 字符串缓冲区，并且因此会对 "foo" 字符串进行 free()  */
```

接下来是关于垃圾回收的部分。

当字符串是 interned 时，它的 GC 标志将被修改为 `IS_STR_INTERNED` 标识，不管它们使用的是什么内存分配类（永久的还是基于请求的）。当你要复制或者释放字符串时，将探测这个标志位。如果字符串是 interned 类型的，引擎将不会增加这个字符串的引用计数。但如果你释放这个字符串，它不会减少引用计数也不会释放。它什么也不做。在进程的生命周期结束时，它才会销毁整个 interned 字符串缓冲区，也就释放了你的 interned 字符串。

如果 OPCache 被触发，这个过程实际上要更加复杂一些。OPCache 扩展改变了使用 interned 字符串的方式。没有 OPCache 时，如果你在请求的过程中创建了一个 interned zend_string，该字符串将在当前请求的结束时被清除，并且不会在下一个请求中重用。但是，如果你使用了 OPCache，则会将 interned 字符串存储在共享内存块中，并在共享同一个“池”的每个 PHP 进程之间，会共享此内存块。此外，在多个请求之间可以复用 interned 字符串。

使用 Interned 字符串会节省内存，因为同一个字符串在内存中的存储次数不会超过一次。但是，它可能会浪费一些 CPU 时间，因为它经常需要查找存储了的 interned 字符串，即使这个过程已经优化很多了。作为一个扩展设计者，以下是一些全局规则：

- 如果使用了 OPCache（应该使用），如果需要创建请求绑定的只读字符串：那就使用 interned 字符串吧。
- 如果你知道自己需要一个 interned 字符串（一种众所周知的 PHP 字符串，比如 “php” 或 “str_replace”），那就是使用 interned 字符串。
- 如果字符串不是只读的，并且可能/应该在创建后会更改，则不要使用 interned 字符串。
- 如果是将来不太可能复用的字符串，就不要使用 interned 字符串。

Interned 字符串详情可以参考[ Zend/zend_string.c](https://github.com/php/php-src/blob/PHP-7.0/Zend/zend_string.c)
