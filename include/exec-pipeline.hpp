// exec-pipeline.hpp - C++26 std::execution-based pipelining framework
// Sender/Receiver model for structured concurrency with GPU-CPU hybrid scheduling

#ifndef EXEC_PIPELINE_HPP
#define EXEC_PIPELINE_HPP

#include <functional>
#include <memory>
#include <type_traits>
#include <concepts>
#include <optional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string_view>
#include <tuple>
#include <cstddef>
#include <exception>
#include <future>
#include <thread>
#include <vector>

#ifdef EXEC_PIPELINE_HAS_EXECUTION_HEADER
#include <execution>
#endif

namespace exec {

// ============================================================================
// Scheduler Concepts (P2300 executor model)
// ============================================================================

template<typename S>
concept scheduler = requires(S s) {
    typename S::sender_type;
    { s.schedule() } -> std::same_as<typename S::sender_type>;
};

template<typename Op>
concept operation_state = requires(Op op) {
    { op.start() };
};

template<typename R, typename... Args>
concept receiver = requires(R r, Args...) {
    { std::move(r).set_value(std::declval<Args>()...) };
    { std::move(r).set_error(std::declval<std::exception_ptr>()) };
    { std::move(r).set_cancel() };
};

template<typename T>
concept value_type = requires {
    typename T::value_type;
};

// ============================================================================
// Value Wrapper for Pipeline
// ============================================================================

template<typename T>
struct source_value {
    T value;

    // Pipe with single operation
    template<typename Op>
    [[nodiscard]] auto operator|(Op&& op) const {
        return std::forward<Op>(op)(value);
    }
};

// ============================================================================
// Map Transform - Transform each element
// ============================================================================

template<typename F>
struct map_transform {
    F func;

    // Handle source_value - unwrap and transform
    template<typename T>
    [[nodiscard]] auto operator()(source_value<T> sv) const {
        return source_value<std::invoke_result_t<F, T>>{
            std::invoke(func, std::move(sv.value))
        };
    }

    // Handle raw value directly
    template<typename T>
    [[nodiscard]] auto operator()(T&& value) const {
        return source_value<std::invoke_result_t<F, std::decay_t<T>>>{
            std::invoke(func, std::forward<T>(value))
        };
    }
};

// ============================================================================
// Filter Predicate - Keep or discard elements
// ============================================================================

template<typename P>
struct filter_predicate {
    P pred;

    // Handle source_value - filter based on value.
    // Both branches must agree on the return type so the result always composes
    // with operator| (which expects std::optional<source_value<...>>).
    template<typename T>
    [[nodiscard]] auto operator()(source_value<T> sv) const -> std::optional<source_value<T>> {
        if (std::invoke(pred, sv.value)) {
            return std::optional<source_value<T>>{source_value<T>{std::move(sv.value)}};
        }
        return std::nullopt;
    }

    // Handle raw value directly
    template<typename T>
    [[nodiscard]] auto operator()(T&& value) const
        -> std::optional<source_value<std::decay_t<T>>> {
        if (std::invoke(pred, value)) {
            return std::optional<source_value<std::decay_t<T>>>{
                source_value<std::decay_t<T>>{std::forward<T>(value)}};
        }
        return std::nullopt;
    }
};

// ============================================================================
// Transform - Apply function and pass result
// ============================================================================

template<typename F>
struct transform_operation {
    F func;

    template<typename T>
    [[nodiscard]] auto operator()(source_value<T> sv) const {
        return source_value<std::invoke_result_t<F, T>>{
            std::invoke(func, std::move(sv.value))
        };
    }

    template<typename T>
    [[nodiscard]] auto operator()(T&& value) const {
        return source_value<std::invoke_result_t<F, std::decay_t<T>>>{
            std::invoke(func, std::forward<T>(value))
        };
    }
};

// ============================================================================
// Pipe Operator - handles optional return from filter
// ============================================================================

// std::optional<source_value<T>> | Op
template<typename T, typename Op>
[[nodiscard]] auto operator|(std::optional<source_value<T>> opt, Op&& op) {
    if (!opt) {
        return std::optional<source_value<std::decay_t<decltype(std::forward<Op>(op)(opt->value))>>>{std::nullopt};
    }
    auto result = std::forward<Op>(op)(std::move(opt->value));
    // If result is another optional, propagate; otherwise wrap
    if constexpr (std::is_same_v<std::decay_t<decltype(result)>, std::optional<source_value<T>>>) {
        return result;
    } else {
        return std::optional<source_value<std::decay_t<decltype(result.value)>>>{std::move(result)};
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

[[nodiscard]] inline auto map(auto func) {
    return map_transform{std::move(func)};
}

[[nodiscard]] inline auto filter(auto pred) {
    return filter_predicate{std::move(pred)};
}

[[nodiscard]] inline auto transform(auto func) {
    return transform_operation{std::move(func)};
}

// ============================================================================
// collect() utility function
// ============================================================================

inline auto collect() {
    return [](auto container) -> decltype(auto) {
        return container;
    };
}

// ============================================================================
// on() - Schedule on specific executor
// ============================================================================

template<typename Sched>
struct on_stage {
    Sched sched;

    template<typename T>
    [[nodiscard]] auto operator()(T&& value) const {
        return source_value<std::decay_t<T>>{std::forward<T>(value)};
    }
};

template<typename Sched>
[[nodiscard]] auto on(Sched s) {
    return on_stage<Sched>{std::move(s)};
}

// ============================================================================
// Connect and Start utilities
// ============================================================================

template<typename S, typename R>
auto connect(S&& s, R&& r) {
    return std::forward<S>(s).connect(std::forward<R>(r));
}

template<typename Op>
void start(Op&& op) {
    op.start();
}

// ============================================================================
// Source Generator Functions
// ============================================================================

template<typename... Ts>
struct source_generator {
    std::tuple<Ts...> values;

    template<typename F>
    void for_each(F f) {
        std::apply([&f](auto&&... args) {
            (f(std::move(args)), ...);
        }, std::move(values));
    }
};

template<typename C>
[[nodiscard]] auto range(C&& container) {
    return [c = std::forward<C>(container)](auto auto_func) mutable {
        using T = std::decay_t<decltype(*std::begin(c))>;
        for (auto&& item : c) {
            auto_func(std::move(item));
        }
    };
}

// ============================================================================
// Execution Policies Mapping
// ============================================================================

#ifdef EXEC_PIPELINE_HAS_EXECUTION_HEADER
inline constexpr auto parallel_policy = std::execution::par;
inline constexpr auto seq_policy = std::execution::seq;
inline constexpr auto unseq_policy = std::execution::unseq;
#else
// Fallback policies
#endif

template<typename T>
[[nodiscard]] auto parallel(T&& container) {
    return std::forward<T>(container);
}

template<typename T>
[[nodiscard]] auto sequential(T&& container) {
    return std::forward<T>(container);
}

// ============================================================================
// Convenience Aliases
// ============================================================================

template<typename T>
using pipeline_result = std::optional<T>;

// ============================================================================
// Internal thread pool implementation
// ============================================================================

namespace detail {

class thread_pool_impl {
public:
    explicit thread_pool_impl(size_t threads);
    ~thread_pool_impl();
    void submit(std::function<void()> f);

private:
    void worker_loop();
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
};

} // namespace detail

// ============================================================================
// ThreadPool Scheduler Implementation (must be defined before sender_type)
// ============================================================================

class thread_pool_scheduler {
public:
    class sender_type;
    friend class sender_type;

    explicit thread_pool_scheduler(size_t threads = std::thread::hardware_concurrency());
    ~thread_pool_scheduler();

    thread_pool_scheduler(thread_pool_scheduler&&) = delete;
    thread_pool_scheduler& operator=(thread_pool_scheduler&&) = delete;
    thread_pool_scheduler(const thread_pool_scheduler&) = delete;
    thread_pool_scheduler& operator=(const thread_pool_scheduler&) = delete;

    auto schedule() const -> sender_type;
    void submit(std::function<void()> f);

private:
    detail::thread_pool_impl pool_;
};

// ============================================================================
// GPU Scheduler Implementation
// ============================================================================

class gpu_scheduler {
public:
    class sender_type;
    friend class sender_type;

    enum class device_type {
        cuda,
        opencl,
        vulkan,
        cpu_fallback
    };

    explicit gpu_scheduler(std::string_view name = "default", device_type type = device_type::cpu_fallback);

    auto schedule() const -> sender_type;
    void submit(std::function<void()> f);

    // GPU computation submission
    template<typename F>
    auto submit_compute(F&& f) -> decltype(std::forward<F>(f)()) {
        return std::forward<F>(f)();
    }

    // Transfer data from GPU to CPU
    template<typename T>
    auto download(const T& gpu_data) -> T {
        return gpu_data;
    }

    // Transfer data from CPU to GPU
    template<typename T>
    auto upload(const T& cpu_data) -> T {
        return cpu_data;
    }

    std::string_view name() const { return device_name_; }
    device_type type() const { return device_type_; }

private:
    std::string device_name_;
    device_type device_type_;
};

// ============================================================================
// Single-thread Scheduler
// ============================================================================

class single_thread_scheduler {
public:
    class sender_type;
    friend class sender_type;

    auto schedule() const -> sender_type;
    void submit(std::function<void()> f) { f(); }
};

// ============================================================================
// Inline Scheduler
// ============================================================================

class inline_scheduler {
public:
    class sender_type;
    friend class sender_type;

    auto schedule() const -> sender_type;
    void submit(std::function<void()> f) { f(); }
};

// ============================================================================
// GPU-CPU Hybrid Pipeline Support
// ============================================================================

namespace gpu {

// GPU memory marker for data transfer optimization
template<typename T>
struct gpu_memory {
    T data;
    bool on_gpu = false;

    gpu_memory() = default;
    explicit gpu_memory(T d, bool gpu = false) : data(std::move(d)), on_gpu(gpu) {}

    T download() && {
        on_gpu = false;
        return std::move(data);
    }

    static gpu_memory<T> upload(T d) {
        return gpu_memory<T>{std::move(d), true};
    }
};

// Transfer operation
template<typename Sched>
struct transfer_to_cpu_fn {
    const Sched& sched;

    template<typename T>
    [[nodiscard]] auto operator()(gpu_memory<T> input) const {
        if (!input.on_gpu) {
            return source_value<T>{std::move(input.data)};
        }
        return sched.submit_compute([&input]() {
            return std::move(input).download();
        });
    }
};

template<typename Sched>
[[nodiscard]] auto transfer_to_cpu(const Sched& sched) {
    return transfer_to_cpu_fn<Sched>{sched};
}

} // namespace gpu

// ============================================================================
// Sender Types Implementation - Must be defined after all scheduler classes
// ============================================================================

class thread_pool_scheduler::sender_type {
public:
    explicit sender_type(const thread_pool_scheduler& sched);

    template<typename R>
    auto connect(R&& receiver) const {
        struct op_state {
            const thread_pool_scheduler* scheduler;
            std::decay_t<R> rec;

            void start() && {
                auto sched = scheduler;
                auto recCopy = [rec = std::move(rec)]() mutable {
                    try {
                        rec.set_value();
                    } catch (...) {
                        rec.set_error(std::current_exception());
                    }
                };
                sched->submit(std::move(recCopy));
            }
        };
        return op_state{scheduler_, std::forward<R>(receiver)};
    }

private:
    const thread_pool_scheduler* scheduler_;
};

class gpu_scheduler::sender_type {
public:
    explicit sender_type(const gpu_scheduler& sched);

    template<typename R>
    auto connect(R&& receiver) const {
        struct op_state {
            const gpu_scheduler* scheduler;
            std::decay_t<R> rec;

            void start() && {
                auto sched = scheduler;
                auto recCopy = [rec = std::move(rec)]() mutable {
                    try {
                        rec.set_value();
                    } catch (...) {
                        rec.set_error(std::current_exception());
                    }
                };
                sched->submit(std::move(recCopy));
            }
        };
        return op_state{scheduler_, std::forward<R>(receiver)};
    }

private:
    const gpu_scheduler* scheduler_;
};

class single_thread_scheduler::sender_type {
public:
    template<typename R>
    auto connect(R&& receiver) const {
        struct op_state {
            std::decay_t<R> rec;

            void start() && {
                try {
                    std::move(rec).set_value();
                } catch (...) {
                    std::move(rec).set_error(std::current_exception());
                }
            }
        };
        return op_state{std::forward<R>(receiver)};
    }
};

class inline_scheduler::sender_type {
public:
    template<typename R>
    auto connect(R&& receiver) const {
        struct op_state {
            std::decay_t<R> rec;

            void start() && {
                try {
                    std::move(rec).set_value();
                } catch (...) {
                    std::move(rec).set_error(std::current_exception());
                }
            }
        };
        return op_state{std::forward<R>(receiver)};
    }
};

// ============================================================================
// Implementation of Scheduler Functions and Constructors
// ============================================================================

// thread_pool_scheduler
inline thread_pool_scheduler::thread_pool_scheduler(size_t threads)
    : pool_(threads) {}

inline thread_pool_scheduler::~thread_pool_scheduler() = default;

inline auto thread_pool_scheduler::schedule() const -> sender_type {
    return sender_type{*this};
}

inline void thread_pool_scheduler::submit(std::function<void()> f) {
    pool_.submit(std::move(f));
}

inline thread_pool_scheduler::sender_type::sender_type(const thread_pool_scheduler& sched)
    : scheduler_(&sched) {}

// gpu_scheduler
inline gpu_scheduler::gpu_scheduler(std::string_view name, device_type type)
    : device_name_(name), device_type_(type) {}

inline auto gpu_scheduler::schedule() const -> sender_type {
    return sender_type{*this};
}

inline void gpu_scheduler::submit(std::function<void()> f) {
    f();
}

inline gpu_scheduler::sender_type::sender_type(const gpu_scheduler& sched)
    : scheduler_(&sched) {}

// single_thread_scheduler
inline auto single_thread_scheduler::schedule() const -> sender_type {
    return sender_type{};
}

// inline_scheduler
inline auto inline_scheduler::schedule() const -> sender_type {
    return sender_type{};
}

// ============================================================================
// Implementation of thread_pool_impl
// ============================================================================

inline detail::thread_pool_impl::thread_pool_impl(size_t threads) : running_(true) {
    for (size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] { this->worker_loop(); });
    }
}

inline detail::thread_pool_impl::~thread_pool_impl() {
    running_ = false;
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

inline void detail::thread_pool_impl::submit(std::function<void()> f) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.emplace(std::move(f));
    }
    cv_.notify_one();
}

inline void detail::thread_pool_impl::worker_loop() {
    while (running_) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !running_ || !tasks_.empty(); });
            if (!running_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

// ============================================================================
// Top-level using declarations for convenience
// ============================================================================

} // namespace exec

using exec::thread_pool_scheduler;
using exec::gpu_scheduler;
using exec::single_thread_scheduler;
using exec::inline_scheduler;
using exec::source_value;
using exec::map;
using exec::filter;
using exec::transform;
using exec::on;
using exec::collect;

#endif // EXEC_PIPELINE_HPP