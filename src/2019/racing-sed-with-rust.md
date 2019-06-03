# 通过 Rust 强化 sed

>* 原文地址：https://www.lambdafunctions.com/articles/racing-sed-with-rust
>* 原文作者：[lambdafunctions](https://www.lambdafunctions.com)
>* 译文出自：https://github.com/suhanyujie
>* 本文永久链接： https://github.com/suhanyujie/article-transfer-rs/blob/master/src/2019/racing-sed-with-rust.md
>* 译者：[suhanyujie](https://github.com/suhanyujie)

> 正文开始

作为我正在做的项目中的一部分，有时我发现自己不得不处理相当大的 X12 文件。对于那些还没有接触过的 X12 的人来说，X12 是电子数据交换的 ANSI 标准。它可以被认为是 XML 的鼻祖；它是为了满足同样的需求而设计的，但是在很久以前，单个字节是非常有价值的东西。标准委员会最初成立于 1979 年，所以它的年龄是比我还大的。

那又如何。

X12 has various issues, but the one that triggered this post is that a file often contains just a single line. An X12 file starts with a fixed-length 106-byte header called an ISA segment. Among other things, this header specifies three different terminator characters to be used in the rest of the document.
X12 有各种各样的问题，但是让我写这篇文章的原因是一个文件时常只包含一行。X12 文件以一个名为 ISA 段的固定长度 106 字节头开始的。初次之外，这个头指定了文档其余部分中使用的三个不同的结束符。


Once the fixed-length header is out of the way, the rest of the document comprises a sequence of variable-length records known as segments. The end of each segment is marked by a segment terminator character. A trailing newline is widely permitted by processing tools, but is not required.
一旦固定长度的头被排除了，文档的其余部分就由一些列可变长度的记录组成，这些记录称为段。每个段的末尾由段结束符标记。处理工具广泛允许使用拖尾换行，但在这里不需要。

The problem with everything being on a single line is that the vast majority of the standard Unix toolbox for data processing and exploration is designed to work with data a line at a time. For example, say you want to take a peek at the beginning of a large file to see what sort of records it contains: $ head < file will print the first 10 lines to your terminal. If the entire file you’re dealing with is a single 1.3 gigabyte line, this is less than helpful.
所有内容在一行中造成的问题是，用于数据处理和探索的标准 Unix 工具箱的绝大部分工具都是设计成一次一行的处理数据。例如，加入你想查看一个大文件的开头，以查看它是否包含：`$ head < file`，并向你的终端打印前 10 行。如果你正在处理的整个文件只有一行，大小是 1.3G，那么这就没有多大帮助了。


Of course, the toolbox does contain ways of dealing with this kind of problem. The traditional and most widely-used segment terminator in X12 is the tilde (~). If we wanted the first 10 lines of an X12 file, we could use sed to insert newlines after every tilde before piping the output to head:
当然，工具箱中确实包含了处理这类问题的方法。X12 中传统的、应用最广泛的段终止符是波浪号（~）。如果我们想要 X12 文件的前 10 行，我们可以使用 sed 在每个波浪线之后插入新的一行，然后再将内容输出到 `head`：

```
$ sed -e 's/~/~\n/g' < INPUT | head
```

（用这种方法对于大多数文档格式来说太过理想化，在我们的实例中是安全地，因为 X12 不支持“转义”等不必要的无用操作；由文件创建者来确保所选择的终止符不会出现在内容字段中。这就是为什么每个文档都可以指定自己的结束符。）

这是可行的，但它有点问题：
    * Using ~ as a terminator is extremely common, but not required. A general-purpose tool needs to look up the correct terminator in the header.
    * 使用 `~` 作为终止符非常常见，但不是必需的。通用工具需要在文件头部查找正确的终止符。
    * 你每次都要记住并手动输入
    * 它需要一次将整个源文件读入内存，如果是一个大文件，这就麻烦了。
    * 它还需要处理这个输入文件，即使 `head` 只取前 10 行。对于大文件，这可能需要一些时间。
    * 它不是幂等的。在已经有换行的文件上运行这个命令会得到双倍行距的行，如果有一个命令总是在每个段之后生成一个换行符，而不管输入文件中是什么，那就好了。

还有其他工具可以解决这些问题；例如，我们可以使用 Perl：

```shell
$ perl -pe 's/~[\n\r]*/~\n/g' < INPUT | head
```

这解决了幂等性问题吗，但没有解决其他的问题，而且这是更需要注意的。

我真正想要的是一个小型的、自包含的工具，我可以将一个 X12 文件传给它，并依赖它执行正确的操作，而不需要任何不必要的命令。由于我正在处理大型源文件，如果它至少包含像 sed 这样的标准工具一样快就好了。听起来像是。。。

## 用 Rust 来拯救
令人高兴的是，Rust 使编写这种命令行使用程序变得非常容易，而不会出现 c 语言中此类代码的问题。

既然我们对执行速度比较追求，那让我们设置一个速度基准。我在 Intel Core i9-7940X 的机器上运行 Linux。我将使用存储在 RAM 上的 1.3 GB X12 文件进行测试，该文件没有多余的行。

```shell
$ time sed -e 's/~/~\n/g' < testfile.x12 > /dev/null
# -> 7.65 seconds
```

现在我们有一些可以用来比较的数据了，让我们尝试一个简单的 Rust 版本程序：

```RUST
use aho_corasick::AhoCorasick;
use std::io::{self, stdin, stdout, BufReader, BufWriter};

fn main() -> io::Result<()> {
    let reader = BufReader::new(stdin());
    let writer = BufWriter::new(stdout());
    let patterns = &["~"];
    let replace_with = &["~\n"];
    let ac = AhoCorasick::new(patterns);
    ac.stream_replace_all(reader, writer, replace_with)
}
```

我们还没有做终止符检查，我们只通过 `STDIN` 和 `STDOUT` 处理 IO，但是用相同的文件下统计时间可以得到 1.68s，不行，还没达到要求。

Reading the correct terminator out of the ISA segment is a simple improvement:
从 ISA 段读取正确的终止符是一个简单的改进方法：

```rust
use aho_corasick::AhoCorasick;
use std::io::{self, stdin, stdout, Read, BufReader, BufWriter};
use std::str;

fn main() -> io::Result<()> {
    let mut reader = BufReader::new(stdin());
    let writer = BufWriter::new(stdout());

    let mut isa = vec![0u8; 106];
    reader.read_exact(&mut isa)?;
    let terminator = str::from_utf8(&isa[105..=105])
        .unwrap();

    let patterns = &[terminator];
    let replace_with = &[format!("{}\n", terminator)];
    AhoCorasick::new(patterns)
        .stream_replace_all(reader, writer, replace_with)
}
```

这将需要向源代码添加几行代码，但不会显著的影响运行时。（细心的人可能已经注意到这个版本没有编写 ISA 段，为了让代码更短，我忽略这一点。）


This is already a significant improvment over the sed one-liner in terms of speed, and it will automatically detect the correct terminator for us. Unfortunately, we start to run into difficulty with this approach if want to handle newlines correctly. We could easily replace all newlines, or we could replace a single newline following a terminator, but we can’t match “any number of newlines but only following a terminator.”
就速度而言，这已经是 sed 单行程序的一个较大改进，它将自动为我们检测正确的终止符。不幸的是，如果想正确处理换行，这种方法就会有困难。我们可以很容易地要么替换所有换行，要么替换终止符后面的单个换行，但是我们不能匹配“任意数量的换行，只能匹配终止符后面的换行”。

We could try applying a regular expression to the stream, but that seems like overkill for such a simple transformation. What if we process the bytestream ourselves without relying on the aho_corasick library?
我们可以尝试将正则表达式应用于流，但是对弈这样一个简单的转换来说，这样做似乎有些过了。如果自己处理字节流而不依赖 `aho_corasick` 库呢？

```rust
use std::io::{self, stdin, stdout, Read, BufReader, BufWriter, ErrorKind};
use byteorder::{ReadBytesExt, WriteBytesExt};

fn main() -> io::Result<()> {
    let mut reader = BufReader::new(stdin());
    let mut writer = BufWriter::new(stdout());

    let mut isa = vec![0u8; 106];
    reader.read_exact(&mut isa)?;
    let terminator = isa[105];

    loop {
        match reader.read_u8() {
            Ok(c) => {
                writer.write_u8(c)?;
                if c == terminator {
                    writer.write_u8(b'\n')?;
                }
            }
            Err(ref e) if e.kind() == ErrorKind::UnexpectedEof => {
                return Ok(());
            }
            Err(err) => {
                return Err(err);
            }
        };
    }
}
```

不会太长，而且，尽管我们在这个版本没有处理换行但至少我们有一个容易的地方为它们添加代码。

如何比较性能？

13 秒。哎呦，原来 `stream_replace_all` 为提高操作效率提供了很大助力。 

We can regain a lot of that time—at the cost of some more code—by managing our own buffer rather than relying on the BufReader and lots of 1-byte read_u8() calls:
通过管理我们自己的缓冲区，而不是依赖于 `BufReader` 和大量的 1-byte 的 `read_u8()` 调用，我们可以重新获得大量的时间 —— 但要付出更多的开销：

```rust
use std::io::{self, stdin, stdout, Read, Write, BufReader, BufWriter, ErrorKind};
use byteorder::{WriteBytesExt};

const BUF_SIZE: usize = 16384;

fn main() -> io::Result<()> {
    let mut reader = BufReader::new(stdin());
    let mut writer = BufWriter::new(stdout());

    let mut isa = vec![0u8; 106];
    reader.read_exact(&mut isa)?;
    let terminator = isa[105];

    let mut buf = vec![0u8; BUF_SIZE];
    loop {
        match reader.read(&mut buf) {
            Ok(0) => { return Ok(()) } // EOF
            Ok(n) => {
                let mut i = 0;
                let mut start = 0;

                loop {
                    if i == n {
                        // No terminator found in the rest
                        // of the buffer. Write it all out
                        // and read some more.
                        writer.write_all(&buf[start..])?;
                        break;
                    }
                    if buf[i] == terminator {
                        writer.write_all(&buf[start..=i])?;
                        writer.write_u8(b'\n')?;
                        start = i + 1;
                    }
                    i += 1;
                }
            }
            Err(ref e) if e.kind() == ErrorKind::UnexpectedEof => {
                return Ok(());
            }
            Err(err) => {
                return Err(err);
            }
        };
    }
}
```

As well as greatly reducing the number of read_u8 calls, we also gain a huge speedup by writing each segment in a single call to write_all (or two, for the last segment in a buffer) rather than a write_u8 call per character.
除了大大减少 `read_u8` 调用的数量，我们还通过在一个单独的调用中编写每个段到 `write_all`（对于缓冲区的最后一个段，可以编写两个段），而不是为每个字符编写一个 `write_u8` 调用，如此，大大提高了速度。

The code is starting to get a bit more involved, but we’re down to 1.75 seconds. That’s a bit better! But… still slower than the first version. What magic is going on there? A look at the dependencies of the aho_corasick crate offers a clue: it depends on memchr.
代码从这里开始变得有些复杂了，但是我们只需要 1.75s。这样好些了！但还是比第一个版本要慢。这是为什么？查看 `aho_corasick` crate 的依赖关系，它提供了一个线索：依赖于 `memchr`。


