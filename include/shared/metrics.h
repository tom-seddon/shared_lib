#ifndef HEADER_4C177C4AC43A49018459AC1B73CDAE9C // -*- mode:c++ -*-
#define HEADER_4C177C4AC43A49018459AC1B73CDAE9C

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <atomic>
#include <string>
#include <vector>
#include <memory>
#include "mutex.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MetricSet;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TimerDef {
  public:
    const std::string name;

    ~TimerDef() = default;

    TimerDef(const TimerDef &) = delete;
    TimerDef &operator=(const TimerDef &) = delete;

    TimerDef(TimerDef &&) = delete;
    TimerDef &operator=(TimerDef &&) = delete;

    void Reset();

    uint64_t GetTotalNumTicks() const;
    uint64_t GetNumSamples() const;

    void AddTicks(uint64_t num_ticks);

    const TimerDef *GetParent() const;
    std::vector<const TimerDef *> GetChildren() const;

  protected:
  private:
    MetricSet *m_set = nullptr;
    TimerDef *m_parent = nullptr;
    std::vector<TimerDef *> m_children;

    std::atomic<uint64_t> m_total_num_ticks{0};
    std::atomic<uint64_t> m_num_samples{0};

    explicit TimerDef(std::string name, MetricSet *set);

    friend class MetricSet;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Timer {
  public:
    explicit inline Timer(TimerDef *def)
        : m_def(def)
        , m_begin_ticks(GetCurrentTickCount()) {
    }

    inline ~Timer() {
        uint64_t end_ticks = GetCurrentTickCount();

        if (m_def) {
            m_def->AddTicks(end_ticks - m_begin_ticks);
        }
    }

    Timer(const Timer &) = delete;
    Timer &operator=(const Timer &) = delete;

    // (could be implemented if required.)
    Timer(Timer &&) = delete;
    Timer &operator=(Timer &&) = delete;

  protected:
  private:
    TimerDef *m_def = nullptr;
    uint64_t m_begin_ticks = 0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MetricSet : public std::enable_shared_from_this<MetricSet> {
  public:
    static std::shared_ptr<MetricSet> Create(std::string name);
    ~MetricSet();

    MetricSet(const MetricSet &) = delete;
    MetricSet &operator=(const MetricSet &) = delete;
    MetricSet(MetricSet &&) = delete;
    MetricSet &operator=(MetricSet &&) = delete;

    // Lifespan of a TimerDef is that of the owning MetricSet: i.e., provided
    // the caller has a shared_ptr<MetricSet>, the return values will be valid.
    //
    // There's no way to remove a TimerDef once created. The contents of a
    // MetricSet aren't intended to be particularly dynamic.
    //
    // The TimerDef nesting should reflect the Timer nesting if the values are
    // to make sense, but there are no actual checks for this...
    TimerDef *CreateTimerDef(std::string name);
    TimerDef *CreateTimerDef(std::string name, TimerDef *parent);

    void ResetTimerDefs();
    std::vector<const TimerDef *> GetRootTimerDefs() const;

    std::string GetName() const;
    void SetName(std::string name);

    static std::vector<std::shared_ptr<MetricSet>> GetAll();

    // Safe to call at any time, including as part of global initialization.
    std::shared_ptr<MetricSet> GetGlobal();

  protected:
  private:
    mutable Mutex m_mutex;

    // lock m_mutex before accessing.
    std::vector<std::unique_ptr<TimerDef>> m_all_timer_defs;
    std::vector<const TimerDef *> m_root_timer_defs;
    std::string m_name;

    // lock g_all_metric_sets_mutex before accessing.
    MetricSet *m_next = nullptr;
    MetricSet *m_prev = nullptr;

    TimerDef *CreateTimerDef2(std::string name, TimerDef *parent);
    explicit MetricSet(std::string name);
    static void CheckLockedList();
    void LinkIntoLockedList();
    static std::shared_ptr<MetricSet> Create2(std::string name);

    friend class TimerDef;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
