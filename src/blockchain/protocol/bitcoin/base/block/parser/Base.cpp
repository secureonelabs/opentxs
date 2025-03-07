// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "blockchain/protocol/bitcoin/base/block/parser/Base.hpp"  // IWYU pragma: associated

#include <boost/endian/buffers.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <utility>

#include "blockchain/protocol/bitcoin/base/block/transaction/TransactionPrivate.hpp"
#include "internal/blockchain/params/ChainData.hpp"
#include "internal/blockchain/protocol/bitcoin/base/Bitcoin.hpp"
#include "internal/blockchain/protocol/bitcoin/base/block/Factory.hpp"
#include "internal/blockchain/protocol/bitcoin/base/block/Types.hpp"
#include "internal/util/P0330.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/Type.hpp"  // IWYU pragma: keep
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/blockchain/block/Hash.hpp"
#include "opentxs/blockchain/block/Transaction.hpp"
#include "opentxs/blockchain/block/TransactionHash.hpp"
#include "opentxs/blockchain/protocol/bitcoin/base/block/Header.hpp"
#include "opentxs/blockchain/protocol/bitcoin/bitcoincash/token/cashtoken/Types.internal.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/crypto/Hasher.hpp"
#include "opentxs/network/blockchain/bitcoin/CompactSize.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Writer.hpp"

namespace opentxs::blockchain::protocol::bitcoin::base::block
{
using opentxs::network::blockchain::bitcoin::DecodeCompactSize;

ParserBase::ParserBase(
    const api::Crypto& crypto,
    blockchain::Type type,
    alloc::Strategy alloc) noexcept
    : crypto_(crypto)
    , chain_(type)
    , cashtoken_(params::get(chain_).SupportsCashtoken())
    , alloc_(alloc)
    , data_()
    , bytes_()
    , header_view_()
    , header_(alloc_.result_)
    , txids_(alloc_.result_)
    , wtxids_(alloc_.result_)
    , transactions_(alloc.result_)
    , mode_(Mode::constructing)
    , verify_hash_(true)
    , block_hash_()
    , merkle_root_()
    , witness_reserved_value_()
    , segwit_commitment_()
    , transaction_count_()
    , has_segwit_commitment_(false)
    , has_segwit_transactions_(false)
    , has_segwit_reserved_value_(false)
    , dip_2_(false)
    , timestamp_()
{
}

auto ParserBase::calculate_hash(const ReadView header) noexcept -> bool
{
    return BlockHash(crypto_, chain_, header, block_hash_.WriteInto());
}

auto ParserBase::calculate_committment() const noexcept -> Hash
{
    const auto data = [&] {
        auto out = ByteArray{calculate_witness()};
        out.Concatenate(witness_reserved_value_.Bytes());

        return out;
    }();

    auto out = Hash{};

    if (false == BlockHash(crypto_, chain_, data.Bytes(), out.WriteInto())) {
        LogError()()("failed to calculate witness committment").Flush();
    }

    return out;
}

auto ParserBase::calculate_merkle() const noexcept -> Hash
{
    return CalculateMerkleValue(crypto_, chain_, txids_);
}

auto ParserBase::calculate_txids(
    bool isSegwit,
    bool isGeneration,
    bool haveWitnesses,
    opentxs::crypto::Hasher& wtxid,
    opentxs::crypto::Hasher& txid,
    EncodedTransaction* out) noexcept(false) -> void
{
    const auto& t = [&]() -> auto& {
        auto& id = txids_.emplace_back();

        if (isSegwit) {
            if (false == txid(id.WriteInto())) {
                txids_.pop_back();

                throw std::runtime_error("failed to calculate txid");
            }
        } else {
            if (false == wtxid(id.WriteInto())) {
                txids_.pop_back();

                throw std::runtime_error("failed to calculate txid");
            }
        }

        return id;
    }();
    const auto& w = [&]() -> auto& {
        auto& id = wtxids_.emplace_back();

        if (isGeneration) {
            // NOTE BIP-141: The wtxid of coinbase transaction is assumed to be
            // 0x0000....0000
        } else if (false == haveWitnesses) {
            // NOTE BIP-141: If all txins are not witness program, a
            // transaction's wtxid is equal to its txid
            id = t;
        } else if (isSegwit) {
            if (false == wtxid(id.WriteInto())) {
                txids_.pop_back();

                throw std::runtime_error("failed to calculate txid");
            }
        } else {
            id = t;
        }

        return id;
    }();

    if (nullptr != out) {
        if (false == copy(t.Bytes(), out->txid_.WriteInto())) {
            throw std::runtime_error("failed to copy txid");
        }

        if (false == copy(w.Bytes(), out->wtxid_.WriteInto())) {
            throw std::runtime_error("failed to copy wtxid");
        }
    }
}

auto ParserBase::calculate_witness() const noexcept -> Hash
{
    return CalculateMerkleValue(crypto_, chain_, wtxids_);
}

auto ParserBase::check(std::string_view message, std::size_t required) const
    noexcept(false) -> void
{
    const auto target = std::max(1_uz, required);

    if (data_.empty() || (data_.size() < target)) {
        const auto error = CString{"input too short: "}.append(message);

        throw std::runtime_error(error.c_str());
    }
}

auto ParserBase::check_dip_2() noexcept(false) -> void
{
    constexpr auto version = 4_uz;
    check("version field", version);
    const auto view = data_.substr(0_uz, version);
    dip_2_ = is_dip_2(view);
}

auto ParserBase::compare_header_to_hash(const Hash& expected) const noexcept
    -> bool
{
    if (verify_hash_) {

        return expected == block_hash_;
    } else {

        return true;
    }
}

auto ParserBase::compare_merkle_to_header() const noexcept -> bool
{
    return merkle_root_ == calculate_merkle();
}

auto ParserBase::compare_segwit_to_commitment() const noexcept -> bool
{
    return segwit_commitment_ == calculate_committment();
}

auto ParserBase::find_payload() noexcept -> bool
{
    if (auto size = DecodeCompactSize(data_); size.has_value()) {
        transaction_count_ = *size;
        txids_.reserve(transaction_count_);
        wtxids_.reserve(transaction_count_);

        if (constructing()) { transactions_.reserve(transaction_count_); }

        return true;
    } else {
        LogError()()("failed to decode transaction count").Flush();

        return false;
    }
}

auto ParserBase::get_transaction(Data data) const noexcept -> void
{
    auto& [position, encoded, out] = data;
    *out = factory::BitcoinTransaction(
        chain_, position, timestamp_, std::move(*encoded), alloc_);
}

auto ParserBase::get_transactions() noexcept(false) -> TransactionMap
{
    const auto count = transactions_.size();
    auto transactions = TransactionMap{
        count, blockchain::block::Transaction{alloc_.result_}, alloc_.result_};
    auto index = [&] {
        auto data = Vector<Data>{alloc_.result_};
        data.reserve(count);
        data.clear();

        for (auto n = 0_uz; n < count; ++n) {
            auto& in = transactions_[n];
            auto& out = transactions[n];
            data.emplace_back(n, std::addressof(in), std::addressof(out));
        }

        return data;
    }();
    get_transactions(index);

    return transactions;
}

// NOTE: https://github.com/dashpay/dips/blob/master/dip-0002.md#compatibility
auto ParserBase::is_dip_2(ReadView version) const noexcept -> bool
{
    using enum blockchain::Type;

    if ((chain_ == Dash) || (chain_ == Dash_testnet3)) {
        struct Version {
            boost::endian::little_uint16_buf_t version_{};
            boost::endian::little_uint16_buf_t type_{};
        };
        static_assert(sizeof(Version) == sizeof(std::uint32_t));

        assert_true(sizeof(Version) == version.size());

        auto decoded = Version{};
        std::memcpy(
            static_cast<void*>(std::addressof(decoded)),
            version.data(),
            version.size());

        return (decoded.version_.value() >= 3u) && (decoded.type_.value() > 0u);
    } else {

        return false;
    }
}

auto ParserBase::is_segwit_tx(EncodedTransaction* out) const noexcept -> bool
{
    const auto construct = (nullptr != out);
    const auto view = data_.substr(4_uz, 2_uz);
    const auto* marker = reinterpret_cast<const std::byte*>(view.data());
    const auto* flag = std::next(marker);
    static constexpr auto segwit = std::byte{0x0};
    const auto output = *marker == segwit;

    if (construct) {
        auto& dest = out->segwit_flag_;

        if (output) {
            dest.emplace(*flag);
        } else {
            dest.reset();
        }
    }

    return output;
}

auto ParserBase::make_index(std::span<TransactionHash> hashes) noexcept
    -> TxidIndex
{
    const auto count = hashes.size();
    auto out = TxidIndex{alloc_.result_};
    out.reserve(count);
    out.clear();

    for (auto n = 0_uz; n < count; ++n) {
        auto& hash = hashes[n];
        out.try_emplace(std::move(hash), n);
    }

    return out;
}

auto ParserBase::operator()(
    const Hash& expected,
    const ReadView bytes) && noexcept -> bool
{
    mode_ = Mode::checking;
    verify_hash_ = true;

    return parse(expected, bytes);
}

auto ParserBase::operator()(ReadView bytes, Hash& out) noexcept -> bool
{
    mode_ = Mode::checking;
    verify_hash_ = false;
    static const auto id = Hash{};
    auto val = parse(id, bytes);
    out = block_hash_;

    return val;
}

auto ParserBase::operator()(
    const Hash& expected,
    ReadView bytes,
    blockchain::block::Block& out) && noexcept -> bool
{
    mode_ = Mode::constructing;
    verify_hash_ = false;

    if (parse(expected, bytes)) {
        const auto count = transactions_.size();

        assert_true(header_.IsValid());
        assert_true(count == txids_.size());
        assert_true(count == wtxids_.size());
    } else {
        LogError()()("invalid block").Flush();

        return false;
    }

    return construct_block(out);
}

auto ParserBase::operator()(
    const std::size_t position,
    const Time& time,
    ReadView bytes,
    blockchain::block::Transaction& out) && noexcept -> bool
{
    mode_ = Mode::constructing;
    verify_hash_ = false;
    data_ = std::move(bytes);

    try {
        const auto isGeneration = (0_uz == position);

        if (false == parse_next_transaction(isGeneration)) {
            throw std::runtime_error{"failed to parse transaction"};
        }

        assert_false(transactions_.empty());

        auto& encoded = transactions_.back();
        out = factory::BitcoinTransaction(
            chain_, position, time, std::move(encoded), alloc_);

        return out.IsValid();
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();
        out = {alloc_.result_};

        return false;
    }
}

auto ParserBase::parse(const Hash& expected, ReadView bytes) noexcept -> bool
{
    const auto original = bytes;
    data_ = std::move(bytes);
    bytes_ = data_.size();

    if (data_.empty()) {
        LogError()()("empty input").Flush();

        return false;
    }

    if (false == parse_header()) {
        LogError()()("failed to parse block header").Flush();

        return false;
    }

    if (false == compare_header_to_hash(expected)) {
        LogError()()(print(chain_))(
            " block header hash does not match expected value")
            .Flush();

        return false;
    }

    if (false == find_payload()) {
        LogError()()(print(chain_))(" failed to locate transactions").Flush();

        return false;
    }

    if (false == parse_transactions()) {
        LogError()()(print(chain_))(" failed to parse transactions for block: ")
            .asHex(original)
            .Flush();

        return false;
    }

    if (const auto excess = data_.size(); 0_uz < excess) {
        LogError()()(excess)(" excess bytes remain after parsing").Flush();
    }

    if (false == compare_merkle_to_header()) {
        LogError()()(print(chain_))(
            " merkle root does not match expected value")
            .Flush();

        return false;
    }

    if (has_segwit_transactions_) {
        if (false == has_segwit_commitment_) {
            LogError()()(print(chain_))(
                " generation transaction does not contain segwit commitment")
                .Flush();

            return false;
        }

        if (false == has_segwit_reserved_value_) {
            LogError()()(print(chain_))(
                " generation transaction does not contain segwit reserved "
                "value")
                .Flush();

            return false;
        }

        if (false == compare_segwit_to_commitment()) {
            LogError()()(print(chain_))(
                " witness root hash does not match expected value")
                .Flush();

            return false;
        }
    }

    return true;
}

auto ParserBase::parse_dip_2(
    opentxs::crypto::Hasher& wtxid,
    opentxs::crypto::Hasher& txid,
    EncodedTransaction* out) noexcept(false) -> void
{
    const auto construct = nullptr != out;
    const auto size = parse_size(
        "dip2 extra bytes",
        true,
        wtxid,
        txid,
        construct ? std::addressof(out->dip_2_bytes_) : nullptr);
    check("dip2 payload", size);
    auto view = data_.substr(0_uz, size);

    if (false == wtxid(view)) {

        throw std::runtime_error{"failed to hash dip2 data for wtxid"};
    }

    if (false == txid(view)) {

        throw std::runtime_error{"failed to hash dip2 data for txid"};
    }

    if (construct) {
        auto& dest = out->dip_2_.emplace();

        if (false == copy(view, dest.WriteInto())) {

            throw std::runtime_error{"failed to extract dip2 payload"};
        }
    }

    data_.remove_prefix(size);
}

auto ParserBase::parse_header() noexcept -> bool
{
    constexpr auto header = 80_uz;
    constexpr auto merkleStart = 36_uz;

    if (data_.size() < header) {
        LogError()()("input does not contain a valid ")(print(chain_))(
            " block header")
            .Flush();

        return false;
    }

    header_view_ = data_.substr(0_uz, header);

    if (false == calculate_hash(header_view_)) {
        LogError()()("failed to calculate ")(print(chain_))(" block hash")
            .Flush();

        return false;
    }

    if (!merkle_root_.Assign(data_.substr(merkleStart, merkle_root_.size()))) {
        LogError()()("failed to extract merkle root").Flush();

        return false;
    }

    if (constructing()) {
        header_ =
            factory::BitcoinBlockHeader(crypto_, chain_, header_view_, alloc_);

        if (header_.IsValid()) {
            timestamp_ = header_.Timestamp();
        } else {
            LogError()()("failed to instantiate header").Flush();

            return false;
        }
    }

    data_.remove_prefix(header);

    return true;
}

auto ParserBase::parse_inputs(
    bool isSegwit,
    opentxs::crypto::Hasher& wtxid,
    opentxs::crypto::Hasher& txid,
    EncodedTransaction* out) noexcept(false) -> std::size_t
{
    const auto construct = nullptr != out;
    const auto count = parse_size(
        "txin count",
        isSegwit,
        wtxid,
        txid,
        construct ? std::addressof(out->input_count_) : nullptr);

    if (construct) { out->inputs_.reserve(count); }

    for (auto j = 0_uz; j < count; ++j) {
        auto* next = [&]() -> EncodedInput* {
            if (construct) {

                return std::addressof(out->inputs_.emplace_back());
            } else {

                return nullptr;
            }
        }();
        constexpr auto outpoint = 36_uz;
        check("outpoint", outpoint);
        auto view = data_.substr(0_uz, outpoint);

        if (false == wtxid(view)) {

            throw std::runtime_error{"failed to hash outpoint for wtxid"};
        }

        if (isSegwit && (false == txid(view))) {

            throw std::runtime_error{"failed to hash outpoint for txid"};
        }

        if (construct) {
            auto& dest = next->outpoint_;
            static_assert(sizeof(dest) == outpoint);
            std::memcpy(
                static_cast<void*>(std::addressof(dest)),
                data_.data(),
                outpoint);
        }

        data_.remove_prefix(outpoint);
        const auto script = parse_size(
            "script size",
            isSegwit,
            wtxid,
            txid,
            construct ? std::addressof(next->cs_) : nullptr);
        check("script", script);
        view = data_.substr(0_uz, script);

        if (false == wtxid(view)) {

            throw std::runtime_error{"failed to hash script for wtxid"};
        }

        if (isSegwit && (false == txid(view))) {

            throw std::runtime_error{"failed to hash script for txid"};
        }

        if (construct && (false == copy(view, next->script_.WriteInto()))) {
            throw std::runtime_error{"failed to copy script opcodes"};
        }

        data_.remove_prefix(script);
        constexpr auto sequence = 4_uz;
        check("sequence", sequence);
        view = data_.substr(0_uz, sequence);

        if (false == wtxid(view)) {

            throw std::runtime_error{"failed to hash sequence for wtxid"};
        }

        if (isSegwit && (false == txid(view))) {

            throw std::runtime_error{"failed to hash sequence for txid"};
        }

        if (construct) {
            auto& dest = next->sequence_;
            static_assert(sizeof(dest) == sequence);
            std::memcpy(
                static_cast<void*>(std::addressof(dest)),
                data_.data(),
                sequence);
        }

        data_.remove_prefix(sequence);
    }

    return count;
}

auto ParserBase::parse_locktime(
    bool isSegwit,
    opentxs::crypto::Hasher& wtxid,
    opentxs::crypto::Hasher& txid,
    EncodedTransaction* out) noexcept(false) -> void
{
    constexpr auto locktime = 4_uz;
    check("lock time", locktime);
    const auto view = data_.substr(0_uz, locktime);

    if (false == wtxid(view)) {

        throw std::runtime_error{"failed to hash locktime for wtxid"};
    }

    if (isSegwit && (false == txid(view))) {

        throw std::runtime_error{"failed to hash locktime for txid"};
    }

    if (nullptr != out) {
        auto& dest = out->lock_time_;
        static_assert(sizeof(dest) == locktime);
        std::memcpy(
            static_cast<void*>(std::addressof(dest)), data_.data(), locktime);
    }

    data_.remove_prefix(locktime);
}

auto ParserBase::parse_next_transaction(const bool isGeneration) noexcept
    -> bool
{
    try {
        const auto minimumSize = 10_uz;

        if (data_.size() < minimumSize) {

            throw std::runtime_error{
                "input too small to be a valid transaction"};
        }

        auto* encoded = [this]() -> EncodedTransaction* {
            if (constructing()) {
                auto& next = transactions_.emplace_back();

                return std::addressof(next);
            } else {

                return nullptr;
            }
        }();
        auto wtxid = opentxs::blockchain::TransactionHasher(crypto_, chain_);
        auto txid = opentxs::blockchain::TransactionHasher(crypto_, chain_);
        check_dip_2();
        const auto isSegwit = [&] {
            if (dip_2_) {

                return false;
            } else if (is_segwit_tx(encoded)) {
                has_segwit_transactions_ = true;

                return true;
            } else {

                return false;
            }
        }();
        parse_version(isSegwit, wtxid, txid, encoded);

        if (isSegwit) {
            constexpr auto markerAndFlag = 2_uz;
            const auto view = data_.substr(0_uz, markerAndFlag);

            if (false == wtxid(view)) {

                throw std::runtime_error{
                    "failed to hash segwit marker and flag"};
            }

            data_.remove_prefix(markerAndFlag);
        }

        const auto txinCount = parse_inputs(isSegwit, wtxid, txid, encoded);
        parse_outputs(isGeneration, isSegwit, wtxid, txid, encoded);
        const auto haveWitnesses = [&] {
            if (isSegwit) {

                return parse_witnesses(isGeneration, txinCount, wtxid, encoded);
            } else {

                return false;
            }
        }();
        parse_locktime(isSegwit, wtxid, txid, encoded);

        if (dip_2_) { parse_dip_2(wtxid, txid, encoded); }

        calculate_txids(
            isSegwit, isGeneration, haveWitnesses, wtxid, txid, encoded);

        return true;
    } catch (const std::exception& e) {
        LogError()()(e.what()).Flush();

        return false;
    }
}

auto ParserBase::parse_outputs(
    bool isGen,
    bool isSegwit,
    opentxs::crypto::Hasher& wtxid,
    opentxs::crypto::Hasher& txid,
    EncodedTransaction* out) noexcept(false) -> void
{
    const auto construct = nullptr != out;
    const auto count = parse_size(
        "txout count",
        isSegwit,
        wtxid,
        txid,
        construct ? std::addressof(out->output_count_) : nullptr);

    if (construct) { out->outputs_.reserve(count); }

    for (auto j = 0_uz; j < count; ++j) {
        auto* next = [&]() -> EncodedOutput* {
            if (construct) {

                return std::addressof(out->outputs_.emplace_back());
            } else {

                return nullptr;
            }
        }();
        constexpr auto value = 8_uz;
        check("value", value);
        auto view = data_.substr(0_uz, value);

        if (false == wtxid(view)) {

            throw std::runtime_error{"failed to hash value for wtxid"};
        }

        if (isSegwit && (false == txid(view))) {

            throw std::runtime_error{"failed to hash value for txid"};
        }

        if (construct) {
            auto& dest = next->value_;
            static_assert(sizeof(dest) == value);
            std::memcpy(
                static_cast<void*>(std::addressof(dest)), data_.data(), value);
        }

        data_.remove_prefix(value);
        const auto script = parse_size(
            "script size",
            isSegwit,
            wtxid,
            txid,
            construct ? std::addressof(next->cs_) : nullptr);
        check("script", script);
        view = data_.substr(0_uz, script);

        if (false == wtxid(view)) {

            throw std::runtime_error{"failed to hash value for script"};
        }

        if (isSegwit && (false == txid(view))) {

            throw std::runtime_error{"failed to hash value for script"};
        }

        if (false == parse_segwit_commitment(isGen, view)) {

            throw std::runtime_error("failed to parse segwit commitment");
        }

        if (construct) {
            if (cashtoken_) {
                bitcoincash::token::cashtoken::deserialize(
                    view, next->cashtoken_);
            }

            if (false == copy(view, next->script_.WriteInto())) {
                throw std::runtime_error{"failed to copy script opcodes"};
            }
        }

        data_.remove_prefix(script);
    }
}

auto ParserBase::parse_segwit_commitment(
    bool isGeneration,
    const ReadView script) noexcept -> bool
{
    if (false == isGeneration) { return true; }

    constexpr auto minimum = 38_uz;

    if (script.size() < minimum) { return true; }

    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    static constexpr std::uint8_t prefix[] = {
        0x6a, 0x24, 0xaa, 0x21, 0xa9, 0xed};

    if (0 == std::memcmp(prefix, script.data(), sizeof(prefix))) {
        const auto rc = segwit_commitment_.Assign(
            script.substr(sizeof(prefix), segwit_commitment_.size()));

        if (false == rc) { return false; }

        has_segwit_commitment_ = true;
    }

    return true;
}

auto ParserBase::parse_size(
    std::string_view message,
    bool isSegwit,
    opentxs::crypto::Hasher& wtxid,
    opentxs::crypto::Hasher& txid,
    CompactSize* out) noexcept(false) -> std::size_t
{
    auto view = ReadView{};

    if (auto size = DecodeCompactSize(data_, view, out); size.has_value()) {
        if (false == wtxid(view)) {
            const auto error =
                CString{"failed to hash "}.append(message).append(" for wtxid");

            throw std::runtime_error(error.c_str());
        }

        if (isSegwit && (false == txid(view))) {
            const auto error =
                CString{"failed to hash "}.append(message).append(" for txid");

            throw std::runtime_error(error.c_str());
        }

        return size.value();
    } else {
        const auto error = CString{"failed to decode: "}.append(message);

        throw std::runtime_error(error.c_str());
    }
}

auto ParserBase::parse_size(std::string_view message) noexcept(false)
    -> std::size_t
{
    auto null = opentxs::blockchain::TransactionHasher(crypto_, chain_);

    return parse_size_segwit(message, null, nullptr);
}

auto ParserBase::parse_size_segwit(
    std::string_view message,
    opentxs::crypto::Hasher& wtxid,
    CompactSize* out) noexcept(false) -> std::size_t
{
    static auto null = opentxs::crypto::Hasher{};

    return parse_size(message, false, wtxid, null, out);
}

auto ParserBase::parse_transactions() noexcept -> bool
{
    for (auto i = 0_uz; i < transaction_count_; ++i) {
        if (false == parse_next_transaction(0_uz == i)) {
            LogError()()("failed to parse transaction ")(i + 1)(" of ")(
                transaction_count_)
                .Flush();

            return false;
        }
    }

    return true;
}

auto ParserBase::parse_version(
    bool isSegwit,
    opentxs::crypto::Hasher& wtxid,
    opentxs::crypto::Hasher& txid,
    EncodedTransaction* out) noexcept(false) -> void
{
    constexpr auto version = 4_uz;
    check("version field", version);
    const auto view = data_.substr(0_uz, version);

    if (false == wtxid(view)) {

        throw std::runtime_error{"failed to hash version for wtxid"};
    }

    if (isSegwit && (false == txid(view))) {

        throw std::runtime_error{"failed to hash version for txid"};
    }

    if (nullptr != out) {
        auto& dest = out->version_;
        static_assert(sizeof(dest) == version);
        std::memcpy(std::addressof(dest), data_.data(), version);
    }

    data_.remove_prefix(version);
}

auto ParserBase::parse_witnesses(
    bool isGeneration,
    std::size_t count,
    opentxs::crypto::Hasher& wtxid,
    EncodedTransaction* out) noexcept(false) -> bool
{
    const auto construct = nullptr != out;
    auto haveWitnesses{false};

    if (construct) { out->witnesses_.reserve(count); }

    for (auto j = 0_uz; j < count; ++j) {
        auto* input = [&]() -> EncodedInputWitness* {
            if (construct) {

                return std::addressof(out->witnesses_.emplace_back());
            } else {

                return nullptr;
            }
        }();
        const auto items = parse_size_segwit(
            "witness item count",
            wtxid,
            construct ? std::addressof(input->cs_) : nullptr);

        if (0_uz < items) { haveWitnesses = true; }

        for (auto k = 0_uz; k < items; ++k) {
            auto* next = [&]() -> EncodedWitnessItem* {
                if (construct) {
                    auto& dest = input->items_;
                    dest.reserve(items);

                    return std::addressof(dest.emplace_back());
                } else {

                    return nullptr;
                }
            }();
            const auto witness = parse_size_segwit(
                "witness size",
                wtxid,
                construct ? std::addressof(next->cs_) : nullptr);
            check("witness", witness);
            auto view = data_.substr(0_uz, witness);

            if (false == wtxid(view)) {

                throw std::runtime_error{"failed to hash witness item"};
            }

            if (construct && (false == copy(view, next->item_.WriteInto()))) {
                throw std::runtime_error{"failed to copy witness item"};
            }

            const auto size = witness_reserved_value_.size();
            const auto witnessReservedValue =
                isGeneration && (0_uz == j) && (0_uz == k) && (size == witness);

            if (witnessReservedValue) {
                if (witness_reserved_value_.Assign(data_.substr(0_uz, size))) {
                    has_segwit_reserved_value_ = true;
                } else {

                    throw std::runtime_error(
                        "failed to assign witness reserved value");
                }
            }

            data_.remove_prefix(witness);
        }
    }

    return haveWitnesses;
}

ParserBase::~ParserBase() = default;
}  // namespace opentxs::blockchain::protocol::bitcoin::base::block
