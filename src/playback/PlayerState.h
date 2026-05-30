#pragma once

// Player state machine for AudioPlayer.
enum class PlayerState {
    Stopped,
    Playing,
    Paused,
    Error,
};
