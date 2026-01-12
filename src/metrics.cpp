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

struct MetricsGlobals {
    Mutex metric_sets_list_mutex;
    MetricSet *metric_sets_list_head = nullptr;
    std::shared_ptr<MetricSet> global_metric_set;
    std::unique_ptr<Counter> dummy_counter;
    std::unique_ptr<TimerDef> dummy_timer_def;
};

static MetricsGlobals *g_metrics;

static void DeleteMetricsGlobals() {
    delete g_metrics;
    g_metrics = nullptr;
}

static void InitMetricSetsListGlobals() {
    g_metrics = new MetricsGlobals;

    MUTEX_SET_NAME(g_metrics->metric_sets_list_mutex, "MetricSets list");

    atexit(&DeleteMetricsGlobals);
}

static void EnsureMetricsGlobalsInitialised() {
    std::call_once(g_metric_sets_list_once_flag, &InitMetricSetsListGlobals);
}

[[nodiscard]] static UniqueLock<Mutex> LockMetricSetsList() {
    EnsureMetricsGlobalsInitialised();

    return UniqueLock<Mutex>(g_metrics->metric_sets_list_mutex);
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
    if (m_set) {
        LockGuard lock(m_set->m_mutex);

        return {m_children.begin(), m_children.end()};
    } else {
        return {};
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Value::Value(std::string name_)
    : name(std::move(name_)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Counter::Reset() {
    m_value.store(0, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t Counter::GetValue() const {
    return m_value.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Counter::Counter(std::string name_)
    : Value(std::move(name_)) {
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
            ASSERT(g_metrics->metric_sets_list_head != this);
        } else {
            ASSERT(g_metrics->metric_sets_list_head == this);
            g_metrics->metric_sets_list_head = m_next;
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

TimerDef *MetricSet::CreateTimerDef(const std::shared_ptr<MetricSet> &set, std::string name) {
    return CreateTimerDef2(set, std::move(name), nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TimerDef *MetricSet::CreateTimerDef(const std::shared_ptr<MetricSet> &set, std::string name, TimerDef *parent) {
    ASSERT(parent);
    ASSERT(parent->m_set == set.get());
    return CreateTimerDef2(set, std::move(name), parent);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Counter *MetricSet::CreateCounter(const std::shared_ptr<MetricSet> &set, std::string name) {
    if (!set) {
        EnsureMetricsGlobalsInitialised();
        return g_metrics->dummy_counter.get();
    } else {
        auto counter = new Counter(std::move(name));
        set->AddValue(counter);
        return counter;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DerivedValue : public Value {
  public:
    DerivedValue(std::string name_, std::function<uint64_t()> fun)
        : Value(std::move(name_))
        , m_fun(std::move(fun)) {
    }

    void Reset() override {
        // no-op.
    }

    uint64_t GetValue() const override {
        return m_fun();
    }

  protected:
  private:
    std::function<uint64_t()> m_fun;
};

void MetricSet::CreateDerivedValue(const std::shared_ptr<MetricSet> &set, std::string name, std::function<uint64_t()> fun) {
    if (set) {
        set->AddValue(new DerivedValue(std::move(name), std::move(fun)));
    }
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

void MetricSet::ResetCounters() {
    LockGuard<Mutex> lock(m_mutex);

    for (const std::unique_ptr<Value> &counter : m_all_values_unique) {
        counter->Reset();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<const Value *> MetricSet::GetValues() const {
    LockGuard<Mutex> lock(m_mutex);

    return m_all_values_raw;
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

TimerDef *MetricSet::CreateTimerDef2(const std::shared_ptr<MetricSet> &set, std::string name, TimerDef *parent) {
    if (!set) {
        UniqueLock<Mutex> lock = LockMetricSetsList();

        // should really create this in advance, but the private constructor
        // complicates matters.
        if (!g_metrics->dummy_timer_def) {
            g_metrics->dummy_timer_def.reset(new TimerDef("dummy TimerDef", nullptr));
        }

        return g_metrics->dummy_timer_def.get();
    } else {
        auto def = new TimerDef(std::move(name), set.get());
        auto unique_def = std::unique_ptr<TimerDef>(def);

        LockGuard<Mutex> lock(set->m_mutex);

        if (!parent) {
            set->m_root_timer_defs.push_back(def);
        } else {
            def->m_parent = parent;
            parent->m_children.push_back(def);
        }

        set->m_all_timer_defs.push_back(std::move(unique_def));

        return def;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<MetricSet>> MetricSet::GetAll() {
    UniqueLock<Mutex> lock = LockMetricSetsList();

    //CheckList();

    std::vector<std::shared_ptr<MetricSet>> all_sets;
    for (MetricSet *set = g_metrics->metric_sets_list_head; set; set = set->m_next) {
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

    if (!g_metrics->global_metric_set) {
        g_metrics->global_metric_set = Create2("Globals");

        g_metrics->global_metric_set->LinkIntoLockedList();
    }

    return g_metrics->global_metric_set;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MetricSet::CheckLockedList() {
    for (MetricSet *set = g_metrics->metric_sets_list_head; set; set = set->m_next) {
        if (set->m_prev) {
            ASSERT(set->m_prev->m_next == set);
        } else {
            ASSERT(set == g_metrics->metric_sets_list_head);
        }

        if (set->m_next) {
            ASSERT(set->m_next->m_prev == set);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MetricSet::LinkIntoLockedList() {
    m_next = g_metrics->metric_sets_list_head;
    if (m_next) {
        ASSERT(!m_next->m_prev);
        m_next->m_prev = this;
    }

    g_metrics->metric_sets_list_head = this;

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

void MetricSet::AddValue(Value *value) {
    LockGuard<Mutex> lock(m_mutex);

    m_all_values_raw.push_back(value);
    m_all_values_unique.push_back(std::unique_ptr<Value>(value));
}
