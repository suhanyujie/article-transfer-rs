# 使用Rust进行解析器学习
* 这是一篇翻译文：*原文链接 https://bodil.lol/parser-combinators/*
* 翻译[首发](https://github.com/suhanyujie/article-transfer-rs/blob/master/src/2019/Learning-Parser-Combinators-With-Rust.md)地址：https://github.com/suhanyujie/article-transfer-rs/blob/master/src/2019/Learning-Parser-Combinators-With-Rust.md
>正文开始：


* 本文面向会使用Rust编程的人员，提供一些解析器的基础知识。如果没有其他基础，将会解释和Rust无关的一些内容，并且，使用Rust实现这个会更加超出预期。如果你还不了解Rust，这个文章也不会讲如何使用Rust；如果你已经了解Rust，那它也不能打包票能教会您解析器知识。如果你想学习Rust，我推荐阅读[《The Rust Programming Language》](https://doc.rust-lang.org/book/)。

## 初学者的独白
* 在很多程序员的职业生涯中，可能都会有这样一个时刻，发现自己需要一个解析器。
* 小白程序员可能会问：解析器是什么？
* 中级程序员会问：这很简单，我会写正则表达式。
* 高级程序员会说：闪开，我知道 `lex` 和 `yacc` 。
* 只有小白的态度是端正的。
* 并不说正则不好。（但请不要尝试将一个复杂的解析器写成正则表达式。）也不是说使用像 `lexer` 生成器这种强大的工具就没有显得技术含量，这些工具经过长久的迭代和改进，已经达到了很好的程度。但从0开始学解析器是很有趣的。而如果你直接走正则表达式或解析器生成器的方向，你将会错过很多精彩的东西，因为他们只是对当前实际问题的抽象后形成的工具。正如[某人](https://en.wikipedia.org/wiki/Shunry%C5%AB_Suzuki#Quotations)所说，在初学者的脑袋中，是充满可能性的。而在专家的头脑中，可能就只习惯了那一种想法。

* 在本文中，我们将学习怎样从头开始使用函数式编程语言构建一个常用的解析器。他们具有很多的有点，一旦你掌握其中的基本思想，和基本原理，作为唯一的抽象，你将在基本组合器之上建立自己的抽象。所有这些必须建立在你使用它们之上。

## 怎样利用好这篇文章
* 强烈建议你新建一个新的Rust项目，并在阅读时，将代码片段键入到文件 `src/lib.rs` 中 （您可以直接从页面赋值代码片段，但最好手打，因为这样会确保你完整的阅读代码）。本文会按顺序介绍你需要的每一段代码。请注意，它可能会引入你之前编写的函数，这种情况下，你应该使用新版本的代码替换旧版本的。
* 代码是基于 `rustc 1.34.0` 版本的编译器的，它是2018大版本的。因此只要确保你使用的是2018（检查 `Cargo.toml` 是否包含了 `edition = "2018"` ）的大版本，你就能遵循使用更新的编译器。代码无需外部依赖。
* 如你所料，要运行文章中介绍的测试，可以使用 `cargo test`

## 折磨人的标记语言
* 我们将为简化版的 `XML` 编写一个解析器。它类似于这样：

```
<parent-element>
  <single-element attribute="value" />
</parent-element>
```

* `XML` 元素以符号 `<` 和一个标识符开始，标识符由若干字母、数字或 `-` 组成。其后是一些空格，或一些可选的属性列表：标识符后跟随一个 `=` 和双引号包含一些字符串。最后，可能有一个 `/>` 进行结束，表示没有子元素的单个元素，也可能有一个 `>` 表示后面有一些列子元素，最后一个结束标记以 `</` 开头，后面跟一个标识符，该标识符必须在与开始标识符标记匹配，最后是 `>` 。
* 这就是我们要做的。没有名称空间，没有文本节点，没有其他节点，而且绝对没有模式验证。我们甚至不需要为这些字符串支持转义符号——它们从第一个双引号开始，到下一个双引号结束，就是这样。如果你想要在实际的字符串中使用双引号，你可以将难处理的需求放到后面。
* 我们将把这些元素解析成类似于这样的结构：

```rust
#[derive(Clone, Debug, PartialEq, Eq)]
struct Element {
    name: String,
    attributes: Vec<(String, String)>,
    children: Vec<Element>,
}
```

* 没有泛型，只有一个带有名称的字符串（即每个标记开头的标识符）、一些字符串的属性（标识符和值），和一个看起来和父元素完全一样的子元素列表。
* （如果你正在键入代码，请确保包含这些 `derives` 。稍后你会需要用到的。）

## 定义解析器
* 那么，是时候开始编写解析器了。
* 解析是从数据流派生出结构的过程。解析器就是用来梳理结构的东西。
* 在我们将要探讨的规程中，解析器最简单的形式就是一个函数，它接收一些输入并返回已解析的输出和输入的其余部分，或者一个错误提示：“无法解析”。
* 简而言之，这也是解析器在更复杂的场景中也是这个样子。你可能会使输入、输出和错误复杂化，如果你要有好的错误信息，你需要它，但是解析器保持不变：处理输入并将解析的结果和其他输入的剩余内容进行输出，或者提示出它无法将输入解析并显示信息。
* 我们把它标记为函数类型

```rust
Fn(Input) -> Result<(Input, Output), Error>
```

* 更详细的说，在我们的例子中，我们要填充类型，就会得到类似下面的结果，因为我们要做的是将一个字符串转换成一个元素结构，这一点上，我们不想将错误复杂地显示出来，所以我们只将我们无法解析的错误作为字符串返回：

```rust
Fn(&str) -> Result<(&str, Element), &str>
```

* 我们使用字符串 slice ，因为它是指向一个字符串片段的有效指针，我们可以通过 slice 的方式引用它，无论怎么做，处理输入的字符串 slice ，并返回剩余内容和处理结果。
* 使用 `&[u8]` （一个字节的 slice ，假设我们限制自己使用 ASCII 对应的字符） 作为输入的类型可能会更简洁，特别是因为一个字符串 slice 的行为不同于其他类型的 slice ，尤其是在不能用数字索引对字符串进行索引的情况下，你必须像这样使用一个字符串 slice `input[0..1]` 。另一方面，对于解析字符串它们提供许多方法，而字符数组 slice 没有。
* 实际上，大多数情况下，我们将依赖这些方法，而不是对其进行索引，因为， `Unicode` 。在 utf-8 中，所有rust字符串都是 utf-8 的，这些索引并不能总是对应于单个字符，最好让标准库帮我们处理这个问题。

## 第一个解析器
* 让我们尝试编写一个解析器，它只查看字符串中的第一个字符，并判断它是否是字母 `a`。

```rust
fn the_letter_a(input: &str) -> Result<(&str, ()), &str> {
  match input.chars().next() {
      Some('a') => Ok((&input['a'.len_utf8()..], ())),
      _ => Err(input),
  }
}
```

* 首先，我们看下输入和输出的类型：我们将一个字符串 slice 作为输入，正如我们讨论的，我们返回一个包含 `(&str, ())` 的 `Result` 或者 `&str` 类型的错误。有趣的是 `(&str, ())` 这部分：正如我们所讨论的，我们应该返回一个能够分析下一个输入的结果的元组。 `&str` 是下一个输入，处理的结果则是单独的 `()` 类型，因为如果这个解析器成功运行，它将只能得到一个结果（找到了字母 `a` ），并且在这种情况下，我们不特别需要返回字母 `a` ，我们只需要指出已经成功的找到了它就行。

* 因此，我们看看解析器本身的代码。首先获取输入的第一个字符：`input.chars().next()` 。我们并没有尝试性的依赖标准库来避免带来 `Unicode` 的问题——我们调用它为字符串的字符提供的一个 `chars()` 迭代器，然后从其中取出第一个字符。这就是一个 `char` 类型的项，并且通过 `Option` 包装着，即 `Option<char>` ，如果是 `None` 类型的 `Option` 则意味着我们获取到的是一个空字符串。

* 更糟糕的是，获取到的字符甚至可能不是我们想象中的 `Unicode` 字符。这很可能就是 `Unicode` 中的 "[grapheme cluster](http://www.unicode.org/glossary/#grapheme_cluster)" ，它可以由几个 `char` 类型的字符组成，这些字符实际上表示 "[scalar values](http://www.unicode.org/glossary/#unicode_scalar_value)" ，它比 "grapheme cluster" 差不多还低2个层次。但是，这种方法未免也太激进了，就我们的目的而言，我们甚至不太可能看到 `ASCII` 字符集以外的字符，所以暂且不管这个问题。

* 我们匹配一下 `Some('a')`，这就是我们正在寻找的特定结果，如果匹配成功，我们将返回成功 `Ok((&input['a'.len_utf8]()..], ()))` 。也就是说，我们从字符串 slice 中移出的解析的项（ 'a' ），并返回其余的字符，以及解析后的值，也就是 `()` 类型。考虑到 `Unicode` 字符集问题，在对字符串 `slice` 前，我们用标准库中的方法查询一下字符 `a` 在 UTF-8 中的长度——长度是1，但绝不要去猜测 Unicode 字符长度。

* 如果我们得到其他类型的结果 `Some(char)` ，或者 `None` ，我们将返回一个 error 。正如之前提到的，我们现在的错误类型就是解析失败时的字符串 `slice` ，也就是我们我们传入的输入。它不是以 `a` 开头，所以返回错误给我们。这不是一个很严重的错误，但至少比“一些地方出了严重错误”要好一些。

* 实际上，尽管我们不需要这个解析器解析这个 `XML` ，但是我们需要做的第一件事是寻找开始的 `<` ，所以我们需要一些类似的东西。特别的，我们还需要解析 `>` ,`/` 和 `=` ，所以，也许我们可以创建一个函数来构建一个解析器来解析我们想要解析的字符。

## 解析器构建器
* 我们想象一下，如果要写一个函数：它可以为任意长度的静态字符串（不仅仅是单个字符）生成一个解析器。这样做甚至更简单一些，因为字符串 slice 是一个合法的 UTF-8 字符串 slice ，并且暂且不考虑 Unicode 字符集问题。

```rust
fn match_literal(expected: &'static str)
    -> impl Fn(&str) -> Result<(&str, ()), &str>
{
    move |input| match input.get(0..expected.len()) {
        Some(next) if next == expected => {
            Ok((&input[expected.len()..], ()))
        }
        _ => Err(input),
    }
}
```

* 现在看起来有点不一样了。
* 首先，看看类型，。我们的函数看起来不像一个解析器，它现在接受我们期望的字符串作为参数，并且返回值是看起来像解析器一样的东西。它是一个返回值是函数的函数——换句话说，它是一个高阶函数。基本上，我们在写的是生成一个像之前我们的 `the_letter_a` 一样的函数。

* 因此，我们不是在函数体中执行一些逻辑，而是返回一个闭包，这个闭包才是执行逻辑的地方，并且与之前的解析器的“函数签名”是匹配的。
* 匹配模式是一样的，只是我们不能直接匹配字符串文本，因为我们不知道他具体是什么，所以我们使用 `match` 和条件判断 `if next == expected` 来匹配。因此，它和之前完全一样，只是逻辑的执行是在闭包的内部。

## 测试解析器
* 我们将编写一个测试来确保我们做的是对的。

```rust
#[test]
fn literal_parser() {
    let parse_joe = match_literal("Hello Joe!");
    assert_eq!(
        Ok(("", ())),
        parse_joe("Hello Joe!")
    );
    assert_eq!(
        Ok((" Hello Robert!", ())),
        parse_joe("Hello Joe! Hello Robert!")
    );
    assert_eq!(
        Err("Hello Mike!"),
        parse_joe("Hello Mike!")
    );
}
```

* 首先，我们构建解析器： `match_literal("Hello Joe!")` 。这应该使用字符串 `Hello Joe!` 作为参数，并返回字符串的其余部分，否则它应该提示失败并返回整个字符串。
* 在第一种情况下，我们只是向他提供它期望的字符串作为参数，然后，我们看到它返回一个空字符串和 `()` 的值，这意味着：“我们按照正常流程解析了字符串，实际上你并不需要它返回给你这个值”。

* 在第二种情况下，我们给它输入字符串 `Hello Joe! Hello Robert!` ，并且我们确实看到它使用了字符串 `Hello Joe!` 并返回其余的输入：` Hello Robert!`(空格开头的所有字符串)
* 在第3个例子中，我们输入了一些不正确的值： `Hello Mike!`，请注意，它确实根据输入给出了错误并中断执行。一般来说， `Mike` 并不是正确的输入部分，它不是这个解析器要寻找的对象。

## 用于不固定参数的解析器
* 这样，我们来解析 `<`,`>`,`=`甚至 `</` 和 `/>` 。我们实际上做的差不多了。
* 在开始 `<` 后的下一个元素是元素的名称。虽然我们不能用一个简单的字符串比较来做到这一点，但是我们可以用正则表达式。
* 但是我们要克制自己，它将是一个很容易在简单代码中复制的正则表达式，并且我们不需要为此而去依赖于 `regex` 的 crate 库。我们要试试只试用 Rust 标准库来进行编写自己的解析器。

* 回顾元素名称标识符的规范，它大概是这样：一个字母的字符，然后是若干个字母数字中横线 `-` 等多个字符。

```rust
fn identifier(input: &str) -> Result<(&str, String), &str> {
    let mut matched = String::new();
    let mut chars = input.chars();

    match chars.next() {
        Some(next) if next.is_alphabetic() => matched.push(next),
        _ => return Err(input),
    }

    while let Some(next) = chars.next() {
        if next.is_alphanumeric() || next == '-' {
            matched.push(next);
        } else {
            break;
        }
    }

    let next_index = matched.len();
    Ok((&input[next_index..], matched))
}
```

* 和往常一样，我们先查看一些类型。这次，我们不是编写函数来构建解析器，而是像最开始的那样编写解析器本身。这里值得注意的是，我们没有返回 `()` 的 result 类型，而是返回一个 String 元组，以及剩余的输入部分。这个字符串将包含我们刚刚解析过的标识符。
* 记住这一点，首先我们创建一个空字符串，并调用它进行匹配。这将得到我们的结果值。我们还会得到一个迭代器来逐个遍历这些分开的输入字符。
* 第一步是看前缀是否是字母开始。我们从迭代器中取出第一个字符，并检查他是否是字母： `next.is_alphabetic()` 。在这里，Rust标准库当然会帮助我们处理 Unicode ——它将匹配任意字母，不仅仅是 ASCII 。如果它是一个字母，我们将把它放入匹配完成的字符串中，如果不是，很明显，我们没有找到元素标识符，我们将直接返回一个 error 。
* 第二步，我们继续从迭代器中提取字符，并把它放入正在构建的字符串中，直到我们找到一个不符合 `is_alphanumeric()` （类似于 `is_alphabetic()` ），也不匹配字母表中的任意字符，也不是 `-` 字符。

* 当我们第一次看到不匹配这些条件时，这意味着我们已经完成了解析，因此我们跳出循环，并返回我们处理好的字符串，记住我们要剥离出我们已经处理的输入字符。同样的，如果迭代器迭代完成，表示我们到达了输入的末尾。
* 值得注意的是，当我们看到不是字母数字或 `-` 时，我们没有返回异常。一旦匹配了第一个字母，我们就已经有足够的内容来创建一个有效的标识符，在解析标识符之后，在输入字符串中解析更多的东西是完全正常的，所以我们只需停止解析并返回结果。只有当我们连第一个字母都找不到时，我们才会返回一个异常，因为在这种情况下，输入中肯定没有标识符。
* 还记得我们要将 XML 文档解析为结构体元素吗？

```Rust
struct Element {
    name: String,
    attributes: Vec<(String, String)>,
    children: Vec<Element>,
}
```

* 实际上，我们刚刚完成了第一部分的解析器，解析 `name` 字段。我们解析器返回的字符串就是这样，对于每个属性的第一部分，它也是正确的。
* 开始测试。

```rust
#[test]
fn identifier_parser() {
    assert_eq!(
        Ok(("", "i-am-an-identifier".to_string())),
        identifier("i-am-an-identifier")
    );
    assert_eq!(
        Ok((" entirely an identifier", "not".to_string())),
        identifier("not entirely an identifier")
    );
    assert_eq!(
        Err("!not at all an identifier"),
        identifier("!not at all an identifier")
    );
}
```

* 我们看到第一种情况，字符串 `i-am-an-identifier` 被完整解析，只剩下空字符串。在第二种情况下，解析器返回 `not` 作为标识符，其余的字符串作为剩余的输入返回。在第三种情况下，解析器完全失败，因为它找到的第一个字符不是字母。

## 组合器
* 现在我们可以解析开头的 `<` ，也可以解析下面的标识符，但是我们需要同时解析这2个，以便于能够向下运行。因此，下一步将编写另一个解析器构建器函数，该函数将两个解析器作为输入，并返回一个新的解析器，它按顺序解析这两个解析器。换句话说，是另一个解析器组合器，因为它将两个解析器组合成一个新的解析器。让我们看看能不能实现它。

```rust
fn pair<P1, P2, R1, R2>(parser1: P1, parser2: P2) -> impl Fn(&str) -> Result<(&str, (R1, R2)), &str>
where
    P1: Fn(&str) -> Result<(&str, R1), &str>,
    P2: Fn(&str) -> Result<(&str, R2), &str>,
{
    move |input| match parser1(input) {
        Ok((next_input, result1)) => match parser2(next_input) {
            Ok((final_input, result2)) => Ok((final_input, (result1, result2))),
            Err(err) => Err(err),
        },
        Err(err) => Err(err),
    }
}
```

* 这里稍微有点复杂，但你应该知道接下来要做的：从查看类型开始。
* 首先，我们有四个类型： `P1`， `P2` ， `R1` ， `R2` 。这是分析器1，分析器2，结果1，结果2。P1 和 P2 是函数，你将注意到它们遵循已建立的解析器函数模式：就像返回值一样，他们以 `&str` 作为输入，并返回剩余输入和解析结果的`Result` ，或者返回一个异常。

* 但是看看每个函数的结果类型： P1 是一个解析器，如果成功，它将生成 R1， P2 也将生成 R2 。最终的解析器的结果是——函数的返回值——是 (R1, R2) 。因此，这个解析器的逻辑是首先在输入上运行解析器 P1 ，保留它的结果，然后在 P1 返回的作为输入运行 P2 ，如果这2个方法都能正常运行，我们将这2个结果合并为一个元组 `(R1, R2)` 。
* 看看代码，它也确实是这么实现的。我们首先在输入上运行第一个解析器，然后运行第2个解析器，然后将两个结果组合成一个元组并返回。如果其中一个解析器遇到异常，我们立即返回对应的错误。
* 这样的话，我们可以传入之前的2个解析器，` match_literal` 和 `identifier` ，来实际的解析一下 XML 标签一开始的字节。我们写个测试测一下它是否能起作用。

```rust
#[test]
fn pair_combinator() {
    let tag_opener = pair(match_literal("<"), identifier);
    assert_eq!(
        Ok(("/>", ((), "my-first-element".to_string()))),
        tag_opener("<my-first-element/>")
    );
    assert_eq!(Err("oops"), tag_opener("oops"));
    assert_eq!(Err("!oops"), tag_opener("<!oops"));
}
```
 
* 它似乎可以运行！但看结果类型：`((), String)` 。很明显，我们只关心右边的值，也就是字符串。大部分情况——我们的一些解析器只匹配输入中的模式，而不产生值，因此可以安全地忽略这类输出。适应这种场景，我们要用我们的解析器组合器来写另外2个组合器：left ，第一个解析器忽略结果，并返回第2个和对应的数字；right，这是我们想要使用在我们上面的测试上产生相反的结果——丢弃左侧的 `()` ，只留下我们处理后的字符串。

## 引入 Functor
* 但在此之前，让我们介绍另一个组合器，它的作用是将使组合字的编写变得更简单：`map` 。
* 这2个组合只有1个目的：更改结果的类型。比如你有一个返回 `((), String)` 的解析器，你希望将它改成只返回字符串，正如随便一个例子所示。
* 为此，我们传递一个函数，这个函数知道如何将原始类型转换为新的类型。在我们的示例中，这很简单： `|(_left, right)| right` 。更一般的说，它看起来类似于这样 `Fn(A) -> B`， 其中的 A 是解析器的原始结果类型，B 是新的类型。

```rust
fn map<P, F, A, B>(parser: P, map_fn: F) -> impl Fn(&str) -> Result<(&str, B), &str>
where
    P: Fn(&str) -> Result<(&str, A), &str>,
    F: Fn(A) -> B,
{
    move |input| match parser(input) {
        Ok((next_input, result)) => Ok((next_input, map_fn(result))),
        Err(err) => Err(err),
    }
}
```

* 类型说明了什么？ P 是我们的解析器。它在成功时返回 A 。F 是我们用来将 P 映射返回值的函数，它看起来和 P 一样，只是它的结果类型是 B 而不是 A 。
* 在代码中，我们运行解析器（输入），如果它成功，我们获取结果并在其上运调用函数 `map_fn(result)` ，将 A 转换为 B ，这就是转换后解析器要执行的逻辑。
* 实际上，让我们改变一下，稍微缩短这个函数，因为这个 map 实际上是一个常见的模式，Result 也实现了这个模式：

```rust
fn map<P, F, A, B>(parser: P, map_fn: F) -> impl Fn(&str) -> Result<(&str, B), &str>
where
    P: Fn(&str) -> Result<(&str, A), &str>,
    F: Fn(A) -> B,
{
    move |input|
        parser(input)
            .map(|(next_input, result)| (next_input, map_fn(result)))
}
```

* 这种模式在 Haskell 及其数学范畴理论中被称为“函子”。如果你有一个 A 类型，并且你还有一个可用的 map 函数，这样你就可以把一个函数从 A 传到 B 中，把它变成同一类型的东西，这个过程叫做“函子”。你可以在 Rust 中看到很多这样的地方，比如 Option 、 Result 、 Iterator 甚至 Future 中，都没有显示的将其这样命名。之所以这样，有一个原因：在 Rust 类型系统中，你不能认为一个 `functor` 是普遍存在的，因为它缺乏更上层的类型，但这是另一个话题了，所以回到原先的主题，这些 functor ，你只要寻找映射它的 map 函数。

## 轮到 Trait
* 你可能已经注意到，我们一直在重复解析器的签名这一点：`Fn(&str) -> Result<(&str, Output), &str>` ，你可能已经厌倦了像这样把它写出来，所以我认为现在是时候引入一个 Trait 了，让代码更加可读，并有利于我们对其进行扩展。
* 但首先 ，让我们为一直在使用的返回值类型创建一个别名：

```rust
type ParseResult<'a, Output> = Result<(&'a str, Output), &'a str>;
```

* 所以现在，我们可以输入 `ParseResult<String>` 这样的东西，而不是之前的那个乱七八糟的东西。我们在其中添加了一个生命周期，因为类型生命需要它，但是很多时候， Rust 编译器应该能够为你推断出来。作为一个规范，尝试着把生命周期去掉，看看 rustc 是否会报异常，如果异常，再把生命周期放回去。
* 在本例中，生命周期 `'a` ，由输入的参数的声明周期决定。
* 现在，谈论 trait。我们还需要在这里输入生命周期，当你使用 trait 时，通常需要生命周期。这是一段额外的输入，但它的发行版就是这样子。

```rust
trait Parser<'a, Output> {
    fn parse(&self, input: &'a str) -> ParseResult<'a, Output>;
}
```

* 目前，它只有一个方法 `parse()` 方法，很熟悉吧：它和我们编写的解析器函数一样。
* 为了更简单一点，我们可以为任何匹配解析器签名的函数实现这个 trait 。

```rust
impl<'a, F, Output> Parser<'a, Output> for F
where
    F: Fn(&'a str) -> ParseResult<Output>,
{
    fn parse(&self, input: &'a str) -> ParseResult<'a, Output> {
        self(input)
    }
}
```

* 这样，我们不仅可以传递到完全实现解析器 trait 的解析器所传递相同函数，还可以实现其它的类型作为解析器。
* 但更重要的是，它使我们无需一直键入那些冗长的函数签名。让我们重写 map 函数，看看它如何工作的。

```rust
fn map<'a, P, F, A, B>(parser: P, map_fn: F) -> impl Parser<'a, B>
where
    P: Parser<'a, A>,
    F: Fn(A) -> B,
{
    move |input|
        parser.parse(input)
            .map(|(next_input, result)| (next_input, map_fn(result)))
}
```

* 尤其是这里要注意一件事：不直接将解析器作为一个函数调用，我们现在必须去 `parser.parse(input)` ，因为我们不知道类型 P 是一个函数类型，我们只知道它实现了解析器，所以我们必须保证好解析器提供的接口。另外的，函数看起来也一样，而类型看起来也是整洁的。对于另外一点，产生了一个新的生命周期 `a` ，但总的来说，这已经改善很多了。
* 如果我们用同样的方式重写 `pair` 函数，那就更好了。

```rust
fn pair<'a, P1, P2, R1, R2>(parser1: P1, parser2: P2) -> impl Parser<'a, (R1, R2)>
where
    P1: Parser<'a, R1>,
    P2: Parser<'a, R2>,
{
    move |input| match parser1.parse(input) {
        Ok((next_input, result1)) => match parser2.parse(next_input) {
            Ok((final_input, result2)) => Ok((final_input, (result1, result2))),
            Err(err) => Err(err),
        },
        Err(err) => Err(err),
    }
}
```

* 这里也是一样，唯一的改变就是整理了的类型签名，并且需要使用 `parser.parse(input)` 而非 `parser(input)` 。
* 实际上，我们也整理一下 `pair` 的函数体，就像我们处理 `map` 一样。

```rust
fn pair<'a, P1, P2, R1, R2>(parser1: P1, parser2: P2) -> impl Parser<'a, (R1, R2)>
where
    P1: Parser<'a, R1>,
    P2: Parser<'a, R2>,
{
    move |input| {
        parser1.parse(input).and_then(|(next_input, result1)| {
            parser2.parse(next_input)
                .map(|(last_input, result2)| (last_input, (result1, result2)))
        })
    }
}
```

* `Result` 中的 `and_then` 方法和 `map` 是类似的，只要映射的函数不将返回的新值放入 Result 中，而是返回一个全新的 Result 。上面代码实际上和前面使用的 match 代码块一样。我们稍后回到 `and_then` ，但现在，我们要实现那些被实现了的 `left` 和 `right` 组合器，现在我们有了一个漂亮的的 `map` 。

## Left 和 Right
* 有了 `pair` 和 `map` ，我们就可以简单的写出 left 和 right 。

```rust
fn left<'a, P1, P2, R1, R2>(parser1: P1, parser2: P2) -> impl Parser<'a, R1>
where
    P1: Parser<'a, R1>,
    P2: Parser<'a, R2>,
{
    map(pair(parser1, parser2), |(left, _right)| left)
}

fn right<'a, P1, P2, R1, R2>(parser1: P1, parser2: P2) -> impl Parser<'a, R2>
where
    P1: Parser<'a, R1>,
    P2: Parser<'a, R2>,
{
    map(pair(parser1, parser2), |(_left, right)| right)
}
```

* 我们使用 `pair` 组合器将2个解析器组合到一个产生元组结果的解析器中，然后我们使用 `map` 组合成员选择我们想要保留的元组部分。
* 重写解析前两部分元素标签的测试，现在更简洁了，在这个过程中，我们获得了一些新的解析器的功能。
* 不过，我们必须先更新两个解析器，用于使用 `Parser` 和 `ParseResult` 。`match_literal` 则会更加复杂：

```rust
fn match_literal<'a>(expected: &'static str) -> impl Parser<'a, ()> {
    move |input: &'a str| match input.get(0..expected.len()) {
        Some(next) if next == expected => Ok((&input[expected.len()..], ())),
        _ => Err(input),
    }
}
```

* 除了改变返回值类型外，我们还必须确保闭包的输入参数类型是 `&'a str` ，否则编译器可能会报错。
* 对于标识符，只需要更改返回类型，就可以了，编译器会帮助你推导生命周期：

```rust
fn identifier(input: &str) -> ParseResult<String> {
```

* 现在测试一下，很不错，返回结果不再是 `()`

```rust
#[test]
fn right_combinator() {
    let tag_opener = right(match_literal("<"), identifier);
    assert_eq!(
        Ok(("/>", "my-first-element".to_string())),
        tag_opener.parse("<my-first-element/>")
    );
    assert_eq!(Err("oops"), tag_opener.parse("oops"));
    assert_eq!(Err("!oops"), tag_opener.parse("<!oops"));
}
```

## 一个或多个空格的处理
* 我们继续解析这个元素标签。我们获取了开始的 `<` ，并且我们获取了标识符。接下来呢？接下来应该是属性。
* 不实际上，这些属性是可选的。我们必须找到一个正确处理可选的方法。
* 还是等等吧，实际上在我们开始处理属性之前，先要处理空格。
* 在元素名称结尾，和第一个属性名（如果有属性的话）之间有一个空格。我们需要处理这个空格。
* 更糟糕的是，我们需要处理一个甚至更多空格，因为形如 `<element      attribute="value"/>` 的写法也是合法的，虽然空格多了点。因此，我们要好好考虑我们是否可以编写组合器来表达解析1个或者多个空格的思想。
* 我们已经在标识符解析器中处理过，但那是通过手动完成的。不足为奇的是，这种代码一般没有很大的不同。

```rust
fn one_or_more<'a, P, A>(parser: P) -> impl Parser<'a, Vec<A>>
where
    P: Parser<'a, A>,
{
    move |mut input| {
        let mut result = Vec::new();

        if let Ok((next_input, first_item)) = parser.parse(input) {
            input = next_input;
            result.push(first_item);
        } else {
            return Err(input);
        }

        while let Ok((next_input, next_item)) = parser.parse(input) {
            input = next_input;
            result.push(next_item);
        }

        Ok((input, result))
    }
}
```

* 首先，我们正在构建的解析器的返回类型是 A ，组合解析器的返回类型是 `Vec<A>` —— 任意数量的 `A` 类型。
* 代码看起来确实和处理标识符的那段很像。首先我们解析第一个元素，如果没有，我们返回一个 error 。然后我们解析尽可能多的元素，知道解析器遇到错误，这时我们返回 collected 到的所有元素的 vector 。
* 看看这段代码，是不是很容易就能将其调整为符合0个或者更多的逻辑？我们只需移除解析器的第一次运行。

```rust
fn zero_or_more<'a, P, A>(parser: P) -> impl Parser<'a, Vec<A>>
where
    P: Parser<'a, A>,
{
    move |mut input| {
        let mut result = Vec::new();

        while let Ok((next_input, next_item)) = parser.parse(input) {
            input = next_input;
            result.push(next_item);
        }

        Ok((input, result))
    }
}
```

* 我们来编写一些测试来确保这2个方法能正常运行：

```rust
#[test]
fn one_or_more_combinator() {
    let parser = one_or_more(match_literal("ha"));
    assert_eq!(Ok(("", vec![(), (), ()])), parser.parse("hahaha"));
    assert_eq!(Err("ahah"), parser.parse("ahah"));
    assert_eq!(Err(""), parser.parse(""));
}

#[test]
fn zero_or_more_combinator() {
    let parser = zero_or_more(match_literal("ha"));
    assert_eq!(Ok(("", vec![(), (), ()])), parser.parse("hahaha"));
    assert_eq!(Ok(("ahah", vec![])), parser.parse("ahah"));
    assert_eq!(Ok(("", vec![])), parser.parse(""));
}
```

* 注意两者之间的区别：对于 `one_or_more` ，查找空字符串是一个错误，因为它至少需要查看它的子解析器的一种情况，但对于 `zero_or_more` ，空字符串只表示 0 的情况，这不是错误。
* 在这一点上，考虑如何归纳这两种情况是合理而必要的，因为其中一个是另一个的副本，只是删除了一个位。可能很容易就能用 `zero_or_more` 来表示 `one_or_more` ，如下所示：

```rust
fn one_or_more<'a, P, A>(parser: P) -> impl Parser<'a, Vec<A>>
where
    P: Parser<'a, A>,
{
    map(pair(parser, zero_or_more(parser)), |(head, mut tail)| {
        tail.insert(0, head);
        tail
    })
}
```

* 在这里，我们遇到了 Rust 的一些问题，我不是说针对 Vec 没有 `cons` 方法的问题，但我知道 Lisp 程序员在读这段代码时都会这样想。甚至可能会更甚一些。
* 我们有了这个解析器，所以我们不能对这个参数传递2次，编译器会告诉你这行不通：你在试着移除一个已经移除的值。那么，我们能让我们的选择器使用参数的引用吗？不行的，事实证明，遇到变量的借用检查问题——我们甚至现在不用去理会。因为这些解析器就是一些函数，所以它们不会直接实现克隆，这将很省事，我们现在遇到困难了，我们不能在组合器中轻松的解析。

* 不过这也没什么大不了的。这意味着我们无法使用组合器表达 `one_or_more` ，但事实证明这2个东西是你经常使用的组合器，也时常被复用，而且，如果你真的想要，你可以使用 `RangeBound` 编写一个组合器，额外附加一个解析器并根据 `range(0..)` 的方式，如  `zero_or_more`, `range(1..)` ,如 `one_or_more`, `range(5..=6)` 或完整的5个、6个，总之随意而为。
* 让我们把他留给读者作为练习。现在，我们只需要 0 或者 1 或者更多就可以。
* 另一个练习是，尝试找到一个解决这些所有权问题的方法——也许通过在 `Rc` 中包装一个解析器使其 `cloneable` 。


* 翻译进度： `A Predicate Combinator...`
