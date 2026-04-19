#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <cassert>

template<typename T>
class MPSCQueue {
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};
        Node() = default;
        Node(const T& v) : data(v) {}
    };
    
    std::atomic<Node*> head;
    Node* tail;
    
public:
    MPSCQueue() {
        Node* sentinel = new Node();
        head.store(sentinel, std::memory_order_relaxed);
        tail = sentinel;
    }
    
    ~MPSCQueue() {
        T val;
        while (pop(val)) {}
        delete tail;
    }
    
    void push(const T& value) {
        Node* new_node = new Node(value);
        Node* old_head = head.exchange(new_node, std::memory_order_acq_rel);
        old_head->next.store(new_node, std::memory_order_release);
    }
    
    bool pop(T& result) {
        Node* next = tail->next.load(std::memory_order_acquire);
        if (next == nullptr) return false;
        
        result = std::move(next->data);
        Node* old_tail = tail;
        tail = next;
        delete old_tail;
        return true;
    }
    
    bool empty() const {
        return tail->next.load(std::memory_order_acquire) == nullptr;
    }
};

int main() {
    // Test 1: Single thread basic push/pop
    {
        MPSCQueue<int> q;
        assert(q.empty());
        
        q.push(1);
        q.push(2);
        q.push(3);
        
        int val;
        assert(q.pop(val) && val == 1);
        assert(q.pop(val) && val == 2);
        assert(q.pop(val) && val == 3);
        assert(!q.pop(val));
        assert(q.empty());
        
        std::cout << "Test 1 (basic FIFO): PASSED\n";
    }
    
    // Test 2: Push after drain
    {
        MPSCQueue<int> q;
        q.push(10);
        
        int val;
        assert(q.pop(val) && val == 10);
        assert(q.empty());
        
        q.push(20);
        q.push(30);
        assert(q.pop(val) && val == 20);
        assert(q.pop(val) && val == 30);
        assert(q.empty());
        
        std::cout << "Test 2 (push after drain): PASSED\n";
    }
    
    // Test 3: Multiple producers, single thread drain
    {
        MPSCQueue<int> q;
        const int N = 10000;
        
        std::thread t1([&]() {
            for (int i = 0; i < N; i++) q.push(i);
        });
        std::thread t2([&]() {
            for (int i = 0; i < N; i++) q.push(i + N);
        });
        
        t1.join();
        t2.join();
        
        int count = 0;
        int val;
        while (q.pop(val)) count++;
        assert(count == 2 * N);
        
        std::cout << "Test 3 (multi-producer drain): PASSED\n";
    }
    
    // Test 4: Concurrent producer and consumer
    {
        MPSCQueue<int> q;
        const int N = 100000;
        std::atomic<bool> done{false};
        
        std::thread producer([&]() {
            for (int i = 0; i < N; i++) q.push(i);
            done.store(true, std::memory_order_release);
        });
        
        int count = 0;
        int val;
        while (true) {
            if (q.pop(val)) {
                count++;
            } else if (done.load(std::memory_order_acquire)) {
                while (q.pop(val)) count++;
                break;
            }
        }
        
        producer.join();
        assert(count == N);
        
        std::cout << "Test 4 (concurrent MPSC): PASSED\n";
    }
    
    // Test 5: 4 producers + 1 consumer
    {
        MPSCQueue<int> q;
        const int PER_THREAD = 50000;
        const int NUM_THREADS = 4;
        std::atomic<bool> all_done{false};
        
        std::vector<std::thread> producers;
        for (int t = 0; t < NUM_THREADS; t++) {
            producers.emplace_back([&, t]() {
                for (int i = 0; i < PER_THREAD; i++) {
                    q.push(t * PER_THREAD + i);
                }
            });
        }
        
        int count = 0;
        int val;
        std::thread consumer([&]() {
            int expected_total = NUM_THREADS * PER_THREAD;
            while (count < expected_total) {
                if (q.pop(val)) {
                    count++;
                } else if (all_done.load(std::memory_order_acquire)) {
                    while (q.pop(val)) count++;
                    break;
                }
            }
        });
        
        for (auto& p : producers) p.join();
        all_done.store(true, std::memory_order_release);
        consumer.join();
        
        assert(count == NUM_THREADS * PER_THREAD);
        
        std::cout << "Test 5 (4 producers + 1 consumer): PASSED\n";
    }
    
    std::cout << "All tests passed!\n";
    return 0;
}