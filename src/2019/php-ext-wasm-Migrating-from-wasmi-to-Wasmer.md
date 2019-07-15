# 【译】php-ext-wasm：从 wasmi 到 Wasmer
>php-ext-wasm: Migrating from wasmi to Wasmer 译文

>* 原文地址：https://medium.com/wasmer/php-ext-wasm-migrating-from-wasmi-to-wasmer-4d1014f41c88
>* 原文仓库：https://github.com/wasmerio/php-ext-wasm
>* 原文作者：[Ivan Enderlin](https://medium.com/@hywan)
>* 译文出自：https://github.com/suhanyujie
>* 本文永久链接：（缺省）
>* 译者：[suhanyujie](https://github.com/suhanyujie)
>* 翻译不当之处，还请指出，谢谢！
>* 这是一个 PHP 扩展相关的文章，结合了 Rust WebAssembly PHP 等前沿的技术，旨在给开发者更多的参考和思路。

>导读：How php-ext-wasm has migrated from wasmi to Wasmer and now enjoys a 29x speedup, is faster than PHP itself, and is closer to native speed.

First as a joke, now as a real product, I started to develop [`php-ext-wasm`](https://github.com/wasmerio/php-ext-wasm): a PHP extension allowing to execute [WebAssembly](https://webassembly.org/) binaries.

The PHP virtual machine (VM) is [Zend Engine](https://github.com/php/php-src/). To write an extension, one needs to develop in C or C++. The extension was simple C bindings to a Rust library I also wrote. At that time, this Rust library was using [wasmi](https://github.com/paritytech/wasmi) for the WebAssembly VM. I knew that `wasmi` wasn’t the fastest WebAssembly VM in the game, but the API is solid, well-tested, it compiles quickly, and is easy to hack. All the requirements to start a project!

After 6 hours of development, I got something working. I was able to run the following PHP program:

```php
<?php
$instance = new Wasm\Instance('simple.wasm');
$result = $instance->sum(1, 2);
var_dump($result); // int(3)
```

The API is straightforward: create an instance (here of `simple.wasm`), then call functions on it (here `sum` with 1 and 2 as arguments). PHP values are transformed into WebAssembly values automatically. For the record, here is the `simple.rs` Rust program that is compiled to a WebAssembly binary:

```rust
#[no_mangle]
pub extern fn sum(x: i32, y: i32) -> i32 {
    x + y
}
```

It was great! 6 hours is a relatively small number of hours to go that far according to me.

However, I quickly noticed that `wasmi` is… slow. [One of the promise of WebAssembly](https://webassembly.org/) is:

>WebAssembly aims to execute at native speed by taking advantage of [common hardware capabilities](https://webassembly.org/docs/portability/#assumptions-for-efficient-execution) available on a wide range of platforms.

And clearly, my extension wasn’t fulfilling this promise. Let’s see a basic comparison with a benchmark.

I chose the [n-body algorithm](https://benchmarksgame-team.pages.debian.net/benchmarksgame/description/nbody.html) from [the Computer Language Benchmarks Game](https://benchmarksgame-team.pages.debian.net/benchmarksgame/) from Debian, mostly because it’s relatively CPU intensive. Also, the algorithm has a simple interface: based on an integer, it returns a floating-point number; this API doesn’t involve any advanced instance memory API, which is perfect to test a proof-of-concept.

As a baseline, I’ve run the n-body algorithm [written in Rust](https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/nbody-rust-7.html), let’s call it `rust-baseline`. The same algorithm has been [written in PHP](https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/nbody-php-3.html), let’s call it `php`. Finally, the algorithm has been compiled from Rust to WebAssembly, and executed with the `php-ext-wasm` extension, let’s call that case `php+wasmi`. All results are for `nbody(5000000)`:

* `rust-baseline`: 287ms,
* `php`: 19,761ms,
* `php+wasmi`: 67,622ms.

OK, so… `php-ext-wasm` with `wasmi` is 3.4 times slower than PHP itself, it is pointless to use WebAssembly in such conditions!

It confirms my first intuition though: In our case, `wasmi` is really great to mock something up, but it’s not fast enough for our expectations.

## Faster, faster, faster…
I wanted to use [Cranelift](https://github.com/CraneStation/cranelift) since the beginning. It’s a code generator, à la [LLVM](http://llvm.org/) (excuse the brutal shortcut, the goal isn’t to explain what Cranelift is in details, but that’s a really awesome project!). To quote the project itself:

>Cranelift is a low-level retargetable code generator. It translates a [target-independent intermediate representation](https://cranelift.readthedocs.io/en/latest/ir.html) into executable machine code.

It basically means that the Cranelift API can be used to generate executable code.

It’s perfect! I can replace `wasmi` by Cranelift, and boom, profit. But… there is other ways to get even faster code execution — at the cost of a longer code compilation though.

For instance, LLVM can provide a very fast code execution, almost at native speed. Or we can generate assembly code dynamically. Well, there is multiple ways to achieve that. What if a project could provide a WebAssembly virtual machine with multiple backends?

## Enter Wasmer
And it was at that specific time that I’ve been hired by [Wasmer](https://github.com/wasmerio/wasmer). To be totally honest, I was looking at Wasmer a few weeks before. It was a surprise and a great opportunity for me. Well, the universe really wants this rewrite from wasmi to `Wasmer`, right 😅?

Wasmer is organized as a set of Rust libraries (called crates). There is even a `wasmer-runtime-c-api` crate which is a C and a C++ API on top of the `wasmer-runtime` crate and the `wasmer-runtime-core` crate, i.e. it allows running the WebAssembly virtual machine as you want, with the backend of your choice: Cranelift, LLVM, or Dynasm (at the time of writing). That’s perfect, it removes my Rust library between the PHP extension and `wasmi`. Then `php-ext-wasm` is reduced to a PHP extension without any Rust code, everything goes to `wasmer-runtime-c-api`. That’s sad to remove Rust from this project, but it relies on more Rust code!

Counting the time to make some patches on `wasmer-runtime-c-api`, I’ve been able to migrate `php-ext-wasm` to Wasmer in 5 days.

By default, `php-ext-wasm` uses Wasmer with the Cranelift backend, it does a great balance between compilation and execution time. It is really good. Let’s run the benchmark, with the addition of `php+wasmer(cranelift)`:

* `rust-baseline`: 287ms,
* `php`: 19,761ms,
* `php+wasmi`: 67,622ms,
* `php+wasmer(cranelift)`: 2,365ms 🎉.

Finally, the PHP extension provides a faster execution than PHP itself! `php+wasmer(cranelift)` is 8.6 times faster than `php` to be exact. And it is 28.6 times faster than `php+wasmi`. Can we reach the native speed (represented by `rust-baseline` here)? It’s very likely with LLVM. That’s for another article. I’m super happy with Cranelift for the moment. (See [our previous blog post to learn how we benchmark different backends in Wasmer, and other WebAssembly runtimes](https://medium.com/wasmer/benchmarking-webassembly-runtimes-18497ce0d76e)).

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
