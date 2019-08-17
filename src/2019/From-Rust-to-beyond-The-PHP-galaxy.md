# 【译】从 Rust 到不只是 Rust：PHP 语言系列
>From Rust to beyond: The PHP galaxy 译文

>* 原文地址：https://mnt.io/2018/10/29/from-rust-to-beyond-the-php-galaxy/
>* 原文仓库：https://github.com/Hywan/gutenberg-parser-rs/tree/master/bindings/
>* 原文作者：[Ivan Enderlin](https://mnt.io/)
>* 译文出自：https://github.com/suhanyujie
>* 本文永久链接：（缺省）
>* 译者：[suhanyujie](https://github.com/suhanyujie)
>* 翻译不当之处，还请指出，谢谢！
>* tags：用 Rust 为 PHP 开发扩展；用 Rust 为 PHP 助力

This blog post is part of a series explaining how to send Rust beyond earth, into many different galaxies. Rust has visited:

- [Prelude](https://mnt.io/2018/08/21/from-rust-to-beyond-prelude/),
- [The WebAssembly galaxy](https://mnt.io/2018/08/22/from-rust-to-beyond-the-webassembly-galaxy/),
- [The ASM.js galaxy][https://mnt.io/2018/08/28/from-rust-to-beyond-the-asm-js-galaxy/],
- [The C galaxy][https://mnt.io/2018/09/11/from-rust-to-beyond-the-c-galaxy/],
- The PHP galaxy (this episode), and
- The NodeJS galaxy.

The galaxy we will explore today is the PHP galaxy. This post will explain what PHP is, how to compile any Rust program to C and then to a PHP native extension.

## What is PHP, and why?
[PHP](https://secure.php.net/) is a:

>popular general-purpose scripting language that is especially suited to Web development. Fast, flexible, and pragmatic, PHP powers everything from your blog to the most popular websites in the world.

PHP has sadly acquired a bad reputation along the years, but recent releases (since PHP 7.0 mostly) have introduced neat language features, and many cleanups, which are excessively ignored by haters. PHP is also a fast scripting language, and is very flexible. PHP now has declared types, traits, variadic arguments, closures (with explicit scopes!), generators, and a huge backward compatibility. The development of PHP is led by [RFCs](https://wiki.php.net/rfc), which is an open and democratic process. The Gutenberg project is a new editor for WordPress. The latter is written in PHP. This is naturally that we want a native extension for PHP to parse the Gutenberg post format. PHP is a language with a [specification](https://github.com/php/php-langspec). The most popular virtual machine is [Zend Engine](http://php.net/manual/en/internals2.php). Other virtual machines exist, like [HHVM](https://hhvm.com/) (but the PHP support has been dropped recently in favor of their own PHP fork, called Hack), [Peachpie](https://www.peachpie.io/), or [Tagua VM](https://github.com/tagua-vm/tagua-vm) (under development). In this post, we will create an extension for Zend Engine. This virtual machine is written in C. Great, we have visited [the C galaxy in the previous episode](https://mnt.io/2018/09/11/from-rust-to-beyond-the-c-galaxy/)!

## Rust 🚀 C 🚀 PHP
![](./images08/rust-to-php.png)

To port our Rust parser into PHP, we first need to port it to C. It’s been done in the previous episode. Two files result from this port to C: `libgutenberg_post_parser.a` and `gutenberg_post_parser.h`, respectively a static library, and the header file.

### Bootstrap with a skeleton
PHP comes with [a script to create an extension skeleton](http://php.net/manual/en/internals2.buildsys.skeleton.php)/template, called [`ext_skel.php`](https://github.com/php/php-src/blob/master/ext/ext_skel.php). This script is accessible from the source of the Zend Engine virtual machine (which we will refer to as `php-src`). One can invoke the script like this:

```other
$ cd php-src/ext/
$ ./ext_skel.php \
      --ext gutenberg_post_parser \
      --author 'Ivan Enderlin' \
      --dir /path/to/extension \
      --onlyunix
$ cd /path/to/extension
$ ls gutenberg_post_parser
tests/
.gitignore
CREDITS
config.m4
gutenberg_post_parser.c
php_gutenberg_post_parser.h
```

The `ext_skel.php` script recommends to go through the following steps:

- Rebuild the configuration of the PHP source (run `./buildconf` at the root of the `php-src` directory),
- Reconfigure the build system to enable the extension, like `./configure --enable-gutenberg_post_parser`,
- Build with `make`,
- Done.

But our extension is very likely to live outside the `php-src` tree. So we will use `phpize` instead. `phpize` is an executable that comes with `php`, `php-cgi`, `phpdbg`, `php-config` etc. It allows to compile extensions against an already compiled `php` binary, which is perfect in our case! We will use it like this :

```other
$ cd /path/to/extension/gutenberg_post_parser

$ # Get the bin directory for PHP utilities.
$ PHP_PREFIX_BIN=$(php-config --prefix)/bin

$ # Clean (except if it is the first run).
$ $PHP_PREFIX_BIN/phpize --clean

$ # “phpize” the extension.
$ $PHP_PREFIX_BIN/phpize

$ # Configure the extension for a particular PHP version.
$ ./configure --with-php-config=$PHP_PREFIX_BIN/php-config

$ # Compile.
$ make install
```

In this post, we will not show all the edits we have done, but we will rather focus on the extension binding. `All the sources can be found here`(https://github.com/Hywan/gutenberg-parser-rs/tree/master/bindings/php/extension/gutenberg_post_parser). Shortly, here is the `config.m4` file:

```other
PHP_ARG_ENABLE(gutenberg_post_parser, whether to enable gutenberg_post_parser support,
[  --with-gutenberg_post_parser          Include gutenberg_post_parser support], no)

if  test "$PHP_GUTENBERG_POST_PARSER" != "no"; then
  PHP_SUBST(GUTENBERG_POST_PARSER_SHARED_LIBADD)

  PHP_ADD_LIBRARY_WITH_PATH(gutenberg_post_parser, ., GUTENBERG_POST_PARSER_SHARED_LIBADD)

  PHP_NEW_EXTENSION(gutenberg_post_parser, gutenberg_post_parser.c, $ext_shared)
fi
```

What it does is basically the following:
- Register the `--with-gutenberg_post_parser` option in the build system, and
- Declare the static library to compile with, and the source of the extension itself.

We must add the `libgutenberg_post_parser.a` and `gutenberg_post_parser.h` files in the same directory (a symlink is perfect), to get a structure such as:

```other
$ ls gutenberg_post_parser
tests/                       # from ext_skel
.gitignore                   # from ext_skel
CREDITS                      # from ext_skel
config.m4                    # from ext_skel (edited)
gutenberg_post_parser.c      # from ext_skel (will be edited)
gutenberg_post_parser.h      # from Rust
libgutenberg_post_parser.a   # from Rust
php_gutenberg_post_parser.h  # from ext_skel
```

The core of the extension is the `gutenberg_post_parser.c` file. This file is responsible to create the module, and to bind our Rust code to PHP.

### The module, aka the extension
As said, we will work in the `gutenberg_post_parser.c` file. First, let’s include everything we need:

```c
#include "php.h"
#include "ext/standard/info.h"
#include "php_gutenberg_post_parser.h"
#include "gutenberg_post_parser.h"
```

The last line includes the `gutenberg_post_parser.h` file generated by Rust (more precisely, by `cbindgen`, if you don’t remember, [take a look at the previous episode](https://mnt.io/2018/09/11/from-rust-to-beyond-the-c-galaxy/)). Then, we have to decide what API we want to expose into PHP? As a reminder, the Rust parser produces an AST defined as:

```rust
pub enum Node<'a> {
    Block {
        name: (Input<'a>, Input<'a>),
        attributes: Option<Input<'a>>,
        children: Vec<Node<'a>>
    },
    Phrase(Input<'a>)
}
```

The C variant of the AST is very similar (with more structures, but the idea is almost identical). So in PHP, the following structure has been selected:

```php
class Gutenberg_Parser_Block {
    public string $namespace;
    public string $name;
    public string $attributes;
    public array $children;
}

class Gutenberg_Parser_Phrase {
    public string $content;
}

function gutenberg_post_parse(string $gutenberg_post): array;
```

The `gutenberg_post_parse` function will output an array of objects of kind `Gutenberg_Parser_Block` or `Gutenberg_Parser_Phrase`, i.e. our AST. So, let’s declare those classes!

### Declare the classes
_Note: The next 4 code blocks are not the core of the post, it is just code that needs to be written, you can skip it if you are not about to write a PHP extension._

```c
zend_class_entry *gutenberg_parser_block_class_entry;
zend_class_entry *gutenberg_parser_phrase_class_entry;
zend_object_handlers gutenberg_parser_node_class_entry_handlers;

typedef struct _gutenberg_parser_node {
    zend_object zobj;
} gutenberg_parser_node;
```

A class entry represents a specific class type. A handler is associated to a class entry. The logic is somewhat complicated. If you need more details, I recommend to read the [PHP Internals Book](http://www.phpinternalsbook.com/). Then, let’s create a function to instanciate those objects:

```c
static zend_object *create_parser_node_object(zend_class_entry *class_entry)
{
    gutenberg_parser_node *gutenberg_parser_node_object;

    gutenberg_parser_node_object = ecalloc(1, sizeof(*gutenberg_parser_node_object) + zend_object_properties_size(class_entry));

    zend_object_std_init(&gutenberg_parser_node_object->zobj, class_entry);
    object_properties_init(&gutenberg_parser_node_object->zobj, class_entry);

    gutenberg_parser_node_object->zobj.handlers = &gutenberg_parser_node_class_entry_handlers;

    return &gutenberg_parser_node_object->zobj;
}
```

Then, let’s create a function to free those objects. It works in two steps: Destruct the object by calling its destructor (in the user-land), then free it for real (in the VM-land):

```c
static void destroy_parser_node_object(zend_object *gutenberg_parser_node_object)
{
    zend_objects_destroy_object(gutenberg_parser_node_object);
}

static void free_parser_node_object(zend_object *gutenberg_parser_node_object)
{
    zend_object_std_dtor(gutenberg_parser_node_object);
}
```

Then, let’s initialize the “module”, i.e. the extension. During the initialisation, we will create the classes in the user-land, declare their attributes etc.

```c
PHP_MINIT_FUNCTION(gutenberg_post_parser)
{
    zend_class_entry class_entry;

    // Declare Gutenberg_Parser_Block.
    INIT_CLASS_ENTRY(class_entry, "Gutenberg_Parser_Block", NULL);
    gutenberg_parser_block_class_entry = zend_register_internal_class(&class_entry TSRMLS_CC);

    // Declare the create handler.
    gutenberg_parser_block_class_entry->create_object = create_parser_node_object;

    // The class is final.
    gutenberg_parser_block_class_entry->ce_flags |= ZEND_ACC_FINAL;

    // Declare the `namespace` public attribute,
    // with an empty string for the default value.
    zend_declare_property_string(gutenberg_parser_block_class_entry, "namespace", sizeof("namespace") - 1, "", ZEND_ACC_PUBLIC);

    // Declare the `name` public attribute,
    // with an empty string for the default value.
    zend_declare_property_string(gutenberg_parser_block_class_entry, "name", sizeof("name") - 1, "", ZEND_ACC_PUBLIC);

    // Declare the `attributes` public attribute,
    // with `NULL` for the default value.
    zend_declare_property_null(gutenberg_parser_block_class_entry, "attributes", sizeof("attributes") - 1, ZEND_ACC_PUBLIC);

    // Declare the `children` public attribute,
    // with `NULL` for the default value.
    zend_declare_property_null(gutenberg_parser_block_class_entry, "children", sizeof("children") - 1, ZEND_ACC_PUBLIC);

    // Declare the Gutenberg_Parser_Block.

    … skip …

    // Declare Gutenberg parser node object handlers.

    memcpy(&gutenberg_parser_node_class_entry_handlers, zend_get_std_object_handlers(), sizeof(gutenberg_parser_node_class_entry_handlers));

    gutenberg_parser_node_class_entry_handlers.offset = XtOffsetOf(gutenberg_parser_node, zobj);
    gutenberg_parser_node_class_entry_handlers.dtor_obj = destroy_parser_node_object;
    gutenberg_parser_node_class_entry_handlers.free_obj = free_parser_node_object;

    return SUCCESS;
}
```

If you are still reading, first: Thank you, and second: Congrats! Then, there is a `PHP_RINIT_FUNCTION` and a `PHP_MINFO_FUNCTION` functions that are already generated by the `ext_skel.php` script. Same for the module entry definition and other module configuration details.

### The `gutenberg_post_parse` function
We will now focus on the `gutenberg_post_parse` PHP function. This function takes a string as a single argument  and returns either `false` if the parsing failed, or an array of objects of kind `Gutenberg_Parser_Block` or `Gutenberg_Parser_Phrase` otherwise. Let’s write it! Notice that it is declared with [the `PHP_FUNCTION` macro](https://github.com/php/php-src/blob/52d91260df54995a680f420884338dfd9d5a0d49/main/php.h#L400).

```c
PHP_FUNCTION(gutenberg_post_parse)
{
    char *input;
    size_t input_len;

    // Read the input as a string.
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &input, &input_len) == FAILURE) {
        return;
    }
```

At this step, the argument has been declared and typed as a string (`"s"`). The string value is in `input` and the string length is in `input_len`. The next step is to parse the `input`. (The length of the string is not needed). This is where we are going to call our Rust code! Let’s do that:

```c
// Parse the input.
    Result parser_result = parse(input);

    // If parsing failed, then return false.
    if (parser_result.tag == Err) {
        RETURN_FALSE;
    }

    // Else map the Rust AST into a PHP array.
    const Vector_Node nodes = parse_result.ok._0;
```

The `Result` type and the `parse` function come from Rust. If you don’t remember those types, please [read the previous episode about the C galaxy](https://mnt.io/2018/09/11/from-rust-to-beyond-the-c-galaxy/). Zend Engine has a macro called `RETURN_FALSE` to return… `false`! Handy isn’t it? Finally, if everything went well, we get back a collection of node as a `Vector_Node` type. The next step is to map those Rust/C types into PHP types, i.e. an array of the Gutenberg classes. Let’s go:

```c
    // Note: return_value is a “magic” variable that holds the value to be returned.
    //
    // Allocate an array.
    array_init_size(return_value, nodes.length);

    // Map the Rust AST.
    into_php_objects(return_value, &nodes);
}
```

Done 😁! Oh wait… the `into_php_objects` function need to be written!

### The `into_php_objects` function
This function is not terribly complex: It’s just full of Zend Engine specific API as expected. We are going to explain how to map a `Block` into a `Gutenberg_Parser_Block` object, and to let the `Phrase` mapping to `Gutenberg_Parser_Phrase` for the assiduous readers. And there we go:

```c
void into_php_objects(zval *php_array, const Vector_Node *nodes)
{
    const uintptr_t number_of_nodes = nodes->length;

    if (number_of_nodes == 0) {
        return;
    }

    // Iterate over all nodes.
    for (uintptr_t nth = 0; nth < number_of_nodes; ++nth) {
        const Node node = nodes->buffer[nth];

        if (node.tag == Block) {
            // Map Block into Gutenberg_Parser_Block.
        } else if (node.tag == Phrase) {
            // Map Phrase into Gutenberg_Parser_Phrase.
        }
    }
}
```

Now let’s map a block. The process is the following:
    1. Allocate PHP strings for the block namespace, and for the block name,
    2. Allocate an object,
    3. Set the block namespace and the block name to their respective object properties,
    4. Allocate a PHP string for the block attributes if any,
    5. Set the block attributes to its respective object property,
    6. If any children, initialise a new array, and call `into_php_objects` with the child nodes and the new array,
    7. Set the children to its respective object property,
    8. Finally, add the block object inside the array to be returned.

```c
const Block_Body block = node.block;
zval php_block, php_block_namespace, php_block_name;

// 1. Prepare the PHP strings.
ZVAL_STRINGL(&php_block_namespace, block.namespace.pointer, block.namespace.length);
ZVAL_STRINGL(&php_block_name, block.name.pointer, block.name.length);
```

Do you remember that namespace, name and other similar data are of type `Slice_c_char`? It’s just a structure with a pointer and a length. The pointer points to the original input string, so that there is no copy (and this is the definition of a slice actually). Well, Zend Engine has [a `ZVAL_STRINGL` macro](https://github.com/php/php-src/blob/52d91260df54995a680f420884338dfd9d5a0d49/Zend/zend_API.h#L563-L565) that allows to create a string from a pointer and a length, great! Unfortunately for us, Zend Engine does [a copy behind the scene](https://github.com/php/php-src/blob/52d91260df54995a680f420884338dfd9d5a0d49/Zend/zend_string.h#L152-L159)… There is no way to keep the pointer and the length only, but it keeps the number of copies small. I think it is to take the full ownership of the data, which is required for the garbage collector.

```c
// 2. Create the Gutenberg_Parser_Block object.
object_init_ex(&php_block, gutenberg_parser_block_class_entry);
```

The object has been instanciated with a class represented by the `gutenberg_parser_block_class_entry`.

```c
// 3. Set the namespace and the name.
add_property_zval(&php_block, "namespace", &php_block_namespace);
add_property_zval(&php_block, "name", &php_block_name);

zval_ptr_dtor(&php_block_namespace);
zval_ptr_dtor(&php_block_name);
```

The `zval_ptr_dtor` adds 1 to the reference counter. This is required for the garbage collector.

```c
// 4. Deal with block attributes if some.
if (block.attributes.tag == Some) {
    Slice_c_char attributes = block.attributes.some._0;
    zval php_block_attributes;

    ZVAL_STRINGL(&php_block_attributes, attributes.pointer, attributes.length);

    // 5. Set the attributes.
    add_property_zval(&php_block, "attributes", &php_block_attributes);

    zval_ptr_dtor(&php_block_attributes);
}
```

It is similar to what has been done for `namespace` and `name`. Now let’s continue with children.

```c
// 6. Handle children.
const Vector_Node *children = (const Vector_Node*) (block.children);

if (children->length > 0) {
    zval php_children_array;

    array_init_size(&php_children_array, children->length);

    // Recursion.
    into_php_objects(&php_children_array, children);

    // 7. Set the children.
    add_property_zval(&php_block, "children", &php_children_array);

    Z_DELREF(php_children_array);
}

free((void*) children);
```

Finally, add the block instance into the array to be returned:

```c
// 8. Insert the object in the collection.
add_next_index_zval(php_array, &php_block);
```

[The entire code lands here.](https://github.com/Hywan/gutenberg-parser-rs/blob/master/bindings/php/extension/gutenberg_post_parser/gutenberg_post_parser.c)

## PHP extension 🚀 PHP userland
Now the extension is written, we have to compile it. That’s the repetitive set of commands we have shown above with `phpize`. Once the extension is compiled, the `generated gutenberg_post_parser.so` file must be located in the extension directory. This directory can be found with the following command:

```other
$ php-config --extension-dir
```

For instance, in my computer, the extension directory is `/usr/local/Cellar/php/7.2.11/pecl/20170718`. Then, to enable the extension for a given execution, you must write:

```other
$ php -d extension=gutenberg_post_parser -m | \
      grep gutenberg_post_parser
```

Or, to enable the extension for all executions, locate the `php.ini` file with `php --ini` and edit it to add:

```other
extension=gutenberg_post_parser
```

Done! Now, let’s use some reflection to check the extension is correctly loaded and handled by PHP:

```other
$ php --re gutenberg_post_parser
Extension [ <persistent> extension #64 gutenberg_post_parser version 0.1.0 ] {

  - Functions {
    Function [ <internal:gutenberg_post_parser> function gutenberg_post_parse ] {

      - Parameters [1] {
        Parameter #0 [ <required> $gutenberg_post_as_string ]
      }
    }
  }

  - Classes [2] {
    Class [ <internal:gutenberg_post_parser> final class Gutenberg_Parser_Block ] {

      - Constants [0] {
      }

      - Static properties [0] {
      }

      - Static methods [0] {
      }

      - Properties [4] {
        Property [ <default> public $namespace ]
        Property [ <default> public $name ]
        Property [ <default> public $attributes ]
        Property [ <default> public $children ]
      }

      - Methods [0] {
      }
    }

    Class [ <internal:gutenberg_post_parser> final class Gutenberg_Parser_Phrase ] {

      - Constants [0] {
      }

      - Static properties [0] {
      }

      - Static methods [0] {
      }

      - Properties [1] {
        Property [ <default> public $content ]
      }

      - Methods [0] {
      }
    }
  }
}
```

Everything looks good: There is one function and two classes that are defined as expected. Now, let’s write some PHP code for the first time in this blog post!

```php
<?php

var_dump(
    gutenberg_post_parse(
        '<!-- wp:foo /-->bar<!-- wp:baz -->qux<!-- /wp:baz -->'
    )
);

/**
 * Will output:
 *     array(3) {
 *       [0]=>
 *       object(Gutenberg_Parser_Block)#1 (4) {
 *         ["namespace"]=>
 *         string(4) "core"
 *         ["name"]=>
 *         string(3) "foo"
 *         ["attributes"]=>
 *         NULL
 *         ["children"]=>
 *         NULL
 *       }
 *       [1]=>
 *       object(Gutenberg_Parser_Phrase)#2 (1) {
 *         ["content"]=>
 *         string(3) "bar"
 *       }
 *       [2]=>
 *       object(Gutenberg_Parser_Block)#3 (4) {
 *         ["namespace"]=>
 *         string(4) "core"
 *         ["name"]=>
 *         string(3) "baz"
 *         ["attributes"]=>
 *         NULL
 *         ["children"]=>
 *         array(1) {
 *           [0]=>
 *           object(Gutenberg_Parser_Phrase)#4 (1) {
 *             ["content"]=>
 *             string(3) "qux"
 *           }
 *         }
 *       }
 *     }
 */
```

It works very well!

## Conclusion
The journey is:
    - A string written in PHP,
    - Allocated by the Zend Engine from the Gutenberg extension,
    - Passed to Rust through FFI (static library + header),
    - Back to Zend Engine in the Gutenberg extension,
    - To generate PHP objects,
    - That are read by PHP.

Rust fits really everywhere! We have seen in details how to write a real world parser in Rust, how to bind it to C and compile it to a static library in addition to C headers, how to create a PHP extension exposing one function and two objects, how to integrate the C binding into PHP, and how to use this extension in PHP. As a reminder, the C binding is about 150 lines of code. The PHP extension is about 300 lines of code, but substracting “decorations” (the boilerplate to declare and manage the extension) that are automatically generated, the PHP extension reduces to about 200 lines of code. Once again, I find this is a small surface of code to review considering the fact that the parser is still written in Rust, and modifying the parser will not impact the bindings (except if the AST is updated obviously)! PHP is a language with a garbage collector. It explains why all strings are copied, so that they are owned by PHP itself. However, the fact that Rust does not copy any data saves memory allocations and deallocations, which is the biggest cost most of the time. Rust also provides safety. This property can be questionned considering the number of binding we are going through: Rust to C to PHP: Does it still hold? From the Rust perspective, yes, but everything that happens inside C or PHP must be considered unsafe. A special care must be put in the C binding to handle all situations. Is it still fast? Well, let’s benchmark. I would like to remind that the first goal of this experiment was to tackle the bad performance of the original PEG.js parser. On the JavaScript ground, WASM and ASM.js have shown to be very much faster (see [the WebAssembly galaxy](https://mnt.io/2018/08/22/from-rust-to-beyond-the-webassembly-galaxy/), and [the ASM.js galaxy](https://mnt.io/2018/08/28/from-rust-to-beyond-the-asm-js-galaxy/)). For PHP, [`phpegjs` is used](https://github.com/nylen/phpegjs): It reads the grammar written for PEG.js and compiles it to PHP. Let’s see how they compare:

文件名 | PEG PHP parser (ms) | Rust parser as a PHP extension (ms) | 提升倍数
---| ---- |---- |---
[`demo-post.html`](https://raw.githubusercontent.com/dmsnell/gutenberg-document-library/master/library/demo-post.html) | 30.409 |  0.0012   |  × 25341
[`shortcode-shortcomings.html`](https://raw.githubusercontent.com/dmsnell/gutenberg-document-library/master/library/shortcode-shortcomings.html) | 76.39 |  0.096   | × 796
[`redesigning-chrome-desktop.html`](https://raw.githubusercontent.com/dmsnell/gutenberg-document-library/master/library/redesigning-chrome-desktop.html) | 225.824 |  0.399   |  × 566
[`web-at-maximum-fps.html`](https://raw.githubusercontent.com/dmsnell/gutenberg-document-library/master/library/web-at-maximum-fps.html) | 173.495 |  0.275   |  × 631
[`early-adopting-the-future.html`](https://raw.githubusercontent.com/dmsnell/gutenberg-document-library/master/library/early-adopting-the-future.html) | 	280.433 |  0.298   |  × 941
[`pygmalian-raw-html.html`](https://raw.githubusercontent.com/dmsnell/gutenberg-document-library/master/library/pygmalian-raw-html.html) | 377.392 |  	0.052   |  × 7258
[`moby-dick-parsed.html`](https://raw.githubusercontent.com/dmsnell/gutenberg-document-library/master/library/moby-dick-parsed.html) | 5,437.630 |  5.037   |  × 1080

The PHP extension of the Rust parser is in average 5230 times faster than the actual PEG PHP implementation. The median of the speedup is 941. Another huge issue was that the PEG parser was not able to handle many Gutenberg documents because of a memory limit. Of course, it is possible to grow the size of the memory, but it is not ideal. With the Rust parser as a PHP extension, memory stays constant and close to the size of the parsed document. I reckon we can optimise the extension further to generate an iterator instead of an array. This is something I want to explore and analyse the impact on the performance. The PHP Internals Book has a [chapter about Iterators](http://www.phpinternalsbook.com/classes_objects/iterators.html). We will see in the next episodes of this series that Rust can reach a lot of galaxies, and the more it travels, the more it gets interesting. Thanks for reading!
