#pragma once
#ifdef __APPLE__
#include <sys/types.h>
#endif

#include <uthash.h>
#include <utarray.h>
#include <assert.h>
#include "cache.hpp"


#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(__movsb)
#else
#include <math.h>
#include <pthread.h>
#endif

#ifdef __APPLE__
#include "osx.common/Logging.h"
#endif

/* convenience forms of HASH_FIND/HASH_ADD/HASH_DEL */
#define HASH_FIND_INT64(head,findint,out)							\
    HASH_FIND(hh,head,findint,sizeof(uint64_t),out)
#define HASH_ADD_INT64(head,intfield,add)							\
    HASH_ADD(hh,head,intfield,sizeof(uint64_t),add)
#define HASH_REPLACE_INT64(head,intfield,add,replaced)				\
    HASH_REPLACE(hh,head,intfield,sizeof(uint64_t),add,replaced)


#if (defined _WIN32) || (defined _WIN64)
#define ENTER_SPIN_LOCK(lock) while (InterlockedCompareExchange(&lock, 1, 0)) {}
#define EXIT_SPIN_LOCK(lock) InterlockedExchange(&lock, 0);
#else
#define ENTER_SPIN_LOCK(lock) pthread_mutex_lock(&lock);
#define EXIT_SPIN_LOCK(lock) pthread_mutex_unlock(&lock);
#endif


template <typename T>
struct ValueType {
    int idx;
    T data;
};

template <typename T>
struct CacheEntry {
    uint64_t key;
    struct ValueType<T> value;
    UT_hash_handle hh;
};

template <typename T>
struct CacheContext {
#if defined(_WIN32) || defined (_WIN64)
    volatile LONG lock;
#else
    pthread_mutex_t lock;
#endif
    bool init;
    size_t max_size;
    struct CacheEntry<T> *cache;
    struct CacheEntry<T> *store;
    UT_array *tracker;
};

template <typename T>
struct CacheEntry<T>* entry_alloc(struct CacheContext<T> *ctx)
{
    if (!ctx || !ctx->store || !ctx->tracker)
        return NULL;
    
    int *pidx = (int*)utarray_back(ctx->tracker);
    if (pidx) {
        uint32_t idx = *pidx;
        utarray_pop_back(ctx->tracker);
        assert(idx < ctx->max_size);
        if (idx < ctx->max_size) {
            struct CacheEntry<T>* entry = &ctx->store[idx];
            assert(entry->value.idx == -1);
            entry->value.idx = idx;
            memset(&entry->value.data, 0, sizeof(entry->value.data));
            return entry;
        }
    }
    assert("entry_alloc() failed!");
    return NULL;
}

template <typename T>
void entry_free(struct CacheContext<T> *ctx, struct CacheEntry<T>* entry)
{
    if (!ctx || !ctx->store || !ctx->tracker || !entry)
        return;
    
    assert(!ctx->init || (entry->value.idx >= 0 && entry->value.idx < ctx->max_size));
    
    utarray_push_back(ctx->tracker, &entry->value.idx);
    memset(entry, 0, sizeof(*entry));
    entry->value.idx = -1;
}

// Creates the context.
template <typename T>
struct CacheContext<T>* cache_create(size_t max_size)
{
    struct CacheContext<T>* ctx = (struct CacheContext<T>*)malloc(sizeof(CacheContext<T>));
    if (!ctx)
        return NULL;
    
    memset(ctx, 0, sizeof(CacheContext<T>));
    
    // Allocate the mem tracker & unresolved array
    utarray_new(ctx->tracker, &ut_int_icd);
    if (!ctx->tracker)
    {
        free(ctx);
        return NULL;
    }
    
    // Allocate memory for the cache
    ctx->store = (CacheEntry<T> *)calloc(max_size, sizeof(*ctx->store));
    if (!ctx->store)
    {
        utarray_free(ctx->tracker);
        free(ctx);
        return NULL;
    }
    else {
        
        // "Free" all entries
        for (int i = 0; i < max_size; i++) {
            struct CacheEntry<T>* entry = &ctx->store[i];
            entry->value.idx = i;
            entry_free(ctx, entry);
        }
        
        ctx->init = true;
        ctx->max_size = max_size;
        CLogDebug("cache created (capcaity=%d)", ctx->max_size);
        
        return ctx;
    }
}

// Destroys the context.
template <typename T>
void cache_destroy(struct CacheContext<T> *ctx)
{
    if (!ctx || !ctx->init) return;
    
    struct CacheEntry<T>* cur, *tmp;
    HASH_ITER(hh, ctx->cache, cur, tmp)
    {
        HASH_DEL(ctx->cache, cur);
        entry_free(ctx, cur);
    }
    
    if (ctx->store) {
        free(ctx->store);
        ctx->store = NULL;
    }
    
    if (ctx->tracker) {
        utarray_free(ctx->tracker);
        ctx->tracker = NULL;
    }
    
    ctx->init = false;
    CLogDebug("cache destoryed");
}

template <typename T>
void cache_clear(struct CacheContext<T> *ctx)
{
    if (!ctx || !ctx->init) return;
    
    ENTER_SPIN_LOCK(ctx->lock);
    struct CacheEntry<T>* cur, *tmp;
    HASH_ITER(hh, ctx->cache, cur, tmp)
    {
        HASH_DEL(ctx->cache, cur);
        entry_free(ctx, cur);
    }
    EXIT_SPIN_LOCK(ctx->lock);
}

template <typename T>
bool cache_is_empty(struct CacheContext<T> *ctx)
{
    if (!ctx || !ctx->init) return 0;
    return (HASH_COUNT(ctx->cache) == 0);
}

template <typename T>
bool cache_is_full(struct CacheContext<T> *ctx)
{
    if (!ctx || !ctx->init) return false;
    if (HASH_COUNT(ctx->cache) >= ctx->max_size)
        return true;
    return false;
}

template <typename T>
unsigned int cache_size(struct CacheContext<T> *ctx)
{
    if (!ctx || !ctx->init) return 0;
    return HASH_COUNT(ctx->cache);
}

template <typename T>
bool cache_delete(struct CacheContext<T> *ctx, uint64_t key)
{
    if (!ctx || !ctx->init) return false;
    bool bRet = false;
    struct CacheEntry<T> *entry = 0;
    ENTER_SPIN_LOCK(ctx->lock);
    HASH_FIND_INT64(ctx->cache, &key, entry);
    if (entry) {
        assert(key == entry->key);
        CLogTrace("cache delete <0x%llx>", entry->key);
        HASH_DEL(ctx->cache, entry);
        entry_free(ctx, entry);
        bRet = true;
    }
    EXIT_SPIN_LOCK(ctx->lock);
    return bRet;
}

template <typename T>
bool cache_delete(struct CacheContext<T> *ctx, struct CacheEntry<T> *entry)
{
    if (!ctx || !ctx->init) return false;
    bool bRet = false;
    ENTER_SPIN_LOCK(ctx->lock);
    if (entry) {
        HASH_DEL(ctx->cache, entry);
        entry_free(ctx, entry);
        bRet = true;
    }
    EXIT_SPIN_LOCK(ctx->lock);
    return bRet;
}

template <typename T>
struct CacheEntry<T> *cache_find(struct CacheContext<T> *ctx, uint64_t key)
{
    if (!ctx || !ctx->init) return NULL;
    struct CacheEntry<T> *entry = 0;
    ENTER_SPIN_LOCK(ctx->lock);
    HASH_FIND_INT64(ctx->cache, &key, entry);
    if (entry) {
        assert(key == entry->key);
    }
    EXIT_SPIN_LOCK(ctx->lock);
    return entry;
}

template <typename T>
struct CacheEntry<T> *cache_find_and_front(struct CacheContext<T> *ctx, uint64_t key)
{
    if (!ctx || !ctx->init) return NULL;
    struct CacheEntry<T> *entry = 0;
    ENTER_SPIN_LOCK(ctx->lock);
    HASH_FIND_INT64(ctx->cache, &key, entry);
    if (entry) {
        assert(key == entry->key);
        // remove it (so the subsequent add will throw it on the front of the list)
        HASH_DEL(ctx->cache, entry);
        HASH_ADD_INT64(ctx->cache, key, entry);
    }
    EXIT_SPIN_LOCK(ctx->lock);
    return entry;
}

template <typename T>
struct CacheEntry<T> *cache_insert(struct CacheContext<T> *ctx, uint64_t key, T *value)
{
    if (!ctx || !ctx->init) return NULL;
    struct CacheEntry<T> *entry = cache_find<T>(ctx, key);
    
    if (!entry) {
        
        ENTER_SPIN_LOCK(ctx->lock);
        
        // we are FULL
        if (HASH_COUNT(ctx->cache) >= ctx->max_size) {
            
            EXIT_SPIN_LOCK(ctx->lock);
            return NULL;
        }
        
        entry = entry_alloc<T>(ctx);
        if (entry) {
            entry->key = key;
            HASH_ADD_INT64(ctx->cache, key, entry);
        }
        
        EXIT_SPIN_LOCK(ctx->lock);
    }
    
    // entry_alloc() should always succeed
    assert(entry != NULL);
    
    if (!entry)
        return NULL;
    
    assert(key == entry->key);
    if (value) memcpy(&entry->value.data, value, sizeof(entry->value.data));
    
    return entry;
}

template <typename T>
struct CacheEntry<T> *cache_insert_and_evict(struct CacheContext<T> *ctx, uint64_t key, T *value)
{
    if (!ctx || !ctx->init) return NULL;
    struct CacheEntry<T> *entry = cache_find_and_front<T>(ctx, key);
    
    if (!entry) {
        
        ENTER_SPIN_LOCK(ctx->lock);
        
        // prune the cache to MAX_CACHE_SIZE
        if (HASH_COUNT(ctx->cache) >= ctx->max_size) {
            // prune the first entry (loop is based on insertion order so this deletes the oldest item)
            // save entry beforehand as since we delete the first item g_cache will change to the next
            entry = ctx->cache;
            CLogTrace("CACHE_CAPACITY(%d) reached, evicting <0x%llx>",
                      HASH_COUNT(ctx->cache), entry->key);
            HASH_DEL(ctx->cache, entry);
            entry_free(ctx, entry);
        }
        
        entry = entry_alloc<T>(ctx);
        if (entry) {
            entry->key = key;
            HASH_ADD_INT64(ctx->cache, key, entry);
        }
        
        EXIT_SPIN_LOCK(ctx->lock);
    }
    
    // entry_alloc() should always succeed
    assert(entry != NULL);
    
    if (!entry)
        return NULL;
    
    assert(key == entry->key);
    if (value) memcpy(&entry->value.data, value, sizeof(entry->value.data));
    
    return entry;
}

template <typename T>
void cache_iter(struct CacheContext<T> *ctx, void(*cb)(struct CacheEntry<T>*), bool lock)
{
    if (!cb || !ctx || !ctx->init) return;
    if (lock) ENTER_SPIN_LOCK(ctx->lock);
    struct CacheEntry<T>* cur, *tmp;
    HASH_ITER(hh, ctx->cache, cur, tmp) {
        cb(cur);
    }
    if (lock) EXIT_SPIN_LOCK(ctx->lock);
}

template <typename T>
struct Cache {
    size_t  __size;
    struct CacheContext<T>*  __ctx;
    
    struct CacheContext<T>* create(size_t max_size) {
        __ctx = cache_create<T>(max_size);
        __size = max_size;
        return __ctx;
    }
    void destroy() { cache_destroy(__ctx); __ctx = 0; };
    
    bool empty() { return cache_is_empty(__ctx); }
    bool full() { return cache_is_full(__ctx); }
    bool limit() { return cache_size(__ctx) >= __size; }
    
    void clear() { cache_clear(__ctx); }
    
    
    size_t size() { return cache_size(__ctx); }
    void resize(size_t size) { if (__ctx) { __size = size > __ctx->max_size ? __ctx->max_size : size; } }
    
    struct CacheEntry<T> *insert(uint64_t key, T *value) { return cache_insert(__ctx, key, value); }
    struct CacheEntry<T> *insert_and_evict(uint64_t key, T *value) { return cache_insert_and_evict(__ctx, key, value); }
    
    bool remove(uint64_t key) { return cache_delete(__ctx, key); }
    bool remove(struct CacheEntry<T> *entry) { return cache_delete(__ctx, entry); }
    struct CacheEntry<T> *find(uint64_t key) { return cache_find(__ctx, key); }
    struct CacheEntry<T> *find_and_front(uint64_t key) { return cache_find_and_front(__ctx, key); }
    
    void iter_lock(void(*cb)(struct CacheEntry<T>*)) { cache_iter(__ctx, cb, 1); }
    void iter_nolock(void(*cb)(struct CacheEntry<T>*)) { cache_iter(__ctx, cb, 0); }
    
    void lock() { if (__ctx) ENTER_SPIN_LOCK(__ctx->lock); };
    void unlock() { if (__ctx) EXIT_SPIN_LOCK(__ctx->lock); };
};
