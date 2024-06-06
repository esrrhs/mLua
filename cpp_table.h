#include "core.h"

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
    String(const char *str, size_t len, size_t hash) : RefCntObj(rot_string), m_str(str, len), m_hash(hash) {}

    String(const std::string &str) : RefCntObj(rot_string), m_str(str), m_hash(StringHash(str.c_str(), str.size())) {}

    ~String();

    const char *c_str() const { return m_str.c_str(); }

    const char *data() const { return m_str.c_str(); }

    size_t size() const { return m_str.size(); }

    bool empty() const { return m_str.empty(); }

    size_t hash() const { return m_hash; }

private:
    std::string m_str;
    size_t m_hash;
};

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
    StringView() : m_str(0), m_len(0), m_hash(0) {}

    StringView(const char *str, size_t len, size_t hash) : m_str(str), m_len(len), m_hash(hash) {}

    StringView(const char *str, size_t len) : m_str(str), m_len(len), m_hash(StringHash(str, len)) {}

    StringView(const std::string &str) : m_str(str.c_str()), m_len(str.size()),
                                         m_hash(StringHash(str.c_str(), str.size())) {}

    StringView(const StringPtr &str) : m_str(str->c_str()), m_len(str->size()), m_hash(str->hash()) {}

    const char *data() const { return m_str; }

    size_t size() const { return m_len; }

    bool empty() const { return m_len == 0; }

    bool operator==(const StringView &rhs) const {
        if (m_hash != rhs.m_hash || m_len != rhs.m_len) {
            return false;
        }
        return memcmp(m_str, rhs.m_str, m_len) == 0;
    }

    bool operator!=(const StringView &rhs) const {
        return !(*this == rhs);
    }

    size_t hash() const { return m_hash; }

protected:
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
        auto it = m_string_map.find(str);
        if (it != m_string_map.end()) {
            return it->second.lock();
        }
        auto value = MakeShared<String>(str.data(), str.size(), str.hash());
        auto key = StringView(value);
        m_string_map[key] = value;
        return value;
    }

    void Remove(StringView str) {
        LLOG("StringHeap remove string %s", str.data());
        m_string_map.erase(str);
    }

    std::vector<StringPtr> Dump() {
        std::vector<StringPtr> ret;
        for (auto &it: m_string_map) {
            auto ptr = it.second.lock();
            if (ptr) {
                ret.push_back(ptr);
            }
        }
        return ret;
    }

private:
    std::unordered_map<StringView, WeakStringPtr, StringViewHash, StringViewEqual> m_string_map;
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

class BitMap {
public:
    BitMap(int bit_size) {
        m_size = (bit_size + 7) / 8;
        m_data = new char[m_size];
        memset(m_data, 0, m_size);
    }

    ~BitMap() {
        delete[] m_data;
    }

    void Set(int index) {
        m_data[index / 8] |= (1 << (index % 8));
    }

    void Clear(int index) {
        m_data[index / 8] &= ~(1 << (index % 8));
    }

    bool Test(int index) const {
        return m_data[index / 8] & (1 << (index % 8));
    }

private:
    char *m_data;
    int m_size;
};

static const int primes[] = {2, 5, 7, 11, 17, 23, 37, 53, 79, 113, 167, 251, 373, 557, 839, 1259, 1889,
                             2833, 4243, 6361, 9533, 14249, 21373, 32059, 48089, 72131, 108197, 162293,
                             243439, 365159, 547739, 821609, 1232413, 1848619, 2772929, 4159393, 6239089,
                             9358633, 14037949, 21056923, 31585387, 47378081, 71067121, 106600683,
                             159901019, 239851529, 359777293, 539665939, 809498909, 1214247359,
                             1821371039};

template<typename Key, typename Value, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
class CoalescedHashMap {
public:
    CoalescedHashMap(int size = 1) {
        m_nodes = new Node[size];
        m_size = size;
        InitFreeList();
        m_bitmap = new BitMap(size);
    }

    ~CoalescedHashMap() {
        delete m_bitmap;
        delete[] m_nodes;
    }

    /*
    ** inserts a new key into a hash table; first, check whether key's main
    ** position is free. If not, check whether colliding node is in its main
    ** position or not: if it is not, move colliding node to an empty place and
    ** put new key in its main position; otherwise (colliding node is in its main
    ** position), new key goes to an empty position.
    */
    void Insert(const Key &key, const Value &value) {
        auto mp = MainPosition(key);
        if (Valid(mp)) { /* main position is taken? */
            // try to find key first
            auto cur = mp;
            while (cur != -1) {
                if (Equal()(m_nodes[cur].key, key)) {
                    m_nodes[cur].value = value;
                    return;
                }
                cur = m_nodes[cur].next;
            }

            auto f = GetFreePosition(); /* get a free place */
            if (f < 0) { /* cannot find a free place? */
                auto b = Rehash();  /* grow table */
                if (b < 0) {
                    return;  /* grow failed */
                }
                return Insert(key, value);  /* insert key into grown table */
            }
            auto othern = MainPosition(m_nodes[mp].key); /* other node's main position */
            if (othern != mp) {  /* is colliding node out of its main position? */
                /* yes; move colliding node into free position */
                auto pre = m_nodes[mp].pre; /* find previous */
                assert(pre != -1);
                auto next = m_nodes[mp].next; /* find next */
                m_nodes[pre].next = f; /* rechain to point to 'f' */
                if (next != -1) {
                    m_nodes[next].pre = f;
                }
                m_nodes[f] = m_nodes[mp]; /* copy colliding node into free pos. */
                m_bitmap->Set(f);
                m_nodes[mp].Clear(); /* now 'mp' is free */
            } else { /* colliding node is in its own main position */
                /* new node will go into free position */
                auto next = m_nodes[mp].next;
                if (next != -1) {
                    m_nodes[f].next = next; /* chain new position */
                    m_nodes[next].pre = f;
                }
                m_nodes[mp].next = f;
                m_nodes[f].pre = mp;
                mp = f;
            }
        } else {
            // remove from free list
            auto pre = m_nodes[mp].pre;
            auto next = m_nodes[mp].next;
            if (pre != -1) {
                m_nodes[pre].next = next;
            }
            if (next != -1) {
                m_nodes[next].pre = pre;
            }
            if (m_free == mp) {
                m_free = next;
            }
            m_nodes[mp].Clear();
        }
        m_nodes[mp].key = key;
        m_nodes[mp].value = value;
        m_bitmap->Set(mp);
    }

    bool Find(const Key &key, Value &value) {
        auto mp = MainPosition(key);
        while (mp != -1) {
            if (m_nodes[mp].key == key) {
                value = m_nodes[mp].value;
                return true;
            }
            mp = m_nodes[mp].next;
        }
        return false;
    }

    bool Erase(const Key &key) {
        auto mp = MainPosition(key);
        auto cur = mp;
        while (cur != -1) {
            if (Equal()(m_nodes[cur].key, key)) {
                auto clear_pos = cur;

                // remove node from chain
                auto pre = m_nodes[cur].pre;
                auto next = m_nodes[cur].next;
                if (pre != -1) {
                    m_nodes[pre].next = next;
                }
                if (next != -1) {
                    m_nodes[next].pre = pre;
                    // if is head, we should move next to main position
                    if (mp == cur) {
                        auto next_next = m_nodes[next].next;
                        m_nodes[cur] = m_nodes[next];
                        m_nodes[cur].pre = -1;
                        if (next_next != -1) {
                            m_nodes[next_next].pre = cur;
                        }
                        clear_pos = next;
                    }
                }

                m_bitmap->Clear(clear_pos);

                // add to free list
                m_nodes[clear_pos].Clear();
                m_nodes[clear_pos].next = m_free;
                m_nodes[clear_pos].pre = -1;
                if (m_free != -1) {
                    m_nodes[m_free].pre = clear_pos;
                }
                m_free = clear_pos;
                return true;
            }
            cur = m_nodes[cur].next;
        }
        return false;
    }

    int Capacity() const {
        return m_size;
    }

    int Size() const {
        int ret = 0;
        for (int i = 0; i < m_size; i++) {
            if (Valid(i)) {
                ret++;
            }
        }
        return ret;
    }

    int MainPositionSize() const {
        int ret = 0;
        for (int i = 0; i < m_size; i++) {
            if (Valid(i) && MainPosition(m_nodes[i].key) == i) {
                ret++;
            }
        }
        return ret;
    }

    std::map<int, int> ChainStatus() const {
        std::map<int, int> ret;
        for (int i = 0; i < m_size; i++) {
            if (Valid(i)) {
                // check is head?
                bool is_head = true;
                for (int j = 0; j < m_size; j++) {
                    if (m_nodes[j].next == i) {
                        is_head = false;
                        break;
                    }
                }

                if (is_head) {
                    int count = 0;
                    int next = i;
                    while (next != -1) {
                        count++;
                        next = m_nodes[next].next;
                    }
                    ret[count]++;
                }
            }
        }
        return ret;
    }

    std::string Dump() {
        std::stringstream ss;
        for (int i = 0; i < m_size; i++) {
            if (Valid(i)) {
                ss << "i:" << i << " key:" << m_nodes[i].key << " value:" << m_nodes[i].value << " pre:"
                   << m_nodes[i].pre << " next:" << m_nodes[i].next << " mp:" << bool(MainPosition(m_nodes[i].key) == i)
                   << std::endl;
            }
        }
        return ss.str();
    }

private:
    struct Node {
        Key key;
        Value value;
        int pre = -1;
        int next = -1;

        void Clear() {
            key = Key();
            value = Value();
            pre = -1;
            next = -1;
        }
    };

    int MainPosition(const Key &key) const {
        return Hash()(key) % m_size;
    }

    bool Valid(int index) const {
        return m_bitmap->Test(index);
    }

    int GetFreePosition() {
        if (m_free == -1) {
            return -1;
        }
        auto ret = m_free;
        auto next = m_nodes[m_free].next;
        if (next != -1) {
            m_nodes[next].pre = -1;
        }
        m_free = next;
        m_nodes[ret].Clear();
        return ret;
    }

    int FindNextCapacity(int n) {
        int size = sizeof(primes) / sizeof(primes[0]);
        int left = 0;
        int right = size - 1;
        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (primes[mid] == n) {
                return primes[mid];
            } else if (primes[mid] < n) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
        return (left < size) ? primes[left] : -1;  // 如果n大于数组中的所有质数，返回-1
    }

    void InitFreeList() {
        for (int i = 0; i < m_size - 1; i++) {
            m_nodes[i + 1].pre = i;
            m_nodes[i].next = i + 1;
        }
        m_nodes[0].pre = -1;
        m_nodes[m_size - 1].next = -1;
        m_free = 0;
    }

    int Rehash() {
        auto size = FindNextCapacity(Capacity() + 1);
        if (size == -1) {
            return -1;
        }
        auto oldnodes = m_nodes;
        auto oldsize = m_size;
        auto oldbitmap = m_bitmap;
        m_nodes = new Node[size];
        m_size = size;
        InitFreeList();
        m_bitmap = new BitMap(size);
        for (int i = 0; i < oldsize; i++) {
            Insert(oldnodes[i].key, oldnodes[i].value);
        }
        delete oldbitmap;
        delete[] oldnodes;
        return 0;
    }

private:
    int m_free = 0; // free list head
    int m_size = 0;
    Node *m_nodes;
    BitMap *m_bitmap;
};

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

        std::unordered_map<StringPtr, MapValue32, StringPtrHash, StringPtrEqual> *m_string_32;
        std::unordered_map<StringPtr, MapValue64, StringPtrHash, StringPtrEqual> *m_string_64;
    };

    static_assert(sizeof(std::unordered_map<int64_t, MapValue64>) == 56,
                  "std::unordered_map<int64_t, MapValue64> size must be 24");

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

    MapValue32 Get32byString(StringPtr key, bool &is_nil) {
        auto it = m_map.m_string_32->find(key);
        if (it != m_map.m_string_32->end()) {
            is_nil = false;
            return it->second;
        }
        is_nil = true;
        return MapValue32();
    }

    MapValue64 Get64byString(StringPtr key, bool &is_nil) {
        auto it = m_map.m_string_64->find(key);
        if (it != m_map.m_string_64->end()) {
            is_nil = false;
            return it->second;
        }
        is_nil = true;
        return MapValue64();
    }

    void Set32by32(int32_t key, MapValue32 value) {
        if (!m_map.m_32_32) {
            m_map.m_32_32 = new std::unordered_map<int32_t, MapValue32>();
        }
        (*m_map.m_32_32)[key] = value;
    }

    void Set64by32(int32_t key, MapValue64 value) {
        if (!m_map.m_32_64) {
            m_map.m_32_64 = new std::unordered_map<int32_t, MapValue64>();
        }
        (*m_map.m_32_64)[key] = value;
    }

    void Set32by64(int64_t key, MapValue32 value) {
        if (!m_map.m_64_32) {
            m_map.m_64_32 = new std::unordered_map<int64_t, MapValue32>();
        }
        (*m_map.m_64_32)[key] = value;
    }

    void Set64by64(int64_t key, MapValue64 value) {
        if (!m_map.m_64_64) {
            m_map.m_64_64 = new std::unordered_map<int64_t, MapValue64>();
        }
        (*m_map.m_64_64)[key] = value;
    }

    void Set32byString(StringPtr key, MapValue32 value) {
        if (!m_map.m_string_32) {
            m_map.m_string_32 = new std::unordered_map<StringPtr, MapValue32, StringPtrHash, StringPtrEqual>();
        }
        (*m_map.m_string_32)[key] = value;
    }

    void Set64byString(StringPtr key, MapValue64 value) {
        if (!m_map.m_string_64) {
            m_map.m_string_64 = new std::unordered_map<StringPtr, MapValue64, StringPtrHash, StringPtrEqual>();
        }
        (*m_map.m_string_64)[key] = value;
    }

    void Remove32by32(int32_t key) {
        if (m_map.m_32_32) {
            m_map.m_32_32->erase(key);
        }
    }

    void Remove64by32(int32_t key) {
        if (m_map.m_32_64) {
            m_map.m_32_64->erase(key);
        }
    }

    void Remove32by64(int64_t key) {
        if (m_map.m_64_32) {
            m_map.m_64_32->erase(key);
        }
    }

    void Remove64by64(int64_t key) {
        if (m_map.m_64_64) {
            m_map.m_64_64->erase(key);
        }
    }

    void Remove32byString(StringPtr key) {
        if (m_map.m_string_32) {
            m_map.m_string_32->erase(key);
        }
    }

    void Remove64byString(StringPtr key) {
        if (m_map.m_string_64) {
            m_map.m_string_64->erase(key);
        }
    }

private:
    void ReleaseAllSharedObj();

    void ReleaseStrBy32() {
        for (auto &it: *m_map.m_32_64) {
            auto v = it.second;
            v.m_string->Release();
        }
    }

    void ReleaseStrBy64() {
        for (auto &it: *m_map.m_64_64) {
            auto v = it.second;
            v.m_string->Release();
        }
    }

    void ReleaseStrByString() {
        for (auto &it: *m_map.m_string_64) {
            auto v = it.second;
            v.m_string->Release();
        }
    }

    void ReleaseObjBy32() {
        for (auto &it: *m_map.m_32_64) {
            auto v = it.second;
            v.m_obj->Release();
        }
    }

    void ReleaseObjBy64() {
        for (auto &it: *m_map.m_64_64) {
            auto v = it.second;
            v.m_obj->Release();
        }
    }

    void ReleaseObjByString() {
        for (auto &it: *m_map.m_string_64) {
            auto v = it.second;
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
