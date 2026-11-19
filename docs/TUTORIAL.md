# exec-pipeline 学习教程

## 入门：Sender/Receiver概念

### 什么是Sender/Receiver？
```
Sender 发送值 → Receiver 接收值
                ↳ set_value(T)
                ↳ set_error(exception_ptr)
                ↳ set_cancel()
```

### 简单示例
```cpp
#include <exec-pipeline.hpp>
#include <iostream>

int main() {
    auto sched = thread_pool_scheduler{4};
    
    auto sender = sched.schedule();
    auto receiver = []() {
        std::cout << "Async task completed!\n";
    };
    
    auto op = exec::connect(sender, std::move(receiver));
    exec::start(std::move(op));
    
    return 0;
}
```

## 第二部分：管道操作符

### 管道语法
```cpp
// 输入值 | 操作1 | 操作2 | 输出值
auto result = source_value<int>{42}
    | map([](int x) { return x * 2; })      // 84
    | map([](int x) { return x + 1; })      // 85
    | filter([](int x) { return x > 50; }); // 85 (通过过滤)
```

### 常见操作

#### map - 转换每个元素
```cpp
pipeline | map([](T x) { return transform(x); })
```

#### filter - 过滤元素
```cpp
pipeline | filter([](T x) { return predicate(x); })
```

#### transform - 批量转换
```cpp
pipeline | transform([](std::vector<T> v) { return processed(v); })
```

## 第三部分：调度器

### 线程池调度器
```cpp
auto pool = exec::thread_pool{4};
auto sched = exec::thread_pool_scheduler{pool};

pipeline | exec::on(sched) | exec::collect();
```

### 单线程调度器
```cpp
auto single = exec::single_thread_scheduler{};
pipeline | exec::on(single) | ...;
```

### GPU调度器
```cpp
auto gpu = exec::gpu_scheduler{"device:0"};
pipeline | exec::on(gpu) | ...;
```

## 性能优化技巧

### 1. 编译期优化
```cpp
// 编译期确定分支
constexpr bool parallel = true;
auto pipeline = 
    cond(parallel) ? exec::on(scheduler) : exec::identity();
```

### 2. 零拷贝
```cpp
// 使用移动语义避免拷贝
auto optimized = 
    exec::source(std::move(large_data))
    | exec::map([](auto data) { return process(std::move(data)); });
```

## 调试技巧

### 日志输出
```cpp
#ifdef DEBUG_PIPELINE
    std::cout << "Stage: " << #STAGE << "\n";
#endif
```