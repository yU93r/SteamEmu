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

#include "dll/settings_parser.h"

#define SI_CONVERT_GENERIC
#define SI_SUPPORT_IOSTREAMS
#define SI_NO_MBCS
#include "simpleini/SimpleIni.h"


static CSimpleIniA ini{};


static void load_custom_broadcasts(std::string broadcasts_filepath, std::set<IP_PORT> &custom_broadcasts)
{
    PRINT_DEBUG("loading broadcasts file '%s'", broadcasts_filepath.c_str());
    std::ifstream broadcasts_file(utf8_decode(broadcasts_filepath));
    common_helpers::consume_bom(broadcasts_file);
    if (broadcasts_file.is_open()) {
        std::string line;
        while (std::getline(broadcasts_file, line)) {
            std::set<IP_PORT> ips = Networking::resolve_ip(line);
            custom_broadcasts.insert(ips.begin(), ips.end());
        }
    }
}

static void load_subscribed_groups_clans(std::string clans_filepath, Settings *settings_client, Settings *settings_server)
{
    PRINT_DEBUG("loading group clans file '%s'", clans_filepath.c_str());
    std::ifstream clans_file(utf8_decode(clans_filepath));
    if (clans_file.is_open()) {
        common_helpers::consume_bom(clans_file);
        std::string line;
        while (std::getline(clans_file, line)) {
            if (line.length() <= 0) continue;

            std::size_t seperator1 = line.find("\t\t");
            std::size_t seperator2 = line.rfind("\t\t");
            std::string clan_id;
            std::string clan_name;
            std::string clan_tag;
            if ((seperator1 != std::string::npos) && (seperator2 != std::string::npos)) {
                clan_id = line.substr(0, seperator1);
                clan_name = line.substr(seperator1+2, seperator2-2);
                clan_tag = line.substr(seperator2+2);

                // fix persistant tabbing problem for clan name
                std::size_t seperator3 = clan_name.find("\t");
                std::string clan_name_fix = clan_name.substr(0, seperator3);
                clan_name = clan_name_fix;
            }

            Group_Clans nclan;
            nclan.id = CSteamID( std::stoull(clan_id.c_str(), NULL, 0) );
            nclan.name = clan_name;
            nclan.tag = clan_tag;

            try {
                settings_client->subscribed_groups_clans.push_back(nclan);
                settings_server->subscribed_groups_clans.push_back(nclan);
                PRINT_DEBUG("Added clan %s", clan_name.c_str());
            } catch (...) {}
        }
    }
}

static void load_overlay_appearance(std::string appearance_filepath, class Settings *settings_client, class Settings *settings_server, class Local_Storage *local_storage)
{
    std::ifstream appearance_file(utf8_decode(appearance_filepath));
    if (appearance_file.is_open()) {
        PRINT_DEBUG("Parsing overlay appearance file '%s'", appearance_filepath.c_str());
        common_helpers::consume_bom(appearance_file);
        std::string line{};
        while (std::getline(appearance_file, line)) {
            if (line.length() <= 0) continue;

            std::size_t seperator = line.find(" ");
            std::string name{};
            std::string value{};
            if (seperator != std::string::npos) {
                name = line.substr(0, seperator);
                name = common_helpers::string_strip(name);

                value = line.substr(seperator + 1);
                value = common_helpers::string_strip(value);
            }

            // comments
            if (name.size() && (
                name[0] == '#' || name[0] == ';' || name.compare(0, 2, "//") == 0
            )) {
                continue;
            }

            PRINT_DEBUG("  Overlay appearance line '%s'='%s'", name.c_str(), value.c_str());
            try {
                if (name.compare("Font_Override") == 0) {
                    // first try the local settings folder
                    std::string nfont_override(common_helpers::to_absolute(value, Local_Storage::get_game_settings_path() + "fonts"));
                    if (common_helpers::file_exist(nfont_override)) {
                        settings_client->overlay_appearance.font_override = nfont_override;
                        settings_server->overlay_appearance.font_override = nfont_override;
                    }

                    // then try the global settings folder
                    if (settings_client->overlay_appearance.font_override.empty()) {
                        nfont_override = common_helpers::to_absolute(value, local_storage->get_global_settings_path() + "fonts");
                        if (common_helpers::file_exist(nfont_override)) {
                            settings_client->overlay_appearance.font_override = nfont_override;
                            settings_server->overlay_appearance.font_override = nfont_override;
                        }
                    }

                    if (settings_client->overlay_appearance.font_override.empty()) {
                        PRINT_DEBUG("  ERROR font file '%s' doesn't exist and will be ignored", value.c_str());
                    } else {
                        PRINT_DEBUG("  loaded font '%s'", nfont_override.c_str());
                    }
                } else if (name.compare("Font_Size") == 0) {
                    float nfont_size = std::stof(value, NULL);
                    settings_client->overlay_appearance.font_size = nfont_size;
                    settings_server->overlay_appearance.font_size = nfont_size;
                } else if (name.compare("Icon_Size") == 0) {
                    float nicon_size = std::stof(value, NULL);
                    settings_client->overlay_appearance.icon_size = nicon_size;
                    settings_server->overlay_appearance.icon_size = nicon_size;
                } else if (name.compare("Font_Glyph_Extra_Spacing_x") == 0) {
                    float size = std::stof(value, NULL);
                    settings_client->overlay_appearance.font_glyph_extra_spacing_x = size;
                    settings_server->overlay_appearance.font_glyph_extra_spacing_x = size;
                } else if (name.compare("Font_Glyph_Extra_Spacing_y") == 0) {
                    float size = std::stof(value, NULL);
                    settings_client->overlay_appearance.font_glyph_extra_spacing_y = size;
                    settings_server->overlay_appearance.font_glyph_extra_spacing_y = size;
                } else if (name.compare("Notification_R") == 0) {
                    float nnotification_r = std::stof(value, NULL);
                    settings_client->overlay_appearance.notification_r = nnotification_r;
                    settings_server->overlay_appearance.notification_r = nnotification_r;
                } else if (name.compare("Notification_G") == 0) {
                    float nnotification_g = std::stof(value, NULL);
                    settings_client->overlay_appearance.notification_g = nnotification_g;
                    settings_server->overlay_appearance.notification_g = nnotification_g;
                } else if (name.compare("Notification_B") == 0) {
                    float nnotification_b = std::stof(value, NULL);
                    settings_client->overlay_appearance.notification_b = nnotification_b;
                    settings_server->overlay_appearance.notification_b = nnotification_b;
                } else if (name.compare("Notification_A") == 0) {
                    float nnotification_a = std::stof(value, NULL);
                    settings_client->overlay_appearance.notification_a = nnotification_a;
                    settings_server->overlay_appearance.notification_a = nnotification_a;
                } else if (name.compare("Background_R") == 0) {
                    float nbackground_r = std::stof(value, NULL);
                    settings_client->overlay_appearance.background_r = nbackground_r;
                    settings_server->overlay_appearance.background_r = nbackground_r;
                } else if (name.compare("Background_G") == 0) {
                    float nbackground_g = std::stof(value, NULL);
                    settings_client->overlay_appearance.background_g = nbackground_g;
                    settings_server->overlay_appearance.background_g = nbackground_g;
                } else if (name.compare("Background_B") == 0) {
                    float nbackground_b = std::stof(value, NULL);
                    settings_client->overlay_appearance.background_b = nbackground_b;
                    settings_server->overlay_appearance.background_b = nbackground_b;
                } else if (name.compare("Background_A") == 0) {
                    float nbackground_a = std::stof(value, NULL);
                    settings_client->overlay_appearance.background_a = nbackground_a;
                    settings_server->overlay_appearance.background_a = nbackground_a;
                } else if (name.compare("Element_R") == 0) {
                    float nelement_r = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_r = nelement_r;
                    settings_server->overlay_appearance.element_r = nelement_r;
                } else if (name.compare("Element_G") == 0) {
                    float nelement_g = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_g = nelement_g;
                    settings_server->overlay_appearance.element_g = nelement_g;
                } else if (name.compare("Element_B") == 0) {
                    float nelement_b = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_b = nelement_b;
                    settings_server->overlay_appearance.element_b = nelement_b;
                } else if (name.compare("Element_A") == 0) {
                    float nelement_a = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_a = nelement_a;
                    settings_server->overlay_appearance.element_a = nelement_a;
                } else if (name.compare("ElementHovered_R") == 0) {
                    float nelement_hovered_r = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_hovered_r = nelement_hovered_r;
                    settings_server->overlay_appearance.element_hovered_r = nelement_hovered_r;
                } else if (name.compare("ElementHovered_G") == 0) {
                    float nelement_hovered_g = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_hovered_g = nelement_hovered_g;
                    settings_server->overlay_appearance.element_hovered_g = nelement_hovered_g;
                } else if (name.compare("ElementHovered_B") == 0) {
                    float nelement_hovered_b = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_hovered_b = nelement_hovered_b;
                    settings_server->overlay_appearance.element_hovered_b = nelement_hovered_b;
                } else if (name.compare("ElementHovered_A") == 0) {
                    float nelement_hovered_a = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_hovered_a = nelement_hovered_a;
                    settings_server->overlay_appearance.element_hovered_a = nelement_hovered_a;
                } else if (name.compare("ElementActive_R") == 0) {
                    float nelement_active_r = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_active_r = nelement_active_r;
                    settings_server->overlay_appearance.element_active_r = nelement_active_r;
                } else if (name.compare("ElementActive_G") == 0) {
                    float nelement_active_g = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_active_g = nelement_active_g;
                    settings_server->overlay_appearance.element_active_g = nelement_active_g;
                } else if (name.compare("ElementActive_B") == 0) {
                    float nelement_active_b = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_active_b = nelement_active_b;
                    settings_server->overlay_appearance.element_active_b = nelement_active_b;
                } else if (name.compare("ElementActive_A") == 0) {
                    float nelement_active_a = std::stof(value, NULL);
                    settings_client->overlay_appearance.element_active_a = nelement_active_a;
                    settings_server->overlay_appearance.element_active_a = nelement_active_a;
                } else if (name.compare("PosAchievement") == 0) {
                    auto pos = Overlay_Appearance::translate_notification_position(value);
                    settings_client->overlay_appearance.ach_earned_pos = pos;
                    settings_server->overlay_appearance.ach_earned_pos = pos;
                } else if (name.compare("PosInvitation") == 0) {
                    auto pos = Overlay_Appearance::translate_notification_position(value);
                    settings_client->overlay_appearance.invite_pos = pos;
                    settings_server->overlay_appearance.invite_pos = pos;
                } else if (name.compare("PosChatMsg") == 0) {
                    auto pos = Overlay_Appearance::translate_notification_position(value);
                    settings_client->overlay_appearance.chat_msg_pos = pos;
                    settings_server->overlay_appearance.chat_msg_pos = pos;
                }

            } catch (...) { }
        }
    }
}

template<typename Out>
static void split_string(const std::string &s, char delim, Out result) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}

static void load_gamecontroller_settings(Settings *settings)
{
    std::string path = Local_Storage::get_game_settings_path() + "controller";
    std::vector<std::string> paths = Local_Storage::get_filenames_path(path);

    for (auto & p: paths) {
        size_t length = p.length();
        if (length < 4) continue;
        if ( std::toupper(p.back()) != 'T') continue;
        if ( std::toupper(p[length - 2]) != 'X') continue;
        if ( std::toupper(p[length - 3]) != 'T') continue;
        if (p[length - 4] != '.') continue;

        PRINT_DEBUG("controller config %s", p.c_str());
        std::string action_set_name = p.substr(0, length - 4);
        std::transform(action_set_name.begin(), action_set_name.end(), action_set_name.begin(),[](unsigned char c){ return std::toupper(c); });

        std::string controller_config_path = path + PATH_SEPARATOR + p;
        std::ifstream input( utf8_decode(controller_config_path) );
        if (input.is_open()) {
            common_helpers::consume_bom(input);
            std::map<std::string, std::pair<std::set<std::string>, std::string>> button_pairs;

            for( std::string line; getline( input, line ); ) {
                if (!line.empty() && line[line.length()-1] == '\n') {
                    line.pop_back();
                }

                if (!line.empty() && line[line.length()-1] == '\r') {
                    line.pop_back();
                }

                std::string action_name;
                std::string button_name;
                std::string source_mode;

                std::size_t deliminator = line.find("=");
                if (deliminator != 0 && deliminator != std::string::npos && deliminator != line.size()) {
                    action_name = line.substr(0, deliminator);
                    std::size_t deliminator2 = line.find("=", deliminator + 1);

                    if (deliminator2 != std::string::npos && deliminator2 != line.size()) {
                        button_name = line.substr(deliminator + 1, deliminator2 - (deliminator + 1));
                        source_mode = line.substr(deliminator2 + 1);
                    } else {
                        button_name = line.substr(deliminator + 1);
                        source_mode = "";
                    }
                }

                std::transform(action_name.begin(), action_name.end(), action_name.begin(),[](unsigned char c){ return std::toupper(c); });
                std::transform(button_name.begin(), button_name.end(), button_name.begin(),[](unsigned char c){ return std::toupper(c); });
                std::pair<std::set<std::string>, std::string> button_config = {{}, source_mode};
                split_string(button_name, ',', std::inserter(button_config.first, button_config.first.begin()));
                button_pairs[action_name] = button_config;
                PRINT_DEBUG("Added %s %s %s", action_name.c_str(), button_name.c_str(), source_mode.c_str());
            }

            settings->controller_settings.action_sets[action_set_name] = button_pairs;
            PRINT_DEBUG("Added %zu action names to %s", button_pairs.size(), action_set_name.c_str());
        }
    }

    settings->glyphs_directory = path + (PATH_SEPARATOR "glyphs" PATH_SEPARATOR);
}

// steam_appid.txt
static uint32 parse_steam_app_id(const std::string &program_path)
{
    uint32 appid = 0;

    // try steam_settings folder
    char array[10] = {};
    array[0] = '0';
    Local_Storage::get_file_data(Local_Storage::get_game_settings_path() + "steam_appid.txt", array, sizeof(array) - 1);
    try {
        appid = std::stoul(array);
    } catch (...) {}

    // try current dir
    if (!appid) {
        memset(array, 0, sizeof(array));
        array[0] = '0';
        Local_Storage::get_file_data("steam_appid.txt", array, sizeof(array) - 1);
        try {
            appid = std::stoul(array);
        } catch (...) {}
    }

    // try exe dir
    if (!appid) {
        memset(array, 0, sizeof(array));
        array[0] = '0';
        Local_Storage::get_file_data(program_path + "steam_appid.txt", array, sizeof(array) - 1);
        try {
            appid = std::stoul(array);
        } catch (...) {}
    }

    // try env vars
    if (!appid) {
        std::string str_appid = get_env_variable("SteamAppId");
        std::string str_gameid = get_env_variable("SteamGameId");
        std::string str_overlay_gameid = get_env_variable("SteamOverlayGameId");
        
        PRINT_DEBUG("str_appid %s str_gameid: %s str_overlay_gameid: %s", str_appid.c_str(), str_gameid.c_str(), str_overlay_gameid.c_str());
        uint32 appid_env = 0;
        uint32 gameid_env = 0;
        uint32 overlay_gameid = 0;

        if (str_appid.size() > 0) {
            try {
                appid_env = std::stoul(str_appid);
            } catch (...) {
                appid_env = 0;
            }
        }

        if (str_gameid.size() > 0) {
            try {
                gameid_env = std::stoul(str_gameid);
            } catch (...) {
                gameid_env = 0;
            }
        }

        if (str_overlay_gameid.size() > 0) {
            try {
                overlay_gameid = std::stoul(str_overlay_gameid);
            } catch (...) {
                overlay_gameid = 0;
            }
        }

        PRINT_DEBUG("appid_env %u gameid_env: %u overlay_gameid: %u", appid_env, gameid_env, overlay_gameid);
        if (appid_env) {
            appid = appid_env;
        }

        if (gameid_env) {
            appid = gameid_env;
        }

        if (overlay_gameid) {
            appid = overlay_gameid;
        }
    }

    PRINT_DEBUG("final appid = %u", appid);
    return appid;
}

// local_save.txt
static bool parse_local_save(const std::string &base_path, std::string &save_path)
{
    char array[256]{};
    int read = Local_Storage::get_file_data(base_path + "local_save.txt", array, sizeof(array) - 1);
    if (read != -1) {
        save_path = Local_Storage::get_program_path() + Settings::sanitize(array);
        PRINT_DEBUG("using local save path '%s'", save_path.c_str());
        return true;
    }
    return false;
}

// listen_port.txt
static uint16 parse_listen_port(class Local_Storage *local_storage)
{
    char array_port[10] = {};
    array_port[0] = '0';
    local_storage->get_data_settings("listen_port.txt", array_port, sizeof(array_port) - 1);
    uint16 port = (uint16)std::stoi(array_port);
    if (port == 0) {
        port = DEFAULT_PORT;
        snprintf(array_port, sizeof(array_port), "%hu", port);
        local_storage->store_data_settings("listen_port.txt", array_port, strlen(array_port));
    }
    return port;
}

// account_name.txt
static std::string parse_account_name(class Local_Storage *local_storage)
{
    char name[100] = {};
    if (local_storage->get_data_settings("account_name.txt", name, sizeof(name) - 1) <= 0) {
        strcpy(name, DEFAULT_NAME);
        local_storage->store_data_settings("account_name.txt", name, strlen(name));
    }
    return std::string(name);
}

// user_steam_id.txt
static CSteamID parse_user_steam_id(class Local_Storage *local_storage)
{
    char array_steam_id[32] = {};
    CSteamID user_id;
    uint64 steam_id = 0;
    bool generate_new = false;
    //try to load steam id from game specific settings folder first
    if (local_storage->get_data(Local_Storage::settings_storage_folder, "user_steam_id.txt", array_steam_id, sizeof(array_steam_id) - 1) > 0) {
        user_id = CSteamID((uint64)std::atoll(array_steam_id));
        if (!user_id.IsValid()) {
            generate_new = true;
        }
    } else {
        generate_new = true;
    }

    if (generate_new) {
        generate_new = false;
        if (local_storage->get_data_settings("user_steam_id.txt", array_steam_id, sizeof(array_steam_id) - 1) > 0) {
            user_id = CSteamID((uint64)std::atoll(array_steam_id));
            if (!user_id.IsValid()) {
                generate_new = true;
            }
        } else {
            generate_new = true;
        }
    }

    if (generate_new) {
        user_id = generate_steam_id_user();
        uint64 steam_id = user_id.ConvertToUint64();
        char temp_text[32] = {};
        snprintf(temp_text, sizeof(temp_text), "%llu", steam_id);
        local_storage->store_data_settings("user_steam_id.txt", temp_text, strlen(temp_text));
    }

    return user_id;
}

// language.txt
// valid list: https://partner.steamgames.com/doc/store/localization/languages
static std::string parse_current_language(class Local_Storage *local_storage)
{
    char language[64] = {};
    if (local_storage->get_data_settings("language.txt", language, sizeof(language) - 1) <= 0) {
        strcpy(language, DEFAULT_LANGUAGE);
        local_storage->store_data_settings("language.txt", language, strlen(language));
    }

    return common_helpers::to_lower(std::string(language));
}

// supported_languages.txt
// valid list: https://partner.steamgames.com/doc/store/localization/languages
static std::set<std::string> parse_supported_languages(class Local_Storage *local_storage, std::string &language)
{
    std::set<std::string> supported_languages{};

    std::string lang_config_path = Local_Storage::get_game_settings_path() + "supported_languages.txt";
    std::ifstream input( utf8_decode(lang_config_path) );

    std::string first_language{};
    if (input.is_open()) {
        common_helpers::consume_bom(input);
        for( std::string line; getline( input, line ); ) {
            if (!line.empty() && line[line.length()-1] == '\n') {
                line.pop_back();
            }

            if (!line.empty() && line[line.length()-1] == '\r') {
                line.pop_back();
            }

            try {
                std::string lang = common_helpers::to_lower(line);
                if (!first_language.size()) first_language = lang;
                supported_languages.insert(lang);
                PRINT_DEBUG("Added supported_language %s", lang.c_str());
            } catch (...) {}
        }
    }

    // if the current emu language is not in the supported languages list
    if (!supported_languages.count(language)) {
        if (first_language.size()) { // get the first supported language if the list wasn't empty
            language = first_language;
        } else { // otherwise just lie and add it then!
            supported_languages.insert(language);
            PRINT_DEBUG("Forced current language '%s' into supported_languages", language.c_str());
        }
    }

    return supported_languages;
}

// DLC.txt
static void parse_dlc(class Settings *settings_client, class Settings *settings_server)
{
    std::string dlc_config_path = Local_Storage::get_game_settings_path() + "DLC.txt";
    std::ifstream input( utf8_decode(dlc_config_path) );
    if (input.is_open()) {
        common_helpers::consume_bom(input);
        settings_client->unlockAllDLC(false);
        settings_server->unlockAllDLC(false);
        PRINT_DEBUG("Locking all DLC");

        for( std::string line; std::getline( input, line ); ) {
            if (!line.empty() && line.front() == '#') {
                continue;
            }
            if (!line.empty() && line.back() == '\n') {
                line.pop_back();
            }

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            std::size_t deliminator = line.find("=");
            if (deliminator != 0 && deliminator != std::string::npos && deliminator != line.size()) {
                AppId_t appid = stol(line.substr(0, deliminator));
                std::string name = line.substr(deliminator + 1);
                bool available = true;

                if (appid) {
                    PRINT_DEBUG("Adding DLC: %u|%s| %u", appid, name.c_str(), available);
                    settings_client->addDLC(appid, name, available);
                    settings_server->addDLC(appid, name, available);
                }
            }
        }
    } else {
        //unlock all DLC
        PRINT_DEBUG("Unlocking all DLC");
        settings_client->unlockAllDLC(true);
        settings_server->unlockAllDLC(true);
    }
}

// app_paths.txt
static void parse_app_paths(class Settings *settings_client, Settings *settings_server, const std::string &program_path)
{
    std::string dlc_config_path = Local_Storage::get_game_settings_path() + "app_paths.txt";
    std::ifstream input( utf8_decode(dlc_config_path) );

    if (input.is_open()) {
        common_helpers::consume_bom(input);
        for( std::string line; getline( input, line ); ) {
            if (!line.empty() && line[line.length()-1] == '\n') {
                line.pop_back();
            }

            if (!line.empty() && line[line.length()-1] == '\r') {
                line.pop_back();
            }

            std::size_t deliminator = line.find("=");
            if (deliminator != 0 && deliminator != std::string::npos && deliminator != line.size()) {
                AppId_t appid = stol(line.substr(0, deliminator));
                std::string rel_path = line.substr(deliminator + 1);
                std::string path = canonical_path(program_path + rel_path);

                if (appid) {
                    if (path.size()) {
                        PRINT_DEBUG("Adding app path: %u|%s|", appid, path.c_str());
                        settings_client->setAppInstallPath(appid, path);
                        settings_server->setAppInstallPath(appid, path);
                    } else {
                        PRINT_DEBUG("Error adding app path for: %u does this path exist? |%s|", appid, rel_path.c_str());
                    }
                }
            }
        }
    }

}

// leaderboards.txt
static void parse_leaderboards(class Settings *settings_client, class Settings *settings_server)
{
    std::string dlc_config_path = Local_Storage::get_game_settings_path() + "leaderboards.txt";
    std::ifstream input( utf8_decode(dlc_config_path) );
    if (input.is_open()) {
        common_helpers::consume_bom(input);

        for( std::string line; getline( input, line ); ) {
            if (!line.empty() && line[line.length()-1] == '\n') {
                line.pop_back();
            }

            if (!line.empty() && line[line.length()-1] == '\r') {
                line.pop_back();
            }

            std::string leaderboard{};
            unsigned int sort_method = 0;
            unsigned int display_type = 0;

            std::size_t deliminator = line.find("=");
            if (deliminator != 0 && deliminator != std::string::npos && deliminator != line.size()) {
                leaderboard = line.substr(0, deliminator);
                std::size_t deliminator2 = line.find("=", deliminator + 1);
                if (deliminator2 != std::string::npos && deliminator2 != line.size()) {
                    sort_method = stol(line.substr(deliminator + 1, deliminator2 - (deliminator + 1)));
                    display_type = stol(line.substr(deliminator2 + 1));
                }
            }

            if (leaderboard.size() && sort_method <= k_ELeaderboardSortMethodDescending && display_type <= k_ELeaderboardDisplayTypeTimeMilliSeconds) {
                PRINT_DEBUG("Adding leaderboard: %s|%u|%u", leaderboard.c_str(), sort_method, display_type);
                settings_client->setLeaderboard(leaderboard, (ELeaderboardSortMethod)sort_method, (ELeaderboardDisplayType)display_type);
                settings_server->setLeaderboard(leaderboard, (ELeaderboardSortMethod)sort_method, (ELeaderboardDisplayType)display_type);
            } else {
                PRINT_DEBUG("Error adding leaderboard for: '%s', are sort method %u or display type %u valid?", leaderboard.c_str(), sort_method, display_type);
            }
        }
    }

}

// stats.txt
static void parse_stats(class Settings *settings_client, class Settings *settings_server)
{
    std::string stats_config_path = Local_Storage::get_game_settings_path() + "stats.txt";
    std::ifstream input( utf8_decode(stats_config_path) );
    if (input.is_open()) {
        common_helpers::consume_bom(input);
        for( std::string line; getline( input, line ); ) {
            if (!line.empty() && line[line.length()-1] == '\n') {
                line.pop_back();
            }

            if (!line.empty() && line[line.length()-1] == '\r') {
                line.pop_back();
            }

            std::string stat_name;
            std::string stat_type;
            std::string stat_default_value;

            std::size_t deliminator = line.find("=");
            if (deliminator != 0 && deliminator != std::string::npos && deliminator != line.size()) {
                stat_name = line.substr(0, deliminator);
                std::size_t deliminator2 = line.find("=", deliminator + 1);

                if (deliminator2 != std::string::npos && deliminator2 != line.size()) {
                    stat_type = line.substr(deliminator + 1, deliminator2 - (deliminator + 1));
                    stat_default_value = line.substr(deliminator2 + 1);
                } else {
                    stat_type = line.substr(deliminator + 1);
                    stat_default_value = "0";
                }
            }

            std::transform(stat_type.begin(), stat_type.end(), stat_type.begin(),[](unsigned char c){ return std::tolower(c); });
            struct Stat_config config = {};

            try {
                if (stat_type == "float") {
                    config.type = GameServerStats_Messages::StatInfo::STAT_TYPE_FLOAT;
                    config.default_value_float = std::stof(stat_default_value);
                } else if (stat_type == "int") {
                    config.type = GameServerStats_Messages::StatInfo::STAT_TYPE_INT;
                    config.default_value_int = std::stol(stat_default_value);
                } else if (stat_type == "avgrate") {
                    config.type = GameServerStats_Messages::StatInfo::STAT_TYPE_AVGRATE;
                    config.default_value_float = std::stof(stat_default_value);
                } else {
                    PRINT_DEBUG("Error adding stat %s, type %s isn't valid", stat_name.c_str(), stat_type.c_str());
                    continue;
                }
            } catch (...) {
                PRINT_DEBUG("Error adding stat %s, default value %s isn't valid", stat_name.c_str(), stat_default_value.c_str());
                continue;
            }

            if (stat_name.size()) {
                PRINT_DEBUG("Adding stat type: %s|%u|%f|%i", stat_name.c_str(), config.type, config.default_value_float, config.default_value_int);
                settings_client->setStatDefiniton(stat_name, config);
                settings_server->setStatDefiniton(stat_name, config);
            } else {
                PRINT_DEBUG("Error adding stat for: %s, empty name", stat_name.c_str());
            }
        }
    }

}

// depots.txt
static void parse_depots(class Settings *settings_client, class Settings *settings_server)
{
    std::string depots_config_path = Local_Storage::get_game_settings_path() + "depots.txt";
    std::ifstream input( utf8_decode(depots_config_path) );
    if (input.is_open()) {
        common_helpers::consume_bom(input);
        for( std::string line; getline( input, line ); ) {
            if (!line.empty() && line[line.length()-1] == '\n') {
                line.pop_back();
            }

            if (!line.empty() && line[line.length()-1] == '\r') {
                line.pop_back();
            }

            try {
                DepotId_t depot_id = std::stoul(line);
                settings_client->depots.push_back(depot_id);
                settings_server->depots.push_back(depot_id);
                PRINT_DEBUG("Added depot %u", depot_id);
            } catch (...) {}
        }
    }

}

// subscribed_groups.txt
static void parse_subscribed_groups(class Settings *settings_client, class Settings *settings_server)
{
    std::string depots_config_path = Local_Storage::get_game_settings_path() + "subscribed_groups.txt";
    std::ifstream input( utf8_decode(depots_config_path) );
    if (input.is_open()) {
        common_helpers::consume_bom(input);
        for( std::string line; getline( input, line ); ) {
            if (!line.empty() && line[line.length()-1] == '\n') {
                line.pop_back();
            }

            if (!line.empty() && line[line.length()-1] == '\r') {
                line.pop_back();
            }

            try {
                uint64 source_id = std::stoull(line);
                settings_client->subscribed_groups.insert(source_id);
                settings_server->subscribed_groups.insert(source_id);
                PRINT_DEBUG("Added source %llu", source_id);
            } catch (...) {}
        }
    }

}

// installed_app_ids.txt
static void parse_installed_app_Ids(class Settings *settings_client, class Settings *settings_server)
{
    std::string installed_apps_list_path = Local_Storage::get_game_settings_path() + "installed_app_ids.txt";
    std::ifstream input( utf8_decode(installed_apps_list_path) );
    if (input.is_open()) {
        settings_client->assume_any_app_installed = false;
        settings_server->assume_any_app_installed = false;
        PRINT_DEBUG("Limiting/Locking installed apps");
        common_helpers::consume_bom(input);
        for( std::string line; getline( input, line ); ) {
            if (!line.empty() && line[line.length()-1] == '\n') {
                line.pop_back();
            }

            if (!line.empty() && line[line.length()-1] == '\r') {
                line.pop_back();
            }

            try {
                AppId_t app_id = std::stoul(line);
                settings_client->installed_app_ids.insert(app_id);
                settings_server->installed_app_ids.insert(app_id);
                PRINT_DEBUG("Added installed app with ID %u", app_id);
            } catch (...) {}
        }
    } else {
        settings_client->assume_any_app_installed = true;
        settings_server->assume_any_app_installed = true;
        PRINT_DEBUG("Assuming any app is installed");
    }

}

// force_branch_name.txt
static void parse_force_branch_name(class Settings *settings_client, class Settings *settings_server)
{
    std::string installed_apps_list_path = Local_Storage::get_game_settings_path() + "force_branch_name.txt";
    std::ifstream input( utf8_decode(installed_apps_list_path) );
    if (input.is_open()) {
        common_helpers::consume_bom(input);
        std::string line;
        getline( input, line );
        line = common_helpers::string_strip(line);
        if (!line.empty()) {
            settings_client->current_branch_name = line;
            settings_server->current_branch_name = line;
            PRINT_DEBUG("Forcing current branch name to '%s'", line.c_str());
        }
    }
}

// steam_settings/mods
static const auto one_week_ago_epoch = std::chrono::duration_cast<std::chrono::seconds>(
    ( startup_time - std::chrono::hours(24 * 7) ).time_since_epoch()
).count();

static size_t get_file_size_safe(const std::string &filepath, const std::string &basepath, int32 default_val = 0)
{
    try
    {
        const auto file_p = common_helpers::to_absolute(filepath, basepath);
        if (file_p.empty()) return default_val;

        size_t size = 0;
        if (common_helpers::file_size(file_p, size)) {
            return size;
        }
    } catch(...) {}
    return default_val;
}

static std::string get_mod_preview_url(const std::string &previewFileName, const std::string &mod_id)
{
    if (previewFileName.empty()) {
        return std::string();
    } else {
        auto settings_folder = std::string(Local_Storage::get_game_settings_path());
        std::replace(settings_folder.begin(), settings_folder.end(), '\\', '/');
        return "file://" + settings_folder + "mod_images/" + mod_id + "/" + previewFileName;
    }
    
}

static void try_parse_mods_file(class Settings *settings_client, Settings *settings_server, nlohmann::json &mod_items, const std::string &mods_folder)
{
    for (auto mod = mod_items.begin(); mod != mod_items.end(); ++mod) {
        try {
            std::string mod_images_fullpath = Local_Storage::get_game_settings_path() + "mod_images" + PATH_SEPARATOR + std::string(mod.key());
            Mod_entry newMod;
            newMod.id = std::stoull(mod.key());
            newMod.title = mod.value().value("title", std::string(mod.key()));

            // make sure this is never empty
            newMod.path = mod.value().value("path", std::string(""));
            if (newMod.path.empty()) {
                newMod.path = mods_folder + PATH_SEPARATOR + std::string(mod.key());
            } else {
                // make sure the path is normalized for current OS, and absolute
                newMod.path = common_helpers::to_absolute(
                    newMod.path,
                    get_full_program_path()
                );
            }

            newMod.fileType = k_EWorkshopFileTypeCommunity;
            newMod.description = mod.value().value("description", std::string(""));
            newMod.steamIDOwner = mod.value().value("steam_id_owner", settings_client->get_local_steam_id().ConvertToUint64());
            newMod.timeCreated = mod.value().value("time_created", (uint32)one_week_ago_epoch);
            newMod.timeUpdated = mod.value().value("time_updated", (uint32)one_week_ago_epoch);
            newMod.timeAddedToUserList = mod.value().value("time_added", (uint32)one_week_ago_epoch);
            newMod.visibility = k_ERemoteStoragePublishedFileVisibilityPublic;
            newMod.banned = false;
            newMod.acceptedForUse = true;
            newMod.tagsTruncated = false;
            newMod.tags = mod.value().value("tags", std::string(""));

            newMod.primaryFileName = mod.value().value("primary_filename", std::string(""));
            int32 primary_filesize = 0;
            if (!newMod.primaryFileName.empty()) {
                primary_filesize = (int32)get_file_size_safe(newMod.primaryFileName, newMod.path, primary_filesize);
            }
            newMod.primaryFileSize = mod.value().value("primary_filesize", primary_filesize);
            
            newMod.previewFileName = mod.value().value("preview_filename", std::string(""));
            int32 preview_filesize = 0;
            if (!newMod.previewFileName.empty()) {
                preview_filesize = (int32)get_file_size_safe(newMod.previewFileName, mod_images_fullpath, preview_filesize);
            }
            newMod.previewFileSize = mod.value().value("preview_filesize", preview_filesize);

            newMod.workshopItemURL = mod.value().value("workshop_item_url", "https://steamcommunity.com/sharedfiles/filedetails/?id=" + std::string(mod.key()));
            newMod.votesUp = mod.value().value("upvotes", (uint32)500);
            newMod.votesDown = mod.value().value("downvotes", (uint32)12);

            float score = 0.97f;
            try
            {
                score = newMod.votesUp / (float)(newMod.votesUp + newMod.votesDown);
            } catch(...) {}
            newMod.score = mod.value().value("score", score);
            
            newMod.numChildren = mod.value().value("num_children", (uint32)0);
            newMod.previewURL = mod.value().value("preview_url", get_mod_preview_url(newMod.previewFileName, std::string(mod.key())));
            
            settings_client->addMod(newMod.id, newMod.title, newMod.path);
            settings_server->addMod(newMod.id, newMod.title, newMod.path);
            settings_client->addModDetails(newMod.id, newMod);
            settings_server->addModDetails(newMod.id, newMod);
            
            PRINT_DEBUG("  parsed mod '%s':", std::string(mod.key()).c_str());
            PRINT_DEBUG("    path (will be used for primary file): '%s'", newMod.path.c_str());
            PRINT_DEBUG("    images path (will be used for preview file): '%s'", mod_images_fullpath.c_str());
            PRINT_DEBUG("    primary_filename: '%s'", newMod.primaryFileName.c_str());
            PRINT_DEBUG("    primary_filesize: %i bytes", newMod.primaryFileSize);
            PRINT_DEBUG("    primary file handle: %llu", settings_client->getMod(newMod.id).handleFile);
            PRINT_DEBUG("    preview_filename: '%s'", newMod.previewFileName.c_str());
            PRINT_DEBUG("    preview_filesize: %i bytes", newMod.previewFileSize);
            PRINT_DEBUG("    preview file handle: %llu", settings_client->getMod(newMod.id).handlePreviewFile);
            PRINT_DEBUG("    workshop_item_url: '%s'", newMod.workshopItemURL.c_str());
            PRINT_DEBUG("    preview_url: '%s'", newMod.previewURL.c_str());
        } catch (std::exception& e) {
            PRINT_DEBUG("MODLOADER ERROR: %s", e.what());
        }
    }
}

// called if mods.json doesn't exist or invalid
static void try_detect_mods_folder(class Settings *settings_client, Settings *settings_server, const std::string &mods_folder)
{
    std::vector<std::string> all_mods = Local_Storage::get_folders_path(mods_folder);
    for (auto & mod_folder: all_mods) {
        std::string mod_images_fullpath = Local_Storage::get_game_settings_path() + "mod_images" + PATH_SEPARATOR + mod_folder;
        try {
            Mod_entry newMod;
            newMod.id = std::stoull(mod_folder);
            newMod.title = mod_folder;

            // make sure this is never empty
            newMod.path = mods_folder + PATH_SEPARATOR + mod_folder;

            newMod.fileType = k_EWorkshopFileTypeCommunity;
            newMod.description = "mod #" + mod_folder;
            newMod.steamIDOwner = settings_client->get_local_steam_id().ConvertToUint64();
            newMod.timeCreated = (uint32)one_week_ago_epoch;
            newMod.timeUpdated = (uint32)one_week_ago_epoch;
            newMod.timeAddedToUserList = (uint32)one_week_ago_epoch;
            newMod.visibility = k_ERemoteStoragePublishedFileVisibilityPublic;
            newMod.banned = false;
            newMod.acceptedForUse = true;
            newMod.tagsTruncated = false;
            newMod.tags = "";

            std::vector<std::string> mod_primary_files = Local_Storage::get_filenames_path(newMod.path);
            newMod.primaryFileName = mod_primary_files.size() ? mod_primary_files[0] : "";
            newMod.primaryFileSize = (int32)get_file_size_safe(newMod.primaryFileName, newMod.path);

            std::vector<std::string> mod_preview_files = Local_Storage::get_filenames_path(mod_images_fullpath);
            newMod.previewFileName = mod_preview_files.size() ? mod_preview_files[0] : "";
            newMod.previewFileSize = (int32)get_file_size_safe(newMod.previewFileName, mod_images_fullpath);

            newMod.workshopItemURL =  "https://steamcommunity.com/sharedfiles/filedetails/?id=" + mod_folder;
            newMod.votesUp = (uint32)500;
            newMod.votesDown = (uint32)12;
            newMod.score = 0.97f;
            newMod.numChildren = (uint32)0;
            newMod.previewURL = get_mod_preview_url(newMod.previewFileName, mod_folder);
            
            settings_client->addMod(newMod.id, newMod.title, newMod.path);
            settings_server->addMod(newMod.id, newMod.title, newMod.path);
            settings_client->addModDetails(newMod.id, newMod);
            settings_server->addModDetails(newMod.id, newMod);
            PRINT_DEBUG("  detected mod '%s':", mod_folder.c_str());
            PRINT_DEBUG("    path (will be used for primary file): '%s'", newMod.path.c_str());
            PRINT_DEBUG("    images path (will be used for preview file): '%s'", mod_images_fullpath.c_str());
            PRINT_DEBUG("    primary_filename: '%s'", newMod.primaryFileName.c_str());
            PRINT_DEBUG("    primary_filesize: %i bytes", newMod.primaryFileSize);
            PRINT_DEBUG("    primary file handle: %llu", settings_client->getMod(newMod.id).handleFile);
            PRINT_DEBUG("    preview_filename: '%s'", newMod.previewFileName.c_str());
            PRINT_DEBUG("    preview_filesize: %i bytes", newMod.previewFileSize);
            PRINT_DEBUG("    preview file handle: %llu", settings_client->getMod(newMod.id).handlePreviewFile);
            PRINT_DEBUG("    workshop_item_url: '%s'", newMod.workshopItemURL.c_str());
            PRINT_DEBUG("    preview_url: '%s'", newMod.previewURL.c_str());
        } catch (...) {}
    }
}

static void parse_mods_folder(class Settings *settings_client, Settings *settings_server, class Local_Storage *local_storage)
{
    std::string mods_folder = Local_Storage::get_game_settings_path() + "mods";
    nlohmann::json mod_items = nlohmann::json::object();
    static constexpr auto mods_json_file = "mods.json";
    std::string mods_json_path = Local_Storage::get_game_settings_path() + mods_json_file;
    if (local_storage->load_json(mods_json_path, mod_items)) {
        PRINT_DEBUG("Attempting to parse mods.json");
        try_parse_mods_file(settings_client, settings_server, mod_items, mods_folder);
    } else { // invalid mods.json or doesn't exist
        PRINT_DEBUG("Failed to load mods.json, attempting to auto detect mods folder");
        try_detect_mods_folder(settings_client, settings_server, mods_folder);
    }

}

// force_language.txt
static bool parse_force_language(std::string &language, const std::string &steam_settings_path)
{
    bool warn_forced = false;

    char forced_language[64] = {};
    int len = Local_Storage::get_file_data(steam_settings_path + "force_language.txt", forced_language, sizeof(forced_language) - 1);
    if (len > 0) {
        forced_language[len] = 0;
        warn_forced = true;
    }
    language = std::string(forced_language);

    return warn_forced;
}

// force_steamid.txt
static bool parse_force_user_steam_id(CSteamID &user_id, const std::string &steam_settings_path)
{
    bool warn_forced = false;

    char steam_id_text[32] = {};
    if (Local_Storage::get_file_data(steam_settings_path + "force_steamid.txt", steam_id_text, sizeof(steam_id_text) - 1) > 0) {
        CSteamID temp_id = CSteamID((uint64)std::atoll(steam_id_text));
        if (temp_id.IsValid()) {
            user_id = temp_id;
            warn_forced = true;
        }
    }

    return warn_forced;
}

// force_account_name.txt
static bool parse_force_account_name(std::string &name, const std::string &steam_settings_path)
{
    bool warn_forced = false;

    char forced_name[100] = {};
    int len = Local_Storage::get_file_data(steam_settings_path + "force_account_name.txt", forced_name, sizeof(forced_name) - 1);
    if (len > 0) {
        forced_name[len] = 0;
        warn_forced = true;
    }
    name = std::string(forced_name);

    return warn_forced;
}

// force_listen_port.txt
static bool parse_force_listen_port(uint16 &port, const std::string &steam_settings_path)
{
    bool warn_forced = false;

    char array_port[10] = {};
    int len = Local_Storage::get_file_data(steam_settings_path + "force_listen_port.txt", array_port, sizeof(array_port) - 1);
    if (len > 0) {
        port = std::stoi(array_port);
        warn_forced = true;
    }

    return warn_forced;
}

// build_id.txt
static void parse_build_id(class Settings *settings_client, class Settings *settings_server)
{
    std::string steam_settings_path(Local_Storage::get_game_settings_path());
    char array_id[10] = {};
    int len = Local_Storage::get_file_data(steam_settings_path + "build_id.txt", array_id, sizeof(array_id) - 1);
    if (len > 0) {
        int build_id = std::stoi(array_id);
        PRINT_DEBUG("build id = %i", build_id);
        settings_client->build_id = build_id;
        settings_server->build_id = build_id;
    }
}

// crash_printer_location.txt
static void parse_crash_printer_location()
{
    std::string installed_apps_list_path(Local_Storage::get_game_settings_path() + "crash_printer_location.txt");
    std::ifstream input( utf8_decode(installed_apps_list_path) );
    if (input.is_open()) {
        common_helpers::consume_bom(input);
        std::string line;
        std::getline( input, line );
        line = common_helpers::string_strip(line);
        if (!line.empty()) {
            auto crash_path = utf8_decode(get_full_program_path() + line);
            if (crash_printer::init(crash_path)) {
                PRINT_DEBUG("Unhandled crashes will be saved to '%s'", line.c_str());
            } else {
                PRINT_DEBUG("Failed to setup unhandled crash printer with path: '%s'", line.c_str());
            }
        }
    }
}

// auto_accept_invite.txt
static void parse_auto_accept_invite(class Settings *settings_client, class Settings *settings_server)
{
    std::string auto_accept_list_path = Local_Storage::get_game_settings_path() + "auto_accept_invite.txt";
    std::ifstream input( utf8_decode(auto_accept_list_path) );
    if (input.is_open()) {
        bool accept_any_invite = true;
        common_helpers::consume_bom(input);
        for( std::string line; getline( input, line ); ) {
            line = common_helpers::string_strip(line);
            if (!line.empty()) {
                accept_any_invite = false;
                try {
                    auto friend_id = std::stoull(line);
                    settings_client->addFriendToOverlayAutoAccept((uint64_t)friend_id);
                    settings_server->addFriendToOverlayAutoAccept((uint64_t)friend_id);
                    PRINT_DEBUG("Auto accepting overlay invitations from user with ID (SteamID64) = %llu", friend_id);
                } catch (...) {}
            }
        }

        if (accept_any_invite) {
            PRINT_DEBUG("Auto accepting any overlay invitation");
            settings_client->acceptAnyOverlayInvites(true);
            settings_server->acceptAnyOverlayInvites(true);
        } else {
            settings_client->acceptAnyOverlayInvites(false);
            settings_server->acceptAnyOverlayInvites(false);
        }
    }
}

// ip_country.txt
static void parse_ip_country(class Settings *settings_client, class Settings *settings_server)
{
    std::string setting_file = Local_Storage::get_game_settings_path() + "ip_country.txt";
    std::ifstream input( utf8_decode(setting_file) );
    if (input.is_open()) {
        common_helpers::consume_bom(input);
        std::string line{};
        std::getline( input, line );
        line = common_helpers::string_strip(line);
        // ISO 3166-1-alpha-2 format is 2 chars only
        if (line.size() == 2) {
            std::transform(line.begin(), line.end(), line.begin(), [](char c){ return std::toupper(c); } );
            settings_client->ip_country = line;
            settings_server->ip_country = line;
            PRINT_DEBUG("Setting IP country to: '%s'", line.c_str());
        }
    }
}

// overlay::hook_delay_sec
static void parse_overlay_hook_delay_sec(class Settings *settings_client, class Settings *settings_server)
{
    auto val = ini.GetLongValue("overlay", "hook_delay_sec", -1);
    if (val >= 0) {
        settings_client->overlay_hook_delay_sec = val;
        settings_server->overlay_hook_delay_sec = val;
        PRINT_DEBUG("Setting overlay hook delay to %i seconds", (int)val);
    }
}

// overlay::renderer_detector_timeout_sec
static void parse_overlay_renderer_detector_timeout_sec(class Settings *settings_client, class Settings *settings_server)
{
    auto val = ini.GetLongValue("overlay", "renderer_detector_timeout_sec", -1);
    if (val > 0) {
        settings_client->overlay_renderer_detector_timeout_sec = val;
        settings_server->overlay_renderer_detector_timeout_sec = val;
        PRINT_DEBUG("Setting overlay renderer detector timeout to %i seconds", (int)val);
    }
}

// mainly enable/disable features
static void parse_simple_features(class Settings *settings_client, class Settings *settings_server)
{
    // [general]
    settings_client->enable_new_app_ticket = ini.GetBoolValue("general", "new_app_ticket", settings_client->enable_new_app_ticket);
    settings_server->enable_new_app_ticket = ini.GetBoolValue("general", "new_app_ticket", settings_server->enable_new_app_ticket);

    settings_client->use_gc_token = ini.GetBoolValue("general", "gc_token", settings_client->use_gc_token);
    settings_server->use_gc_token = ini.GetBoolValue("general", "gc_token", settings_server->use_gc_token);

    settings_client->disable_account_avatar = ini.GetBoolValue("general", "disable_account_avatar", settings_client->disable_account_avatar);
    settings_server->disable_account_avatar = ini.GetBoolValue("general", "disable_account_avatar", settings_server->disable_account_avatar);

    settings_client->is_beta_branch = ini.GetBoolValue("general", "is_beta_branch", settings_client->is_beta_branch);
    settings_server->is_beta_branch = ini.GetBoolValue("general", "is_beta_branch", settings_server->is_beta_branch);

    settings_client->achievement_bypass = ini.GetBoolValue("general", "achievements_bypass", settings_client->achievement_bypass);
    settings_server->achievement_bypass = ini.GetBoolValue("general", "achievements_bypass", settings_server->achievement_bypass);

    settings_client->steam_deck = ini.GetBoolValue("general", "steam_deck", settings_client->steam_deck);
    settings_server->steam_deck = ini.GetBoolValue("general", "steam_deck", settings_server->steam_deck);

    settings_client->disable_leaderboards_create_unknown = ini.GetBoolValue("general", "disable_leaderboards_create_unknown", settings_client->disable_leaderboards_create_unknown);
    settings_server->disable_leaderboards_create_unknown = ini.GetBoolValue("general", "disable_leaderboards_create_unknown", settings_server->disable_leaderboards_create_unknown);

    settings_client->immediate_gameserver_stats = ini.GetBoolValue("general", "immediate_gameserver_stats", settings_client->immediate_gameserver_stats);
    settings_server->immediate_gameserver_stats = ini.GetBoolValue("general", "immediate_gameserver_stats", settings_server->immediate_gameserver_stats);

    settings_client->matchmaking_server_details_via_source_query = ini.GetBoolValue("general", "matchmaking_server_details_via_source_query", settings_client->matchmaking_server_details_via_source_query);
    settings_server->matchmaking_server_details_via_source_query = ini.GetBoolValue("general", "matchmaking_server_details_via_source_query", settings_server->matchmaking_server_details_via_source_query);

    settings_client->matchmaking_server_list_always_lan_type = ini.GetBoolValue("general", "matchmaking_server_list_actual_type", settings_client->matchmaking_server_list_always_lan_type);
    settings_server->matchmaking_server_list_always_lan_type = ini.GetBoolValue("general", "matchmaking_server_list_actual_type", settings_server->matchmaking_server_list_always_lan_type);


    // [connectivity]
    settings_client->disable_networking = ini.GetBoolValue("connectivity", "disable_networking", settings_client->disable_networking);
    settings_server->disable_networking = ini.GetBoolValue("connectivity", "disable_networking", settings_server->disable_networking);

    settings_client->disable_sharing_stats_with_gameserver = ini.GetBoolValue("connectivity", "disable_sharing_stats_with_gameserver", settings_client->disable_sharing_stats_with_gameserver);
    settings_server->disable_sharing_stats_with_gameserver = ini.GetBoolValue("connectivity", "disable_sharing_stats_with_gameserver", settings_server->disable_sharing_stats_with_gameserver);
    
    settings_client->disable_source_query = ini.GetBoolValue("connectivity", "disable_source_query", settings_client->disable_source_query);
    settings_server->disable_source_query = ini.GetBoolValue("connectivity", "disable_source_query", settings_server->disable_source_query);

    settings_client->share_leaderboards_over_network = ini.GetBoolValue("connectivity", "share_leaderboards_over_network", settings_client->share_leaderboards_over_network);
    settings_server->share_leaderboards_over_network = ini.GetBoolValue("connectivity", "share_leaderboards_over_network", settings_server->share_leaderboards_over_network);

    settings_client->disable_lobby_creation = ini.GetBoolValue("connectivity", "disable_lobby_creation", settings_client->disable_lobby_creation);
    settings_server->disable_lobby_creation = ini.GetBoolValue("connectivity", "disable_lobby_creation", settings_server->disable_lobby_creation);

    settings_client->download_steamhttp_requests = ini.GetBoolValue("connectivity", "download_steamhttp_requests", settings_client->download_steamhttp_requests);
    settings_server->download_steamhttp_requests = ini.GetBoolValue("connectivity", "download_steamhttp_requests", settings_server->download_steamhttp_requests);

    settings_client->force_steamhttp_success = ini.GetBoolValue("connectivity", "force_steamhttp_success", settings_client->force_steamhttp_success);
    settings_server->force_steamhttp_success = ini.GetBoolValue("connectivity", "force_steamhttp_success", settings_server->force_steamhttp_success);


    // [overlay]
    settings_client->disable_overlay = !ini.GetBoolValue("overlay", "enable_experimental_overlay", settings_client->disable_overlay);
    settings_server->disable_overlay = !ini.GetBoolValue("overlay", "enable_experimental_overlay", settings_server->disable_overlay);

    settings_client->disable_overlay_achievement_notification = ini.GetBoolValue("overlay", "disable_achievement_notification", settings_client->disable_overlay_achievement_notification);
    settings_server->disable_overlay_achievement_notification = ini.GetBoolValue("overlay", "disable_achievement_notification", settings_server->disable_overlay_achievement_notification);

    settings_client->disable_overlay_friend_notification = ini.GetBoolValue("overlay", "disable_friend_notification", settings_client->disable_overlay_friend_notification);
    settings_server->disable_overlay_friend_notification = ini.GetBoolValue("overlay", "disable_friend_notification", settings_server->disable_overlay_friend_notification);

    settings_client->disable_overlay_warning_any = ini.GetBoolValue("overlay", "disable_warning_any", settings_client->disable_overlay_warning_any);
    settings_server->disable_overlay_warning_any = ini.GetBoolValue("overlay", "disable_warning_any", settings_server->disable_overlay_warning_any);

    settings_client->disable_overlay_warning_bad_appid = ini.GetBoolValue("overlay", "disable_warning_bad_appid", settings_client->disable_overlay_warning_bad_appid);
    settings_server->disable_overlay_warning_bad_appid = ini.GetBoolValue("overlay", "disable_warning_bad_appid", settings_server->disable_overlay_warning_bad_appid);

    settings_client->disable_overlay_warning_forced_setting = ini.GetBoolValue("overlay", "disable_warning_forced_setting", settings_client->disable_overlay_warning_forced_setting);
    settings_server->disable_overlay_warning_forced_setting = ini.GetBoolValue("overlay", "disable_warning_forced_setting", settings_server->disable_overlay_warning_forced_setting);

    settings_client->disable_overlay_warning_local_save = ini.GetBoolValue("overlay", "disable_warning_local_save", settings_client->disable_overlay_warning_local_save);
    settings_server->disable_overlay_warning_local_save = ini.GetBoolValue("overlay", "disable_warning_local_save", settings_server->disable_overlay_warning_local_save);

}


static void load_main_config()
{
    static std::recursive_mutex ini_mtx{};
    static bool loaded = false;
    
    std::lock_guard lck(ini_mtx);
    if (loaded) return;
    loaded = true;

    constexpr const static auto merge_ini = [](const CSimpleIniA &new_ini) {
        std::list<CSimpleIniA::Entry> sections{};
        new_ini.GetAllSections(sections);
        for (auto const &sec : sections) {
            std::list<CSimpleIniA::Entry> keys{};
            new_ini.GetAllKeys(sec.pItem, keys);
            for (auto const &key : keys) {
                std::list<CSimpleIniA::Entry> vals{};
                new_ini.GetAllValues(sec.pItem, key.pItem, vals);
                for (const auto &val : vals) {
                    ini.SetValue(sec.pItem, key.pItem, val.pItem);
                }
            }
        }
    };

    ini.SetUnicode();

    CSimpleIniA local_ini{};
    local_ini.SetUnicode();

    // we have to load the local one first, since it might change base saves_folder_name
    {
        std::ifstream local_ini_file(Local_Storage::get_game_settings_path() + "configs.ini", std::ios::binary | std::ios::in);
        if (local_ini_file.is_open()) {
            auto err = local_ini.LoadData(local_ini_file);
            PRINT_DEBUG("result of parsing local configs.ini %i (success == 0)", (int)err);
            local_ini_file.close();
            
            if (err == SI_OK) {
                std::string saves_folder_name{};
                auto ptr = local_ini.GetValue("saves", "saves_folder_name", nullptr);
                if (ptr && ptr[0]) {
                    saves_folder_name = Settings::sanitize(common_helpers::string_strip(ptr));
                }
                
                if (saves_folder_name.size()) {
                    Local_Storage::set_saves_folder_name(saves_folder_name);
                    PRINT_DEBUG("changed name of the base folder used for save data to '%s'", saves_folder_name.c_str());
                }
            }
        }
    }
    
    {
        CSimpleIniA global_ini{};
        global_ini.SetUnicode();

        std::ifstream ini_file(Local_Storage::get_user_appdata_path() + Local_Storage::settings_storage_folder + PATH_SEPARATOR + "configs.ini", std::ios::binary | std::ios::in);
        if (ini_file.is_open()) {
            auto err = global_ini.LoadData(ini_file);
            PRINT_DEBUG("result of parsing global configs.ini %i (success == 0)", (int)err);
            ini_file.close();
            
            if (err == SI_OK) {
                merge_ini(global_ini);
            }
        }

    }

    if (!local_ini.IsEmpty()) {
        merge_ini(local_ini);
    }

#ifndef EMU_RELEASE_BUILD
    // dump the final ini file
    {
        PRINT_DEBUG("final ini start ---------");
        std::list<CSimpleIniA::Entry> sections{};
        ini.GetAllSections(sections);
        for (auto const &sec : sections) {
            PRINT_DEBUG("[%s]", sec.pItem);
            std::list<CSimpleIniA::Entry> keys{};
            ini.GetAllKeys(sec.pItem, keys);
            for (auto const &key : keys) {
                std::list<CSimpleIniA::Entry> vals{};
                ini.GetAllValues(sec.pItem, key.pItem, vals);
                for (const auto &val : vals) {
                    PRINT_DEBUG("%s=%s", key.pItem, val.pItem);
                }
            }
            PRINT_DEBUG("");
        }
        PRINT_DEBUG("final ini end *********");
    }
#endif // EMU_RELEASE_BUILD

    reset_LastError();

}


uint32 create_localstorage_settings(Settings **settings_client_out, Settings **settings_server_out, Local_Storage **local_storage_out)
{
    PRINT_DEBUG("start ----------");

    load_main_config();

#if defined(EMU_BUILD_STRING)
    PRINT_DEBUG("emu build '%s'", EXPAND_AS_STR(EMU_BUILD_STRING));
#else
    PRINT_DEBUG("<unspecified emu build>");
#endif

    parse_crash_printer_location();

    const std::string program_path(Local_Storage::get_program_path());
    const std::string steam_settings_path(Local_Storage::get_game_settings_path());
    uint32 appid = parse_steam_app_id(program_path);
    
    std::string save_path(Local_Storage::get_user_appdata_path());
    bool local_save = parse_local_save(program_path, save_path);
    local_save = parse_local_save(steam_settings_path, save_path); // allow file in local steam_settings folder to override it
    PRINT_DEBUG("program path: '%s', save path: '%s'", program_path.c_str(), save_path.c_str());

    Local_Storage *local_storage = new Local_Storage(save_path);
    PRINT_DEBUG("global settings path for this app/game: '%s'", local_storage->get_global_settings_path().c_str());
    local_storage->setAppId(appid);

    // Listen port
    uint16 port = parse_listen_port(local_storage);
    // Custom broadcasts
    std::set<IP_PORT> custom_broadcasts{};
    load_custom_broadcasts(local_storage->get_global_settings_path() + "custom_broadcasts.txt", custom_broadcasts);
    load_custom_broadcasts(steam_settings_path + "custom_broadcasts.txt", custom_broadcasts);

    // Acount name
    std::string name(parse_account_name(local_storage));
    // Steam ID
    CSteamID user_id = parse_user_steam_id(local_storage);
    // Language
    std::string language(parse_current_language(local_storage));
    // Supported languages, this will change the current language if needed
    std::set<std::string> supported_languages(parse_supported_languages(local_storage, language));


    // boolean flags and forced configurations

    bool warn_forced_setting = false;
    if (file_exists_(steam_settings_path + "force_steamid.txt")) {
        warn_forced_setting |= parse_force_user_steam_id(user_id, steam_settings_path);
    }

    if (file_exists_(steam_settings_path + "force_account_name.txt")) {
        warn_forced_setting |= parse_force_account_name(name, steam_settings_path);
    }

    if (file_exists_(steam_settings_path + "force_language.txt")) {
        warn_forced_setting |= parse_force_language(language, steam_settings_path);
    }

    if (file_exists_(steam_settings_path + "force_listen_port.txt")) {
        warn_forced_setting |= parse_force_listen_port(port, steam_settings_path);
    }

    bool steam_offline_mode = false;
    if (ini.GetBoolValue("connectivity", "offline", false)) {
        steam_offline_mode = true;
    }

    Settings *settings_client = new Settings(user_id, CGameID(appid), name, language, steam_offline_mode);
    Settings *settings_server = new Settings(generate_steam_id_server(), CGameID(appid), name, language, steam_offline_mode);

    // listen port
    settings_client->set_port(port);
    settings_server->set_port(port);
    // broadcasts list
    settings_client->custom_broadcasts = custom_broadcasts;
    settings_server->custom_broadcasts = custom_broadcasts;
    // overlay warning for forced setting
    settings_client->overlay_warn_forced_setting = warn_forced_setting;
    settings_server->overlay_warn_forced_setting = warn_forced_setting;
    // overlay warning for local save
    settings_client->overlay_warn_local_save = local_save;
    settings_server->overlay_warn_local_save = local_save;
    // supported languages list
    settings_client->set_supported_languages(supported_languages);
    settings_server->set_supported_languages(supported_languages);

    parse_build_id(settings_client, settings_server);

    parse_simple_features(settings_client, settings_server);
    parse_dlc(settings_client, settings_server);
    parse_app_paths(settings_client, settings_server, program_path);
    parse_leaderboards(settings_client, settings_server);
    parse_stats(settings_client, settings_server);
    parse_depots(settings_client, settings_server);
    parse_subscribed_groups(settings_client, settings_server);
    parse_installed_app_Ids(settings_client, settings_server);
    parse_force_branch_name(settings_client, settings_server);

    load_subscribed_groups_clans(local_storage->get_global_settings_path() + "subscribed_groups_clans.txt", settings_client, settings_server);
    load_subscribed_groups_clans(steam_settings_path + "subscribed_groups_clans.txt", settings_client, settings_server);

    load_overlay_appearance(local_storage->get_global_settings_path() + "overlay_appearance.txt", settings_client, settings_server, local_storage);
    load_overlay_appearance(steam_settings_path + "overlay_appearance.txt", settings_client, settings_server, local_storage);

    parse_mods_folder(settings_client, settings_server, local_storage);
    load_gamecontroller_settings(settings_client);
    parse_auto_accept_invite(settings_client, settings_server);
    parse_ip_country(settings_client, settings_server);
    parse_overlay_hook_delay_sec(settings_client, settings_server);
    parse_overlay_renderer_detector_timeout_sec(settings_client, settings_server);

    *settings_client_out = settings_client;
    *settings_server_out = settings_server;
    *local_storage_out = local_storage;

    reset_LastError();
    PRINT_DEBUG("end *********");
    return appid;
}

void save_global_settings(Local_Storage *local_storage, const char *name, const char *language)
{
    local_storage->store_data_settings("account_name.txt", name, strlen(name));
    local_storage->store_data_settings("language.txt", language, strlen(language));
}

bool settings_disable_lan_only()
{
    load_main_config();
    return ini.GetBoolValue("connectivity", "disable_lan_only", false);
}
