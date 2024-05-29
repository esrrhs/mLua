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
            Delete();
        }
    }

    virtual void Delete() {
        delete this;
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

// T must be a class derived from RefCntObj, use to manage the life cycle of T
template<typename T>
class WeakPtr {
public:
    WeakPtr() : m_ptr(0) {}

    WeakPtr(const SharedPtr<T> &ptr) : m_ptr(ptr.get()) {}

    WeakPtr(const WeakPtr &rhs) : m_ptr(rhs.m_ptr) {}

    ~WeakPtr() {}

    WeakPtr &operator=(const WeakPtr &rhs) {
        m_ptr = rhs.m_ptr;
        return *this;
    }

    SharedPtr<T> lock() const {
        if (!m_ptr || m_ptr->Ref() == 0) {
            return SharedPtr<T>();
        }
        return SharedPtr<T>(m_ptr);
    }

    void reset() {
        m_ptr = 0;
    }

    bool expired() const {
        return m_ptr && m_ptr->Ref() == 0;
    }

    bool operator==(const WeakPtr &rhs) const {
        return m_ptr == rhs.m_ptr;
    }

    bool operator!=(const WeakPtr &rhs) const {
        return m_ptr != rhs.m_ptr;
    }

private:
    T *m_ptr;
};

// a simple string class, use to store string data
class String : public RefCntObj {
public:
    String(const char *str, size_t len) : m_str(str, len) {}

    String(const std::string &str) : m_str(str) {}

    virtual ~String() {}

    virtual void Delete();

    const char *c_str() const { return m_str.c_str(); }

    size_t size() const { return m_str.size(); }

    bool empty() const { return m_str.empty(); }

private:
    std::string m_str;
};

typedef SharedPtr<String> StringPtr;
typedef WeakPtr<String> WeakStringPtr;

// a simple string view class, std::string_view is not available in c++11
class StringView {
public:
    StringView() : m_str(0), m_len(0) {}

    StringView(const char *str, size_t len) : m_str(str), m_len(len) {}

    StringView(const std::string &str) : m_str(str.c_str()), m_len(str.size()) {}

    StringView(const StringPtr &str) : m_str(str->c_str()), m_len(str->size()) {}

    const char *data() const { return m_str; }

    size_t size() const { return m_len; }

    bool empty() const { return m_len == 0; }

    bool operator==(const StringView &rhs) const {
        if (m_len != rhs.m_len) {
            return false;
        }
        return memcmp(m_str, rhs.m_str, m_len) == 0;
    }

    bool operator!=(const StringView &rhs) const {
        if (m_len != rhs.m_len) {
            return true;
        }
        return memcmp(m_str, rhs.m_str, m_len) != 0;
    }

protected:
    const char *m_str;
    size_t m_len;
};

class HashStringView : public StringView {
public:
    HashStringView(const char *str, size_t len) : StringView(str, len) { m_hash = Hash(str, m_len); }

    HashStringView(const std::string &str) : StringView(str.c_str(), str.size()) { m_hash = Hash(m_str, m_len); }

    HashStringView(const StringView &str) : StringView(str.data(), str.size()) { m_hash = Hash(m_str, m_len); }

    HashStringView(const StringPtr &str) : StringView(str->c_str(), str->size()) { m_hash = Hash(m_str, m_len); }

    HashStringView(const StringPtr &str, size_t hash) : StringView(str->c_str(), str->size()), m_hash(hash) {}

    size_t hash() const { return m_hash; }

    bool operator==(const HashStringView &rhs) const {
        if (m_hash != rhs.m_hash || m_len != rhs.m_len) {
            return false;
        }
        return memcmp(m_str, rhs.m_str, m_len) == 0;
    }

    bool operator!=(const HashStringView &rhs) const {
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
    size_t m_hash;
};

struct HashStringViewHash {
    size_t operator()(const HashStringView &str) const {
        return str.hash();
    }
};

struct HashStringViewEqual {
    bool operator()(const HashStringView &str1, const HashStringView &str2) const {
        return str1 == str2;
    }
};

// a simple string heap class, use to store unique string data
class StringHeap {
public:
    StringHeap() {}

    ~StringHeap() {}

    StringPtr Add(HashStringView str) {
        auto it = m_string_map.find(str);
        if (it != m_string_map.end()) {
            return it->second.lock();
        }
        auto value = MakeShared<String>(str.data(), str.size());
        auto key = HashStringView(value, str.hash());
        m_string_map[key] = value;
        return value;
    }

    void Remove(HashStringView str) {
        LLOG("StringHeap remove string %s", str.data());
        m_string_map.erase(str);
    }

private:
    std::unordered_map<HashStringView, WeakStringPtr, HashStringViewHash, HashStringViewEqual> m_string_map;
};

enum MessageIdType {
    mt_int32 = 1,
    mt_uint32 = 2,
    mt_int64 = 3,
    mt_uint64 = 4,
    mt_float = 5,
    mt_double = 6,
    mt_bool = 7,
    mt_string = 8,
};

// same as lua table, use to store key-value schema data
class Layout : public RefCntObj {
public:
    Layout() {}

    ~Layout() {}

    struct Member : public RefCntObj {
        std::string name;
        std::string type;
        std::string key;
        std::string value;
        int pos;
        int size;
        int tag;
        int shared;
        int message_id;
        int value_message_id;
        int key_size;
        int key_shared;
    };

    typedef SharedPtr<Member> MemberPtr;

    void SetMember(int tag, MemberPtr member) {
        m_member[tag] = member;
    }

    MemberPtr GetMember(int tag) {
        return m_member[tag];
    }

    std::unordered_map<int, MemberPtr> &GetMember() {
        return m_member;
    }

    void SetName(const std::string &name) {
        m_name = name;
    }

    const std::string &GetName() const {
        return m_name;
    }

    int GetTotalSize() const {
        return m_total_size;
    }

    void SetTotalSize(int size) {
        m_total_size = size;
    }

    void SetMessageId(int message_id) {
        m_message_id = message_id;
    }

    int GetMessageId() const {
        return m_message_id;
    }

private:
    int m_message_id;
    std::string m_name;
    std::unordered_map<int, MemberPtr> m_member;
    int m_total_size;
};

typedef SharedPtr<Layout> LayoutPtr;

class LayoutMgr {
public:
    LayoutMgr() {}

    ~LayoutMgr() {}

    LayoutPtr GetLayout(const std::string &name) {
        auto it = m_layout.find(name);
        if (it != m_layout.end()) {
            return it->second;
        }
        return 0;
    }

    void SetLayout(const std::string &name, LayoutPtr layout) {
        m_layout[name] = layout;
    }

    int GetMessageId(const std::string &name) {
        auto it = m_message_id.find(name);
        if (it != m_message_id.end()) {
            return it->second;
        }
        return -1;
    }

    void SetMessageId(const std::string &name, int message_id) {
        m_message_id[name] = message_id;
    }

private:
    std::unordered_map<std::string, LayoutPtr> m_layout;
    std::unordered_map<std::string, int> m_message_id;
};

// use to store lua struct data
class Container : public RefCntObj {
public:
    Container(LayoutPtr layout);

    ~Container();

    StringView GetName() const {
        return m_layout->GetName();
    }

    int GetMessageId() const {
        return m_layout->GetMessageId();
    }

    template<typename T>
    bool Get(int idx, T &value, bool &is_nil) {
        int max = idx + 1 + sizeof(T);
        if (idx < 0 || max > m_layout->GetTotalSize()) {
            return false;
        }
        if (max > (int) m_buffer.size()) {
            // hot fix, new member added, just return nil
            is_nil = true;
            return true;
        }
        char flag = m_buffer[idx] & 0x01;
        if (flag) {
            is_nil = false;
            value = *(T *) (m_buffer.data() + idx + 1);
        } else {
            is_nil = true;
        }
        return true;
    }

    template<typename T>
    bool Set(int idx, const T &value, bool is_nil) {
        int max = idx + 1 + sizeof(T);
        if (idx < 0 || max > m_layout->GetTotalSize()) {
            return false;
        }
        if (max > (int) m_buffer.size()) {
            // hot fix, new member added, need to resize buffer, use double size
            m_buffer.reserve(std::min(m_layout->GetTotalSize(), (int) (2 * m_buffer.capacity())));
            m_buffer.resize(max);
        }
        if (is_nil) {
            m_buffer[idx] &= 0xfe;
        } else {
            m_buffer[idx] |= 0x01;
            *(T *) (m_buffer.data() + idx + 1) = value;
        }
        return true;
    }

    template<typename T>
    bool GetSharedObj(int idx, SharedPtr<T> &out, bool &is_nil) {
        T *old = 0;
        bool is_old_nil = false;
        auto ret = Get<T *>(idx, old, is_old_nil);
        if (!ret) {
            return false;
        }
        if (is_old_nil) {
            is_nil = true;
        } else {
            out = SharedPtr<T>(old);
            is_nil = false;
        }
        return true;
    }

    template<typename T>
    bool SetSharedObj(int idx, SharedPtr<T> in, bool is_nil) {
        T *old = 0;
        bool is_old_nil = false;
        auto ret = Get<T *>(idx, old, is_old_nil);
        if (!ret) {
            return false;
        }
        if (is_nil) {
            if (!is_old_nil) {
                old->Release();
            }
            return Set<T *>(idx, 0, true);
        } else {
            in.get()->AddRef();
            if (!is_old_nil) {
                old->Release();
            }
            return Set<T *>(idx, in.get(), false);
        }
    }

private:
    void ReleaseAllSharedObj();

private:
    LayoutPtr m_layout;
    std::vector<char> m_buffer;
};

typedef SharedPtr<Container> ContainerPtr;

// use to store lua array data
class Array : public RefCntObj {
public:
    Array(Layout::MemberPtr layout_member);

    ~Array();

    StringView GetName() const {
        return m_layout_member->name;
    }

    Layout::MemberPtr GetLayoutMember() const {
        return m_layout_member;
    }

    int GetMessageId() const {
        return m_layout_member->message_id;
    }

    template<typename T>
    bool Get(int idx, T &value, bool &is_nil) {
        idx = idx * m_layout_member->key_size;
        int max = idx + sizeof(T);
        if (idx < 0) {
            return false;
        }
        if (max > (int) m_buffer.size()) {
            // out of range, just return nil
            is_nil = true;
            return true;
        }
        char flag = m_buffer[idx] & 0x01;
        if (flag) {
            is_nil = false;
            value = *(T *) (m_buffer.data() + idx + 1);
        } else {
            is_nil = true;
        }
        return true;
    }

    template<typename T>
    bool Set(int idx, const T &value, bool is_nil) {
        idx = idx * m_layout_member->key_size;
        int max = idx + 1 + sizeof(T);
        if (idx < 0) {
            return false;
        }
        if (max > (int) m_buffer.size()) {
            // out of range, need to resize buffer, use double size
            m_buffer.reserve(2 * m_buffer.capacity());
            m_buffer.resize(max);
        }
        if (is_nil) {
            m_buffer[idx] &= 0xfe;
        } else {
            m_buffer[idx] |= 0x01;
            *(T *) (m_buffer.data() + idx + 1) = value;
        }
        return true;
    }

    template<typename T>
    bool GetSharedObj(int idx, SharedPtr<T> &out, bool &is_nil) {
        T *old = 0;
        bool is_old_nil = false;
        auto ret = Get<T *>(idx, old, is_old_nil);
        if (!ret) {
            return false;
        }
        if (is_old_nil) {
            is_nil = true;
        } else {
            out = SharedPtr<T>(old);
            is_nil = false;
        }
        return true;
    }

    template<typename T>
    bool SetSharedObj(int idx, SharedPtr<T> in, bool is_nil) {
        T *old = 0;
        bool is_old_nil = false;
        auto ret = Get<T *>(idx, old, is_old_nil);
        if (!ret) {
            return false;
        }
        if (is_nil) {
            if (!is_old_nil) {
                old->Release();
            }
            return Set<T *>(idx, 0, true);
        } else {
            in.get()->AddRef();
            if (!is_old_nil) {
                old->Release();
            }
            return Set<T *>(idx, in.get(), false);
        }
    }

private:
    void ReleaseAllSharedObj();

private:
    Layout::MemberPtr m_layout_member;
    std::vector<char> m_buffer;
};

typedef SharedPtr<Array> ArrayPtr;

class Map : public RefCntObj {
public:
    Map(Layout::MemberPtr layout_member);

    ~Map();

    StringView GetName() const {
        return m_layout_member->name;
    }

    Layout::MemberPtr GetLayoutMember() const {
        return m_layout_member;
    }

    int GetKeyMessageId() const {
        return m_layout_member->message_id;
    }

    int GetValueMessageId() const {
        return m_layout_member->value_message_id;
    }

    union MapValue32 {
        int32_t m_32;
        uint32_t m_u32;
        float m_float;
        bool m_bool;
    };

    union MapValue64 {
        int64_t m_64;
        uint64_t m_u64;
        double m_double;
        String *m_string;
        Container *m_obj;
    };

    static_assert(sizeof(MapValue32) == 4, "MapValue32 size must be 4");
    static_assert(sizeof(MapValue64) == 8, "MapValue64 size must be 8");

    union MapPointer {
        void *m_void;

        std::unordered_map<int32_t, MapValue32> *m_32_32;
        std::unordered_map<int32_t, MapValue64> *m_32_64;

        std::unordered_map<int64_t, MapValue32> *m_64_32;
        std::unordered_map<int64_t, MapValue64> *m_64_64;
    };

    MapPointer GetMap() const {
        return m_map;
    }

    MapValue32 Get32by32(int32_t key, bool &is_nil) {
        auto it = m_map.m_32_32->find(key);
        if (it != m_map.m_32_32->end()) {
            is_nil = false;
            return it->second;
        }
        is_nil = true;
        return MapValue32();
    }

    MapValue64 Get64by32(int32_t key, bool &is_nil) {
        auto it = m_map.m_32_64->find(key);
        if (it != m_map.m_32_64->end()) {
            is_nil = false;
            return it->second;
        }
        is_nil = true;
        return MapValue64();
    }

    MapValue32 Get32by64(int64_t key, bool &is_nil) {
        auto it = m_map.m_64_32->find(key);
        if (it != m_map.m_64_32->end()) {
            is_nil = false;
            return it->second;
        }
        is_nil = true;
        return MapValue32();
    }

    MapValue64 Get64by64(int64_t key, bool &is_nil) {
        auto it = m_map.m_64_64->find(key);
        if (it != m_map.m_64_64->end()) {
            is_nil = false;
            return it->second;
        }
        is_nil = true;
        return MapValue64();
    }

private:
    void ReleaseAllSharedObj();

private:
    Layout::MemberPtr m_layout_member;
    MapPointer m_map;
};

typedef SharedPtr<Map> MapPtr;

// use to store Container which passed to lua
class LuaContainerHolder {
public:
    LuaContainerHolder() {}

    ~LuaContainerHolder();

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

    ArrayPtr GetArray(void *ptr) {
        auto it = m_array.find(ptr);
        if (it != m_array.end()) {
            return it->second;
        }
        return 0;
    }

    void SetArray(void *ptr, ArrayPtr array) {
        m_array[ptr] = array;
    }

    void RemoveArray(void *ptr) {
        m_array.erase(ptr);
    }

    MapPtr GetMap(void *ptr) {
        auto it = m_map.find(ptr);
        if (it != m_map.end()) {
            return it->second;
        }
        return 0;
    }

    void SetMap(void *ptr, MapPtr map) {
        m_map[ptr] = map;
    }

    void RemoveMap(void *ptr) {
        m_map.erase(ptr);
    }

private:
    std::unordered_map<void *, ContainerPtr> m_container;
    std::unordered_map<void *, ArrayPtr> m_array;
    std::unordered_map<void *, MapPtr> m_map;
};

}

std::vector<luaL_Reg> GetCppTableFuncs();
