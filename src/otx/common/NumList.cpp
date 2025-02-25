// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "internal/otx/common/NumList.hpp"  // IWYU pragma: associated

#include <cstdint>
#include <locale>
#include <utility>

#include "internal/core/String.hpp"
#include "internal/util/Pimpl.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"

// OTNumList (helper class.)

namespace opentxs
{
NumList::NumList()
    : data_()
{
}

NumList::NumList(const UnallocatedSet<std::int64_t>& theNumbers)
    : NumList()
{
    Add(theNumbers);
}

NumList::NumList(UnallocatedSet<std::int64_t>&& theNumbers)
    : data_(std::move(theNumbers))
{
}

NumList::NumList(std::int64_t lInput)
    : NumList()
{
    Add(lInput);
}

NumList::NumList(const String& strNumbers)
    : NumList()
{
    Add(strNumbers);
}

NumList::NumList(const UnallocatedCString& strNumbers)
    : NumList()
{
    Add(strNumbers);
}

auto NumList::Add(const String& strNumbers)
    -> bool  // if false, means the numbers
             // were already there. (At least
             // one of them.)
{
    return Add(strNumbers.Get());
}

auto NumList::Add(const UnallocatedCString& strNumbers)
    -> bool  // if false, means the
             // numbers were
             // already there. (At
             // least one of them.)
{
    return Add(strNumbers.c_str());
}

// This function is private, so you can't use it without passing an OTString.
// (For security reasons.) It takes a comma-separated list of numbers, and adds
// them to *this.
//
auto NumList::Add(const char* szNumbers) -> bool  // if false, means the numbers
                                                  // were already there. (At
                                                  // least one of them.)
{
    assert_false(nullptr == szNumbers);  // Should never happen.

    bool bSuccess = true;
    std::int64_t lNum = 0;
    const char* pChar = szNumbers;
    const std::locale loc;

    // Skip any whitespace.
    while (std::isspace(*pChar, loc)) { pChar++; }

    bool bStartedANumber =
        false;  // During the loop, set this to true when processing a digit,
                // and set to false when anything else. That way when we go to
                // add the number to the list, and it's "0", we'll know it's a
                // real number we're supposed to add, and not just a default
                // value.

    for (;;)  // We already know it's not null, due to the assert. (So at least
              // one iteration will happen.)
    {
        if (std::isdigit(*pChar, loc)) {
            bStartedANumber = true;

            const std::int32_t nDigit = (*pChar - '0');

            lNum *= 10;  // Move it up a decimal place.
            lNum += nDigit;
        }
        // if separator, or end of string, either way, add lNum to *this.
        else if (
            (',' == *pChar) || ('\0' == *pChar) ||
            std::isspace(*pChar, loc))  // first sign of a space, and we are
                                        // done with current number. (On to
                                        // the next.)
        {
            if ((lNum > 0) || (bStartedANumber && (0 == lNum))) {
                if (!Add(lNum))  // <=========
                {
                    bSuccess = false;  // We still go ahead and try to add them
                                       // all, and then return this sort of
                                       // status when it's all done.
                }
            }

            lNum = 0;  // reset for the next transaction number (in the
                       // comma-separated list.)
            bStartedANumber = false;  // reset
        } else {
            LogError()()(
                "Error: Unexpected character found in "
                "erstwhile comma-separated list of longs: ") (*pChar)(".")
                .Flush();
            bSuccess = false;
            break;
        }

        // End of the road.
        if ('\0' == *pChar) { break; }

        pChar++;

        // Skip any whitespace.
        while (std::isspace(*pChar, loc)) { pChar++; }

    }  // while

    return bSuccess;
}

auto NumList::Add(const std::int64_t& theValue)
    -> bool  // if false, means the value
             // was
             // already there.
{
    auto it = data_.find(theValue);

    if (data_.end() == it)  // it's not already there, so add it.
    {
        data_.insert(theValue);
        return true;
    }
    return false;  // it was already there.
}

auto NumList::Peek(std::int64_t& lPeek) const -> bool
{
    auto it = data_.begin();

    if (data_.end() != it)  // it's there.
    {
        lPeek = *it;
        return true;
    }
    return false;
}

auto NumList::Pop() -> bool
{
    auto it = data_.begin();

    if (data_.end() != it)  // it's there.
    {
        data_.erase(it);
        return true;
    }
    return false;
}

auto NumList::Remove(const std::int64_t& theValue)
    -> bool  // if false, means the value
             // was
             // NOT already there.
{
    auto it = data_.find(theValue);

    if (data_.end() != it)  // it's there.
    {
        data_.erase(it);
        return true;
    }
    return false;  // it wasn't there (so how could you remove it then?)
}

auto NumList::Verify(const std::int64_t& theValue) const
    -> bool  // returns true/false
             // (whether value is
             // already there.)
{
    auto it = data_.find(theValue);

    return (data_.end() == it) ? false : true;
}

// True/False, based on whether values are already there.
// (ALL theNumbersmust be present.)
// So if *this contains "3,4,5,6" and rhs contains "4,5" then match is TRUE.
//
auto NumList::Verify(const UnallocatedSet<std::int64_t>& theNumbers) const
    -> bool
{
    bool bSuccess = true;

    for (const auto& it : theNumbers) {
        if (!Verify(it)) {  // It must have NOT already been there.
            bSuccess = false;
        }
    }

    return bSuccess;
}

/// True/False, based on whether OTNumLists MATCH in COUNT and CONTENT (NOT
/// ORDER.)
///
auto NumList::Verify(const NumList& rhs) const -> bool
{
    if (Count() != rhs.Count()) {
        LogError()()("Incorrect count ")(rhs.Count())(" should be ")(Count())
            .Flush();

        return false;
    }

    for (const auto& it : data_) {
        if (false == rhs.Verify(it)) {
            LogError()()("Number ")(it)(" missing").Flush();

            return false;
        }
    }

    return true;
}

/// True/False, based on whether ANY of the numbers in rhs are found in *this.
///
auto NumList::VerifyAny(const NumList& rhs) const -> bool
{
    return rhs.VerifyAny(data_);
}

/// Verify whether ANY of the numbers on *this are found in setData.
///
auto NumList::VerifyAny(const UnallocatedSet<std::int64_t>& setData) const
    -> bool
{
    for (const auto& it : data_) {
        auto it_find = setData.find(it);

        if (it_find != setData.end()) {  // found a match.
            return true;
        }
    }

    return false;
}

auto NumList::Add(const NumList& theNumList)
    -> bool  // if false, means the numbers
             // were already there. (At
             // least one of them.)
{
    UnallocatedSet<std::int64_t> theOutput;
    theNumList.Output(theOutput);  // returns false if the numlist was empty.

    return Add(theOutput);
}

auto NumList::Add(const UnallocatedSet<std::int64_t>& theNumbers)
    -> bool  // if false, means
             // the
// numbers were already
// there. (At least one
// of them.)
{
    bool bSuccess = true;

    for (const auto& it : theNumbers) {
        if (!Add(it)) {  // It must have already been there.
            bSuccess = false;
        }
    }

    return bSuccess;
}

auto NumList::Remove(const UnallocatedSet<std::int64_t>& theNumbers)
    -> bool  // if false,
             // means
// the numbers were
// NOT already
// there. (At least
// one of them.)
{
    bool bSuccess = true;

    for (const auto& it : theNumbers) {
        if (!Remove(it)) {  // It must have NOT already been there.
            bSuccess = false;
        }
    }

    return bSuccess;
}

// Outputs the numlist as a set of numbers.
// (To iterate OTNumList, call this, then iterate the output.)
//
auto NumList::Output(UnallocatedSet<std::int64_t>& theOutput) const
    -> bool  // returns false
             // if
// the numlist was
// empty.
{
    theOutput = data_;

    return !data_.empty();
}

// Outputs the numlist as a comma-separated string (for serialization, usually.)
//
auto NumList::Output(String& strOutput) const -> bool  // returns false if the
                                                       // numlist was empty.
{
    std::int32_t nIterationCount = 0;

    for (const auto& it : data_) {
        nIterationCount++;
        auto sx = String::Factory(
            (1 == nIterationCount ? "" : ",") + std::to_string(it));

        strOutput.Concatenate(
            // If first iteration, prepend a blank string (instead of a comma.)
            // Like this:  "%" PRId64 ""
            // But for all subsequent iterations, Concatenate: ",%" PRId64 ""
            String::Factory(
                (1 == nIterationCount ? "" : ",") + std::to_string(it)));
    }

    return !data_.empty();
}

auto NumList::Count() const -> std::int32_t
{
    return static_cast<std::int32_t>(data_.size());
}

void NumList::Release() { data_.clear(); }

}  // namespace opentxs
