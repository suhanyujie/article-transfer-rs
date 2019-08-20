# [译]从 Bash 到 Rust

>* 原文地址：https://wiredforge.com/blog/getpid/index.html
>* 原文作者：[Robert Masen](https://github.com/FreeMasen)
>* 译文出自：https://github.com/suhanyujie/article-transfer-rs
>* 本文永久链接： 
>* 译者：[suhanyujie](https://github.com/suhanyujie)

Over the past year, I have found myself writing bash scripts to automate redeploying my long running services. The process of pulling down changes, re-building the binary, packaging any javascript, killing the old process and starting the new process is something that seemed like it should be easy to automate. The only difficult thing about it would be looking up the old pid to kill the old process, otherwise it is somewhat straight forward. The redeploy script I came up with looks something like this.


```shell
git pull
npm install
webpack
cargo build --release
echo finding old pid
ID=`ps ax -o pid,args | egrep "[w]iredforge" | head -1 | sed -e 's/^[ \t]*// ' | cut -d ' ' -f 1`
if [ -z $ID ] then;
    echo no pid found
else
    echo 'killing pid: '$ID
    kill -kill $ID
fi
echo starting server
nohup ./target/release/wiredforge > wiredforge.log &
```

The key line we are going to concentrate on is:

```shell
ps -axo pid,args | egrep "[w]iredforge" | head -1 | sed -e 's/^[ \t]*// ' | cut -d ' ' -f 1
```

To break this down I first am running ps -axo pid,args, ps is a command that will list out currently running processes on unix-like computers. By passing ps the flags a and x I am making sure that the list will include processes started by other users as well as ones with no terminal associated with them. The o flag is used to define what output columns should be included, I want the columns pid (process id) and args (command line arguments that started the process). Running this command will output something like this.

```shell
PID  ARGS
1111 /bin/bash
2222 wiredforge
3333 pizza_freak
444  chat_bot
6666 robertmasen.pizza
```

Now that I have a clean list of pids and processes running, I pass that over to egrep with the pattern argument of my program name and I've wrapped the first letter in a regex character class, this will prevent egrep from matching itself. Now I have only the pids and args that match my search. Hopefully it looks like this.

```other
2222 wiredforge
```

To make sure I only have one line I pass the result of egrep to head which will just truncate the result to the first new line. Then I pass the result to sed, a stream editor, with the argument s/^[ \t]*//, by passing the s flag, I am telling sed that I want to do a substitution and the syntax here is /[target]/[new value]/. So /^[ \t]*// is saying replace any combinations of space or tab at the start of the text with nothing, effectively trimming any leading whitespace. Finally we pass the output of sed to cut with the arguments d which takes in the deliminator and the flag f which takes the desired position of the result, so split the text on spaces and give me the first part. All of that together should I should end up with 2222

This works ok, though it is difficult to remember when I want to add a redeploy script to a new long running process. So I decided I wanted to build a small program that would do most of this for me, not in bash.

My first thought was to use a std::process::Command to just run ps -axo pid,args which would look something like this.

```rust
fn ps() -> Vec<(String, String)> {
    let output = Command::new("ps").arg("-axo").arg("pid,args").output().expect("failed to get output from ps");
    let text = String::from_utf8_lossy(&output.stdout);
    text.lines().filter_map(parse_line).collect()
}

fn parse_line(line: &str) -> Option<(String, String)> {
    // get the index of the first space, return None if not found
    let first_space = line.find(' ')?;
    // break the string from the first space as the pid
    let pid = String::From(line[..first_space].trim());
    // use everything after the pid for the process
    let process = String::from(line[first_space..].trim());
    Some((pid, process))
}
```

This works out ok, though I have to worry about how I want to search the process and it just feels a little bit hacky. In addition, I found that more than 2 columns and you can't guarantee that the output will be complete, it sometimes gets truncated which would be an issue if I wanted to extend this to include python or nodejs programs. Looking into how ps gets its information I learned about procfs which is a virtual file system used by some unix-like systems to provide process information to users. The basic idea of procfs is that you can navigate to the folder /proc and in it there will be a folder for each process running, named with its pid. It has a lot more to offer than I can cover here but this is the thing we are most interested in. To give you an idea, it looks something like this.

```shell
cd /proc
ls
1 1111 2222 3333 444 6666
```

In each of these folders there is additional information about that process as files. The one I am primarily concerned with is comm, this has one line which is the executable name that started the process. Some others that are nice include cmdline which lists the full command line arguments used and exe which is a symlink to the actual executable. For for my program I only really need the comm information.

```shell
tail /proc/2222/comm
wiredforge
```

So, the basic flow would be to loop over the /proc folder's contents and log each directory's name as a pid, then read the contents of comm in that folder for the process name. I chose to use the walkdir library to make the loop a little easier.

```rust
fn get_processes() -> Vec<(usize, String)> {
    // loop over the files in /proc
    // min_depth(1) mean we don't want proc
    // max_depth(1) means we don't want to go into any directories that are inside of proc
    // this second thing is important because there are some restricted files and I don't
    // want to have to deal with that.
    // filter_map takes a function that returns an Option, so we can offload the actual
    // processing to process_entry
    // then collect what makes it through into a Vec<(usize, String)>
    WalkDir::new("/proc").min_depth(1).max_depth(1).filter_map(process_entry).collect()
}

fn process_entry(entry: Result<Entry, walkdir::Error>) -> Option<(usize, String)> {
    // pull the actual entry out of the result
    // returning None if it fails
    let entry = entry.ok()?;
    // if the entry isn't a directory, skip it
    if !entry.file_type().is_dir() {
        return None;
    }
    // try and convert the file name from an OsStr into an &str
    // returning None if it fails
    let pid = entry.file_name().to_str().ok()?.to_string();
    // try and parse that into a usize if the folder name isn't
    // a number we can just skip it
    let pid: usize = pid.parse().ok()?;
    // read the contents of the comm file to a string
    // returning None if it fails
    let comm = ::std::io::read_to_string(entry.path().join("comm")).ok()?;
    Some((pid, comm))
}
```

Exactly what we were looking for, the pid and a command name that will be easily matchable. Though, this will only work on unix-like computers that utilize procfs. Now lets put a UI in front of it, for this I am going to use docopt.

```rust
extern crate docopt;
extern crate walkdir;

use docopt::DocOpt;
use walkdir::WalkDir;

static HELP: &str = "
GET PID a tool for getting a pid for a running process.DocError
Usage:
    getpid <name>
    getpid [--help|-h]
Options:
    name       The name of the executable running
    --help -h  print this message
";
#[derive(Deserialize)]
struct Args {
    arg_name: String,
}

fn main() -> Result<(), Error> {
    // parse the command line arguments
    // and exit with help text if fails
    let args: Args = Docopt::new(HELP)
                .and_then(|d| d.deserialize())
                .unwrap_or_else(|e| e.exit());
    // if the arg is an empty string
    // show the help text and exit
    if args.arg_name == String::new() {
        println!("{}", HELP);
        ::std::process::exit(0);
    }
    // get the processes and keep only those
    // with a matching name
    let matches = get_processes().filter(|p| p.1 == args.arg_name);
    if matches.len() < 1 {
        // if no matches were found
        // print this message to stderr
        eprintln!("No process found")
    } else if matches.len() > 1 {
        // if more than one match was found
        // print this message to stderr
        eprintln!("More than one process with that name");
    } else {
        println!("{}", matches[0].0)
    }
}
```

The one thing I will note here is that it is important we are logging to stderr for failures with eprintln!, since I am going to be assigning the stdout value to a variable in a bash script, I only want to print to stdout on success. With that, let's test it out, a good test would be to just pass the name of the program to itself.

```shell
cargo run -- getpid
8888
```

That works great for my Ubuntu server but macOS doesn't utilize procfs, so on the computer I do most of my development, it will fail. Less important but it would still be nice to use it in both places.

Looking into how I would get pids and process names from macOS, I learned about an interface called sysctl. This is the primary way for programs to get information about the system they are running on. The api for this is defined in a c header file called sys/sysctl.h, that means we can use the tool bindgen to make a rust bindings file for us. To start, we need to get bindgen, one way to do that is by having cargo install it.

```shell
cargo install bindgen
```

The above command will download the rust source, compile it for you and then put it in your path. The rust bindgen tutorial uses a build.rs to re-create the bindings every time it is built, I don't want to do that since it creates a bit more complexity. However, the tutorial does point out that an easy way to generate bindings for a system library is to create a c header file as a wrapper around the bindings we want. Mine looks like this.

```c
#include <sys/sysctl>
```

That is all you need to include in your file, I named mine wrapper.h. Once this is done we are going to run bindgen.

```shell
bindgen ./wrapper.h -o ./src/bindings.rs
```

This will generate a file for us with all of the definitions we will need. At the top of bindings.rs you probably want to add the following attributes since the c header violates almost all of rust's syntax rules.

```rust
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(unused)]
```

This will silence all the warnings, there are a lot of warnings. Once that is done we want to declare it in our main.rs file. We need to make sure that it only gets included if we are compiling on a mac though so we are going to add a cfg attribute. I am going to also add a second module that will act as our interface for these bindings called sysctl.

```rust
#[cfg(target_os = "macos")]
mod bindings;
#[cfg(target_os = "macos")]
mod sysctl;
```

Now lets create our interface, create a file ./src/sysctl.rs and add the following to the top.

```rust
#![cfg(target_os = "macos")]
use bindings::{kinfo_proc, sysctl};

pub fn get_processes() -> Vec<(String, String)> {

}
```

Now we are going to start using the bindings, this can get a little hairy since when interoperating with c we will lose all of the rust niceties. Our goal is going to be to reproduce this stack overflow answer but with a few twists. Essentially we will be building a Vec of kinfo_procs from raw memory and then returning the kp_proc.p_pid and kp_proc.p_comm properties as strings. To start we need to create some variables.

```rust
// this resolves in sysctl to kern.procs.all
let mut name: [i32; 4] = [1, 14, 0, 0];
// the length of the above name
let name_len: u32 = 4;
// This will be the length of bytes returned
// for our Vec
let mut len = 0;
```

Now that we have those, we need to calculate the len, we do this with a call to sysctl. Since we are inter-operating with another programing language, we are going to need to write some unsafe code blocks.

```rust
let mut err: i32 = unsafe {
    sysctl(
        // First argument is the name as a pointer
        name.as_mut_ptr(),
        // Next is the length of that name
        name_len,
        // Here we are sending a pointer to NULL
        ::std::ptr::null_mut(),
        // This value will hold our return value
        &mut len,
        // Here we are again sending a pointer to NULL
        ::std::ptr::null_mut(),
        // The last argument is 0
        0
    )
};
// if this is > 0 we have encountered an error
if err > 0 {
    eprintln!("Error getting length of list: {}", err);
    return vec![];
}
```
