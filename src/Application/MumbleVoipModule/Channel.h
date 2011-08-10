// For conditions of distribution and use, see copyright notice in license.txt

#pragma once

#include "MumbleFwd.h"
#include <QString>

namespace MumbleLib
{
    //! Wrapper over MumbleClient::Channel class. Presents an mumble channel.
    //! @todo Add signals: UserJoined, UserLeft ???
    //! @todo Add Users() method ???
    class Channel
    {
    public:
        //! Default constructor
        //! @param channel mumbleclient library Channel object 
        Channel(const ::MumbleClient::Channel* channel);

        virtual ~Channel();

        //! @return name of the channel
        QString Name() const;

        //! @return full recursive name of the channel. e.g. "Root/parent/channel"
        QString FullName() const;

        //! @return id of the channel
        int Id() const;

        //! @return description for the channel
        QString Description() const;

    private:
        const ::MumbleClient::Channel* channel_;
        QString channel_name_;
    };

}
