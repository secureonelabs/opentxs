// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cassert>
#include <memory>

#include "opentxs/Export.hpp"

namespace opentxs
{
template <class C>
class OPENTXS_EXPORT Pimpl
{
public:
    using interface_type = C;

    explicit Pimpl(C* in) noexcept
        : pimpl_(in)
    {
        assert(pimpl_);
    }

    Pimpl(const Pimpl& rhs) noexcept
        : pimpl_(
#ifndef _WIN32
              rhs.pimpl_->clone()
#else
              dynamic_cast<C*>(rhs.pimpl_->clone())
#endif
          )
    {
        assert(pimpl_);
    }

    Pimpl(const C& rhs) noexcept
        : pimpl_(
#ifndef _WIN32
              rhs.clone()
#else
              dynamic_cast<C*>(rhs.clone())
#endif
          )
    {
        assert(pimpl_);
    }

    Pimpl(Pimpl&& rhs) noexcept
        : pimpl_(std::move(rhs.pimpl_))
    {
        assert(pimpl_);
    }

    Pimpl(std::unique_ptr<C>&& rhs) noexcept
        : pimpl_(std::move(rhs))
    {
        assert(pimpl_);
    }

    auto operator=(const Pimpl& rhs) noexcept -> Pimpl&
    {
        if (this != std::addressof(rhs)) {
            pimpl_.reset(
#ifndef _WIN32
                rhs.pimpl_->clone()
#else
                dynamic_cast<C*>(rhs.pimpl_->clone())
#endif
            );
        }

        assert(pimpl_);

        return *this;
    }

    auto operator=(Pimpl&& rhs) noexcept -> Pimpl&
    {
        pimpl_ = std::move(rhs.pimpl_);
        assert(pimpl_);

        return *this;
    }

    auto operator=(const C& rhs) noexcept -> Pimpl&
    {
        pimpl_.reset(
#ifndef _WIN32
            rhs.clone()
#else
            dynamic_cast<C*>(rhs.clone())
#endif
        );
        assert(pimpl_);

        return *this;
    }

    operator C&() noexcept { return *pimpl_; }
    operator const C&() const noexcept { return *pimpl_; }

    auto operator->() -> C* { return pimpl_.get(); }
    auto operator->() const -> const C* { return pimpl_.get(); }

    auto get() noexcept -> C& { return *pimpl_; }
    auto get() const noexcept -> const C& { return *pimpl_; }

    ~Pimpl() = default;

#if defined _WIN32
    Pimpl() = default;
#else
    Pimpl() = delete;
#endif

private:
    std::unique_ptr<C> pimpl_{nullptr};
};  // class Pimpl
}  // namespace opentxs
