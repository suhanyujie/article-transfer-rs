# Rust 的 Result 类型入门
>* A Primer on Rust’s Result Type 译文

>* 原文链接：https://medium.com/@JoeKreydt/a-primer-on-rusts-result-type-66363cf18e6a
>* 原文作者：[Joe Kreydt](https://medium.com/@JoeKreydt?)
>* 译文出处：https://github.com/suhanyujie/article-transfer-rs
>* 译者：[suhanyujie](https://www.github.com/suhanyujie)

* ![](https://miro.medium.com/max/2099/1*AoZOz1AJS15yyB3TLUn93A.jpeg)
The Result type is the de facto way of handling errors in Rust, and it is clever; maybe too clever.

Result is not exactly intuitive for those learning Rust, and trying to figure out how to use it by reading its documentation page is a good way to turn your brain into a french fry. That’s fine if you’re hungry, but if you need to handle an error or use a function that returns a Result type (and many do), then it’s not cool.

In hopes of saving a few brains, I am going to explain Rust’s Result in plain English.

# What is Result?
## According to the Rust book
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

In this program, the user is prompted to enter a number. Then, the user’s input is read and stored as a string type. We want a number type, not a string, so we use the parse() function to convert the string to a 64 bit floating point number (f64).

If the user enters a number when prompted, the parse() function should have no issues converting the user’s input string into f64. Yet we get an error.

The error occurs because the parse() function does not just take the string, convert it to a number, and return the number that it converted. Instead, it takes the string, converts it to a number, then returns a Result type. The Result type needs to be unwrapped to get the converted number.

## Fixing the Error with Unwrap() or Expect()
The converted number can be extracted from the Result type by appending the unwrap() function after the parse() like so:

```rust
let my_number: f64 = my_string.trim().parse().unwrap();
```

The unwrap() function looks at the Result type, which is either Ok or Err. If the Result is Ok, unwrap() returns the converted value. If the Result is Err, unwrap() lets the program crash.

![](https://miro.medium.com/max/470/1*bPYM5NAZ8OYenRAejcI7uA.gif)

You can also handle a Result with the expect() function like so:

```rust
let my_number: f64 = my_string.trim().parse().expect(“Parse failed”);
```

The way expect() works is similar to unwrap(), but if the Result is Err, expect() will let the program crash and display the string that was passed to it. In the example, that string is “Parse failed.”

## The Problem with Unwrap() and Expect()
When using the unwrap() and expect() functions, the program crashes if there is an error. That is okay when there is very little chance of an error occurring, but in some cases an error is more likely to occur.

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
- We need to make sure the user entered a number and not something else, like a word or a letter.
- Rust’s read_line() function takes the user’s input as a string. We need to convert that string to a floating-point number.
- If the user doesn’t enter a number, we need to clear out the variable that holds the user’s input and prompt the user to try again.

Part of problem number three (clearing the my_string variable) is solved in the first line inside the loop:

```rust
my_string.clear();
```

Next, we accept the user’s input with:

```rust
stdin().read_line(&mut my_string)
    .expect(“Did not enter a correct string”);
```

The read_line() function returns a Result type. We are handling that Result with the expect() function. That is fine in this situation because the chances of read_line() resulting in an error is extremely rare. A user typically cannot enter anything but a string in the terminal, and that is all read_line() needs in this case.

The string of user input returned by read_line() is stored in the my_string variable.

## The Juicy Part
Now that we have the user’s input stored in my_string, we need to convert the string to a floating-point number. The parse() function does that conversion, then returns the value in a Result. So we have more Result handling to do, but this time there is a good chance we will have an error on our hands. If the user enters anything but a number, parse() will return an error Result (Err). If that happens, we don’t want the program to crash. We want to tell the user that a number wasn’t entered, and to try again. For that, we need our code to do one thing if parse() succeed and something else if parse() failed. Queue the match expression.
