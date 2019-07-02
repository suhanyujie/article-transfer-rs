# [转] Go 的并发模式：Context
- tips：昨天看了飞雪无情的关于 Context 的文章，对 go 中 Context 有了一个初步的认识。今天看到一个 go [官方博客](https://blog.golang.org/context)的关于 Context 的介绍。准备自己翻译，发现网上有译文了，我就不做重复劳动了，转载一下，并注明出处。感谢译者的分享。

>* 原文地址：https://blog.golang.org/context
>* 原译文地址：https://brantou.github.io/2017/05/19/go-concurrency-patterns-context/

在go服务端，每个传入的 request 都在自己的 goroutine 中做后续处理。 request handlers 经常启动其他 goroutines 以访问后端，如数据库和rpc服务。 服务于 request 的一组常用典型的 goroutines 访问特定的请求值，例如最终用户的身份，授权令牌和请求的截止日期。 当 request 被取消或触发超时时，在该 request 上工作的所有 goroutine 应该快速退出，以便系统可以回收所使用的任何资源。

在google内部，开发了一个 context 包，可以轻松地跨越api边界,传递请求范围值，取消信号和截止日期到 request 所涉及的所有 goroutine 。 该包是开源的被称作 [context](https://golang.org/pkg/contextkj)。 本文介绍了如何使用该包并提供了一个完整的工作示例。

## context
context 包的核心就是 context 类型(这里的描述是精简的，详情可见 [godoc](https://golang.org/pkg/context))：

```go
// a context carries a deadline, cancelation signal, and request-scoped values
// across api boundaries. its methods are safe for simultaneous use by multiple
// goroutines.
type Context interface {
    // done returns a channel that is closed when this context is canceled
    // or times out.
    Done() <-chan struct{}

    // err indicates why this context was canceled, after the done channel
    // is closed.
    Err() error

    // deadline returns the time when this context will be canceled, if any.
    Deadline() (deadline time.time, ok bool)

    // value returns the value associated with key or nil if none.
    Value(key interface{}) interface{}
}
```

Done 方法返回一个 channel ，用于发送取消信号(代表 Context 已关闭)到运行时函数：当 channel 关闭时，函数应该放弃后续流程并返回。 Err 方法返回一个错误，指出为什么 context 被取消。 管道和取消文章更详细地讨论了 done channel 的惯用法。

Done 方法返回一个 channel ，用于发送取消信号(代表 Context 已关闭)到运行时函数：当 channel 关闭时，函数应该放弃后续流程并返回。 Err 方法返回一个错误，指出为什么 context 被取消。 管道和取消文章更详细地讨论了 done channel 的惯用法。

由于 Done channel 只接收的原因，/Context/ 没有取消方法：接收取消信号的函数通常不应当具备发送信号的功能。 特别是，当父操作启动子操作的 goroutines 时，这些子操作不应该能够取消父操作。 相反， WithCancel 函数（如下所述）提供了一种取消新的 Context 值的方法。

Context 可以安全地同时用于多个 goroutines 。 代码可以将单个 Context 传递给任意数量的 goroutine ，并能发送取消该Context的信号到所有的关联的 goroutine 。

Deadline 方法允许功能确定是否应该开始工作; 如果剩下的时间太少，可能不值得。 代码中也可能会使用截止时间来为I/O操作设置超时。

Value 允许 Context 传送请求数据。 该数据必须能安全的同时用于多个 goroutine 。

## Context的衍生
context/包提供了从现有 /Context 衍生出新的 Context 的函数。 这些 Context 形成一个树状的层级结构：当一个 Context 被取消时，从它衍生出的所有 Context 也被取消。

Background 是任何Context树的根; 它永远不会被取消：

```go
// Background returns an empty Context. It is never canceled, has no deadline,
// and has no values. Background is typically used in main, init, and tests,
// and as the top-level Context for incoming requests.
func Background() Context
```

WithCancel 和 WithTimeout 返回衍生出的 Context ，衍生出的子 Context 可早于父 Context 被取消。 与传入的 request 相关联的上下文通常在请求处理程序返回时被取消。 WithCancel 也可用于在使用多个副本时取消冗余请求。 WithTimeout 对设置后台服务器请求的最后期限很有用：

```go
// WithCancel returns a copy of parent whose Done channel is closed as soon as
 // parent.Done is closed or cancel is called.
 func WithCancel(parent Context) (ctx Context, cancel CancelFunc)

 // A CancelFunc cancels a Context.
 type CancelFunc func()

 // WithTimeout returns a copy of parent whose Done channel is closed as soon as
 // parent.Done is closed, cancel is called, or timeout elapses. The new
 // Context's Deadline is the sooner of now+timeout and the parent's deadline, if
 // any. If the timer is still running, the cancel function releases its
 // resources.
 func WithTimeout(parent Context, timeout time.Duration) (Context, CancelFunc)


// WithDeadline returns a copy of the parent context with the deadline adjusted
// to be no later than d. If the parent's deadline is already earlier than d,
// WithDeadline(parent, d) is semantically equivalent to parent. The returned
// context's Done channel is closed when the deadline expires, when the returned
// cancel function is called, or when the parent context's Done channel is
// closed, whichever happens first.
//
// Canceling this context releases resources associated with it, so code should
// call cancel as soon as the operations running in this Context complete.
func WithDeadline(parent Context, deadline time.Time) (Context, CancelFunc)
```

WithValue 提供了一种将请求范围内的值与 Context 相关联的方法：

```go
// WithValue returns a copy of parent whose Value method returns val for key.
func WithValue(parent Context, key interface{}, val interface{}) Context
```

注： 使用context的Value相关方法只应该用于在程序和接口中传递的和请求相关的元数据，不要用它来传递一些可选的参数；

掌握如何使用 context 包的最佳方法是通过一个真实完整的示例。

## Context 使用的简单示例
简单的示例，更容易理解 Context 各衍生函数适用的场景，而且编辑本文档使用的是 Org-mode, 在编辑的过程中，即可执行(对org-mode感兴趣的人，可在评论里联系我)。 这里的代码，来源于 context 的godoc。

### WithCancel
WithCancel 的示例, 演示如何使用可取消 context 来防止 goroutine 泄漏。 示例函数的结尾，由gen启动的goroutine将返回而不会发送泄漏。

```go
package main

import (
  "context"
  "fmt"
)

func main() {
  // gen generates integers in a separate goroutine and
  // sends them to the returned channel.
  // The callers of gen need to cancel the context once
  // they are done consuming generated integers not to leak
  // the internal goroutine started by gen.
  gen := func(ctx context.Context) <-chan int {
    dst := make(chan int)
    n := 1
    go func() {
      for {
	select {
	case <-ctx.Done():
	  return // returning not to leak the goroutine
	case dst <- n:
	  n++
	}
      }
    }()
    return dst
  }

  ctx, cancel := context.WithCancel(context.Background())
  defer cancel() // cancel when we are finished consuming integers

  for n := range gen(ctx) {
    fmt.Println(n)
    if n == 5 {
      break
    }
  }
}
```

### WithDeadline
WithDeadline 的示例,通过一个截止日期的 Context 来告知一个阻塞的函数，一旦它到了最终期限，就放弃它的工作

```go
package main

import (
  "context"
  "fmt"
  "time"
)

func main() {
  d := time.Now().Add(50 * time.Millisecond)
  ctx, cancel := context.WithDeadline(context.Background(), d)

  // Even though ctx will be expired, it is good practice to call its
  // cancelation function in any case. Failure to do so may keep the
  // context and its parent alive longer than necessary.
  defer cancel()

  select {
  case <-time.After(1 * time.Second):
    fmt.Println("overslept")
  case <-ctx.Done():
    fmt.Println(ctx.Err())
  }

}
```

### Withtimeount
WithTimeount 的示例, 传递具有超时的 Context 以告知阻塞函数，它将在超时过后丢弃其工作。

```go
package main

import (
  "context"
  "fmt"
  "time"
)

func main() {
  // Pass a context with a timeout to tell a blocking function that it
  // should abandon its work after the timeout elapses.
  ctx, cancel := context.WithTimeout(context.Background(), 50*time.Millisecond)
  defer cancel()

  select {
  case <-time.After(1 * time.Second):
    fmt.Println("overslept")
  case <-ctx.Done():
    fmt.Println(ctx.Err()) // prints "context deadline exceeded"
  }

}
```

### WithValue
WithValue 的简单示例代码：

```go
package main

import (
  "context"
  "fmt"
)

func main() {
  type favContextKey string

  f := func(ctx context.Context, k favContextKey) {
    if v := ctx.Value(k); v != nil {
      fmt.Println("found value:", v)
      return
    }
    fmt.Println("key not found:", k)
  }

  k := favContextKey("language")
  ctx := context.WithValue(context.Background(), k, "Go")

  f(ctx, k)
  f(ctx, favContextKey("color"))

}
```

## 示例：Google Web Search
示例是一个HTTP服务器，通过将查询“golang”转发到 Google Web Search API 并渲染查询结果, 来处理 "/search？q=golang＆timeout=1s" 之类的URL。 timeout参数告诉服务器在该时间过去之后取消请求。

示例代码被拆分为三个包：

server 提供了 main 函数和 "/search" 的处理函数。
userip 提供了从 request 提取用户ip地址和关联一个 Context 的函数。
google 提供了把搜索字段发送的 Google 的 Search 函数。

### server
服务器通过为 golang 提供前几个 Google 搜索结果来处理像 "search？q=golang" 之类的请求。 它注册 /handleSearch 来处理 "search"。 处理函数创建一个名为ctx的 /Context ，并在处理程序返回时,一并被取消。 如果 request 包含超时URL参数，则超时时会自动取消上下文：

```go
func handleSearch(w http.ResponseWriter, req *http.Request) {
  // ctx is the Context for this handler. Calling cancel closes the
  // ctx.Done channel, which is the cancellation signal for requests
  // started by this handler.
  var (
    ctx    context.Context
    cancel context.CancelFunc
  )
  timeout, err := time.ParseDuration(req.FormValue("timeout"))
  if err == nil {
    // The request has a timeout, so create a context that is
    // canceled automatically when the timeout expires.
    ctx, cancel = context.WithTimeout(context.Background(), timeout)
  } else {
    ctx, cancel = context.WithCancel(context.Background())
  }
  defer cancel() // Cancel ctx as soon as handleSearch returns.
}
```

处理程序从 request 中提取查询关键字，并通过调用 userip 包来提取客户端的IP地址。 后端请求需要客户端的IP地址，因此handleSearch将其附加到ctx：

```go
// Check the search query.
query := req.FormValue("q")
if query == "" {
  http.Error(w, "no query", http.StatusBadRequest)
  return
}

// Store the user IP in ctx for use by code in other packages.
userIP, err := userip.FromRequest(req)
if err != nil {
  http.Error(w, err.Error(), http.StatusBadRequest)
  return
}
ctx = userip.NewContext(ctx, userIP)
```

处理程序使用ctx和查询关键字调用 google.Search ：

```go
// Run the Google search and print the results.
start := time.Now()
results, err := google.Search(ctx, query)
elapsed := time.Since(start)
if err != nil {
  http.Error(w, err.Error(), http.StatusInternalServerError)
  return
}
```

如果搜索成功，处理程序将渲染返回结果：

```go
if err := resultsTemplate.Execute(w, struct {
  Results          google.Results
  Timeout, Elapsed time.Duration
}{
  Results: results,
  Timeout: timeout,
  Elapsed: elapsed,
}); err != nil {
  log.Print(err)
  return
}
```

### userip
userip包提供从请求中提取用户IP地址并将其与 Context 相关联的函数。 Context 提供了 key-value 映射的 map ，其中 key 和 value 均为 interface{} 类型。 key 类型必须支持相等性， value 必须是多个 goroutine 安全的。 userip 这样的包会隐藏 map 的细节，并提供强类型访问特定的 Context 值。

为了避免关键字冲突， userip 定义了一个不导出的类型 key ，并使用此类型的值作为 Context 的关键字：

```go
// The key type is unexported to prevent collisions with context keys defined in
// other packages.
type key int

// userIPkey is the context key for the user IP address.  Its value of zero is
// arbitrary.  If this package defined other context keys, they would have
// different integer values.
const userIPKey key = 0
```

FromRequest 从 http.Request 中提取一个 userIP 值：

```go
func FromRequest(req *http.Request) (net.IP, error) {
  ip, _, err := net.SplitHostPort(req.RemoteAddr)
  if err != nil {
    return nil, fmt.Errorf("userip: %q is not IP:port", req.RemoteAddr)
  }

  userIP := net.ParseIP(ip)
  if userIP == nil {
    return nil, fmt.Errorf("userip: %q is not IP:port", req.RemoteAddr)
  }
  return userIP, nil
}
```

NewContext返回一个带有userIP的新Context：

```go
func NewContext(ctx context.Context, userIP net.IP) context.Context {
    return context.WithValue(ctx, userIPKey, userIP)
}
```

FromContext 从 Context 中提取 userIP ：

```go
func FromContext(ctx context.Context) (net.IP, bool) {
    // ctx.Value returns nil if ctx has no value for the key;
    // the net.IP type assertion returns ok=false for nil.
    userIP, ok := ctx.Value(userIPKey).(net.IP)
    return userIP, ok
}
```

### google
google.Search 函数向 Google Web Search API 发出HTTP请求，并解析JSON编码结果。 它接受Context参数ctx，并且在ctx.Done关闭时立即返回。

Google Web Search API请求包括搜索查询和用户IP作为查询参数：

```go
func Search(ctx context.Context, query string) (Results, error) {
    // Prepare the Google Search API request.
    req, err := http.NewRequest("GET", "https://ajax.googleapis.com/ajax/services/search/web?v=1.0", nil)
    if err != nil {
	return nil, err
    }
    q := req.URL.Query()
    q.Set("q", query)

    // If ctx is carrying the user IP address, forward it to the server.
    // Google APIs use the user IP to distinguish server-initiated requests
    // from end-user requests.
    if userIP, ok := userip.FromContext(ctx); ok {
	q.Set("userip", userIP.String())
    }
    req.URL.RawQuery = q.Encode()

    // Issue the HTTP request and handle the response.

}
```

Search 使用一个辅助函数 httpDo 来发出HTTP请求, 如果在处理请求或响应时关闭 ctx.Done ，取消 httpDo 。 Search 将传递闭包给 httpDo 来处理HTTP响应：

```go
var results Results
err = httpDo(ctx, req, func(resp *http.Response, err error) error {
  if err != nil {
    return err
  }
  defer resp.Body.Close()

  // Parse the JSON search result.
  // https://developers.google.com/web-search/docs/#fonje
  var data struct {
    ResponseData struct {
      Results []struct {
	TitleNoFormatting string
	URL               string
      }
    }
  }
  if err := json.NewDecoder(resp.Body).Decode(&data); err != nil {
    return err
  }
  for _, res := range data.ResponseData.Results {
    results = append(results, Result{Title: res.TitleNoFormatting, URL: res.URL})
  }
  return nil
})
// httpDo waits for the closure we provided to return, so it's safe to
// read results here.
return results, err
```

httpDo 函数发起HTTP请求，并在新的 goroutine 中处理其响应。 如果在 goroutine 退出之前关闭了ctx.Done，它将取消该请求：

```go
func httpDo(ctx context.Context, req *http.Request, f func(*http.Response, error) error) error {
    // Run the HTTP request in a goroutine and pass the response to f.
    tr := &http.Transport{}
    client := &http.Client{Transport: tr}
    c := make(chan error, 1)
    go func() { c <- f(client.Do(req)) }()
    select {
    case <-ctx.Done():
	tr.CancelRequest(req)
	<-c // Wait for f to return.
	return ctx.Err()
    case err := <-c:
	return err
    }
}
```

## 适配Context到已有代码
许多服务器框架提供用于承载请求范围值的包和类型。 可以定义 Context 接口的新实现，以便使得现有的框架和期望Context参数的代码进行适配。

例如，Gorilla的 github.com/gorilla/context 包允许处理程序通过提供从HTTP请求到键值对的映射来将数据与传入的请求相关联。 在 gorilla.go 中，提供了一个 Context 实现，其 Value 方法返回与 Gorilla 包中的特定HTTP请求相关联的值。

其他软件包提供了类似于 Context 的取消支持。 例如，Tomb 提供了一种杀死方法，通过关闭死亡 channel 来发出取消信号。 Tomb还提供了等待 goroutine 退出的方法，类似于sync.WaitGroup。 在 tomb.go 中，提供一个 Context 实现，当其父 Context 被取消或提供的 Tomb 被杀死时，该 Context 被取消。

## 总结
在Google，我们要求Go程序员通过 Context 参数作为传入和传出请求之间的呼叫路径上每个函数的第一个参数。 这允许由许多不同团队开发的Go代码进行良好的互操作。 它提供对超时和取消的简单控制，并确保安全证书等关键值正确转移Go程序。

希望在 Context 上构建的服务器框架应该提供 Context 的实现，以便在它们的包之间和期望 Context 参数的包之间进行适配。 客户端库将接受来自调用代码的 Context 。 通过为请求范围的数据和取消建立通用接口， Context 使得开发人员更容易地共享用于创建可扩展服务的代码。
