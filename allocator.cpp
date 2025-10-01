#include <cstddef>
#include <type_traits>
#include <new>
#include <memory>
#include <utility>
#include <map>
#include <iostream>

template <class T, std::size_t N>
struct FixedAlloc
{
    using value_type = T;
    template <class U>
    struct rebind
    {
        using other = FixedAlloc<U, N>;
    };

    FixedAlloc() noexcept : pool_(std::make_shared<Pool>()) {}

    template <class U>
    FixedAlloc(const FixedAlloc<U, N> &) noexcept
        : pool_(std::make_shared<Pool>()) {}

    [[nodiscard]] T *allocate(std::size_t n)
    {
        if (n == 0)
            return nullptr;
        auto &P = *pool_;
        if (!P.mem)
        {
            P.mem = ::operator new(N * sizeof(T), std::align_val_t(alignof(T)));
            P.used = 0;
        }
        if (P.used + n > N)
            throw std::bad_alloc();
        T *p = static_cast<T *>(P.mem) + P.used;
        P.used += n;
        return p;
    }

    void deallocate(T *, std::size_t) noexcept
    {
    }

    bool operator==(const FixedAlloc &rhs) const noexcept { return pool_.get() == rhs.pool_.get(); }
    bool operator!=(const FixedAlloc &rhs) const noexcept { return !(*this == rhs); }

private:
    struct Pool
    {
        void *mem = nullptr;
        std::size_t used = 0;

        ~Pool()
        {
            if (mem)
            {
                ::operator delete(mem, std::align_val_t(alignof(T)));
                mem = nullptr;
            }
        }
    };

    std::shared_ptr<Pool> pool_;
};

template <class T, class Alloc = std::allocator<T>>
class SimpleList
{
    struct Node
    {
        T v;
        Node *next;
        Node(const T &x) : v(x), next(nullptr) {}
        Node(T &&x) : v(std::move(x)), next(nullptr) {}
    };

    using NodeAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<Node>;
    using AT = std::allocator_traits<NodeAlloc>;

public:
    SimpleList() = default;
    explicit SimpleList(const Alloc &a) : alloc_(a) {}
    ~SimpleList() { clear(); }

    void push_back(const T &x) { emplace_back(x); }
    void push_back(T &&x) { emplace_back(std::move(x)); }

    template <class... Args>
    void emplace_back(Args &&...args)
    {
        NodeAlloc a(alloc_);
        try
        {
            AT::construct(a, n, T(std::forward<Args>(args)...));
        }
        catch (...)
        {
            AT::deallocate(a, n, 1);
            throw;
        }
        if (!head_)
            head_ = tail_ = n;
        else
        {
            tail_->next = n;
            tail_ = n;
        }
    }

    template <class F>
    void for_each(F f) const
    {
        for (Node *p = head_; p; p = p->next)
            f(p->v);
    }

    void clear()
    {
        NodeAlloc a(alloc_);
        Node *p = head_;
        while (p)
        {
            Node *q = p->next;
            AT::destroy(a, p);
            AT::deallocate(a, p, 1);
            p = q;
        }
        head_ = tail_ = nullptr;
    }

private:
    Node *head_ = nullptr;
    Node *tail_ = nullptr;
    Alloc alloc_{};
};

static int fact(int n)
{
    int r = 1;
    for (int i = 2; i <= n; ++i)
        r *= i;
    return r;
}

int main()
{
    {
        std::map<int, int> m;
        for (int i = 0; i < 10; ++i)
            m.emplace(i, fact(i));
        for (const auto &kv : m)
            std::cout << kv.first << ' ' << kv.second << '\n';
    }
    std::cout << "---\n";

    {
        using Pair = std::pair<const int, int>;
        using AMap = FixedAlloc<Pair, 11>;
        std::map<int, int, std::less<int>, AMap> m((std::less<int>{}), AMap{});
        for (int i = 0; i < 10; ++i)
            m.emplace(i, fact(i));
        for (const auto &kv : m)
            std::cout << kv.first << ' ' << kv.second << '\n';
    }
    std::cout << "---\n";

    {
        SimpleList<int> lst;
        for (int i = 0; i < 10; ++i)
            lst.push_back(i);
        lst.for_each([](int x)
                     { std::cout << x << '\n'; });
    }
    std::cout << "---\n";

    {
        using AList = FixedAlloc<int, 10>;
        SimpleList<int, AList> lst(AList{});
        for (int i = 0; i < 10; ++i)
            lst.push_back(i);
        lst.for_each([](int x)
                     { std::cout << x << '\n'; });
    }
}
