#include <cstddef>
#include <type_traits>
#include <new> // std::align_val_t, aligned operator new/delete
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

    FixedAlloc() noexcept = default;
    template <class U>
    FixedAlloc(const FixedAlloc<U, N> &) noexcept {}

    [[nodiscard]] T *allocate(std::size_t n)
    {
        if (n == 0)
            return nullptr;
        Pool &P = pool();
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

    template <class U>
    bool operator==(const FixedAlloc<U, N> &) const noexcept { return true; }
    template <class U>
    bool operator!=(const FixedAlloc<U, N> &) const noexcept { return false; }

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

    static Pool &pool()
    {
        static Pool P;
        return P;
    }
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

    explicit SimpleList(const Alloc &a) : node_alloc_(NodeAlloc(a)) {}

    ~SimpleList() { clear(); }

    void push_back(const T &x) { emplace_back(x); }
    void push_back(T &&x) { emplace_back(std::move(x)); }

    template <class... Args>
    void emplace_back(Args &&...args)
    {
        Node *nodep = AT::allocate(node_alloc_, 1);
        try
        {
            AT::construct(node_alloc_, nodep, T(std::forward<Args>(args)...));
        }
        catch (...)
        {
            AT::deallocate(node_alloc_, nodep, 1);
            throw;
        }
        if (!head_)
            head_ = tail_ = nodep;
        else
        {
            tail_->next = nodep;
            tail_ = nodep;
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
        Node *p = head_;
        while (p)
        {
            Node *q = p->next;
            AT::destroy(node_alloc_, p);
            AT::deallocate(node_alloc_, p, 1);
            p = q;
        }
        head_ = tail_ = nullptr;
    }

private:
    Node *head_ = nullptr;
    Node *tail_ = nullptr;
    NodeAlloc node_alloc_{};
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
