// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "otx/common/OTStorage.hpp"  // IWYU pragma: associated

#include <opentxs/OTDB/Generics.pb.h>
#include <opentxs/OTDB/Markets.pb.h>
#include <cstdio>
#include <filesystem>
#include <fstream>  // IWYU pragma: keep
#include <sstream>  // IWYU pragma: keep
#include <typeinfo>

#include "internal/core/Armored.hpp"
#include "internal/core/String.hpp"
#include "internal/util/P0330.hpp"
#include "internal/util/Pimpl.hpp"
#include "opentxs/api/Factory.internal.hpp"
#include "opentxs/api/Paths.internal.hpp"
#include "opentxs/api/Session.hpp"
#include "opentxs/api/Session.internal.hpp"
#include "opentxs/api/session/Factory.hpp"
#include "opentxs/core/ByteArray.hpp"
#include "opentxs/protobuf/Types.internal.hpp"
#include "opentxs/util/Log.hpp"
#include "otx/common/OTStoragePB.hpp"

/*
 // We want to store EXISTING OT OBJECTS (Usually signed contracts)
 // These have an EXISTING OT path, such as "inbox/acct_id".
 // These files are always in the form of a STRING.
 // The easiest way for me to store/retrieve those strings is:


 using namespace OTDB;

 bool bSuccessStore = StoreString(strContents, strFolder, strFilename);
 bool bSuccessQuery = QueryString(strRetrieved, strFolder, strFilename);


 // Internal to the above functions, the default Packing/Buffer is
 // used, and the default Storage type is used. But what if I want to
 // CHOOSE the storage and packing? Perhaps the default (filesystem) is not
 // good enough for me, and I prefer a key/value DB.

 // Storage.
 // Before creating my OWN storage, let's try using the default storage object
 // itself, instead of asking the API to use it for me:

 OTDB::Storage * pStorage = OTDB::GetDefaultStorage();
 assert_true(nullptr!=pStorage);

 bool bSuccessStore = pStorage->StoreString(strContents, strFolder,
 strFilename);
 bool bSuccessQuery = pStorage->QueryString(strRetrieved, strFolder,
 strFilename);


 // So if I wanted to create my OWN instance of storage, instead of using the
 // default one, it would be similar:

 OTDB::Storage * pStorage = OTDB::CreateStorageContext(STORE_FILESYSTEM,
 PACK_MESSAGE_PACK);
 assert_true(nullptr!=pStorage);

 bool bSuccessInit  = pStorage->Init("/path/to/data_folder", "wallet.xml");

 if (bSuccessInit)
 {
    bool bSuccessStore = pStorage->StoreString(strContents, strFolder,
 strFilename);
    bool bSuccessQuery = pStorage->QueryString(strRetrieved, strFolder,
 strFilename);
 }


 // Creating like above is also how the default storage context gets
 instantiated
 // (internally) when you first start storing and querying.

 // But Storage needs to be SET UP -- whether a database connection initiated,
 // or files loaded, or sub-directories created, or a Tor connection or
 whatever.
 // Therefore, there is an Init() call, which may have different parameters for
 // each storage type. That way, all subclasses might use it differently, and
 // the parameters are easily thrown into a config file later.


 // What if it was a CouchDB database, instead of the filesystem?
 // And using Google's Protocol Buffers for packing, isntead of MsgPack?
 // (Note: OT doesn't actually support CouchDB yet.) But it would look like:

 Storage * pStorage =
 CreateStorageContext(STORE_COUCHDB, PACK_PROTOCOL_BUFFERS);
 assert_true(nullptr!=pStorage);

 // This time, Init receives database connect info instead of filesystem info...
 bool bSuccessInit  = pStorage->Init("IP ADDRESS", "PORT", "USERNAME",
 "PASSWORD", "DATABASE NAME");

 etc.


 // So what if I want to use the default, but I want that DEFAULT to be CouchDB
 and Google?
 // Just do this (near the beginning of the execution of the application):

 bool bInit = InitDefaultStorage(STORE_COUCHDB, PACK_PROTOCOL_BUFFERS,
                    "IP ADDRESS", "PORT", "USERNAME", "PASSWORD", "DB NAME");

 if (true == bInit)
 {
    // Then do this as normal:

    Storage * pStorage = GetDefaultStorage();
    assert_true(nullptr!=pStorage);

    bool bSuccessStore = pStorage->StoreString(strContents, strFolder,
 strFilename);
    bool bSuccessQuery = pStorage->QueryString(strRetrieved, strFolder,
 strFilename);
 }


 // What if you want to store an OBJECT in that location instead of a string?
 // The object must be instantiated by the Storage Context...

 BitcoinAcct * pAcct = pStorage->CreateObject(STORED_OBJ_BITCOIN_ACCT);
 assert_false(nullptr == pAcct);

 pAcct->acct_id_                = "jkhsdf987345kjhf8lkjhwef987345";
 pAcct->bitcoin_acct_name_    = "Read-Only Label (Bitcoin Internal acct)";
 pAcct->gui_label_            = "Editable Label (Moneychanger)";


 // Perhaps you want to load up a Wallet and add this BitcoinAcct to it...

 WalletData * pWalletData =
 pStorage->QueryObject(STORED_OBJ_WALLET_DATA, "moneychanger", "wallet.pak");

 if (nullptr != pWalletData) // It loaded.
 {
    if (pWalletData->AddBitcoinAcct(*pAcct))
        bool bSuccessStore = pStorage->StoreObject(*pWalletData, "moneychanger",
 strFilename);
    else
        delete pAcct;

    delete pWalletData;
 }

 // Voila! The above code creates a BitcoinAcct (data object, not the real
 thing)
 // and then loads up the Moneychanger WalletData object, adds the BitcoinAcct
 to
 // it, and then stores it again.

 // SIMPLE, RIGHT?

 // Through this mechanism:
 //
 // 1) You can store your objects using the same storage context as the rest of
 OT.
 // 2) You can dictate a different storage context, just for yourself, or for
 the entire OT library as well.
 // 3) You can subclass OTDB::Storage and thus invent new storage methods.
 // 4) You can easily store and load objects and strings.
 // 5) You can swap out the packing code (msgpack, protobuf, json, etc) with no
 change to any other code.
 // 6) It's consistent and easy-to-use for all object types.
 // 7) For generic objects, there is a Blob type, a String type, and a StringMap
 type.
 */

opentxs::OTDB::Storage* opentxs::OTDB::details::s_pStorage = nullptr;

opentxs::OTDB::mapOfFunctions* opentxs::OTDB::details::pFunctionMap =
    nullptr;  // This is a pointer so I can control what order it is created in,
              // on
              // startup.

// NOLINTNEXTLINE(modernize-avoid-c-arrays)
const char* opentxs::OTDB::StoredObjectTypeStrings[] = {
    "OTDBString",     // Just a string.
    "Blob",           // Binary data of arbitrary size.
    "StringMap",      // A StringMap is a list of Key/Value pairs, useful for
                      // storing
                      // nearly anything.
    "WalletData",     // The GUI wallet's stored data
    "BitcoinAcct",    // The GUI wallet's stored data about a Bitcoin acct
    "BitcoinServer",  // The GUI wallet's stored data about a Bitcoin RPC port.
    "RippleServer",   // The GUI wallet's stored data about a Ripple server.
    "LoomServer",     // The GUI wallet's stored data about a Loom server.
    "ServerInfo",     // A Nym has a list of these.
    "ContactNym",     // This is a Nym record inside a contact of your address
                      // book.
    "ContactAcct",    // This is an account record inside a contact of your
                      // address
                      // book.
    "Contact",        // Your address book has a list of these.
    "AddressBook",    // Your address book.
    "MarketData",     // The description data for any given Market ID.
    "MarketList",     // A list of MarketDatas.
    "BidData",        // Offer details (doesn't contain private details)
    "AskData",        // Offer details (doesn't contain private details)
    "OfferListMarket",  // A list of offer details, for a specific market.
    "TradeDataMarket",  // Trade details (doesn't contain private data)
    "TradeListMarket",  // A list of trade details, for a specific market.
    "OfferDataNym",     // Private offer details for a particular Nym and Offer.
    "OfferListNym",     // A list of private offer details for a particular Nym.
    "TradeDataNym",     // Private trade details for a particular Nym and Trade.
    "TradeListNym",  // A list of private trade details for a particular Nym and
                     // Offer.
    "StoredObjError"  // (Should never be.)
};

namespace opentxs::OTDB
{
// NAMESPACE CONSTRUCTOR / DESTRUCTOR

// s_pStorage is "private" to the namespace.
// Use GetDefaultStorage() to access this variable.
// Use InitDefaultStorage() to set up this variable.
//
// Storage * ::details::s_pStorage = nullptr;
// These are actually defined in the namespace (.h file).

// mapOfFunctions * details::pFunctionMap;

InitOTDBDetails theOTDBConstructor;  // Constructor for this instance (define
                                     // all
                                     // namespace variables above this line.)

InitOTDBDetails::InitOTDBDetails()  // Constructor for namespace
{
#if defined(OTDB_MESSAGE_PACK) || defined(OTDB_PROTOCOL_BUFFERS)
    assert_true(nullptr == details::pFunctionMap);
    details::pFunctionMap = new mapOfFunctions;

    assert_false(nullptr == details::pFunctionMap);
    mapOfFunctions& theMap = *(details::pFunctionMap);
#endif

#if defined(OTDB_PROTOCOL_BUFFERS)
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    theMap[std::make_pair(PACK_PROTOCOL_BUFFERS, STORED_OBJ_STRING)] =
        &StringPB::Instantiate;
    theMap[std::make_pair(PACK_PROTOCOL_BUFFERS, STORED_OBJ_STRING_MAP)] =
        &StringMapPB::Instantiate;
    theMap[std::make_pair(PACK_PROTOCOL_BUFFERS, STORED_OBJ_MARKET_DATA)] =
        &MarketDataPB::Instantiate;
    theMap[std::make_pair(PACK_PROTOCOL_BUFFERS, STORED_OBJ_MARKET_LIST)] =
        &MarketListPB::Instantiate;
    theMap[std::make_pair(PACK_PROTOCOL_BUFFERS, STORED_OBJ_BID_DATA)] =
        &BidDataPB::Instantiate;
    theMap[std::make_pair(PACK_PROTOCOL_BUFFERS, STORED_OBJ_ASK_DATA)] =
        &AskDataPB::Instantiate;
    theMap[std::make_pair(
        PACK_PROTOCOL_BUFFERS, STORED_OBJ_OFFER_LIST_MARKET)] =
        &OfferListMarketPB::Instantiate;
    theMap[std::make_pair(
        PACK_PROTOCOL_BUFFERS, STORED_OBJ_TRADE_DATA_MARKET)] =
        &TradeDataMarketPB::Instantiate;
    theMap[std::make_pair(
        PACK_PROTOCOL_BUFFERS, STORED_OBJ_TRADE_LIST_MARKET)] =
        &TradeListMarketPB::Instantiate;
    theMap[std::make_pair(PACK_PROTOCOL_BUFFERS, STORED_OBJ_OFFER_DATA_NYM)] =
        &OfferDataNymPB::Instantiate;
    theMap[std::make_pair(PACK_PROTOCOL_BUFFERS, STORED_OBJ_OFFER_LIST_NYM)] =
        &OfferListNymPB::Instantiate;
    theMap[std::make_pair(PACK_PROTOCOL_BUFFERS, STORED_OBJ_TRADE_DATA_NYM)] =
        &TradeDataNymPB::Instantiate;
    theMap[std::make_pair(PACK_PROTOCOL_BUFFERS, STORED_OBJ_TRADE_LIST_NYM)] =
        &TradeListNymPB::Instantiate;
#endif
}

InitOTDBDetails::~InitOTDBDetails()  // Destructor for namespace
{
    assert_false(nullptr == details::pFunctionMap);
    delete details::pFunctionMap;
    details::pFunctionMap = nullptr;

#if defined(OTDB_PROTOCOL_BUFFERS)
    google::protobuf::ShutdownProtobufLibrary();
#endif
}

// INTERFACE for the Namespace (for coders to use.)

auto GetDefaultStorage() -> Storage* { return OTDB::details::s_pStorage; }

// You might normally create your own Storage object, choosing the storage type
// and the packing type, and then call Init() on that object in order to get it
// up and running.  This function is the equivalent of doing all that, but with
// the
// DEFAULT storage object (which OT uses when none is specified.)
//
auto InitDefaultStorage(StorageType eStoreType, PackType ePackType) -> bool
{
    // This allows you to call multiple times if you want to change the default
    // storage.
    //
    //        if (nullptr != details::s_pStorage)
    //        {
    //            otErr << "OTDB::InitDefaultStorage: Existing storage context
    // already exists. (Erasing / replacing it.)\n";
    //
    //            delete details::s_pStorage;
    //            details::s_pStorage = nullptr;
    //        }

    if (nullptr == details::s_pStorage) {
        LogVerbose()()("Existing storage context doesn't ")(
            "already exist. (Creating it.) ")
            .Flush();

        details::s_pStorage = Storage::Create(eStoreType, ePackType);
    }

    if (nullptr == details::s_pStorage) {
        LogError()()("Failed while calling "
                     "OTDB::Storage::Create().")
            .Flush();
        return false;
    }

    return true;
}

// %newobject Factory::createObj();
auto CreateStorageContext(StorageType eStoreType, PackType ePackType)
    -> Storage*
{
    Storage* pStorage = Storage::Create(eStoreType, ePackType);

    return pStorage;  // caller responsible to delete
}

// %newobject Factory::createObj();
auto CreateObject(StoredObjectType eType) -> Storable*
{
    Storage* pStorage = details::s_pStorage;

    if (nullptr == pStorage) { return nullptr; }

    return pStorage->CreateObject(eType);
}

// bool bSuccess = Store(strInbox, "inbox", "lkjsdf908w345ljkvd");
// bool bSuccess = Store(strMint,  "mints", NOTARY_ID,
// INSTRUMENT_DEFINITION_ID);
// bool bSuccess = Store(strPurse, "purse", NOTARY_ID, NYM_ID,
// INSTRUMENT_DEFINITION_ID);

// BELOW FUNCTIONS use the DEFAULT Storage context.

// Check that if oneStr is "", then twoStr and threeStr are "" also... and so
// on...
auto CheckStringsExistInOrder(
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr,
    const std::source_location& loc) -> bool
{
    auto ot_strFolder = String::Factory(strFolder),
         ot_oneStr = String::Factory(oneStr),
         ot_twoStr = String::Factory(twoStr),
         ot_threeStr = String::Factory(threeStr);

    if (String::Factory(dataFolder)->Exists()) {
        if (ot_strFolder->Exists()) {
            if (!ot_oneStr->Exists()) {
                if ((!ot_twoStr->Exists()) && (!ot_threeStr->Exists())) {
                } else {
                    LogAbort()(loc)(": ot_twoStr or ot_threeStr exist, when "
                                    "ot_oneStr doesn't exist!")
                        .Abort();
                }
            } else if ((!ot_twoStr->Exists()) && (ot_threeStr->Exists())) {
                LogAbort()(loc)(": ot_twoStr or ot_threeStr exist, when "
                                "ot_oneStr doesn't exist!")
                    .Abort();
            }
        } else {
            LogAbort()(loc)(": ot_strFolder must always exist!").Abort();
        }
    } else {
        LogAbort()(loc)(": dataFolder must always exist!").Abort();
    }
    return true;
}

// See if the file is there.
auto Exists(
    const api::Session& api,
    const UnallocatedCString& dataFolder,
    UnallocatedCString strFolder,
    UnallocatedCString oneStr,
    UnallocatedCString twoStr,
    UnallocatedCString threeStr) -> bool
{
    {
        auto ot_strFolder = String::Factory(strFolder),
             ot_oneStr = String::Factory(oneStr),
             ot_twoStr = String::Factory(twoStr),
             ot_threeStr = String::Factory(threeStr);
        assert_true(ot_strFolder->Exists(), "ot_strFolder is empty.");

        if (!ot_oneStr->Exists()) {
            assert_true(
                !ot_twoStr->Exists() && !ot_threeStr->Exists(), "bad options");
            oneStr = strFolder;
            strFolder = ".";
        }
    }

    Storage* pStorage = details::s_pStorage;

    if (nullptr == pStorage) {
        LogConsole()()("details::s_pStorage is null. (Returning false.).")
            .Flush();
        return false;
    }

    return pStorage->Exists(
        api, dataFolder, strFolder, oneStr, twoStr, threeStr);
}

auto FormPathString(
    const api::Session& api,
    UnallocatedCString& strOutput,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> std::int64_t
{
    auto ot_strFolder = String::Factory(strFolder),
         ot_oneStr = String::Factory(oneStr),
         ot_twoStr = String::Factory(twoStr),
         ot_threeStr = String::Factory(threeStr);
    assert_true(ot_strFolder->Exists(), "ot_strFolder is empty.");

    if (!ot_oneStr->Exists()) {
        assert_true(
            !ot_twoStr->Exists() && !ot_threeStr->Exists(), "bad options");
        ot_oneStr = String::Factory(strFolder.c_str());
        ot_strFolder = String::Factory(".");
    }

    Storage* pStorage = details::s_pStorage;

    if (nullptr == pStorage) {
        LogConsole()()("details::s_pStorage is null. (Returning -1).").Flush();
        return -1;
    }

    return pStorage->FormPathString(
        api,
        strOutput,
        dataFolder,
        ot_strFolder->Get(),
        ot_oneStr->Get(),
        twoStr,
        threeStr);
}

// Store/Retrieve a string.

auto StoreString(
    const api::Session& api,
    const UnallocatedCString& strContents,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    auto ot_strFolder = String::Factory(strFolder),
         ot_oneStr = String::Factory(oneStr),
         ot_twoStr = String::Factory(twoStr),
         ot_threeStr = String::Factory(threeStr);
    assert_true(ot_strFolder->Exists(), "ot_strFolder is null");

    if (!ot_oneStr->Exists()) {
        assert_true(
            !ot_twoStr->Exists() && !ot_threeStr->Exists(), "bad options");
        ot_oneStr = String::Factory(strFolder.c_str());
        ot_strFolder = String::Factory(".");
    }

    Storage* pStorage = details::s_pStorage;

    if (nullptr == pStorage) { return false; }

    return pStorage->StoreString(
        api,
        strContents,
        dataFolder,
        ot_strFolder->Get(),
        ot_oneStr->Get(),
        twoStr,
        threeStr);
}

auto QueryString(
    const api::Session& api,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr,
    const std::source_location& loc) -> UnallocatedCString
{
    auto ot_strFolder = String::Factory(strFolder),
         ot_oneStr = String::Factory(oneStr),
         ot_twoStr = String::Factory(twoStr),
         ot_threeStr = String::Factory(threeStr);

    if (!CheckStringsExistInOrder(
            dataFolder, strFolder, oneStr, twoStr, threeStr, loc)) {
        return {};
    }

    if (!ot_oneStr->Exists()) {
        assert_true(
            !ot_twoStr->Exists() && !ot_threeStr->Exists(), "bad options");
        ot_oneStr = String::Factory(strFolder.c_str());
        ot_strFolder = String::Factory(".");
    }

    Storage* pStorage = details::s_pStorage;

    if (nullptr == pStorage) { return {}; }

    return pStorage->QueryString(
        api,
        dataFolder,
        ot_strFolder->Get(),
        ot_oneStr->Get(),
        twoStr,
        threeStr);
}

// Store/Retrieve a plain string.

auto StorePlainString(
    const api::Session& api,
    const UnallocatedCString& strContents,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    auto ot_strFolder = String::Factory(strFolder),
         ot_oneStr = String::Factory(oneStr),
         ot_twoStr = String::Factory(twoStr),
         ot_threeStr = String::Factory(threeStr);
    assert_true(ot_strFolder->Exists(), "ot_strFolder is null");

    if (!ot_oneStr->Exists()) {
        assert_true(
            !ot_twoStr->Exists() && !ot_threeStr->Exists(), "bad options");
        ot_oneStr = String::Factory(strFolder.c_str());
        ot_strFolder = String::Factory(".");
    }
    Storage* pStorage = details::s_pStorage;

    assert_true(
        (strFolder.length() > 3) || (0 == strFolder.compare(0, 1, ".")));
    assert_true((oneStr.length() < 1) || (oneStr.length() > 3));

    if (nullptr == pStorage) { return false; }

    return pStorage->StorePlainString(
        api,
        strContents,
        dataFolder,
        ot_strFolder->Get(),
        ot_oneStr->Get(),
        twoStr,
        threeStr);
}

auto QueryPlainString(
    const api::Session& api,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> UnallocatedCString
{
    auto ot_strFolder = String::Factory(strFolder),
         ot_oneStr = String::Factory(oneStr),
         ot_twoStr = String::Factory(twoStr),
         ot_threeStr = String::Factory(threeStr);
    assert_true(ot_strFolder->Exists(), "ot_strFolder is null");

    if (!ot_oneStr->Exists()) {
        assert_true(
            !ot_twoStr->Exists() && !ot_threeStr->Exists(), "bad options");
        ot_oneStr = String::Factory(strFolder.c_str());
        ot_strFolder = String::Factory(".");
    }

    Storage* pStorage = details::s_pStorage;

    assert_true(
        (strFolder.length() > 3) || (0 == strFolder.compare(0, 1, ".")));
    assert_true((oneStr.length() < 1) || (oneStr.length() > 3));

    if (nullptr == pStorage) { return {}; }

    return pStorage->QueryPlainString(
        api,
        dataFolder,
        ot_strFolder->Get(),
        ot_oneStr->Get(),
        twoStr,
        threeStr);
}

// Store/Retrieve an object. (Storable.)

auto StoreObject(
    const api::Session& api,
    Storable& theContents,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    auto ot_strFolder = String::Factory(strFolder),
         ot_oneStr = String::Factory(oneStr),
         ot_twoStr = String::Factory(twoStr),
         ot_threeStr = String::Factory(threeStr);
    assert_true(ot_strFolder->Exists(), "ot_strFolder is null");

    if (!ot_oneStr->Exists()) {
        assert_true(
            !ot_twoStr->Exists() && !ot_threeStr->Exists(), "bad options");
        ot_oneStr = String::Factory(strFolder.c_str());
        ot_strFolder = String::Factory(".");
    }

    Storage* pStorage = details::s_pStorage;

    if (nullptr == pStorage) {
        LogError()()("No default storage object allocated.").Flush();
        return false;
    }

    return pStorage->StoreObject(
        api,
        theContents,
        dataFolder,
        ot_strFolder->Get(),
        ot_oneStr->Get(),
        twoStr,
        threeStr);
}

// Use %newobject Storage::Query();
auto QueryObject(
    const api::Session& api,
    const StoredObjectType theObjectType,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> Storable*
{
    auto ot_strFolder = String::Factory(strFolder),
         ot_oneStr = String::Factory(oneStr),
         ot_twoStr = String::Factory(twoStr),
         ot_threeStr = String::Factory(threeStr);
    assert_true(ot_strFolder->Exists(), "ot_strFolder is null");

    if (!ot_oneStr->Exists()) {
        assert_true(
            !ot_twoStr->Exists() && !ot_threeStr->Exists(), "bad options");
        ot_oneStr = String::Factory(strFolder.c_str());
        ot_strFolder = String::Factory(".");
    }

    Storage* pStorage = details::s_pStorage;

    if (nullptr == pStorage) { return nullptr; }

    return pStorage->QueryObject(
        api,
        theObjectType,
        dataFolder,
        ot_strFolder->Get(),
        ot_oneStr->Get(),
        twoStr,
        threeStr);
}

// Store/Retrieve a Storable object to/from an Armored object.

auto EncodeObject(const api::Session& api, Storable& theContents)
    -> UnallocatedCString
{
    Storage* pStorage = details::s_pStorage;

    if (nullptr == pStorage) {
        LogError()()("No Default Storage object allocated.").Flush();
        return "";
    }
    return pStorage->EncodeObject(api, theContents);
}

// Use %newobject Storage::DecodeObject();
auto DecodeObject(
    const api::Crypto& crypto,
    const StoredObjectType theObjectType,
    const UnallocatedCString& strInput) -> Storable*
{
    Storage* pStorage = details::s_pStorage;

    if (nullptr == pStorage) { return nullptr; }

    return pStorage->DecodeObject(crypto, theObjectType, strInput);
}

// Erase a value by location.

auto EraseValueByKey(
    const api::Session& api,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    Storage* pStorage = details::s_pStorage;

    if (nullptr == pStorage) {
        LogError()()("No Default Storage object allocated.").Flush();
        return false;
    }

    return pStorage->EraseValueByKey(
        api, dataFolder, strFolder, oneStr, twoStr, threeStr);
}

// Used internally. Creates the right subclass for any stored object type,
// based on which packer is needed.

auto Storable::Create(StoredObjectType eType, PackType thePackType) -> Storable*
{
    if (nullptr == details::pFunctionMap) { return nullptr; }

    Storable* pStorable = nullptr;

    // The Pack type, plus the Stored Object type, is the Key to the map of
    // function pointers.
    const InstantiateFuncKey theKey(thePackType, eType);

    // If the key works, we get the function pointer to the static Create()
    // method for
    // the appropriate object type.

    auto it = details::pFunctionMap->find(theKey);

    if (details::pFunctionMap->end() == it) { return nullptr; }

    InstantiateFunc* pFunc = it->second;

    if (nullptr != pFunc) {
        pStorable = (*pFunc)();  // Now we instantiate the object...
    }

    return pStorable;  // May return nullptr...
}

// static. OTPacker Factory.
//
auto OTPacker::Create(PackType ePackType) -> OTPacker*
{
    OTPacker* pPacker = nullptr;

    switch (ePackType) {
#if defined(OTDB_MESSAGE_PACK)
        case PACK_MESSAGE_PACK: {
            pPacker = new PackerMsgpack;
            assert_false(nullptr == pPacker);
        } break;
#endif
#if defined(OTDB_PROTOCOL_BUFFERS)
        case PACK_PROTOCOL_BUFFERS: {
            pPacker = new PackerPB;
            assert_false(nullptr == pPacker);
        } break;
#endif
#if !defined(OTDB_MESSAGE_PACK)
        case PACK_MESSAGE_PACK:
#endif
#if !defined(OTDB_PROTOCOL_BUFFERS)
        case PACK_PROTOCOL_BUFFERS:
#endif
        case PACK_TYPE_ERROR:
        default: {
        }
    }

    return pPacker;  // May return nullptr...
}

auto OTPacker::GetType() const -> PackType
{
#if defined(OTDB_MESSAGE_PACK)
    if (typeid(*this) == typeid(PackerMsgpack)) return PACK_MESSAGE_PACK;
#endif
#if defined(OTDB_PROTOCOL_BUFFERS)
    if (typeid(*this) == typeid(PackerPB)) { return PACK_PROTOCOL_BUFFERS; }
#endif
    return PACK_TYPE_ERROR;
}

// Basically, ALL of the Storables have to implement the IStorable interface
// (or one of its subclasses).  They can override hookBeforePack(), and they
// can override onPack(). Those two methods will be where all the action is,
// for each subclass of OTPacker.
//
auto OTPacker::Pack(Storable& inObj) -> PackedBuffer*
{
    auto* pStorable = dynamic_cast<IStorable*>(&inObj);

    if (nullptr == pStorable)  // ALL Storables should implement SOME
                               // subinterface of IStorable
    {
        LogError()()("Error: IStorable dynamic_cast failed.").Flush();
        return nullptr;
    }

    // This is polymorphic, so we get the right kind of buffer for the packer.
    //
    PackedBuffer* pBuffer = CreateBuffer();
    assert_false(nullptr == pBuffer);

    // Must delete pBuffer, or return it, below this point.

    pStorable->hookBeforePack();  // Give the subclass a chance to prepare its
                                  // data for packing...

    // This line (commented out) shows how the line below it would have looked
    // if I had ended
    // up using polymorphic templates:
    //    if (!makeTStorablepStorable->pack(*pBuffer))

    if (!pStorable->onPack(*pBuffer, inObj)) {
        delete pBuffer;
        return nullptr;
    }

    return pBuffer;
}

// Similar to Pack, above.
// Unpack takes the contents of the PackedBuffer and unpacks them into
// the Storable. ASSUMES that the PackedBuffer is the right type for
// the Packer, usually because the Packer is the one who instantiated
// it.  Also assumes that the Storable's actual object type is the
// appropriate one for the data that is sitting in that buffer.
//
auto OTPacker::Unpack(PackedBuffer& inBuf, Storable& outObj) -> bool
{
    auto* pStorable = dynamic_cast<IStorable*>(&outObj);

    if (nullptr == pStorable) { return false; }

    // outObj is the OUTPUT OBJECT.
    // If we're unable to unpack the contents of inBuf
    // into outObj, return false.
    //
    if (!pStorable->onUnpack(inBuf, outObj)) { return false; }

    pStorable->hookAfterUnpack();  // Give the subclass a chance to settle its
                                   // data after unpacking...

    return true;
}

auto OTPacker::Pack(const UnallocatedCString& inObj) -> PackedBuffer*
{
    // This is polymorphic, so we get the right kind of buffer for the packer.
    //
    PackedBuffer* pBuffer = CreateBuffer();
    assert_false(nullptr == pBuffer);

    // Must delete pBuffer, or return it, below this point.

    if (!pBuffer->PackString(inObj)) {
        delete pBuffer;
        return nullptr;
    }

    return pBuffer;
}

auto OTPacker::Unpack(PackedBuffer& inBuf, UnallocatedCString& outObj) -> bool
{

    // outObj is the OUTPUT OBJECT.
    // If we're unable to unpack the contents of inBuf
    // into outObj, return false.
    //
    if (!inBuf.UnpackString(outObj)) { return false; }

    return true;
}

// NOTICE!!! that when you add something to the list, it is CLONED. (Caller
// is still responsible to delete the argument.)
//

#define IMPLEMENT_GET_ADD_REMOVE(scope, name)                                  \
                                                                               \
    using PointerTo##name = std::shared_ptr<name>;                             \
                                                                               \
    using listOf##name##s = UnallocatedDeque<PointerTo##name>;                 \
                                                                               \
    auto scope Get##name##Count() -> std::size_t                               \
    {                                                                          \
        return list_##name##s.size();                                          \
    }                                                                          \
                                                                               \
    auto scope Get##name(std::size_t nIndex) -> name*                          \
    {                                                                          \
        if (nIndex < list_##name##s.size()) {                                  \
            const auto& theP = list_##name##s.at(nIndex);                      \
            return theP.get();                                                 \
        }                                                                      \
        return nullptr;                                                        \
    }                                                                          \
                                                                               \
    auto scope Remove##name(std::size_t nIndex##name) -> bool                  \
    {                                                                          \
        if (nIndex##name < list_##name##s.size()) {                            \
            list_##name##s.erase(list_##name##s.begin() + nIndex##name);       \
            return true;                                                       \
        } else                                                                 \
            return false;                                                      \
    }                                                                          \
                                                                               \
    auto scope Add##name(name& disownObject) -> bool                           \
    {                                                                          \
        list_##name##s.emplace_back(PointerTo##name{disownObject.clone()});    \
        return true;                                                           \
    }

IMPLEMENT_GET_ADD_REMOVE(ContactNym::, ServerInfo)  // No semicolon on this one!

IMPLEMENT_GET_ADD_REMOVE(Contact::, ContactNym)  // No semicolon on this one!

IMPLEMENT_GET_ADD_REMOVE(Contact::, ContactAcct)  // No semicolon on this one!

IMPLEMENT_GET_ADD_REMOVE(AddressBook::, Contact)  // No semicolon on this one!

IMPLEMENT_GET_ADD_REMOVE(MarketList::, MarketData)  // No semicolon on this one!

IMPLEMENT_GET_ADD_REMOVE(
    OfferListMarket::,
    BidData)  // No semicolon on this one!

IMPLEMENT_GET_ADD_REMOVE(
    OfferListMarket::,
    AskData)  // No semicolon on this one!

IMPLEMENT_GET_ADD_REMOVE(
    TradeListMarket::,
    TradeDataMarket)  // No semicolon on this one!

IMPLEMENT_GET_ADD_REMOVE(
    OfferListNym::,
    OfferDataNym)  // No semicolon on this one!

IMPLEMENT_GET_ADD_REMOVE(
    TradeListNym::,
    TradeDataNym)  // No semicolon on this one!

// Make sure SWIG "loses ownership" of any objects pushed onto these lists.
// (So I am safe to destruct them indiscriminately.)
//
// UPDATE: Nevertheless, no need to erase the lists (below) since they now
// store smart pointers, instead of regular pointers, so they are self-cleaning.
//

// NOLINTBEGIN(modernize-use-equals-default)
ContactNym::~ContactNym()
{
    //      while (GetServerInfoCount() > 0)
    //          RemoveServerInfo(0);
}

Contact::~Contact()
{
    //      while (GetContactNymCount() > 0)
    //          RemoveContactNym(0);
    //
    //      while (GetContactAcctCount() > 0)
    //          RemoveContactAcct(0);
}

AddressBook::~AddressBook()
{
    //      while (GetContactCount() > 0)
    //          RemoveContact(0);
}
// NOLINTEND(modernize-use-equals-default)

/* Protocol Buffers notes.

 // optional string bitcoin_id = 1;
 inline bool has_bitcoin_id() const;
 inline void clear_bitcoin_id();
 static const std::int32_t kBitcoinIdFieldNumber = 1;
 inline const ::UnallocatedCString& bitcoin_id() const;
 inline void set_bitcoin_id(const ::UnallocatedCString& value);
 inline void set_bitcoin_id(const char* value);
 inline void set_bitcoin_id(const char* value, std::size_t size);
 inline ::UnallocatedCString* mutable_bitcoin_id();
 inline ::UnallocatedCString* release_bitcoin_id();

 // optional string bitcoin_name = 2;
 inline bool has_bitcoin_name() const;
 inline void clear_bitcoin_name();
 static const std::int32_t kBitcoinNameFieldNumber = 2;
 inline const ::UnallocatedCString& bitcoin_name() const;
 inline void set_bitcoin_name(const ::UnallocatedCString& value);
 inline void set_bitcoin_name(const char* value);
 inline void set_bitcoin_name(const char* value, std::size_t size);
 inline ::UnallocatedCString* mutable_bitcoin_name();
 inline ::UnallocatedCString* release_bitcoin_name();

 // optional string gui_label_ = 3;
 inline bool has_gui_label() const;
 inline void clear_gui_label();
 static const std::int32_t kGuiLabelFieldNumber = 3;
 inline const ::UnallocatedCString& gui_label() const;
 inline void set_gui_label(const ::UnallocatedCString& value);
 inline void set_gui_label(const char* value);
 inline void set_gui_label(const char* value, std::size_t size);
 inline ::UnallocatedCString* mutable_gui_label();
 inline ::UnallocatedCString* release_gui_label();
 */
/*
 bool SerializeToString(string* output) const; serializes the message and stores
 the bytes in the given string.
 (Note that the bytes are binary, not text; we only use the string class as a
 convenient container.)
 bool ParseFromString(const string& data); parses a message from the given
 string.

 bool SerializeToOstream(ostream* output) const; writes the message to the given
 C++ ostream.
 bool ParseFromIstream(istream* input); parses a message from the given C++
 istream.
 */

// This is a case for template polymorphism.
// See this article:  http://accu.org/index.php/articles/471
//
/*
 template <class T>    // TStorable...
 class TStorable        // a "template subclass" of Storable. This is like a
 version of java
 {                    // interfaces, which C++ normally implements via pure
 virtual base classes
     T const& t;    // and multiple inheritance. But in this case, I need to
 have a consistent
 public:                // interface across disparate classes (in various
 circumstances including
     TStorable(T const& obj) : t(obj) { }    // here with protocol buffers) and
 template interfaces
     bool pack(PackedBuffer& theBuffer)    // allow me to do that even with
 classes in a different hierarchy.
     { return t.onPack(theBuffer); }
 };

 template <class T>
 TStorable<T> makeTStorable( T& obj )
 {
    return TStorable<T>( obj );
 }
 */

/* // Specialization:
 template<>
 void TStorable<BigBenClock>::talk()
 {
    t.playBongs();
 }

 // Passing and returning as parameter:

 template <class T>
 void makeItTalk( TStorable<T> t )
 {
    t.talk();
 }

 template <class T>
 TStorable<T> makeTalkative( T& obj )
 {
    return TStorable<T>( obj );
 }
 */

// Why have IStorablePB::onPack? What is this all about?
//
// Because normally, packing is done by Packer. I have a packer subclass for
// the protocol buffers library, but notice that I don't have a packer for EVERY
// SINGLE STORABLE OT OBJECT, for the protocol buffers library. So when
// Packer::Pack()
// is called, the subclass being activated is PackerPB, not
// PackerForBitcoinAccountOnPB.
//
// With MsgPack, that would be the end of it, since the MsgPack Storables all
// derive from
// the same base class (due to the msgPack define) and a single call handles all
// of them.
// But with Protocol Buffers (and probably with custom objects, which are coming
// next), EACH
// PB-Storable has to wrap an instance of the PB-derived serializable object
// generated by
// protoc. Each instance thus has a PB member of a slightly different type, and
// there is no
// common base class between them that will give me a reference to that member,
// without
// overriding some virtual function IN ALL OF THE PB-SERIALIZABLE OBJECTS so
// that each can
// individually pass back the reference to its unique PB-derived member.
//
// Even if there were, LET US REMEMBER that all of the various Storables
// (instantiated for
// various specific packers), such as BitcoinAcctPB for example, are supposed to
// be derived
// from a data class such as BitcoinAcct. That way, BitcoinAcct can focus on the
// data itself,
// regardless of packer type, and OT can deal with its data in a pure way,
// meanwhile the
// actual object used can be one of 5 different subclasses of that, depending on
// which
// packer was employed. All of those subclasses (for protocol buffers, for
// msgpack, for json,
// etc) must be derived from the data class, BitcoinAcct.
//
// Remember, if ALL of the protocol-buffers wrapper classes, such as
// BitcoinAcctPB,
// BitcoinServerPB, LoomAcctPB, LoomServerPB, etc, are all derived from some
// StorablePB object,
// so they can all share a virtual function and thereby return a reference to
// their internally-
// wrapped object, then how are all of those classes supposed to ALSO be derived
// from their
// DATA classes, such as BitcoinAcct, BitcoinServer, LoomAcct, LoomServer, etc??
//
// The answer is multiple inheritance. Or INTERFACES, to be more specific. I
// have implemented
// Java-style interfaces as well as polymorphism-by-template to resolve these
// issues.
//
// The Storable (parameter to Pack) is actually the object that somehow has to
// override--or implement--the actual packing. Only it really knows. Therefore I
// have decided
// to add an INTERFACE, which is OPTIONAL, which makes it possible to hook and
// override the
// packing/unpacking, but such that things are otherwise handled in a broad
// stroke, without
// having to override EVERY LITTLE THING to accomplish it.
//
// Storables, as I said, will all be derived from their respective data objects,
// no matter
// which packer is being employed. When packing one, the framework will check to
// see if IStorable
// is present. It it is, then the framework will use it instead of continuing
// with the normal
// Pack procedure. It will also call the hook (first) so values can be copied
// where appropriate,
// before the actual packing occurs, or after (for unpacking.)
//
// This means, first, that few of the storables will ever actually have to
// override Pack() or
// Unpack(), as long as they override onPack() as necessary. AND, when onPack()
// IS overridden,
// it will be able to handle many different objects (via the Interface,
// templates, etc), instead
// of each having to provide a separate Pack() implementation for EVERY SINGLE
// PB object. For
// example, the IStorablePB interface handles ALL of the PB objects, without ANY
// of them having
// to override some special pack function.
//
// It WOULD have been possible to add this interface to Storable itself.
// Functions such as
// Pack(), Unpack(), hookBeforePack(), onPack(), etc could have been added there
// and then passed
// down to all the subclasses. But that is not as elegant, for these reasons:
// 1) Remember that BitcoinAcct is purely data-oriented, and is not a
// packing-related class.
//    (though its subclasses are.) So the members would be out of context,
// except for some lame
//    explanation that the subclasses use them for other purposes unrelated to
// this class.
// 2) EVERY SINGLE class would be forced to provide its own implementation of
// those functions,
//    since a common base class for groups of them is already discounted, since
// they are derived
//    from their data classes, not their packer classes.
//
//
//
//
//
//
// Interface:    IStorablePB
//

// Protocol Buffers packer.
//
#if defined(OTDB_PROTOCOL_BUFFERS)

auto IStorablePB::getPBMessage()
    -> ::google::protobuf::MessageLite*  // This is really
                                         // only here so it
                                         // can be
// overridden. Only
// subclasses of
// IStorablePB will
// actually exist.
{
    return nullptr;
}

template <
    class theBaseType,
    class theInternalType,
    StoredObjectType theObjectType>
auto ProtobufSubclass<theBaseType, theInternalType, theObjectType>::
    getPBMessage() -> ::google::protobuf::MessageLite*
{
    return (&pb_obj_);
}

//    if (!makeTStorablepStorable->pack(*pBuffer))
//::google::protobuf::MessageLite& IStorablePB::getPBMessage()
//{
//    return makeTStorablePBgetPBMessage();
//}

auto IStorablePB::onPack(
    PackedBuffer& theBuffer,
    Storable&) -> bool  // buffer is OUTPUT.
{
    // check here to make sure theBuffer is the right TYPE.
    auto* pBuffer = dynamic_cast<BufferPB*>(&theBuffer);

    if (nullptr == pBuffer) {  // Buffer is wrong type!!
        return false;
    }

    ::google::protobuf::MessageLite* pMessage = getPBMessage();

    if (nullptr == pMessage) { return false; }

    if (!pMessage->SerializeToString(&(pBuffer->buffer_))) { return false; }

    return true;
}

auto IStorablePB::onUnpack(
    PackedBuffer& theBuffer,
    Storable&) -> bool  // buffer is INPUT.
{
    // check here to make sure theBuffer is the right TYPE.
    auto* pBuffer = dynamic_cast<BufferPB*>(&theBuffer);

    if (nullptr == pBuffer) {  // Buffer is wrong type!!
        return false;
    }

    ::google::protobuf::MessageLite* pMessage = getPBMessage();

    if (nullptr == pMessage) { return false; }

    if (!pMessage->ParseFromString(pBuffer->buffer_)) { return false; }

    return true;
}

/*
 bool SerializeToString(string* output) const;:
 Serializes the message and stores the bytes in the given string. Note that the
 bytes are binary,
 not text; we only use the string class as a convenient container.

 bool ParseFromString(const string& data);:
 parses a message from the given string.
 */
auto BufferPB::PackString(const UnallocatedCString& theString) -> bool
{
    StringPB theWrapper;

    ::google::protobuf::MessageLite* pMessage = theWrapper.getPBMessage();

    if (nullptr == pMessage) { return false; }

    auto* pBuffer = dynamic_cast<String_InternalPB*>(pMessage);

    if (nullptr == pBuffer) {  // Buffer is wrong type!!
        return false;
    }

    pBuffer->set_value(theString);

    if (!pBuffer->SerializeToString(&buffer_)) { return false; }

    return true;
}

auto BufferPB::UnpackString(UnallocatedCString& theString) -> bool
{
    StringPB theWrapper;

    ::google::protobuf::MessageLite* pMessage = theWrapper.getPBMessage();

    if (nullptr == pMessage) { return false; }

    auto* pBuffer = dynamic_cast<String_InternalPB*>(pMessage);

    if (nullptr == pBuffer) {  // Buffer is wrong type!!
        return false;
    }

    if (!pBuffer->ParseFromString(buffer_)) { return false; }

    theString = pBuffer->value();

    return true;
}

auto BufferPB::ReadFromIStream(std::istream& inStream, std::int64_t lFilesize)
    -> bool
{
    auto size = static_cast<unsigned long>(lFilesize);

    char* buf = new char[size];
    assert_false(nullptr == buf);

    inStream.read(buf, size);

    if (inStream.good()) {
        buffer_.assign(buf, size);
        delete[] buf;
        return true;
    }

    delete[] buf;
    buf = nullptr;

    return false;

    // buffer_.ParseFromIstream(&inStream);
}

auto BufferPB::WriteToOStream(std::ostream& outStream) -> bool
{
    // bool    SerializeToOstream(ostream* output) const
    if (buffer_.length() > 0) {
        outStream.write(buffer_.c_str(), buffer_.length());
        return outStream.good() ? true : false;
    } else {
        LogError()()("Buffer had zero length in BufferPB::WriteToOStream.")
            .Flush();
    }

    return false;
    // buffer_.SerializeToOstream(&outStream);
}

auto BufferPB::GetData() -> const std::uint8_t*
{
    return reinterpret_cast<const std::uint8_t*>(buffer_.c_str());
}

auto BufferPB::GetSize() -> std::size_t { return buffer_.size(); }

void BufferPB::SetData(const std::uint8_t* pData, std::size_t theSize)
{
    buffer_.assign(reinterpret_cast<const char*>(pData), theSize);
}

// !! All of these have to provide implementations for the hookBeforePack
// and hookAfterUnpack methods. In .cpp file:
/*
 void SUBCLASS_HERE::hookBeforePack()
 {
 pb_obj_.set_PROPERTY_NAME_GOES_HERE(PROPERTY_NAME_GOES_HERE);
 }
 void SUBCLASS_HERE::hookAfterUnpack()
 {
 PROPERTY_NAME_GOES_HERE    = pb_obj_.PROPERTY_NAME_GOES_HERE();
 }
 */
//

#define OT_IMPLEMENT_PB_LIST_PACK(pb_name, element_type)                       \
    pb_obj_.clear_##pb_name();                                                 \
    for (auto it = list_##element_type##s.begin();                             \
         it != list_##element_type##s.end();                                   \
         ++it) {                                                               \
        const auto& thePtr = *it;                                              \
        element_type##PB* pObject =                                            \
            dynamic_cast<element_type##PB*>(thePtr.get());                     \
        assert_false(nullptr == pObject);                                      \
        ::google::protobuf::MessageLite* pMessage = pObject->getPBMessage();   \
        assert_false(nullptr == pMessage);                                     \
        element_type##_InternalPB* pInternal =                                 \
            dynamic_cast<element_type##_InternalPB*>(pMessage);                \
        assert_false(nullptr == pInternal);                                    \
        element_type##_InternalPB* pNewInternal = pb_obj_.add_##pb_name();     \
        assert_false(nullptr == pNewInternal);                                 \
        pObject->hookBeforePack();                                             \
        pNewInternal->CopyFrom(*pInternal);                                    \
    }

#define OT_IMPLEMENT_PB_LIST_UNPACK(pb_name, element_type, ELEMENT_ENUM)       \
    while (Get##element_type##Count() > 0) Remove##element_type(0);            \
    for (std::int32_t i = 0; i < pb_obj_.pb_name##_size(); i++) {              \
        const element_type##_InternalPB& theInternal = pb_obj_.pb_name(i);     \
        element_type##PB* pNewWrapper = dynamic_cast<element_type##PB*>(       \
            Storable::Create(ELEMENT_ENUM, PACK_PROTOCOL_BUFFERS));            \
        assert_false(nullptr == pNewWrapper);                                  \
        ::google::protobuf::MessageLite* pMessage =                            \
            pNewWrapper->getPBMessage();                                       \
        assert_false(nullptr == pMessage);                                     \
        element_type##_InternalPB* pInternal =                                 \
            dynamic_cast<element_type##_InternalPB*>(pMessage);                \
        assert_false(nullptr == pInternal);                                    \
        pInternal->CopyFrom(theInternal);                                      \
        pNewWrapper->hookAfterUnpack();                                        \
        const PointerTo##element_type thePtr(                                  \
            dynamic_cast<element_type*>(pNewWrapper));                         \
        list_##element_type##s.push_back(thePtr);                              \
    }

template <>
void StringMapPB::hookBeforePack()
{
    pb_obj_.clear_node();  // "node" is the repeated field of Key/Values.

    // Loop through all the key/value pairs in the map, and add them to
    // pb_obj_.node.
    //
    for (auto& it : the_map_) {
        KeyValue_InternalPB* pNode = pb_obj_.add_node();
        pNode->set_key(it.first);
        pNode->set_value(it.second);
    }
}

template <>
void StringMapPB::hookAfterUnpack()
{
    //    the_map_ = pb_obj_.the_map_();

    the_map_.clear();

    for (std::int32_t i = 0; i < pb_obj_.node_size(); i++) {
        const KeyValue_InternalPB& theNode = pb_obj_.node(i);

        the_map_.insert(std::pair<UnallocatedCString, UnallocatedCString>(
            theNode.key(), theNode.value()));
    }
}

template <>
void StringPB::hookBeforePack()
{
    pb_obj_.set_value(string_);
    // The way StringPB is used, this function will never actually get called.
    // (But if you used it like the others, it would work, since this function
    // is here.)
}
template <>
void StringPB::hookAfterUnpack()
{
    string_ = pb_obj_.value();
    // The way StringPB is used, this function will never actually get called.
    // (But if you used it like the others, it would work, since this function
    // is here.)
}

template <>
void MarketDataPB::hookBeforePack()
{
    pb_obj_.set_gui_label(gui_label_);
    pb_obj_.set_notary_id(notary_id_);
    pb_obj_.set_market_id(market_id_);
    pb_obj_.set_instrument_definition_id(instrument_definition_id_);
    pb_obj_.set_currency_type_id(currency_type_id_);
    pb_obj_.set_scale(scale_);
    pb_obj_.set_total_assets(total_assets_);
    pb_obj_.set_number_bids(number_bids_);
    pb_obj_.set_number_asks(number_asks_);
    pb_obj_.set_last_sale_price(last_sale_price_);
    pb_obj_.set_last_sale_date(last_sale_date_);
    pb_obj_.set_current_bid(current_bid_);
    pb_obj_.set_current_ask(current_ask_);
    pb_obj_.set_volume_trades(volume_trades_);
    pb_obj_.set_volume_assets(volume_assets_);
    pb_obj_.set_volume_currency(volume_currency_);
    pb_obj_.set_recent_highest_bid(recent_highest_bid_);
    pb_obj_.set_recent_lowest_ask(recent_lowest_ask_);
}

template <>
void MarketDataPB::hookAfterUnpack()
{
    gui_label_ = pb_obj_.gui_label();
    notary_id_ = pb_obj_.notary_id();
    market_id_ = pb_obj_.market_id();
    instrument_definition_id_ = pb_obj_.instrument_definition_id();
    currency_type_id_ = pb_obj_.currency_type_id();
    scale_ = pb_obj_.scale();
    total_assets_ = pb_obj_.total_assets();
    number_bids_ = pb_obj_.number_bids();
    number_asks_ = pb_obj_.number_asks();
    last_sale_price_ = pb_obj_.last_sale_price();
    last_sale_date_ = pb_obj_.last_sale_date();
    current_bid_ = pb_obj_.current_bid();
    current_ask_ = pb_obj_.current_ask();
    volume_trades_ = pb_obj_.volume_trades();
    volume_assets_ = pb_obj_.volume_assets();
    volume_currency_ = pb_obj_.volume_currency();
    recent_highest_bid_ = pb_obj_.recent_highest_bid();
    recent_lowest_ask_ = pb_obj_.recent_lowest_ask();
}

template <>
void MarketListPB::hookBeforePack()
{
    OT_IMPLEMENT_PB_LIST_PACK(market_data, MarketData)
}

template <>
void MarketListPB::hookAfterUnpack()
{
    OT_IMPLEMENT_PB_LIST_UNPACK(market_data, MarketData, STORED_OBJ_MARKET_DATA)
}

template <>
void BidDataPB::hookBeforePack()
{
    pb_obj_.set_gui_label(gui_label_);
    pb_obj_.set_transaction_id(transaction_id_);
    pb_obj_.set_price_per_scale(price_per_scale_);
    pb_obj_.set_available_assets(available_assets_);
    pb_obj_.set_minimum_increment(minimum_increment_);
    pb_obj_.set_date(date_);
}

template <>
void BidDataPB::hookAfterUnpack()
{
    gui_label_ = pb_obj_.gui_label();
    transaction_id_ = pb_obj_.transaction_id();
    price_per_scale_ = pb_obj_.price_per_scale();
    available_assets_ = pb_obj_.available_assets();
    minimum_increment_ = pb_obj_.minimum_increment();
    date_ = pb_obj_.date();
}

template <>
void AskDataPB::hookBeforePack()
{
    pb_obj_.set_gui_label(gui_label_);
    pb_obj_.set_transaction_id(transaction_id_);
    pb_obj_.set_price_per_scale(price_per_scale_);
    pb_obj_.set_available_assets(available_assets_);
    pb_obj_.set_minimum_increment(minimum_increment_);
    pb_obj_.set_date(date_);
}

template <>
void AskDataPB::hookAfterUnpack()
{
    gui_label_ = pb_obj_.gui_label();
    transaction_id_ = pb_obj_.transaction_id();
    price_per_scale_ = pb_obj_.price_per_scale();
    available_assets_ = pb_obj_.available_assets();
    minimum_increment_ = pb_obj_.minimum_increment();
    date_ = pb_obj_.date();
}

template <>
void OfferListMarketPB::hookBeforePack()
{
    OT_IMPLEMENT_PB_LIST_PACK(bids, BidData)
    OT_IMPLEMENT_PB_LIST_PACK(asks, AskData)
}

template <>
void OfferListMarketPB::hookAfterUnpack()
{
    OT_IMPLEMENT_PB_LIST_UNPACK(bids, BidData, STORED_OBJ_BID_DATA)
    OT_IMPLEMENT_PB_LIST_UNPACK(asks, AskData, STORED_OBJ_ASK_DATA)
}

template <>
void TradeDataMarketPB::hookBeforePack()
{
    pb_obj_.set_gui_label(gui_label_);
    pb_obj_.set_transaction_id(transaction_id_);
    pb_obj_.set_date(date_);
    pb_obj_.set_price(price_);
    pb_obj_.set_amount_sold(amount_sold_);
}

template <>
void TradeDataMarketPB::hookAfterUnpack()
{
    gui_label_ = pb_obj_.gui_label();
    transaction_id_ = pb_obj_.transaction_id();
    date_ = pb_obj_.date();
    price_ = pb_obj_.price();
    amount_sold_ = pb_obj_.amount_sold();
}

template <>
void TradeListMarketPB::hookBeforePack()
{
    OT_IMPLEMENT_PB_LIST_PACK(trades, TradeDataMarket)
}

template <>
void TradeListMarketPB::hookAfterUnpack()
{
    OT_IMPLEMENT_PB_LIST_UNPACK(
        trades, TradeDataMarket, STORED_OBJ_TRADE_DATA_MARKET)
}

template <>
void OfferDataNymPB::hookBeforePack()
{
    pb_obj_.set_gui_label(gui_label_);
    pb_obj_.set_valid_from(valid_from_);
    pb_obj_.set_valid_to(valid_to_);
    pb_obj_.set_notary_id(notary_id_);
    pb_obj_.set_instrument_definition_id(instrument_definition_id_);
    pb_obj_.set_asset_acct_id(asset_acct_id_);
    pb_obj_.set_currency_type_id(currency_type_id_);
    pb_obj_.set_currency_acct_id(currency_acct_id_);
    pb_obj_.set_selling(selling_);
    pb_obj_.set_scale(scale_);
    pb_obj_.set_price_per_scale(price_per_scale_);
    pb_obj_.set_transaction_id(transaction_id_);
    pb_obj_.set_total_assets(total_assets_);
    pb_obj_.set_finished_so_far(finished_so_far_);
    pb_obj_.set_minimum_increment(minimum_increment_);
    pb_obj_.set_stop_sign(stop_sign_);
    pb_obj_.set_stop_price(stop_price_);
    pb_obj_.set_date(date_);
}

template <>
void OfferDataNymPB::hookAfterUnpack()
{
    gui_label_ = pb_obj_.gui_label();
    valid_from_ = pb_obj_.valid_from();
    valid_to_ = pb_obj_.valid_to();
    notary_id_ = pb_obj_.notary_id();
    instrument_definition_id_ = pb_obj_.instrument_definition_id();
    asset_acct_id_ = pb_obj_.asset_acct_id();
    currency_type_id_ = pb_obj_.currency_type_id();
    currency_acct_id_ = pb_obj_.currency_acct_id();
    selling_ = pb_obj_.selling();
    scale_ = pb_obj_.scale();
    price_per_scale_ = pb_obj_.price_per_scale();
    transaction_id_ = pb_obj_.transaction_id();
    total_assets_ = pb_obj_.total_assets();
    finished_so_far_ = pb_obj_.finished_so_far();
    minimum_increment_ = pb_obj_.minimum_increment();
    stop_sign_ = pb_obj_.stop_sign();
    stop_price_ = pb_obj_.stop_price();
    date_ = pb_obj_.date();
}

template <>
void OfferListNymPB::hookBeforePack()
{
    OT_IMPLEMENT_PB_LIST_PACK(offers, OfferDataNym)
}

template <>
void OfferListNymPB::hookAfterUnpack()
{
    OT_IMPLEMENT_PB_LIST_UNPACK(offers, OfferDataNym, STORED_OBJ_OFFER_DATA_NYM)
}

template <>
void TradeDataNymPB::hookBeforePack()
{
    pb_obj_.set_gui_label(gui_label_);
    pb_obj_.set_transaction_id(transaction_id_);
    pb_obj_.set_completed_count(completed_count_);
    pb_obj_.set_date(date_);
    pb_obj_.set_price(price_);
    pb_obj_.set_amount_sold(amount_sold_);
    pb_obj_.set_updated_id(updated_id_);
    pb_obj_.set_offer_price(offer_price_);
    pb_obj_.set_finished_so_far(finished_so_far_);
    pb_obj_.set_instrument_definition_id(instrument_definition_id_);
    pb_obj_.set_currency_id(currency_id_);
    pb_obj_.set_currency_paid(currency_paid_);
    pb_obj_.set_asset_acct_id(asset_acct_id_);
    pb_obj_.set_currency_acct_id(currency_acct_id_);
    pb_obj_.set_scale(scale_);
    pb_obj_.set_is_bid(is_bid_);
    pb_obj_.set_asset_receipt(asset_receipt_);
    pb_obj_.set_currency_receipt(currency_receipt_);
    pb_obj_.set_final_receipt(final_receipt_);
}

template <>
void TradeDataNymPB::hookAfterUnpack()
{
    gui_label_ = pb_obj_.gui_label();
    transaction_id_ = pb_obj_.transaction_id();
    completed_count_ = pb_obj_.completed_count();
    date_ = pb_obj_.date();
    price_ = pb_obj_.price();
    amount_sold_ = pb_obj_.amount_sold();
    updated_id_ = pb_obj_.updated_id();
    offer_price_ = pb_obj_.offer_price();
    finished_so_far_ = pb_obj_.finished_so_far();
    instrument_definition_id_ = pb_obj_.instrument_definition_id();
    currency_id_ = pb_obj_.currency_id();
    currency_paid_ = pb_obj_.currency_paid();
    asset_acct_id_ = pb_obj_.asset_acct_id();
    currency_acct_id_ = pb_obj_.currency_acct_id();
    scale_ = pb_obj_.scale();
    is_bid_ = pb_obj_.is_bid();
    asset_receipt_ = pb_obj_.asset_receipt();
    currency_receipt_ = pb_obj_.currency_receipt();
    final_receipt_ = pb_obj_.final_receipt();
}

template <>
void TradeListNymPB::hookBeforePack()
{
    OT_IMPLEMENT_PB_LIST_PACK(trades, TradeDataNym)
}

template <>
void TradeListNymPB::hookAfterUnpack()
{
    OT_IMPLEMENT_PB_LIST_UNPACK(trades, TradeDataNym, STORED_OBJ_TRADE_DATA_NYM)
}

#endif  // defined (OTDB_PROTOCOL_BUFFERS)

//
// STORAGE :: GetPacker
//
// Use this to access the OTPacker, throughout duration of this Storage object.
// If it doesn't exist yet, this function will create it on the first call. (The
// parameter allows you the choose what type will be created, other than
// default.
// You probably won't use it. But if you do, you'll only call it once per
// instance
// of Storage.)
//
auto Storage::GetPacker(PackType ePackType) -> OTPacker*
{
    // Normally if you use Create(), the packer is created at that time.
    // However, in the future, coders using the API may create subclasses of
    // Storage through SWIG, which Create could not anticipate. This mechanism
    // makes sure that in those cases, the packer still gets set (on the first
    // Get() call), and the coder using the API still has the ability to choose
    // what type of packer will be used.
    //
    if (nullptr == packer_) { packer_ = OTPacker::Create(ePackType); }

    return packer_;  // May return nullptr. (If Create call above fails.)
}

// (SetPacker(), from .h file)
// This is called once, in the factory.
// void Storage::SetPacker(OTPacker& thePacker) { assert_true(nullptr ==
// packer_); packer_ =  &thePacker; }

//
// Factory for Storable objects...
//
auto Storage::CreateObject(const StoredObjectType& eType) -> Storable*
{
    OTPacker* pPacker = GetPacker();

    if (nullptr == pPacker) {
        LogError()()("Failed, since GetPacker() "
                     "returned nullptr.")
            .Flush();
        return nullptr;
    }

    Storable* pStorable = Storable::Create(eType, pPacker->GetType());

    return pStorable;  // May return nullptr.
}

// Factory for the Storage context itself.
//
auto Storage::Create(const StorageType& eStorageType, const PackType& ePackType)
    -> Storage*
{
    Storage* pStore = nullptr;

    switch (eStorageType) {
        case STORE_FILESYSTEM: {
            pStore = StorageFS::Instantiate();
            assert_false(nullptr == pStore);
        } break;
        case STORE_TYPE_SUBCLASS:
        default: {
            LogError()()("Failed: Unknown storage type.").Flush();
        }
    }

    // IF we successfully created the storage context, now let's
    // try to create the packer that goes with it.
    // (They are created together and linked until death.)

    if (nullptr != pStore) {
        OTPacker* pPacker = OTPacker::Create(ePackType);

        if (nullptr == pPacker) {
            LogError()()("Failed while creating packer.").Flush();

            // For whatever reason, we failed. Memory issues or whatever.
            delete pStore;
            pStore = nullptr;
            return nullptr;
        }

        // Now they're married.
        pStore->SetPacker(*pPacker);
    } else {
        LogError()()("Failed, since pStore is nullptr.").Flush();
    }

    return pStore;  // Possible to return nullptr.
}

auto Storage::GetType() const -> StorageType
{
    // If I find the type, then I return it. Otherwise I ASSUME
    // that the coder using the API has subclassed Storage, and
    // that this is a custom Storage type invented by the API user.

    if (typeid(*this) == typeid(StorageFS)) {
        return STORE_FILESYSTEM;
        //    else if (typeid(*this) == typeid(StorageCouchDB))
        //        return STORE_COUCH_DB;
        //  Etc.
        //
    } else {
        return STORE_TYPE_SUBCLASS;  // The Java coder using API must have
    }
    // subclassed Storage himself.
}

auto Storage::StoreString(
    const api::Session& api,
    const UnallocatedCString& strContents,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    auto ot_strFolder = String::Factory(strFolder),
         ot_oneStr = String::Factory(oneStr),
         ot_twoStr = String::Factory(twoStr),
         ot_threeStr = String::Factory(threeStr);
    assert_true(ot_strFolder->Exists(), "ot_strFolder is null");

    if (!ot_oneStr->Exists()) {
        assert_true(
            !ot_twoStr->Exists() && !ot_threeStr->Exists(), "bad options");
        ot_oneStr = String::Factory(strFolder.c_str());
        ot_strFolder = String::Factory(".");
    }

    OTPacker* pPacker = GetPacker();

    if (nullptr == pPacker) { return false; }

    PackedBuffer* pBuffer = pPacker->Pack(strContents);

    if (nullptr == pBuffer) { return false; }

    const bool bSuccess = onStorePackedBuffer(
        api,
        *pBuffer,
        dataFolder,
        ot_strFolder->Get(),
        ot_oneStr->Get(),
        twoStr,
        threeStr);

    // Don't want any leaks here, do we?
    delete pBuffer;
    pBuffer = nullptr;

    return bSuccess;
}

auto Storage::QueryString(
    const api::Session& api,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> UnallocatedCString
{
    UnallocatedCString theString("");

    OTPacker* pPacker = GetPacker();

    if (nullptr == pPacker) { return theString; }

    PackedBuffer* pBuffer = pPacker->CreateBuffer();

    if (nullptr == pBuffer) { return theString; }

    // Below this point, responsible for pBuffer.

    const bool bSuccess = onQueryPackedBuffer(
        api, *pBuffer, dataFolder, strFolder, oneStr, twoStr, threeStr);

    if (!bSuccess) {
        delete pBuffer;
        pBuffer = nullptr;
        return theString;
    }

    // We got the packed buffer back from the query!
    // Now let's unpack it and return the Storable object.

    const bool bUnpacked = pPacker->Unpack(*pBuffer, theString);

    if (!bUnpacked) {
        delete pBuffer;
        theString = "";
        return theString;
    }

    // Success :-)

    // Don't want any leaks here, do we?
    delete pBuffer;
    pBuffer = nullptr;

    return theString;
}

// For when you want NO PACKING.

auto Storage::StorePlainString(
    const api::Session& api,
    const UnallocatedCString& strContents,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    return onStorePlainString(
        api, strContents, dataFolder, strFolder, oneStr, twoStr, threeStr);
}

auto Storage::QueryPlainString(
    const api::Session& api,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> UnallocatedCString
{
    UnallocatedCString theString("");

    if (!onQueryPlainString(
            api, theString, dataFolder, strFolder, oneStr, twoStr, threeStr)) {
        theString = "";
    }

    return theString;
}

auto Storage::StoreObject(
    const api::Session& api,
    Storable& theContents,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    OTPacker* pPacker = GetPacker();

    if (nullptr == pPacker) {

        LogError()()("No packer allocated.").Flush();

        return false;
    }

    PackedBuffer* pBuffer = pPacker->Pack(theContents);

    if (nullptr == pBuffer) {
        LogError()()("Packing failed.").Flush();
        return false;
    }

    const bool bSuccess = onStorePackedBuffer(
        api, *pBuffer, dataFolder, strFolder, oneStr, twoStr, threeStr);

    if (!bSuccess) {
        LogError()()("Storing failed calling "
                     "onStorePackedBuffer.")
            .Flush();
        return false;
    }

    // Don't want any leaks here, do we?
    delete pBuffer;

    return bSuccess;
}

// Use %newobject Storage::Query();
//
auto Storage::QueryObject(
    const api::Session& api,
    const StoredObjectType& theObjectType,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> Storable*
{
    OTPacker* pPacker = GetPacker();

    if (nullptr == pPacker) { return nullptr; }

    PackedBuffer* pBuffer = pPacker->CreateBuffer();

    if (nullptr == pBuffer) { return nullptr; }

    // Below this point, responsible for pBuffer.

    Storable* pStorable = CreateObject(theObjectType);

    if (nullptr == pStorable) {
        delete pBuffer;
        return nullptr;
    }

    // Below this point, responsible for pBuffer AND pStorable.

    const bool bSuccess = onQueryPackedBuffer(
        api, *pBuffer, dataFolder, strFolder, oneStr, twoStr, threeStr);

    if (!bSuccess) {
        delete pBuffer;
        delete pStorable;

        return nullptr;
    }

    // We got the packed buffer back from the query!
    // Now let's unpack it and return the Storable object.

    const bool bUnpacked = pPacker->Unpack(*pBuffer, *pStorable);

    if (!bUnpacked) {
        delete pBuffer;
        delete pStorable;

        return nullptr;
    }

    // Success :-)

    // Don't want any leaks here, do we?
    delete pBuffer;

    return pStorable;  // caller is responsible to delete.
}

auto Storage::EncodeObject(const api::Session& api, Storable& theContents)
    -> UnallocatedCString
{
    UnallocatedCString strReturnValue("");

    OTPacker* pPacker = GetPacker();

    if (nullptr == pPacker) {
        LogError()()("No packer allocated.").Flush();
        return strReturnValue;
    }

    PackedBuffer* pBuffer = pPacker->Pack(theContents);

    if (nullptr == pBuffer) {
        LogError()()("Packing failed.").Flush();
        return strReturnValue;
    }

    // OTPackedBuffer:
    //        virtual const    std::uint8_t *    GetData()=0;
    //        virtual            std::size_t            GetSize()=0;
    //
    const auto nNewSize = pBuffer->GetSize();
    const auto* pNewData =
        reinterpret_cast<const std::byte*>(pBuffer->GetData());

    if ((nNewSize < 1) || (nullptr == pNewData)) {
        delete pBuffer;
        pBuffer = nullptr;

        LogError()()("Packing failed (2).").Flush();
        return strReturnValue;
    }

    const auto theData = ByteArray{pNewData, nNewSize};
    const auto theArmor = api.Factory().Internal().Armored(theData);

    strReturnValue.assign(theArmor->Get(), theArmor->GetLength());

    // Don't want any leaks here, do we?
    delete pBuffer;

    return strReturnValue;
}

// Use %newobject Storage::DecodeObject();
//
auto Storage::DecodeObject(
    const api::Crypto& crypto,
    const StoredObjectType& theObjectType,
    const UnallocatedCString& strInput) -> Storable*
{
    if (strInput.size() < 1) { return nullptr; }

    OTPacker* pPacker = GetPacker();

    if (nullptr == pPacker) { return nullptr; }

    PackedBuffer* pBuffer = pPacker->CreateBuffer();

    if (nullptr == pBuffer) { return nullptr; }

    // Below this point, responsible for pBuffer.

    Storable* pStorable = CreateObject(theObjectType);

    if (nullptr == pStorable) {
        delete pBuffer;
        return nullptr;
    }

    // Below this point, responsible for pBuffer AND pStorable.

    auto theArmor = Armored::Factory(crypto);
    theArmor->Set(
        strInput.c_str(), static_cast<std::uint32_t>(strInput.size()));
    const auto thePayload = ByteArray{theArmor};

    // Put thePayload's contents into pBuffer here.
    //
    pBuffer->SetData(
        static_cast<const std::uint8_t*>(thePayload.data()), thePayload.size());

    // Now let's unpack it and return the Storable object.

    const bool bUnpacked = pPacker->Unpack(*pBuffer, *pStorable);

    if (!bUnpacked) {
        delete pBuffer;
        delete pStorable;

        return nullptr;
    }

    // Success :-)

    // Don't want any leaks here, do we?
    delete pBuffer;

    return pStorable;  // caller is responsible to delete.
}

auto Storage::EraseValueByKey(
    const api::Session& api,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    const bool bSuccess =
        onEraseValueByKey(api, dataFolder, strFolder, oneStr, twoStr, threeStr);

    if (!bSuccess) {
        LogError()()("Failed trying to erase a value "
                     "(while calling onEraseValueByKey).")
            .Flush();
    }

    return bSuccess;
}

// STORAGE FS  (OTDB::StorageFS is the filesystem version of OTDB::Storage.)

/*
 - Based on the input, constructs the full path and returns it in strOutput.
 - This function will try to create all the folders leading up to the
   file itself.
 - Also returns true/false based on whether the path actually exists.
 - If some failure occurs along the way, the path returned will not be the
   full path, but the path where the failure occurred.

 New return values:

 -1        -- Error
  0        -- File not found
  1+    -- File found and it's length.

 */
auto StorageFS::ConstructAndCreatePath(
    const api::Session& api,
    UnallocatedCString& strOutput,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> std::int64_t
{
    return ConstructAndConfirmPathImp(
        api, true, strOutput, dataFolder, strFolder, oneStr, twoStr, threeStr);
}

auto StorageFS::ConstructAndConfirmPath(
    const api::Session& api,
    UnallocatedCString& strOutput,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> std::int64_t
{
    return ConstructAndConfirmPathImp(
        api, false, strOutput, dataFolder, strFolder, oneStr, twoStr, threeStr);
}

auto StorageFS::ConstructAndConfirmPathImp(
    const api::Session& api,
    const bool bMakePath,
    UnallocatedCString& strOutput,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& zeroStr,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> std::int64_t
{
    const UnallocatedCString strRoot(dataFolder.c_str());
    const UnallocatedCString strZero(3 > zeroStr.length() ? "" : zeroStr);
    const UnallocatedCString strOne(3 > oneStr.length() ? "" : oneStr);
    const UnallocatedCString strTwo(3 > twoStr.length() ? "" : twoStr);
    const UnallocatedCString strThree(3 > threeStr.length() ? "" : threeStr);

    // must be 3chars in length, or equal to "."
    if (strZero.empty() && (0 != zeroStr.compare("."))) {
        LogError()()("Empty: zeroStr"
                     " is too short (and not)! "
                     "zeroStr was: ")(zeroStr)(".")
            .Flush();
        return -1;
    }

    // the first string must not be empty
    if (strOne.empty()) {
        LogError()()("Empty: oneStr is passed in!").Flush();
        return -2;
    }

    // if the second string is empty, so must the third.
    if (strTwo.empty() && !strThree.empty()) {
        LogError()()("Error: strThree passed in: ")(
            strThree)(" while strTwo is empty!")
            .Flush();
        return -3;
    }

    const bool bHaveZero = !strZero.empty();
    const bool bOneIsLast = strTwo.empty();
    const bool bTwoIsLast = !bOneIsLast && strThree.empty();
    const bool bThreeIsLast = !bOneIsLast && !bTwoIsLast;

    // main vairables;
    UnallocatedCString strBufFolder("");
    UnallocatedCString strBufPath("");

    // main block
    {
        // root (either way)
        strBufFolder += strRoot;

        // Zero
        if (bHaveZero) {
            strBufFolder += strZero;
            strBufFolder += "/";
        }

        // One
        if (bOneIsLast) {
            strBufPath = strBufFolder;
            strBufPath += strOne;
            goto ot_exit_block;
        }

        strBufFolder += strOne;
        strBufFolder += "/";

        // Two
        if (bTwoIsLast) {
            strBufPath = strBufFolder;
            strBufPath += strTwo;
            goto ot_exit_block;
        }

        strBufFolder += strTwo;
        strBufFolder += "/";

        // Three
        if (bThreeIsLast) {
            strBufPath = strBufFolder;
            strBufPath += threeStr;
            goto ot_exit_block;
        }
        // should never get here.
        LogAbort()().Abort();
    }
ot_exit_block:

    // set as constants. (no more changing).
    const auto strFolder = std::filesystem::path{strBufFolder};
    const auto strPath = std::filesystem::path{strBufPath};
    strOutput = strPath.string();

    if (bMakePath) { api.Internal().Paths().BuildFolderPath(strFolder); }

    {
        const bool bFolderExists = std::filesystem::exists(strFolder);

        if (bMakePath && !bFolderExists) {
            LogError()()("Error: was told to make path (")(
                strFolder)("), however cannot confirm the path!")
                .Flush();
            return -4;
        }
        if (!bMakePath && !bFolderExists) {
            LogDetail()()("Debug: Cannot find Folder: ")(strFolder).Flush();
        }
    }

    {
        auto lFileLength = 0_uz;
        const bool bFileExists =
            api.Internal().Paths().FileExists(strPath, lFileLength);

        if (bFileExists) {
            return lFileLength;
        } else {
            return 0;
        }
    }
}

// Store/Retrieve an object. (Storable.)

auto StorageFS::onStorePackedBuffer(
    const api::Session& api,
    PackedBuffer& theBuffer,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    UnallocatedCString strOutput;

    if (0 >
        ConstructAndCreatePath(
            api, strOutput, dataFolder, strFolder, oneStr, twoStr, threeStr)) {
        LogError()()("Error writing to ")(strOutput)(".").Flush();
        return false;
    }

    // TODO: Should check here to see if there is a .lock file for the target...

    // TODO: If not, next I should actually create a .lock file for myself right
    // here..

    // SAVE to the file here
    std::ofstream ofs(strOutput.c_str(), std::ios::out | std::ios::binary);

    if (ofs.fail()) {
        LogError()()("Error opening file: ")(strOutput)(".").Flush();
        return false;
    }

    ofs.clear();
    const bool bSuccess = theBuffer.WriteToOStream(ofs);
    ofs.close();

    // TODO: Remove the .lock file.

    return bSuccess;
}

auto StorageFS::onQueryPackedBuffer(
    const api::Session& api,
    PackedBuffer& theBuffer,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    UnallocatedCString strOutput;

    const std::int64_t lRet = ConstructAndConfirmPath(
        api, strOutput, dataFolder, strFolder, oneStr, twoStr, threeStr);

    if (0 > lRet) {
        LogError()()("Error with ")(strOutput)(".").Flush();
        return false;
    } else if (0 == lRet) {
        LogDetail()()("Failure reading from ")(
            strOutput)(": file does not exist.")
            .Flush();
        return false;
    }

    // READ from the file here

    std::ifstream fin(strOutput.c_str(), std::ios::in | std::ios::binary);

    if (!fin.is_open()) {
        LogError()()("Error opening file: ")(strOutput)(".").Flush();
        return false;
    }

    const bool bSuccess = theBuffer.ReadFromIStream(fin, lRet);

    fin.close();

    return bSuccess;
}

// Store/Retrieve a plain string, (without any packing.)

auto StorageFS::onStorePlainString(
    const api::Session& api,
    const UnallocatedCString& theBuffer,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    UnallocatedCString strOutput;

    if (0 >
        ConstructAndCreatePath(
            api, strOutput, dataFolder, strFolder, oneStr, twoStr, threeStr)) {
        LogError()()("Error writing to ")(strOutput)(".").Flush();
        return false;
    }

    // TODO: Should check here to see if there is a .lock file for the target...

    // TODO: If not, next I should actually create a .lock file for myself right
    // here..

    // SAVE to the file here.
    //
    // Here's where the serialization code would be changed to CouchDB or
    // whatever.
    // In a key/value database, szFilename is the "key" and strFinal.Get() is
    // the "value".
    //
    std::ofstream ofs(strOutput.c_str(), std::ios::out | std::ios::binary);

    if (ofs.fail()) {
        LogError()()("Error opening file: ")(strOutput)(".").Flush();
        return false;
    }

    ofs.clear();
    ofs << theBuffer;
    const bool bSuccess = ofs.good();
    ofs.close();

    // TODO: Remove the .lock file.

    return bSuccess;
}

auto StorageFS::onQueryPlainString(
    const api::Session& api,
    UnallocatedCString& theBuffer,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    UnallocatedCString strOutput;

    const std::int64_t lRet = ConstructAndConfirmPath(
        api, strOutput, dataFolder, strFolder, oneStr, twoStr, threeStr);

    if (0 > lRet) {
        LogError()()("Error with ")(strOutput)(".").Flush();
        return false;
    } else if (0 == lRet) {
        LogDetail()()("Failure reading from ")(
            strOutput)(": file does not exist.")
            .Flush();
        return false;
    }

    // Open the file here

    std::ifstream fin(strOutput.c_str(), std::ios::in | std::ios::binary);

    if (!fin.is_open()) {
        LogError()()("Error opening file: ")(strOutput)(".").Flush();
        return false;
    }

    // Read from the file as a plain string.

    std::stringstream buffer;
    buffer << fin.rdbuf();

    bool bSuccess = fin.good();

    if (bSuccess) {
        theBuffer = buffer.str();  // here's the actual output of this function.
    } else {
        theBuffer = "";
        return false;
    }

    bSuccess = (theBuffer.length() > 0);

    fin.close();

    return bSuccess;
}

// Erase a value by location.
//
auto StorageFS::onEraseValueByKey(
    const api::Session& api,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    UnallocatedCString strOutput;

    if (0 >
        ConstructAndConfirmPath(
            api, strOutput, dataFolder, strFolder, oneStr, twoStr, threeStr)) {
        LogError()()(":Error: "
                     "Failed calling ConstructAndConfirmPath with: "
                     "strOutput: ")(strOutput)(" | strFolder: ")(
            strFolder)(" | oneStr: ")(oneStr)(" | twoStr: ")(
            twoStr)(" | threeStr: ")(threeStr)(".")
            .Flush();

        return false;
    }

    // TODO: Should check here to see if there is a .lock file for the target...

    // TODO: If not, next I should actually create a .lock file for myself right
    // here..

    // SAVE to the file here. (a blank string.)
    //
    // Here's where the serialization code would be changed to CouchDB or
    // whatever.
    // In a key/value database, szFilename is the "key" and strFinal.Get() is
    // the "value".
    //
    std::ofstream ofs(strOutput.c_str(), std::ios::out | std::ios::binary);

    if (ofs.fail()) {
        LogError()()("Error opening file: ")(strOutput)(".").Flush();
        return false;
    }

    ofs.clear();
    ofs << "(This space intentionally left blank.)\n";
    bool bSuccess{false};
    ofs.close();
    // Note: I bet you think I should be overwriting the file 7 times here with
    // random data, right? Wrong: YOU need to override OTStorage and create your
    // own subclass, where you can override onEraseValueByKey and do that stuff
    // yourself. It's outside of the scope of OT.

    if (std::remove(strOutput.c_str()) != 0) {
        bSuccess = false;
        LogError()()("** Failed trying to delete file: ")(strOutput)(".")
            .Flush();
    } else {
        bSuccess = true;
        LogVerbose()()("** Success deleting file:  ")(strOutput).Flush();
    }

    // TODO: Remove the .lock file.

    return bSuccess;
}

// Constructor for Filesystem storage context.
//
StorageFS::StorageFS()
    : Storage()
{
}

StorageFS::~StorageFS() = default;

// See if the file is there.

auto StorageFS::Exists(
    const api::Session& api,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> bool
{
    UnallocatedCString strOutput;

    return (
        0 <
        ConstructAndConfirmPath(
            api, strOutput, dataFolder, strFolder, oneStr, twoStr, threeStr));
}

// Returns path size, plus path in strOutput.
//
auto StorageFS::FormPathString(
    const api::Session& api,
    UnallocatedCString& strOutput,
    const UnallocatedCString& dataFolder,
    const UnallocatedCString& strFolder,
    const UnallocatedCString& oneStr,
    const UnallocatedCString& twoStr,
    const UnallocatedCString& threeStr) -> std::int64_t
{
    return ConstructAndConfirmPath(
        api, strOutput, dataFolder, strFolder, oneStr, twoStr, threeStr);
}
}  // namespace opentxs::OTDB
