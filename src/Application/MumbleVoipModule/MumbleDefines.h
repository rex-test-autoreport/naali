// For conditions of distribution and use, see copyright notice in license.txt

#pragma once

//! \todo Rename to 'MumbleProtocol.h'
namespace MumbleVoip
{
    const int SAMPLE_RATE = 48000; // always 48000 in mumble
    const int FRAMES_PER_PACKET = 6;
    const int NUMBER_OF_CHANNELS = 1;
    const int SAMPLES_IN_FRAME = 480;
    const int SAMPLE_WIDTH = 16;
}
