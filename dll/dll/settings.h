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

#ifndef SETTINGS_INCLUDE_H
#define SETTINGS_INCLUDE_H

#include "base.h"

struct IP_PORT;

struct DLC_entry {
    AppId_t appID{};
    std::string name{};
    bool available{};
};

struct Mod_entry {
    PublishedFileId_t id{};
    std::string title{};
    std::string path{};

    std::string previewURL{};
    EWorkshopFileType fileType{};
    std::string description{};
    uint64 steamIDOwner{};
    uint32 timeCreated{};
    uint32 timeUpdated{};
    uint32 timeAddedToUserList{};
    ERemoteStoragePublishedFileVisibility visibility{};
    bool banned = false;
    bool acceptedForUse{};
    bool tagsTruncated{};
    std::string tags{};

    // file/url information
    UGCHandle_t handleFile = generate_file_handle();
    UGCHandle_t handlePreviewFile = generate_file_handle();

    std::string primaryFileName{};
    int32 primaryFileSize{};
    std::string previewFileName{};
    int32 previewFileSize{};
    std::string workshopItemURL{};

    // voting information
    uint32 votesUp{};
    uint32 votesDown{};
    float score{};

    // collection details
    uint32 numChildren{}; // TODO
    
private:
    UGCHandle_t generate_file_handle() {
        static UGCHandle_t val = 0;

        ++val;
        if (val == 0 || val == k_UGCHandleInvalid) val = 1;
        
        return val;
    }

};

struct Leaderboard_config {
    enum ELeaderboardSortMethod sort_method{};
    enum ELeaderboardDisplayType display_type{};
};

struct Stat_config {
    GameServerStats_Messages::StatInfo::Stat_Type type{};
    union {
        float default_value_float;
        int32 default_value_int;
    };
};

struct Image_Data {
    uint32 width{};
    uint32 height{};
    std::string data{};
};

struct Controller_Settings {
    std::map<std::string, std::map<std::string, std::pair<std::set<std::string>, std::string>>> action_sets{};
    std::map<std::string, std::string> action_set_layer_parents{};
    std::map<std::string, std::map<std::string, std::pair<std::set<std::string>, std::string>>> action_set_layers{};
};

struct Group_Clans {
    CSteamID id{};
    std::string name{};
    std::string tag{};
};

struct Overlay_Appearance {
    enum NotificationPosition {
        top_left, top_center, top_right,
        bot_left, bot_center, bot_right,
    };

    constexpr const static NotificationPosition default_pos = NotificationPosition::top_right;

    std::string font_override{}; // path to a custom user-provided TTF font
    float font_size = 16.0f;
    
    float icon_size = 64.0f;

    float font_glyph_extra_spacing_x = 0.0f;
    float font_glyph_extra_spacing_y = 0.0f;

    float notification_r = 0.16f;
    float notification_g = 0.29f;
    float notification_b = 0.48f;
    float notification_a = 1.0f;

    float notification_rounding = 0.0f; // corners roundness for all notifications
    float notification_margin_x = 0.0f; // horizontal margin
    float notification_margin_y = 0.0f; // vertical margin
    
    uint32 notification_animation = 350; // sliding animation duration (millisec)
    uint32 notification_duration_progress = 6000; // achievement progress indication duration (millisec)
    uint32 notification_duration_achievement = 7000; // achievement unlocked duration (millisec)
    uint32 notification_duration_invitation = 8000; // friend invitation duration (millisec)
    uint32 notification_duration_chat = 4000; // sliding animation duration duration (millisec)

    std::string ach_unlock_datetime_format = "%H:%M - %d/%m/%Y";
    
    float background_r = -1.0f;
    float background_g = -1.0f;
    float background_b = -1.0f;
    float background_a = -1.0f;

    float element_r = -1.0f;
    float element_g = -1.0f;
    float element_b = -1.0f;
    float element_a = -1.0f;

    float element_hovered_r = -1.0f;
    float element_hovered_g = -1.0f;
    float element_hovered_b = -1.0f;
    float element_hovered_a = -1.0f;

    float element_active_r = -1.0f;
    float element_active_g = -1.0f;
    float element_active_b = -1.0f;
    float element_active_a = -1.0f;

    NotificationPosition ach_earned_pos = NotificationPosition::bot_right; // achievement earned
    NotificationPosition invite_pos = default_pos; // lobby/game invitation
    NotificationPosition chat_msg_pos = NotificationPosition::top_center; // chat message from a friend

    static NotificationPosition translate_notification_position(const std::string &str);
};

class Settings {
    CSteamID steam_id{}; // user id
    CGameID game_id{};
    std::string name{};
    std::string language{}; // default "english"
    CSteamID lobby_id = k_steamIDNil;

    bool offline = false;
    uint16 port{}; // Listen port, default 47584

    bool unlockAllDLCs = true;
    std::vector<struct DLC_entry> DLCs{};
    
    //installed app ids, Steam_Apps::BIsAppInstalled()
    bool assume_any_app_installed = true;
    std::set<AppId_t> installed_app_ids{};

    std::map<AppId_t, std::string> app_paths{};
    std::vector<struct Mod_entry> mods{};
    std::map<std::string, Leaderboard_config> leaderboards{};
    std::map<std::string, Stat_config> stats{};

    //supported languages
    std::set<std::string> supported_languages_set{};
    std::string supported_languages{};

public:

    //Depots
    std::vector<DepotId_t> depots{};

    //custom broadcasts
    std::set<IP_PORT> custom_broadcasts{};

    //networking
    bool disable_networking = false;

    //gameserver source query
    bool disable_source_query = false;

    //matchmaking server list & server details
    bool matchmaking_server_list_always_lan_type = true;
    bool matchmaking_server_details_via_source_query = false;

    //app build id
    int build_id = 10;

    //make lobby creation fail in the matchmaking interface
    bool disable_lobby_creation = false;

    //steamhttp external download support
    bool download_steamhttp_requests = false;
    bool force_steamhttp_success = false;

    //steam deck flag
    bool steam_deck = false;
    
    // use new app_ticket auth instead of old one
    bool enable_new_app_ticket = false;
    // can use GC token for generation
    bool use_gc_token = false;

    // allow stats not defined by the user?
    bool allow_unknown_stats = false;

    // when a stat that's tied to an achievement gets a new value, should the emu save that progress only if it's higher?
    // the stat itself is always saved regardless of that flag, only affects the achievement progress
    bool save_only_higher_stat_achievement_progress = true;

    // bypass to make SetAchievement() always return true, prevent some games from breaking
    bool achievement_bypass = false;

    bool disable_account_avatar = true;

    // setting this env var conflicts with Steam Input
    bool disable_steamoverlaygameid_env_var = false;

    // enable owning Steam Applications IDs (mostly builtin apps + dedicated servers)
    bool enable_builtin_preowned_ids = false;

    std::map<int, struct Image_Data> images{};

    //subscribed lobby/group ids
    std::set<uint64> subscribed_groups{};
    std::vector<Group_Clans> subscribed_groups_clans{};

    // get the alpha-2 code from: https://www.iban.com/country-codes 
    std::string ip_country = "US";

    //is playing on beta branch + current/forced branch name
    bool is_beta_branch = false;
    std::string current_branch_name = "public";

    //controller
    struct Controller_Settings controller_settings{};
    std::string glyphs_directory{};


    // allow Steam_User_Stats::FindLeaderboard() to always succeed and create the given unknown leaderboard
    bool disable_leaderboards_create_unknown = false;
    // share leaderboards with other players playing the same game on the same network
    bool share_leaderboards_over_network = false;

    // don't share stats and achievements with the game server
    bool disable_sharing_stats_with_gameserver = false;
    // synchronize user stats/achievements with game servers as soon as possible instead of caching them.
    bool immediate_gameserver_stats = false;

    //overlay
    bool disable_overlay = true;
    int overlay_hook_delay_sec = 0; // "Saints Row (2022)" needs a lot of time to initialize, otherwise detection will fail
    int overlay_renderer_detector_timeout_sec = 15; // "Saints Row (2022)" takes almost ~8 sec to detect renderer (DX12)
    bool disable_overlay_achievement_notification = false;
    bool disable_overlay_friend_notification = false;
    bool disable_overlay_achievement_progress = false;
    //warn people who use local save
    bool overlay_warn_local_save = false;
    //disable overlay warning for local save
    bool disable_overlay_warning_local_save = false;
    //disable overlay warning for bad app ID (= 0)
    bool disable_overlay_warning_bad_appid = false;
    // disable all overlay warnings
    bool disable_overlay_warning_any = false;
    Overlay_Appearance overlay_appearance{};
    // whether to auto accept any overlay invites
    bool auto_accept_any_overlay_invites = false;
    // list of user steam IDs to auto-accept invites from
    std::set<uint64_t> auto_accept_overlay_invites_friends{};


#ifdef LOBBY_CONNECT
    static constexpr const bool is_lobby_connect = true;
#else
    static constexpr const bool is_lobby_connect = false;
#endif

    Settings(CSteamID steam_id, CGameID game_id, const std::string &name, const std::string &language, bool offline);

    static std::string sanitize(const std::string &name);

    CSteamID get_local_steam_id();
    CGameID get_local_game_id();

    const char *get_local_name();
    void set_local_name(const char *name);

    const char *get_language();
    void set_language(const char *language);

    void set_supported_languages(const std::set<std::string> &langs);
    const std::set<std::string>& get_supported_languages_set() const;
    const std::string& get_supported_languages() const;

    void set_game_id(CGameID game_id);

    void set_lobby(CSteamID lobby_id);
    CSteamID get_lobby();

    bool is_offline();

    uint16 get_port();
    void set_port(uint16 port);

    //DLC stuff
    void unlockAllDLC(bool value);
    void addDLC(AppId_t appID, std::string name, bool available);
    unsigned int DLCCount() const;
    bool hasDLC(AppId_t appID);
    bool getDLC(unsigned int index, AppId_t &appID, bool &available, std::string &name);

    //installed apps, used by Steam_Apps::BIsAppInstalled()
    void assumeAnyAppInstalled(bool val);
    void addInstalledApp(AppId_t appID);
    bool isAppInstalled(AppId_t appID) const;

    //App Install paths
    void setAppInstallPath(AppId_t appID, const std::string &path);
    std::string getAppInstallPath(AppId_t appID);

    //mod stuff
    void addMod(PublishedFileId_t id, const std::string &title, const std::string &path);
    void addModDetails(PublishedFileId_t id, const Mod_entry &details);
    Mod_entry getMod(PublishedFileId_t id);
    bool isModInstalled(PublishedFileId_t id);
    std::set<PublishedFileId_t> modSet();

    //leaderboards
    void setLeaderboard(const std::string &leaderboard, enum ELeaderboardSortMethod sort_method, enum ELeaderboardDisplayType display_type);
    const std::map<std::string, Leaderboard_config>& getLeaderboards() const;

    //stats
    const std::map<std::string, Stat_config>& getStats() const;
    std::map<std::string, Stat_config>::const_iterator setStatDefiniton(const std::string &name, const struct Stat_config &stat_config);

    //images
    int add_image(const std::string &data, uint32 width, uint32 height);

    // overlay auto accept stuff
    void acceptAnyOverlayInvites(bool value);
    void addFriendToOverlayAutoAccept(uint64_t friend_id);
    bool hasOverlayAutoAcceptInviteFromFriend(uint64_t friend_id) const;
    size_t overlayAutoAcceptInvitesCount() const;

};

#endif // SETTINGS_INCLUDE_H
