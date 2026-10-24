#include <shared/system.h>
#include <shared/mutex.h>

#if MUTEX_DEBUGGING

#include <shared/debug.h>
#include <vector>
#include <set>
#include <atomic>
#include <mutex>
#include <string.h>
#include <shared_mutex>
#include <algorithm>
#include <variant>

#include <shared/enum_def.h>
#include <shared/mutex.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MutexMetadataImpl : public MutexMetadata {
    std::variant<std::mutex, std::shared_mutex> actual_mutex;
    std::mutex *mutex = nullptr;
    std::shared_mutex *shared_mutex = nullptr;

    // num_try_locks and ever_locked values are both bogus. The caller's copy is filled out correctly on demand by GetDetails.
    MutexStats stats;
    std::atomic<bool> reset{false};

    std::atomic<bool> ever_lock{false};
    std::atomic<bool> ever_lock_shared{false};

    // A try_lock needs accounting for even if the mutex ends up not taken.
    std::atomic<uint64_t> num_try_locks{0};
    std::atomic<uint64_t> num_try_lock_shareds{0};

    std::atomic<uint8_t> interesting_events{0};

    mutable std::shared_mutex name_mutex;
    std::string name;

    template <class T>
    MutexMetadataImpl(const T *);
    ~MutexMetadataImpl();

    void RequestReset() override;
    void GetDetails(MutexDetails *details) const override;
    uint8_t GetInterestingEvents() const override;
    void SetInterestingEvents(uint8_t events) override;

    void Reset();
};

struct MutexFullMetadata : public std::enable_shared_from_this<MutexFullMetadata> {
    MutexFullMetadata *next = nullptr, *prev = nullptr;
    MutexBase *mutex = nullptr;
    MutexMetadataImpl meta;

    // this is just here to grab an appropriate extra ref to the
    // metadata list mutex. If a global mutex gets created before the
    // metadata mutex shared_ptr, that mutex would otherwise be
    // destroyed after it...
    std::shared_ptr<std::mutex> metadata_list_mutex;

    template <class T>
    MutexFullMetadata(const T *);
};

static std::once_flag g_mutex_metadata_list_mutex_initialise_once_flag;
static std::shared_ptr<std::mutex> g_mutex_metadata_list_mutex; //shared_ptr() is constexpr
static std::atomic<uint64_t> g_mutex_name_overhead_ticks{0};
static std::atomic<bool> g_assume_free_uncontended_locks{false};

// controlled by g_mutex_metadata_list_mutex
static std::vector<std::shared_ptr<MutexMetadata>> g_mutex_metadata_list;

// change counter last time g_mutex_metadata_list was regenerated. controlled by g_mutex_metadata_list_mutex.
static uint64_t g_mutex_metadata_list_last_change_counter = 0;

// current change counter. controlled by g_mutex_metadata_list_mutex
static uint64_t g_mutex_metadata_list_change_counter = 1;

static MutexFullMetadata *g_mutex_metadata_head;

static void InitMutexMetadataListMutex() {
    ASSERT(!g_mutex_metadata_list_mutex);
    g_mutex_metadata_list_mutex = std::make_shared<std::mutex>();
}

static void EnsureMutexMetadataListInitialised() {
    std::call_once(g_mutex_metadata_list_mutex_initialise_once_flag, &InitMutexMetadataListMutex);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////ko

static void CheckLockedMetadataList() {
    if (MutexFullMetadata *m = g_mutex_metadata_head) {
        do {
            ASSERT(m->prev->next == m);
            ASSERT(m->next->prev == m);

            m = m->next;
        } while (m != g_mutex_metadata_head);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void InvalidateLockedMetadataList() {
    ++g_mutex_metadata_list_change_counter;

    // don't let any control blocks hang around unnecessarily.
    g_mutex_metadata_list.clear();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexStats::MutexStats()
    : start_ticks(GetCurrentTickCount()) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexMetadata::MutexMetadata() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexMetadata::~MutexMetadata() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
MutexMetadataImpl::MutexMetadataImpl(const T *)
    : actual_mutex(std::in_place_type_t<T>()) {
    this->mutex = std::get_if<std::mutex>(&this->actual_mutex);
    this->shared_mutex = std::get_if<std::shared_mutex>(&this->actual_mutex);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexMetadataImpl::~MutexMetadataImpl() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MutexMetadataImpl::RequestReset() {
    // TODO: a little ill-advised, as it's not protected by any kind of lock! But it'll just about hang together.
    this->stats = {};
    this->reset.store(true, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MutexMetadataImpl::GetDetails(MutexDetails *details) const {
    details->is_shared_mutex = !!this->shared_mutex;

    details->stats = this->stats;

    details->stats.lock_stats.num_try_locks = this->num_try_locks.load(std::memory_order_acquire);
    details->stats.lock_shared_stats.num_try_locks = this->num_try_lock_shareds.load(std::memory_order_acquire);

    details->stats.lock_stats.ever_locked = this->ever_lock.load(std::memory_order_acquire);
    details->stats.lock_shared_stats.ever_locked = this->ever_lock_shared.load(std::memory_order_acquire);

    {
        uint64_t start_ticks = GetCurrentTickCount();

        {
            std::shared_lock<std::shared_mutex> lock(this->name_mutex);
            details->name = this->name;
        }

        g_mutex_name_overhead_ticks.fetch_add(GetCurrentTickCount() - start_ticks, std::memory_order_acq_rel);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t MutexMetadataImpl::GetInterestingEvents() const {
    return this->interesting_events;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MutexMetadataImpl::SetInterestingEvents(uint8_t events) {
    // don't generate an unnecessary write
    if (events != this->interesting_events.load(std::memory_order_relaxed)) {
        this->interesting_events.store(events, std::memory_order_relaxed);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MutexMetadataImpl::Reset() {
    this->stats = {};
    this->num_try_locks.store(0);
    this->num_try_lock_shareds.store(0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
MutexFullMetadata::MutexFullMetadata(const T *dummy)
    : meta(dummy) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexBase::MutexBase(std::shared_ptr<MutexFullMetadata> metadata)
    : m_metadata(std::move(metadata)) {
    EnsureMutexMetadataListInitialised();

    m_meta = &m_metadata.get()->meta;

    m_metadata->next = m_metadata.get();
    m_metadata->prev = m_metadata.get();
    m_metadata->mutex = this;
    m_metadata->metadata_list_mutex = g_mutex_metadata_list_mutex;

    LockGuard<std::mutex> lock(*g_mutex_metadata_list_mutex);

    if (!g_mutex_metadata_head) {
        g_mutex_metadata_head = m_metadata.get();
    } else {
        m_metadata->prev = g_mutex_metadata_head->prev;
        m_metadata->next = g_mutex_metadata_head;

        m_metadata->prev->next = m_metadata.get();
        m_metadata->next->prev = m_metadata.get();
    }

    InvalidateLockedMetadataList();
    CheckLockedMetadataList();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexBase::~MutexBase() {
    {
        LockGuard<std::mutex> lock(*m_metadata->metadata_list_mutex);

        if (m_metadata.get() == g_mutex_metadata_head) {
            g_mutex_metadata_head = m_metadata->next;

            if (m_metadata.get() == g_mutex_metadata_head) {
                // this was the last one in the list...
                g_mutex_metadata_head = nullptr;
            }
        }

        m_metadata->next->prev = m_metadata->prev;
        m_metadata->prev->next = m_metadata->next;

        m_metadata->mutex = nullptr;

        InvalidateLockedMetadataList();
        CheckLockedMetadataList();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MutexBase::SetName(std::string name) {
    uint64_t start_ticks = GetCurrentTickCount();

    LockGuard<std::shared_mutex> lock(m_meta->name_mutex);
    m_meta->name = std::move(name);

    g_mutex_name_overhead_ticks.fetch_add(GetCurrentTickCount() - start_ticks, std::memory_order_acq_rel);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This exists purely as somewhere to put a breakpoint.
static void OnInterestingEvents(uint8_t interesting_events, MutexMetadataImpl *meta) {
    (void)meta;

    if (interesting_events & MutexInterestingEvent_Lock) {
#ifdef _MSC_VER
        __nop();
#elif __APPLE__
        int x = 0;
        (void)x;
#endif
    }

    if (interesting_events & MutexInterestingEvent_ContendedLock) {
#ifdef _MSC_VER
        __nop();
#elif __APPLE__
        int x = 0;
        (void)x;
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class MutexType>
static inline void Lock(MutexType *mutex, MutexMetadataImpl *meta) {
    const bool assume_free_uncontended_locks = g_assume_free_uncontended_locks.load(std::memory_order_acquire);

    uint64_t lock_start_ticks = 0; //spurious initialization to inhibit warning
    if (!assume_free_uncontended_locks) {
        lock_start_ticks = GetCurrentTickCount();
    }

    uint64_t lock_wait_ticks = 0;

    uint8_t interesting_events = meta->interesting_events.load(std::memory_order_relaxed);

    if (mutex->try_lock()) {
        interesting_events &= (uint8_t)~MutexInterestingEvent_ContendedLock;
    } else {
        if (assume_free_uncontended_locks) {
            lock_start_ticks = GetCurrentTickCount();
        }

        mutex->lock();
        ++meta->stats.lock_stats.num_contended_locks;

        if (assume_free_uncontended_locks) {
            lock_wait_ticks += GetCurrentTickCount() - lock_start_ticks;
        }
    }

    ++meta->stats.lock_stats.num_locks;
    if (!assume_free_uncontended_locks) {
        lock_wait_ticks += GetCurrentTickCount() - lock_start_ticks;
    }

    if (meta->reset.exchange(false)) {
        meta->Reset();
    }

    meta->ever_lock.store(true, std::memory_order_release);
    meta->stats.lock_stats.total_lock_wait_ticks += lock_wait_ticks;

    if (lock_wait_ticks < meta->stats.lock_stats.min_lock_wait_ticks) {
        meta->stats.lock_stats.min_lock_wait_ticks = lock_wait_ticks;
    }

    if (lock_wait_ticks > meta->stats.lock_stats.max_lock_wait_ticks) {
        meta->stats.lock_stats.max_lock_wait_ticks = lock_wait_ticks;
    }

    if (interesting_events != 0) {
        OnInterestingEvents(interesting_events, meta);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class MutexType>
static inline bool TryLock(MutexType *mutex, MutexMetadataImpl *meta) {
    bool succeeded = mutex->try_lock();

    if (succeeded) {
        ++meta->stats.lock_stats.num_successful_try_locks;

        // no need so set ever_locked - the successful lock that's blocking this
        // one already did it
    }

    ++meta->num_try_locks;

    if (meta->reset.exchange(false)) {
        meta->Reset();
    }

    return succeeded;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Mutex::Mutex()
    : MutexBase(std::make_shared<MutexFullMetadata>((std::mutex *)nullptr)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Mutex::lock() {
    Lock(m_meta->mutex, m_meta);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Mutex::try_lock() {
    return TryLock(m_meta->mutex, m_meta);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Mutex::unlock() {
    m_meta->mutex->unlock();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SharedMutex::SharedMutex()
    : MutexBase(std::make_shared<MutexFullMetadata>((std::shared_mutex *)nullptr)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SharedMutex::lock() {
    Lock(m_meta->shared_mutex, m_meta);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SharedMutex::try_lock() {
    return TryLock(m_meta->shared_mutex, m_meta);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SharedMutex::unlock() {
    m_meta->shared_mutex->unlock();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SharedMutex::lock_shared() {
    const bool assume_free_uncontended_locks = g_assume_free_uncontended_locks.load(std::memory_order_acquire);

    uint64_t lock_start_ticks = 0;
    if (!assume_free_uncontended_locks) {
        lock_start_ticks = GetCurrentTickCount();
    }

    uint64_t lock_wait_ticks = 0;

    uint8_t interesting_events = m_meta->interesting_events.load(std::memory_order_relaxed);

    if (m_meta->shared_mutex->try_lock_shared()) {
        interesting_events &= (uint8_t)~MutexInterestingEvent_ContendedLockShared;
    } else {
        if (assume_free_uncontended_locks) {
            lock_start_ticks = GetCurrentTickCount();
        }

        m_meta->shared_mutex->lock_shared();
        ++m_meta->stats.lock_shared_stats.num_contended_locks;

        if (assume_free_uncontended_locks) {
            lock_wait_ticks += GetCurrentTickCount() - lock_start_ticks;
        }
    }

    ++m_meta->stats.lock_shared_stats.num_locks;
    if (!assume_free_uncontended_locks) {
        lock_wait_ticks += GetCurrentTickCount() - lock_start_ticks;
    }

    if (m_meta->reset.exchange(false)) {
        m_meta->Reset();
    }

    m_meta->ever_lock_shared.store(true, std::memory_order_release);
    m_meta->stats.lock_shared_stats.total_lock_wait_ticks += lock_wait_ticks;

    if (lock_wait_ticks < m_meta->stats.lock_shared_stats.min_lock_wait_ticks) {
        m_meta->stats.lock_shared_stats.min_lock_wait_ticks = lock_wait_ticks;
    }

    if (lock_wait_ticks > m_meta->stats.lock_shared_stats.max_lock_wait_ticks) {
        m_meta->stats.lock_shared_stats.max_lock_wait_ticks = lock_wait_ticks;
    }

    if (interesting_events != 0) {
        OnInterestingEvents(interesting_events, m_meta);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SharedMutex::try_lock_shared() {
    bool succeeded = m_meta->shared_mutex->try_lock_shared();

    if (succeeded) {
        ++m_meta->stats.lock_shared_stats.num_successful_try_locks;

        // no need so set ever_locked - the successful lock that's blocking this
        // one already did it
    }

    ++m_meta->num_try_lock_shareds;

    if (m_meta->reset.exchange(false)) {
        m_meta->Reset();
    }

    return succeeded;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SharedMutex::unlock_shared() {
    m_meta->shared_mutex->unlock_shared();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool GetAssumeFreeUncontendedMutexLocks() {
    return g_assume_free_uncontended_locks.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetAssumeFreeUncontendedMutexLocks(bool assume_free_uncontended_locks) {
    g_assume_free_uncontended_locks.store(assume_free_uncontended_locks, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t GetMutexNameOverheadTicks() {
    return g_mutex_name_overhead_ticks.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t GetMutexMetadataChangeCounter() {
    EnsureMutexMetadataListInitialised();

    LockGuard<std::mutex> lock(*g_mutex_metadata_list_mutex);

    return g_mutex_metadata_list_change_counter;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<MutexMetadata>> GetAllMutexMetadata() {
    EnsureMutexMetadataListInitialised();

    LockGuard<std::mutex> lock(*g_mutex_metadata_list_mutex);

    if (g_mutex_metadata_list_last_change_counter != g_mutex_metadata_list_change_counter) {
        g_mutex_metadata_list.clear();

        if (MutexFullMetadata *m = g_mutex_metadata_head) {
            do {
                g_mutex_metadata_list.push_back(std::shared_ptr<MutexMetadata>(m->shared_from_this(), &m->meta));

                m = m->next;
            } while (m != g_mutex_metadata_head);
        }

        g_mutex_metadata_list_last_change_counter = g_mutex_metadata_list_change_counter;
    }

    return g_mutex_metadata_list;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexNameSetter::MutexNameSetter(Mutex *mutex, const char *name) {
    MUTEX_SET_NAME(*mutex, name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
