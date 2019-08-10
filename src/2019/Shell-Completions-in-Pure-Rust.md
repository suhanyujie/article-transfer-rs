# Shell Completions in Pure Rust
>Shell Completions in Pure Rust 译文

>* 原文地址：https://www.joshmcguigan.com/blog/shell-completions-pure-rust/
>* 原文作者：[Josh Mcguigan](https://github.com/JoshMcguigan)
>* 译文出自：https://github.com/suhanyujie/article-transfer-rs
>* 本文永久链接： （暂无）
>* 译者：[suhanyujie](https://github.com/suhanyujie)

If you’ve ever hit the tab key while typing commands in the shell, you are familiar with the value of shell completions. Bash has some default completion behavior, which is why you can type `hel<TAB>` and end up with `help`. It is also possible to add additional completion scripts which define the completion behavior for a particular application. For example, if you have a completion script installed for git, typing `git comm<TAB>` will complete to `git commit`.

## How does completion work?
Much of the completion behavior you see is built in to bash itself. An example of this is first word completion, where bash will provide suggestions before you’ve even finished typing the first word of a command (for example, try typing `log<TAB>` at an empty bash prompt). After the first word, bash will first look to see if a completion has been configured for that command. If not, bash will fall back to completing file/directory names as the arguments.

Custom completion behavior is configured using a special bash built-in called `complete`. `complete` can be used to designate either a bash function or any other command as the completion script for a particular command. When the user requests completions for a command, `complete` will run specified code, passing in as args information about what the user has already typed, and expecting as output the completion suggestions. Typically these completion scripts are written in bash, but we’ll look at how it is possible to write them in Rust.

## A minimal Rust completion script
First, I’ll make a new command line app using `cargo new democli`. By default, cargo will construct a single binary with an entry point in `src/main.rs`, but in this case we need two binaries: one for our actual application, and the other to define our shell completions.

Make a `bin` directory inside the src directory, and then move `src/main.rs` to `src/bin/democli.rs`. Then create a new file` src/bin/__democli_shell_completion.rs`. Make sure the two Rust files contain the following:

```rust
// src/bin/democli.rs
// Our actual application logic would go here, but for now it is just stubbed out
fn main() {}

// src/bin/__democli_shell_completion.rs
fn main() {
    println!("add");
    println!("commit");
}
```

Now run `cargo install --force --path .` to install these binaries, then run `complete -C __democli_shell_completion democli` to configure bash to use our completion script.

Note that the `complete` configuration is tied to that particular shell instance, so to make it persist you’d want to add that command to your `~/.bash_profile`.

Now `democli <TAB>` will suggest completions `add` and `commit`. Unfortunately, `democli commi<TAB>` also suggests both `add` and `commit` even though at this point we’d prefer it to complete `commit`. This is because our completion script unconditionally returns both commands to the shell, so let’s fix that.

```rust
// src/bin/__democli_shell_completion.rs
fn main() {
    let subcommands = vec!["add", "commit"];                                                                       
    let args : Vec<String> = std::env::args().collect();                                                           
    let word_being_completed = &args[2];                                                                           
    for subcommand in subcommands { 
       if subcommand.starts_with(word_being_completed) {
           println!("{}", subcommand);
       }
    }   
}   
```

Run `cargo install --force --path .` again to update the installed version of the completion binary and try the completions again with `democli commi<TAB>` to see the improved completion behavior. The `word_being_completed` that we are getting from `args[2]` will be equal to the characters in the word under the users cursor when they press tab.

## Advanced completions
There is much we could improve about our completion script above (try `democli add com<TAB>`). Fortunately the shell provides us with some additional context about the text the user has entered before pressing tab.

### Arguments
- the name of the command whose arguments are being completed
- the word under the users cursor when they press tab
- the word preceding the word under the users cursor

### Environment Variables
- `COMP_LINE` - the full text that the user has entered
- `COMP_POINT` - the cursor position (a numeric index into `COMP_LINE`)

High quality completion scripts use this information to provide contextualized completions. For example, `git add <TAB>` should provide completions for file/directory names, while `ssh user@<TAB>` should complete with known host names. It would be inconvenient to force every completion script which wanted these types of completions to define them from scratch, so bash provides another built-in called `compgen` to generate these advanced completions for you.

Typically a completion script will determine which type of completion is needed by looking at the arguments and/or environment variables passed in from the shell. For subcommand completions, the completion script can return the subcommand options as we’ve shown above. For more advanced completions, the completion script will call out to compgen, for example `comgen -d` will return directory name completions. See [here](https://www.gnu.org/software/bash/manual/html_node/Programmable-Completion-Builtins.html), under the Actions settings for the full list of completion types supported by `compgen`.

While you could call `compgen` from Rust, it is a shell built-in so you’d have to call it within the context of a shell, meaning you’d be starting two processes for each call to `compgen`. This is probably acceptable in most use cases, since typically a completion script would make only a single call to `compgen`, but it would be nice if there was a pure Rust implementation of the `compgen` helpers.

## Why write completions in Rust?
Writing shell completions in bash means you get easy integration with `compgen`, plus you can use the same distribution mechanism as most other shell completion scripts, which limits the chances of surprising your users. So why should you write completions in Rust instead?

Completion scripts for a feature-ful command line application can get quite large, so having the Rust compiler around to check your work can be handy. You can also use all the same testing tools you are familiar with from within the Rust community to test your completion scripts. For authors of Rust command line parsing libraries, there is the option of creating derive macros to generate completion scripts for your users (CLI app authors), and since you are creating the scripts in Rust rather than bash, you can use all the standard Rust codegen tooling rather than trying to generate completion scripts in bash.

## Introducing `shell_completion`
I think there is a lot of opportunity in this area to help make Rust CLI apps even better. For that reason, I’ve started a crate [shell_completion](https://github.com/joshmcguigan/shell_completion) with the ultimate goal of making it easier for Rust developers to add shell completions to their CLI applications.

The crate is just a basic outline for now, parsing the arguments / environment variables passed in from the shell into a struct and allowing for some very simple completions. There is a lot still to be figured out, and to that end I’ve opened several issues for discussion:

- [compgen completions tracking issue](https://github.com/JoshMcguigan/shell_completion/issues/1)
- [cross platform support](https://github.com/JoshMcguigan/shell_completion/issues/2)
- [derive completion scripts](https://github.com/JoshMcguigan/shell_completion/issues/3)
- [higher level API](https://github.com/JoshMcguigan/shell_completion/issues/4)

Any and all contributions are welcome, whether in the form of discussion on the issues above, creating new issues for things I’ve missed, or code contributions in the form of PRs.

## Conclusion
Shell completions go a long way toward improving the user experience of a CLI application. By building tooling to allow developers to write and deploy completion scripts the same way they write/deploy their application, we can encourage more developers to add shell completions to their CLI applications. There is a lot of work that can be done in this space, and I’ve attempted to nudge the community one small step closer to that goal with the publishing of the [shell_completions](https://github.com/joshmcguigan/shell_completion) crate.
