# exec-pipeline

**C++26 std::execution-based pipelining framework for structured concurrency with GPU-CPU hybrid scheduling**

[![C++26](https://img.shields.io/badge/C%2B%2B26-active-green)](https://isocpp.org/std/status)
[![Stars](https://img.shields.io/github/stars/zkzWilliamzhou/exec-pipeline?style=social)](https://github.com/zkzWilliamzhou/exec-pipeline)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](./LICENSE)

## 🎯 Why This Exists

C++异步编程碎片化问题：
- std::async：无结构化并发
- Asio：非标准化
- Intel TBB：重量级

**exec-pipeline 提供 std::execution 标准化异步框架。**

## ✨ Key Features

- **Sender/Receiver模型**：C++26标准化异步
- **结构化并发**：自动资源清理
- **GPU-CPU混合调度**：统一接口
- **零运行时开销**：编译期确定执行图
- **错误传播**：标准化异常传递

## 💻 Quick Start

```cpp
#include <exec-pipeline.hpp>
#include <thread-pool.hpp>

int main() {
    auto scheduler = thread_pool_scheduler{4};
    
    // 定义流水线
    auto pipeline = 
        exec::source(1, 10)                    // 生成1-10
        | exec::map([](int x) { return x * x; }) // 平方
        | exec::filter([](int x) { return x % 2 == 0; })  // 偶数
        | exec::on(scheduler)                    // 使用线程池
        | exec::collect();                       // 收集结果
    
    // 执行
    auto results = pipeline.execute();
    // [4, 16, 36, 64, 100]
    
    return 0;
}
```

## 📊 流水线图

```
source(1,10)
    │
    ▼
map(square)
    │
    ▼
filter(even)
    │
    ▼
on(thread_pool)
    │
    ▼
collect()
    ▼
[4, 16, 36, 64, 100]
```

## 🔧 核心组件

### Sender/Receiver模式
```cpp
// Sender: 表示异步工作单元
auto sender = exec::schedule(scheduler);

// Receiver: 定义完成/错误处理
auto receiver = exec::make_receiver(
    [](int value) { std::cout << "Got: " << value << "\n"; },
    [](std::exception_ptr e) { std::cerr << "Error!\n"; }
);

// Connect
auto operation = exec::connect(sender, receiver);

// Start
exec::start(operation);
```

### 调度器 (Schedulers)
```cpp
// 线程池调度器
auto thread_pool = exec::thread_pool{4};
auto tpe = exec::thread_pool_scheduler{thread_pool};

// GPU调度器
auto gpu = exec::gpu_scheduler{"device:0"};

// 单线程调度器
auto single = exec::single_thread_scheduler{};
```

### 操作 Adaptors
```cpp
// map 适配器
auto doubled = exec::map([](int x) { return x * 2; });

// filter 适配器
auto evens = exec::filter([](int x) { return x % 2 == 0; });

// transform 适配器
auto sorted = exec::sort(std::less<>{});

// take 适配器
auto first_10 = exec::take(10);
```

## 🤝 使用示例

### 1. 文件I/O流水线
```cpp
auto file_pipeline = 
    exec::source(file_paths)                   // 文件路径列表
    | exec::map([](const std::string& path) {   
        return read_file(path);                  // 异步读取
    })
    | exec::filter([](const std::string& content) {
        return !content.empty();                 // 过滤空文件
    })
    | exec::transform([](const std::string& content) {
        return process_content(content);         // 处理内容
    })
    | exec::collect<std::vector>();              // 收集结果
```

### 2. GPU计算流水线
```cpp
auto gpu_pipeline =
    exec::source(data)                           // 输入数据
    | exec::map(to_tensor)                       // 转换为张量
    | exec::on(gpu_scheduler)                    // 转到GPU
    | exec::transform(nn_model)                  // 神经网络推理
    | exec::on(thread_pool_scheduler)            // 结果回CPU
    | exec::filter([](const auto& result) {
        return result.confidence > 0.9;         // 置信度过滤
    });
```

### 3. 数据库查询流水线
```cpp
auto db_pipeline =
    exec::source(user_ids)                       // 用户ID列表
    | exec::transform([](int id) {               // 批量查询
        return db.select<User>().where("id", "=", std::to_string(id)).execute();
    })
    | exec::flatten()                            // 展平结果集
    | exec::filter([](const User& u) { return u.active; })
    | exec::transform(&User::to_json);           // 序列化
```

## 🏗️ 编译期优化

### 编译期依赖图
```cpp
// 编译期确定执行顺序
constexpr auto graph = exec::dependency_graph{
    step1 >> step2,  // step1 在 step2 前
    step3 || step4   // step3 和 step4 并行
};
```

### 零运行时分支
```cpp
// 编译期分支消除
auto pipeline = 
    cond(parallel_enabled) 
        ? exec::on(scheduler)
        : exec::identity();
```

## 🔗 与C++26 std::execution集成

### 完整的Sender/Receiver实现
```cpp
// 实现自定义Sender
template<typename T>
struct value_sender {
    T value;
    
    template<typename U>
    struct opstate {
        U receiver;
        T value;
        
        void start() && {
            std::move(receiver).set_value(value);
        }
    };
    
    template<typename U>
    auto connect(U&& receiver) && {
        return opstate<std::decay_t<U>>{
            std::forward<U>(receiver),
            std::move(value)
        };
    }
};
```

## 📈 性能特性

| 特性 | 性能 |
|------|------|
| 零运行时调度开销 | ✅ 编译期调度器选择 |
| 零拷贝数据传递 | ✅ 移动语义 |
| 编译期并行化 | ✅ 操作符重载 |
| 内存分配消除 | ✅ 固定大小缓冲区 |

## 📄 许可证

MIT License