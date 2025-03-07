// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "internal/network/blockchain/bitcoin/message/Message.hpp"  // IWYU pragma: associated

#include <cstdint>
#include <limits>
#include <utility>

#include "internal/network/blockchain/bitcoin/message/Addr.hpp"
#include "internal/network/blockchain/bitcoin/message/Addr2.hpp"
#include "internal/network/blockchain/bitcoin/message/Block.hpp"
#include "internal/network/blockchain/bitcoin/message/Cfcheckpt.hpp"
#include "internal/network/blockchain/bitcoin/message/Cfheaders.hpp"
#include "internal/network/blockchain/bitcoin/message/Cfilter.hpp"
#include "internal/network/blockchain/bitcoin/message/Getaddr.hpp"
#include "internal/network/blockchain/bitcoin/message/Getblocks.hpp"
#include "internal/network/blockchain/bitcoin/message/Getcfcheckpt.hpp"
#include "internal/network/blockchain/bitcoin/message/Getcfheaders.hpp"
#include "internal/network/blockchain/bitcoin/message/Getcfilters.hpp"
#include "internal/network/blockchain/bitcoin/message/Getdata.hpp"
#include "internal/network/blockchain/bitcoin/message/Getheaders.hpp"
#include "internal/network/blockchain/bitcoin/message/Headers.hpp"
#include "internal/network/blockchain/bitcoin/message/Inv.hpp"
#include "internal/network/blockchain/bitcoin/message/Mempool.hpp"
#include "internal/network/blockchain/bitcoin/message/Notfound.hpp"
#include "internal/network/blockchain/bitcoin/message/Ping.hpp"
#include "internal/network/blockchain/bitcoin/message/Pong.hpp"
#include "internal/network/blockchain/bitcoin/message/Reject.hpp"
#include "internal/network/blockchain/bitcoin/message/Sendaddr2.hpp"
#include "internal/network/blockchain/bitcoin/message/Tx.hpp"
#include "internal/network/blockchain/bitcoin/message/Types.hpp"
#include "internal/network/blockchain/bitcoin/message/Verack.hpp"
#include "internal/network/blockchain/bitcoin/message/Version.hpp"
#include "internal/util/PMR.hpp"
#include "network/blockchain/bitcoin/message/base/MessagePrivate.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/util/Log.hpp"

namespace opentxs::network::blockchain::bitcoin::message::internal
{
Message::Message(MessagePrivate* imp) noexcept
    : imp_(std::move(imp))
{
    assert_false(nullptr == imp_);
}

Message::Message(allocator_type alloc) noexcept
    : Message(MessagePrivate::Blank(alloc))
{
}

Message::Message(const Message& rhs, allocator_type alloc) noexcept
    : Message(rhs.imp_->clone(alloc))
{
}

Message::Message(Message&& rhs) noexcept
    : Message(std::exchange(rhs.imp_, nullptr))
{
}

Message::Message(Message&& rhs, allocator_type alloc) noexcept
    : imp_(nullptr)
{
    pmr::move_construct(imp_, rhs.imp_, alloc);
}

auto Message::asAddr2() const& noexcept -> const Addr2&
{
    return imp_->asAddr2Public();
}

auto Message::asAddr2() & noexcept -> Addr2& { return imp_->asAddr2Public(); }

auto Message::asAddr2() && noexcept -> Addr2
{
    return std::exchange(imp_, nullptr);
}

auto Message::asAddr() const& noexcept -> const Addr&
{
    return imp_->asAddrPublic();
}

auto Message::asAddr() & noexcept -> Addr& { return imp_->asAddrPublic(); }

auto Message::asAddr() && noexcept -> Addr
{
    return std::exchange(imp_, nullptr);
}

auto Message::asBlock() const& noexcept -> const Block&
{
    return imp_->asBlockPublic();
}

auto Message::asBlock() & noexcept -> Block& { return imp_->asBlockPublic(); }

auto Message::asBlock() && noexcept -> Block
{
    return std::exchange(imp_, nullptr);
}

auto Message::asCfcheckpt() const& noexcept -> const Cfcheckpt&
{
    return imp_->asCfcheckptPublic();
}

auto Message::asCfcheckpt() & noexcept -> Cfcheckpt&
{
    return imp_->asCfcheckptPublic();
}

auto Message::asCfcheckpt() && noexcept -> Cfcheckpt
{
    return std::exchange(imp_, nullptr);
}

auto Message::asCfheaders() const& noexcept -> const Cfheaders&
{
    return imp_->asCfheadersPublic();
}

auto Message::asCfheaders() & noexcept -> Cfheaders&
{
    return imp_->asCfheadersPublic();
}

auto Message::asCfheaders() && noexcept -> Cfheaders
{
    return std::exchange(imp_, nullptr);
}

auto Message::asCfilter() const& noexcept -> const Cfilter&
{
    return imp_->asCfilterPublic();
}

auto Message::asCfilter() & noexcept -> Cfilter&
{
    return imp_->asCfilterPublic();
}

auto Message::asCfilter() && noexcept -> Cfilter
{
    return std::exchange(imp_, nullptr);
}

auto Message::asGetaddr() const& noexcept -> const Getaddr&
{
    return imp_->asGetaddrPublic();
}

auto Message::asGetaddr() & noexcept -> Getaddr&
{
    return imp_->asGetaddrPublic();
}

auto Message::asGetaddr() && noexcept -> Getaddr
{
    return std::exchange(imp_, nullptr);
}

auto Message::asGetblocks() const& noexcept -> const Getblocks&
{
    return imp_->asGetblocksPublic();
}

auto Message::asGetblocks() & noexcept -> Getblocks&
{
    return imp_->asGetblocksPublic();
}

auto Message::asGetblocks() && noexcept -> Getblocks
{
    return std::exchange(imp_, nullptr);
}

auto Message::asGetcfcheckpt() const& noexcept -> const Getcfcheckpt&
{
    return imp_->asGetcfcheckptPublic();
}

auto Message::asGetcfcheckpt() & noexcept -> Getcfcheckpt&
{
    return imp_->asGetcfcheckptPublic();
}

auto Message::asGetcfcheckpt() && noexcept -> Getcfcheckpt
{
    return std::exchange(imp_, nullptr);
}

auto Message::asGetcfheaders() const& noexcept -> const Getcfheaders&
{
    return imp_->asGetcfheadersPublic();
}

auto Message::asGetcfheaders() & noexcept -> Getcfheaders&
{
    return imp_->asGetcfheadersPublic();
}

auto Message::asGetcfheaders() && noexcept -> Getcfheaders
{
    return std::exchange(imp_, nullptr);
}

auto Message::asGetcfilters() const& noexcept -> const Getcfilters&
{
    return imp_->asGetcfiltersPublic();
}

auto Message::asGetcfilters() & noexcept -> Getcfilters&
{
    return imp_->asGetcfiltersPublic();
}

auto Message::asGetcfilters() && noexcept -> Getcfilters
{
    return std::exchange(imp_, nullptr);
}

auto Message::asGetdata() const& noexcept -> const Getdata&
{
    return imp_->asGetdataPublic();
}

auto Message::asGetdata() & noexcept -> Getdata&
{
    return imp_->asGetdataPublic();
}

auto Message::asGetdata() && noexcept -> Getdata
{
    return std::exchange(imp_, nullptr);
}

auto Message::asGetheaders() const& noexcept -> const Getheaders&
{
    return imp_->asGetheadersPublic();
}

auto Message::asGetheaders() & noexcept -> Getheaders&
{
    return imp_->asGetheadersPublic();
}

auto Message::asGetheaders() && noexcept -> Getheaders
{
    return std::exchange(imp_, nullptr);
}

auto Message::asHeaders() const& noexcept -> const Headers&
{
    return imp_->asHeadersPublic();
}

auto Message::asHeaders() & noexcept -> Headers&
{
    return imp_->asHeadersPublic();
}

auto Message::asHeaders() && noexcept -> Headers
{
    return std::exchange(imp_, nullptr);
}

auto Message::asInv() const& noexcept -> const Inv&
{
    return imp_->asInvPublic();
}

auto Message::asInv() & noexcept -> Inv& { return imp_->asInvPublic(); }

auto Message::asInv() && noexcept -> Inv
{
    return std::exchange(imp_, nullptr);
}

auto Message::asMempool() const& noexcept -> const Mempool&
{
    return imp_->asMempoolPublic();
}

auto Message::asMempool() & noexcept -> Mempool&
{
    return imp_->asMempoolPublic();
}

auto Message::asMempool() && noexcept -> Mempool
{
    return std::exchange(imp_, nullptr);
}

auto Message::asNotfound() const& noexcept -> const Notfound&
{
    return imp_->asNotfoundPublic();
}

auto Message::asNotfound() & noexcept -> Notfound&
{
    return imp_->asNotfoundPublic();
}

auto Message::asNotfound() && noexcept -> Notfound
{
    return std::exchange(imp_, nullptr);
}

auto Message::asPing() const& noexcept -> const Ping&
{
    return imp_->asPingPublic();
}

auto Message::asPing() & noexcept -> Ping& { return imp_->asPingPublic(); }

auto Message::asPing() && noexcept -> Ping
{
    return std::exchange(imp_, nullptr);
}

auto Message::asPong() const& noexcept -> const Pong&
{
    return imp_->asPongPublic();
}

auto Message::asPong() & noexcept -> Pong& { return imp_->asPongPublic(); }

auto Message::asPong() && noexcept -> Pong
{
    return std::exchange(imp_, nullptr);
}

auto Message::asReject() const& noexcept -> const Reject&
{
    return imp_->asRejectPublic();
}

auto Message::asReject() & noexcept -> Reject&
{
    return imp_->asRejectPublic();
}

auto Message::asReject() && noexcept -> Reject
{
    return std::exchange(imp_, nullptr);
}

auto Message::asSendaddr2() const& noexcept -> const Sendaddr2&
{
    return imp_->asSendaddr2Public();
}

auto Message::asSendaddr2() & noexcept -> Sendaddr2&
{
    return imp_->asSendaddr2Public();
}

auto Message::asSendaddr2() && noexcept -> Sendaddr2
{
    return std::exchange(imp_, nullptr);
}

auto Message::asTx() const& noexcept -> const Tx& { return imp_->asTxPublic(); }

auto Message::asTx() & noexcept -> Tx& { return imp_->asTxPublic(); }

auto Message::asTx() && noexcept -> Tx { return std::exchange(imp_, nullptr); }

auto Message::asVerack() const& noexcept -> const Verack&
{
    return imp_->asVerackPublic();
}

auto Message::asVerack() & noexcept -> Verack&
{
    return imp_->asVerackPublic();
}

auto Message::asVerack() && noexcept -> Verack
{
    return std::exchange(imp_, nullptr);
}

auto Message::asVersion() const& noexcept -> const Version&
{
    return imp_->asVersionPublic();
}

auto Message::asVersion() & noexcept -> Version&
{
    return imp_->asVersionPublic();
}

auto Message::asVersion() && noexcept -> Version
{
    return std::exchange(imp_, nullptr);
}

auto Message::Blank() noexcept -> Message&
{
    static auto blank = Message{};

    return blank;
}

auto Message::Command() const noexcept -> message::Command
{
    return imp_->Command();
}

auto Message::Describe() const noexcept -> ReadView { return imp_->Describe(); }

auto Message::get_allocator() const noexcept -> allocator_type
{
    return imp_->get_allocator();
}

auto Message::IsValid() const noexcept -> bool { return imp_->IsValid(); }

auto Message::MaxPayload() -> std::size_t
{
    static_assert(
        std::numeric_limits<std::size_t>::max() >=
        std::numeric_limits<std::uint32_t>::max());

    return std::numeric_limits<std::uint32_t>::max();
}

auto Message::Network() const noexcept -> opentxs::blockchain::Type
{
    return imp_->Network();
}

auto Message::operator=(const Message& rhs) noexcept -> Message&
{
    // NOLINTNEXTLINE(misc-unconventional-assign-operator)
    return pmr::copy_assign_base(this, imp_, rhs.imp_);
}

auto Message::operator=(Message&& rhs) noexcept -> Message&
{
    // NOLINTNEXTLINE(misc-unconventional-assign-operator)
    return pmr::move_assign_base(*this, rhs, imp_, rhs.imp_);
}

auto Message::swap(Message& rhs) noexcept -> void { pmr::swap(imp_, rhs.imp_); }

auto Message::Transmit(Transport type, zeromq::Message& out) const
    noexcept(false) -> void
{
    imp_->Transmit(type, out);
}

Message::~Message() { pmr::destroy(imp_); }
}  // namespace opentxs::network::blockchain::bitcoin::message::internal
