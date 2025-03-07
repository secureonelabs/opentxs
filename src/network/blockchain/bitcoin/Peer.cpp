// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "network/blockchain/bitcoin/Peer.hpp"  // IWYU pragma: associated

#include <frozen/set.h>
#include <opentxs/protobuf/BlockchainPeerAddress.pb.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <iterator>
#include <random>
#include <span>
#include <stdexcept>
#include <utility>

#include "BoostAsio.hpp"
#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/block/Transaction.hpp"
#include "internal/blockchain/database/Peer.hpp"
#include "internal/blockchain/node/Config.hpp"
#include "internal/blockchain/node/Manager.hpp"
#include "internal/blockchain/node/Mempool.hpp"
#include "internal/blockchain/node/blockoracle/BlockBatch.hpp"
#include "internal/blockchain/node/blockoracle/BlockOracle.hpp"
#include "internal/blockchain/node/blockoracle/Types.hpp"
#include "internal/blockchain/node/headeroracle/HeaderOracle.hpp"
#include "internal/blockchain/node/headeroracle/Types.hpp"
#include "internal/blockchain/params/ChainData.hpp"
#include "internal/blockchain/protocol/bitcoin/base/block/Transaction.hpp"
#include "internal/network/asio/Types.hpp"
#include "internal/network/blockchain/Address.hpp"
#include "internal/network/blockchain/bitcoin/Factory.hpp"
#include "internal/network/blockchain/bitcoin/message/Addr.hpp"
#include "internal/network/blockchain/bitcoin/message/Addr2.hpp"
#include "internal/network/blockchain/bitcoin/message/Block.hpp"
#include "internal/network/blockchain/bitcoin/message/Cfheaders.hpp"
#include "internal/network/blockchain/bitcoin/message/Cfilter.hpp"
#include "internal/network/blockchain/bitcoin/message/Factory.hpp"
#include "internal/network/blockchain/bitcoin/message/Getaddr.hpp"
#include "internal/network/blockchain/bitcoin/message/Getcfheaders.hpp"
#include "internal/network/blockchain/bitcoin/message/Getcfilters.hpp"
#include "internal/network/blockchain/bitcoin/message/Getdata.hpp"
#include "internal/network/blockchain/bitcoin/message/Getheaders.hpp"
#include "internal/network/blockchain/bitcoin/message/Header.hpp"
#include "internal/network/blockchain/bitcoin/message/Headers.hpp"
#include "internal/network/blockchain/bitcoin/message/Inv.hpp"
#include "internal/network/blockchain/bitcoin/message/Mempool.hpp"
#include "internal/network/blockchain/bitcoin/message/Message.hpp"
#include "internal/network/blockchain/bitcoin/message/Notfound.hpp"
#include "internal/network/blockchain/bitcoin/message/Ping.hpp"
#include "internal/network/blockchain/bitcoin/message/Pong.hpp"
#include "internal/network/blockchain/bitcoin/message/Reject.hpp"
#include "internal/network/blockchain/bitcoin/message/Sendaddr2.hpp"
#include "internal/network/blockchain/bitcoin/message/Tx.hpp"
#include "internal/network/blockchain/bitcoin/message/Types.hpp"
#include "internal/network/blockchain/bitcoin/message/Verack.hpp"
#include "internal/network/blockchain/bitcoin/message/Version.hpp"
#include "internal/network/zeromq/Context.hpp"
#include "internal/network/zeromq/socket/Raw.hpp"
#include "internal/util/Future.hpp"
#include "internal/util/P0330.hpp"
#include "internal/util/alloc/Logging.hpp"
#include "network/blockchain/bitcoin/Inventory.hpp"
#include "network/blockchain/bitcoin/Peer.tpp"
#include "opentxs/Context.hpp"
#include "opentxs/Time.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/WorkType.internal.hpp"
#include "opentxs/api/Network.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/Session.internal.hpp"
#include "opentxs/api/network/ZeroMQ.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/blockchain/Category.hpp"  // IWYU pragma: keep
#include "opentxs/blockchain/Type.hpp"      // IWYU pragma: keep
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/Block.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/blockchain/block/Position.hpp"
#include "opentxs/blockchain/block/Transaction.hpp"
#include "opentxs/blockchain/block/TransactionHash.hpp"
#include "opentxs/blockchain/block/Types.hpp"
#include "opentxs/blockchain/cfilter/GCS.hpp"
#include "opentxs/blockchain/cfilter/Hash.hpp"
#include "opentxs/blockchain/cfilter/Header.hpp"
#include "opentxs/blockchain/node/BlockOracle.hpp"
#include "opentxs/blockchain/node/FilterOracle.hpp"
#include "opentxs/blockchain/node/HeaderOracle.hpp"
#include "opentxs/blockchain/node/Manager.hpp"
#include "opentxs/blockchain/node/Types.internal.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/FixedByteArray.hpp"
#include "opentxs/network/asio/Socket.hpp"
#include "opentxs/network/blockchain/Address.hpp"
#include "opentxs/network/blockchain/Transport.hpp"  // IWYU pragma: keep
#include "opentxs/network/blockchain/Types.hpp"
#include "opentxs/network/blockchain/bitcoin/Service.hpp"  // IWYU pragma: keep
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/protobuf/Types.internal.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Writer.hpp"
#include "util/Container.hpp"
#include "util/ScopeGuard.hpp"

namespace opentxs::factory
{
auto BlockchainPeerBitcoin(
    std::shared_ptr<const api::internal::Session> api,
    std::shared_ptr<const opentxs::blockchain::node::Manager> network,
    network::blockchain::bitcoin::message::Nonce nonce,
    int peerID,
    network::blockchain::Address address,
    const Set<network::blockchain::Address>& gossip,
    std::string_view fromParent,
    std::optional<network::asio::Socket> socket) -> void
{
    assert_false(nullptr == api);
    assert_false(nullptr == network);
    assert_true(address.IsValid());

    using Network = opentxs::network::blockchain::Transport;
    using ReturnType = opentxs::network::blockchain::bitcoin::Peer;

    switch (address.Type()) {
        case Network::ipv6:
        case Network::ipv4:
        case Network::zmq: {
        } break;
        case Network::onion2:
        case Network::onion3:
        case Network::eep:
        case Network::cjdns:
        default: {
            LogAbort()().Abort();
        }
    }

    const auto& zmq = api->Network().ZeroMQ().Context().Internal();
    const auto batchID = zmq.PreallocateBatch();
    auto alloc = alloc::PMR<ReturnType>{zmq.Alloc(batchID)};
    auto actor = std::allocate_shared<ReturnType>(
        alloc,
        std::move(api),
        std::move(network),
        nonce,
        peerID,
        std::move(address),
        Set<network::blockchain::Address>{gossip, alloc},
        blockchain::params::get(network->Internal().Chain()).P2PVersion(),
        fromParent,
        std::move(socket),
        batchID);
    actor->Init(actor);
}
}  // namespace opentxs::factory

namespace opentxs::network::blockchain::bitcoin
{
using namespace std::literals;

Peer::Peer(
    std::shared_ptr<const api::internal::Session> api,
    std::shared_ptr<const opentxs::blockchain::node::Manager> network,
    message::Nonce nonce,
    int peerID,
    blockchain::Address address,
    Set<network::blockchain::Address> gossip,
    message::ProtocolVersion protocol,
    std::string_view fromParent,
    std::optional<asio::Socket> socket,
    const zeromq::BatchID batch,
    allocator_type alloc) noexcept
    : Imp(std::move(api),
          std::move(network),
          peerID,
          std::move(address),
          std::move(gossip),
          30s,
          1min,
          10min,
          HeaderType::size_,
          fromParent,
          std::move(socket),
          batch,
          alloc)
    , mempool_(network_.Internal().Mempool())
    , user_agent_([&] {
        auto out = CString{get_allocator()};
        out.append("/opentxs:"sv);
        out.append(VersionString());
        out.append("/"sv);

        return out;
    }())
    , peer_cfilter_([&] {
        switch (config_.profile_) {
            case BlockchainProfile::desktop_native: {

                return true;
            }
            case BlockchainProfile::mobile:
            case BlockchainProfile::desktop:
            case BlockchainProfile::server: {

                return false;
            }
            default: {
                LogAbort()().Abort();
            }
        }
    }())
    , nonce_(nonce)
    , inv_block_([&] {
        using Type = Inventory::Type;
        // TODO do some chains use MsgWitnessBlock?

        return Type::MsgBlock;
    }())
    , inv_tx_([&] {
        using Type = Inventory::Type;

        if (opentxs::blockchain::params::get(chain_).SupportsSegwit()) {

            return Type::MsgWitnessTx;
        } else {

            return Type::MsgTx;
        }
    }())
    , local_address_([&] {
        const auto& params = opentxs::blockchain::params::get(chain_);
        using enum Transport;
        static const auto addr = asio::serialize(asio::localhost4to6());

        return api_.Factory().BlockchainAddress(
            params.P2PDefaultProtocol(),
            ipv6,
            addr.Bytes(),
            params.P2PDefaultPort(),
            chain_,
            Clock::now(),
            get_local_services(protocol_, chain_, config_, alloc));
    }())
    , protocol_((0 == protocol) ? default_protocol_version_ : protocol)
    , bip37_(false)
    , addr_v2_(false)
    , can_gossip_zmq_(this->address().Type() == Transport::zmq)
    , handshake_()
    , verification_()
{
}

auto Peer::can_gossip(const blockchain::Address& address) const noexcept -> bool
{
    if (address.Internal().Incoming()) { return false; }

    using enum blockchain::Transport;

    if (addr_v2_) {
        switch (address.Type()) {
            case ipv4:
            case ipv6:
            case onion2:
            case onion3:
            case eep:
            case cjdns: {

                return true;
            }
            case zmq: {
                if (false == can_gossip_zmq_) { return false; }

                switch (address.Subtype()) {
                    case ipv4:
                    case ipv6:
                    case onion2:
                    case onion3:
                    case eep:
                    case cjdns: {

                        return true;
                    }
                    case invalid:
                    case zmq:
                    default: {

                        return false;
                    }
                }
            }
            case invalid:
            default: {

                return false;
            }
        }
    } else {
        switch (address.Type()) {
            case ipv4:
            case ipv6:
            case onion2:
            case cjdns: {

                return true;
            }
            case invalid:
            case onion3:
            case eep:
            case zmq:
            default: {

                return false;
            }
        }
    }
}

auto Peer::check_handshake(allocator_type monotonic) noexcept -> void
{
    if (handshake_.got_version_ && handshake_.got_verack_) {
        transition_state_verify(monotonic);
    }
}

auto Peer::check_verification(allocator_type monotonic) noexcept -> void
{
    const auto verified =
        verification_.got_block_header_ &&
        (verification_.got_cfheader_ || (false == peer_cfilter_));

    if (verified) { transition_state_run(monotonic); }
}

auto Peer::extract_body_size(const zeromq::Frame& header) const noexcept
    -> std::size_t
{
    return message::internal::Header{header.Bytes()}.PayloadSize();
}

auto Peer::get_local_services(
    const message::ProtocolVersion version,
    const opentxs::blockchain::Type network,
    const opentxs::blockchain::node::internal::Config& config,
    allocator_type alloc) noexcept
    -> Set<opentxs::network::blockchain::bitcoin::Service>
{
    using Chain = opentxs::blockchain::Type;
    using enum opentxs::network::blockchain::bitcoin::Service;
    auto output = Set<opentxs::network::blockchain::bitcoin::Service>{alloc};
    output.clear();

    if (has_segwit(network)) { output.emplace(Witness); }

    if (is_descended_from(associated_mainnet(network), Chain::BitcoinCash)) {
        output.emplace(BitcoinCash);
    }

    switch (config.profile_) {
        case BlockchainProfile::mobile: {
        } break;
        case BlockchainProfile::desktop:
        case BlockchainProfile::desktop_native: {
            output.emplace(Service::Limited);
            output.emplace(Service::CompactFilters);
        } break;
        case BlockchainProfile::server: {
            output.emplace(Service::Network);
            output.emplace(Service::CompactFilters);
        } break;
        default: {

            LogAbort()().Abort();
        }
    }

    return output;
}

auto Peer::ignore_message(message::Command type) const noexcept -> bool
{
    using enum State;
    using enum message::Command;

    switch (state()) {
        case pre_init:
        case init:
        case connect:
        case shutdown: {
            LogAbort()()(name_)(": processing message in invalid state")
                .Abort();
        }
        case handshake: {
            switch (type) {
                case sendaddr2:
                case verack:
                case version: {
                } break;
                default: {
                    log_()(name_)(": ignoring ")(print(type))(
                        " during handshake")
                        .Flush();

                    return true;
                }
            }
        } break;
        case verify:
        case run:
        default: {
            log_()(name_)(": processing ")(print(type)).Flush();
        }
    }

    return false;
}

auto Peer::is_implemented(message::Command in) noexcept -> bool
{
    using enum message::Command;
    static constexpr auto set = frozen::make_set<message::Command>({
        addr,       addr2,     block,        cfcheckpt,    cfheaders,   cfilter,
        getaddr,    getblocks, getcfcheckpt, getcfheaders, getcfilters, getdata,
        getheaders, headers,   inv,          mempool,      notfound,    ping,
        pong,       reject,    sendaddr2,    tx,           verack,      version,
    });

    return set.end() != set.find(in);
}

auto Peer::process_addresses(
    std::span<Address> data,
    allocator_type monotonic) noexcept -> void
{
    reset_peers_timer();
    database_.Import([&] {
        auto peers = Vector<Address>{monotonic};
        peers.reserve(data.size());
        peers.clear();

        for (auto& address : data) {
            address.Internal().SetLastConnected({});
            peers.emplace_back(address);
        }

        return peers;
    }());
    add_known_address(data);
    to_peer_manager_.SendDeferred([&] {
        using enum opentxs::blockchain::node::PeerManagerJobs;
        auto out = MakeWork(gossip_address);

        for (const auto& address : data) {
            const auto proto = [&] {
                auto p = protobuf::BlockchainPeerAddress{};
                address.Internal().Serialize(p);

                return p;
            }();
            protobuf::write(proto, out.AppendBytes());
        }

        return out;
    }());
}

auto Peer::process_block_hash(
    const Inventory& inv,
    allocator_type monotonic) noexcept -> bool
{
    const auto block = opentxs::blockchain::block::Hash{inv.hash_.Bytes()};
    add_known_block(block);

    if (block_oracle_.Internal().BlockExists(block)) { return false; }

    if (fetch_all_blocks()) {

        return true;
    } else {
        to_header_oracle_.SendDeferred([&] {
            using enum opentxs::blockchain::node::headeroracle::Job;
            auto out = MakeWork(submit_block_hash);
            out.AddFrame(block);

            return out;
        }());
    }

    return false;
}

auto Peer::process_block_hashes(
    std::span<Inventory> hashes,
    allocator_type monotonic) noexcept -> void
{
    auto unseen = Vector<Inventory>{monotonic};
    unseen.reserve(hashes.size());
    unseen.clear();

    for (auto& hash : hashes) {
        if (process_block_hash(hash, monotonic)) { unseen.emplace_back(hash); }
    }

    if (false == unseen.empty()) {
        transmit_protocol_getdata(unseen, monotonic);
    }
}

auto Peer::process_broadcasttx(Message&& msg, allocator_type monotonic) noexcept
    -> void
{
    const auto body = msg.Payload();

    assert_true(1 < body.size());

    transmit_protocol_tx(body[1].Bytes(), monotonic);
}

auto Peer::process_protocol(
    Message&& message,
    allocator_type monotonic) noexcept -> void
{
    const auto& log = log_;

    try {
        auto command = factory::BitcoinP2PMessage(
            api_,
            chain_,
            address().Type(),
            protocol_,
            std::move(message),
            monotonic);
        log()(name_)(": processing ")(command.Describe()).Flush();

        if (is_implemented(command.Command()) && (false == command.IsValid())) {
            const auto error = CString{monotonic}
                                   .append("received invalid ")
                                   .append(command.Describe());

            throw std::runtime_error{error.c_str()};
        }

        const auto type = command.Command();

        if (const auto chain = command.Network(); chain != chain_) {
            auto error = CString{monotonic};
            error.append("received message intended for ");
            error.append(print(chain));

            throw std::runtime_error{error.c_str()};
        }

        if (ignore_message(type)) { return; }

        using enum message::Command;

        // NOTE update is_implemented when new messages are added
        switch (type) {
            case unknown: {
                LogError()("Received unimplemented ")(command.Describe())(
                    " command from ")(name_)
                    .Flush();
            } break;
            case addr: {
                process_protocol(command.asAddr(), monotonic);
            } break;
            case addr2: {
                process_protocol(command.asAddr2(), monotonic);
            } break;
            case block: {
                process_protocol(command.asBlock(), monotonic);
            } break;
            case cfcheckpt: {
                process_protocol(command.asCfcheckpt(), monotonic);
            } break;
            case cfheaders: {
                process_protocol(command.asCfheaders(), monotonic);
            } break;
            case cfilter: {
                process_protocol(command.asCfilter(), monotonic);
            } break;
            case getaddr: {
                process_protocol(command.asGetaddr(), monotonic);
            } break;
            case getblocks: {
                process_protocol(command.asGetblocks(), monotonic);
            } break;
            case getcfcheckpt: {
                process_protocol(command.asGetcfcheckpt(), monotonic);
            } break;
            case getcfheaders: {
                process_protocol(command.asGetcfheaders(), monotonic);
            } break;
            case getcfilters: {
                process_protocol(command.asGetcfilters(), monotonic);
            } break;
            case getdata: {
                process_protocol(command.asGetdata(), monotonic);
            } break;
            case getheaders: {
                process_protocol(command.asGetheaders(), monotonic);
            } break;
            case headers: {
                process_protocol(command.asHeaders(), monotonic);
            } break;
            case inv: {
                process_protocol(command.asInv(), monotonic);
            } break;
            case mempool: {
                process_protocol(command.asMempool(), monotonic);
            } break;
            case notfound: {
                process_protocol(command.asNotfound(), monotonic);
            } break;
            case ping: {
                process_protocol(command.asPing(), monotonic);
            } break;
            case pong: {
                process_protocol(command.asPong(), monotonic);
            } break;
            case reject: {
                process_protocol(command.asReject(), monotonic);
            } break;
            case sendaddr2: {
                process_protocol(command.asSendaddr2(), monotonic);
            } break;
            case tx: {
                process_protocol(command.asTx(), monotonic);
            } break;
            case verack: {
                process_protocol(command.asVerack(), monotonic);
            } break;
            case version: {
                process_protocol(command.asVersion(), monotonic);
            } break;
            case alert:
            case authch:
            case avahello:
            case blocktxn:
            case checkorder:
            case cmpctblock:
            case feefilter:
            case filteradd:
            case filterclear:
            case filterload:
            case getblocktxn:
            case merkleblock:
            case protoconf:
            case reply:
            case sendcmpct:
            case senddsq:
            case sendheaders2:
            case sendheaders:
            case submitorder:
            case xversion:
            default: {
                log("Received unhandled ")(print(type))(" command from ")(name_)
                    .Flush();
            }
        }
    } catch (const std::exception& e) {
        disconnect(e.what(), monotonic);
    }
}

auto Peer::process_protocol(
    message::internal::Addr& message,
    allocator_type monotonic) noexcept(false) -> void
{
    process_addresses(message.get(), monotonic);
}

auto Peer::process_protocol(
    message::internal::Addr2& message,
    allocator_type monotonic) noexcept(false) -> void
{
    process_addresses(message.get(), monotonic);
}

auto Peer::process_protocol(
    message::internal::Block& message,
    allocator_type monotonic) noexcept(false) -> void
{
    const auto data = message.get();
    update_block_job(data, monotonic);
    to_block_oracle_.SendDeferred(
        [&] {
            using enum opentxs::blockchain::node::blockoracle::Job;
            auto work = MakeWork(submit_block);
            work.AddFrame(data.data(), data.size());

            return work;
        }(),
        true);
}

auto Peer::process_protocol(
    message::internal::Cfcheckpt&,
    allocator_type) noexcept(false) -> void
{
    // TODO
}

auto Peer::process_protocol(
    message::internal::Cfheaders& message,
    allocator_type monotonic) noexcept(false) -> void
{
    switch (state()) {
        case State::verify: {
            process_protocol_verify(message, monotonic);
        } break;
        case State::run: {
            // TODO
        } break;
        default: {
        }
    }
}

auto Peer::process_protocol_verify(
    message::internal::Cfheaders& message,
    allocator_type monotonic) noexcept(false) -> void
{
    log_()(name_)(": Received checkpoint cfheader message from ")(name_)
        .Flush();
    auto postcondition = ScopeGuard{[this, monotonic] {
        if (false == verification_.got_cfheader_) {
            auto why = CString{get_allocator()};
            why.append("Disconnecting "sv);
            why.append(name_);
            why.append(" due to cfheader checkpoint failure"sv);
            disconnect(why, monotonic);
        }
    }};
    auto data = message.get();

    if (const auto count = data.size(); 1_uz != count) {
        log_()(name_)(": unexpected cfheader count: ")(count).Flush();

        return;
    }

    const auto [height, checkpointHash, parentHash, filterHash] =
        header_oracle_.Internal().GetDefaultCheckpoint();
    const auto receivedCfheader =
        opentxs::blockchain::internal::FilterHashToHeader(
            api_, data[0].Bytes(), message.Previous().Bytes());

    if (filterHash != receivedCfheader) {
        log_()(name_)(": unexpected cfheader: ")
            .asHex(receivedCfheader)(". Expected: ")
            .asHex(filterHash)
            .Flush();

        return;
    }

    log_()(name_)(": Cfheader checkpoint validated for ")(name_).Flush();
    verification_.got_cfheader_ = true;
    set_cfilter_capability(true);
    check_verification(monotonic);
}

auto Peer::process_protocol(
    message::internal::Cfilter&,
    allocator_type) noexcept(false) -> void
{
    // TODO
}

auto Peer::process_protocol(
    message::internal::Getaddr&,
    allocator_type monotonic) noexcept(false) -> void
{
    send_good_addresses(monotonic);
}

auto Peer::process_protocol(
    message::internal::Getblocks& message,
    allocator_type) noexcept(false) -> void
{
    // TODO
}

auto Peer::process_protocol(
    message::internal::Getcfcheckpt& message,
    allocator_type) noexcept(false) -> void
{
    // TODO
}

auto Peer::process_protocol(
    message::internal::Getcfheaders& message,
    allocator_type monotonic) noexcept(false) -> void
{
    const auto& stop = message.Stop();

    if (false == header_oracle_.IsInBestChain(stop)) { return; }

    const auto fromGenesis = (0 == message.Start());
    const auto blocks = header_oracle_.BestHashes(
        fromGenesis ? 0 : message.Start() - 1, stop, fromGenesis ? 2000 : 2001);
    const auto count = blocks.size();

    if (0_uz == count) { return; }

    const auto filterType = message.Type();
    const auto previousHeader =
        filter_oracle_.LoadFilterHeader(filterType, *blocks.cbegin());

    if (previousHeader.empty()) { return; }

    auto filterHashes = Vector<opentxs::blockchain::cfilter::Hash>{monotonic};
    filterHashes.reserve(count);
    filterHashes.clear();
    const auto start = fromGenesis ? 0_uz : 1_uz;
    static const auto blank = opentxs::blockchain::cfilter::Header{};
    const auto& previous = fromGenesis ? blank : previousHeader;

    for (auto i{start}; i < count; ++i) {
        const auto& blockHash = blocks.at(i);
        const auto cfilter = filter_oracle_.LoadFilter(
            filterType, blockHash, {get_allocator(), monotonic});

        if (false == cfilter.IsValid()) { break; }

        filterHashes.emplace_back(cfilter.Hash());
    }

    if (0 == filterHashes.size()) { return; }

    transmit_protocol_cfheaders(
        filterType, stop, previous, filterHashes, monotonic);
}

auto Peer::process_protocol(
    message::internal::Getcfilters& message,
    allocator_type monotonic) noexcept(false) -> void
{
    const auto& stopHash = message.Stop();
    const auto stopHeader = header_oracle_.LoadHeader(stopHash);

    if (false == stopHeader.IsValid()) {
        log_()(name_)(": skipping request with unknown stop header").Flush();

        return;
    }

    const auto startHeight{message.Start()};
    const auto stopHeight{stopHeader.Height()};

    if (startHeight > stopHeight) {
        log_()(name_)(": skipping request with malformed start height (")(
            startHeight)(") vs stop (")(stopHeight)(")")
            .Flush();

        return;
    }

    if (0 > startHeight) {
        log_()(name_)(": skipping request with negative start height (")(
            startHeight)(")")
            .Flush();

        return;
    }

    constexpr auto limit = 1000_uz;
    const auto count =
        static_cast<std::size_t>((stopHeight - startHeight) + 1u);

    if (count > limit) {
        log_()(name_)(": skipping request with excessive filter requests (")(
            count)(") vs allowed (")(limit)(")")
            .Flush();

        return;
    } else {
        log_()(name_)(": requests ")(count)(" filters from height ")(
            startHeight)(" to ")(stopHeight)(" (")
            .asHex(stopHeader.Hash())(")")
            .Flush();
    }

    const auto type = message.Type();
    const auto hashes = header_oracle_.BestHashes(startHeight, stopHash);
    const auto data = [&] {
        auto out = Vector<opentxs::blockchain::cfilter::GCS>{get_allocator()};
        out.reserve(count);
        out.clear();

        assert_true(0u == out.size());

        const auto& filters = network_.FilterOracle();

        for (const auto& hash : hashes) {
            log_()(name_)(": loading cfilter for block ")
                .asHex(stopHeader.Hash())
                .Flush();
            const auto& cfilter = out.emplace_back(filters.LoadFilter(
                type, hash, {out.get_allocator(), monotonic}));

            if (false == cfilter.IsValid()) { break; }
        }

        return out;
    }();

    if (data.size() != count) {
        LogError()()(name_)(": failed to load all filters, requested (")(
            count)("), loaded (")(data.size())(")")
            .Flush();

        return;
    }

    assert_true(data.size() == hashes.size());

    auto h{hashes.begin()};

    for (auto g{data.begin()}; g != data.end(); ++g, ++h) {
        transmit_protocol_cfilter(type, *h, *g, monotonic);
    }
}

auto Peer::process_protocol(
    message::internal::Getdata& message,
    allocator_type monotonic) noexcept(false) -> void
{
    const auto& log = log_;
    using enum Inventory::Type;
    auto notFound = Vector<Inventory>{monotonic};
    notFound.clear();

    for (const auto& inv : message.get()) {
        switch (inv.type_) {
            case MsgWitnessTx:
            case MsgTx: {
                const auto txid = Txid{inv.hash_.Bytes()};
                log()(name_)(": peer has requested transaction ")
                    .asHex(txid)
                    .Flush();
                auto tx = mempool_.Query(txid, monotonic);

                if (tx.IsValid()) {
                    log()(name_)(": sending transaction ")
                        .asHex(txid)(" to peer")
                        .Flush();
                    add_known_tx(txid);
                    const auto bytes = [&] {
                        auto out = Space{};
                        tx.Internal().asBitcoin().Serialize(writer(out));

                        return out;
                    }();
                    transmit_protocol_tx(reader(bytes), monotonic);
                } else {
                    log()(name_)(": transaction ")
                        .asHex(txid)(" not found in mempool")
                        .Flush();
                    notFound.emplace_back(inv);
                }
            } break;
            case MsgWitnessBlock:
            case MsgBlock: {
                const auto id =
                    opentxs::blockchain::block::Hash{inv.hash_.Bytes()};
                log()(name_)(": peer has requested block ").asHex(id).Flush();
                auto future = block_oracle_.Load(id);

                if (IsReady(future)) {
                    log()(name_)(": sending block ")
                        .asHex(id)(" to peer")
                        .Flush();
                    const auto block = future.get();

                    assert_true(block.IsValid());

                    add_known_block(id);
                    transmit_protocol_block(
                        [&] {
                            auto output = api_.Factory().Data();
                            block.Serialize(output.WriteInto());

                            return output;
                        }()
                            .Bytes(),
                        monotonic);
                } else {
                    log()(name_)(": block ")
                        .asHex(id)(" not found in database")
                        .Flush();
                    notFound.emplace_back(inv);
                }
            } break;
            case None:
            case MsgFilteredBlock:
            case MsgCmpctBlock:
            case MsgFilteredWitnessBlock:
            default: {
                notFound.emplace_back(inv);
            }
        }
    }

    if (false == notFound.empty()) {
        transmit_protocol_notfound(notFound, monotonic);
    }
}

auto Peer::process_protocol(
    message::internal::Getheaders& message,
    allocator_type monotonic) noexcept(false) -> void
{
    const auto parents = message.get();
    const auto hashes =
        header_oracle_.BestHashes(parents, message.Stop(), 2000_uz);
    // TODO BlockOracle::BestHashes should not return any hashes that are
    // provided as part of the first argument. Before we change that though we
    // need to check all users of that function to make sure they will keep
    // working.
    const auto exclude = [&] {
        auto out = Set<opentxs::blockchain::block::Hash>{monotonic};
        out.clear();
        std::ranges::copy(parents, std::inserter(out, out.end()));

        return out;
    }();
    const auto effective = [&] {
        auto out = decltype(hashes){monotonic};
        out.reserve(hashes.size());
        out.clear();
        std::ranges::copy_if(
            hashes, std::back_inserter(out), [&](const auto& hash) {
                return false == exclude.contains(hash);
            });

        return out;
    }();
    auto headers = [&] {
        auto out = Vector<opentxs::blockchain::block::Header>{monotonic};
        out.reserve(hashes.size());
        out.clear();
        std::ranges::transform(
            effective, std::back_inserter(out), [&](const auto& hash) -> auto {
                return header_oracle_.LoadHeader(hash);
            });

        return out;
    }();
    transmit_protocol_headers(headers, monotonic);
}

auto Peer::process_protocol(
    message::internal::Headers& message,
    allocator_type monotonic) noexcept(false) -> void
{
    switch (state()) {
        case State::verify: {
            process_protocol_verify(message, monotonic);
        } break;
        case State::run: {
            process_protocol_run(message, monotonic);
        } break;
        default: {
        }
    }
}

auto Peer::process_protocol_verify(
    message::internal::Headers& message,
    allocator_type monotonic) noexcept(false) -> void
{
    log_()(name_)(": Received checkpoint block header message from ")(name_)
        .Flush();
    auto postcondition = ScopeGuard{[this, monotonic] {
        if (false == verification_.got_block_header_) {
            auto why = CString{get_allocator()};
            why.append("Disconnecting "sv);
            why.append(name_);
            why.append(" due to block header checkpoint failure"sv);
            disconnect(why, monotonic);
        }
    }};

    const auto headers = message.get();

    if (const auto count = headers.size(); 1_uz != count) {
        log_()(name_)(": unexpected block header count: ")(count);

        for (const auto& hash : headers) { log_("\n * ").asHex(hash.Hash()); }

        log_.Flush();

        return;
    }

    const auto [height, checkpointHash, parentHash, filterHash] =
        header_oracle_.Internal().GetDefaultCheckpoint();
    const auto& receivedBlockHash = headers.front().Hash();

    if (checkpointHash != receivedBlockHash) {
        log_()(name_)(": unexpected block header hash: ")
            .asHex(receivedBlockHash)(". Expected: ")
            .asHex(checkpointHash)
            .Flush();

        return;
    }

    log_()(name_)(": Block header checkpoint validated for ")(name_).Flush();
    verification_.got_block_header_ = true;
    set_block_header_capability(true);
    check_verification(monotonic);
}

auto Peer::process_protocol_run(
    message::internal::Headers& message,
    allocator_type monotonic) noexcept(false) -> void
{
    auto headers = message.get();

    if (false == headers.empty()) {
        const auto newestID = headers.back().Hash();
        auto& internal =
            const_cast<opentxs::blockchain::node::internal::HeaderOracle&>(
                header_oracle_.Internal());

        if (internal.AddHeaders(headers)) {
            const auto header = header_oracle_.LoadHeader(newestID);

            assert_true(header.IsValid());

            update_remote_position(header.Position());
        }
    }

    update_get_headers_job(monotonic);
}

auto Peer::process_protocol(
    message::internal::Inv& message,
    allocator_type monotonic) noexcept(false) -> void
{
    const auto& log = log_;
    auto data = message.get();
    using Inv = Inventory;
    auto blocks = Vector<Inv>{monotonic};
    blocks.reserve(data.size());
    blocks.clear();
    auto transactions = Vector<Inv>{monotonic};
    transactions.reserve(data.size());
    transactions.clear();

    for (auto& inv : data) {
        const auto& hash = inv.hash_;
        log()(name_)(": received ")(inv.DisplayType())(" hash ")
            .asHex(hash)
            .Flush();
        using enum Inventory::Type;

        switch (inv.type_) {
            case MsgBlock:
            case MsgWitnessBlock: {
                blocks.emplace_back(std::move(inv));
            } break;
            case MsgTx:
            case MsgWitnessTx: {
                transactions.emplace_back(std::move(inv));
            } break;
            case None:
            case MsgFilteredBlock:
            case MsgFilteredWitnessBlock:
            case MsgCmpctBlock:
            default: {
            }
        }
    }

    process_block_hashes(blocks, monotonic);
    process_transaction_hashes(transactions, monotonic);
}

auto Peer::process_protocol(
    message::internal::Mempool& message,
    allocator_type monotonic) noexcept(false) -> void
{
    reconcile_mempool(monotonic);
}

auto Peer::process_protocol(
    message::internal::Notfound& message,
    allocator_type) noexcept(false) -> void
{
    // TODO
}

auto Peer::process_protocol(
    message::internal::Ping& message,
    allocator_type monotonic) noexcept(false) -> void
{
    const auto nonce = message.Nonce();

    if (nonce_ == nonce) {
        disconnect(
            "received ping nonce indicates connection to self", monotonic);
    } else {
        transmit_protocol_pong(nonce, monotonic);
    }
}

auto Peer::process_protocol(
    message::internal::Pong& message,
    allocator_type monotonic) noexcept(false) -> void
{
    const auto nonce = message.Nonce();

    if (nonce_ != nonce) { disconnect("invalid nonce in pong", monotonic); }
}

auto Peer::process_protocol(
    message::internal::Reject& message,
    allocator_type) noexcept(false) -> void
{
    const auto reason = [&] {
        auto out = message.Reason();

        if (valid(out)) {

            return out;
        } else {

            return "(no reason given)"sv;
        }
    }();
    LogConsole()(name_)(" rejected ")(message.RejectedMessage())(
        " message because: ")(reason)
        .Flush();
}

auto Peer::process_protocol(
    message::internal::Sendaddr2& message,
    allocator_type) noexcept(false) -> void
{
    addr_v2_ = true;
}

auto Peer::process_protocol(
    message::internal::Tx& message,
    allocator_type) noexcept(false) -> void
{
    const auto& log = log_;
    // TODO use the mempool's allocator

    if (auto tx = message.Transaction(get_allocator()); tx.IsValid()) {
        {
            const auto& id = tx.ID();
            log()(name_)(": received transaction ").asHex(id).Flush();
            add_known_tx(id);
        }
        mempool_.Submit(std::move(tx));
    } else {
        LogError()()(name_)(": unable to instantiate received transaction")
            .Flush();
    }
}

auto Peer::process_protocol(
    message::internal::Verack& message,
    allocator_type monotonic) noexcept(false) -> void
{
    if (const auto state = this->state(); State::handshake != state) {
        using enum message::Command;
        auto error = CString{get_allocator()};
        error.append("received ");
        error.append(print(verack));
        error.append(" during ");
        error.append(print_state(state));
        error.append(" state");
        disconnect(error, monotonic);
    }

    handshake_.got_verack_ = true;
    check_handshake(monotonic);
}

auto Peer::process_protocol(
    message::internal::Version& message,
    allocator_type monotonic) noexcept(false) -> void
{
    if (const auto state = this->state(); State::handshake != state) {
        using enum message::Command;
        auto error = CString{get_allocator()};
        error.append("received ");
        error.append(print(version));
        error.append(" during ");
        error.append(print_state(state));
        error.append(" state");
        disconnect(error, monotonic);
    }

    to_header_oracle_.SendDeferred([&] {
        using enum opentxs::blockchain::node::headeroracle::Job;
        auto out = MakeWork(update_remote_height);
        out.AddFrame(message.Height());

        return out;
    }());
    protocol_ = std::min(protocol_, message.ProtocolVersion());
    update_address(message.RemoteServices(monotonic));
    using enum opentxs::blockchain::Type;

    if (Dir::incoming == dir_) { transmit_protocol_version(monotonic); }

    using enum opentxs::blockchain::Category;

    switch (category(chain_)) {
        case output_based: {
            if (protocol_ >= 70015) { transmit_protocol_sendaddr2(monotonic); }
        } break;
        case unknown_category:
        case balance_based:
        default: {
        }
    }

    transmit_protocol_verack(monotonic);
    handshake_.got_version_ = true;
    check_handshake(monotonic);
}

auto Peer::process_transaction_hashes(
    std::span<Inventory> invs,
    allocator_type monotonic) noexcept -> void
{
    const auto& log = log_;
    const auto hashes = [&] {
        auto out = Vector<Txid>{monotonic};
        std::ranges::transform(
            invs, std::back_inserter(out), [&](const auto& in) {
                return in.hash_.Bytes();
            });

        return out;
    }();
    const auto mempool = mempool_.Submit(hashes, monotonic);

    assert_true(hashes.size() == invs.size());
    assert_true(hashes.size() == mempool.size());

    auto unseen = [&] {
        auto out = Vector<Inventory>{monotonic};
        out.reserve(hashes.size());
        out.clear();

        for (auto i = 0_uz; i < hashes.size(); ++i) {
            const auto& inv = invs[i];
            const auto& txid = hashes[i];
            const auto& download = mempool[i];
            add_known_tx(txid);

            if (download) {
                log()(name_)(": downloading unseen transaction ")
                    .asHex(txid)
                    .Flush();
                out.emplace_back(inv);
            } else {
                log()(name_)(": mempool already contains transaction ")
                    .asHex(txid)
                    .Flush();
            }
        }

        return out;
    }();

    if (false == unseen.empty()) {
        transmit_protocol_getdata(unseen, monotonic);
    }
}

auto Peer::reconcile_mempool(allocator_type monotonic) noexcept -> void
{
    const auto local = mempool_.Dump(monotonic);
    const auto remote = get_known_tx(monotonic);
    const auto missing = [&] {
        auto out = Vector<Txid>{monotonic};
        out.reserve(std::max(local.size(), remote.size()));
        out.clear();
        std::ranges::set_difference(local, remote, std::back_inserter(out));

        return out;
    }();
    using bitcoin::Inventory;
    auto items = [&] {
        auto out = Vector<Inventory>{monotonic};
        out.reserve(missing.size());
        out.clear();
        std::ranges::transform(
            missing,
            std::back_inserter(out),
            [this](const auto& hash) -> Inventory {
                return {inv_tx_, hash.Bytes()};
            });

        return out;
    }();
    transmit_protocol_inv(items, monotonic);
}

auto Peer::request_checkpoint_block_header(allocator_type monotonic) noexcept
    -> void
{
    auto [height, checkpointBlockHash, parentBlockHash, filterHash] =
        header_oracle_.Internal().GetDefaultCheckpoint();
    transmit_protocol_getheaders(
        std::move(parentBlockHash), checkpointBlockHash, monotonic);
}

auto Peer::request_checkpoint_cfheader(allocator_type monotonic) noexcept
    -> void
{
    auto [height, checkpointBlockHash, parentBlockHash, filterHash] =
        header_oracle_.Internal().GetDefaultCheckpoint();
    transmit_protocol_getcfheaders(height, checkpointBlockHash, monotonic);
}

auto Peer::transition_state_handshake(allocator_type monotonic) noexcept -> void
{
    Imp::transition_state_handshake(monotonic);

    if (Dir::outgoing == dir_) { transmit_protocol_version(monotonic); }
}

auto Peer::transition_state_verify(allocator_type monotonic) noexcept -> void
{
    Imp::transition_state_verify(monotonic);
    const auto& log = log_;
    const auto checks = [&] {
        auto out = 0;

        if (Dir::incoming == dir_) {
            log()(name_)(" is not required to validate checkpoints").Flush();
        } else {
            log()(name_)(" must validate block header ");
            ++out;

            if (peer_cfilter_) {
                log("and cfheader ");
                ++out;
            }

            log("checkpoints").Flush();
        }

        return out;
    }();

    switch (checks) {
        case 0: {
            transition_state_run(monotonic);
        } break;
        case 1: {
            request_checkpoint_block_header(monotonic);
        } break;
        case 2: {
            request_checkpoint_block_header(monotonic);
            request_checkpoint_cfheader(monotonic);
        } break;
        default: {
            LogAbort()().Abort();
        }
    }
}

auto Peer::transmit_addresses(
    std::span<network::blockchain::Address> addresses,
    allocator_type monotonic) noexcept -> void
{
    const auto& log = log_;
    auto out = Vector<network::blockchain::Address>{monotonic};
    out.reserve(addresses.size());
    out.clear();
    std::copy_if(
        std::make_move_iterator(addresses.begin()),
        std::make_move_iterator(addresses.end()),
        std::back_inserter(out),
        [this](const auto& addr) { return can_gossip(addr); });
    log()(name_)(": ")(out.size())(" of ")(addresses.size())(
        " received addresses are eligible for gossip")
        .Flush();

    if (out.empty()) { return; }

    constexpr auto limit = 1000_uz;

    if (out.size() > limit) {
        auto selection = decltype(out){monotonic};
        selection.reserve(limit);
        selection.clear();
        std::sample(
            std::make_move_iterator(out.begin()),
            std::make_move_iterator(out.end()),
            std::back_inserter(selection),
            limit,
            std::mt19937{std::random_device{}()});
        out.swap(selection);
    }

    assert_true(out.size() <= limit);

    add_known_address(out);

    if (addr_v2_) {
        transmit_protocol_addr2(out, monotonic);
    } else {
        transmit_protocol_addr(out, monotonic);
    }
}

auto Peer::transmit_block_hash(
    opentxs::blockchain::block::Hash&& hash,
    allocator_type monotonic) noexcept -> void
{
    using Inv = Inventory;

    transmit_protocol_inv(Inv{inv_block_, std::move(hash)}, monotonic);
}

auto Peer::transmit_ping(allocator_type monotonic) noexcept -> void
{
    transmit_protocol_ping(monotonic);
}

auto Peer::transmit_protocol_addr(
    std::span<network::blockchain::Address> addresses,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Addr;
    transmit_protocol<Type>(monotonic, protocol_, addresses);
}

auto Peer::transmit_protocol_addr2(
    std::span<network::blockchain::Address> addresses,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Addr2;
    transmit_protocol<Type>(monotonic, protocol_, addresses);
}

auto Peer::transmit_protocol_block(
    const ReadView serialized,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Block;
    transmit_protocol<Type>(monotonic, serialized);
}

auto Peer::transmit_protocol_cfheaders(
    opentxs::blockchain::cfilter::Type type,
    const opentxs::blockchain::block::Hash& stop,
    const opentxs::blockchain::cfilter::Header& previous,
    std::span<opentxs::blockchain::cfilter::Hash> hashes,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Cfheaders;
    transmit_protocol<Type>(monotonic, type, stop, previous, hashes);
}

auto Peer::transmit_protocol_cfilter(
    opentxs::blockchain::cfilter::Type type,
    const opentxs::blockchain::block::Hash& hash,
    const opentxs::blockchain::cfilter::GCS& filter,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Cfilter;
    transmit_protocol<Type>(monotonic, type, hash, filter);
}

auto Peer::transmit_protocol_getaddr(allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Getaddr;
    transmit_protocol<Type>(monotonic);
}

auto Peer::transmit_protocol_getcfheaders(
    const opentxs::blockchain::block::Height start,
    const opentxs::blockchain::block::Hash& stop,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Getcfheaders;
    transmit_protocol<Type>(
        monotonic, filter_oracle_.DefaultType(), start, stop);
}

auto Peer::transmit_protocol_getcfilters(
    const opentxs::blockchain::block::Height start,
    const opentxs::blockchain::block::Hash& stop,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Getcfilters;
    transmit_protocol<Type>(
        monotonic, filter_oracle_.DefaultType(), start, stop);
}

auto Peer::transmit_protocol_getdata(
    Inventory&& inv,
    allocator_type monotonic) noexcept -> void
{
    using bitcoin::Inventory;
    auto items = move_construct<Inventory>(span_from_object(inv), monotonic);
    transmit_protocol_getdata(items, monotonic);
}

auto Peer::transmit_protocol_getdata(
    std::span<Inventory> items,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Getdata;
    transmit_protocol<Type>(monotonic, items);
}

auto Peer::transmit_protocol_getheaders(allocator_type monotonic) noexcept
    -> void
{
    static const auto stop = opentxs::blockchain::block::Hash{};

    transmit_protocol_getheaders(stop, monotonic);
}

auto Peer::transmit_protocol_getheaders(
    const opentxs::blockchain::block::Hash& stop,
    allocator_type monotonic) noexcept -> void
{
    auto history = header_oracle_.RecentHashes();
    transmit_protocol_getheaders(history, stop, monotonic);
}

auto Peer::transmit_protocol_getheaders(
    opentxs::blockchain::block::Hash&& parent,
    const opentxs::blockchain::block::Hash& stop,
    allocator_type monotonic) noexcept -> void
{
    using opentxs::blockchain::block::Hash;
    auto history = move_construct<Hash>(span_from_object(parent), monotonic);
    transmit_protocol_getheaders(history, stop, monotonic);
}

auto Peer::transmit_protocol_getheaders(
    std::span<opentxs::blockchain::block::Hash> history,
    const opentxs::blockchain::block::Hash& stop,
    allocator_type monotonic) noexcept -> void
{
    if (history.empty() && (history.front() == stop)) { return; }

    using Type = message::internal::Getheaders;
    transmit_protocol<Type>(monotonic, protocol_, history, stop);
}

auto Peer::transmit_protocol_getheaders(
    std::span<opentxs::blockchain::block::Hash> history,
    allocator_type monotonic) noexcept -> void
{
    static const auto stop = opentxs::blockchain::block::Hash{};

    transmit_protocol_getheaders(history, stop, monotonic);
}

auto Peer::transmit_protocol_headers(
    std::span<opentxs::blockchain::block::Header> headers,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Headers;
    transmit_protocol<Type>(monotonic, headers);
}

auto Peer::transmit_protocol_inv(
    Inventory&& inv,
    allocator_type monotonic) noexcept -> void
{
    using bitcoin::Inventory;
    auto items = move_construct<Inventory>(span_from_object(inv), monotonic);
    transmit_protocol_inv(items, monotonic);
}

auto Peer::transmit_protocol_inv(
    std::span<Inventory> inv,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Inv;
    transmit_protocol<Type>(monotonic, inv);
}

auto Peer::transmit_protocol_mempool(allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Mempool;
    transmit_protocol<Type>(monotonic);
}

auto Peer::transmit_protocol_notfound(
    std::span<Inventory> payload,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Notfound;
    transmit_protocol<Type>(monotonic, payload);
}

auto Peer::transmit_protocol_ping(allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Ping;
    transmit_protocol<Type>(monotonic, nonce_);
}

auto Peer::transmit_protocol_pong(
    const message::Nonce& nonce,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Pong;
    transmit_protocol<Type>(monotonic, nonce);
}

auto Peer::transmit_protocol_sendaddr2(allocator_type monotonic) noexcept
    -> void
{
    using Type = message::internal::Sendaddr2;
    transmit_protocol<Type>(monotonic);
}

auto Peer::transmit_protocol_tx(
    ReadView serialized,
    allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Tx;
    transmit_protocol<Type>(monotonic, serialized);
}

auto Peer::transmit_protocol_verack(allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Verack;
    transmit_protocol<Type>(monotonic);
}

auto Peer::transmit_protocol_version(allocator_type monotonic) noexcept -> void
{
    using Type = message::internal::Version;
    transmit_protocol<Type>(
        monotonic,
        protocol_,
        local_address_,
        address(),
        nonce_,
        user_agent_,
        header_oracle_.BestChain().height_,
        bip37_);
}

auto Peer::transmit_request_block_headers(allocator_type monotonic) noexcept
    -> void
{
    transmit_protocol_getheaders(monotonic);
}

auto Peer::transmit_request_block_headers(
    const opentxs::blockchain::node::internal::HeaderJob& job,
    allocator_type monotonic) noexcept -> void
{
    auto history = job.Recent();
    transmit_protocol_getheaders(history, monotonic);
}

auto Peer::transmit_request_blocks(
    opentxs::blockchain::node::internal::BlockBatch& job,
    allocator_type monotonic) noexcept -> void
{
    auto blocks = [&] {
        const auto& data = job.Get();
        using bitcoin::Inventory;
        auto out = Vector<Inventory>{monotonic};
        out.reserve(data.size());
        out.clear();
        std::ranges::transform(
            data,
            std::back_inserter(out),
            [this](const auto& hash) -> Inventory {
                log_()("requesting block ").asHex(hash).Flush();

                return {inv_block_, hash};
            }

        );

        return out;
    }();
    transmit_protocol_getdata(blocks, monotonic);
}

auto Peer::transmit_request_mempool(allocator_type monotonic) noexcept -> void
{
    transmit_protocol_mempool(monotonic);
}

auto Peer::transmit_request_peers(allocator_type monotonic) noexcept -> void
{
    transmit_protocol_getaddr(monotonic);
}

auto Peer::transmit_txid(const Txid& txid, allocator_type monotonic) noexcept
    -> void
{
    using Inv = Inventory;
    transmit_protocol_inv(Inv{inv_tx_, txid.Bytes()}, monotonic);
}

Peer::~Peer() = default;
}  // namespace opentxs::network::blockchain::bitcoin
