# 【译】使用 Rust 构建你自己的 Shell

>* 原文地址：https://www.joshmcguigan.com/blog/build-your-own-shell-rust/
>* 原文作者：[Josh Mcguigan](https://github.com/JoshMcguigan)
>* 译文出自：https://github.com/suhanyujie
>* 本文永久链接： https://github.com/suhanyujie/article-transfer-rs/blob/master/src/2019/Build_Your_Own_Shell_using_Rust.md
>* 译者：[suhanyujie](https://github.com/suhanyujie)

> 正文开始

* 这是一个使用 Rust 构建自己的 shell 的教程，已经被收录在 [build-your-own-x](https://github.com/danistefanovic/build-your-own-x) 列表中。自己创建一个 shell 是理解 shell、终端模拟器、以及 OS 等协同工作的好办法。

## shell 是什么？
* shell 是一个程序，它可以用于控制你的计算机。这在很大程度上简化了启动应用程序。但 shell 本身并不是一个交互式应用程序。
* 大多数用户通过终端模拟器来和 shell 交互。用户 [geirha](https://askubuntu.com/a/111149)  对终端模拟器的形如如下：
>终端模拟器（通常简称为终端）就是一个“窗口”，是的，它运行一个基于文本的程序，默认情况下，它就是你登陆的 shell （也就是 Ubuntu 下的 bash）。当你在窗口中键入字符时，终端除了将这些字符发送到 shell （或其他程序）的 stdin 之外，还会在窗口中绘制这些字符。 shell 输出到 stdout 和 stderr 的字符被发送到终端，终端在窗口中绘制这些字符。

* 在本教程中，我们将编写自己的 shell ，并在普通的终端模拟器（通常在 cargo 运行的地方）中运行它。

## 从简单开始
* 最简单的 shell 只需要几行 Rust 代码。这里我们创建一个新字符串，用于保存用户输入。`stdin().read_line` 的作用将会阻塞，直到用户按下回车键，然后它将整个用户输入的内容（包括回车键的空行）写入字符串。使用 `input.trim()` 删除换行符等空行，我们尝试在命令行中运行它。

```rust
fn main(){
    let mut input = String::new();
    stdin().read_line(&mut input).unwrap();

    // read_line leaves a trailing newline, which trim removes
    let command = input.trim(); 

    Command::new(command)
        .spawn()
        .unwrap();
}
```

* 运行此操作后，你应该会在你的终端中看到一个正在等待输入的闪烁光标。尝试键入 ls 并回车，你将看到 ls 命令打印当前目录的内容，然后 shell 将推出。
* 注意：这个例子不能在 [Rust Playground](https://play.rust-lang.org/) 上运行，因为它目前不支持 stdin 等长时间运行和处理。

## 接收多个命令
* 我们不希望在用户输入单个命令后退出 shell。支持多个命令主要是将上面的代码封装在一个 `loop` 中，并添加调用  `wait` 来等待每个子进程，以确保我们不会在当前进程退出之前提示用户输入额外的信息。我还添加了几行来打印字符 `>`，以便用户更容易的将其输入与派生的进程输出区分开来。

```rust
fn main(){
    loop {
        // use the `>` character as the prompt
        // need to explicitly flush this to ensure it prints before read_line
        print!("> ");
        stdout().flush();

        let mut input = String::new();
        stdin().read_line(&mut input).unwrap();

        let command = input.trim();

        let mut child = Command::new(command)
            .spawn()
            .unwrap();

        // don't accept another command until this one completes
        child.wait(); 
    }
}
```

* 运行这段代码后，你将看到在运行第一个命令之后，会返回提示符，以便你可以输入第二个命令。使用 `ls` 和 `pwd` 命令来尝试一下。

## 参数处理
* 如果你尝试在上面的 shell 上运行命令 `ls -a` ，它将会崩溃。因为它不知道怎么处理参数，它尝试运行一个名为 `ls -a` 的命令，但正确的行为是使用参数 `-a` 运行一个名为 `ls` 的命令。
* 通过将用户输入拆分为空格字符，并将第一个空格之前的内容作为命令的名称（例如 `ls`），而将第一个空格之后的内容作为参数传递给该命令（例如 `-a`），这个问题在下面就会解决。

```rust
fn main(){
    loop {
        print!("> ");
        stdout().flush();

        let mut input = String::new();
        stdin().read_line(&mut input).unwrap();

        // everything after the first whitespace character 
        //     is interpreted as args to the command
        let mut parts = input.trim().split_whitespace();
        let command = parts.next().unwrap();
        let args = parts;

        let mut child = Command::new(command)
            .args(args)
            .spawn()
            .unwrap();

        child.wait();
    }
}
```

## shell 的内建函数
* 事实证明， shell 不能简单的将某些命令分派给另一个进程。这些都是影响 shell 内部，所以，必须由 shell 本身实现。
* 最常见的例子可能就是 `cd` 命令。要了解为什么 cd 必须是内建的 shell，请[查看这个链接](https://unix.stackexchange.com/a/38809)。处理内建的命令之外，实际上还有一个名为 cd 的程序。[这里](https://unix.stackexchange.com/a/38819)有关于二元概念的解释
* 下面我们添加内建 cd 处理来支持我们的 shell 

```rust
fn main(){
    loop {
        print!("> ");
        stdout().flush();

        let mut input = String::new();
        stdin().read_line(&mut input).unwrap();

        let mut parts = input.trim().split_whitespace();
        let command = parts.next().unwrap();
        let args = parts;

        match command {
            "cd" => {
                // default to '/' as new directory if one was not provided
                let new_dir = args.peekable().peek().map_or("/", |x| *x);
                let root = Path::new(new_dir);
                if let Err(e) = env::set_current_dir(&root) {
                    eprintln!("{}", e);
                }
            },
            command => {
                let mut child = Command::new(command)
                    .args(args)
                    .spawn()
                    .unwrap();

                child.wait();
            }
        }
    }
}
```

## 错误处理
* 如果你看到这儿，你可能会发现，如果你输入一个不存在的命令，上面的 shell 将会崩溃。在下面的版本中，通过向用户输出一个错误，然后允许他们输入另一个命令，可以很好的处理这个问题。
* 由于错误输入的命令是退出 shell 的一个简单方法，所以我还实现了另一个内置的 shell 处理，也就是 `exit` 命令。

```rust
fn main(){
    loop {
        print!("> ");
        stdout().flush();

        let mut input = String::new();
        stdin().read_line(&mut input).unwrap();

        let mut parts = input.trim().split_whitespace();
        let command = parts.next().unwrap();
        let args = parts;

        match command {
            "cd" => {
                let new_dir = args.peekable().peek().map_or("/", |x| *x);
                let root = Path::new(new_dir);
                if let Err(e) = env::set_current_dir(&root) {
                    eprintln!("{}", e);
                }
            },
            "exit" => return,
            command => {
                let child = Command::new(command)
                    .args(args)
                    .spawn();

                // gracefully handle malformed user input
                match child {
                    Ok(mut child) => { child.wait(); },
                    Err(e) => eprintln!("{}", e),
                };
            }
        }
    }
}
```

## 管道符
* 如果没有管道操作符的功能的 shell 是很难用于实际生产环境的。如果你不熟悉这个特性，可以使用 `|` 字符告诉 shell 将第一个命令的结果输出重定向到第二个命令的输入。例如，运行 `ls | grep Cargo` 会触发以下操作：
    * `ls` 将列出当前目录中的所有文件和目录
    * shell 将通过管道将以上的文件和目录列表输入到 `grep`
    * `grep` 将过滤这个列表，并只输出文件名包含字符 `Cargo` 的文件

* shell 的最后一次迭代包括了对管道的基础支持。要了解管道和 IO 重定向的其他功能，可以[参考这个文章](https://robots.thoughtbot.com/input-output-redirection-in-the-shell)

```rust
fn main(){
    loop {
        print!("> ");
        stdout().flush();

        let mut input = String::new();
        stdin().read_line(&mut input).unwrap();

        // must be peekable so we know when we are on the last command
        let mut commands = input.trim().split(" | ").peekable();
        let mut previous_command = None;

        while let Some(command) = commands.next()  {

            let mut parts = command.trim().split_whitespace();
            let command = parts.next().unwrap();
            let args = parts;

            match command {
                "cd" => {
                    let new_dir = args.peekable().peek()
                        .map_or("/", |x| *x);
                    let root = Path::new(new_dir);
                    if let Err(e) = env::set_current_dir(&root) {
                        eprintln!("{}", e);
                    }

                    previous_command = None;
                },
                "exit" => return,
                command => {
                    let stdin = previous_command
                        .map_or(
                            Stdio::inherit(),
                            |output: Child| Stdio::from(output.stdout.unwrap())
                        );

                    let stdout = if commands.peek().is_some() {
                        // there is another command piped behind this one
                        // prepare to send output to the next command
                        Stdio::piped()
                    } else {
                        // there are no more commands piped behind this one
                        // send output to shell stdout
                        Stdio::inherit()
                    };

                    let output = Command::new(command)
                        .args(args)
                        .stdin(stdin)
                        .stdout(stdout)
                        .spawn();

                    match output {
                        Ok(output) => { previous_command = Some(output); },
                        Err(e) => {
                            previous_command = None;
                            eprintln!("{}", e);
                        },
                    };
                }
            }
        }

        if let Some(mut final_command) = previous_command {
            // block until the final command has finished
            final_command.wait();
        }

    }
}
```

## 结语
* 在不到 100 行的代码中，我们创建了一个 shell ，它可以用于许多日常操作，但是一个真正的 shell 会有更多的特性和功能。GNU 网站有一个关于 bash shell 的在线手册，其中包括了 [shell 特性](https://www.gnu.org/software/bash/manual/html_node/Basic-Shell-Features.html#Basic-Shell-Features)的列表，这是着手研究更高级功能的好地方。

* 请注意，这对我来说是一个学习的项目，在简单性和健壮性之间需要权衡的情况下，我选择简单性。
* 这个 shell 项目可以在我的 [GitHub](https://github.com/JoshMcguigan/bubble-shell) 上找到。在撰写本文时，最新提交是 [`a47640`](https://github.com/JoshMcguigan/bubble-shell/tree/a6b81d837e4f5e68cf0b72a4d55e95fb08a47640) 。另一个你可能感兴趣的学习 Rust shell 项目是 [Rush](https://github.com/psinghal20/rush)
