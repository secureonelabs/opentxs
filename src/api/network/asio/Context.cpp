// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "api/network/asio/Context.hpp"  // IWYU pragma: associated

#include <boost/thread/thread.hpp>
#include <cassert>
#include <memory>
#include <mutex>
#include <utility>

#include "BoostAsio.hpp"
#include "internal/util/Mutex.hpp"
#include "internal/util/Signals.hpp"
#include "internal/util/Thread.hpp"

namespace opentxs::api::network::asio
{
struct Context::Imp {
    operator boost::asio::io_context&() noexcept { return context_; }

    auto Init(unsigned int threads, ThreadPriority priority) noexcept -> bool
    {
        if (false == running_) {
            work_ = boost::asio::require(
                context_.get_executor(),
                boost::asio::execution::outstanding_work.tracked);
            static const auto options = [] {
                auto out = boost::thread::attributes{};
                out.set_stack_size(thread_pool_stack_size_);

                return out;
            }();

            for (unsigned int i{0}; i < threads; ++i) {
                auto thread = std::make_unique<boost::thread>(
                    options, [this, priority] { run(priority); });

                assert(thread);

                thread_pool_.add_thread(thread.release());
            }

            running_ = true;

            return true;
        }

        return false;
    }
    auto Stop() noexcept -> void
    {
        if (running_) {
            auto lock = Lock{lock_};
            work_ = boost::asio::any_io_executor{};
            context_.stop();
            thread_pool_.join_all();
            context_.restart();
            running_ = false;
        }
    }

    Imp() noexcept
        : lock_()
        , running_(false)
        , context_()
        , work_()
        , thread_pool_()
    {
    }
    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp&) -> Imp& = delete;
    auto operator=(Imp&&) -> Imp& = delete;

    ~Imp() { Stop(); }

private:
    mutable std::mutex lock_;
    bool running_;
    boost::asio::io_context context_;
    boost::asio::any_io_executor work_;
    boost::thread_group thread_pool_;

    auto run(ThreadPriority priority) noexcept -> void
    {
        SetThisThreadsName("asio thread - starting");
        SetThisThreadsPriority(priority);
        Signals::Block();
        context_.run();
    }
};

Context::Context() noexcept
    : imp_(std::make_unique<Imp>().release())
{
}

Context::operator boost::asio::io_context&() noexcept { return *imp_; }

auto Context::Init(unsigned int threads, ThreadPriority priority) noexcept
    -> bool
{
    return imp_->Init(threads, priority);
}

auto Context::Stop() noexcept -> void { imp_->Stop(); }

Context::~Context()
{
    if (nullptr != imp_) {
        delete imp_;
        imp_ = nullptr;
    }
}
}  // namespace opentxs::api::network::asio
