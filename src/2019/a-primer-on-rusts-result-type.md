# Rust 的 Result 类型入门
>* A Primer on Rust’s Result Type 译文

>* 原文链接：https://medium.com/@JoeKreydt/a-primer-on-rusts-result-type-66363cf18e6a
>* 原文作者：[Joe Kreydt](https://medium.com/@JoeKreydt?)
>* 译文出处：https://github.com/suhanyujie/article-transfer-rs
>* 译者：[suhanyujie](https://www.github.com/suhanyujie)

* ![](https://miro.medium.com/max/2099/1*AoZOz1AJS15yyB3TLUn93A.jpeg)
The Result type is the de facto way of handling errors in Rust, and it is clever; maybe too clever.

Result is not exactly intuitive for those learning Rust, and trying to figure out how to use it by reading [its documentation page is](https://doc.rust-lang.org/std/result/) a good way to turn your brain into a french fry. That’s fine if you’re hungry, but if you need to handle an error or use a function that returns a Result type (and many do), then it’s not cool.

In hopes of saving a few brains, I am going to explain Rust’s Result in plain English.

# What is Result?
## According to [the Rust book](https://doc.rust-lang.org/1.30.0/book/first-edition/error-handling.html)
“Result expresses the possibility of error. Usually, the error is used to explain why the execution of some computation failed.”

![](https://miro.medium.com/max/400/1*g1A-DkLZ6dPjOKTo4kzrGg.gif)

## In Plain English
Result is a type returned by a function that can be either Ok or Err. If it is Ok, then the function completed as expected. If it is Err, then the function resulted in an error.

# What does Result do?
## According to the Rust book
“The Result type is a way of representing one of two possible outcomes in a computation. By convention, one outcome is meant to be expected or “Ok” while the other outcome is meant to be unexpected or “Err”.”

## In English, Please
Functions return values. Those values are of a certain data type. A function can return a value of the Result type. The Result type changes based on whether the function executed as expected or not. Then, the programmer can write some code that will do A if the function executed as expected, or B if the function encountered a problem.

# Error If You Don’t Handle a Result

```
error[E0308]: mismatched types
  --> main.rs:20:26
   |
20 |     let my_number: f64 = my_string.trim().parse(); //.unwrap();
   |                          ^^^^^^^^^^^^^^^^^^^^^^^^ expected f64, found enum `std::result::Result`
   |
   = note: expected type `f64`
              found type `std::result::Result<_, _>`
error: aborting due to previous error
For more information about this error, try `rustc --explain E0308`.
compiler exit status 1
```

The key part of the error is, “expected f64, found enum.” In similar scenarios, you might also see:
    - “expected u32, found enum”
    - “expected String, found enum”
    - “expected [insert type here], found enum”

**If you get an error like the ones above, it is because you need to handle a Result that has been returned by a function in your program.**

# A Program that Results in the Error

```rust
use std::io::{stdin, self, Write};
fn main(){
    let mut my_string = String::new();
    print!(“Enter a number: “);
    io::stdout().flush().unwrap();
    stdin().read_line(&mut my_string)
        .expect(“Did not enter a correct string”);
    let my_number: f64 = my_string.trim().parse();
    println!(“Yay! You entered a number. It was {:?}”, my_num);
}
```

In this program, the user is prompted to enter a number. Then, the user’s input is read and stored as a string type. We want a number type, not a string, so we use the _parse()_ function to convert the string to a 64 bit floating point number (f64).

If the user enters a number when prompted, the _parse()_ function should have no issues converting the user’s input string into f64. Yet we get an error.

The error occurs because the _parse()_ function does not just take the string, convert it to a number, and return the number that it converted. Instead, it takes the string, converts it to a number, then returns a Result type. The Result type needs to be unwrapped to get the converted number.

## Fixing the Error with Unwrap() or Expect()
The converted number can be extracted from the Result type by appending the _unwrap()_ function after the _parse()_ like so:

```rust
let my_number: f64 = my_string.trim().parse().unwrap();
```

The _unwrap()_ function looks at the Result type, which is either _Ok_ or _Err_. If the Result is _Ok_, _unwrap()_ returns the converted value. If the Result is _Err_, _unwrap()_ lets the program crash.

![](https://miro.medium.com/max/470/1*bPYM5NAZ8OYenRAejcI7uA.gif)

You can also handle a Result with the _expect()_ function like so:

```rust
let my_number: f64 = my_string.trim().parse().expect(“Parse failed”);
```

The way _expect()_ works is similar to _unwrap()_, but if the Result is _Err_, _expect()_ will let the program crash **and** display the string that was passed to it. In the example, that string is “Parse failed.”

## The Problem with Unwrap() and Expect()
When using the _unwrap()_ and _expect()_ functions, the program crashes if there is an error. That is okay when there is very little chance of an error occurring, but in some cases an error is more likely to occur.

In our example, it is likely that the user will make a typo, entering something other than a number (maybe a letter or a symbol). We do not want the program to crash every time the user makes a mistake. Instead, we want to tell the user to try entering the number again. This is where Result becomes very useful, especially when it is combined with a match expression.

# Fixing the Error with a Match Expression

```rust
use std::io::{stdin, self, Write};
fn main(){
    let mut my_string = String::new();
    print!(“Enter a number: “);
    io::stdout().flush().unwrap();
    let my_num = loop {
        my_string.clear();
        stdin().read_line(&mut my_string)
            .expect(“Did not enter a correct string”);
        match my_string.trim().parse::<f64>() {
            Ok(_s) => break _s,
            Err(_err) => println!(“Try again. Enter a number.”)
        }
    };
    println!(“You entered {:?}”, my_num);
}
```

If you ask me, that is some fun code!

The main difference between the fixed program and the broken program above is inside the loop. Let’s break it down.

# The Code Explained
Before the loop, we prompted the user to enter a number. Then we declared my_num.

We assign the value returned from the loop (the user’s input, which will have been converted from a string to a number) to my_num:

```rust
let my_num = loop {
```

Within the loop, we handle the user’s input. After we accept the input, there are three problems we need to solve.

- 1.We need to make sure the user entered a number and not something else, like a word or a letter.
- 2.Rust’s _read_line()_ function takes the user’s input as a string. We need to convert that string to a floating-point number.
- 3.If the user doesn’t enter a number, we need to clear out the variable that holds the user’s input and prompt the user to try again.

Part of problem number three (clearing the my_string variable) is solved in the first line inside the loop:

```rust
my_string.clear();
```

Next, we accept the user’s input with:

```rust
stdin().read_line(&mut my_string)
    .expect(“Did not enter a correct string”);
```

The _read_line()_ function returns a Result type. We are handling that Result with the _expect()_ function. That is fine in this situation because the chances of _read_line()_ resulting in an error is extremely rare. A user typically cannot enter anything but a string in the terminal, and that is all _read_line()_ needs in this case.

The string of user input returned by _read_line()_ is stored in the _my_string_ variable.

## The Juicy Part
Now that we have the user’s input stored in my_string, we need to convert the string to a floating-point number. The _parse()_ function does that conversion, then returns the value in a Result. So we have more Result handling to do, but this time there is a good chance we will have an error on our hands. If the user enters anything but a number, _parse()_ will return an error Result (_Err_). If that happens, we don’t want the program to crash. We want to tell the user that a number wasn’t entered, and to try again. For that, we need our code to do one thing if _parse()_ succeed and something else if _parse()_ failed. Queue the match expression.

## Dissecting the Match Expression

```rust
match my_string.trim().parse::<f64>() {
    Ok(_s) => break _s,
    Err(_err) => println!(“Try again. Enter a number.”)
}
```

First, we use the match keyword to declare our match expression. Then, we provide the value that will be analyzed by the match expression. That value comes from:

```rust
my_string.trim().parse::<f64>()
```

That little chunk of code takes my_string, which is holding the user’s input, and feeds it into the _trim()_ function. _Trim()_ just removes any extra lines or spaces that might have tagged along with the user’s input. We need _trim()_ because _read_line()_ attaches an extra line to the input, which messes up the type conversion. The cleaned up my_string is then passed into the _parse()_ function which attempts to convert it into a floating-point number.

If _parse()_ successfully converts my_string to a number, it returns Ok. Within that Ok, we can find our floating-point number. If the user entered something other than a number, then _parse()_ will not be able to complete the conversion. If _parse()_ can’t do its job, it returns Err.

Within the curly brackets (the body) of our match expression, we tell the computer what to do based on what type _parse()_ returns:

```rust
Ok(_s) => break _s,
Err(_err) => println!(“Try again. Enter a number.”)
```

**If the Result is Ok**, _parse()_ was able to convert the type. In that case, we call a break, which stops the loop from looping, and we return the value stored in the Ok, which has been placed in the _s variable.

**If the Result is Err**, _parse()_ was not able to convert the type. In that case, we tell the user to “Try again. Enter a number.” Since we do not call a break, the loop starts over.

>If I had to explain the whole Result thing in one sentence, it would be: If a function returns a Result, a match expression can execute different code based on whether the Result is Ok or Err.

## Using Result in Your Own Functions
Now that you understand how to handle Result, you might want to use it in some of the functions you create.

Let’s look at an example.

```rust
fn main(){
    let my_num = 50;
    
    fn is_it_fifty(num: u32) -> Result<u32, &’static str> {
        let error = “It didn’t work”;
        if num == 50 {
            Ok(num)
        } else {
            Err(error)
        }
    }
    match is_it_fifty(my_num) {
        Ok(_v) => println!(“Good! my_num is 50”),
        Err(_e) => println!(“Error. my_num is {:?}”, my_num)
    }
}
```

This program checks the value of _my_num_. If the value is 50, then it shows success, but if the value is not 50, it shows an error.

The _is_it_fifty()_ function is the main specimen here. It is the declared function that returns a Result. Let’s look at in line by line.

First, we declare _my_num_ and set its value. Then, we declare the _is_it_fifty()_ function:

```rust
fn is_it_fifty(num: u32) -> Result<u32, &’static str> {
```

In our declaration, we specify that the function will take a parameter named num that is a 32 bit unsigned integer type (u32). Next, we specify the function’s return type. It will return a Result. The type returned in the Result will be either u32 or a string literal (&’static str).

Then, we enter the body of our _is_it_fifty()_ function.

```rust
let error = “It didn’t work”;
if num == 50 {
    Ok(num)
} else {
    Err(error)
}
```

The code in the body is an if else expression. It evaluates the argument passed into the function.
If it is 50, then the function will return the Result Ok. The Ok will contain the value that was passed into the function (_num_).

If the argument is anything other than 50, the function will return Result Err. The _Err_ will contain the value of the error variable which is, “It didn’t work.”

Any time that function is used, the Result it returns must be handled. In our program, as with most Rust programs, that is done with a match expression. I described the match expression part earlier.

The Result can also be handled with _unwrap()_ or _expect()_ — also explained earlier.

![](https://miro.medium.com/max/480/1*ZLronSWbmj4IwGoWepecHQ.gif)

## Summing It All Up
Result is a type returned by a function that indicates whether the function ran successfully.

Many of Rust’s built-in functions return a Result, and when that is the case, there is no way to avoid it. If a function returns a Result, it must be handled.

Common ways of handling a Result are the _unwrap()_ function, the _expect()_ function, and a match expression.

It is possible to return a Result from your own functions. It is a great way of accounting for errors.

That’s pretty much all you need to know about Rust’s Result type, but in case you want to learn a bit more, or understand where I gathered my information, below is a list of the resources I used.

## Resources
* https://doc.rust-lang.org/std/result/
* https://doc.rust-lang.org/1.2.0/book/match.html
    * See section: matching on enums
* https://doc.rust-lang.org/1.30.0/book/first-edition/error-handling.html
* https://doc.rust-lang.org/rust-by-example/flow_control/match.html
* https://blog.jonstodle.com/things-i-enjoy-in-rust-error-handling/
* https://stevedonovan.github.io/rust-gentle-intro/6-error-handling.html
* https://doc.rust-lang.org/book/ch03-03-how-functions-work.html
* https://doc.rust-lang.org/std/result/enum.Result.html#method.expect
