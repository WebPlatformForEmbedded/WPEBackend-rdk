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
    static uint32_t KeyCodeToWpeKey(uint16_t code, uint16_t modifier)
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
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_A : WPE_KEY_a;
            break;
        case KEY_B:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_B : WPE_KEY_b;
            break;
        case KEY_C:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_C : WPE_KEY_c;
            break;
        case KEY_D:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_D : WPE_KEY_d;
            break;
        case KEY_E:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_E : WPE_KEY_e;
            break;
        case KEY_F:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_F : WPE_KEY_f;
            break;
        case KEY_G:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_G : WPE_KEY_g;
            break;
        case KEY_H:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_H : WPE_KEY_h;
            break;
        case KEY_I:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_I : WPE_KEY_i;
            break;
        case KEY_J:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_J : WPE_KEY_j;
            break;
        case KEY_K:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_K : WPE_KEY_k;
            break;
        case KEY_L:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_L : WPE_KEY_l;
            break;
        case KEY_M:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_M : WPE_KEY_m;
            break;
        case KEY_N:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_N : WPE_KEY_n;
            break;
        case KEY_O:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_O : WPE_KEY_o;
            break;
        case KEY_P:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_P : WPE_KEY_p;
            break;
        case KEY_Q:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_Q : WPE_KEY_q;
            break;
        case KEY_R:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_R : WPE_KEY_r;
            break;
        case KEY_S:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_S : WPE_KEY_s;
            break;
        case KEY_T:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_T : WPE_KEY_t;
            break;
        case KEY_U:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_U : WPE_KEY_u;
            break;
        case KEY_V:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_V : WPE_KEY_v;
            break;
        case KEY_W:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_W : WPE_KEY_w;
            break;
        case KEY_X:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_X : WPE_KEY_x;
            break;
        case KEY_Y:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_Y : WPE_KEY_y;
            break;
        case KEY_Z:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_Z : WPE_KEY_z;
            break;

        case KEY_0:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_parenright : WPE_KEY_0;
            break;
        case KEY_1:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_exclam : WPE_KEY_1;
            break;
        case KEY_2:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_at : WPE_KEY_2;
            break;
        case KEY_3:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_numbersign : WPE_KEY_3;
            break;
        case KEY_4:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_dollar : WPE_KEY_4;
            break;
        case KEY_5:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_percent : WPE_KEY_5;
            break;
        case KEY_6:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_asciicircum : WPE_KEY_6;
            break;
        case KEY_7:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_ampersand : WPE_KEY_7;
            break;
        case KEY_8:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_asterisk : WPE_KEY_8;
            break;
        case KEY_9:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_parenleft : WPE_KEY_9;
            break;

        case KEY_MINUS:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_underscore : WPE_KEY_minus;
            break;
        case KEY_EQUAL:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_plus : WPE_KEY_equal;
            break;
        case KEY_SEMICOLON:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_colon : WPE_KEY_semicolon;
            break;
        case KEY_APOSTROPHE:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_quotedbl : WPE_KEY_apostrophe;
            break;
        case KEY_COMMA:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_less : WPE_KEY_comma;
            break;
        case KEY_DOT:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_greater : WPE_KEY_period;
            break;
        case KEY_SLASH:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_question : WPE_KEY_slash;
            break;
        case KEY_BACKSLASH:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_bar : WPE_KEY_backslash;
            break;
        case KEY_CAPSLOCK:
            keyCode = WPE_KEY_Caps_Lock;
            break;
        case KEY_LEFTBRACE:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_braceleft : WPE_KEY_bracketleft;
            break;
        case KEY_RIGHTBRACE:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_braceright : WPE_KEY_bracketright;
            break;
        case KEY_GRAVE:
            keyCode = (modifier == wpe_input_keyboard_modifier_shift) ? WPE_KEY_asciitilde : WPE_KEY_grave;
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
            keyCode = WPE_KEY_AudioPause;
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

    inline static uint16_t KeyCodeToWpeModifier(uint16_t code)
    {
        uint16_t modifier = 0;
        switch (code) {
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            modifier = wpe_input_keyboard_modifier_shift;
            break;
        case KEY_LEFTALT:
        case KEY_RIGHTALT:
            modifier = wpe_input_keyboard_modifier_alt;
            break;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            modifier = wpe_input_keyboard_modifier_control;
            break;
        default:
            break;
        }
        return modifier;
    }
};    
}
