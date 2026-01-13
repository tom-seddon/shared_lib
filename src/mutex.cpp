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

#include <shared/enum_def.h>
#include <shared/mutex.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MutexMetadataImpl : public MutexMetadata {
    std::mutex mutex;

    // num_try_locks and ever_locked values are both bogus. The caller's copy is filled out correctly on demand by GetDetails.
    MutexStats stats;
    std::atomic<bool> reset{false};

    std::atomic<bool> ever_locked{false};

    // A try_lock needs accounting for even if the mutex ends up not taken.
    std::atomic<uint64_t> num_try_locks{0};

    std::atomic<uint8_t> interesting_events{0};

    mutable std::shared_mutex name_mutex;
    std::string name;

    ~MutexMetadataImpl();

    void RequestReset() override;
    void GetDetails(MutexDetails *details) const override;
    uint8_t GetInterestingEvents() const override;
    void SetInterestingEvents(uint8_t events) override;

    void Reset();
};

struct MutexFullMetadata : public std::enable_shared_from_this<MutexFullMetadata> {
    MutexFullMetadata *next = nullptr, *prev = nullptr;
    Mutex *mutex = nullptr;
    MutexMetadataImpl meta;

    // this is just here to grab an appropriate extra ref to the
    // metadata list mutex. If a global mutex gets created before the
    // metadata mutex shared_ptr, that mutex would otherwise be
    // destroyed after it...
    std::shared_ptr<std::mutex> metadata_list_mutex;
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
//////////////////////////////////////////////////////////////////////////

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
    details->stats = this->stats;

    details->stats.num_try_locks = this->num_try_locks.load(std::memory_order_acquire);

    details->stats.ever_locked = this->ever_locked.load(std::memory_order_acquire);

    {
        uint64_t start_ticks = GetCurrentTickCount();

        this->name_mutex.lock_shared();
        details->name = this->name;
        this->name_mutex.unlock_shared();

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
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Mutex::Mutex()
    : m_metadata(std::make_shared<MutexFullMetadata>()) {
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

Mutex::~Mutex() {
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

void Mutex::SetName(std::string name) {
    uint64_t start_ticks = GetCurrentTickCount();

    LockGuard<std::shared_mutex> lock(m_meta->name_mutex);
    m_meta->name = std::move(name);

    g_mutex_name_overhead_ticks.fetch_add(GetCurrentTickCount() - start_ticks, std::memory_order_acq_rel);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexMetadata *Mutex::GetMutableMetadata() {
    return m_meta;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const MutexMetadata *Mutex::GetMetadata() const {
    return m_meta;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Mutex::lock() {
    const bool assume_free_uncontended_locks = g_assume_free_uncontended_locks.load(std::memory_order_acquire);

    uint64_t lock_start_ticks = 0; //spurious initialization to inhibit warning
    if (!assume_free_uncontended_locks) {
        lock_start_ticks = GetCurrentTickCount();
    }

    uint64_t lock_wait_ticks = 0;

    uint8_t interesting_events = m_meta->interesting_events.load(std::memory_order_relaxed);

    if (m_meta->mutex.try_lock()) {
        interesting_events &= (uint8_t)~MutexInterestingEvent_ContendedLock;
    } else {
        if (assume_free_uncontended_locks) {
            lock_start_ticks = GetCurrentTickCount();
        }

        m_meta->mutex.lock();
        ++m_meta->stats.num_contended_locks;

        if (assume_free_uncontended_locks) {
            lock_wait_ticks += GetCurrentTickCount() - lock_start_ticks;
        }
    }

    ++m_meta->stats.num_locks;
    if (!assume_free_uncontended_locks) {
        lock_wait_ticks += GetCurrentTickCount() - lock_start_ticks;
    }

    if (m_meta->reset.exchange(false)) {
        m_meta->Reset();
    }

    m_meta->ever_locked.store(true, std::memory_order_release);
    m_meta->stats.total_lock_wait_ticks += lock_wait_ticks;

    if (lock_wait_ticks < m_meta->stats.min_lock_wait_ticks) {
        m_meta->stats.min_lock_wait_ticks = lock_wait_ticks;
    }

    if (lock_wait_ticks > m_meta->stats.max_lock_wait_ticks) {
        m_meta->stats.max_lock_wait_ticks = lock_wait_ticks;
    }

    if (interesting_events != 0) {
        this->OnInterestingEvents(interesting_events, m_meta);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Mutex::try_lock() {
    bool succeeded = m_meta->mutex.try_lock();

    if (succeeded) {
        ++m_meta->stats.num_successful_try_locks;

        // no need so set ever_locked - the successful lock that's blocking this
        // one already did it
    }

    ++m_meta->num_try_locks;

    if (m_meta->reset.exchange(false)) {
        m_meta->Reset();
    }

    return succeeded;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Mutex::unlock() {
    m_meta->mutex.unlock();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t Mutex::GetMetadataChangeCounter() {
    EnsureMutexMetadataListInitialised();

    LockGuard<std::mutex> lock(*g_mutex_metadata_list_mutex);

    return g_mutex_metadata_list_change_counter;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<MutexMetadata>> Mutex::GetAllMetadata() {
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

uint64_t Mutex::GetNameOverheadTicks() {
    return g_mutex_name_overhead_ticks.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Mutex::GetAssumeFreeUncontendedLocks() {
    return g_assume_free_uncontended_locks.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Mutex::SetAssumeFreeUncontendedLocks(bool assume_free_uncontended_locks) {
    g_assume_free_uncontended_locks.store(assume_free_uncontended_locks, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This exists purely as somewhere to put a breakpoint.
void Mutex::OnInterestingEvents(uint8_t interesting_events, MutexMetadataImpl *meta) {
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

MutexNameSetter::MutexNameSetter(Mutex *mutex, const char *name) {
    MUTEX_SET_NAME(*mutex, name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
