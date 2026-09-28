#include "host/host_gfx.h"

#include "board/board.h"

namespace {

BoardProfile gBoard = {
    "StickS3", 240, 135, 1, 18, 128, 16, "Btn B", ButtonArrow::None, true, false,
};

const BoardProfile kSticks = {
    "StickS3", 240, 135, 1, 18, 128, 16, "Btn B", ButtonArrow::None, true, false,
};
const BoardProfile kFeather = {
    "Reverse TFT Feather", 240, 135, 3, 18, 128, 16,
    "middle btn",          ButtonArrow::None, false, false,
};
const BoardProfile kTdisplay = {
    "T-Display-S3", 320, 170, 1, 24, 128, 16,
    "BOOT btn",     ButtonArrow::Left, true, true,
};

}  // namespace

const BoardProfile& board() { return gBoard; }

void hostSetBoard(const char* name) {
    if (name != nullptr && name[0] == 'f') {
        gBoard = kFeather;
    } else if (name != nullptr && name[0] == 't') {
        gBoard = kTdisplay;
    } else {
        gBoard = kSticks;
    }
}
