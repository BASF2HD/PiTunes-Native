#pragma once

#include <QIcon>

namespace PiTunes {
enum class PlayerIcon {
    Repeat, RepeatOne, Shuffle, Search, Volume, Mute, Previous, Play, Pause, Next, Left, Right,
    Drawer, Albums, Songs, Artists, Playlist, More, Settings, Favourite, Close, Stop
};

QIcon playerIcon(PlayerIcon icon, bool active = true);
} // namespace PiTunes
