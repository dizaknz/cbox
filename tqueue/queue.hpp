#ifndef TQUEUE
#define TQUEUE

#include <queue>
#include <mutex>
#include <condition_variable>

// A threadsafe-queue.
template <class T>
struct TQueue
{
    public:
        TQueue(void) : q(), m(), c()
        {}

        ~TQueue(void)
        {}

        void push(T t)
        {
            std::lock_guard<std::mutex> lock(m);
            q.push(t);
            c.notify_one();
        }

        T pop(void)
        {
            std::unique_lock<std::mutex> lock(m);
            // blocks until element is available
            while(q.empty())
            {
                c.wait(lock);
            }
            T val = q.front();
            q.pop();

            return val;
        }

    private:
        std::queue<T> q;
        mutable std::mutex m;
        std::condition_variable c;
};
#endif
