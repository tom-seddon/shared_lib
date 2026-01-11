#include <shared/system.h>
#include <shared/system.h>
#include <shared/metrics.h>
#include <shared/debug.h>
#include <memory>
#include <vector>
#include <string>
#include <shared/mutex.h>
#include <mutex>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static std::unique_ptr<std::vector<std::weak_ptr<MetricSet>>> g_all_metric_sets;
static std::once_flag g_metric_sets_list_once_flag;
static std::shared_ptr<Mutex> g_metric_sets_list_mutex;
static MetricSet *g_metric_sets_list_head = nullptr;
static std::shared_ptr<MetricSet> g_global_metric_set;

static void InitMetricSetsListGlobals() {
    g_metric_sets_list_mutex = std::make_shared<Mutex>();
    MUTEX_SET_NAME(*g_metric_sets_list_mutex, "MetricSets list");
}

static [[nodiscard]] UniqueLock<Mutex> LockMetricSetsList() {
    std::call_once(g_metric_sets_list_once_flag, &InitMetricSetsListGlobals);

    return UniqueLock<Mutex>(*g_metric_sets_list_mutex);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TimerDef::TimerDef(std::string name_, MetricSet *set)
    : name(std::move(name_))
    , m_set(set) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TimerDef::Reset() {
    m_total_num_ticks = 0;
    m_num_samples = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t TimerDef::GetTotalNumTicks() const {
    return m_total_num_ticks.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t TimerDef::GetNumSamples() const {
    return m_num_samples.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TimerDef::AddTicks(uint64_t num_ticks) {
    m_total_num_ticks.fetch_add(num_ticks, std::memory_order_acq_rel);
    m_num_samples.fetch_add(1, std::memory_order_acq_rel);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const TimerDef *TimerDef::GetParent() const {
    return m_parent;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<const TimerDef *> TimerDef::GetChildren() const {
    LockGuard lock(m_set->m_mutex);

    return {m_children.begin(), m_children.end()};
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MetricSet::MetricSet(std::string name) {
    this->SetName(std::move(name));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<MetricSet> MetricSet::Create(std::string name) {
    std::shared_ptr<MetricSet> set = Create2(std::move(name));

    UniqueLock<Mutex> lock = LockMetricSetsList();

    set->LinkIntoLockedList();

    return set;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MetricSet::~MetricSet() {
    {
        UniqueLock<Mutex> lock = LockMetricSetsList();

        if (m_prev) {
            m_prev->m_next = m_next;
            m_prev = nullptr;
            ASSERT(g_metric_sets_list_head != this);
        } else {
            ASSERT(g_metric_sets_list_head == this);
            g_metric_sets_list_head = m_next;
        }

        if (m_next) {
            m_next->m_prev = m_prev;
            m_next = nullptr;
        }

        CheckLockedList();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TimerDef *MetricSet::CreateTimerDef(std::string name) {
    return this->CreateTimerDef2(std::move(name), nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TimerDef *MetricSet::CreateTimerDef(std::string name, TimerDef *parent) {
    ASSERT(parent);
    ASSERT(parent->m_set == this);
    return this->CreateTimerDef2(std::move(name), parent);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MetricSet::ResetTimerDefs() {
    LockGuard<Mutex> lock(m_mutex);

    for (const std::unique_ptr<TimerDef> &def : m_all_timer_defs) {
        def->Reset();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<const TimerDef *> MetricSet::GetRootTimerDefs() const {
    LockGuard<Mutex> lock(m_mutex);

    return m_root_timer_defs;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string MetricSet::GetName() const {
    LockGuard<Mutex> lock(m_mutex);

    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MetricSet::SetName(std::string name) {
    LockGuard<Mutex> lock(m_mutex);

    m_name = std::move(name);

    MUTEX_SET_NAME(m_mutex, "MetricSet " + m_name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TimerDef *MetricSet::CreateTimerDef2(std::string name, TimerDef *parent) {
    auto def = new TimerDef(std::move(name), this);
    auto unique_def = std::unique_ptr<TimerDef>(def);

    LockGuard<Mutex> lock(m_mutex);

    if (!parent) {
        m_root_timer_defs.push_back(def);
    } else {
        def->m_parent = parent;
        parent->m_children.push_back(def);
    }

    m_all_timer_defs.push_back(std::move(unique_def));

    return def;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<MetricSet>> MetricSet::GetAll() {
    UniqueLock<Mutex> lock = LockMetricSetsList();

    //CheckList();

    std::vector<std::shared_ptr<MetricSet>> all_sets;
    for (MetricSet *set = g_metric_sets_list_head; set; set = set->m_next) {
        if (std::shared_ptr<MetricSet> shared = set->shared_from_this()) {
            all_sets.push_back(std::move(shared));
        }
    }

    return all_sets;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<MetricSet> MetricSet::GetGlobal() {
    UniqueLock<Mutex> lock = LockMetricSetsList();

    if (!g_global_metric_set) {
        g_global_metric_set = Create2("Globals");

        g_global_metric_set->LinkIntoLockedList();
    }

    return g_global_metric_set;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MetricSet::CheckLockedList() {
    for (MetricSet *set = g_metric_sets_list_head; set; set = set->m_next) {
        if (set->m_prev) {
            ASSERT(set->m_prev->m_next == set);
        } else {
            ASSERT(set == g_metric_sets_list_head);
        }

        if (set->m_next) {
            ASSERT(set->m_next->m_prev == set);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MetricSet::LinkIntoLockedList() {
    m_next = g_metric_sets_list_head;
    if (m_next) {
        ASSERT(!m_next->m_prev);
        m_next->m_prev = this;
    }

    g_metric_sets_list_head = this;

    CheckLockedList();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<MetricSet> MetricSet::Create2(std::string name) {
    auto set = new MetricSet(std::move(name));

    // Ugh, but... private constructor...
    auto set_shared = std::shared_ptr<MetricSet>(set);

    return set_shared;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
