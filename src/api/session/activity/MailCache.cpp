// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "api/session/activity/MailCache.hpp"  // IWYU pragma: associated

#include <chrono>
#include <cstring>
#include <functional>
#include <iterator>
#include <mutex>
#include <queue>
#include <utility>

#include "internal/api/session/Storage.hpp"
#include "internal/core/String.hpp"
#include "internal/core/contract/peer/Object.hpp"
#include "internal/network/zeromq/socket/Publish.hpp"
#include "internal/otx/common/Message.hpp"
#include "internal/util/Mutex.hpp"
#include "internal/util/PasswordPrompt.hpp"
#include "internal/util/Pimpl.hpp"
#include "opentxs/Context.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/WorkType.hpp"  // IWYU pragma: keep
#include "opentxs/WorkType.internal.hpp"
#include "opentxs/api/Factory.internal.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Factory.internal.hpp"
#include "opentxs/api/session/Storage.hpp"
#include "opentxs/api/session/Wallet.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/identifier/Generic.hpp"
#include "opentxs/identifier/Nym.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/storage/Types.internal.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/PasswordPrompt.hpp"
#include "util/ByteLiterals.hpp"
#include "util/JobCounter.hpp"
#include "util/ScopeGuard.hpp"

namespace opentxs::api::session::activity
{
struct MailCache::Imp : public std::enable_shared_from_this<Imp> {
    struct Task {
        Outstanding counter_;
        const PasswordPrompt reason_;
        const identifier::Nym nym_;
        const identifier::Generic item_;
        const otx::client::StorageBox box_;
        const SimpleCallback done_;
        std::promise<UnallocatedCString> promise_;

        Task(
            const api::Session& api,
            const identifier::Nym& nym,
            const identifier::Generic& id,
            const otx::client::StorageBox box,
            const PasswordPrompt& reason,
            SimpleCallback done,
            JobCounter& jobs) noexcept
            : counter_(jobs.Allocate())
            , reason_([&] {
                auto out =
                    api.Factory().PasswordPrompt(reason.GetDisplayString());
                out.Internal().SetPassword(reason.Internal().Password());

                return out;
            }())
            , nym_(nym)
            , item_(id)
            , box_(box)
            , done_(std::move(done))
            , promise_()
        {
            // TODO hold shared_ptr<api::Session> as a member variable
            ++counter_;
        }

        Task() = delete;
        Task(const Task&) = delete;
        Task(Task&&) = delete;
        auto operator=(const Task&) -> Task& = delete;
        auto operator=(Task&&) -> Task& = delete;

        ~Task() = default;
    };

    auto Mail(
        const identifier::Nym& nym,
        const identifier::Generic& id,
        const otx::client::StorageBox& box) const noexcept
        -> std::unique_ptr<Message>
    {
        auto output = std::unique_ptr<Message>{};
        auto raw = UnallocatedCString{};
        auto alias = UnallocatedCString{};
        using enum opentxs::storage::ErrorReporting;
        const bool loaded =
            api_.Storage().Internal().Load(nym, id, box, raw, alias, silent);

        if (false == loaded) {
            LogError()()("Failed to load message ")(id, api_.Crypto()).Flush();

            return output;
        }

        if (raw.empty()) {
            LogError()()("Empty message ")(id, api_.Crypto()).Flush();

            return output;
        }

        output = api_.Factory().Internal().Session().Message();

        assert_false(nullptr == output);

        if (false ==
            output->LoadContractFromString(String::Factory(raw.c_str()))) {
            LogError()()("Failed to deserialize message ")(id, api_.Crypto())
                .Flush();

            output.reset();
        }

        return output;
    }
    auto ProcessThreadPool(Task* pTask) const noexcept -> void
    {
        assert_false(nullptr == pTask);

        auto& task = *pTask;
        auto message = UnallocatedCString{};
        auto postcondition = ScopeGuard{[&] {
            task.promise_.set_value(message);

            {
                auto work = MakeWork(value(WorkType::MessageLoaded));
                work.AddFrame(task.nym_);
                work.AddFrame(task.item_);
                work.AddFrame(task.box_);
                work.AddFrame(message);
                message_loaded_.Send(std::move(work));
            }

            --task.counter_;
            // NOTE make a copy of the callback since executing it will delete
            // the task
            const auto cb{task.done_};

            assert_false(nullptr == cb);

            cb();
        }};
        const auto mail = Mail(task.nym_, task.item_, task.box_);

        if (!mail) {
            message = "Error: Unable to load mail item";

            return;
        }

        const auto nym = api_.Wallet().Nym(task.nym_);

        if (false == bool(nym)) {
            message = "Error: Unable to load recipient nym";

            return;
        }

        const auto object = api_.Factory().Internal().Session().PeerObject(
            nym, mail->payload_, task.reason_);

        if (!object) {
            message = "Error: Unable to decrypt message";

            return;
        }

        if (!object->Message()) {
            message = "Unable to display message";

            return;
        }

        message = *object->Message();
    }

    auto CacheText(
        const identifier::Nym& nym,
        const identifier::Generic& id,
        const otx::client::StorageBox box,
        const UnallocatedCString& text) noexcept -> void
    {
        auto key = this->key(nym, id, box);
        auto promise = std::promise<UnallocatedCString>{};
        promise.set_value(text);
        auto lock = Lock{lock_};
        results_.try_emplace(std::move(key), promise.get_future());
    }
    auto Get(
        const identifier::Nym& nym,
        const identifier::Generic& id,
        const otx::client::StorageBox box,
        const PasswordPrompt& reason) noexcept
        -> std::shared_future<UnallocatedCString>
    {
        auto lock = Lock{lock_};
        auto key = this->key(nym, id, box);

        if (const auto it = results_.find(key); results_.end() != it) {

            return it->second;
        }

        auto [tIt, newTask] = tasks_.try_emplace(
            key,
            api_,
            nym,
            id,
            box,
            reason,
            [this, key] { finish_task(key); },
            jobs_);

        assert_true(newTask);

        auto& task = tIt->second;
        const auto [fIt, newFuture] =
            results_.try_emplace(key, task.promise_.get_future());

        assert_true(newFuture);

        const auto& future = fIt->second;
        fifo_.push(std::move(key));
        RunJob([me = shared_from_this(), pTask = &task] {
            me->ProcessThreadPool(pTask);
        });

        return future;
    }

    Imp(const api::Session& api,
        const opentxs::network::zeromq::socket::Publish& messageLoaded) noexcept
        : api_(api)
        , message_loaded_(messageLoaded)
        , lock_()
        , jobs_()
        , cached_bytes_(0)
        , tasks_()
        , results_()
        , fifo_()
    {
    }
    Imp() = delete;
    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp&) -> Imp& = delete;
    auto operator=(Imp&&) -> Imp& = delete;

    ~Imp() = default;

private:
    const api::Session& api_;
    const opentxs::network::zeromq::socket::Publish& message_loaded_;
    mutable std::mutex lock_;
    JobCounter jobs_;
    std::size_t cached_bytes_;
    UnallocatedMap<identifier::Generic, Task> tasks_;
    UnallocatedMap<identifier::Generic, std::shared_future<UnallocatedCString>>
        results_;
    std::queue<identifier::Generic> fifo_;

    auto key(
        const identifier::Nym& nym,
        const identifier::Generic& id,
        const otx::client::StorageBox box) const noexcept -> identifier::Generic
    {
        const auto preimage = [&] {
            auto out = space(nym.size() + id.size() + sizeof(box));
            auto* it = out.data();
            std::memcpy(it, nym.data(), nym.size());
            std::advance(it, nym.size());
            std::memcpy(it, id.data(), id.size());
            std::advance(it, id.size());
            std::memcpy(it, &box, sizeof(box));

            return out;
        }();

        return api_.Factory().IdentifierFromPreimage(reader(preimage));
    }

    // NOTE this should only be called from the thread pool
    auto finish_task(const identifier::Generic& key) noexcept -> void
    {
        static constexpr auto limit = 250_mib;
        static constexpr auto wait = 0s;

        auto lock = Lock{lock_};
        tasks_.erase(key);

        {
            const auto& result = results_.at(key).get();
            cached_bytes_ += result.size();
        }

        while (cached_bytes_ > limit) {
            // NOTE don't clear the only cached item, no matter how large
            if (1 >= fifo_.size()) { break; }

            {
                auto& id = fifo_.front();

                {
                    const auto& future = results_.at(id);
                    const auto status = future.wait_for(wait);

                    if (std::future_status::ready != status) { break; }

                    const auto& result = future.get();
                    cached_bytes_ -= result.size();
                }

                results_.erase(id);
            }

            fifo_.pop();
        }
    }
};

MailCache::MailCache(
    const api::Session& api,
    const opentxs::network::zeromq::socket::Publish& messageLoaded) noexcept
    : imp_(std::make_shared<Imp>(api, messageLoaded))
{
    assert_false(nullptr == imp_);
}

auto MailCache::CacheText(
    const identifier::Nym& nym,
    const identifier::Generic& id,
    const otx::client::StorageBox box,
    const UnallocatedCString& text) noexcept -> void
{
    return imp_->CacheText(nym, id, box, text);
}

auto MailCache::GetText(
    const identifier::Nym& nym,
    const identifier::Generic& id,
    const otx::client::StorageBox box,
    const PasswordPrompt& reason) noexcept
    -> std::shared_future<UnallocatedCString>
{
    return imp_->Get(nym, id, box, reason);
}

auto MailCache::LoadMail(
    const identifier::Nym& nym,
    const identifier::Generic& id,
    const otx::client::StorageBox& box) const noexcept
    -> std::unique_ptr<Message>
{
    return imp_->Mail(nym, id, box);
}

MailCache::~MailCache() = default;
}  // namespace opentxs::api::session::activity
