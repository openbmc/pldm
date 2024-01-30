#include <coroutine>
#include <iostream>

template <typename AsyncFunc>
struct PldmFunction
{
    struct awaiter
    {
        const AsyncFunc& handler;
        awaiter(const AsyncFunc& h) : handler(h) {}

        bool await_ready()
        {
            return false; // Returns true if the coroutine should not be
                          // suspended.
        }
        void await_suspend(std::coroutine_handle<> h)
        {
            handler(h);
        }
        void await_resume() {
        } // Returns the result of the 'co_await' expression.
    };

    awaiter operator co_await() const noexcept
    {
        return awaiter(asyncFunc);
    }
    AsyncFunc& getHandler()
    {
        return asyncFunc;
    }

    AsyncFunc& asyncFunc;
    PldmFunction(AsyncFunc& h) : asyncFunc(h) {}
};

struct CoroTask
{
    struct promise_type
    {
        CoroTask get_return_object()
        {
            return CoroTask{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend()
        {
            return {};
        }
        std::suspend_never final_suspend() noexcept
        {
            return {};
        }
        void return_void() {}
        void unhandled_exception() {}
    };
    std::coroutine_handle<promise_type> handle;
    explicit CoroTask(std::coroutine_handle<promise_type> handle) :
        handle(handle)
    {}
};
