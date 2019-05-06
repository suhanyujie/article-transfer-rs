# 用`Wasmer`进行插件开发1
* *原文链接 https://wiredforge.com/blog/wasmer-plugin-pt-1/index.html/*

* 几个月之前，[Wasmer](https://wasmer.io/) 团队发布了一个 `Web Assembly(aka wasm)` 解释器，用于rust程序的嵌入式开发。对于任何想要在项目中添加插件的人来说，这尤其令人兴奋，因为Rust提供了一种直接将程序编译到wasm的方法，这个应该是一个很好的选择。在这个系列的博客文章中，我们将研究如何使用wasmer和rust构建插件系统。

## 步骤
* 在我们深入研究细节之前，我们心里要想像一个这个项目的轮廓。如果你在你的电脑上继续学习，你可以做到；如果没有电脑，一切做起来可能显得没那么神奇。为此，我们将利用cargo的workspace特性，该特性可以让我们在一个父项目中聚合一组相关的项目。这里相关的代码你都能在[github仓库](https://github.com/FreeMasen/wiredforge-wasmer-plugin-code)上找到，每个分支代表这个系列的不同状态。我们要研究的基本结构是这样的：

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






