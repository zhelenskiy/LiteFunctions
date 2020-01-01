#include <atomic>
#include <iostream>
#include <thread>

#include <boost/pool/object_pool.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/tss.hpp>

template<class T>
struct Counter {
  T data;
  size_t uses = 1;
  explicit Counter(T data) : data(std::move(data)) {}
};

template<class F>
using FunctionalPool = boost::object_pool<Counter<F>>;

template<class F>
boost::thread_specific_ptr<FunctionalPool<F>> pool{};

template<class F>
static FunctionalPool<F> *getPool() {
  if (!pool<F>.get()) {
    pool<F>.reset(new FunctionalPool<F>());
  }
  return pool<F>.get();
}

template<class F>
class FunctionHolder {
  std::thread::id owner = std::this_thread::get_id();
  Counter<F> *counter;

 public:
  explicit FunctionHolder(Counter<F> *counter) : counter(counter) {}

  FunctionHolder(const FunctionHolder<F> &other)
      : counter(owner == other.owner ? (++other.counter->uses, other.counter)
                                     : (getPool<F>()->construct(other.counter->data))) {}

  template<class T>
  explicit FunctionHolder(T functor) : counter(getPool<F>()->construct(std::move(functor))) {}

  template<class... Args>
  auto operator()(Args &&... args) { return counter->data(std::forward<Args>(args)...); }

  ~FunctionHolder() {
    if (!--counter->uses) {
      assert(getPool<F>()->is_from(counter));
      getPool<F>()->destroy(counter);
    }
  }
};

template<class T>
FunctionHolder(T &&holder) -> FunctionHolder<std::remove_cv_t<T>>;

template<class Res, class... Args>
struct VTable {
  void (*copy)(const void *from, void *to);
  void (*destruct)(void *f);
  Res (*invoke)(void *f, Args...);
};

template<class F, class Res, class... Args>
constexpr thread_local VTable<Res, Args...> vTable = {
    [](const void *from, void *to) { new(to) F(*static_cast<F const *>(from)); },
    [](void *f) { static_cast<F *>(f)->~F(); },
    [](void *f, Args... args) -> Res { return (*static_cast<F *>(f))(std::move(args)...); }
};

template<class T>
struct SmartFunction;

using FunctionHolderExample = FunctionHolder<int>; // All they have the same size and alignment.

template<class Res, class... Args>
struct SmartFunction<Res(Args...)> {
  std::aligned_storage_t<sizeof(FunctionHolderExample), alignof(FunctionHolderExample)> data{};
  const VTable<Res, Args...> *curVTable;

  template<class F>
  explicit SmartFunction(const FunctionHolder<F> &holder) : curVTable(&vTable<FunctionHolder<F>, Res, Args...>) {
    new(&data) FunctionHolder<F>(holder);
  }

  template<class F>
  explicit SmartFunction(F functor) : SmartFunction<Res(Args...)>(FunctionHolder(std::move(functor))) {}

  SmartFunction(const SmartFunction &other) : curVTable(other.curVTable) {
    curVTable->copy(&other.data, &data);
  }

  ~SmartFunction() { curVTable->destruct(&data); }

  Res operator()(Args &&... args) { return curVTable->invoke(&data, std::forward<Args>(args)...); };
};

int main() {
  SmartFunction<int(int)> f([](int n) { return n * 2; });
  std::cout << f(3) << std::endl << SmartFunction<int(int)>(f)(2) << std::endl;
  std/*or boost*/::thread([&f] { std::cout << SmartFunction<int(int)>(f)(5) << std::endl; }).join();
}