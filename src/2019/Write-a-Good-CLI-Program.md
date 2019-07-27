# 写个好的 CLI 程序
>* Write a Good CLI Program 译文

>* 原文链接：https://qiita.com/tigercosmos/items/678f39b1209e60843cc3
>* 原文作者：[Liu, An-Chi](https://www.facebook.com/pg/CodingNeutrino) ([劉安齊](https://github.com/tigercosmos))
>* 译文出处：https://www.github.com/suhanyujie
>* 译者：[suhanyujie](https://www.github.com/suhanyujie)

A command line interface(CLI) program runs on the terminal, which means there is no graphic interface, aka none-GUI.

Actually, we use CLI every day, such as ls, ps, top, etc. There is also a awesome-cli-apps collects many good CLI program. You can take a look. I recommend exa, A modern version of ‘ls’ written in Rust.

![](./images07/68747470733a2f2f7261772e67697468756275736572636f6e74656e742e636f6d2f6f6768616d2f6578612f6d61737465722f73637265656e73686f74732e706e67.png)

## A CLI program
A CLI program would look like:

```shell
$ ./program_name [arguments] [flags] [options]
```

Usually we could add -h or --help to see the information.

Take the cargo program for example:

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

Now you will know how to use cargo.

## Create project
Let's start to build a new CLI program!

I name the project meow here.

```shell
$ cargo new meow
$ cd meow
```

## Arguments
As we have seen how a CLI would be, there are usually arguments for a CLI.

A simple and naive solution is:

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

Then we get the arguments.

However, in the real word, the CLI is more complicated, such as:

```shell
$ ./foo -g -e a1 a3 a4
$ ./foo a1 -e -l --path=~/test/123
```

So the naive way is inconvenient to use.
- arguments might have default value
- flags would exchange positions
- options would exchange positions
- arg1 might binds arg2

So, we need a crate to help us do this job easily.

### Clap
Clap is a full featured, fast Command Line Argument Parser for Rust.

How to use it?

First, create a cli.yml file, which is the setting for arguments. It looks like:

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

Then we add the following code in main.rs

```rust
#[macro_use]
extern crate clap;
use clap::App;

fn main() {
    // The YAML file is found relative to the current file, similar to how modules are found
    let yaml = load_yaml!("cli.yml");
    let m = App::from_yaml(yaml).get_matches();

     match m.value_of("argument1") {
         // ...
     }

     // ...
}
```

The clap crate will load and parse the yml, and we can use the arguments in the program.

The running result of the program with the cli.yml above with -h is:

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

Very convenient. Isn't it?

## Configuration
A CLI will also need configuration. Some parameters should be determined before run time and they are record in the configuration file, which usally is .env, .config, .setting.

For example, a .env file:

```env
PORT = 8000
PATH = "home/foo/bar"
MODE = "happy mode"
ZONE = 8
AREA = "Taipei"
```

you could write by hands

* read file .env
* split \n
* split = and add data to HashMap

or use a crate.

### dotenv_codegen
dotenv_codegen is a simple .env configuration parser with macro.

The crate reads .env. It is esay to use.

```rust
fn main() {
  println!("{}", dotenv!("PORT"));
}
```

## Environment Variables
You might also want to call the environment variable in the system, such as JAVA_HOME.

```rust
use std::env;

let key = "HOME";
match env::var_os(key) {
    Some(val) => println!("{}: {:?}", key, val),
    None => println!("{} is not defined in the environment.", key)
}
```

## Error handling
Erro handling is also important. We don't want the program always panic! and shut down the program when it encounter an error. Sometimes, the errors don't matter too much, so we can handle them without break the program, such as running againg or adopting another rule when there is an error.

### panic
```rust
panic!("this is panic");
```

Simple but powerless.

* break the program
* exit without error code
* better use in script

### Result
Result pass the error without crash. If the function breaks, it will return Error with error type. Then we can decide what to do next according to the type, such as "retry" or "give up".

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
        // we need `fmt` to tranlate to the message
    }
}
```

### Error Message
You might want to print or use the error message of the error type. You need to impl the fmt for MyErr, so that it will have its own error message.

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

## Standard error
There are standard output and standard error in the system.

println!() is the standard output, and eprintln!() is the standard error.

For example:

```shell
$ cargo run > output.txt
```

## Exit Code
none-zero code lets other programs know it failed.

```rust
use std::process;

fn main() {
    process::exit(1);
}
```

## Conclusion
A CLI program could do any kind of works, and a good CLI need a well design. It parses arguments and configuration. It reads environment variables. It handles errors well. It output message in standard output and error. It exits with code when it failed.
