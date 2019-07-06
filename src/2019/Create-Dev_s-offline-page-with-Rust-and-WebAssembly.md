# 【译】使用 Rust 和 WebAssembly 构建离线画图页面

>* 原文地址：https://dev.to/sendilkumarn/create-dev-s-offline-page-with-rust-and-webassembly-21gn
>* 原文仓库：https://github.com/sendilkumarn/draw-page
>* 原文作者：[Sendil Kumar N](https://dev.to/sendilkumarn)
>* 译文出自：https://github.com/suhanyujie
>* 本文永久链接：（缺省）
>* 译者：[suhanyujie](https://github.com/suhanyujie)
>* 翻译不当之处，还请指出，谢谢！
>* 文中的页面效果可以参考这里：[离线画图页](https://dev.to/offline)

Dev 网站的[离线画图页](https://dev.to/offline)很有趣。我们能用 Rust 和 WebAssembly 来实现吗？

答案是肯定的。让我们现在就来实现它。

首先，我们通过 Webpack 创建了一个基于 Rust 和 WebAssembly 的简单应用。

```shell
npm init rust-webpack dev-offline-canvas
```

Rust 和 WebAssembly 生态提供了 `web_sys`，它在 Web API 上提供了很多需要的绑定。可以从[这里]((https://rustwasm.github.io/wasm-bindgen/api/web_sys/))查看。

示例应用已经引入了 `web_sys` 依赖。`web_sys` crate 中包含了所有可用的 WebAPI 绑定。

>如果引入所有的 WebAPI 绑定将会增加绑定文件的大小。按需引入必 API 是比较重要的。

我们移除已经存在的 feature 列表（位于 toml 文件中）

```toml
features = [
    'console'
]
```

并使用下面的替代：

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

上面的 features 列表是我们将在本例中需要使用的一些 features。

## 开始写 Rust 代码
打开文件 `src/lib.rs`。

使用下面的代码替换掉文件中的 `start()` 函数： 

```rust
#[wasm_bindgen(start)]
pub fn start() -> Result<(), JsValue> {

   Ok()
}
```

一旦实例化了 WebAssembly 模块，`#[wasm_bindgen(start)]` 就会调用这个函数。可以查看规范中关于 start 函数的[详细信息](https://github.com/WebAssembly/design/blob/master/Modules.md#module-start-function)。

我们在 Rust 中将得到 `window` 对象。

```rust
let window = web_sys::window().expect("should have a window in this context");
```

接着从 `window` 对象中获取 document。

```rust
let document = window.document().expect("window should have a document");
```

创建一个 Canvas 元素，将其插入到 document 中。

```rust
let canvas = document
         .create_element("canvas")?
         .dyn_into::<web_sys::HtmlCanvasElement>()?;

document.body().unwrap().append_child(&canvas)?;
```

设置 canvas 元素的宽、高和边框。

```rust
canvas.set_width(640);
canvas.set_height(480);
canvas.style().set_property("border", "solid")?;
```

在 Rust 中，一旦离开当前上下文或者函数已经 return，对应的内存就会被释放。但在 JavaScript 中，`window`, `document` 在页面的启动和运行时都是活动的（位于生命周期中）。

因此，为内存创建一个引用并使其静态化，直到程序运行结束，这一点很重要。

获取 Canvas 渲染的上下文，并在其外层包装一个 wrapper，以保证它的生命周期。

`RC` 表示 `Reference Counted`。

Rc 类型提供在堆中分配类型为 T 的值，并共享其所有权。在 Rc 上调用 clone 会生成指向堆中相同值的新的指针。当指向给定值的最后一个 Rc 指针即将被释放时，它指向的值也将被释放。 —— [RC 文档](https://doc.rust-lang.org/std/rc/struct.Rc.html)

这个引用被 clone 并用于回调方法。

```rust
let context = canvas
        .get_context("2d")?
        .unwrap()
        .dyn_into::<web_sys::CanvasRenderingContext2d>()?;

let context = Rc::new(context);
```

因为我们要响应 mouse 事件。因此我们将创建一个名为 `pressed` 的布尔类型的变量。`pressed` 用于保存 `mouse click`（鼠标点击）的当前值。

```rust
let pressed = Rc::new(Cell::new(false));
```

现在，我们需要为 `mouseDown`、`mouseUp`、`mouseMove` 创建一个闭包（回调函数）。

```rust
{ mouse_down(&context, &pressed, &canvas); }
{ mouse_move(&context, &pressed, &canvas); }
{ mouse_up(&context, &pressed, &canvas); }
```

我们将把这些事件触发时需要执行的操作定义为独立的函数。这些函数接收 canvas 元素的上下文和鼠标按下状态作为参数。

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

他们非常类似于你平时写的 `JavaScript` 的 API，但它们是用 Rust 编写的。

现在我们都设置好了。我们可以运行应用程序并在画布中画画。 🎉 🎉 🎉

但我们还没有设定颜色。

## 添加多个颜色
增加颜色样本，创建一个 div 列表，并使用它们作为颜色选择器。

在 `start` 函数中定义我们需要的颜色列表。

```rust
#[wasm_bindgen(start)]
pub fn start() -> Result<(), JsValue> {
    // ....... Some content
    let colors = vec!["#F4908E", "#F2F097", "#88B0DC", "#F7B5D1", "#53C4AF", "#FDE38C"];

    Ok()
}
```

然后遍历颜色列表，为所有颜色创建一个 div，并将其加入到 document 中。对于每个 div，还需要添加一个 `onClick` 处理程序来更改画板颜色。

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

其中 click 函数实现如下所示：

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

现在稍微美化一下。打开 `static/index.html` 文件。在其中添加 div 样式。

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

这就是我们的画板了，我们已经创建好了这个应用。🎉

可以从[这里](https://github.com/sendilkumarn/draw-page)检出示例应用。

希望这个例子能给你开启美妙的 WebAssembly 旅程带来灵感。如果你有任何的问题、建议、感受，欢迎给我留言评论。

你可以在 [Twitter](https://twitter.com/sendilkumarn) 关注我。

如果你喜欢这个文章，请给这个[文章](https://dev.to/aspittel/how-to-create-the-drawing-interaction-on-dev-s-offline-page-1mbe)点赞或留言。❤️ 

还可以阅读我的其他 WebAssembly 文章，[点击这儿](https://dev.to/sendilkumarn/increase-rust-and-webassembly-performance-382h)。
