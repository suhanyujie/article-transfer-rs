# 【译】使用 Wasmer 开发插件-第三部分
>* Using Wasmer for Plugins Part 3 译文

>* 原文链接：https://wiredforge.com/blog/wasmer-plugin-pt-3/index.html
>* 原文作者：[Robert Masen](https://github.com/freemasen)
>* 译文来自：https://github.com/suhanyujie/article-transfer-rs
>* 译者：[suhanyujie](https://www.github.com/suhanyujie)

* [第一部分](part1.md)
* [第二部分](part2.md)
* 第三部分

In the last two posts of this series we covered all of the things we would need to use [`Wasmer`](http://wasmer.io/) as the base for a plugin system. In [part one](https://wiredforge.com/blog/wasmer-plugin-pt-1/index.html) we went over the basics of passing simple data in and out of a web assembly module, in [part two](https://wiredforge.com/blog/wasmer-plugin-pt-2/index.html) we dug deeper into how you might do the same with more complicated data. In this part we are going to explore how we might ease the experience for people developing plugins for our application.

The majority of this is going to happen in a `proc_macro`, if you have never built one of these before, it can seem intimidating but we will go slow so don't fret. The first thing to understand is that `proc_macros` are meta-programming, meaning we are writing code that writes code. Currently there are 3 options to chose from when writing a `proc_macro` but they all follow the same basic structure; a function that will take `TokenStream`s as arguments and return a `TokenStream`. A `TokenStream` is a collection of rust language parts, for example a keyword like `fn` or punctuation like `{`. It is almost like we are getting the text from a source file and returning a modified version of that text, though we get the added benefit of the fact that `rustc` is going to have validated it at least knows all of the parts in that text and will only let us add parts to it that it knows. To make this whole process a little easier, we are going to lean on a few crates pretty heavily, they are [`syn`](https://crates.io/crates/syn), [`proc-macro2`](https://crates.io/crates/proc-macro2), and `quote`. `syn` is going to parse the `TokenStream` into a structure that has more information, it will help answer questions like 'is this a function?' or 'is this function public?'. Many parts of that's structure are provided by `proc-macro2`. `quote` is going to help us create a `TokenStream` by "quasi-quoting" some rust text, we'll get into what that means in just a moment.

Now that we have our dependencies outlined, let's talk about the three types of `proc_macro`s. First we have a custom derive, if you have ever use the `#[derive(Serialize)]` attribute, you have used a custom derive. For these, we need to define a function that takes a single `TokenStream` argument and returns a new TokenStream, this return value will be append to the one passed in. That mean's we can't modify the original code, only augment it with something like an `impl` block, which makes it great for deriving a trait. Another option is often referred to as _function like_ macros, these look just like the macros created with `#[macro_rules]` when used but are defined using a similar system to the custom derives. The big difference between custom derives and function like macros is the return value for the latter is going to replace the argument provided, not extend it. Lastly we have the _attribute like_ macros, this is the one we are going to use. Attribute macros work the same as function like macros in that they will replace the code provided. The big difference is that an attribute definition the function we write will take 2 arguments, the first of which will be the contents of the attribute and the second is what that attribute is sitting on top of. To use the example from the rust book


