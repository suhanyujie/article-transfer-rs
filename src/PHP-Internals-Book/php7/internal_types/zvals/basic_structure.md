# 【译】PHP 内核 — zval 基础结构

>* 原文地址：http://www.phpinternalsbook.com/php7/internal_types/zvals/basic_structure.html
>* 原文仓库：https://github.com/phpinternalsbook/PHP-Internals-Book
>* 原文作者：[phpinternalsbook](https://github.com/phpinternalsbook/PHP-Internals-Book)
>* 译文出自：https://github.com/suhanyujie
>* 本文永久链接：https://github.com/suhanyujie/article-transfer-rs/blob/master/src/PHP-Internals-Book/php7/internal_types/zvals/basic_structure.md
>* 译者：[suhanyujie](https://github.com/suhanyujie)
>* 翻译不当之处，还请指出，谢谢！

# 基础结构
一个 zval（“Zend value”的简写）可以表示 PHP 中任意类型的值。因此，它可能是 PHP 中最重要的结构，你会经常使用它。本节描述 zval 背后的基本概念以及用法。

## 类型和值
除此之外，每个 zval 都存储了一些值和该值的类型信息。这是必要的，因为 PHP 是一种动态类型的语言，而且只有在运行时才知道变量的类型，而非编译时。此外，类型可以在 zval 的生命周期中更改，因此，如果 zval 之前存储了一个整数，那么它可能在稍后的时间里就变成一个存储字符串的变量。

类型存储为整数标记（无符号整数）。它可以是几个值中的一个。有些值对应于 PHP 中可用的八种类型，其他值仅用于内部引擎的使用。使用 IS_TYPE 形式的常量引用这些值。例如 IS_NULL 对应于 null 类型，IS_STRING 对应于 string（字符串）类型。

实际值存储在 union 中，它的定义如下：

```c
typedef union _zend_value {
    zend_long         lval;
    double            dval;
    zend_refcounted  *counted;
    zend_string      *str;
    zend_array       *arr;
    zend_object      *obj;
    zend_resource    *res;
    zend_reference   *ref;
    zend_ast_ref     *ast;
    zval             *zv;
    void             *ptr;
    zend_class_entry *ce;
    zend_function    *func;
    struct {
        uint32_t w1;
        uint32_t w2;
    } ww;
} zend_value;
```

对于那些不熟悉 union 的人来说：union定义了不同类型的多个成员，但是一次只能使用个其中的一个。例如：如果 value.lval 被存储了，那么你就需要使用 value.lval 来查找值而不是其他的成员（这样做将违反“strict aliasing”保证，并会导致未定义的行为）。原因是 union（联合体）将所有的成员存储在相同的内存位置，并根据访问的成员的不同，对位于该位置的值进行不同的解析。一个 union 类型的大小是其最大成员的大小。

当使用 zval 时，类型标记用于查明哪个 union 成员当前正在使用。在了解用于此目的的 api 之前，我们需要先看看 PHP 支持的不同类型以及它们是如何存储的：

最简单的类型是 IS_NULL：实际上它不需要存储任何值，因为只有一个 null 值。

为了存储数字，PHP 提供了 IS_LONG 和 IS_DOUBLE 类型，它们分别使用 zend_long lval 和 double dval 成员。前者用于存储整数，后者用于存储双精度浮点数。

关于 zend_long 类型，有一些事情需要注意：首先，这是一个带符号的整数类型，即它可以存储正整数和负整数，但是通常不适合执行位操作。其次，zend_long 表示操作系统的 long 类型，无论你使用什么样的系统，zend_long 在 32 位操作系统上是 4 字节，而在 64 位操作系统上大小是 8 个字节。 

此外，你可以使用与 long、SIZEOF_ZEND_LONG 或者 ZEND_LONG_MAX 等相关的宏。详情可参考源码中 Zend/zend_long.h 文件。

用于存储浮点数的双精度类型通常是遵循 IEEE-754 规范中提到的 8 字节。这里不讨论这种格式的细节，但是你应该至少知道这样一个事实：这种类型的精度有限，并且通常无法存储你想要的准确值。

布尔值使用 `IS_TRUE` 或 `IS_FALSE` 表示，不需要存储任何信息。存在一个标记为 `_IS_BOOL` 的伪类型中，但你不应该把它作为 zval 使用，这是不正确的。这种“假类型”用于一些非常少见的内部情况（例如类型推断等）。

剩下的四种类型将会快速提一下，详细的讨论会在它们各自的章节中分析：

字符串（`IS_STRING`）存储在 `zend_string` 结构中，它们由 `char *` 字符数组和 `size_t` 的长度组成。你将在 [zend_string 结构章节](http://www.phpinternalsbook.com/php7/internal_types/strings.html)中可以找到关于 `zend_string` 结构以及专用 API 等信息。

数组使用 `IS_ARRAY` 类型标记，并存储在 `zend_array *arr` 成员中。哈希表结构如何工作将在哈希表一章中进行讨论。

对象（`IS_OBJECT`）使用 zend_object *obj member 成员。将会在[对象章节](http://www.phpinternalsbook.com/php7/internal_types/objects.html)详细描述PHP 类和对象系统

Resources (IS_RESOURCE) are a special type using the zend_resource *res member. Resources are covered in the Resources chapter.
资源（IS_RESOURCE）使用 `zend_resource *res member` 的特殊成员。详细资料参考[资源章节](http://www.phpinternalsbook.com/php7/internal_types/zend_resources.html)

总而言之，这里有一个表，其中包含所有可用的类型标签以及值对应的存储位置：
| 类型        | 存储位置   | 
| :--------   | :----- |
| `IS_NULL` | none |
| `IS_TRUE` 或 `IS_FALSE` | none |
| `IS_LONG` | `zend_long lval` |
| `IS_DOUBLE` | `double dval` |
| `IS_STRING` | `zend_string *str` |
| `IS_ARRAY` | `zend_array *arr` |
| `IS_OBJECT` | `zend_object *obj` |
| `IS_RESOURCE` | `zend_resource *res` |

## 特殊类型
你可能会在 zval 中看到其它类型，那些类型我们还没回顾。这些类型是不存在与 PHP 用户态的相关类型中，仅用于引擎内部的使用。zval 结构一直被认为是非常灵活的，它在内部用于携带几乎所有能用到的信息数据，而不仅限于我们刚刚讨论过的 PHP 特定类型。

The special IS_UNDEF type has a special meaning. That means “This zval contains no data of interest, do not access any data field from it”. This is used for memory management purposes. If you see an IS_UNDEF zval, that means that it is of no special type and contains no valid information.
特殊的 IS_UNDEF 类型具有特殊的含义。这意味着“这个 zval 不包含有用数据，不要通过它访问真实的数据字段”。这是出于[内存管理](http://www.phpinternalsbook.com/php7/internal_types/zvals/memory_and_gc.html)的目的。如果你看到一个 IS_UNDEF 的 zval，这意味着它没有特殊类型，也不包含有效信息。

The zend_refcounted *counted field is very tricky to understand. Basically, that field serve as a header for any other reference-countable type. This part is detailed into the Zval memory management and garbage collection chapter.
`zend_refcounted *counted` 字段非常难以理解。基本上，该字段用作任何其它可饮用计数类型的主体部分。本部分将详细介绍 [Zval 内存管理和垃圾回收](http://www.phpinternalsbook.com/php7/internal_types/zvals/memory_and_gc.html)。

The zend_reference *ref is used to represent a PHP reference. The IS_REFERENCE type flag is then used. Here as well, we dedicated a chapter to such a concept, have a look at the Zval memory management and garbage collection chapter.
`zend_reference *ref` 用于表示 PHP 的引用。通过使用 `IS_REFERENCE` 类型标识符。在这里，我们也专门用一章来介绍这个概念，看看 [Zval 内存管理和垃圾回收](http://www.phpinternalsbook.com/php7/internal_types/zvals/memory_and_gc.html)章节。

当你从编译器操作 AST，使用 `zend_ast_ref *ast`。PHP 编译会在 [Zend 编译器](http://www.phpinternalsbook.com/php7/zend_engine/zend_compiler.html)一章中详细介绍。

`zval *zv` 只在内部使用。通常你应该操作它。它跟 IS_INDIRECT 一起使用，并且这允许将 `zval *` 嵌入到 zval 中。这种字段的特殊暗用法，还是有的，例如 PHP 中的 `$GLOBALS[]` 超全局变量。

有些类似于 `void *ptr` 是非常有用的字段。它也是一样的：不在 PHP 用户空间使用，只在内部使用。当你想要将某些东西存储到 zval 中时，你基本上会使用这个字段。是的，这是一个 `void *`，它在 C 语言中表示指向任何大小的内存区域的指针，包含（希望如此）任何内容。然后在 zval 中使用 IS_PTR 标识。

在你阅读[对象章节](http://www.phpinternalsbook.com/php7/internal_types/objects.html)时，你将了解到 `zend_class_entry` 类型。 `zval zend_class_entry *ce` 字段，它用于将 PHP 中类的引用携带到对应的 zval 中。在这里，PHP 语言本身（用户空间）并没有直接使用，但是在内部，就需要用它。

最后，使用 `zend_function *func field` 字段将 PHP 函数嵌入到 zval 中。[函数一章](http://www.phpinternalsbook.com/php7/internal_types/functions.html)详细介绍了 PHP 函数

## 访问宏
我们看看 zval 结构体实际是什么样子：

```c
struct _zval_struct {
        zend_value        value;                    /* value */
        union {
                struct {
                        ZEND_ENDIAN_LOHI_4(
                                zend_uchar    type,                 /* active type */
                                zend_uchar    type_flags,
                                zend_uchar    const_flags,
                                zend_uchar    reserved)         /* call info for EX(This) */
                } v;
                uint32_t type_info;
        } u1;
        union {
                uint32_t     next;                 /* hash collision chain */
                uint32_t     cache_slot;           /* literal cache slot */
                uint32_t     lineno;               /* line number (for ast nodes) */
                uint32_t     num_args;             /* arguments number for EX(This) */
                uint32_t     fe_pos;               /* foreach position */
                uint32_t     fe_iter_idx;          /* foreach iterator index */
                uint32_t     access_flags;         /* class constant access flags */
                uint32_t     property_guard;       /* single property guard */
                uint32_t     extra;                /* not further specified */
        } u2;
};
```

如前所述，zval 有成员来存储值以及对应的 type_info。值存储在上面讨论的 zvalue_value 联合体中，类型标记保存在上面讨论的 u1 的联合体中并且类型 tag 被保存在 u1 联合体中之一的 `zend_uchar`。此外，改结构有一个 u2 属性。我们暂时忽略它，稍后再讨论它们的功能。

使用 type_info 访问 u1。type_info 可以具体化为详细的 type，type_flags，const_flags 和 保留字段。记住，我们讨论的 u1 的联合体。`u1.v` 字段中的四个信息的权重与存储在 `u1.type_info` 中的信息是一样的。这里使用了一个巧妙的内存对其方式。u1 非常常用，因为它将有关的类型信息存储在 zval 中。

u2 有完全不同的含义。我们现在不需要详细描述 u2 字段，暂且忽略它，稍后再讨论。

了解 zval 结构，你现在可以使用它编写代码：

```c
zval zv_ptr = /* ... 从某个地方获取 zval */;

if (zv_ptr->type == IS_LONG) {
    php_printf("Zval is a long with value %ld\n", zv_ptr->value.lval);
} else /* ... handle other types */
```

虽然上面的代码可以工作，但这不是编写 zval 的惯用方式。它直接访问 zval 成员，而不是使用宏：

```c
zval *zv_ptr = /* ... */;

if (Z_TYPE_P(zv_ptr) == IS_LONG) {
    php_printf("Zval is a long with value %ld\n", Z_LVAL_P(zv_ptr));
} else /* ... */
```

上面的代码使用 `Z_TYPE_P()` 宏检索类型标记，使用 `Z_LVAL_P()` 获取 long (integer) 类型的值。所有访问宏都有 `_P` 后缀或没有后缀。你使用哪种取决于你是处理 `zval` 还是 `zval*`。

```c
zval zv;
zval *zv_ptr;
zval **zv_ptr_ptr; /* 这种指针的指针，比较少见 */

Z_TYPE(zv);                 // = zv.type
Z_TYPE_P(zv_ptr);           // = zv_ptr->type
```

基本上 P 代表“指针”。这只对 zval* 有效，即没有用于处理 zval** 或者类似这种的特殊宏，因为在实际的实践中很少需要这样做（你只需首先使用 `*` 操作符进行解引用）。

与 Z_LVAL 类似，还有用于获取所有其它类型值的宏。为了演示它们的用法，我们将创建一个简单的函数来打印 zval 信息：

```c
PHP_FUNCTION(dump)
{
    zval *zv_ptr;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &zv_ptr) == FAILURE) {
        return;
    }

    switch (Z_TYPE_P(zv_ptr)) {
        case IS_NULL:
            php_printf("NULL: null\n");
            break;
        case IS_TRUE:
            php_printf("BOOL: true\n");
            break;
        case IS_FALSE:
            php_printf("BOOL: false\n");
            break;
        case IS_LONG:
            php_printf("LONG: %ld\n", Z_LVAL_P(zv_ptr));
            break;
        case IS_DOUBLE:
            php_printf("DOUBLE: %g\n", Z_DVAL_P(zv_ptr));
            break;
        case IS_STRING:
            php_printf("STRING: value=\"");
            PHPWRITE(Z_STRVAL_P(zv_ptr), Z_STRLEN_P(zv_ptr));
            php_printf("\", length=%zd\n", Z_STRLEN_P(zv_ptr));
            break;
        case IS_RESOURCE:
            php_printf("RESOURCE: id=%d\n", Z_RES_HANDLE_P(zv_ptr));
            break;
        case IS_ARRAY:
            php_printf("ARRAY: hashtable=%p\n", Z_ARRVAL_P(zv_ptr));
            break;
        case IS_OBJECT:
            php_printf("OBJECT: object=%p\n", Z_OBJ_P(zv_ptr));
            break;
    }
}

const zend_function_entry funcs[] = {
    PHP_FE(dump, NULL)
    PHP_FE_END
};
```

试试运行它：

```bash
dump(null);                 // NULL: null
dump(true);                 // BOOL: true
dump(false);                // BOOL: false
dump(42);                   // LONG: 42
dump(4.2);                  // DOUBLE: 4.2
dump("foo");                // STRING: value="foo", length=3
dump(fopen(__FILE__, "r")); // RESOURCE: id=???
dump(array(1, 2, 3));       // ARRAY: hashtable=0x???
dump(new stdClass);         // OBJECT: object=0x???
```

用于访问值的宏非常的简洁：用于 long 的 `Z_LVAL`，用于 double 的 `Z_DVAL`。对于字符串使用 `Z_STR` 将返回 `zend_string * string`，如果使用 `ZSTR_VAL` 将访问到其中的 `char *`，而 `Z_STRLEN` 提供给我们访问其长度的功能。可以使用 `Z_RES_HANDLE` 获取资源类型对应的资源 ID，使用 `Z_ARRVAL` 访问数组的 `zend_array *`。

当你想要访问 zval 的内容时，应该始终使用这些宏，而不是直接访问它的成员。这里维护了一层对应的抽象，并使意图更清晰。在将来的 PHP 版本中，使用宏还可以防止对内部 zval 的一些更改造成的一些问题。

## 设定值
上面介绍的大多数宏是访问 zval 结构的某个成员，因此同样的，可以使用对应的宏来读取和写入各自的值。例如，思考下面的函数，它只返回字符串 “hello world!”：

```c
PHP_FUNCTION(hello_world) {
    Z_TYPE_P(return_value) = IS_STRING;
    Z_STR_P(return_value) = zend_string_init("hello world!", strlen("hello world!"), 0);
};

/* ... */
    PHP_FE(hello_world, NULL)
/* ... */
```

运行 `php -r "echo hello_world();"`，现在应该在终端打印出 `hello world` 了。

在上面的例子中，我们设置了 `return_value` 变量，它是 `PHP_FUNCTION` 宏提供的 `zval*` 变量。我们将在下一张更详细的研究这个变量，因为现在只要知道这个变量的值是函数的返回值就够了。默认情况下，它会被初始化为 `IS_NULL` 类型

使用访问宏设置 zval 的值很简单，但是有一些事项需要注意：首先，你需要记住类型标记决定了 zval 的类型。仅仅设置值还不够（通过 `Z_STR_P`），我们还需要设置类型标记。

此外，你需要知道，在大多数情况下，zval “拥有”自己的值，并且 zval 的生命周期将比你设置的值的生命周期更长。有时这并不适用于处理临时的 zval，但在大多数情况下是正确的。

使用上面的例子，这意味着 `return_value` 将在函数体离开之后继续存在（这是非常明显的，否则函数调用后没有人可以使用返回值），所以它不能使用（引用）函数的任何临时变量。

因此我们需要使用 zend_string_init() 创建一个新的 zend_string。这样将会在堆上创建一个单独的字符串副本。因为 zval “携带”它的值，它将确保在 zval 被销毁时释放这个副本，或者至少减少它的 refcount（引用计数）。这也适用于 zval 的任何其他“复杂”值。例如，如果你为一个数组设置 zend_array*，那 zval 稍后将携带该值，并在 zval 被销毁时释放它。所谓“释放”，指的要么是减少引用计数，要么在引用计数为零是释放内存空间。当使用整数或者双精度等基本类型时，你显然不需要关心这些，因为它们总是被复制的（以值拷贝的方式进行一些操作）。所有这些内存管理步骤，如内存分配、释放或引用计数；详细信息请参阅 [Zval 内存管理和垃圾回收](http://www.phpinternalsbook.com/php7/internal_types/zvals/memory_and_gc.html)一章。

设置 zval 值是很常见的操作，PHP 为此提供了另一组宏。使用它们可以设置类型标记以及对应的值。使用这样的宏的好处前面已经提过了：

```c
ZVAL_NULL(return_value);

ZVAL_FALSE(return_value);
ZVAL_TRUE(return_value);

ZVAL_LONG(return_value, 42);
ZVAL_DOUBLE(return_value, 4.2);
ZVAL_RES(return_value, zend_resource *);

ZVAL_EMPTY_STRING(return_value);
/* 一个特殊的方法管理空字符串 */

ZVAL_STRING(return_value, "string");
/* = ZVAL_NEW_STR(z, zend_string_init("string", strlen("string"), 0)); */

ZVAL_STRINGL(return_value, "nul\0string", 10);
/* = ZVAL_NEW_STR(z, zend_string_init("nul\0string", 10, 0)); */
```

注意，这些设定值的宏，不会自动地销毁 zval 之前持有的值。对于 return_value 的 zval，是没关系的，因为它被初始化为 IS_NULL（IS_NULL 没有需要释放的值），但是其他情况下，你需要先使用下面小节中描述的使用函数效果旧的值。
