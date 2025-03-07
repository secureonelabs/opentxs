// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "blockchain/cfilter/GCSImp.hpp"  // IWYU pragma: associated

#include <boost/multiprecision/cpp_int.hpp>
#include <opentxs/protobuf/GCS.pb.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "internal/blockchain/Blockchain.hpp"
#include "internal/blockchain/cfilter/GCS.hpp"
#include "internal/util/Bytes.hpp"
#include "internal/util/P0330.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/crypto/Hash.hpp"
#include "opentxs/api/session/Crypto.hpp"
#include "opentxs/blockchain/cfilter/FilterType.hpp"  // IWYU pragma: keep
#include "opentxs/blockchain/cfilter/Hash.hpp"
#include "opentxs/blockchain/cfilter/Header.hpp"
#include "opentxs/blockchain/cfilter/Types.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/crypto/HashType.hpp"  // IWYU pragma: keep
#include "opentxs/crypto/Types.hpp"
#include "opentxs/network/blockchain/bitcoin/CompactSize.hpp"
#include "opentxs/protobuf/Types.internal.hpp"
#include "opentxs/util/Allocator.hpp"
#include "opentxs/util/Bytes.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Numbers.hpp"
#include "opentxs/util/WriteBuffer.hpp"
#include "opentxs/util/Writer.hpp"
#include "util/Container.hpp"

namespace opentxs
{
constexpr auto bitmask(const std::uint64_t n) -> std::uint64_t
{
    return (1u << n) - 1u;
}

constexpr auto range(std::uint32_t N, std::uint32_t M) noexcept -> gcs::Range
{
    return gcs::Range{N} * gcs::Range{M};
}
}  // namespace opentxs

namespace opentxs::gcs
{
using BitReader = blockchain::internal::BitReader;
using BitWriter = blockchain::internal::BitWriter;

static auto golomb_decode(const std::uint8_t P, BitReader& stream) noexcept(
    false) -> Delta
{
    auto quotient = Delta{0};

    while (1 == stream.read(1)) { quotient++; }

    auto remainder = stream.read(P);

    return Delta{(quotient << P) + remainder};
}

static auto golomb_encode(
    const std::uint8_t P,
    const Delta value,
    BitWriter& stream) noexcept -> void
{
    auto remainder = Delta{value & bitmask(P)};
    auto quotient = Delta{value >> P};

    while (quotient > 0) {
        stream.write(1, 1);
        --quotient;
    }

    stream.write(1, 0);
    stream.write(P, remainder);
}

auto GolombDecode(
    const std::uint32_t N,
    const std::uint8_t P,
    const Vector<std::byte>& encoded,
    alloc::Default alloc) noexcept(false) -> Elements
{
    auto output = Elements{alloc};
    auto stream = BitReader{encoded};
    auto last = Element{0};

    for (auto i = 0_uz; i < N; ++i) {
        auto delta = golomb_decode(P, stream);
        auto value = last + delta;
        output.emplace_back(value);
        last = value;
    }

    return output;
}

auto GolombEncode(
    const std::uint8_t P,
    const Elements& hashedSet,
    alloc::Default alloc) noexcept(false) -> Vector<std::byte>
{
    auto output = Vector<std::byte>{alloc};
    output.reserve(hashedSet.size() * P * 2u);
    auto stream = BitWriter{output};
    auto last = Element{0};

    for (const auto& item : hashedSet) {
        auto delta = Delta{item - last};

        if (delta != 0) { golomb_encode(P, delta, stream); }

        last = item;
    }

    stream.flush();

    return output;
}

auto HashToRange(
    const api::Session& api,
    const ReadView key,
    const Range range,
    const ReadView item) noexcept(false) -> Element
{
    return HashToRange(range, Siphash(api, key, item));
}

auto HashToRange(const Range range, const Hash hash) noexcept(false) -> Element
{
    namespace mp = boost::multiprecision;

    return ((mp::uint128_t{hash} * mp::uint128_t{range}) >> 64u)
        .convert_to<Element>();
}

auto HashedSetConstruct(
    const api::Session& api,
    const ReadView key,
    const std::uint32_t N,
    const std::uint32_t M,
    const blockchain::cfilter::Targets& items,
    alloc::Default alloc) noexcept(false) -> Elements
{
    auto output = Elements{alloc};
    std::ranges::transform(
        items, std::back_inserter(output), [&](const auto& item) {
            return HashToRange(api, key, range(N, M), item);
        });
    std::ranges::sort(output);

    return output;
}

auto Siphash(
    const api::Session& api,
    const ReadView key,
    const ReadView item) noexcept(false) -> Hash
{
    if (16 != key.size()) { throw std::runtime_error("Invalid key"); }

    auto output = Hash{};
    auto writer = preallocated(sizeof(output), &output);

    if (false ==
        api.Crypto().Hash().HMAC(
            crypto::HashType::SipHash24, key, item, std::move(writer))) {
        throw std::runtime_error("siphash failed");
    }

    return output;
}
}  // namespace opentxs::gcs

namespace opentxs::blockchain::cfilter::implementation
{
GCS::GCS(
    const VersionNumber version,
    const api::Session& api,
    const std::uint8_t bits,
    const std::uint32_t fpRate,
    const std::uint32_t count,
    std::optional<gcs::Elements>&& elements,
    Vector<std::byte>&& compressed,
    ReadView key,
    allocator_type alloc) noexcept(false)
    : GCSPrivate(alloc)
    , version_(version)
    , api_(api)
    , bits_(bits)
    , false_positive_rate_(fpRate)
    , count_(count)
    , key_()
    , compressed_(std::move(compressed), alloc)
    , elements_(std::move(elements))
{
    static_assert(16u == sizeof(key_));

    if (false == copy(key, writer(const_cast<Key&>(key_)))) {
        throw std::runtime_error(
            "Invalid key size: " + std::to_string(key.size()));
    }
}

GCS::GCS(
    const api::Session& api,
    const std::uint8_t bits,
    const std::uint32_t fpRate,
    const std::uint32_t count,
    const ReadView key,
    const ReadView encoded,
    allocator_type alloc) noexcept(false)
    : GCS(
          1,
          api,
          bits,
          fpRate,
          count,
          std::nullopt,
          [&] {
              auto out = Vector<std::byte>{alloc};
              copy(encoded, writer(out));

              return out;
          }(),
          key,
          alloc)
{
}

GCS::GCS(
    const api::Session& api,
    const std::uint8_t bits,
    const std::uint32_t fpRate,
    const std::uint32_t count,
    const ReadView key,
    Vector<std::byte>&& encoded,
    allocator_type alloc) noexcept(false)
    : GCS(1,
          api,
          bits,
          fpRate,
          count,
          std::nullopt,
          std::move(encoded),
          key,
          alloc)
{
}

GCS::GCS(
    const api::Session& api,
    const std::uint8_t bits,
    const std::uint32_t fpRate,
    const std::uint32_t count,
    const ReadView key,
    gcs::Elements&& hashed,
    Vector<std::byte>&& compressed,
    allocator_type alloc) noexcept(false)
    : GCS(1,
          api,
          bits,
          fpRate,
          count,
          std::move(hashed),
          std::move(compressed),
          key,
          alloc)
{
}

GCS::GCS(const GCS& rhs, allocator_type alloc) noexcept
    : GCS(
          rhs.version_,
          rhs.api_,
          rhs.bits_,
          rhs.false_positive_rate_,
          rhs.count_,
          [&]() -> std::optional<gcs::Elements> {
              if (rhs.elements_.has_value()) {

                  return gcs::Elements{rhs.elements_.value(), alloc};
              } else {

                  return std::nullopt;
              }
          }(),
          Vector<std::byte>{rhs.compressed_, alloc},
          reader(rhs.key_),
          alloc)
{
}

auto GCS::Compressed(Writer&& out) const noexcept -> bool
{
    return copy(reader(compressed_), std::move(out));
}

auto GCS::decompress() const noexcept -> const gcs::Elements&
{
    if (false == elements_.has_value()) {
        auto& set = elements_;
        set = gcs::GolombDecode(count_, bits_, compressed_, alloc_);
        std::ranges::sort(set.value());
    }

    return elements_.value();
}

auto GCS::Encode(Writer&& cb) const noexcept -> bool
{
    using CompactSize = network::blockchain::bitcoin::CompactSize;
    const auto bytes = CompactSize{count_}.Encode();
    const auto max = std::numeric_limits<std::size_t>::max() - bytes.size();

    if (max < compressed_.size()) {
        LogError()()("filter is too large to encode").Flush();

        return false;
    }

    const auto target = bytes.size() + compressed_.size();
    auto out = cb.Reserve(target);

    if (false == out.IsValid(target)) {
        LogError()()("failed to allocate space for output").Flush();

        return false;
    }

    auto* i = out.as<std::byte>();
    std::memcpy(i, bytes.data(), bytes.size());
    std::advance(i, bytes.size());

    if (0_uz < compressed_.size()) {
        std::memcpy(i, compressed_.data(), compressed_.size());
        std::advance(i, compressed_.size());
    }

    return true;
}

auto GCS::Hash() const noexcept -> cfilter::Hash
{
    auto preimage = Vector<std::byte>{get_allocator()};
    Encode(writer(preimage));

    return blockchain::internal::FilterToHash(api_, reader(preimage));
}

auto GCS::hashed_set_construct(
    const Vector<ByteArray>& elements,
    allocator_type alloc) const noexcept -> gcs::Elements
{
    return hashed_set_construct(transform(elements, alloc), alloc);
}

auto GCS::hashed_set_construct(
    const Vector<Space>& elements,
    allocator_type alloc) const noexcept -> gcs::Elements
{
    return hashed_set_construct(transform(elements, alloc), alloc);
}

auto GCS::hashed_set_construct(const gcs::Hashes& targets, allocator_type alloc)
    const noexcept -> gcs::Elements
{
    auto out = gcs::Elements{alloc};
    out.reserve(targets.size());
    const auto range = Range();
    std::ranges::transform(
        targets, std::back_inserter(out), [&](const auto& hash) {
            return gcs::HashToRange(range, hash);
        });

    return out;
}

auto GCS::hashed_set_construct(const Targets& elements, allocator_type alloc)
    const noexcept -> gcs::Elements
{
    return gcs::HashedSetConstruct(
        api_, reader(key_), count_, false_positive_rate_, elements, alloc);
}

auto GCS::hash_to_range(const ReadView in) const noexcept -> gcs::Range
{
    return gcs::HashToRange(api_, reader(key_), Range(), in);
}

auto GCS::Header(const cfilter::Header& previous) const noexcept
    -> cfilter::Header
{
    auto preimage = Vector<std::byte>{get_allocator()};
    Encode(writer(preimage));

    return blockchain::internal::FilterToHeader(
        api_, reader(preimage), previous.Bytes());
}

auto GCS::Match(
    const Targets& targets,
    allocator_type alloc,
    allocator_type monotonic) const noexcept -> Matches
{
    static constexpr auto reserveMatches = 16_uz;
    auto output = Matches{alloc};
    output.reserve(reserveMatches);
    using Map = opentxs::Map<gcs::Element, Matches>;
    auto hashed = gcs::Elements{monotonic};
    hashed.reserve(targets.size());
    hashed.clear();
    auto matches = gcs::Elements{monotonic};
    matches.reserve(reserveMatches);
    matches.clear();
    auto map = Map{monotonic};

    for (auto i = targets.cbegin(); i != targets.cend(); ++i) {
        const auto& hash = hashed.emplace_back(hash_to_range(*i));
        map[hash].emplace_back(i);
    }

    dedup(hashed);
    const auto& set = decompress();
    std::ranges::set_intersection(hashed, set, std::back_inserter(matches));

    for (const auto& match : matches) {
        auto& values = map.at(match);
        std::ranges::copy(values, std::back_inserter(output));
    }

    return output;
}

auto GCS::Match(const gcs::Hashes& prehashed, alloc::Default monotonic)
    const noexcept -> PrehashedMatches
{
    static constexpr auto reserveMatches = 16_uz;
    auto output = PrehashedMatches{prehashed.get_allocator()};
    output.reserve(reserveMatches);
    using Map = opentxs::Map<gcs::Element, PrehashedMatches>;
    auto hashed = gcs::Elements{monotonic};
    hashed.reserve(prehashed.size());
    hashed.clear();
    auto matches = gcs::Elements{monotonic};
    matches.reserve(reserveMatches);
    matches.clear();
    auto map = Map{monotonic};
    const auto range = Range();

    for (auto i = prehashed.cbegin(); i != prehashed.cend(); ++i) {
        const auto& hash = hashed.emplace_back(gcs::HashToRange(range, *i));
        map[hash].emplace_back(i);
    }

    dedup(hashed);
    const auto& set = decompress();
    std::ranges::set_intersection(hashed, set, std::back_inserter(matches));

    for (const auto& match : matches) {
        auto& values = map.at(match);
        std::ranges::copy(values, std::back_inserter(output));
    }

    return output;
}

auto GCS::Range() const noexcept -> gcs::Range
{
    return range(count_, false_positive_rate_);
}

auto GCS::Serialize(protobuf::GCS& output) const noexcept -> bool
{
    output.set_version(version_);
    output.set_bits(bits_);
    output.set_fprate(false_positive_rate_);
    output.set_key(reinterpret_cast<const char*>(key_.data()), key_.size());
    output.set_count(count_);
    output.set_filter(
        reinterpret_cast<const char*>(compressed_.data()), compressed_.size());

    return true;
}

auto GCS::Serialize(Writer&& out) const noexcept -> bool
{
    auto proto = protobuf::GCS{};

    if (false == Serialize(proto)) { return false; }

    return protobuf::write(proto, std::move(out));
}

auto GCS::Test(const Data& target, allocator_type monotonic) const noexcept
    -> bool
{
    return Test(target.Bytes(), monotonic);
}

auto GCS::Test(const ReadView target, allocator_type monotonic) const noexcept
    -> bool
{
    const auto input = [&] {
        auto out = Targets{monotonic};
        out.clear();
        out.emplace_back(target);

        return out;
    }();
    const auto set = hashed_set_construct(input, monotonic);

    assert_true(1 == set.size());

    const auto& hash = set.front();

    for (const auto& element : decompress()) {
        if (element == hash) {

            return true;
        } else if (element > hash) {

            return false;
        }
    }

    return false;
}

auto GCS::Test(const Vector<ByteArray>& targets, allocator_type monotonic)
    const noexcept -> bool
{
    return test(hashed_set_construct(targets, monotonic), monotonic);
}

auto GCS::Test(const Vector<Space>& targets, allocator_type monotonic)
    const noexcept -> bool
{
    return test(hashed_set_construct(targets, monotonic), monotonic);
}

auto GCS::Test(const gcs::Hashes& targets, alloc::Default monotonic)
    const noexcept -> bool
{
    return test(hashed_set_construct(targets, monotonic), monotonic);
}

auto GCS::test(const gcs::Elements& targets, allocator_type monotonic)
    const noexcept -> bool
{
    const auto& set = decompress();
    auto matches = Vector<gcs::Element>{monotonic};
    matches.reserve(std::min(targets.size(), set.size()));
    matches.clear();
    std::ranges::set_intersection(targets, set, std::back_inserter(matches));

    return false == matches.empty();
}

auto GCS::transform(const Vector<ByteArray>& in, allocator_type alloc) noexcept
    -> Targets
{
    auto output = Targets{alloc};
    output.reserve(in.size());
    output.clear();
    std::ranges::transform(in, std::back_inserter(output), [](const auto& i) {
        return i.Bytes();
    });

    return output;
}

auto GCS::transform(const Vector<Space>& in, allocator_type alloc) noexcept
    -> Targets
{
    auto output = Targets{alloc};
    output.reserve(in.size());
    output.clear();
    std::ranges::transform(in, std::back_inserter(output), [](const auto& i) {
        return reader(i);
    });

    return output;
}
}  // namespace opentxs::blockchain::cfilter::implementation
