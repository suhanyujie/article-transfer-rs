# Create Dev's offline page with Rust and WebAssembly 🦄💡✨
# 【译】使用 Rust 和 WebAssembly 构建离线画图页面

>* 原文地址：https://dev.to/sendilkumarn/create-dev-s-offline-page-with-rust-and-webassembly-21gn
>* 原文仓库：https://github.com/sendilkumarn/draw-page
>* 原文作者：[Sendil Kumar N](https://dev.to/sendilkumarn)
>* 译文出自：https://github.com/suhanyujie
>* 本文永久链接：（缺省）
>* 译者：[suhanyujie](https://github.com/suhanyujie)
>* 翻译不当之处，还请指出，谢谢！

Dev's [offline page](https://dev.to/offline) is fun. Can we do that with Rust and WebAssembly?

The answer is yes. Let us do it.

First, we will create a simple Rust and WebAssembly application with Webpack.

```shell
npm init rust-webpack dev-offline-canvas
```

The Rust and WebAssembly ecosystem provides `web_sys` that provides the necessary binding over the Web APIs. Check it out [here](https://rustwasm.github.io/wasm-bindgen/api/web_sys/).

The sample application already has `web_sys` dependency. The `web_sys` crate includes all the available WebAPI bindings.

>Including all the WebAPI bindings will increase the binding file size. It is very important to include only the APIs that we need.

We will remove the existing feature

```toml
features = [
    'console'
]
```

and replace it with the following:

```toml
features = [
  'CanvasRenderingContext2d',
  'CssStyleDeclaration',
  'Document',
  'Element',
  'EventTarget',
  'HtmlCanvasElement',
  'HtmlElement',
  'MouseEvent',
  'Node',
  'Window',
]
```

The above list of features is the entire set of features that we will be using in this example.

## Lets write some Rust
Open the `src/lib.rs`.

replace the `start()` function with the following:

```rust
#[wasm_bindgen(start)]
pub fn start() -> Result<(), JsValue> {

   Ok()
}
```

The `#[wasm_bindgen(start)]` calls this function as soon as the WebAssembly Module is instantiated. Check out more about the start function in the spec [here](https://github.com/WebAssembly/design/blob/master/Modules.md#module-start-function).

We will get the `window` object in the Rust.

```rust
let window = web_sys::window().expect("should have a window in this context");
```

Then get the document from the `window` object.

```rust
let document = window.document().expect("window should have a document");
```

Create a Canvas element and append it to the document.

```rust
let canvas = document
         .create_element("canvas")?
         .dyn_into::<web_sys::HtmlCanvasElement>()?;

document.body().unwrap().append_child(&canvas)?;
```

Set width, height, and the border for the canvas element.

```rust
canvas.set_width(640);
canvas.set_height(480);
canvas.style().set_property("border", "solid")?;
```

In the Rust, the memories are discarded once the execution goes out of context or when the method returns any value. But in JavaScript, the `window`, `document` is alive as long as the page is up and running.

So it is important to create a reference for the memory and make it live statically until the program is completely shut down.

Get the Canvas' rendering context and create a wrapper around it in order to preserve its lifetime.

`RC` stands for `Reference Counted`.

The type Rc provides shared ownership of a value of type T, allocated in the heap. Invoking clone on Rc produces a new pointer to the same value in the heap. When the last Rc pointer to a given value is destroyed, the pointed-to value is also destroyed. - [RC docs](https://doc.rust-lang.org/std/rc/struct.Rc.html)

This reference is cloned and used for callback methods.

```rust
let context = canvas
        .get_context("2d")?
        .unwrap()
        .dyn_into::<web_sys::CanvasRenderingContext2d>()?;

let context = Rc::new(context);
```

Since we are going to capture the mouse events. We will create a boolean variable called `pressed`. The `pressed` will hold the current value of `mouse click`.

```rust
let pressed = Rc::new(Cell::new(false));
```

Now we need to create a closure (call back function) for `mouseDown` | `mouseUp` | `mouseMove`.

```rust
{ mouse_down(&context, &pressed, &canvas); }
{ mouse_move(&context, &pressed, &canvas); }
{ mouse_up(&context, &pressed, &canvas); }
```

We will define the actions that we need to do during those events as separate functions. These functions take the context of the Canvas element and pressed status.

```rust
fn mouse_up(context: &std::rc::Rc<web_sys::CanvasRenderingContext2d>, pressed: &std::rc::Rc<std::cell::Cell<bool>>, canvas: &web_sys::HtmlCanvasElement) {
    let context = context.clone();
    let pressed = pressed.clone();
    let closure = Closure::wrap(Box::new(move |event: web_sys::MouseEvent| {
        pressed.set(false);
        context.line_to(event.offset_x() as f64, event.offset_y() as f64);
        context.stroke();
    }) as Box<dyn FnMut(_)>);
    canvas.add_event_listener_with_callback("mouseup", closure.as_ref().unchecked_ref()).unwrap();
    closure.forget();
}

fn mouse_move(context: &std::rc::Rc<web_sys::CanvasRenderingContext2d>, pressed: &std::rc::Rc<std::cell::Cell<bool>>, canvas: &web_sys::HtmlCanvasElement){
    let context = context.clone();
    let pressed = pressed.clone();
    let closure = Closure::wrap(Box::new(move |event: web_sys::MouseEvent| {
        if pressed.get() {
            context.line_to(event.offset_x() as f64, event.offset_y() as f64);
            context.stroke();
            context.begin_path();
            context.move_to(event.offset_x() as f64, event.offset_y() as f64);
        }
    }) as Box<dyn FnMut(_)>);
    canvas.add_event_listener_with_callback("mousemove", closure.as_ref().unchecked_ref()).unwrap();
    closure.forget();
}

fn mouse_down(context: &std::rc::Rc<web_sys::CanvasRenderingContext2d>, pressed: &std::rc::Rc<std::cell::Cell<bool>>, canvas: &web_sys::HtmlCanvasElement){
    let context = context.clone();
    let pressed = pressed.clone();

    let closure = Closure::wrap(Box::new(move |event: web_sys::MouseEvent| {
        context.begin_path();
        context.set_line_width(5.0);
        context.move_to(event.offset_x() as f64, event.offset_y() as f64);
        pressed.set(true);
    }) as Box<dyn FnMut(_)>);
    canvas.add_event_listener_with_callback("mousedown", closure.as_ref().unchecked_ref()).unwrap();
    closure.forget();
}
```

They are very similar to how your `JavaScript` API will look like but they are written in Rust.

Now we are all set. We can run the application and draw inside the canvas. 🎉 🎉 🎉

But we do not have any colours.

## Lets add some colours.
To add the colour swatches. Create a list of divs and use them as a selector.

Define the list of colours that we need to add inside the `start` program.

```rust
#[wasm_bindgen(start)]
pub fn start() -> Result<(), JsValue> {
    // ....... Some content
    let colors = vec!["#F4908E", "#F2F097", "#88B0DC", "#F7B5D1", "#53C4AF", "#FDE38C"];

    Ok()
}
```

Then run through the list and create a div for all the colours and append it to the document. For every div add an `onClick` handler too to change the colour.

```rust
for c in colors {
    let div = document
        .create_element("div")?
        .dyn_into::<web_sys::HtmlElement>()?;
    div.set_class_name("color");
    {
        click(&context, &div, c.clone());  // On Click Closure.
    }

    div.style().set_property("background-color", c);
    let div = div.dyn_into::<web_sys::Node>()?;
    document.body().unwrap().append_child(&div)?;
}
```

The click hander is as follows:

```rust
fn click(context: &std::rc::Rc<web_sys::CanvasRenderingContext2d>, div: &web_sys::HtmlElement, c: &str) {
    let context = context.clone();
    let c = JsValue::from(String::from(c));
    let closure = Closure::wrap(Box::new(move || {
        context.set_stroke_style(&c);            
    }) as Box<dyn FnMut()>);

    div.set_onclick(Some(closure.as_ref().unchecked_ref()));
    closure.forget();
}
```

Now a little beautification. Open the `static/index.html` and add the style for the colour div.

```css
<style>
       .color {
            display: inline-block;
            width: 50px;
            height: 50px;
            border-radius: 50%;
            cursor: pointer;
            margin: 10px;
       }
 </style>
```

That is it, we have created the application. 🎉

Check out the demo application available [here](https://github.com/sendilkumarn/draw-page).

I hope this gives you a motivation to start your awesome WebAssembly journey. If you have any questions/suggestions/feel that I missed something feel free to add a comment.

You can follow me on [Twitter](https://twitter.com/sendilkumarn).

If you like this article, please leave a like or a comment. ❤️ for the [article](https://dev.to/aspittel/how-to-create-the-drawing-interaction-on-dev-s-offline-page-1mbe).

Check out my more WebAssembly articles [here](https://dev.to/sendilkumarn/increase-rust-and-webassembly-performance-382h).

