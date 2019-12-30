# 写个好的 CLI 程序
>* Write a Good CLI Program 译文

>* 原文链接：https://qiita.com/tigercosmos/items/678f39b1209e60843cc3
>* 原文作者：[Liu, An-Chi](https://www.facebook.com/pg/CodingNeutrino) ([劉安齊](https://github.com/tigercosmos))
>* 译文出处：https://github.com/suhanyujie/article-transfer-rs
>* 译者：[suhanyujie](https://www.github.com/suhanyujie)

命令行接口程序（CLI）是运行在终端的，这也就意味着没有图形界面，即 none-GUI

事实上，我们每天都会使用 CLI，例如 `ls`、`ps`、`top` 等。有一个[很棒的 cli 应用列表](https://github.com/agarrharr/awesome-cli-apps)，它收集了很多优秀的 CLI 程序。你可以看一下。我推荐 [exa](https://github.com/ogham/exa)，它是一个用 Rust 编写的 `ls` 程序。

![](./images07/68747470733a2f2f7261772e67697468756275736572636f6e74656e742e636f6d2f6f6768616d2f6578612f6d61737465722f73637265656e73686f74732e706e67.png)

## CLI 程序
CLI 程序使用起来跟下面类似：

```shell
$ ./program_name [arguments] [flags] [options]
```

一般我们可以通过 `-h` 和 `--help` 参数看到相关信息。

以 `cargo` 程序为例：

```shell
$ cargo -h
Rust's package manager

USAGE:
    cargo [OPTIONS] [SUBCOMMAND]

OPTIONS:
    -V, --version           Print version info and exit
        --list              List installed commands
        --explain <CODE>    Run `rustc --explain CODE`
    -v, --verbose           Use verbose output (-vv very verbose/build.rs output)
    -q, --quiet             No output printed to stdout
        --color <WHEN>      Coloring: auto, always, never
        --frozen            Require Cargo.lock and cache are up to date
        --locked            Require Cargo.lock is up to date
    -Z <FLAG>...            Unstable (nightly-only) flags to Cargo, see 'cargo -Z help' for details
    -h, --help              Prints help information

Some common cargo commands are (see all commands with --list):
    build       Compile the current project
    check       Analyze the current project and report errors, but don't build object files
    clean       Remove the target directory
    doc         Build this project's and its dependencies' documentation
    new         Create a new cargo project
    init        Create a new cargo project in an existing directory
    run         Build and execute src/main.rs
    test        Run the tests
    bench       Run the benchmarks
    update      Update dependencies listed in Cargo.lock
    search      Search registry for crates
    publish     Package and upload this project to the registry
    install     Install a Rust binary
    uninstall   Uninstall a Rust binary

See 'cargo help <command>' for more information on a specific command.
```

现在你应该知道怎样去使用 `cargo` 了。

## 创建项目
我们以创建一个新的 CLI 项目开始！

这里我将其命名为 `meow`

```shell
$ cargo new meow
$ cd meow
```

`cargo` 会帮助你完成新项目的创建。

## 参数
我们已经看到 CLI 程序的样子，它通常有一些参数。

一个简单直接的方案是：

```rust
// main.rs
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    println!("{:?}", args);
}
```

```shell
$./meow a1 a2 a3
["meow", "a1", "a2", "a3"]
```

我们获取到所有的 CLI 参数。

然而，说实话，实际的 CLI 程序的参数会更复杂些，比如：

```shell
$ ./foo -g -e a1 a3 a4
$ ./foo a1 -e -l --path=~/test/123
```

因而，那个简单的方法就不太便于使用，原因如下：
- 参数可能有默认值
- 标识会有不同的顺序
- 一些选项的顺序会不同
- 参数 `arg1` 可能会绑定参数 `arg2`

所以，我们需要一个 crate 帮助我们简化这个工作。

### Clap
[Clap](https://github.com/clap-rs/clap) 具备我们所需的功能，它是 Rust 实现的命令行参数快速解析工具。

怎么使用它呢？

首先，创建一个 `cli.yml` 文件，用于参数配置。它内容类似于下面：

```yml
<!--  cli.yml -->
name: myapp
version: "1.0"
author: Kevin K. <kbknapp@gmail.com>
about: Does awesome things
args:
    - config:
        short: c
        long: config
        value_name: FILE
        help: Sets a custom config file
        takes_value: true
    - INPUT:
        help: Sets the input file to use
        required: true
        index: 1
    - verbose:
        short: v
        multiple: true
        help: Sets the level of verbosity
subcommands:
    - test:
        about: controls testing features
        version: "1.3"
        author: Someone E. <someone_else@other.com>
        args:
            - debug:
                short: d
                help: print debug information
```

接着，我们在 `main.rs` 增加以下代码：

```rust
#[macro_use]
extern crate clap;
use clap::App;

fn main() {
    // YAML 文件以相对于当前的入口文件被检索到，类似于 Rust 中的模块查找
    let yaml = load_yaml!("cli.yml");
    let m = App::from_yaml(yaml).get_matches();

     match m.value_of("argument1") {
         // ...
     }

     // ...
}
```

`clap` crate 会加载并解析 yml 文件，然后我们可以在程序中使用解析出的参数。

有了上方的 `cli.yml` 文件，我们通过 `-h` 参数运行程序就会得到下面的结果：

```shell
$ meow -h
My Super Program 1.0
Kevin K. <kbknapp@gmail.com>
Does awesome things

USAGE:
    MyApp [FLAGS] [OPTIONS] <INPUT> [SUBCOMMAND]

FLAGS:
    -h, --help       Prints help information
    -v               Sets the level of verbosity
    -V, --version    Prints version information

OPTIONS:
    -c, --config <FILE>    Sets a custom config file

ARGS:
    INPUT    The input file to use

SUBCOMMANDS:
    help    Prints this message or the help of the given subcommand(s)
    test    Controls testing features
```

非常方便，对吧？

## 配置
和大多数程序一样， CLI 程序也需要配置。一些参数应该在运行前确定，并记录在譬如 `.env`、`.config`、`.setting` 的配置文件中。

例如 `.env` 文件：

```env
PORT = 8000
PATH = "home/foo/bar"
MODE = "happy mode"
ZONE = 8
AREA = "Taipei"
```

你可以手写以下逻辑

* 读取文件 `.env`
* 通过 `\n` 分离
* 将数据通过 `=` 分离出来并存入 `HashMap`

也可以使用一个 crate 帮你实现这些功能

### dotenv_codegen
[dotenv_codegen](https://crates.io/crates/dotenv_codegen) 是一个带有宏的 `.env` 配置解析器。

通过这个 crate 读取 `.env`，很容易使用。

```rust
fn main() {
  println!("{}", dotenv!("PORT"));
}
```

## 环境变量
你可能还想调用系统中的环境变量，如环境变量 `JAVA_HOME`。

```rust
use std::env;

let key = "HOME";
match env::var_os(key) {
    Some(val) => println!("{}: {:?}", key, val),
    None => println!("{} is not defined in the environment.", key)
}
```

## 错误处理
错误处理非常重要。我们不想程序经常 `panic!`，也不希望当它遭遇错误后就退出程序。有时候，错误不是很致命，我们需要在不退出程序的情况下处理错误，比如在遇到错误时运行特定的规则或逻辑。

### panic
```rust
panic!("this is panic");
```

以下是一些简便但不优雅的处理方式

* 直接退出程序
* 退出时，不指定错误码
* 以脚本的方式使用

### Result
`Result` 在没有崩溃的情况下传递错误。如果函数中断，它将返回 `Error` 并带有错误类型信息。然后我们根据类型决定下一步做什么，如“重试”，或者“放弃”。

```rust
enum MyErr {
    Reason1,
    Reason2,
}
fn foo() -> Result<(), MyErr> {
    match bar {
        Some(_)=>{}
        Nono => Err(MyErr::Reason1)
    }
}
fn hoo() {
    match foo() {
        Ok(_) => reply(),
        Err(e) => println!(e) 
        // `e` not work yet
        // 我们需要 `fmt` 将信息转换一下
    }
}
```

### 错误消息
你可能要打印或者使用错误消息。此时，你需要为 `MyErr` 实现（`impl`）格式化（`fmt`），这样就会拥有自己定义的错误消息。

```rust
enum MyErr {
    Reason1(String),
    Reason2(String, u32),
}
impl fmt::Display for MyErrError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            MyErr::Reason1(ref s) => 
                write!(f, "`{}` is the error", s),
            MyErr::Reason2(ref s, ref num) => 
                write!(f, "`{}` and `{}` are error", s, num),
        }
    }
}
```

```rust
Err(e) => println!("{}", e)
// `XXX` is the error
```

## 标准错误
在操作系统中，有标准输出和标准错误。

`println!()` 是标准输出，`eprintln!()` 是标准错误。

例如：

```shell
$ cargo run > output.txt
```

此时标准输出流会重定向到 `output.txt` 文件中。

那么，如果我们想要把错误消息写到一个日志文件中，可以像“标准错误”一样使用 `eprintln!()` 来展示错误信息。

## 退出 code
非 0 值的“退出码”可以让其他程序知道运行的程序有异常。

```rust
use std::process;

fn main() {
    process::exit(1);
}
```

## 结语
CLI 程序完成任意类型的任务，并且好的 CLI 程序需要良好的设计。它可以解析参数和配置。它可以读取环境变量。它还能处理错误。能将信息输出到标准输出和标准错误流中。当它异常退出时会有对应的退出码。
