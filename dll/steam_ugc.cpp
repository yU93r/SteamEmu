/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

#include "dll/steam_ugc.h"

UGCQueryHandle_t Steam_UGC::new_ugc_query(bool return_all_subscribed, std::set<PublishedFileId_t> return_only)
{
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    ++handle;
    if ((handle == 0) || (handle == k_UGCQueryHandleInvalid)) handle = 50;

    struct UGC_query query{};
    query.handle = handle;
    query.return_all_subscribed = return_all_subscribed;
    query.return_only = return_only;
    ugc_queries.push_back(query);
    PRINT_DEBUG("handle = %llu", query.handle);
    return query.handle;
}

std::optional<Mod_entry> Steam_UGC::get_query_ugc(UGCQueryHandle_t handle, uint32 index)
{
    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return std::nullopt;
    if (index >= request->results.size()) return std::nullopt;

    auto it = request->results.begin();
    std::advance(it, index);
    
    PublishedFileId_t file_id = *it;
    if (!settings->isModInstalled(file_id)) return std::nullopt;

    return settings->getMod(file_id);
}

std::optional<std::vector<std::string>> Steam_UGC::get_query_ugc_tags(UGCQueryHandle_t handle, uint32 index)
{
    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return std::nullopt;

    auto tags_tokens = std::vector<std::string>{};
    std::stringstream ss(res.value().tags);
    std::string tmp{};
    while(ss >> tmp) {
        if (tmp.back() == ',') tmp = tmp.substr(0, tmp.size() - 1);
        tags_tokens.push_back(tmp);
    }

    return tags_tokens;

}

void Steam_UGC::set_details(PublishedFileId_t id, SteamUGCDetails_t *pDetails)
{
    if (pDetails) {
        memset(pDetails, 0, sizeof(SteamUGCDetails_t));

        pDetails->m_nPublishedFileId = id;

        if (settings->isModInstalled(id)) {
            PRINT_DEBUG("  mod is installed, setting details");
            pDetails->m_eResult = k_EResultOK;

            auto mod = settings->getMod(id);
            pDetails->m_bAcceptedForUse = mod.acceptedForUse;
            pDetails->m_bBanned = mod.banned;
            pDetails->m_bTagsTruncated = mod.tagsTruncated;
            pDetails->m_eFileType = mod.fileType;
            pDetails->m_eVisibility = mod.visibility;
            pDetails->m_hFile = mod.handleFile;
            pDetails->m_hPreviewFile = mod.handlePreviewFile;
            pDetails->m_nConsumerAppID = settings->get_local_game_id().AppID();
            pDetails->m_nCreatorAppID = settings->get_local_game_id().AppID();
            pDetails->m_nFileSize = mod.primaryFileSize;
            pDetails->m_nPreviewFileSize = mod.previewFileSize;
            pDetails->m_rtimeCreated = mod.timeCreated;
            pDetails->m_rtimeUpdated = mod.timeUpdated;
            pDetails->m_ulSteamIDOwner = settings->get_local_steam_id().ConvertToUint64();

            pDetails->m_rtimeAddedToUserList = mod.timeAddedToUserList;
            pDetails->m_unVotesUp = mod.votesUp;
            pDetails->m_unVotesDown = mod.votesDown;
            pDetails->m_flScore = mod.score;

            mod.primaryFileName.copy(pDetails->m_pchFileName, sizeof(pDetails->m_pchFileName) - 1);
            mod.description.copy(pDetails->m_rgchDescription, sizeof(pDetails->m_rgchDescription) - 1);
            mod.tags.copy(pDetails->m_rgchTags, sizeof(pDetails->m_rgchTags) - 1);
            mod.title.copy(pDetails->m_rgchTitle, sizeof(pDetails->m_rgchTitle) - 1);
            mod.workshopItemURL.copy(pDetails->m_rgchURL, sizeof(pDetails->m_rgchURL) - 1);

            // TODO should we enable this?
            // pDetails->m_unNumChildren = mod.numChildren;

            // TODO make sure the filesize is good
            pDetails->m_ulTotalFilesSize = mod.total_files_sizes;
        } else {
            PRINT_DEBUG("  mod isn't installed, returning failure");
            pDetails->m_eResult = k_EResultFail;
        }
    }
}

void Steam_UGC::read_ugc_favorites()
{
    if (!local_storage->file_exists("", ugc_favorits_file)) return;

    unsigned int size = local_storage->file_size("", ugc_favorits_file);
    if (!size) return;

    std::string data(size, '\0');
    int read = local_storage->get_data("", ugc_favorits_file, &data[0], (unsigned int)data.size());
    if ((size_t)read != data.size()) return;

    std::stringstream ss(data);
    std::string line{};
    while (std::getline(ss, line)) {
        try
        {
            unsigned long long fav_id = std::stoull(line);
            favorites.insert(fav_id);
            PRINT_DEBUG("added item to favorites %llu", fav_id);
        } catch(...) { }
    }
    
}

bool Steam_UGC::write_ugc_favorites()
{
    std::stringstream ss{};
    for (auto id : favorites) {
        ss << id << "\n";
        ss.flush();
    }
    auto file_data = ss.str();
    int stored = local_storage->store_data("", ugc_favorits_file, &file_data[0], static_cast<unsigned int>(file_data.size()));
    return (size_t)stored == file_data.size();
}


Steam_UGC::Steam_UGC(class Settings *settings, class Ugc_Remote_Storage_Bridge *ugc_bridge, class Local_Storage *local_storage, class SteamCallResults *callback_results, class SteamCallBacks *callbacks)
{
    this->settings = settings;
    this->ugc_bridge = ugc_bridge;
    this->local_storage = local_storage;
    this->callbacks = callbacks;
    this->callback_results = callback_results;

    read_ugc_favorites();
}


// Query UGC associated with a user. Creator app id or consumer app id must be valid and be set to the current running app. unPage should start at 1.
UGCQueryHandle_t Steam_UGC::CreateQueryUserUGCRequest( AccountID_t unAccountID, EUserUGCList eListType, EUGCMatchingUGCType eMatchingUGCType, EUserUGCListSortOrder eSortOrder, AppId_t nCreatorAppID, AppId_t nConsumerAppID, uint32 unPage )
{
    PRINT_DEBUG("%u %i %i %i %u %u %u", unAccountID, eListType, eMatchingUGCType, eSortOrder, nCreatorAppID, nConsumerAppID, unPage);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    if (nCreatorAppID != settings->get_local_game_id().AppID() || nConsumerAppID != settings->get_local_game_id().AppID()) return k_UGCQueryHandleInvalid;
    if (unPage < 1) return k_UGCQueryHandleInvalid;
    if (eListType < 0) return k_UGCQueryHandleInvalid;
    if (unAccountID != settings->get_local_steam_id().GetAccountID()) return k_UGCQueryHandleInvalid;
    
    // TODO
    return new_ugc_query(eListType == k_EUserUGCList_Subscribed || eListType == k_EUserUGCList_Published);
}


// Query for all matching UGC. Creator app id or consumer app id must be valid and be set to the current running app. unPage should start at 1.
UGCQueryHandle_t Steam_UGC::CreateQueryAllUGCRequest( EUGCQuery eQueryType, EUGCMatchingUGCType eMatchingeMatchingUGCTypeFileType, AppId_t nCreatorAppID, AppId_t nConsumerAppID, uint32 unPage )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nCreatorAppID != settings->get_local_game_id().AppID() || nConsumerAppID != settings->get_local_game_id().AppID()) return k_UGCQueryHandleInvalid;
    if (unPage < 1) return k_UGCQueryHandleInvalid;
    if (eQueryType < 0) return k_UGCQueryHandleInvalid;
    
    // TODO
    return new_ugc_query();
}

// Query for all matching UGC using the new deep paging interface. Creator app id or consumer app id must be valid and be set to the current running app. pchCursor should be set to NULL or "*" to get the first result set.
UGCQueryHandle_t Steam_UGC::CreateQueryAllUGCRequest( EUGCQuery eQueryType, EUGCMatchingUGCType eMatchingeMatchingUGCTypeFileType, AppId_t nCreatorAppID, AppId_t nConsumerAppID, const char *pchCursor )
{
    PRINT_DEBUG("other");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nCreatorAppID != settings->get_local_game_id().AppID() || nConsumerAppID != settings->get_local_game_id().AppID()) return k_UGCQueryHandleInvalid;
    if (eQueryType < 0) return k_UGCQueryHandleInvalid;
    
    // TODO
    return new_ugc_query();
}

// Query for the details of the given published file ids (the RequestUGCDetails call is deprecated and replaced with this)
UGCQueryHandle_t Steam_UGC::CreateQueryUGCDetailsRequest( PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs )
{
    PRINT_DEBUG("%p, max file IDs = [%u]", pvecPublishedFileID, unNumPublishedFileIDs);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (!pvecPublishedFileID) return k_UGCQueryHandleInvalid;
    if (unNumPublishedFileIDs < 1) return k_UGCQueryHandleInvalid;

    // TODO
    std::set<PublishedFileId_t> only(pvecPublishedFileID, pvecPublishedFileID + unNumPublishedFileIDs);
    
#ifndef EMU_RELEASE_BUILD
    for (const auto &id : only) {
        PRINT_DEBUG("  file ID = %llu", id);
    }
#endif

    return new_ugc_query(false, only);
}


// Send the query to Steam
STEAM_CALL_RESULT( SteamUGCQueryCompleted_t )
SteamAPICall_t Steam_UGC::SendQueryUGCRequest( UGCQueryHandle_t handle )
{
    PRINT_DEBUG("%llu", handle);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return k_uAPICallInvalid;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request)
        return k_uAPICallInvalid;

    if (request->return_all_subscribed) {
        request->results = std::set<PublishedFileId_t>(ugc_bridge->subbed_mods_itr_begin(), ugc_bridge->subbed_mods_itr_end());
    }

    if (request->return_only.size()) {
        for (auto & s : request->return_only) {
            if (ugc_bridge->has_subbed_mod(s)) {
                request->results.insert(s);
            }
        }
    }

    // send these handles to steam_remote_storage since the game will later
    // call Steam_Remote_Storage::UGCDownload() with these files handles (primary + preview)
    for (auto fileid : request->results) {
        auto mod = settings->getMod(fileid);
        ugc_bridge->add_ugc_query_result(mod.handleFile, fileid, true);
        ugc_bridge->add_ugc_query_result(mod.handlePreviewFile, fileid, false);
    }

    SteamUGCQueryCompleted_t data = {};
    data.m_handle = handle;
    data.m_eResult = k_EResultOK;
    data.m_unNumResultsReturned = static_cast<uint32>(request->results.size());
    data.m_unTotalMatchingResults = static_cast<uint32>(request->results.size());
    data.m_bCachedData = false;
    
    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}


// Retrieve an individual result after receiving the callback for querying UGC
bool Steam_UGC::GetQueryUGCResult( UGCQueryHandle_t handle, uint32 index, SteamUGCDetails_t *pDetails )
{
    PRINT_DEBUG("%llu %u %p", handle, index, pDetails);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) {
        return false;
    }

    if (index >= request->results.size()) {
        return false;
    }

    auto it = request->results.begin();
    std::advance(it, index);
    PublishedFileId_t file_id = *it;
    set_details(file_id, pDetails);
    return true;
}

std::optional<std::string> Steam_UGC::get_query_ugc_tag(UGCQueryHandle_t handle, uint32 index, uint32 indexTag)
{
    auto res = get_query_ugc_tags(handle, index);
    if (!res.has_value()) return std::nullopt;
    if (indexTag >= res.value().size()) return std::nullopt;

    std::string tmp = res.value()[indexTag];
    if (tmp.back() == ',') {
        tmp = tmp.substr(0, tmp.size() - 1);
    }
    return tmp;
}

uint32 Steam_UGC::GetQueryUGCNumTags( UGCQueryHandle_t handle, uint32 index )
{
    PRINT_DEBUG_TODO();
    // TODO is this correct?
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return 0;
    
    auto res = get_query_ugc_tags(handle, index);
    return res.has_value() ? static_cast<uint32>(res.value().size()) : 0;
}

bool Steam_UGC::GetQueryUGCTag( UGCQueryHandle_t handle, uint32 index, uint32 indexTag, STEAM_OUT_STRING_COUNT( cchValueSize ) char* pchValue, uint32 cchValueSize )
{
    PRINT_DEBUG_TODO();
    // TODO is this correct?
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;
    if (!pchValue || !cchValueSize) return false;

    auto res = get_query_ugc_tag(handle, index, indexTag);
    if (!res.has_value()) return false;

    memset(pchValue, 0, cchValueSize);
    res.value().copy(pchValue, cchValueSize - 1);
    return true;
}

bool Steam_UGC::GetQueryUGCTagDisplayName( UGCQueryHandle_t handle, uint32 index, uint32 indexTag, STEAM_OUT_STRING_COUNT( cchValueSize ) char* pchValue, uint32 cchValueSize )
{
    PRINT_DEBUG_TODO();
    // TODO is this correct?
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;
    if (!pchValue || !cchValueSize) return false;

    auto res = get_query_ugc_tag(handle, index, indexTag);
    if (!res.has_value()) return false;

    memset(pchValue, 0, cchValueSize);
    res.value().copy(pchValue, cchValueSize - 1);
    return true;
}

bool Steam_UGC::GetQueryUGCPreviewURL( UGCQueryHandle_t handle, uint32 index, STEAM_OUT_STRING_COUNT(cchURLSize) char *pchURL, uint32 cchURLSize )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    //TODO: escape simulator tries downloading this url and unsubscribes if it fails
    if (handle == k_UGCQueryHandleInvalid) return false;
    if (!pchURL || !cchURLSize) return false;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return false;

    auto &mod = res.value();
    PRINT_DEBUG("Steam_UGC:GetQueryUGCPreviewURL: '%s'", mod.previewURL.c_str());
    memset(pchURL, 0, cchURLSize);
    mod.previewURL.copy(pchURL, cchURLSize - 1);
    return true;
}


bool Steam_UGC::GetQueryUGCMetadata( UGCQueryHandle_t handle, uint32 index, STEAM_OUT_STRING_COUNT(cchMetadatasize) char *pchMetadata, uint32 cchMetadatasize )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;

    return false;
}


bool Steam_UGC::GetQueryUGCChildren( UGCQueryHandle_t handle, uint32 index, PublishedFileId_t* pvecPublishedFileID, uint32 cMaxEntries )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return false;
}


bool Steam_UGC::GetQueryUGCStatistic( UGCQueryHandle_t handle, uint32 index, EItemStatistic eStatType, uint64 *pStatValue )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return false;
}

bool Steam_UGC::GetQueryUGCStatistic( UGCQueryHandle_t handle, uint32 index, EItemStatistic eStatType, uint32 *pStatValue )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return false;
}

uint32 Steam_UGC::GetQueryUGCNumAdditionalPreviews( UGCQueryHandle_t handle, uint32 index )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return 0;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return 0;
    
    return 0;
}


bool Steam_UGC::GetQueryUGCAdditionalPreview( UGCQueryHandle_t handle, uint32 index, uint32 previewIndex, STEAM_OUT_STRING_COUNT(cchURLSize) char *pchURLOrVideoID, uint32 cchURLSize, STEAM_OUT_STRING_COUNT(cchURLSize) char *pchOriginalFileName, uint32 cchOriginalFileNameSize, EItemPreviewType *pPreviewType )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return false;
}

bool Steam_UGC::GetQueryUGCAdditionalPreview( UGCQueryHandle_t handle, uint32 index, uint32 previewIndex, char *pchURLOrVideoID, uint32 cchURLSize, bool *hz )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return false;
}

uint32 Steam_UGC::GetQueryUGCNumKeyValueTags( UGCQueryHandle_t handle, uint32 index )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return 0;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return 0;
    
    return 0;
}


bool Steam_UGC::GetQueryUGCKeyValueTag( UGCQueryHandle_t handle, uint32 index, uint32 keyValueTagIndex, STEAM_OUT_STRING_COUNT(cchKeySize) char *pchKey, uint32 cchKeySize, STEAM_OUT_STRING_COUNT(cchValueSize) char *pchValue, uint32 cchValueSize )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return false;
}

bool Steam_UGC::GetQueryUGCKeyValueTag( UGCQueryHandle_t handle, uint32 index, const char *pchKey, STEAM_OUT_STRING_COUNT(cchValueSize) char *pchValue, uint32 cchValueSize )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return false;
}

// TODO no public docs
// Some items can specify that they have a version that is valid for a range of game versions (Steam branch)
uint32 Steam_UGC::GetNumSupportedGameVersions( UGCQueryHandle_t handle, uint32 index )
{
    PRINT_DEBUG("%llu %u // TODO", handle, index);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return 0;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return 0;
    
    return 1;
}

// TODO no public docs
bool Steam_UGC::GetSupportedGameVersionData( UGCQueryHandle_t handle, uint32 index, uint32 versionIndex, STEAM_OUT_STRING_COUNT( cchGameBranchSize ) char *pchGameBranchMin, STEAM_OUT_STRING_COUNT( cchGameBranchSize ) char *pchGameBranchMax, uint32 cchGameBranchSize )
{
    PRINT_DEBUG("%llu %u %u // TODO", handle, index, versionIndex);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    if (versionIndex != 0) { // TODO I assume this is supposed to be an index in the range [ 0, GetNumSupportedGameVersions() )
        return false;
    }
    
    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return false;

    auto &mod = res.value();

    // TODO I assume each mod/workshop item has a min version and max version for the game
    if (pchGameBranchMin && static_cast<size_t>(cchGameBranchSize) > mod.min_game_branch.size()) {
        memset(pchGameBranchMin, 0, cchGameBranchSize);
        memcpy(pchGameBranchMin, mod.min_game_branch.c_str(), mod.min_game_branch.size());
    }
    if (pchGameBranchMax && static_cast<size_t>(cchGameBranchSize) > mod.max_game_branch.size()) {
        memset(pchGameBranchMax, 0, cchGameBranchSize);
        memcpy(pchGameBranchMax, mod.max_game_branch.c_str(), mod.max_game_branch.size());
    }

    return true;
}

uint32 Steam_UGC::GetQueryUGCContentDescriptors( UGCQueryHandle_t handle, uint32 index, EUGCContentDescriptorID *pvecDescriptors, uint32 cMaxEntries )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return 0;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return 0;

    return 0;
}

// Release the request to free up memory, after retrieving results
bool Steam_UGC::ReleaseQueryUGCRequest( UGCQueryHandle_t handle )
{
    PRINT_DEBUG("%llu", handle);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;

    ugc_queries.erase(request);
    return true;
}


// Options to set for querying UGC
bool Steam_UGC::AddRequiredTag( UGCQueryHandle_t handle, const char *pTagName )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

bool Steam_UGC::AddRequiredTagGroup( UGCQueryHandle_t handle, const SteamParamStringArray_t *pTagGroups )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

bool Steam_UGC::AddExcludedTag( UGCQueryHandle_t handle, const char *pTagName )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnOnlyIDs( UGCQueryHandle_t handle, bool bReturnOnlyIDs )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnKeyValueTags( UGCQueryHandle_t handle, bool bReturnKeyValueTags )
{
    PRINT_DEBUG_TODO();
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnLongDescription( UGCQueryHandle_t handle, bool bReturnLongDescription )
{
    PRINT_DEBUG_TODO();
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnMetadata( UGCQueryHandle_t handle, bool bReturnMetadata )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnChildren( UGCQueryHandle_t handle, bool bReturnChildren )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnAdditionalPreviews( UGCQueryHandle_t handle, bool bReturnAdditionalPreviews )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnTotalOnly( UGCQueryHandle_t handle, bool bReturnTotalOnly )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnPlaytimeStats( UGCQueryHandle_t handle, uint32 unDays )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetLanguage( UGCQueryHandle_t handle, const char *pchLanguage )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetAllowCachedResponse( UGCQueryHandle_t handle, uint32 unMaxAgeSeconds )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

// TODO no public docs
// allow ISteamUGC to be used in a tools like environment for users who have the appropriate privileges for the calling appid
bool Steam_UGC::SetAdminQuery( UGCUpdateHandle_t handle, bool bAdminQuery )
{
    PRINT_DEBUG("%llu %i // TODO", handle, (int)bAdminQuery);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    request->admin_query = bAdminQuery;
    return true;
}


// Options only for querying user UGC
bool Steam_UGC::SetCloudFileNameFilter( UGCQueryHandle_t handle, const char *pMatchCloudFileName )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


// Options only for querying all UGC
bool Steam_UGC::SetMatchAnyTag( UGCQueryHandle_t handle, bool bMatchAnyTag )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetSearchText( UGCQueryHandle_t handle, const char *pSearchText )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetRankedByTrendDays( UGCQueryHandle_t handle, uint32 unDays )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::AddRequiredKeyValueTag( UGCQueryHandle_t handle, const char *pKey, const char *pValue )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

bool Steam_UGC::SetTimeCreatedDateRange( UGCQueryHandle_t handle, RTime32 rtStart, RTime32 rtEnd )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

bool Steam_UGC::SetTimeUpdatedDateRange( UGCQueryHandle_t handle, RTime32 rtStart, RTime32 rtEnd )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

// DEPRECATED - Use CreateQueryUGCDetailsRequest call above instead!
SteamAPICall_t Steam_UGC::RequestUGCDetails( PublishedFileId_t nPublishedFileID, uint32 unMaxAgeSeconds )
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    SteamUGCRequestUGCDetailsResult_t data{};
    data.m_bCachedData = false;
    set_details(nPublishedFileID, &(data.m_details));
    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}

SteamAPICall_t Steam_UGC::RequestUGCDetails( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG("old");
    return RequestUGCDetails(nPublishedFileID, 0);
}


// Steam Workshop Creator API
STEAM_CALL_RESULT( CreateItemResult_t )
SteamAPICall_t Steam_UGC::CreateItem( AppId_t nConsumerAppId, EWorkshopFileType eFileType )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return k_uAPICallInvalid;
}
 // create new item for this app with no content attached yet


UGCUpdateHandle_t Steam_UGC::StartItemUpdate( AppId_t nConsumerAppId, PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return k_UGCUpdateHandleInvalid;
}
 // start an UGC item update. Set changed properties before commiting update with CommitItemUpdate()


bool Steam_UGC::SetItemTitle( UGCUpdateHandle_t handle, const char *pchTitle )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // change the title of an UGC item


bool Steam_UGC::SetItemDescription( UGCUpdateHandle_t handle, const char *pchDescription )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // change the description of an UGC item


bool Steam_UGC::SetItemUpdateLanguage( UGCUpdateHandle_t handle, const char *pchLanguage )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // specify the language of the title or description that will be set


bool Steam_UGC::SetItemMetadata( UGCUpdateHandle_t handle, const char *pchMetaData )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // change the metadata of an UGC item (max = k_cchDeveloperMetadataMax)


bool Steam_UGC::SetItemVisibility( UGCUpdateHandle_t handle, ERemoteStoragePublishedFileVisibility eVisibility )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // change the visibility of an UGC item


bool Steam_UGC::SetItemTags( UGCUpdateHandle_t updateHandle, const SteamParamStringArray_t *pTags )
{
    PRINT_DEBUG("old");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}

bool Steam_UGC::SetItemTags( UGCUpdateHandle_t updateHandle, const SteamParamStringArray_t *pTags, bool bAllowAdminTags )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // change the tags of an UGC item

bool Steam_UGC::SetItemContent( UGCUpdateHandle_t handle, const char *pszContentFolder )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // update item content from this local folder


bool Steam_UGC::SetItemPreview( UGCUpdateHandle_t handle, const char *pszPreviewFile )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 //  change preview image file for this item. pszPreviewFile points to local image file, which must be under 1MB in size

bool Steam_UGC::SetAllowLegacyUpload( UGCUpdateHandle_t handle, bool bAllowLegacyUpload )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}

bool Steam_UGC::RemoveAllItemKeyValueTags( UGCUpdateHandle_t handle )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // remove all existing key-value tags (you can add new ones via the AddItemKeyValueTag function)

bool Steam_UGC::RemoveItemKeyValueTags( UGCUpdateHandle_t handle, const char *pchKey )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // remove any existing key-value tags with the specified key


bool Steam_UGC::AddItemKeyValueTag( UGCUpdateHandle_t handle, const char *pchKey, const char *pchValue )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // add new key-value tags for the item. Note that there can be multiple values for a tag.


bool Steam_UGC::AddItemPreviewFile( UGCUpdateHandle_t handle, const char *pszPreviewFile, EItemPreviewType type )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 //  add preview file for this item. pszPreviewFile points to local file, which must be under 1MB in size


bool Steam_UGC::AddItemPreviewVideo( UGCUpdateHandle_t handle, const char *pszVideoID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 //  add preview video for this item


bool Steam_UGC::UpdateItemPreviewFile( UGCUpdateHandle_t handle, uint32 index, const char *pszPreviewFile )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 //  updates an existing preview file for this item. pszPreviewFile points to local file, which must be under 1MB in size


bool Steam_UGC::UpdateItemPreviewVideo( UGCUpdateHandle_t handle, uint32 index, const char *pszVideoID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 //  updates an existing preview video for this item


bool Steam_UGC::RemoveItemPreview( UGCUpdateHandle_t handle, uint32 index )
{
    PRINT_DEBUG("%llu %u", handle, index);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // remove a preview by index starting at 0 (previews are sorted)

bool Steam_UGC::AddContentDescriptor( UGCUpdateHandle_t handle, EUGCContentDescriptorID descid )
{
    PRINT_DEBUG("%llu %u", handle, descid);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}

bool Steam_UGC::RemoveContentDescriptor( UGCUpdateHandle_t handle, EUGCContentDescriptorID descid )
{
    PRINT_DEBUG("%llu %u", handle, descid);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}

// TODO no public docs
bool Steam_UGC::SetRequiredGameVersions( UGCUpdateHandle_t handle, const char *pszGameBranchMin, const char *pszGameBranchMax )
{
    PRINT_DEBUG("%llu '%s' '%s' // TODO", handle, pszGameBranchMin, pszGameBranchMax);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    if (pszGameBranchMin) request->min_branch = pszGameBranchMin;
    if (pszGameBranchMax) request->max_branch = pszGameBranchMax;
    return true;
}

STEAM_CALL_RESULT( SubmitItemUpdateResult_t )
SteamAPICall_t Steam_UGC::SubmitItemUpdate( UGCUpdateHandle_t handle, const char *pchChangeNote )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return k_uAPICallInvalid;
}
 // commit update process started with StartItemUpdate()


EItemUpdateStatus Steam_UGC::GetItemUpdateProgress( UGCUpdateHandle_t handle, uint64 *punBytesProcessed, uint64* punBytesTotal )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return k_EItemUpdateStatusInvalid;
}


// Steam Workshop Consumer API

STEAM_CALL_RESULT( SetUserItemVoteResult_t )
SteamAPICall_t Steam_UGC::SetUserItemVote( PublishedFileId_t nPublishedFileID, bool bVoteUp )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (!settings->isModInstalled(nPublishedFileID)) return k_uAPICallInvalid; // TODO is this correct
    
    auto mod  = settings->getMod(nPublishedFileID);
    SetUserItemVoteResult_t data{};
    data.m_eResult = EResult::k_EResultOK;
    data.m_nPublishedFileId = nPublishedFileID;
    if (bVoteUp) {
        ++mod.votesUp;
    } else {
        ++mod.votesDown;
    }
    settings->addModDetails(nPublishedFileID, mod);
    return callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
}


STEAM_CALL_RESULT( GetUserItemVoteResult_t )
SteamAPICall_t Steam_UGC::GetUserItemVote( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (nPublishedFileID == k_PublishedFileIdInvalid || !settings->isModInstalled(nPublishedFileID)) return k_uAPICallInvalid; // TODO is this correct

    auto mod  = settings->getMod(nPublishedFileID);
    GetUserItemVoteResult_t data{};
    data.m_eResult = EResult::k_EResultOK;
    data.m_nPublishedFileId = nPublishedFileID;
    data.m_bVotedDown = mod.votesDown;
    data.m_bVotedUp = mod.votesUp;
    data.m_bVoteSkipped = true;
    return callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
}


STEAM_CALL_RESULT( UserFavoriteItemsListChanged_t )
SteamAPICall_t Steam_UGC::AddItemToFavorites( AppId_t nAppId, PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG("%u %llu", nAppId, nPublishedFileID);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (nAppId == k_uAppIdInvalid || nAppId != settings->get_local_game_id().AppID()) return k_uAPICallInvalid; // TODO is this correct
    if (nPublishedFileID == k_PublishedFileIdInvalid || !settings->isModInstalled(nPublishedFileID)) return k_uAPICallInvalid; // TODO is this correct

    UserFavoriteItemsListChanged_t data{};
    data.m_nPublishedFileId = nPublishedFileID;
    data.m_bWasAddRequest = true;

    auto add = favorites.insert(nPublishedFileID);
    if (add.second) { // if new insertion
        PRINT_DEBUG(" adding new item to favorites");
        bool ok = write_ugc_favorites();
        data.m_eResult = ok ? EResult::k_EResultOK : EResult::k_EResultFail;
    } else { // nPublishedFileID already exists
        data.m_eResult = EResult::k_EResultOK;
    }
    
    return callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
}


STEAM_CALL_RESULT( UserFavoriteItemsListChanged_t )
SteamAPICall_t Steam_UGC::RemoveItemFromFavorites( AppId_t nAppId, PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (nAppId == k_uAppIdInvalid || nAppId != settings->get_local_game_id().AppID()) return k_uAPICallInvalid; // TODO is this correct
    if (nPublishedFileID == k_PublishedFileIdInvalid || !settings->isModInstalled(nPublishedFileID)) return k_uAPICallInvalid; // TODO is this correct

    UserFavoriteItemsListChanged_t data{};
    data.m_nPublishedFileId = nPublishedFileID;
    data.m_bWasAddRequest = false;

    auto removed = favorites.erase(nPublishedFileID);
    if (removed) {
        PRINT_DEBUG(" removing item from favorites");
        bool ok = write_ugc_favorites();
        data.m_eResult = ok ? EResult::k_EResultOK : EResult::k_EResultFail;
    } else { // nPublishedFileID didn't exist
        data.m_eResult = EResult::k_EResultOK;
    }
    
    return callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
}


STEAM_CALL_RESULT( RemoteStorageSubscribePublishedFileResult_t )
SteamAPICall_t Steam_UGC::SubscribeItem( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    RemoteStorageSubscribePublishedFileResult_t data;
    data.m_nPublishedFileId = nPublishedFileID;
    if (settings->isModInstalled(nPublishedFileID)) {
        data.m_eResult = k_EResultOK;
        ugc_bridge->add_subbed_mod(nPublishedFileID);
    } else {
        data.m_eResult = k_EResultFail;
    }
    return callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
}
 // subscribe to this item, will be installed ASAP

STEAM_CALL_RESULT( RemoteStorageUnsubscribePublishedFileResult_t )
SteamAPICall_t Steam_UGC::UnsubscribeItem( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    RemoteStorageUnsubscribePublishedFileResult_t data;
    data.m_nPublishedFileId = nPublishedFileID;
    if (!ugc_bridge->has_subbed_mod(nPublishedFileID)) {
        data.m_eResult = k_EResultFail; //TODO: check if this is accurate
    } else {
        data.m_eResult = k_EResultOK;
        ugc_bridge->remove_subbed_mod(nPublishedFileID);
    }

    return callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
}
 // unsubscribe from this item, will be uninstalled after game quits

uint32 Steam_UGC::GetNumSubscribedItems()
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    PRINT_DEBUG("  Steam_UGC::GetNumSubscribedItems = %zu", ugc_bridge->subbed_mods_count());
    return (uint32)ugc_bridge->subbed_mods_count();
}
 // number of subscribed items 

uint32 Steam_UGC::GetSubscribedItems( PublishedFileId_t* pvecPublishedFileID, uint32 cMaxEntries )
{
    PRINT_DEBUG("%p %u", pvecPublishedFileID, cMaxEntries);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if ((size_t)cMaxEntries > ugc_bridge->subbed_mods_count()) {
        cMaxEntries = (uint32)ugc_bridge->subbed_mods_count();
    }

    std::copy_n(ugc_bridge->subbed_mods_itr_begin(), cMaxEntries, pvecPublishedFileID);
    return cMaxEntries;
}
 // all subscribed item PublishFileIDs

// get EItemState flags about item on this client
uint32 Steam_UGC::GetItemState( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (ugc_bridge->has_subbed_mod(nPublishedFileID)) {
        if (settings->isModInstalled(nPublishedFileID)) {
            PRINT_DEBUG("  mod is subscribed and installed");
            return k_EItemStateInstalled | k_EItemStateSubscribed;
        }

        PRINT_DEBUG("  mod is subscribed");
        return k_EItemStateSubscribed;
    }

    PRINT_DEBUG("  mod isn't found");
    return k_EItemStateNone;
}


// get info about currently installed content on disc for items that have k_EItemStateInstalled set
// if k_EItemStateLegacyItem is set, pchFolder contains the path to the legacy file itself (not a folder)
bool Steam_UGC::GetItemInstallInfo( PublishedFileId_t nPublishedFileID, uint64 *punSizeOnDisk, STEAM_OUT_STRING_COUNT( cchFolderSize ) char *pchFolder, uint32 cchFolderSize, uint32 *punTimeStamp )
{
    PRINT_DEBUG("%llu %p %p [%u] %p", nPublishedFileID, punSizeOnDisk, pchFolder, cchFolderSize, punTimeStamp);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (!cchFolderSize) return false;
    if (!settings->isModInstalled(nPublishedFileID)) return false;

    auto mod = settings->getMod(nPublishedFileID);
    
    // I don't know if this is accurate behavior, but to avoid returning true with invalid data
    if ((cchFolderSize - 1) < mod.path.size()) { // -1 because the last char is reserved for null terminator
        PRINT_DEBUG("  ERROR mod path: '%s' [%zu bytes] cannot fit into the given buffer", mod.path.c_str(), mod.path.size());
        return false;
    }

    if (punSizeOnDisk) *punSizeOnDisk = mod.primaryFileSize;
    if (punTimeStamp) *punTimeStamp = mod.timeUpdated;
    if (pchFolder && cchFolderSize) {
        // human fall flat doesn't send a nulled buffer, and won't recognize the proper mod path because of that
        memset(pchFolder, 0, cchFolderSize);
        mod.path.copy(pchFolder, cchFolderSize - 1);
        PRINT_DEBUG("  final mod path: '%s'", pchFolder);
    }

    return true;
}


// get info about pending update for items that have k_EItemStateNeedsUpdate set. punBytesTotal will be valid after download started once
bool Steam_UGC::GetItemDownloadInfo( PublishedFileId_t nPublishedFileID, uint64 *punBytesDownloaded, uint64 *punBytesTotal )
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (!settings->isModInstalled(nPublishedFileID)) return false;

    auto mod = settings->getMod(nPublishedFileID);
    if (punBytesDownloaded) *punBytesDownloaded = mod.primaryFileSize;
    if (punBytesTotal) *punBytesTotal = mod.primaryFileSize;
    return true;
}

bool Steam_UGC::GetItemInstallInfo( PublishedFileId_t nPublishedFileID, uint64 *punSizeOnDisk, STEAM_OUT_STRING_COUNT( cchFolderSize ) char *pchFolder, uint32 cchFolderSize, bool *pbLegacyItem ) // returns true if item is installed
{
    PRINT_DEBUG("old");
    return GetItemInstallInfo(nPublishedFileID, punSizeOnDisk, pchFolder, cchFolderSize, (uint32*) nullptr);
}

bool Steam_UGC::GetItemUpdateInfo( PublishedFileId_t nPublishedFileID, bool *pbNeedsUpdate, bool *pbIsDownloading, uint64 *punBytesDownloaded, uint64 *punBytesTotal )
{
    PRINT_DEBUG("old");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    bool res = GetItemDownloadInfo(nPublishedFileID, punBytesDownloaded, punBytesTotal);
    if (res) {
        if (pbNeedsUpdate) *pbNeedsUpdate = false;
        if (pbIsDownloading) *pbIsDownloading = false;
    }
    return res;
}

bool Steam_UGC::GetItemInstallInfo( PublishedFileId_t nPublishedFileID, uint64 *punSizeOnDisk, char *pchFolder, uint32 cchFolderSize ) // returns true if item is installed
{
    PRINT_DEBUG("older");
    return GetItemInstallInfo(nPublishedFileID, punSizeOnDisk, pchFolder, cchFolderSize, (uint32*) nullptr);
}


// download new or update already installed item. If function returns true, wait for DownloadItemResult_t. If the item is already installed,
// then files on disk should not be used until callback received. If item is not subscribed to, it will be cached for some time.
// If bHighPriority is set, any other item download will be suspended and this item downloaded ASAP.
bool Steam_UGC::DownloadItem( PublishedFileId_t nPublishedFileID, bool bHighPriority )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    return false;
}


// game servers can set a specific workshop folder before issuing any UGC commands.
// This is helpful if you want to support multiple game servers running out of the same install folder
bool Steam_UGC::BInitWorkshopForGameServer( DepotId_t unWorkshopDepotID, const char *pszFolder )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}


// SuspendDownloads( true ) will suspend all workshop downloads until SuspendDownloads( false ) is called or the game ends
void Steam_UGC::SuspendDownloads( bool bSuspend )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
}


// usage tracking
STEAM_CALL_RESULT( StartPlaytimeTrackingResult_t )
SteamAPICall_t Steam_UGC::StartPlaytimeTracking( PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    StopPlaytimeTrackingResult_t data;
    data.m_eResult = k_EResultOK;
    return callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
}

STEAM_CALL_RESULT( StopPlaytimeTrackingResult_t )
SteamAPICall_t Steam_UGC::StopPlaytimeTracking( PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    StopPlaytimeTrackingResult_t data;
    data.m_eResult = k_EResultOK;
    return callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
}

STEAM_CALL_RESULT( StopPlaytimeTrackingResult_t )
SteamAPICall_t Steam_UGC::StopPlaytimeTrackingForAllItems()
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    StopPlaytimeTrackingResult_t data;
    data.m_eResult = k_EResultOK;
    return callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
}


// parent-child relationship or dependency management
STEAM_CALL_RESULT( AddUGCDependencyResult_t )
SteamAPICall_t Steam_UGC::AddDependency( PublishedFileId_t nParentPublishedFileID, PublishedFileId_t nChildPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nParentPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}

STEAM_CALL_RESULT( RemoveUGCDependencyResult_t )
SteamAPICall_t Steam_UGC::RemoveDependency( PublishedFileId_t nParentPublishedFileID, PublishedFileId_t nChildPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nParentPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}


// add/remove app dependence/requirements (usually DLC)
STEAM_CALL_RESULT( AddAppDependencyResult_t )
SteamAPICall_t Steam_UGC::AddAppDependency( PublishedFileId_t nPublishedFileID, AppId_t nAppID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}

STEAM_CALL_RESULT( RemoveAppDependencyResult_t )
SteamAPICall_t Steam_UGC::RemoveAppDependency( PublishedFileId_t nPublishedFileID, AppId_t nAppID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}

// request app dependencies. note that whatever callback you register for GetAppDependenciesResult_t may be called multiple times
// until all app dependencies have been returned
STEAM_CALL_RESULT( GetAppDependenciesResult_t )
SteamAPICall_t Steam_UGC::GetAppDependencies( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}


// delete the item without prompting the user
STEAM_CALL_RESULT( DeleteItemResult_t )
SteamAPICall_t Steam_UGC::DeleteItem( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}

// Show the app's latest Workshop EULA to the user in an overlay window, where they can accept it or not
bool Steam_UGC::ShowWorkshopEULA()
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}

// Retrieve information related to the user's acceptance or not of the app's specific Workshop EULA
STEAM_CALL_RESULT( WorkshopEULAStatus_t )
SteamAPICall_t Steam_UGC::GetWorkshopEULAStatus()
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return k_uAPICallInvalid;
}

// Return the user's community content descriptor preferences
uint32 Steam_UGC::GetUserContentDescriptorPreferences( EUGCContentDescriptorID *pvecDescriptors, uint32 cMaxEntries )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return 0;
}
