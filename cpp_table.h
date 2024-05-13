#include "core.h"

namespace cpp_table {

// there is no loop reference in protobuf defined message, so we can use a simple reference count
class RefCntObj {
public:
    RefCntObj() : m_ref(0) {}

    virtual ~RefCntObj() {}

    void AddRef() { ++m_ref; }

    void Release() {
        if (--m_ref == 0) {
            delete this;
        }
    }

    int Ref() const { return m_ref; }

private:
    int m_ref;
};

// T must be a class derived from RefCntObj, use to manage the life cycle of T
// simpler than std::shared_ptr, no weak_ptr, no custom deleter, and single thread only
template<typename T>
class SharedPtr {
public:
    SharedPtr() : m_ptr(0) {}

    SharedPtr(T *ptr) : m_ptr(ptr) {
        if (m_ptr) {
            m_ptr->AddRef();
        }
    }

    SharedPtr(const SharedPtr &rhs) : m_ptr(rhs.m_ptr) {
        if (m_ptr) {
            m_ptr->AddRef();
        }
    }

    ~SharedPtr() {
        if (m_ptr) {
            m_ptr->Release();
        }
    }

    SharedPtr &operator=(const SharedPtr &rhs) {
        if (m_ptr != rhs.m_ptr) {
            if (m_ptr) {
                m_ptr->Release();
            }
            m_ptr = rhs.m_ptr;
            if (m_ptr) {
                m_ptr->AddRef();
            }
        }
        return *this;
    }

    T *operator->() const { return m_ptr; }

    T &operator*() const { return *m_ptr; }

    T *get() const { return m_ptr; }

    operator bool() const { return m_ptr != 0; }

    bool operator==(const SharedPtr &rhs) const { return m_ptr == rhs.m_ptr; }

    bool operator!=(const SharedPtr &rhs) const { return m_ptr != rhs.m_ptr; }

private:
    T *m_ptr;
};

// make a shared pointer
template<typename T, typename... Args>
SharedPtr<T> MakeShared(Args &&... args) {
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
}

// a simple string class, use to store string data
class String : public RefCntObj {
public:
    String(const char *str, size_t len) : m_str(str, len) {}

    String(const std::string &str) : m_str(str) {}

    const char *c_str() const { return m_str.c_str(); }

    size_t size() const { return m_str.size(); }

    bool empty() const { return m_str.empty(); }

private:
    std::string m_str;
};

typedef SharedPtr<String> StringPtr;

// a simple string view class, std::string_view is not available in c++11
class StringView {
public:
    StringView() : m_str(0), m_len(0) {}

    StringView(const char *str, size_t len) : m_str(str), m_len(len) { m_hash = Hash(str, m_len); }

    StringView(const std::string &str) : m_str(str.c_str()), m_len(str.size()) { m_hash = Hash(m_str, m_len); }

    StringView(const StringPtr &str) : m_str(str->c_str()), m_len(str->size()) { m_hash = Hash(m_str, m_len); }

    const char *data() const { return m_str; }

    size_t size() const { return m_len; }

    bool empty() const { return m_len == 0; }

    size_t hash() const { return m_hash; }

    bool operator==(const StringView &rhs) const {
        if (m_hash != rhs.m_hash || m_len != rhs.m_len) {
            return false;
        }
        return memcmp(m_str, rhs.m_str, m_len) == 0;
    }

    bool operator!=(const StringView &rhs) const {
        if (m_hash != rhs.m_hash || m_len != rhs.m_len) {
            return true;
        }
        return memcmp(m_str, rhs.m_str, m_len) != 0;
    }

private:
    static size_t Hash(const char *str, size_t len) {
        unsigned long h = 0x5bd1e995;
        const unsigned char *data = (const unsigned char *) str;

        while (len >= 4) {
            unsigned int k = *(unsigned int *) data;
            h ^= k;
            data += 4;
            len -= 4;
        }

        // Handle the last few bytes of the input array
        switch (len) {
            case 3:
                h ^= data[2] << 16;
            case 2:
                h ^= data[1] << 8;
            case 1:
                h ^= data[0];
        }
        return h;
    }

private:
    const char *m_str;
    size_t m_len;
    size_t m_hash;
};

struct StringViewHash {
    size_t operator()(const StringView &str) const {
        return str.hash();
    }
};

struct StringViewEqual {
    bool operator()(const StringView &str1, const StringView &str2) const {
        return str1 == str2;
    }
};

// a simple string heap class, use to store unique string data
class StringHeap {
public:
    StringHeap() {}

    ~StringHeap() {}

    StringPtr Add(StringView str) {
        auto it = m_string.find(str);
        if (it != m_string.end()) {
            return it->second;
        }
        auto ret = MakeShared<String>(str.data(), str.size());
        m_string[StringView(ret)] = ret;
        return ret;
    }

    void Remove(StringView str) {
        m_string.erase(str);
    }

private:
    std::unordered_map<StringView, StringPtr, StringViewHash, StringViewEqual> m_string;
};

// use to store lua struct data
class Container : public RefCntObj {
public:
    Container(StringView name, int initial_size);

    ~Container();

    StringView GetName() const { return m_name; }

    template<typename T>
    T *Get(int idx) {
        if (idx < 0 || idx + sizeof(T) > m_buffer.size()) {
            return 0;
        }
        return (T *) (m_buffer.data() + idx);
    }

    template<typename T>
    bool Set(int idx, const T &value) {
        if (idx < 0 || idx + sizeof(T) > m_buffer.size()) {
            return false;
        }
        *(T *) (m_buffer.data() + idx) = value;
        return true;
    }

private:
    StringPtr m_name;
    std::vector<char> m_buffer;
};

typedef SharedPtr<Container> ContainerPtr;

// use to store Container which passed to lua
class LuaContainerHolder {
public:
    LuaContainerHolder() {}

    ~LuaContainerHolder() {}

    ContainerPtr Get(void *ptr) {
        auto it = m_container.find(ptr);
        if (it != m_container.end()) {
            return it->second;
        }
        return 0;
    }

    void Set(void *ptr, ContainerPtr container) {
        m_container[ptr] = container;
    }

    void Remove(void *ptr) {
        m_container.erase(ptr);
    }

private:
    std::unordered_map<void *, ContainerPtr> m_container;
};

}

std::vector<luaL_Reg> GetCppTableFuncs();
