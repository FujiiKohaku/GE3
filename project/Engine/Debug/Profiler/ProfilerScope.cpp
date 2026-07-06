#include "ProfilerScope.h"

#ifdef _DEBUG
#include "Profiler.h"
#include "CPUProfiler.h"
#include "TimelineProfiler.h"

ProfilerScope::ProfilerScope(const std::string& name)
    : name_(name)
{
    Profiler::GetInstance()->GetCPUProfiler()->Begin(name_);
    Profiler::GetInstance()->GetTimelineProfiler()->BeginEvent(name_);
}

ProfilerScope::~ProfilerScope()
{
    Profiler::GetInstance()->GetCPUProfiler()->End(name_);
    Profiler::GetInstance()->GetTimelineProfiler()->EndEvent(name_);
}
#endif
