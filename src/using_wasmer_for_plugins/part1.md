# 【译】用 `Wasmer` 进行插件开发 1
>* Using Wasmer for Plugins Part 1 译文
>* 原文链接 https://wiredforge.com/blog/wasmer-plugin-pt-1/index.html
>* 原文 Gitbook：https://freemasen.github.io/wiredforge-wasmer-plugin-code/
>* 译文来自：https://github.com/suhanyujie/article-transfer-rs/

几个月之前，[Wasmer](https://wasmer.io/) 团队发布了一个 `Web Assembly(aka wasm)` 解释器，用于rust程序的嵌入式开发。对于任何想要在项目中添加插件的人来说，这尤其令人兴奋，因为Rust提供了一种直接将程序编译到 `wasm` 的方法，这个应该是一个很好的选择。在这个系列的博客文章中，我们将研究如何使用 `wasmer` 和 `rust` 构建插件系统。

## 环境设置
在我们深入研究细节之前，我们心里要想像一个这个项目的轮廓。如果你在电脑上继续接下来的学习，你可以做到；如果没有电脑，一切做起来可能显得没那么神奇。为此，我们将利用 `cargo` 的workspace特性，该特性可以让我们在一个父项目中聚合一组相关的项目。这里相关的代码你都能在 [Github 仓库](https://github.com/FreeMasen/wiredforge-wasmer-plugin-code) 上找到，每个分支代表这个系列的不同状态。我们要研究的基本结构是这样的：

```
wasmer-plugin-example
├── Cargo.toml
├── crates
│   ├── example-macro
│   │   ├── Cargo.toml
│   │   └── src
│   │       └── lib.rs
│   ├── example-plugin
│   │   ├── Cargo.toml
│   │   └── src
│   │       └── lib.rs
│   └── example-runner
│       ├── Cargo.toml
│       └── src
│           └── main.rs
└── src
    └── lib.rs
```

* `wasmer-plugin-example` 是一个rust库，我们将在下一个部分谈论其中的细节。
* `crates` 这个目录将会存放我们所有其他的项目
* `example-plugin` 用于测试插件以保证运行结果是我们期望的
* `example-runner` 二进制项目，将会作为插件的host
* `example-macro` 一个 `proc_macro` 库，将会在下一个部分文章中进行创建

为此，我们将从创建父项目开始。运行以下命令：

```
cargo new --lib wasmer-plugin-example
cd wasmer-plugin-example
```

一旦我们创建了这个目录，我们就切换到那个目录中，然后用你选择好的编辑器打开，然后打开其中的 `Cargo.toml` 文件。我们需要将 `[workspace]` 增加到配置中，并指向到上方所说的 crate 库中的 3 个项目。配置内容参考如下所示：

```
[package]
name = "wasmer-plugin-example"
version = "0.1.0"
authors = ["freemasen <r@wiredforge.com>"]
edition = "2018"

[dependencies]


[workspace]
members = [
    "./crates/example-macro",
    "./crates/example-plugin",
    "./crates/example-runner",
]
```

现在我们可以在当前目录中创建crates目录和项目：

```
mkdir ./crates
cd ./crates
cargo new --lib example-plugin
cargo new --lib example-macro
cargo new example-runner
```

这样，我们就设置好了工作目录。我们可以在该项目内的任意目录中使用 cargo 命令，以及工作区中的任意项目中构建目标。我们用 `-p` 参数来告诉 `cargo` 我们想要应用的项目。例如我们想要构建 `example-plugin` 项目，可以使用下面的命令：

```
cargo build -p example-plugin
```

在我们所有工作空间中，为了设置好开发环境，我们会花一些时间。首先，大多数情况下，我们需要有rust编译器，`cargo` 和 `rustup` 。如果你需要这些，可以去 [rustup.rs](rustup.rs) 。在我们安装了这些工具后，我们需要用 `rustup` 来构建目标 `web assembly` 。

```
rustup target add wasm32-unknown-unknown
```

除了 `rust` 所必须的之外，我们还需要 `wasmer` 相关的东西。完整的指南可以在[这里找到](https://github.com/wasmerio/wasmer#dependencies)，对于大多数系统，你可能只需要确认安装了 `cmake`  即可，对于windows，可能稍微复杂一些，但依赖指南上有链接地址。

## 第一个插件
把上面说的先放一边，我们该进入正题了，`Web Assembly` 的规范仅仅允许数值的存在。值得庆幸的是，在 Rust 中的 web assembly 已经可以为我们处理这个问题，但我们想要调用插件中的功能，只需要接收数字作为参数，并且只返回数字。记住这个规范，我们先以一个非常简单的实例作为开始。我会记录这个示例，虽然不会很有帮助，但我保证我们将会逐渐提升能力来做更多有趣的东西。

```rust
// ./crates/example-plugin/src/lib.rs
#[no_mangle]
pub fn add(one: i32, two: i32) -> i32 {
    one + two
}
```

上方示例看起来是一个非常原生并且没有意义的示例，但它符合我们的只处理数值的需求。现在我们开始编译成为 Web Assembly，我们需要在 `Cargo.toml` 文件中设置一些东西。

```rust
# ./crates/example-plugin/Cargo.toml
[package]
name = "example-plugin"
version = "0.1.0"
authors = ["freemasen <r@wiredforge.com>"]
edition = "2018"

[dependencies]


[lib]
crate-type = ["cdylib"]
```

这里比较关键的是 `crate-type = ["cdylib"]`，它表示我们将会编译这个 crate 库生成一个 c 动态链接库。现在我们使用下面的命令进行编译：

```shell
cargo build --target wasm32-unknown-unknown
```

到这里，我们应该有一个文件位于： `./target/wasm32-unknown-unknown/debug/example_plugin.wasm` 。现在，让我们构建一个可以运行这个的程序，第一步我们将声明好所有依赖。

## 第一个运行器

```rust
# ./crates/example-runner/Cargo.toml
[package]
name = "example-runner"
version = "0.1.0"
authors = ["freemasen <r@wiredforge.com>"]
edition = "2018"

[dependencies]
wasmer_runtime = "0.3.0"
```

这里，我们增加了 `wamer_runtime` 库，我们将用它连接我们的 `web assembly` 模块。

```rust
// ./crates/example-runner/src/main.rs
use wasmer_runtime::{
    imports,
    instantiate,
};
// 现在，我们使用它来读取 wasm 字节
static WASM: &[u8] = include_bytes!("../../../target/wasm32-unknown-unknown/debug/example_plugin.wasm");

fn main() {
    // 实例化 web assembly 模块
    let instance = instantiate(WASM, &imports!{}).expect("failed to instantiate wasm module");
    // 绑定模块中的 add 函数
    let add = instance.func::<(i32, i32), i32>("add").expect("failed to bind function add");
    // 调用 add 函数
    let three = add.call(1, 2).expect("failed to execute add");
    println!("three: {}", three); // "three: 3"
}
```

首先，我们使用 `use` 语句，导入其中的 2 个方法实现；`imports` 宏简单地用于定义导入的对象以及将字节转换为 web assembly 模块的 `instantiate` 函数。现在，我们将会使用 `include_bytes!` 宏，使用它来读入我们的字节，但我们想要它更灵活一些。在 `main` 函数中，我们将通过 2 个参数调用 `instantiate`，第一个参数是 `wasm` 字节，第二个是空的导入对象。接下来，我们将使用 `instantiate` 中的 `func` 去绑定 `add` 函数，该函数有两个 `i32` 类型参数和一个 `i32` 类型的返回值。此时，我们可以通过 `call` 调用函数中的 `add` 方法。然后打印结果到终端。当我们使用 `cargo` 命令运行它，应该会成功地打印 `three: 3` 出现在终端中。

耶，成功了！但那并没什么卵用。让我们研究一下，我们需要让它更有用一些。

## 深入探究
### 我们需要
* 运行函数前需要访问 `WASM` 内存 
* 一个插入更加复杂的数据结果到内存中的方法
* 一个方法，wasm 模块将在何处与什么样的数据通信
* 插件被执行后，从 wasm 内存中获取更新信息的系统

首先，运行我们的功能之前，我们需要一个方法来初始化 `wasm` 模块内存中一些值。幸运的是，`wasmer_runtime` 提供给我们一个完整的方法。让我们更新我们的实例，给定在一个字符串，返回字符串的长度，这跟上面的小例子相比，并没有好到哪儿去，但。。。一步一步来吧。

## 我们的第 2 个插件

```rust
// ./crates/example-plugin/src/lib.rs

// 如果这是纯 Rust 交互，我们的代码就是如下所示：
pub fn length(s: &str) -> u32 {
    s.len() as u32
}

// 因为我们不需要将数据从 wasm 转换到 rust 中
#[no_mangle]
pub fn _length(ptr: i32, len: u32) -> u32 {
    // 从内存中获取字符串
    let value = unsafe { 
        let slice = ::std::slice::from_raw_parts(ptr as _, len as _);
        String::from_utf8_lossy(slice)
    };
    // 传递值到 `length` 中，并返回结果
    length(&value)
}
```

这一次，我们需要做的还有很多，让我们回顾一下发生了什么。首先，我们定义了一个 `length` 的函数，如果我们从另一个 rust 程序中使用这个库，这正是我们想要的。一旦我们使用这个库作为一个 `wasm` 模块，我们需要增加一个处理内存交互的辅助方法。这看起来似乎是一个奇怪的结构，但这样做，可以提供额外的灵活性，随着我们的深入，这种灵活性会更加明显。`_length` 函数就能起到这个辅助作用。首先，我们需要参数和返回值来匹配跨越 `wasm` 边界时可用的值（只能是数值）。然后，我们的参数将是存放字符串的东西，`ptr` 是这个字符串的开头部分，并且 `len` 是字符串的长度。因为我们处理的是原始内存，所以我们需要在一个 `unsafe` 的代码块内进行转换（我知道这有点吓人，但我们要确保运行器中确实有这个字符串）。一旦我们将字符串从内存中取出，就可以像平常一样将其传递到 `length`，然后返回结果。继续像之前那样进行构建。

```shell
cargo build --target wasm32-unknown-unknown
```

现在我们看看如何在启动文件中设置它。

```rust
// ./crates/example-runner/src/main.rs
use wasmer_runtime::{
    imports,
    instantiate,
};

// 现在，我们将使用它读取 wasm 字节
static WASM: &[u8] = include_bytes!("../../../target/wasm32-unknown-unknown/debug/example_plugin.wasm");

fn main() {
    let instance = instantiate(&WASM, &imports!{}).expect("failed to instantiate wasm module");
    // 代码修改从这开始的，首先获取模块的上下文
    let context = instance.context();
    // 然后从 web assembly 上下文中得到起始内存 0，只支持一个内存块，所以它一直为 0
    let memory = context.memory(0);
    // 现在，我们可以获取内存的 view
    let view = memory.view::<u8>();
    // 这是我们要传递到 wasm 中的字符串
    let s = "supercalifragilisticexpialidocious".to_string();
    // string 转换为 bytes
    let bytes = s.as_bytes();
    // bytes 的长度
    let len = bytes.len();
    // 循环 wasm 内存 view 中的字节和字符串字节
    for (cell, byte) in view[1..len + 1].iter().zip(bytes.iter()) {
        // 将 wasm 内存中的字节设为字符串的字节值
        cell.set(*byte)
    }
    // 绑定辅助方法
    let length = instance.func::<(i32, u32), u32>("_length").expect("Failed to bind _length");
    let wasm_len = match length.call(1 as i32, len as u32) {
        Ok(l) => l,
        Err(e) => panic!("{}\n\n{:?}", e, e),
    }; //.expect("Failed to execute _length");
    println!("original: {}, wasm: {}", len, wasm_len); // original: 34, wasm: 34
}
```

好了，这次我们需要做更多的事情。开始的几行，跟之前完全一样，我们将读取 `wasm` 并且实例化它。一旦加载完成，我们将获得 `wasm` 内存中的 view，我们首先从模块实例中获得 Ctx (context) 。一旦有了上下文，我们就可以通过调用 `memory(0)`，web assembly 目前仅有一个内存区域，所以在短时间内，它的值总是 0，但以后可能允许有多块内存区域。获取原始内存的最后一步是调用 `view()` 方法，我们终于可以修改模块的内存了。`view`的类型是 `Vec<Cell<u8>>`， 因此我们有一个字节数组，但是每个字节都包装在一个 `Cell` 中。根据文档中，通过 [`Cell`](https://doc.rust-lang.org/std/cell/struct.Cell.html) 的方式可以修改本来的不可变值，在我们的场景中，它的意思是：“我不会让这个内存变长或变短，只是更改它的值”。

现在，我们定义传递给 wasm 内存的字符串，并将其转换为字节数组。我们还想跟踪字符串的字节长度，因此我们将其捕获它作为 `len`。要将字符串的字节数组放入内存字节中，我们将使用 [`Zip`](https://doc.rust-lang.org/std/iter/struct.Zip.html) 迭代器，这让我们循环时能同时得到 2 个值。在每次迭代中，会在 cell 索引等于字符字节索引时停止，在循环体中，我们将 `wasm` 内存字节的值赋值为字符串字节的值。注意，我们从 view 的索引 1 开始，这意味着我们的 `ptr` 参数是 1，并且我们的字节长度将是 `len` 参数。

```shell
cargo run
original: 34, wasm: 34
```

耶，再次成功了！但是，仍然没什么卵用。但是它将给我们处理复杂数据提供了一个良好的基础。在第2部分中，我们将了解如何与等式两边的 `wasm` 内存进行交互。
