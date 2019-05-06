# 用`Wasmer`进行插件开发1
* *原文链接 https://wiredforge.com/blog/wasmer-plugin-pt-1/index.html/*
>正文开始：

* 几个月之前，[Wasmer](https://wasmer.io/) 团队发布了一个 `Web Assembly(aka wasm)` 解释器，用于rust程序的嵌入式开发。对于任何想要在项目中添加插件的人来说，这尤其令人兴奋，因为Rust提供了一种直接将程序编译到 `wasm` 的方法，这个应该是一个很好的选择。在这个系列的博客文章中，我们将研究如何使用 `wasmer` 和 `rust` 构建插件系统。

## 步骤
* 在我们深入研究细节之前，我们心里要想像一个这个项目的轮廓。如果你在电脑上继续接下来的学习，你可以做到；如果没有电脑，一切做起来可能显得没那么神奇。为此，我们将利用 `cargo` 的workspace特性，该特性可以让我们在一个父项目中聚合一组相关的项目。这里相关的代码你都能在 [github仓库](https://github.com/FreeMasen/wiredforge-wasmer-plugin-code) 上找到，每个分支代表这个系列的不同状态。我们要研究的基本结构是这样的：

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
* 为此，我们将从创建父项目开始。运行以下命令：

```
cargo new --lib wasmer-plugin-example
cd wasmer-plugin-example
```

* 一旦我们创建了这个目录，我们就切换到那个目录中，然后用你选择好的编辑器打开，然后打开其中的 `Cargo.toml` 文件。我们需要将 `[workspace]` 增加到配置中，并指向到上方所说的crates库中的3个项目。配置内容参考如下所示：

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

* 现在我们可以在当前目录中创建crates目录和项目：

```
mkdir ./crates
cd ./crates
cargo new --lib example-plugin
cargo new --lib example-macro
cargo new example-runner
```

* 这样，我们就设置好了工作空间。这将允许我们使用项目内任何目录中的cargo命令，以及工作区中任何其他项目中的目标活动。我们用 `-p` 参数来告诉 `cargo` 我们想要应用的项目。如果我们想要构建 `example-plugin` 项目的实例，可以使用下面的命令：

```
cargo build -p example-plugin
```

* 在我们所有工作空间中，为了设置好开发环境，我们会花一些时间。首先，大多数情况下，我们需要有rust编译器，`cargo` 和 `rustup` 。如果你需要这些，可以去 [rustup.rs](rustup.rs) 。在我们安装了这些工具后，我们需要用 `rustup` 来构建目标 `web assembly` 。

```
rustup target add wasm32-unknown-unknown
```

* 除了 `rust` 所必须的之外，我们还需要 `wasmer` 相关的东西。完整的指南可以在这里找到，对于大多数系统，你可能只需要确认安装了 `cmake`  即可，对于windows，可能稍微复杂一些，但依赖指南上有链接地址。

## 第一个插件
* 这种情况下，我们应该谈论房间中的大象，`Web Assembly` 的规范仅仅允许数字的存在。值得庆幸的是，在rust中的 `web assembly` 已经可以为我们处理这个问题，但我们想要调用插件中的功能，只需要接收数字作为参数，并且只返回数字。记住这个规范，让我们开始一个非常简单的实例。我会记录这个示例，虽然不会很有帮助，但我保证我们将会逐渐提升能力来做更多有趣的东西。

```rust
// ./crates/example-plugin/src/lib.rs
#[no_mangle]
pub fn add(one: i32, two: i32) -> i32 {
    one + two
}
```

* 上方示例看起来是一个非常原生并且没有意义的示例，但它符合我们的只处理数字的需求。现在我们开始编译称为 `Web Assembly` ，我们需要在 `Cargo.toml` 文件中设置一些东西。

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

* 这里比较关键的是 `crate-type = ["cdylib"]`，它表示我们将会编译这个crate称为一个c链接库。现在我们使用下面的命令进行编译：

```shell
cargo build --target wasm32-unknown-unknown
```

* 到这里，我们应该有一个文件位于： `./target/wasm32-unknown-unknown/debug/example_plugin.wasm` 。现在，让我们构建一个可以运行这个的程序，第一步我们将设置好所有依赖。

## 我们的第一个运行器

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

* 这里，我们增加了 `wamer_runtime` 库，我们将用它连接我们的 `web assembly` 模块。

```rust
// ./crates/example-runner/src/main.rs
use wasmer_runtime::{
    imports,
    instantiate,
};
// For now we are going to use this to read in our wasm bytes
static WASM: &[u8] = include_bytes!("../../../target/wasm32-unknown-unknown/debug/example_plugin.wasm");

fn main() {
    // Instantiate the web assembly module
    let instance = instantiate(WASM, &imports!{}).expect("failed to instantiate wasm module");
    // Bind the add function from the module
    let add = instance.func::<(i32, i32), i32>("add").expect("failed to bind function add");
    // execute the add function
    let three = add.call(1, 2).expect("failed to execute add");
    println!("three: {}", three); // "three: 3"
}
```

* 首先，我们使用`use`语句，导入其中的2个方法实现；为了简单定义我们导入的项目的导入宏，和将字节转换为 `web assembly` 模块的函数实例。我们将会使用 `include_bytes` 。现在，使用宏来读入我们的字节，但我们想要它更灵活一些。在main函数中，我们将通过2个参数调用实例,第一个参数是 `wasm` 字节，第2个是导入的空对象。接下来，我们将使用实例中的函数去绑定那个2个 `i32` 类型数据相加返回一个 `i32` 类型值的函数。此时，我们可以调用函数中的add方法。然后打印结果到终端下。当我们使用 `cargo` 命令运行它，应该会成功的打印3：3出现在终端中。
* 耶，成功了！但那并没什么卵用。让我们研究一下，我们需要让它更有用一些。

## 深入探究
### 我们需要
* 运行函数前需要访问 `WASM` 内存 
* 插入一个更加复杂的数据结果到内存中的1个方法
* 一个方法，用于与wasm模块通信数据的位置和具体内容
* 插件被执行时，1个用于提取从wasm内存中更新的信息的系统
* 首先，运行我们的功能之前，我们需要一个初始化 `wasm` 模块内存中一些值的方法。幸运的是，`wasmer_runtime` 提供给我们一个完整的方法。让我们更新我们的实例，给定在一个字符串和返回字符串的长度，这跟上面的小例子相比，并不好到哪儿去，但。。。一步一步来吧。

## 我们的第2个插件

```rust
// ./crates/example-plugin/src/lib.rs

/// This is the actual code we would 
/// write if this was a pure rust
/// interaction
pub fn length(s: &str) -> u32 {
    s.len() as u32
}

/// Since it isn't we need a way to
/// translate the data from wasm
/// to rust
#[no_mangle]
pub fn _length(ptr: i32, len: u32) -> u32 {
    // Extract the string from memory.
    let value = unsafe { 
        let slice = ::std::slice::from_raw_parts(ptr as _, len as _);
        String::from_utf8_lossy(slice)
    };
    //pass the value to `length` and return the result
    length(&value)
}
```

* 这一次，我们需要做的还有很多，让我们回顾一下发生了什么。首先，我们定义了一个 `length` 的函数，这正是我们想要的，如我们从另一个rust程序中使用这个库。一旦我们使用这个库作为一个`wasm` 模块，我们需要增加一个处理内存交互的辅助。这看起来似乎是一个奇怪的结构，但这样做，可以提供额外的灵活性，随着我们的深入，这种灵活性会更加明显。`_length` 函数就能起到这个辅助作用。首先，我们需要参数和返回值来匹配跨越 `wasm` 边界时可用的值（只有数字）。然后，我们的参数将描述字符串，指针是这个字符串的其实部分，并且 `len` 是字符串的长度。因为我们处理的是原始内存，所以我们需要在一个`unsafe`的块内进行转换（我知道这有点吓人，但我们要确保运行器中确实有这个字符串）。一旦我们将字符串从内存中取出，就可以向平常一样将其传递到length,返回结果。继续像之前那样进行构建吧。

```shell
cargo build --target wasm32-unknown-unknown
```

* 现在我们看看如何在启动器中设置它。

```rust
// ./crates/example-runner/src/main.rs
use wasmer_runtime::{
    imports,
    instantiate,
};

// For now we are going to use this to read in our wasm bytes
static WASM: &[u8] = include_bytes!("../../../target/wasm32-unknown-unknown/debug/example_plugin.wasm");

fn main() {
    let instance = instantiate(&WASM, &imports!{}).expect("failed to instantiate wasm module");
    // The changes start here
    // First we get the module's context
    let context = instance.context();
    // Then we get memory 0 from that context
    // web assembly only supports one memory right
    // now so this will always be 0.
    let memory = context.memory(0);
    // Now we can get a view of that memory
    let view = memory.view::<u8>();
    // This is the string we are going to pass into wasm
    let s = "supercalifragilisticexpialidocious".to_string();
    // This is the string as bytes
    let bytes = s.as_bytes();
    // Our length of bytes
    let len = bytes.len();
    // loop over the wasm memory view's bytes
    // and also the string bytes
    for (cell, byte) in view[1..len + 1].iter().zip(bytes.iter()) {
        // set each wasm memory byte to 
        // be the value of the string byte
        cell.set(*byte)
    }
    // Bind our helper function
    let length = instance.func::<(i32, u32), u32>("_length").expect("Failed to bind _length");
    let wasm_len = match length.call(1 as i32, len as u32) {
        Ok(l) => l,
        Err(e) => panic!("{}\n\n{:?}", e, e),
    }; //.expect("Failed to execute _length");
    println!("original: {}, wasm: {}", len, wasm_len); // original: 34, wasm: 34
}
```

* 好了，这次我们需要做更多的事情。开始的几行，更之前完全一样，我们将读取 `wasm` 并且实例化它。一旦加载完成，我们将获得 `wasm` 内存中的视图，我们首先从模块实例中获得 Ctx (context) 。一旦有了上下文，我们就可以通过调用内存(0)，`web assembly` 目前仅仅只有一个内存区域，所以在短期内，它的值总是0，但以后可能允许有多个内存。获取原始内存的最后一步是调用 `view()` 方法，我们终于可以修改模块的内存了。视图的类型是 `Vec<Cell<u8>>` ， 因此我们有一个字节数组，但是每个字节都包装在一个单元中。根据文档中，一个单元( [Cell](https://doc.rust-lang.org/std/cell/struct.Cell.html) )是允许修改不可变值的，在我们的场景中，它的意思是：“我不会让这个内存变长或变短，只是更改它的值”。
* 现在，我们定义要传递给wasm内存的字符串，并将其转换为字节。我们还想跟踪字符串的字节长度，因此我们将其捕获它作为 `len` 。要将字符串字节放入内存字节中，我们将使用 [`Zip`](https://doc.rust-lang.org/std/iter/struct.Zip.html) 迭代器，这将允许我们循环时能同时得到2个值。在每次迭代中，我们会在同一个索引中的单元和字符字节处停止，在循环体中，我们将 `wasm` 内存字节的值赋值为字符串字节的值。注意，我们从视图的索引1开始，这意味着我们的参数指针将是1，并且我们的字节长度将是 `len` 参数。

```shell
cargo run
original: 34, wasm: 34
```

* 耶，再次成功了！但是，仍然没什么卵用。但是它将给我们处理复杂数据提供了一个良好的基础。在第2部分中，我们将了解如何与等式两边的 `wasm` 内存进行交互。
