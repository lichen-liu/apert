#pragma once

#include <thread>
#include <vector>
#include <deque>
#include <atomic>
#include "task.hpp"

/// work stealing private deque worker - receiver initiated

namespace TP
{
    enum class WSPDR_POLICY
    {
        STEAL_ONE = 0,
        STEAL_HALF = 1,
        DEFAULT = STEAL_HALF
    };

    class WSPDR_WORKER
    {
    public:
        void init(int worker_id, std::vector<WSPDR_WORKER *> workers, WSPDR_POLICY policy = WSPDR_POLICY::DEFAULT)
        {
            this->worker_id_ = worker_id;
            this->workers_ = std::move(workers);
            this->policy_ = policy;
        }
        void run();                                  // Running on a thread
        void send_task(TASK task, bool is_anchored); // Can be used cross thread, but only when task deque is empty (with assert)
        void terminate();
        void status() const;

    private:
        void add_task(TASK task); // Must not be used cross thread (with assert)
        bool try_send_steal_request(int requester_worker_id);
        void distribute_task(std::vector<TASK> task);
        void communicate();
        bool try_acquire_once();
        void update_tasks_status();
        bool is_alive() const { return this->is_alive_; }

    private:
        static constexpr int NO_REQUEST = -1;
        struct TASK_HOLDER
        {
            TASK task;
            bool is_anchored;
        };

    private:
        std::deque<TASK_HOLDER> tasks_;
        std::vector<WSPDR_WORKER *> workers_; // back when using by self, front when using by other
        std::vector<TASK> received_tasks_;
        std::thread::id thread_id_;
        int worker_id_ = -1;
        int num_tasks_done_ = 0;
        WSPDR_POLICY policy_ = WSPDR_POLICY::DEFAULT;
        std::atomic<int> request_ = NO_REQUEST;
        std::atomic<bool> has_tasks_ = false;
        std::atomic<bool> received_tasks_notify_ = false;
        std::atomic<bool> terminate_notify_ = false;
        std::atomic<bool> is_alive_ = false;
    };
}

#include "wspdr_worker_impl.hpp"