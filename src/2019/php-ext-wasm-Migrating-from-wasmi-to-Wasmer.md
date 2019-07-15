# 【译】php-ext-wasm：从 wasmi 迁移到 Wasmer
>php-ext-wasm: Migrating from wasmi to Wasmer 译文

>* 原文地址：https://medium.com/wasmer/php-ext-wasm-migrating-from-wasmi-to-wasmer-4d1014f41c88
>* 原文仓库：https://github.com/wasmerio/php-ext-wasm
>* 原文作者：[Ivan Enderlin](https://medium.com/@hywan)
>* 译文出自：https://github.com/suhanyujie
>* 本文永久链接：（缺省）
>* 译者：[suhanyujie](https://github.com/suhanyujie)
>* 翻译不当之处，还请指出，谢谢！
>* 这是一个 PHP 扩展相关的文章，结合了 Rust WebAssembly PHP 等前沿的技术，旨在给开发者更多的参考和思路。

>导读：php-ext-wasm 是如何从 wasmi 迁移到 Wasmer 并提高了 29 倍的速度，比原生的 PHP 更快，并且接近于机器码的速度。

开头我开了一个玩笑，现在我开始将 [`php-ext-wasm`](https://github.com/wasmerio/php-ext-wasm) 作为一个真正的产品来开发：一个可以执行 [WebAssembly](https://webassembly.org/) 二进制文件的 PHP 扩展。

PHP 虚拟机（VM）即 [Zend Engine](https://github.com/php/php-src/)。要编写扩展，需要使用 C 或者 C++ 进行开发。扩展是 Rust 实现的简单的 C 绑定库。当时，这个 Rust 库使用 [wasmi](https://github.com/paritytech/wasmi) 作为 WebAssembly 的虚拟机。我知道 `wasmi` 不是这个场景中最快的 WebAssembly 虚拟机，但它的 API 是可靠的、经过测试的、编译速度快，并且易于 hack。基于这些，我们开始写一个项目吧!

经过 6 小时的开发，我得到了一些有用的东西。可以运行以下 PHP 程序：

```php
<?php
$instance = new Wasm\Instance('simple.wasm');
$result = $instance->sum(1, 2);
var_dump($result); // int(3)
```

API 很简单：创建一个实例（这里是 `simple.wasm`），然后调用它的函数（这里的 `sum`，参数是 1 和 2）。PHP 值将被自动转换为 WebAssembly 中的值。郑重声明，以下是 Rust 程序文件 `simple.rs`，它被编译为一个 WebAssembly 二进制文件：

```rust
#[no_mangle]
pub extern fn sum(x: i32, y: i32) -> i32 {
    x + y
}
```

太棒了！在我看来，6 小时能得到这样的成果还是很划算的。

然而，我很快就注意到 `wasmi` 很慢。[WebAssembly 的优势]((https://webassembly.org/))之一是：

>WebAssembly 旨在充分利用广泛平台上可用的[共用硬件功能](https://webassembly.org/docs/portability/#assumptions-for-efficient-execution)以机器码的速度执行程序。

很明显，我的扩展没有这个优势。让我们看看基准测试比较。

我从 Debian 的[计算机语言基准测试游戏](https://benchmarksgame-team.pages.debian.net/benchmarksgame/)中选择了 [n-body 算法](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/nbody.html)。并且相对来讲，它属于 CPU 密集型算法。该算法具有简单的接口：基于整数返回浮点数；这个 API 不涉及任何高级实例的内存 API，这对于测试一个“概念验证”来说是很好的。

作为参考，我运行了 n-body 算法[ Rust 编写](https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/nbody-rust-7.html)，我们称之为 `rust-baseline`。同样的算法[用 PHP 实现](https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/nbody-php-3.html)，我们称之为 `php`。最后，将算法从 Rust 编译为 WebAssembly，并使用 `php-ext-wasm` 扩展执行，我们暂且将这种场景称为 `php+wasmi`。所有测试都是基于 `nbody(5000000)`：

* `rust-baseline`: 287ms,
* `php`: 19,761ms,
* `php+wasmi`: 67,622ms.

好的，那么…… 使用 `wasmi` 的 `php-ext-wasm` 比原生 PHP 慢 3.4 倍，对于这种低性能的结果来说，使用 WebAssembly 是没有意义的。

不过，它证实了我的第一直觉：早我们的例子中，`wasmi` 确实可以很好的模拟一些东西，但它还不够快，不符合我们的预期。

## 再快写些，再快写些，再快写些……
从一开始我就想使用 [Cranelift](https://github.com/CraneStation/cranelift)。它是一个代码生成器，类似于 [LLVM](http://llvm.org/)（别介意我用这种简写，我们的目标不是详细解释 Cranelift，但它确实是一个很好的项目！）引用项目本身的描述：

>Cranelift 是一个底层的可重定向的代码生成器。它将[与目标无关的中间表示形式]转换为可执行的机器码。

这基本上意味着可以使用 Cranelift API 生成可执行代码。

这个方案很不错！基于 Cranelift 带来的好处,我可以用它替换 `wasmi`。但是，还有其他方法可以获得更快的代码执行速度 —— 但代价是需要更长的时间编译和调试代码。

例如，LLVM 可以提供非常快的代码执行速度，几乎可以达到机器码的执行速度。或者我们可以动态生成汇编代码。有很多方法可以做到这一点。假如一个项目可以提供一个具有多个后端 WebAssembly 虚拟机的方法，该怎么办？

## 进入 Wasmer
就在那个时候，我被 [Wasmer](https://github.com/wasmerio/wasmer) 录用了。说实话，几周前我还在关注 Wasmer 呢。这对我来说是惊喜，也是非常好的机会。大家都希望从 wasmi 到 `Wasmer` 进行重写，是吗 😅？

Wasmer 是一些 Rust 库（叫做 crate）组成的。甚至有一个 `wasmer-runtime-c-api` crate 是用 C 和 C++ API 并基于 `wasmer-runtime` crate 和 `wasmer-runtime-core` crate 来实现的。它可以运行 WebAssembly 虚拟机，后端的可选择方案是：Cranelift，LLVM，或者 Dynasm（在撰写本文时发现的）。很好，它在 PHP 扩展和 `wasmi` 之间移除了我的 Rust 库。`php-ext-wasm` 被简化为一个不带有 Rust 代码的 PHP 扩展，所有问题都转向了 `wasmer-runtime-c-api`。这个项目中移除了 Rust 比较令人遗憾，但它依赖了更多其他的 Rust 代码！

在给 `wasmer-runtime-c-api` 打补丁时估算了一下时间，我差不多能够在 5 天内将 `php-ext-wasm` 迁移到 Wasmer。

默认情况下，`php-ext-wasm` 使用 Wasmer 和 Cranelift 后端，它在编译和执行时间之间取得了平衡。很棒！我们加上 `php+wasmer(cranelift)` 方案，然后进行基准测试：

* `rust-baseline`: 287ms,
* `php`: 19,761ms,
* `php+wasmi`: 67,622ms,
* `php+wasmer(cranelift)`: 2,365ms 🎉.

最后，PHP 扩展的方案的测试结果显示性能比原生 PHP 代码更好！`php+wasmer(cranelift)` 很明显比 `php` 快 8.6 倍。比 `php+wasmi` 快 28.6 倍。有方案能达到机器码速度（这里代表 `rust-baseline`）吗？很有可能是 LLVM。这是另一篇文章的内容。我现在很高兴使用了 Cranelift。（看[我们之前的博客文章，了解如何在 Wasmer 和其他 WebAssembly 运行时测试不同的后端](https://medium.com/wasmer/benchmarking-webassembly-runtimes-18497ce0d76e)。）

## More Optimizations
Wasmer provides more features, like module caching. Those features are now included in the PHP extension. When booting the `nbody.wasm` file (19kb), it took 4.2ms. By booting, I mean: reading the WebAssembly binary from a file, parsing it, validating it, compiling it to executable code and a WebAssembly module structure.

PHP execution model is: starts, runs, dies. Memory is freed for each request. If one wants to use `php-ext-wasm`, you don’t really want to pay that “booting cost” every time.

Hopefully, wasmer-runtime-c-api now provides a module serialization API, which is integrated into the PHP extension itself. It saves the “booting cost”, but it adds a “deserialization cost”. That second cost is smaller, but still, we need to know it exists.

Hopefully again, Zend Engine has an API to get persistent in-memory data between PHP executions. `php-ext-wasm` supports that API to get persistent modules, et voilà.

Now it takes 4.2ms for the first boot of `nbody.wasm` and 0.005ms for all the next boots. It’s 840 times faster!

## Conclusion
Wasmer is a young — but mature — framework to build WebAssembly runtimes on top of. The default backend is Cranelift, and it shows its promises: It brings a correct balance between compilation time and execution time.

`wasmi` has been a good companion to develop a Proof-Of-Concept. This library has its place in other usages though, like very short-living WebAssembly binaries (I’m thinking of Ethereum contracts that compile to WebAssembly for instance, which is one of the actual use cases). It’s important to understand that no runtime is better than another, it depends on the use case.

The next step is to stabilize `php-ext-wasm` to release a 1.0.0 version.

See you there!

If you want to follow the development, take a look at [@wasmerio](https://twitter.com/wasmerio) and [@mnt_io](https://twitter.com/mnt_io) on Twitter.
