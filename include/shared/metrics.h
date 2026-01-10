#ifndef HEADER_4C177C4AC43A49018459AC1B73CDAE9C // -*- mode:c++ -*-
#define HEADER_4C177C4AC43A49018459AC1B73CDAE9C

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <atomic>
#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TimerDefs should be global objects. Anything else is (currently...)
// unsupported, and you'll just get a big mess.
//
// The TimerDef nesting should reflect the Timer nesting, but there are no
// checks.

class TimerDef {
  public:
    const std::string name;

    explicit TimerDef(std::string name, TimerDef *parent = nullptr);
    ~TimerDef();

    TimerDef(const TimerDef &) = delete;
    TimerDef &operator=(const TimerDef &) = delete;

    TimerDef(TimerDef &&) = delete;
    TimerDef &operator=(TimerDef &&) = delete;

    void Reset();

    uint64_t GetTotalNumTicks() const;
    uint64_t GetNumSamples() const;

    void AddTicks(uint64_t num_ticks);

    const TimerDef *GetParent() const;

    size_t GetNumChildren() const;
    const TimerDef *GetChildByIndex(size_t index) const;

  protected:
  private:
    TimerDef *m_parent = nullptr;
    std::vector<TimerDef *> m_children;

    std::atomic<uint64_t> m_total_num_ticks{0};
    std::atomic<uint64_t> m_num_samples{0};
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<const TimerDef *> GetRootTimerDefs();
void ResetTimerDefs();

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

        m_def->AddTicks(end_ticks - m_begin_ticks);
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

#endif
