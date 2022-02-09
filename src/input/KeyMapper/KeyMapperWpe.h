#pragma once

#include <linux/input.h>
#include <wpe/wpe.h>

namespace WPE {

class KeyMapper {
public:
    KeyMapper(const KeyMapper&) = delete;
    KeyMapper& operator=(const KeyMapper&) = delete;
    KeyMapper() = default;
    ~KeyMapper() = default;

public:
    static uint32_t KeyCodeToWpeKey(uint16_t code)
    {
        uint32_t keyCode = 0;
        switch (code) {
        case KEY_BACKSPACE:
            keyCode = WPE_KEY_BackSpace;
            break;
        case KEY_DELETE:
            keyCode = WPE_KEY_Delete;
            break;
        case KEY_TAB:
            keyCode = WPE_KEY_Tab;
            break;
        case KEY_LINEFEED:
        case KEY_ENTER:
        case KEY_KPENTER:
            keyCode = WPE_KEY_Return;
            break;
        case KEY_CLEAR:
            keyCode = WPE_KEY_Clear;
            break;
        case KEY_SPACE:
            keyCode = WPE_KEY_space;
            break;
        case KEY_HOME:
            keyCode = WPE_KEY_Home;
            break;
        case KEY_END:
            keyCode = WPE_KEY_End;
            break;
        case KEY_PAGEUP:
            keyCode = WPE_KEY_Prior;
            break;
        case KEY_PAGEDOWN:
            keyCode = WPE_KEY_Next;
            break;
        case KEY_LEFT:
            keyCode = WPE_KEY_Left;
            break;
        case KEY_RIGHT:
            keyCode = WPE_KEY_Right;
            break;
        case KEY_DOWN:
            keyCode = WPE_KEY_Down;
            break;
        case KEY_UP:
            keyCode = WPE_KEY_Up;
            break;
        case KEY_ESC:
            keyCode = WPE_KEY_Escape;
            break;
        case KEY_A:
            keyCode = WPE_KEY_a;
            break;
        case KEY_B:
            keyCode = WPE_KEY_b;
            break;
        case KEY_C:
            keyCode = WPE_KEY_c;
            break;
        case KEY_D:
            keyCode = WPE_KEY_d;
            break;
        case KEY_E:
            keyCode = WPE_KEY_e;
            break;
        case KEY_F:
            keyCode = WPE_KEY_f;
            break;
        case KEY_G:
            keyCode = WPE_KEY_g;
            break;
        case KEY_H:
            keyCode = WPE_KEY_h;
            break;
        case KEY_I:
            keyCode = WPE_KEY_i;
            break;
        case KEY_J:
            keyCode = WPE_KEY_j;
            break;
        case KEY_K:
            keyCode = WPE_KEY_k;
            break;
        case KEY_L:
            keyCode = WPE_KEY_l;
            break;
        case KEY_M:
            keyCode = WPE_KEY_m;
            break;
        case KEY_N:
            keyCode = WPE_KEY_n;
            break;
        case KEY_O:
            keyCode = WPE_KEY_o;
            break;
        case KEY_P:
            keyCode = WPE_KEY_p;
            break;
        case KEY_Q:
            keyCode = WPE_KEY_q;
            break;
        case KEY_R:
            keyCode = WPE_KEY_r;
            break;
        case KEY_S:
            keyCode = WPE_KEY_s;
            break;
        case KEY_T:
            keyCode = WPE_KEY_t;
            break;
        case KEY_U:
            keyCode = WPE_KEY_u;
            break;
        case KEY_V:
            keyCode = WPE_KEY_v;
            break;
        case KEY_W:
            keyCode = WPE_KEY_w;
            break;
        case KEY_X:
            keyCode = WPE_KEY_x;
            break;
        case KEY_Y:
            keyCode = WPE_KEY_y;
            break;
        case KEY_Z:
            keyCode = WPE_KEY_z;
            break;

        case KEY_0:
            keyCode = WPE_KEY_0;
            break;
        case KEY_1:
            keyCode = WPE_KEY_1;
            break;
        case KEY_2:
            keyCode = WPE_KEY_2;
            break;
        case KEY_3:
            keyCode = WPE_KEY_3;
            break;
        case KEY_4:
            keyCode = WPE_KEY_4;
            break;
        case KEY_5:
            keyCode = WPE_KEY_5;
            break;
        case KEY_6:
            keyCode = WPE_KEY_6;
            break;
        case KEY_7:
            keyCode = WPE_KEY_7;
            break;
        case KEY_8:
            keyCode = WPE_KEY_8;
            break;
        case KEY_9:
            keyCode = WPE_KEY_9;
            break;

        case KEY_MINUS:
            keyCode = WPE_KEY_minus;
            break;
        case KEY_EQUAL:
            keyCode = WPE_KEY_equal;
            break;
        case KEY_SEMICOLON:
            keyCode = WPE_KEY_semicolon;
            break;
        case KEY_APOSTROPHE:
            keyCode = WPE_KEY_apostrophe;
            break;
        case KEY_COMMA:
            keyCode = WPE_KEY_comma;
            break;
        case KEY_DOT:
            keyCode = WPE_KEY_period;
            break;
        case KEY_SLASH:
            keyCode = WPE_KEY_slash;
            break;
        case KEY_BACKSLASH:
            keyCode = WPE_KEY_backslash;
            break;
        case KEY_CAPSLOCK:
            keyCode = WPE_KEY_Caps_Lock;
            break;

        case KEY_RED:
            keyCode = WPE_KEY_Red;
            break;
        case KEY_GREEN:
            keyCode = WPE_KEY_Green;
            break;
        case KEY_YELLOW:
            keyCode = WPE_KEY_Yellow;
            break;
        case KEY_BLUE:
            keyCode = WPE_KEY_Blue;
            break;

        case KEY_NUMERIC_POUND:
            keyCode = WPE_KEY_sterling;
            break;
        case KEY_EURO:
            keyCode = WPE_KEY_EuroSign;
            break;

        case KEY_PLAY:
            keyCode = WPE_KEY_AudioPlay;
            break;
        case KEY_PAUSE:
            keycode = WPE_KEY_AudioPause;
            break;
        case KEY_FASTFORWARD:
            keyCode = WPE_KEY_AudioForward;
            break;
        case KEY_REWIND:
            keyCode = WPE_KEY_AudioRewind;
            break;

        default:
            break;
        }
        return keyCode;
    }
};    
}
