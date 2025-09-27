#include <cstddef>
#include <type_traits>
#include <new>
#include <memory>
#include <utility>
#include <map>
#include <iostream>

// ===== 1) Минимальный фикс-пул аллокатор =====
template<class T, std::size_t N>
struct FixedAlloc {
    using value_type = T;
    template<class U> struct rebind { using other = FixedAlloc<U, N>; };

    FixedAlloc() noexcept {}
    template<class U> FixedAlloc(const FixedAlloc<U,N>&) noexcept {}

    T* allocate(std::size_t n) {
        if (_used + n > N) throw std::bad_alloc();
        auto p = reinterpret_cast<T*>(&_buf[_used]);
        _used += n;
        return p;
    }
    void deallocate(T*, std::size_t) noexcept {}

    template<class U> bool operator==(const FixedAlloc<U,N>&) const noexcept { return true; }
    template<class U> bool operator!=(const FixedAlloc<U,N>&) const noexcept { return false; }

private:
    using slot = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
    static slot _buf[N];
    static std::size_t _used;
};
template<class T, std::size_t N> typename FixedAlloc<T,N>::slot FixedAlloc<T,N>::_buf[N];
template<class T, std::size_t N> std::size_t FixedAlloc<T,N>::_used = 0;

// ===== 2) Свой контейнер: однонаправленный список =====
template<class T, class Alloc = std::allocator<T>>
class SimpleList {
    struct Node {
        T v; Node* next;
        Node(const T& x): v(x), next(0) {}
        Node(T&& x): v(std::move(x)), next(0) {}
    };
    using NodeAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<Node>;
    using AT = std::allocator_traits<NodeAlloc>;

public:
    SimpleList() = default;
    explicit SimpleList(const Alloc& a): alloc_(a) {}
    ~SimpleList(){ clear(); }

    void push_back(const T& x){ emplace_back(x); }
    void push_back(T&& x){ emplace_back(std::move(x)); }

    template<class... Args>
    void emplace_back(Args&&... args){
        NodeAlloc a(alloc_);                     // lvalue аллокатор
        Node* n = AT::allocate(a, 1);
        AT::construct(a, n, T(std::forward<Args>(args)...));
        if(!head_) head_ = tail_ = n;
        else { tail_->next = n; tail_ = n; }
    }

    template<class F>
    void for_each(F f) const { for(Node* p=head_; p; p=p->next) f(p->v); }

    void clear(){
        NodeAlloc a(alloc_);
        Node* p=head_;
        while(p){
            Node* q=p->next;
            AT::destroy(a, p);
            AT::deallocate(a, p, 1);
            p=q;
        }
        head_=tail_=0;
    }

private:
    Node* head_ = 0;
    Node* tail_ = 0;
    Alloc alloc_{};
};

// ===== 3) Демонстрация по условию =====
static int fact(int n){ int r=1; for(int i=2;i<=n;++i) r*=i; return r; }

int main() {
    // std::map с дефолтным аллокатором
    {
        std::map<int,int> m;
        for(int i=0;i<10;++i) m.emplace(i, fact(i));
        for(const auto& kv: m) std::cout << kv.first << ' ' << kv.second << '\n';
    }
    std::cout << "---\n";

    // std::map с нашим аллокатором (10 юзер-элементов + запас под служебный узел)
    {
        using Pair = std::pair<const int,int>;
        using AMap = FixedAlloc<Pair, 11>;
        std::map<int,int,std::less<int>,AMap> m;
        for(int i=0;i<10;++i) m.emplace(i, fact(i));
        for(const auto& kv: m) std::cout << kv.first << ' ' << kv.second << '\n';
    }
    std::cout << "---\n";

    // свой контейнер без кастомного аллокатора
    {
        SimpleList<int> lst;
        for(int i=0;i<10;++i) lst.push_back(i);
        lst.for_each([](int x){ std::cout << x << '\n'; });
    }
    std::cout << "---\n";

    // свой контейнер с нашим аллокатором, ограниченным 10 элементами
    {
        using AList = FixedAlloc<int,10>;
        SimpleList<int, AList> lst(AList{});
        for(int i=0;i<10;++i) lst.push_back(i);
        lst.for_each([](int x){ std::cout << x << '\n'; });
    }
}
