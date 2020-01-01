## Lite thread-safe dynamically polymorphic pool-based function wrappers.

* Thread-safety is based on copying instances of functions to the thread local pool.
* It is done so, because any real attempts to keep thread-safety with shared data lead to big overhead even in case of the only thread and easy copyable functions (that is the most popular case).
* If the function is copied within one thread, only counter of uses is incremented.
* If you want to have a big function (with a `std::vector` captured, for example), you can wrap it with `std::shared_ptr` or something like that.
---
***The more universal your solution is, the less you can use it (due to performance issues).***