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

#ifndef __INCLUDED_STEAM_TIMELINE_H__
#define __INCLUDED_STEAM_TIMELINE_H__

#include "base.h"

class Steam_Timeline :
public ISteamTimeline
{
private:
    struct TimelineEvent_t
    {
        // emu specific: time when this event was added to the list via 'Steam_Timeline::AddTimelineEvent()'
        const std::chrono::system_clock::time_point time_added = std::chrono::system_clock::now();

        // The name of the icon to show at the timeline at this point. This can be one of the icons uploaded through the Steamworks partner Site for your title, or one of the provided icons that start with steam_. The Steam Timelines overview includes a list of available icons.
        // https://partner.steamgames.com/doc/features/timeline#icons
        std::string pchIcon{};

        // Title-provided localized string in the language returned by SteamUtils()->GetSteamUILanguage().
        std::string pchTitle{};

        // Title-provided localized string in the language returned by SteamUtils()->GetSteamUILanguage().
        std::string pchDescription{};

        // Provide the priority to use when the UI is deciding which icons to display in crowded parts of the timeline. Events with larger priority values will be displayed more prominently than events with smaller priority values. This value must be between 0 and k_unMaxTimelinePriority.
        uint32 unPriority{}; 

        // One use of this parameter is to handle events whose significance is not clear until after the fact. For instance if the player starts a damage over time effect on another player, which kills them 3.5 seconds later, the game could pass -3.5 as the start offset and cause the event to appear in the timeline where the effect started.
        float flStartOffsetSeconds{};

        // The duration of the event, in seconds. Pass 0 for instantaneous events.
        float flDurationSeconds{};

        // Allows the game to describe events that should be suggested to the user as possible video clips.
        ETimelineEventClipPriority ePossibleClip{};
    };

    struct TimelineState_t
    {
        // emu specific: time when this state was changed via 'Steam_Timeline::SetTimelineGameMode()'
        const std::chrono::system_clock::time_point time_added = std::chrono::system_clock::now();
        
        std::string description{}; // A localized string in the language returned by SteamUtils()->GetSteamUILanguage()
        ETimelineGameMode bar_color{}; // the color of the timeline bar
    };

    class Settings *settings{};
    class Networking *network{};
    class SteamCallResults *callback_results{};
    class SteamCallBacks *callbacks{};
    class RunEveryRunCB *run_every_runcb{};

    std::vector<TimelineEvent_t> timeline_events{};
    std::vector<TimelineState_t> timeline_states{TimelineState_t{}}; // it seems to always start with a default event

    // unconditional periodic callback
    void RunCallbacks();
    // network callback, triggered once we have a network message
    void Callback(Common_Message *msg);

    static void steam_callback(void *object, Common_Message *msg);
    static void steam_run_every_runcb(void *object);

public:
    Steam_Timeline(class Settings *settings, class Networking *network, class SteamCallResults *callback_results, class SteamCallBacks *callbacks, class RunEveryRunCB *run_every_runcb);
    ~Steam_Timeline();

    void SetTimelineStateDescription( const char *pchDescription, float flTimeDelta );

    void ClearTimelineStateDescription( float flTimeDelta );

    void AddTimelineEvent( const char *pchIcon, const char *pchTitle, const char *pchDescription, uint32 unPriority, float flStartOffsetSeconds, float flDurationSeconds, ETimelineEventClipPriority ePossibleClip );

    void SetTimelineGameMode( ETimelineGameMode eMode );

};

#endif // __INCLUDED_STEAM_TIMELINE_H__
