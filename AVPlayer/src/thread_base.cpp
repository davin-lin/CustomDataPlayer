#include "thread_base.h"

static void ThreadEntry(void* arg) {
    ThreadBase* thread = (ThreadBase*)arg;
    thread->Run();
}

ThreadBase::ThreadBase() {
}

ThreadBase::~ThreadBase() {
    Stop();
}

void ThreadBase::Start() {
    thread_ = std::thread(&ThreadBase::Run, this);
}

void ThreadBase::Stop() {
    bool expected = false;
    if (stop_.compare_exchange_weak(expected, true)) {
        if (thread_.joinable()) {
            thread_.join();
        }
    }
}