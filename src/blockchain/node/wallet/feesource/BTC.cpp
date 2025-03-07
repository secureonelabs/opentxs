// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "internal/blockchain/node/wallet/Factory.hpp"  // IWYU pragma: associated

#include <boost/json.hpp>
#include <exception>
#include <optional>
#include <string_view>
#include <utility>

#include "blockchain/node/wallet/feesource/FeeSource.hpp"
#include "internal/blockchain/node/wallet/FeeSource.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/util/PMR.hpp"
#include "internal/util/alloc/Logging.hpp"
#include "opentxs/api/Network.hpp"
#include "opentxs/api/Session.internal.hpp"
#include "opentxs/api/network/ZeroMQ.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Types.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::blockchain::node::wallet
{
using namespace std::literals;

class Bitcoiner_live final : public FeeSource::Imp
{
public:
    auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }

    Bitcoiner_live(
        std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        network::zeromq::BatchID batch,
        allocator_type alloc) noexcept
        : Imp(std::move(api),
              std::move(node),
              "bitcoiner.live"sv,
              "/api/fees/estimates/latest"sv,
              true,
              std::move(batch),
              std::move(alloc))
    {
        LogTrace()()("My notification endpoint is ")(asio_).Flush();
    }

private:
    auto process(const boost::json::value& data) noexcept
        -> std::optional<Amount> final
    {
        try {
            const auto& rate =
                data.at("estimates").at("30").at("sat_per_vbyte").as_double();
            LogTrace()()("Received fee estimate from API: ")(rate).Flush();

            return process_double(rate, 1000);
        } catch (const std::exception& e) {
            LogError()()(e.what()).Flush();

            return std::nullopt;
        }
    }
};

class BitGo final : public FeeSource::Imp
{
public:
    auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }

    BitGo(
        std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        network::zeromq::BatchID batch,
        allocator_type alloc) noexcept
        : Imp(std::move(api),
              std::move(node),
              "www.bitgo.com"sv,
              "/api/v2/btc/tx/fee"sv,
              true,
              std::move(batch),
              std::move(alloc))
    {
        LogTrace()()("My notification endpoint is ")(asio_).Flush();
    }

private:
    auto process(const boost::json::value& data) noexcept
        -> std::optional<Amount> final
    {
        try {
            const auto& rate = data.at("feePerKb").as_int64();
            LogTrace()()("Received fee estimate from API: ")(rate).Flush();

            return process_int(rate, 1);
        } catch (const std::exception& e) {
            LogError()()(e.what()).Flush();

            return std::nullopt;
        }
    }
};

class Bitpay final : public FeeSource::Imp
{
public:
    auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }

    Bitpay(
        std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        network::zeromq::BatchID batch,
        allocator_type alloc) noexcept
        : Imp(std::move(api),
              std::move(node),
              "insight.bitpay.com"sv,
              "/api/utils/estimatefee?nbBlocks=2,4,6"sv,
              true,
              std::move(batch),
              std::move(alloc))
    {
        LogTrace()()("My notification endpoint is ")(asio_).Flush();
    }

private:
    auto process(const boost::json::value& data) noexcept
        -> std::optional<Amount> final
    {
        try {
            const auto& rate = data.at("2").as_double();
            LogTrace()()("Received fee estimate from API: ")(rate).Flush();

            return process_double(rate, 100000);
        } catch (const std::exception& e) {
            LogError()()(e.what()).Flush();

            return std::nullopt;
        }
    }
};

class Blockchain_info final : public FeeSource::Imp
{
public:
    auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }

    Blockchain_info(
        std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        network::zeromq::BatchID batch,
        allocator_type alloc) noexcept
        : Imp(std::move(api),
              std::move(node),
              "api.blockchain.info"sv,
              "/mempool/fees"sv,
              true,
              std::move(batch),
              std::move(alloc))
    {
        LogTrace()()("My notification endpoint is ")(asio_).Flush();
    }

private:
    auto process(const boost::json::value& data) noexcept
        -> std::optional<Amount> final
    {
        try {
            const auto& rate = data.at("regular").as_int64();
            LogTrace()()("Received fee estimate from API: ")(rate).Flush();

            return process_int(rate, 1000);
        } catch (const std::exception& e) {
            LogError()()(e.what()).Flush();

            return std::nullopt;
        }
    }
};

class Blockchair final : public FeeSource::Imp
{
public:
    auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }

    Blockchair(
        std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        network::zeromq::BatchID batch,
        allocator_type alloc) noexcept
        : Imp(std::move(api),
              std::move(node),
              "api.blockchair.com"sv,
              "/bitcoin/stats"sv,
              true,
              std::move(batch),
              std::move(alloc))
    {
        LogTrace()()("My notification endpoint is ")(asio_).Flush();
    }

private:
    auto process(const boost::json::value& data) noexcept
        -> std::optional<Amount> final
    {
        try {
            const auto& rate = data.at("data")
                                   .at("suggested_transaction_fee_per_byte_sat")
                                   .as_int64();
            LogTrace()()("Received fee estimate from API: ")(rate).Flush();

            return process_int(rate, 1000);
        } catch (const std::exception& e) {
            LogError()()(e.what()).Flush();

            return std::nullopt;
        }
    }
};

class BlockCypher final : public FeeSource::Imp
{
public:
    auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }

    BlockCypher(
        std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        network::zeromq::BatchID batch,
        allocator_type alloc) noexcept
        : Imp(std::move(api),
              std::move(node),
              "api.blockcypher.com"sv,
              "/v1/btc/main"sv,
              true,
              std::move(batch),
              std::move(alloc))
    {
        LogTrace()()("My notification endpoint is ")(asio_).Flush();
    }

private:
    auto process(const boost::json::value& data) noexcept
        -> std::optional<Amount> final
    {
        try {
            const auto& rate = data.at("medium_fee_per_kb").as_int64();
            LogTrace()()("Received fee estimate from API: ")(rate).Flush();

            return process_int(rate, 1);
        } catch (const std::exception& e) {
            LogError()()(e.what()).Flush();

            return std::nullopt;
        }
    }
};

class Blockstream final : public FeeSource::Imp
{
public:
    auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }

    Blockstream(
        std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        network::zeromq::BatchID batch,
        allocator_type alloc) noexcept
        : Imp(std::move(api),
              std::move(node),
              "blockstream.info"sv,
              "/api/fee-estimates"sv,
              true,
              std::move(batch),
              std::move(alloc))
    {
        LogTrace()()("My notification endpoint is ")(asio_).Flush();
    }

private:
    auto process(const boost::json::value& data) noexcept
        -> std::optional<Amount> final
    {
        try {
            const auto& rate = data.at("2").as_double();
            LogTrace()()("Received fee estimate from API: ")(rate).Flush();

            return process_double(rate, 1000);
        } catch (const std::exception& e) {
            LogError()()(e.what()).Flush();

            return std::nullopt;
        }
    }
};

class BTC_com final : public FeeSource::Imp
{
public:
    auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }

    BTC_com(
        std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        network::zeromq::BatchID batch,
        allocator_type alloc) noexcept
        : Imp(std::move(api),
              std::move(node),
              "btc.com"sv,
              "/service/fees/distribution"sv,
              true,
              std::move(batch),
              std::move(alloc))
    {
        LogTrace()()("My notification endpoint is ")(asio_).Flush();
    }

private:
    auto process(const boost::json::value& data) noexcept
        -> std::optional<Amount> final
    {
        try {
            const auto& rate =
                data.at("fees_recommended").at("one_block_fee").as_int64();
            LogTrace()()("Received fee estimate from API: ")(rate).Flush();

            return process_int(rate, 1000);
        } catch (const std::exception& e) {
            LogError()()(e.what()).Flush();

            return std::nullopt;
        }
    }
};

class Earn final : public FeeSource::Imp
{
public:
    auto get_deleter() noexcept -> delete_function final
    {
        return pmr::make_deleter(this);
    }

    Earn(
        std::shared_ptr<const api::internal::Session> api,
        std::shared_ptr<const node::Manager> node,
        network::zeromq::BatchID batch,
        allocator_type alloc) noexcept
        : Imp(std::move(api),
              std::move(node),
              "bitcoinfees.earn.com"sv,
              "/api/v1/fees/recommended"sv,
              true,
              std::move(batch),
              std::move(alloc))
    {
        LogTrace()()("My notification endpoint is ")(asio_).Flush();
    }

private:
    auto process(const boost::json::value& data) noexcept
        -> std::optional<Amount> final
    {
        try {
            const auto& rate = data.at("hourFee").as_int64();
            LogTrace()()("Received fee estimate from API: ")(rate).Flush();

            return process_int(rate, 1000);
        } catch (const std::exception& e) {
            LogError()()(e.what()).Flush();

            return std::nullopt;
        }
    }
};
}  // namespace opentxs::blockchain::node::wallet

namespace opentxs::factory
{
auto BTCFeeSources(
    std::shared_ptr<const api::internal::Session> api,
    std::shared_ptr<const blockchain::node::Manager> node) noexcept -> void
{
    assert_false(nullptr == api);
    assert_false(nullptr == node);

    using Source = blockchain::node::wallet::FeeSource;
    const auto& asio = api->Network().ZeroMQ().Context().Internal();
    Source{[&]() -> std::shared_ptr<Source::Imp> {
        using Imp = blockchain::node::wallet::Bitcoiner_live;
        const auto batchID = asio.PreallocateBatch();

        return std::allocate_shared<Imp>(
            alloc::PMR<Imp>{asio.Alloc(batchID)}, api, node, batchID);
    }()}
        .Init();
    Source{[&]() -> std::shared_ptr<Source::Imp> {
        using Imp = blockchain::node::wallet::BitGo;
        const auto batchID = asio.PreallocateBatch();

        return std::allocate_shared<Imp>(
            alloc::PMR<Imp>{asio.Alloc(batchID)}, api, node, batchID);
    }()}
        .Init();
    Source{[&]() -> std::shared_ptr<Source::Imp> {
        using Imp = blockchain::node::wallet::Bitpay;
        const auto batchID = asio.PreallocateBatch();

        return std::allocate_shared<Imp>(
            alloc::PMR<Imp>{asio.Alloc(batchID)}, api, node, batchID);
    }()}
        .Init();
    Source{[&]() -> std::shared_ptr<Source::Imp> {
        using Imp = blockchain::node::wallet::Blockchain_info;
        const auto batchID = asio.PreallocateBatch();

        return std::allocate_shared<Imp>(
            alloc::PMR<Imp>{asio.Alloc(batchID)}, api, node, batchID);
    }()}
        .Init();
    Source{[&]() -> std::shared_ptr<Source::Imp> {
        using Imp = blockchain::node::wallet::Blockchair;
        const auto batchID = asio.PreallocateBatch();

        return std::allocate_shared<Imp>(
            alloc::PMR<Imp>{asio.Alloc(batchID)}, api, node, batchID);
    }()}
        .Init();
    Source{[&]() -> std::shared_ptr<Source::Imp> {
        using Imp = blockchain::node::wallet::BlockCypher;
        const auto batchID = asio.PreallocateBatch();

        return std::allocate_shared<Imp>(
            alloc::PMR<Imp>{asio.Alloc(batchID)}, api, node, batchID);
    }()}
        .Init();
    Source{[&]() -> std::shared_ptr<Source::Imp> {
        using Imp = blockchain::node::wallet::Blockstream;
        const auto batchID = asio.PreallocateBatch();

        return std::allocate_shared<Imp>(
            alloc::PMR<Imp>{asio.Alloc(batchID)}, api, node, batchID);
    }()}
        .Init();
    Source{[&]() -> std::shared_ptr<Source::Imp> {
        using Imp = blockchain::node::wallet::BTC_com;
        const auto batchID = asio.PreallocateBatch();

        return std::allocate_shared<Imp>(
            alloc::PMR<Imp>{asio.Alloc(batchID)}, api, node, batchID);
    }()}
        .Init();
    Source{[&]() -> std::shared_ptr<Source::Imp> {
        using Imp = blockchain::node::wallet::Earn;
        const auto batchID = asio.PreallocateBatch();

        return std::allocate_shared<Imp>(
            alloc::PMR<Imp>{asio.Alloc(batchID)}, api, node, batchID);
    }()}
        .Init();
    // clang-format on
}
}  // namespace opentxs::factory
