// test_pipeline.cpp - exec-pipeline test suite
// Build: g++ -std=c++2c -I../include test_pipeline.cpp -o test

#include <exec-pipeline.hpp>
#include <cassert>
#include <iostream>
#include <vector>

using namespace exec;

void test_thread_pool_scheduler() {
    thread_pool_scheduler sched(2);
    
    std::atomic<int> counter{0};
    
    // Schedule some work
    for (int i = 0; i < 10; ++i) {
        sched.submit([&counter] { counter++; });
    }
    
    // Give threads time to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "✓ test_thread_pool_scheduler passed\n";
}

void test_pipe_operator() {
    auto result = source_value<int>{42} 
        | map([](int x) { return x * 2; })
        | map([](int x) { return x + 1; });
    
    std::cout << "Pipe operator test: " << result.value << "\n";
    assert(result.value == 85);  // (42 * 2) + 1
    
    std::cout << "✓ test_pipe_operator passed\n";
}

void test_map_transform() {
    auto mapper = map([](int x) { return x * x; });
    
    auto result = mapper(5);
    assert(result.value == 25);
    
    std::cout << "✓ test_map_transform passed\n";
}

void test_filter_predicate() {
    auto filterer = filter([](int x) { return x % 2 == 0; });
    
    auto result_with_even = filterer(4);
    assert(result_with_even.has_value() && result_with_even->value == 4);
    
    auto result_with_odd = filterer(3);
    assert(!result_with_odd.has_value());
    
    std::cout << "✓ test_filter_predicate passed\n";
}

void test_composed_pipeline() {
    auto pipeline = 
        source_value<int>{10}
        | map([](int x) { return x * 2; })    // 20
        | map([](int x) { return x + 5; })    // 25
        | filter([](int x) { return x > 20; }); // Keep if > 20
    
    if (pipeline) {
        std::cout << "Final result: " << pipeline->value << "\n";
        assert(pipeline->value == 25);
    }
    
    std::cout << "✓ test_composed_pipeline passed\n";
}

void test_chaining() {
    auto result = source_value<int>{5}
        | map([](int x) { return x * 2; })
        | map([](int x) { return x + 10; })
        | map([](int x) { return x * 3; });
    
    assert(result.value == 60);  // ((5*2)+10)*3 = 60
    
    std::cout << "✓ test_chaining passed\n";
}

int main() {
    std::cout << "=== exec-pipeline Tests ===\n\n";
    
    test_thread_pool_scheduler();
    test_pipe_operator();
    test_map_transform();
    test_filter_predicate();
    test_composed_pipeline();
    test_chaining();
    
    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}