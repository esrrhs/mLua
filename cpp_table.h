#pragma once

#include "core.h"
#include "coalesced_hashmap.h"

namespace cpp_table {

enum RefObjType {
    rot_container,
    rot_array,
    rot_map,
    rot_string,
    rot_layout,
    rot_layout_member,
};

// there is no loop reference in protobuf defined message, so we can use a simple reference count
class RefCntObj {
public:
    RefCntObj(RefObjType type) : m_type(type), m_ref(0) {}

    ~RefCntObj() {}

    void AddRef() { ++m_ref; }

    void Release() {
        if (--m_ref == 0) {
            Delete();
        }
    }

    void Delete();

    int Ref() const { return m_ref; }

private:
    RefObjType m_type: 8;
    int m_ref: 24;
};

static_assert(sizeof(RefCntObj) == 4, "RefCntObj size must be 4");

// T must be a class derived from RefCntObj, use to manage the life cycle of T
// simpler than std::shared_ptr, no weak_ptr, and single thread only
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
    auto p = (T *) malloc(sizeof(T));
    new(p) T(std::forward<Args>(args)...);
    return SharedPtr<T>(p);
}

template<typename T, typename... Args>
SharedPtr<T> MakeSharedBySize(size_t sz, Args &&... args) {
    auto p = (T *) malloc(sz);
    new(p) T(std::forward<Args>(args)...);
    return SharedPtr<T>(p);
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

    T *get() const {
        return m_ptr;
    }

private:
    T *m_ptr;
};

static size_t StringHash(const char *str, size_t len) {
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

// a simple string class, use to store string data
class String : public RefCntObj {
public:
    String(const char *str, size_t len) : RefCntObj(rot_string) {
        m_len = len;
        memcpy(m_str, str, len);
        m_str[len] = 0;
    }

    ~String();

    const char *c_str() const { return m_str; }

    const char *data() const { return m_str; }

    size_t size() const { return m_len; }

    bool empty() const { return m_len == 0; }

    size_t hash() const { return StringHash(m_str, m_len); }

    bool operator==(const String &rhs) const {
        if (m_len != rhs.m_len) {
            return false;
        }
        return memcmp(m_str, rhs.m_str, m_len) == 0;
    }

    bool operator!=(const String &rhs) const {
        return !(*this == rhs);
    }

private:
    int m_len = 0;
    char m_str[0];
};

static_assert(sizeof(String) == 8, "String size must be 8");

typedef SharedPtr<String> StringPtr;
typedef WeakPtr<String> WeakStringPtr;

struct StringPtrHash {
    size_t operator()(const StringPtr &str) const {
        return str->hash();
    }
};

struct StringPtrEqual {
    bool operator()(const StringPtr &str1, const StringPtr &str2) const {
        return str1.get() == str2.get();
    }
};

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
        return !(*this == rhs);
    }

    size_t hash() const { return StringHash(m_str, m_len); }

protected:
    const char *m_str = 0;
    int m_len = 0;
};

// a simple string heap class, use to store unique string data
class StringHeap {
public:
    StringHeap() {}

    ~StringHeap() {}

    StringPtr Add(StringView str) {
        WeakStringPtr wv;
        if (m_string_set.Find(str, wv)) {
            return wv.lock();
        }
        auto value = MakeSharedBySize<String>(sizeof(String) + str.size() + 1, str.data(), str.size());
        m_string_set.Insert(value);
        return value;
    }

    void Remove(StringView str) {
        LLOG("StringHeap remove string %s", str.data());
        m_string_set.Erase(str);
    }

    std::vector<StringPtr> Dump() {
        std::vector<StringPtr> ret;
        for (auto it = m_string_set.Begin(); it != m_string_set.End(); ++it) {
            auto str = it.GetKey();
            auto ptr = str.lock();
            if (ptr) {
                ret.push_back(ptr);
            }
        }
        return ret;
    }

private:
    struct WeakStringHash {
        size_t operator()(const WeakStringPtr &str) const {
            return str.get()->hash();
        }

        size_t operator()(const StringView &str) const {
            return str.hash();
        }
    };

    struct WeakStringEqual {
        bool operator()(const WeakStringPtr &str1, const WeakStringPtr &str2) const {
            return *str1.get() == *str2.get();
        }

        bool operator()(const WeakStringPtr &str1, const StringView &str2) const {
            return StringView(str1.get()->data(), str1.get()->size()) == str2;
        }
    };

    coalesced_hashmap::CoalescedHashSet <WeakStringPtr, WeakStringHash, WeakStringEqual> m_string_set;
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
    Layout() : RefCntObj(rot_layout) {}

    ~Layout() {}

    struct Member : public RefCntObj {
        Member() : RefCntObj(rot_layout_member) {}

        void CopyFrom(Member *other) {
            name = other->name;
            type = other->type;
            key = other->key;
            value = other->value;
            pos = other->pos;
            size = other->size;
            tag = other->tag;
            shared = other->shared;
            message_id = other->message_id;
            value_message_id = other->value_message_id;
            key_size = other->key_size;
            key_shared = other->key_shared;
        }

        StringPtr name;
        StringPtr type;
        StringPtr key;
        StringPtr value;
        int pos = 0;
        int size = 0;
        int tag = 0;
        int shared = 0;
        int message_id = 0;
        int value_message_id = 0;
        int key_size = 0;
        int key_shared = 0;
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

    void SetName(StringPtr name) {
        m_name = name;
    }

    StringPtr GetName() const {
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
    StringPtr m_name;
    std::unordered_map<int, MemberPtr> m_member;
    int m_total_size;
};

typedef SharedPtr<Layout> LayoutPtr;
static_assert(sizeof(LayoutPtr) == 8, "LayoutPtr size must be 8");

class LayoutMgr {
public:
    LayoutMgr() {}

    ~LayoutMgr() {}

    LayoutPtr GetLayout(StringPtr name) {
        auto it = m_layout.find(name);
        if (it != m_layout.end()) {
            return it->second;
        }
        return 0;
    }

    void SetLayout(StringPtr name, LayoutPtr layout) {
        m_layout[name] = layout;
    }

    int GetMessageId(StringPtr name) {
        auto it = m_message_id.find(name);
        if (it != m_message_id.end()) {
            return it->second;
        }
        return -1;
    }

    void SetMessageId(StringPtr name, int message_id) {
        m_message_id[name] = message_id;
    }

private:
    std::unordered_map<StringPtr, LayoutPtr, StringPtrHash, StringPtrEqual> m_layout;
    std::unordered_map<StringPtr, int, StringPtrHash, StringPtrEqual> m_message_id;
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
        if (max > m_buffer_size) {
            // hot fix, new member added, just return nil
            is_nil = true;
            return true;
        }
        char flag = m_buffer[idx] & 0x01;
        if (flag) {
            is_nil = false;
            value = *(T *) (m_buffer + idx + 1);
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
        if (max > m_buffer_size) {
            // hot fix, new member added, need to resize buffer, use double size
            auto new_size = std::min(m_layout->GetTotalSize(), max * 2);
            auto new_buffer = new char[new_size];
            memset(new_buffer, 0, new_size);
            if (m_buffer) {
                memcpy(new_buffer, m_buffer, m_buffer_size);
                delete[] m_buffer;
            }
            m_buffer = new_buffer;
            m_buffer_size = new_size;
        }
        if (is_nil) {
            m_buffer[idx] &= 0xfe;
        } else {
            m_buffer[idx] |= 0x01;
            *(T *) (m_buffer + idx + 1) = value;
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
    int m_buffer_size = 0;
    LayoutPtr m_layout;
    char *m_buffer = 0;
};

static_assert(sizeof(Container) == 24, "Container size must be 24");
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
        if (max > m_buffer_size) {
            // out of range, just return nil
            is_nil = true;
            return true;
        }
        char flag = m_buffer[idx] & 0x01;
        if (flag) {
            is_nil = false;
            value = *(T *) (m_buffer + idx + 1);
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
        if (max > m_buffer_size) {
            // out of range, need to resize buffer, use double size
            auto new_size = 2 * max;
            auto new_buffer = new char[new_size];
            memset(new_buffer, 0, new_size);
            if (m_buffer) {
                memcpy(new_buffer, m_buffer, m_buffer_size);
                delete[] m_buffer;
            }
            m_buffer = new_buffer;
            m_buffer_size = new_size;
        }
        if (is_nil) {
            m_buffer[idx] &= 0xfe;
        } else {
            m_buffer[idx] |= 0x01;
            *(T *) (m_buffer + idx + 1) = value;
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
    int m_buffer_size = 0;
    Layout::MemberPtr m_layout_member;
    char *m_buffer = 0;
};

static_assert(sizeof(Array) == 24, "Array size must be 24");
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
        MapValue32() : m_32(0) {}

        int32_t m_32;
        uint32_t m_u32;
        float m_float;
        bool m_bool;
    };

    union MapValue64 {
        MapValue64() : m_64(0) {}

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

        typedef coalesced_hashmap::CoalescedHashMap <int32_t, MapValue32> Map32by32;
        typedef coalesced_hashmap::CoalescedHashMap <int32_t, MapValue64> Map64by32;
        Map32by32 *m_32_32;
        Map64by32 *m_32_64;

        typedef coalesced_hashmap::CoalescedHashMap <int64_t, MapValue32> Map32by64;
        typedef coalesced_hashmap::CoalescedHashMap <int64_t, MapValue64> Map64by64;
        Map32by64 *m_64_32;
        Map64by64 *m_64_64;

        typedef coalesced_hashmap::CoalescedHashMap <StringPtr, MapValue32, StringPtrHash, StringPtrEqual> Map32byString;
        typedef coalesced_hashmap::CoalescedHashMap <StringPtr, MapValue64, StringPtrHash, StringPtrEqual> Map64byString;
        Map32byString *m_string_32;
        Map64byString *m_string_64;
    };

    MapPointer GetMap() const {
        return m_map;
    }

    MapValue32 Get32by32(int32_t key, bool &is_nil) {
        MapValue32 value;
        is_nil = !m_map.m_32_32->Find(key, value);
        return value;
    }

    MapValue64 Get64by32(int32_t key, bool &is_nil) {
        MapValue64 value;
        is_nil = !m_map.m_32_64->Find(key, value);
        return value;
    }

    MapValue32 Get32by64(int64_t key, bool &is_nil) {
        MapValue32 value;
        is_nil = !m_map.m_64_32->Find(key, value);
        return value;
    }

    MapValue64 Get64by64(int64_t key, bool &is_nil) {
        MapValue64 value;
        is_nil = !m_map.m_64_64->Find(key, value);
        return value;
    }

    MapValue32 Get32byString(StringPtr key, bool &is_nil) {
        MapValue32 value;
        is_nil = !m_map.m_string_32->Find(key, value);
        return value;
    }

    MapValue64 Get64byString(StringPtr key, bool &is_nil) {
        MapValue64 value;
        is_nil = !m_map.m_string_64->Find(key, value);
        return value;
    }

    void Set32by32(int32_t key, MapValue32 value) {
        if (!m_map.m_32_32) {
            m_map.m_32_32 = new MapPointer::Map32by32();
        }
        m_map.m_32_32->Insert(key, value);
    }

    void Set64by32(int32_t key, MapValue64 value) {
        if (!m_map.m_32_64) {
            m_map.m_32_64 = new MapPointer::Map64by32();
        }
        m_map.m_32_64->Insert(key, value);
    }

    void Set32by64(int64_t key, MapValue32 value) {
        if (!m_map.m_64_32) {
            m_map.m_64_32 = new MapPointer::Map32by64();
        }
        m_map.m_64_32->Insert(key, value);
    }

    void Set64by64(int64_t key, MapValue64 value) {
        if (!m_map.m_64_64) {
            m_map.m_64_64 = new MapPointer::Map64by64();
        }
        m_map.m_64_64->Insert(key, value);
    }

    void Set32byString(StringPtr key, MapValue32 value) {
        if (!m_map.m_string_32) {
            m_map.m_string_32 = new MapPointer::Map32byString();
        }
        m_map.m_string_32->Insert(key, value);
    }

    void Set64byString(StringPtr key, MapValue64 value) {
        if (!m_map.m_string_64) {
            m_map.m_string_64 = new MapPointer::Map64byString();
        }
        m_map.m_string_64->Insert(key, value);
    }

    void Remove32by32(int32_t key) {
        if (m_map.m_32_32) {
            m_map.m_32_32->Erase(key);
        }
    }

    void Remove64by32(int32_t key) {
        if (m_map.m_32_64) {
            m_map.m_32_64->Erase(key);
        }
    }

    void Remove32by64(int64_t key) {
        if (m_map.m_64_32) {
            m_map.m_64_32->Erase(key);
        }
    }

    void Remove64by64(int64_t key) {
        if (m_map.m_64_64) {
            m_map.m_64_64->Erase(key);
        }
    }

    void Remove32byString(StringPtr key) {
        if (m_map.m_string_32) {
            m_map.m_string_32->Erase(key);
        }
    }

    void Remove64byString(StringPtr key) {
        if (m_map.m_string_64) {
            m_map.m_string_64->Erase(key);
        }
    }

private:
    void ReleaseAllSharedObj();

    void ReleaseStrBy32() {
        for (auto it = m_map.m_32_64->Begin(); it != m_map.m_32_64->End(); ++it) {
            auto v = it.GetValue();
            v.m_string->Release();
        }
    }

    void ReleaseStrBy64() {
        for (auto it = m_map.m_64_64->Begin(); it != m_map.m_64_64->End(); ++it) {
            auto v = it.GetValue();
            v.m_string->Release();
        }
    }

    void ReleaseStrByString() {
        for (auto it = m_map.m_string_64->Begin(); it != m_map.m_string_64->End(); ++it) {
            auto v = it.GetValue();
            v.m_string->Release();
        }
    }

    void ReleaseObjBy32() {
        for (auto it = m_map.m_32_64->Begin(); it != m_map.m_32_64->End(); ++it) {
            auto v = it.GetValue();
            v.m_obj->Release();
        }
    }

    void ReleaseObjBy64() {
        for (auto it = m_map.m_64_64->Begin(); it != m_map.m_64_64->End(); ++it) {
            auto v = it.GetValue();
            v.m_obj->Release();
        }
    }

    void ReleaseObjByString() {
        for (auto it = m_map.m_string_64->Begin(); it != m_map.m_string_64->End(); ++it) {
            auto v = it.GetValue();
            v.m_obj->Release();
        }
    }

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

    size_t GetContainerSize() const {
        return m_container.size();
    }

    size_t GetArraySize() const {
        return m_array.size();
    }

    size_t GetMapSize() const {
        return m_map.size();
    }

private:
    std::unordered_map<void *, ContainerPtr> m_container;
    std::unordered_map<void *, ArrayPtr> m_array;
    std::unordered_map<void *, MapPtr> m_map;
};

}

std::vector<luaL_Reg> GetCppTableFuncs();
