// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "interface/ui/seedtree/SeedTree.hpp"  // IWYU pragma: associated

#include <opentxs/protobuf/HDPath.pb.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "interface/ui/base/Widget.hpp"
#include "internal/identity/Nym.hpp"
#include "internal/interface/ui/SeedTreeItem.hpp"
#include "internal/network/zeromq/Pipeline.hpp"
#include "internal/util/SharedPimpl.hpp"
#include "opentxs/api/Factory.internal.hpp"
#include "opentxs/api/crypto/Seed.hpp"
#include "opentxs/api/session/Client.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/api/session/Storage.hpp"
#include "opentxs/api/session/Wallet.hpp"
#include "opentxs/crypto/Bip32Child.hpp"  // IWYU pragma: keep
#include "opentxs/crypto/Seed.hpp"
#include "opentxs/crypto/SeedStyle.hpp"  // IWYU pragma: keep
#include "opentxs/crypto/Types.hpp"
#include "opentxs/identifier/Generic.hpp"
#include "opentxs/identifier/HDSeed.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/identity/Types.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/PasswordPrompt.hpp"  // IWYU pragma: keep

namespace opentxs::factory
{
auto SeedTreeModel(
    const api::session::Client& api,
    const SimpleCallback& cb) noexcept
    -> std::unique_ptr<ui::internal::SeedTree>
{
    using ReturnType = ui::implementation::SeedTree;

    return std::make_unique<ReturnType>(api, cb);
}
}  // namespace opentxs::factory

namespace opentxs::ui::implementation
{
SeedTree::SeedTree(
    const api::session::Client& api,
    const SimpleCallback& cb) noexcept
    : SeedTreeList(api, identifier::Generic{}, cb, false)
    , Worker(api, 100ms, "ui::SeedTree")
    , callbacks_()
    , default_nym_()
    , default_seed_()
{
    init_executor({
        UnallocatedCString{api.Endpoints().NymCreated()},
        UnallocatedCString{api.Endpoints().NymDownload()},
        UnallocatedCString{api.Endpoints().SeedUpdated()},
    });
    pipeline_.Push(MakeWork(Work::init));
}

auto SeedTree::add_children(ChildMap&& map) noexcept -> void
{
    add_items([&] {
        auto rows = ChildDefinitions{};

        for (auto& it : map) {
            auto& [seedID, seedData] = it;
            auto& [isPrimary, seedName, seedType, nymMap] = seedData;
            rows.emplace_back(
                std::move(seedID),
                std::make_pair(isPrimary, std::move(seedName)),
                [&] {
                    auto out = CustomData{};
                    out.reserve(1);
                    out.emplace_back(std::make_unique<crypto::SeedStyle>(
                                         std::get<2>(it.second))
                                         .release());

                    return out;
                }(),
                [&] {
                    auto output = CustomData{};
                    using Nyms = UnallocatedVector<SeedTreeItemRowData>;
                    auto& data = [&]() -> auto& {
                        auto& ptr = output.emplace_back(
                            std::make_unique<Nyms>().release());

                        assert_true(1u == output.size());
                        assert_false(nullptr == ptr);

                        return *reinterpret_cast<Nyms*>(ptr);
                    }();

                    for (auto& [nymID, nymData] : std::get<3>(it.second)) {
                        auto& [nymIndex, nymName] = nymData;
                        auto name = std::move(nymName);
                        auto& row = data.emplace_back(
                            std::move(nymID),
                            nymIndex,
                            [&] {
                                auto out = CustomData{};
                                out.reserve(1);
                                out.emplace_back(
                                    std::make_unique<UnallocatedCString>(
                                        std::move(name))
                                        .release());

                                return out;
                            }(),
                            CustomData{});
                        LogInsane()()("processing nym ")(row.id_, api_.Crypto())
                            .Flush();
                    }

                    return output;
                }());
        }

        return rows;
    }());
}

auto SeedTree::ClearCallbacks() const noexcept -> void
{
    Widget::ClearCallbacks();
    callbacks_.modify([](auto& data) { data = {}; });
}

auto SeedTree::check_default_nym() noexcept -> void
{
    const auto& api = api_;
    const auto old = *default_nym_.lock_shared();
    auto data = api.Wallet().DefaultNym();
    auto& [current, count] = data;

    if ((0u < count) && (old != current)) {
        default_nym_.modify([&](auto& nym) { nym.Assign(data.first); });

        {
            auto handle = callbacks_.lock_shared();
            const auto& cb = handle->nym_changed_;

            if (cb) { cb(current); }

            UpdateNotify();
        }

        if (false == old.empty()) { process_nym(old); }
    }
}

auto SeedTree::check_default_seed() noexcept -> void
{
    const auto& api = api_;
    const auto old = *default_seed_.lock_shared();
    const auto seedData = api.Crypto().Seed().DefaultSeed();
    const auto [id, count] = seedData;

    if ((0u < count) && (old != id)) {
        default_seed_.modify([&](auto& seed) { seed.Assign(seedData.first); });

        {
            auto handle = callbacks_.lock_shared();
            const auto& cb = handle->seed_changed_;

            if (cb) { cb(id); }

            UpdateNotify();
        }

        if (false == old.empty()) { process_seed(old); }
    }
}

auto SeedTree::construct_row(
    const SeedTreeRowID& id,
    const SeedTreeSortKey& index,
    CustomData& custom) const noexcept -> RowPointer
{
    return factory::SeedTreeItemModel(*this, api_, id, index, custom);
}

auto SeedTree::Debug() const noexcept -> UnallocatedCString
{
    auto out = std::stringstream{};
    auto counter{-1};
    out << "Seed tree\n";
    auto row = First();

    if (row->Valid()) {
        out << "  * row " << std::to_string(++counter) << ":\n";
        out << row->Debug();

        while (false == row->Last()) {
            row = Next();
            out << "  * row " << std::to_string(++counter) << ":\n";
            out << row->Debug();
        }
    } else {
        out << "  * empty\n";
    }

    return out.str();
}

auto SeedTree::DefaultNym() const noexcept -> identifier::Nym
{
    wait_for_startup();

    return *default_nym_.lock_shared();
}

auto SeedTree::DefaultSeed() const noexcept -> crypto::SeedID
{
    wait_for_startup();

    return *default_seed_.lock_shared();
}

auto SeedTree::load() noexcept -> void
{
    add_children([&] {
        auto out = ChildMap{};
        load_seeds(out);
        load_nyms(out);

        return out;
    }());
    check_default_seed();
}

auto SeedTree::load_seed(
    const crypto::SeedID& id,
    UnallocatedCString& name,
    crypto::SeedStyle& type,
    bool& isPrimary) const noexcept(false) -> void
{
    const auto& api = api_;
    const auto& factory = api.Factory();
    const auto& seeds = api.Crypto().Seed();
    const auto reason = factory.PasswordPrompt("Display seed tree");
    const auto seed = seeds.GetSeed(id, reason);

    if (crypto::SeedStyle::Error == seed.Type()) {
        throw std::runtime_error{"invalid seed"};
    }

    name = seeds.SeedDescription(id);
    type = seed.Type();
    isPrimary = (id == seeds.DefaultSeed().first);
}

auto SeedTree::load_nym(identifier::Nym&& nymID, ChildMap& out) const noexcept
    -> void
{
    LogTrace()()(nymID, api_.Crypto()).Flush();
    const auto& api = api_;
    const auto nym = api.Wallet().Nym(nymID);

    try {
        if (false == nym->HasPath()) {
            // TODO implement non-HD nyms
            throw std::runtime_error{
                "non-HD nyms are not yet supported in this model"};
        }

        const auto path = [&] {
            auto data = protobuf::HDPath{};
            nym->Internal().Path(data);

            return data;
        }();

        if (2 > path.child().size()) {
            throw std::runtime_error{"invalid path (missing nym index)"};
        }

        const auto index = path.child(1) ^ static_cast<std::uint32_t>(
                                               crypto::Bip32Child::HARDENED);

        static_assert(sizeof(index) <= sizeof(std::size_t));

        const auto seedID = api.Factory().Internal().SeedID(path.seed());

        if (seedID.empty()) {
            throw std::runtime_error{"invalid path (missing seed id)"};
        }

        auto& nymMap = std::get<3>(load_seed(seedID, out));

        if (auto it = nymMap.find(nymID); nymMap.end() == it) {
            nymMap.try_emplace(
                std::move(nymID),
                static_cast<SeedTreeItemSortKey>(index),
                nym_name(*nym));
        }
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return;
    }
}

auto SeedTree::load_nyms(ChildMap& out) const noexcept -> void
{
    for (const auto& nymID : api_.Wallet().LocalNyms()) {
        load_nym(std::move(const_cast<identifier::Nym&>(nymID)), out);
    }
}

auto SeedTree::load_seed(const crypto::SeedID& id, ChildMap& out) const
    noexcept(false) -> SeedData&
{
    if (auto it = out.find(id); out.end() != it) {

        return it->second;
    } else {
        auto& data = out[id];
        auto& [isPrimary, name, style, nyms] = data;
        load_seed(id, name, style, isPrimary);

        return data;
    }
}

auto SeedTree::load_seeds(ChildMap& out) const noexcept -> void
{
    const auto& api = api_;

    for (auto& [id, alias] : api.Storage().SeedList()) {
        const auto seedID = api.Factory().SeedIDFromBase58(id);

        try {
            load_seed(seedID, out);
        } catch (const std::exception& e) {
            LogError()()(e.what()).Flush();
            out.erase(seedID);
        }
    }
}

auto SeedTree::nym_name(const identity::Nym& nym) const noexcept
    -> UnallocatedCString
{
    auto out = std::stringstream{};
    out << nym.Name();
    auto handle = default_nym_.lock_shared();
    const auto& id = handle;
    LogTrace()()("Default nym is ")(*id, api_.Crypto()).Flush();

    if (nym.ID() == *id) { out << " (default)"; }

    return out.str();
}

auto SeedTree::pipeline(Message&& in) noexcept -> void
{
    if (false == running_.load()) { return; }

    const auto body = in.Payload();

    if (1 > body.size()) {
        LogError()()("Invalid message").Flush();

        LogAbort()().Abort();
    }

    const auto work = [&] {
        try {

            return body[0].as<Work>();
        } catch (...) {

            LogAbort()().Abort();
        }
    }();

    if ((false == startup_complete()) && (Work::init != work)) {
        pipeline_.Push(std::move(in));

        return;
    }

    switch (work) {
        case Work::shutdown: {
            if (auto previous = running_.exchange(false); previous) {
                shutdown(shutdown_promise_);
            }
        } break;
        case Work::new_nym:
        case Work::changed_nym: {
            process_nym(std::move(in));
        } break;
        case Work::changed_seed: {
            process_seed(std::move(in));
        } break;
        case Work::init: {
            startup();
        } break;
        case Work::statemachine: {
            do_work();
        } break;
        default: {
            LogError()()("Unhandled type").Flush();

            LogAbort()().Abort();
        }
    }
}

auto SeedTree::process_nym(Message&& in) noexcept -> void
{
    const auto body = in.Payload();

    assert_true(1 < body.size());

    const auto id = api_.Factory().NymIDFromHash(body[1].Bytes());
    check_default_nym();

    if (api_.Wallet().IsLocalNym(id)) { process_nym(id); }
}

auto SeedTree::process_nym(const identifier::Nym& id) noexcept -> void
{
    add_children([&] {
        auto out = ChildMap{};
        load_nym(identifier::Nym{id}, out);

        return out;
    }());
}

auto SeedTree::process_seed(Message&& in) noexcept -> void
{
    const auto body = in.Payload();

    assert_true(1 < body.size());

    const auto id = api_.Factory().SeedIDFromHash(body[1].Bytes());
    check_default_seed();
    process_seed(id);
}

auto SeedTree::process_seed(const crypto::SeedID& id) noexcept -> void
{
    auto index = SeedTreeSortKey{};
    auto custom = [&] {
        auto out = CustomData{};
        out.reserve(1);
        out.emplace_back(std::make_unique<crypto::SeedStyle>().release());

        return out;
    }();
    auto& type = *reinterpret_cast<crypto::SeedStyle*>(custom.at(0));

    try {
        load_seed(id, index.second, type, index.first);
        add_item(id, index, custom);
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return;
    }
}

auto SeedTree::SetCallbacks(Callbacks&& callbacks) noexcept -> void
{
    callbacks_.modify([cb = std::move(callbacks)](auto& data) mutable {
        data = std::move(cb);
    });
}

auto SeedTree::startup() noexcept -> void
{
    load();
    finish_startup();
    trigger();
}

SeedTree::~SeedTree()
{
    wait_for_startup();
    ClearCallbacks();
    signal_shutdown().get();
}
}  // namespace opentxs::ui::implementation
