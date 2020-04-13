# 使用 Wasmer 开发插件-第二部分
>* Using Wasmer for Plugins Part 2 译文

>* 原文链接：https://wiredforge.com/blog/wasmer-plugin-pt-2/index.html
>* 原文作者：[Robert Masen](https://github.com/freemasen)
>* 译文来自：https://github.com/suhanyujie/article-transfer-rs
>* 译者：[suhanyujie](https://www.github.com/suhanyujie)

如果你还有看过这个系列文章，你最好先查看[第一部分](https://wiredforge.com/blog/wasmer-plugin-pt-1/index.html)，在第一部分中我们回顾了使用 wasmer 的基础知识。在这篇你文章中，我们将讨论怎样将更复杂的数据从 wasm 模块中传递回运行器中。

### 另一个插件
To start we are going to create another plugin, this one will take a string as an argument and return that string doubled. Here is what that plugin would look like.
>首先，我们创建另一个插件，这个插件中将一个字符串作为参数，并返回自身的两倍。下面是这个插件的部分代码。

```rust
// ./crates/example-plugin/src/lib.rs

/// This is the actual code we would 
/// write if this was a pure rust
/// interaction
/// 这段代码是我们用 Rust 实现的需要用的代码
pub fn double(s: &str) -> String {
    s.repeat(2)
}

/// Since it isn't we need a way to
/// translate the data from wasm
/// to rust
/// 因为它不是我们需要的将 wasm 数据转换到 rust 数据的代码
#[no_mangle]
pub fn _double(ptr: i32, len: u32) -> i32 {
    // Extract the string from memory.
    // 从内存中取出字符串
    let value = unsafe { 
        let slice = ::std::slice::from_raw_parts(ptr as _, len as _);
        String::from_utf8_lossy(slice)
    };
    // pass the value to `double` and 
    // return the result as a pointer
    // 将值传递给 `double` 并返回指针形式的结果
    double(&value).as_ptr() as i32
}
```

>这里发生的大部分的事情跟我们第一篇所实现的完全相同，唯一不同的是在最后一行中添加了 `.as_ptr()`，返回值现在是 `i32`。`as_ptr` 是一个方法，它将返回一个值在内存中的索引位置（字节索引），一般来讲这是一件可怕的事情，但我保证我们不会发生问题。那么我们该如何使用这个新插件呢？

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
    // 首先，我们获取模块的上下文
    // 不同的地方从这里开始
    let context = instance.context();
    // 然后我们从上下文的 web 程序中获得 0 开始索引的内存，目前只支持一块内存，所以它一直是 0
    let memory = context.memory(0);
    // 现在我们可以获取内存的 view
    let view = memory.view::<u8>();
    // 这是我们要传递到 wasm 中的字符串
    let s = "supercalifragilisticexpialidocious".to_string();
    // 将字符串转为字节数组
    let bytes = s.as_bytes();
    // 获取字节数组的长度
    let len = bytes.len();
    // 循环遍历 wasm 内存的 view 的字节和字符串字节
    for (cell, byte) in view[1..len + 1].iter().zip(bytes.iter()) {
        // 将每个 wasm 内存字节设置为字符串字节的值
        cell.set(*byte)
    }
    // 绑定辅助函数
    let double = instance.func::<(i32, u32), i32>("_double").expect("Failed to bind _double");
    // 调用辅助函数，返回字符串的开头放到 start 中
    let start = double.call(1 as i32, len as u32).expect("Failed to execute _double") as usize;
    // 计算 “start + 两倍长的 len” 的和
    let end = start + (len * 2);
    // 从 wasm 内存的新 view 中捕获字符串，并将其转换为字节
    let string_buffer: Vec<u8> = memory
                                    .view()[start..end]
                                    .iter()
                                    .map(|c|c.get())
                                    .collect();
    // 将字节数组转换为字符串
    let wasm_string = String::from_utf8(string_buffer)
                            .expect("Failed to convert wasm memory to string");
    println!("doubled: {}", wasm_string);
}
```

同样的，几乎所有代码都在后面的例子中复用。我们需要稍微改变 `func` 的类型参数和名称。接下来我们将调用 `func` 将想我们上次做的那样，这次的返回值将代表新字符串的开始索引。由于我们只会将字符串翻倍，所以我们可以通过将原始长度的两倍加上起始长度来计算结束的位置，在起始部分和结束部分我们可以捕获字节的一部分。如果你有一个字节切片，你可以尝试使用 `String::from_utf8` 方法将其转换为字符串。如果我们运行上面的代码，可以看到如下结果：

```shell
cargo run
doubled: supercalifragilisticexpialidocioussupercalifragilisticexpialidocious
```

Huzzah! Success... though the situations where you would know the size of any data after a plugin ran is going to be too small to be useful. Now the big question becomes, if web assembly functions can only return 1 value how could we possibly know both the start and the length of any value coming back? One solution would be to reserve a section of memory that the wasm module could put the length in and then get the length when it's done.
>万岁！成功了。。。尽管你知道插件运行后数据的大小是很小的。但现在最大的问题是，如果 web assembly 组装的函数只能返回一个值，我们怎么可能同时知道返回值的起始位置和长度呢？一种解决方案是，为 wasm 模块保留一段内存，让它输入长度，然后在完成后获得对应的长度。

### Two values from one function
>一个函数返回多个值

Let's keep the same basic structure of our last plugin, this time though, we are going to get the length from a reserved part of memory.
>我们保持上一个插件示例代码的基本结构，这一次，我们将从内存的保留部分获取长度。

```rust
pub fn double(s: &str) -> String {
    s.repeat(2)
}

#[no_mangle]
pub fn _double(ptr: i32, len: u32) -> i32 {
    // 从内存中获取字符串
    let value = unsafe { 
        let slice = ::std::slice::from_raw_parts(ptr as _, len as _);
        String::from_utf8_lossy(slice)
    };
    // 将其变成双倍字符串
    let ret = double(&value);
    // 获取长度
    let len = ret.len() as u32;
    // 将长度写入索引为 1 的内存字节中
    unsafe {
        ::std::ptr::write(1 as _, len);
    }
    // 返回起始索引
    ret.as_ptr() as _
}
```

This time in our plugin we have one change, the call to [`::std::ptr::write`](https://doc.rust-lang.org/std/ptr/fn.write.html), which will write to any place in memory you tell it to any value you want. This is a pretty dangerous thing to do, it is important that we have all our ducks in a row or we may corrupt some existing memory. This is going to write the 4 bytes that make up the variable `len` into memory at index 1, 2, 3, and 4. The key to making that work is that we are going to need to leave those 4 bytes empty when we insert our value from the runner.
>在这次的插件中，我们一个不同的地方，即调用 [`::std::ptr::write`](https://doc.rust-lang.org/std/ptr/fn.write.html)，它可以将数据写入到内存中的任何地方，只要你告诉它写入的值和位置。这是一个比较危险的事，要非常谨慎的考虑好所有问题，否则可能会破坏现有的内存和数据。现在我们要把 4 字节的 `len` 变量写入到索引为 1、2、3 和 4 的内存中。让这个过程正常工作的关键是，当我们从有 runner 中插入值时，需要将这 4 字节的内存保留为空。

我们构建一下程序。

```shell
cargo -p example-plugin --target wasm32-unknown-unknown
```

Now we can get started on the runner.
>现在我们开始写 runner。

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
    // 不同点从这里开始，首先我们获取模块的上下文
    let context = instance.context();
    // 然后我们从上下文 web assembly 中获得编号为 0 的内存，目前只支持 1 块内存，所以编号为 0
    let memory = context.memory(0);
    // 现在获取内存的 view（视图）
    let view = memory.view::<u8>();
    // 用 0 填充开始的 4 个字节
    for cell in view[1..5].iter() {
        cell.set(0);
    }
    // 这是我们要传递到 wasm 中的字符串
    let s = "supercalifragilisticexpialidocious".to_string();
    // 将字符串转为字节数组
    let bytes = s.as_bytes();
    // 获取字节数组的长度
    let len = bytes.len();
    // 循环遍历 wasm 内存视图的字节和字符串字节
    for (cell, byte) in view[5..len + 5].iter().zip(bytes.iter()) {
        // 将每个 wasm 字节的值对应设为字符串字节的值
        cell.set(*byte)
    }
    // 绑定辅助函数
    let double = instance.func::<(i32, u32), i32>("_double").expect("Failed to bind _double");
    // 调用辅助函数返回存储字符串的起始内存位置
    let start = double.call(5 as i32, len as u32).expect("Failed to execute _double") as usize;
    // 获取更新后的内存 view
    let new_view = memory.view::<u8>();
    // 设置一个新的变量，存储数据转换后的新的长度
    let mut new_len_bytes = [0u8;4];
    for i in 0..4 {
        // 如果可以，尝试通过新的 view 的索引（1、2、3、4），返回它对应的数据，否则默认返回 0
        new_len_bytes[i] = new_view.get(i + 1).map(|c| c.get()).unwrap_or(0);
    }
    // 将 4 字节转换为 u32 并强制转换为 usize
    let new_len = u32::from_ne_bytes(new_len_bytes) as usize;
    // 计算 end 值为 start + new_len
    let end = start + new_len;
    // 从 wasm 内存新 view 中以字节的形式获取字符串
    let string_buffer: Vec<u8> = new_view[start..end]
                                    .iter()
                                    .map(|c|c.get())
                                    .collect();
    // 将字节转换为字符串
    let wasm_string = String::from_utf8(string_buffer)
                            .expect("Failed to convert wasm memory to string");
    println!("doubled: {}", wasm_string);
}
```

Ok, a few more things are going on in this one. First we immediately update the memory's bytes 1 through 4 to be set to 0, this is where we are going to put the new length. We continue normally until after we call `_double`. This time through we are going to pull those first 4 bytes out of the wasm memory into a 4 byte array and convert that to a u32. We need to cast this u32 to a usize because we are going to be using it in as an index later. We can now update our `end` to use this new value instead of the old one. From that point on we keep going the same way. If we were to run this we should see the following.
>好了，在上方这个示例中有还会有更多东西。首先，我们更新内存中 1-4 的字节设为 0，这是我们要存放新的长度的地方。接着我们调用 `_double`。这一次，我们把 wasm 内存中的前 4 个字节转换成 4 字节数组，然后再转换成 u32 类型。我们需要将这个 u32 转换为 usize，因为稍后我们要将其当做索引来使用。我们现在可以更新 `end` 来使用新的值。从那一刻起，我们持续这样做。运行这段代码，我们会看到如下结果：

```shell
cargo run
doubled: supercalifragilisticexpialidocioussupercalifragilisticexpialidocious
```

Huzzah! Success... and it is far more robust that before. If we executed a wasm module that exported `_double` that actually tripled a string or cut the string in half, we would still know the correct length. Now that we can pass arbitrary sets of bytes from rust to wasm and back again that means we have to tools to pass more complicated data. All we need now is a way to turn any struct into bytes and then back again, for that we can use something like [`bincode`](https://github.com/TyOverby/bincode) which is a binary serialization format used by [WebRender](https://github.com/servo/webrender) and [Servo's ipc-channel](https://github.com/servo/ipc-channel). It implements the traits defined by the serde crate which greatly opens our options.
>万岁！成功了。。。它比之前更强大。如果我们执行一个导出的 `_double` wasm 模块，这个模块实际上是将一个字符串扩充至原来的三倍，或者将字符串减半，我们仍然知道正确的长度。现在，我们可以将任意的字节从 rust 传递到 wasm，然后再返回，这意味着我们必须使用工具来处理更复杂的数据。我们现在需要的是一种把任何结构转换成字节并返回的方法实现，我们可以使用类似 [`bincode`](https://github.com/TyOverby/bincode) （一个用于二进制格式序列化的库，已经用于 [WebRender](https://github.com/servo/webrender) 和 [Servo's ipc-channel](https://github.com/servo/ipc-channel)）。它实现了 serde crate 中定义的很多 trait。这极大地扩充了我们的选择。

Since there are a bunch of `serde` trait implementations for a bunch of standard rust types including strings and tuples, let's leverage that to create a slightly more interesting example.
>因为 `serde` crate 中已经为一些标准的 rust 类型实现了很多 trait，包括字符串和元组，我们可以利用它来做一个稍微有趣的例子。

### Slightly More Interesting™
>更加有趣

First we want to update the dependencies for both our runner and plugin projects. Update the 2 Cargo.toml files to look like this.
>首先我们想要更新插件中 runner 和 plugin 依赖项。更新 2 个 Cargo.toml 文件。如下所示：

```toml
# ./crates/example-runner/Cargo.toml
[package]
name = "example-runner"
version = "0.1.0"
authors = ["rfm <r@robertmasen.pizza>"]
edition = "2018"

[dependencies]
wasmer-runtime = "0.3.0"
bincode = "1"
```

```toml
# ./crates/example-plugin/Cargo.toml
[package]
name = "example-plugin"
version = "0.1.0"
authors = ["rfm <r@robertmasen.pizza>"]
edition = "2018"

[dependencies]
bincode = "1"

[lib]
crate-type = ["cdylib"]
```

Now we can use bincode both of these projects. This time around, the goal is going to be to create a plugin that will take a tuple of a u8 and a string and return an updated version of that tuple.
>现在我们可以使用这两个项目的二进制代码。这一次，我们的目标是创建一个插件，它使用 u8 和字符串构成的元组为参数，并返回更新后的元组。

```rust
// ./crates/example-plugin/src/lib.rs
use bincode::{deserialize, serialize};
/// 如果这是纯 rust 交互，我们的实际代码如下
pub fn multiply(pair: (u8, String)) -> (u8, String) {
    // 根据提供的数据中的 String 部分，创建一个副本
    let s = pair.1.repeat(pair.0 as usize);
    // 将 u8 的数据乘以新字符串的长度
    let u = pair.0.wrapping_mul(s.len() as u8);
    (u, s)
}

/// 因为这不是我们需要的方法，我们需要的是从 wasm 转换到 rust 的实现代码
#[no_mangle]
pub fn _multiply(ptr: i32, len: u32) -> i32 {
    // 从内存中提取字符串
    let slice = unsafe { 
        ::std::slice::from_raw_parts(ptr as _, len as _)
    };
    // 将内存中的 slice 数据反序列化
    let pair = deserialize(slice).expect("Failed to deserialize tuple");
    // 获取新版本的数据
    let updated = multiply(pair);
    // 将新的值序列化
    let ret = serialize(&updated).expect("Failed to serialize tuple");
    // 获取数据的长度
    let len = ret.len() as u32;
    // 将长度写入到内存块的第一字节
    unsafe {
        ::std::ptr::write(1 as _, len);
    }
    // 返回起始地址索引
    ret.as_ptr() as _
}
```

Just like last time time we take in our `ptr` and `len` arguments, we pass those along to `::std::slice::from_raw_parts` which creates a reference to our bytes. After we get those bytes we can deserialize them into a tuple of a u8 and a string. Now we can pass that tuple along to the `multiply` function and capture the results as `updated`. Next we are going to serialize that value into a `Vec<u8>` and as the variable `ret`. The rest is going to be exactly like our string example, capture the length, write it to memory index 1 and return the start index of the bytes. Let's build this.
>就像上次我们接收 `ptr` 和 `len` 参数那样，我们将其传递给 `::std::slice::from_raw_parts`，这样会创建字节的引用。拿到这些字节后，我们可以将他们反序列化为 u8 和字符串组成的元组。现在我们可以将元组传递给 `multiply` 函数，并获取结果 `updated`。接下来，我们将该值序列化为 `Vec<u8>` 并作为变量 `ret`。其余部分将与我们的字符串示例完全相同，捕获长度，将其写入索引为 1 的内存中并返回字节的起始索引。我们编译一下试试。

```shell
cargo -p example-plugin --target wasm32-unknown-unknown
```

Now for our runner.
>现在要实现我们的 runner。

```rust
// ./crates/example-runner/src/main.rs
use wasmer_runtime::{
    imports,
    instantiate,
};

use std::time::{
    UNIX_EPOCH,
    SystemTime,
};

use bincode::{
    deserialize,
    serialize,
};

// 现在我们要读取 wasm 字节
static WASM: &[u8] = include_bytes!("../../../target/wasm32-unknown-unknown/debug/example_plugin.wasm");

fn main() {
    let instance = instantiate(&WASM, &imports!{}).expect("failed to instantiate wasm module");
    // 不同从这里开始
    // 首先我们获取模块的上下文
    let context = instance.context();
    // 然后我们从 web assembly 的上下文获取编号为 0 的内存，因为暂时只支持一块内存，所以这个总是 0。
    let memory = context.memory(0);
    // 现在我们可以获取内存的 view
    let view = memory.view::<u8>();
    // 前 4 个字节的内存用 0 填充
    for cell in view[1..5].iter() {
        cell.set(0);
    }
    // 这是我们要传递到 wasm 中的字符串
    let s = "supercalifragilisticexpialidocious".to_string();
    let now = SystemTime::now();
    let diff = now.duration_since(UNIX_EPOCH).expect("Failed to calculate timestamp");
    let u = ((diff.as_millis() % 10) + 1) as u8;
    let pair = (u, s);
    let bytes = serialize(&pair).expect("Failed to serialize tuple");
    // 字节的长度
    let len = bytes.len();
    // 循环遍历 wasm 内存中 view 字节和字符串字节
    for (cell, byte) in view[5..len + 5].iter().zip(bytes.iter()) {
        // 将每个 wasm 内存字节设置为对应字符串字节的值
        cell.set(*byte)
    }
    // 绑定辅助函数
    let double = instance.func::<(i32, u32), i32>("_multiply").expect("Failed to bind _multiply");
    // 调用辅助函数并将返回的字符串存储在 start 中
    let start = double.call(5 as i32, len as u32).expect("Failed to execute _multiply") as usize;
    // Get an updated view of memory
    // 获取更新的内存 view
    let new_view = memory.view::<u8>();
    // Setup the 4 bytes that will be converted
    // into our new length
    // 设定一个 4 字节的空间，用于存储我们的长度数据
    let mut new_len_bytes = [0u8;4];
    for i in 0..4 {
        // 如果可以，尝试从内存 view 的 (1,2,3,4) 获取 i+1 的值，否则返回 0
        new_len_bytes[i] = new_view.get(i + 1).map(|c| c.get()).unwrap_or(0);
    }
    // 将 4 个字节类型转换为 u32 再转换为 usize
    let new_len = u32::from_ne_bytes(new_len_bytes) as usize;
    // 用 start + new_len 计算 end 值
    let end = start + new_len;
    // 从 wasm 内存的 view 中获取字符串并转换为字节数组 
    let updated_bytes: Vec<u8> = new_view[start..end]
                                    .iter()
                                    .map(|c|c.get())
                                    .collect();
    // 将字节数组转换为字符串
    let updated: (u8, String) = deserialize(&updated_bytes)
                            .expect("Failed to convert wasm memory to tuple");
    println!("multiply {}: ({}, {:?})", pair.0, updated.0, updated.1);
}
```

First, we have updated our `use` statements to include some `std::time` items and the bincode functions for serializing and deserializing. We are going to use the same string as we did last time and calculate a pseudo random number between 1 and 10 that will serve as the parts of our tuple. Once we have constructed our tuple, we pass that off to `bincode::serialize` which gets us back to a `Vec<u8>`. We continue on just like our string example until after we get the new length back from the wasm module. At this point we are going to build the updated_bytes the same as before and pass those along to `bincode::deserialize` which should get us back to a tuple.
>首先，我们更新了 `use` 语句，用来包含 `std::time` 项和序列化、反序列化的二进制代码函数。我们将使用和前面一样的字符串参数，计算 1~10 之间的伪随机数，该随机数作为元组的一部分，一旦我们构建好了元组，我们将其传递给 `bincode::serialize`，并返回 `Vec<u8>` 类型数据。后面，就像我们的字符串示例一样，继续操作，直到从 wasm 模块中获得新的长度为止。此时，我们将构建和之前一样的 updated_bytes，并将其传递给 `bincode::deserialize` 函数，最终得到元组类型的返回值。

```shell
cargo run
multiply 2: (136, "supercalifragilisticexpialidocioussupercalifragilisticexpialidocious")
```

Huzzah! Another success! At this point it might be a good idea to address the ergonomics all of this, if we asked another developer to understand all of this, do you think anyone would build a plugin for our system? Probably not. In the next post we are going to cover how to ease that process by leveraging `proc_macros`.
>万岁！一个新的成功！在这一点上，对于解决人体工程学这是一个好的方法，如果我们询问另一个开发者了解这个方式，你认为你能像这样清楚的了解如何构建一个插件吗？可能不会。在下一篇中，我们将介绍如何通过 `proc_macros` 来简化这个过程。

[第三部分](https://wiredforge.com/blog/wasmer-plugin-pt-3/index.html)
