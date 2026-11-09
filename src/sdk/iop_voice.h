// iop_voice.h -- the host mixer's volume groups, as RXSNDDRV tags the voices it allocates (RosePan's
// sdk/iop_voice.h, cut to what the driver uses). yamipan::iop::Spu2 keeps each voice's group for the
// voice engine, which is still to come.
#pragma once

// The player's volume group of a voice: music (BGM, ADPCM streams, the PCM input) or effects (SEs).
enum
{
    YAMIPAN_VOICE_GROUP_MUSIC = 0,
    YAMIPAN_VOICE_GROUP_EFFECTS = 1,
    YAMIPAN_VOICE_GROUPS
};

void YamiPan_VoiceSetGroup(int core, int voice, int group);
