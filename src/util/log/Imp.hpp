// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <source_location>
#include <string_view>

#include "internal/util/Log.hpp"
#include "opentxs/Time.hpp"
#include "opentxs/Types.hpp"
#include "opentxs/storage/Types.hpp"
#include "opentxs/util/Log.hpp"
#include "util/log/Logger.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace boost
{
namespace system
{
class error_code;
}  // namespace system
}  // namespace boost

namespace opentxs
{
namespace api
{
class Crypto;
}  // namespace api

namespace blockchain
{
namespace block
{
class Outpoint;
class Position;
}  // namespace block
}  // namespace blockchain

namespace display
{
class Scale;
}  // namespace display

namespace identifier
{
class Generic;
}  // namespace identifier

namespace internal
{
class LogBuffer;
}  // namespace internal

class Amount;
class Data;
class PaymentCode;
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs
{
class Log::Imp final : public internal::Log
{
public:
    [[noreturn]] auto Abort() const noexcept -> void;
    [[noreturn]] auto Assert(
        const std::source_location& loc,
        std::string_view message) const noexcept -> void;
    auto asHex(const Data& in) const noexcept -> void;
    auto asHex(std::string_view in) const noexcept -> void;
    auto Buffer(const Amount& in) const noexcept -> void;
    auto Buffer(const Amount& in, UnitType currency) const noexcept -> void;
    auto Buffer(const Amount& in, const display::Scale& scale) const noexcept
        -> void;
    auto Buffer(const PaymentCode& in) const noexcept -> void;
    auto Buffer(const Time in) const noexcept -> void;
    auto Buffer(const storage::Hash& in) const noexcept -> void;
    auto Buffer(const blockchain::block::Outpoint& outpoint) const noexcept
        -> void;
    auto Buffer(const blockchain::block::Position& position) const noexcept
        -> void;
    auto Buffer(const boost::system::error_code& error) const noexcept -> void;
    auto Buffer(const identifier::Generic& in, const api::Crypto& api)
        const noexcept -> void;
    auto Buffer(const std::chrono::nanoseconds& in) const noexcept -> void;
    auto Buffer(const std::filesystem::path& in) const noexcept -> void;
    auto Buffer(const std::source_location& loc) const noexcept -> void;
    auto Buffer(const std::string_view in) const noexcept -> void;
    auto Flush() const noexcept -> void;
    auto Trace(const std::source_location& loc, std::string_view message)
        const noexcept -> void;

    Imp(const int logLevel) noexcept;
    Imp() = delete;
    Imp(const Imp&) = delete;
    Imp(Imp&&) = delete;
    auto operator=(const Imp&) -> Imp& = delete;
    auto operator=(Imp&&) -> Imp& = delete;

    ~Imp() final = default;

private:
    const int level_;
    const std::shared_ptr<internal::Logger> logger_;

    static auto get_buffer() noexcept -> internal::LogBuffer&;
    static auto get_data() noexcept
        -> std::shared_ptr<internal::Logger::Source>;
    static auto get_data(internal::LogBuffer& buf) noexcept
        -> std::shared_ptr<internal::Logger::Source>;

    auto active() const noexcept -> bool;
    auto buffer(std::string_view text) const noexcept -> void;
    auto send(
        const LogAction action = LogAction::flush,
        const Console console = Console::err) const noexcept -> void;
    [[noreturn]] auto wait_for_terminate() const noexcept -> void;
};
}  // namespace opentxs
