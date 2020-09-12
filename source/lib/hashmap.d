module lib.hasmap;

import lib.alloc;
import lib.list;

struct HashMap(K, V) {
    struct Entry {
        K key;
        V value;
    }

    List!(Entry) *entries;

    size_t buckets;
    ulong function(K str) hash;
    bool function(K k1, K k2) cmp;

    this(size_t num_buckets, ulong function(K str) h, bool function(K k1, K k2) c) {
        buckets = num_buckets;
        entries = newArray!(List!(Entry))(num_buckets);
        hash = h;
        cmp = c;
    }

    void insert(K key, V value) {
        size_t index = hash(key) % buckets;
        entries[index].push(Entry(key, value));
    }

    bool get(K key, V* output) {
        size_t index = hash(key) % buckets;
        auto arr = entries[index];
        size_t len = arr.length();
        foreach (ulong i; 0..len) {
            if(cmp(key, arr[i].key)) {
                *output = arr[i].value;
                return true;
            }
        }
        return false;
    }

    void remove(K key) {
        size_t index = hash(key) % buckets;
        auto arr = entries[index];
        size_t len = arr.length();
        foreach (ulong i; 0..len) {
            if(cmp(key, arr[i].key)) {
                arr.erase(i);
                return;
            }
        }
    }
}
