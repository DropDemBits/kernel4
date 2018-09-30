/**
 * Copyright (C) 2018 DropDemBits
 * 
 * This file is part of Kernel4.
 * 
 * Kernel4 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kernel4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kernel4.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

/*
 * Based off the USB HID usage tableS
 */

#ifndef __KEYCODES_H__
#define __KEYCODES_H__

// Modifiers
#define KEY_MOD_LCTRL           0x01
#define KEY_MOD_LSHIFT          0x02
#define KEY_MOD_LALT            0x04
#define KEY_MOD_LGUI            0x08
#define KEY_MOD_RCTRL           0x10
#define KEY_MOD_RSHIFT          0x20
#define KEY_MOD_RALT            0x40
#define KEY_MOD_RGUI            0x80

// Table 0x07
#define KEY_RESERVED            0x00
// 0x01 Error Roll Over
// 0x02 POST Fail
// 0x03 Error Undefined
#define KEY_A                   0x04
#define KEY_B                   0x05
#define KEY_C                   0x06
#define KEY_D                   0x07
#define KEY_E                   0x08
#define KEY_F                   0x09
#define KEY_G                   0x0a
#define KEY_H                   0x0b
#define KEY_I                   0x0c
#define KEY_J                   0x0d
#define KEY_K                   0x0e
#define KEY_L                   0x0f
#define KEY_M                   0x10
#define KEY_N                   0x11
#define KEY_O                   0x12
#define KEY_P                   0x13
#define KEY_Q                   0x14
#define KEY_R                   0x15
#define KEY_S                   0x16
#define KEY_T                   0x17
#define KEY_U                   0x18
#define KEY_V                   0x19
#define KEY_W                   0x1a
#define KEY_X                   0x1b
#define KEY_Y                   0x1c
#define KEY_Z                   0x1d

#define KEY_1                   0x1e
#define KEY_2                   0x1f
#define KEY_3                   0x20
#define KEY_4                   0x21
#define KEY_5                   0x22
#define KEY_6                   0x23
#define KEY_7                   0x24
#define KEY_8                   0x25
#define KEY_9                   0x26
#define KEY_0                   0x27

#define KEY_ENTER               0x28
#define KEY_ESCAPE              0x29
#define KEY_BACKSPACE           0x2a
#define KEY_TAB                 0x2b
#define KEY_SPACE               0x2c
#define KEY_DASH                0x2d
#define KEY_EQUAL               0x2e
#define KEY_L_BRACE             0x2f
#define KEY_R_BRACE             0x30
#define KEY_BACKSLASH           0x31
#define KEY_HASH                0x32
#define KEY_SEMICOLON           0x33
#define KEY_APOSTROPHE          0x34
#define KEY_GRAVE               0x35
#define KEY_COMMA               0x36
#define KEY_DOT                 0x37
#define KEY_SLASH               0x38
#define KEY_CAPSLOCK            0x39

#define KEY_F1                  0x3a
#define KEY_F2                  0x3b
#define KEY_F3                  0x3c
#define KEY_F4                  0x3d
#define KEY_F5                  0x3e
#define KEY_F6                  0x3f
#define KEY_F7                  0x40
#define KEY_F8                  0x41
#define KEY_F9                  0x42
#define KEY_F10                 0x43
#define KEY_F11                 0x44
#define KEY_F12                 0x45
#define KEY_PRINTSCR            0x46
#define KEY_SCROLL_LOCK         0x47
#define KEY_PAUSE               0x48
#define KEY_INSERT              0x49
#define KEY_HOME                0x4a
#define KEY_PG_UP               0x4b
#define KEY_DELETE              0x4c
#define KEY_END                 0x4d
#define KEY_PG_DOWN             0x4e

#define KEY_RIGHT_ARROW         0x4f
#define KEY_LEFT_ARROW          0x50
#define KEY_DOWN_ARROW          0x51
#define KEY_UP_ARROW            0x52

#define KEY_NUMLOCK             0x53
#define KEY_KPSLASH             0x54
#define KEY_KPSTAR              0x55
#define KEY_KPDASH              0x56
#define KEY_KPPLUS              0x57
#define KEY_KPENTER             0x58
#define KEY_KP1                 0x59
#define KEY_KP2                 0x5a
#define KEY_KP3                 0x5b
#define KEY_KP4                 0x5c
#define KEY_KP5                 0x5d
#define KEY_KP6                 0x5e
#define KEY_KP7                 0x5f
#define KEY_KP8                 0x60
#define KEY_KP9                 0x61
#define KEY_KP0                 0x62
#define KEY_KPDOT               0x63
#define KEY_NU_SLASH            0x64
#define KEY_APP                 0x65
#define KEY_POWER               0x66

#define KEY_KPEQUAL             0x67

#define KEY_F13                 0x68
#define KEY_F14                 0x69
#define KEY_F15                 0x6a
#define KEY_F16                 0x6b
#define KEY_F17                 0x6c
#define KEY_F18                 0x6d
#define KEY_F19                 0x6e
#define KEY_F20                 0x6f
#define KEY_F21                 0x70
#define KEY_F22                 0x71
#define KEY_F23                 0x72
#define KEY_F24                 0x73

#define KEY_EXEC                0x74
#define KEY_HELP                0x75
#define KEY_MENU                0x76
#define KEY_SELECT              0x77
#define KEY_STOP                0x78
#define KEY_REPEAT              0x79
#define KEY_UNDO                0x7a
#define KEY_CUT                 0x7b
#define KEY_COPY                0x7c
#define KEY_PASTE               0x7d
#define KEY_FIND                0x7e
#define KEY_MUTE                0x7f
#define KEY_VOLUP               0x80
#define KEY_VOLDOWN             0x81

#define KEY_LK_CAPS             0x82
#define KEY_LK_NUM              0x83
#define KEY_LK_SCROLL           0x84
#define KEY_KPCOMMA_INTL        0x85
#define KEY_KPEQUAL_AS          0x86
#define KEY_INTL1               0x87
#define KEY_INTL2               0x88
#define KEY_INTL3               0x89
#define KEY_INTL4               0x8a
#define KEY_INTL5               0x8b
#define KEY_INTL6               0x8c
#define KEY_INTL7               0x8d
#define KEY_INTL8               0x8e
#define KEY_INTL9               0x8f
#define KEY_LANG1               0x90
#define KEY_LANG2               0x91
#define KEY_LANG3               0x92
#define KEY_LANG4               0x93
#define KEY_LANG5               0x94
#define KEY_LANG6               0x95
#define KEY_LANG7               0x96
#define KEY_LANG8               0x97
#define KEY_LANG9               0x98
#define KEY_ERASE               0x99
#define KEY_SYSRQ               0x9a
#define KEY_CANCEL              0x9b
#define KEY_BLANK               0x9c
#define KEY_PRIOR               0x9d
#define KEY_RETURN              0x9e
#define KEY_SEPARATOR           0x9f
#define KEY_OUT                 0xa0
#define KEY_OPER                0xa1

#define KEY_CLEAR               0xa2
#define KEY_CRSEL               0xa3
#define KEY_EXSEL               0xa4
// 0xA5-0xAF Reserved

#define KEY_KP00                0xb0
#define KEY_KP000               0xb1
#define KEY_THOU_SEP            0xb2
#define KEY_DECI_SEP            0xb3
#define KEY_CURRENCY            0xb4
#define KEY_CURRENCY_SUB        0xb5
#define KEY_KPL_PAREN           0xb6
#define KEY_KPR_PAREN           0xb7
#define KEY_KPL_CURLY           0xb8
#define KEY_KPR_CURLY           0xb9
#define KEY_KPTAB               0xba
#define KEY_KPBACKSPACE         0xbb
#define KEY_KPA                 0xbc
#define KEY_KPB                 0xbd
#define KEY_KPC                 0xbe
#define KEY_KPD                 0xbf
#define KEY_KPE                 0xc0
#define KEY_KPF                 0xc1
#define KEY_KPXOR               0xc2
#define KEY_KPBITXOR            0xc3
#define KEY_KPPERCENT           0xc4
#define KEY_KPLT                0xc5
#define KEY_KPGT                0xc6
#define KEY_KPBITAND            0xc7
#define KEY_KPAND               0xc8
#define KEY_KPBITOR             0xc9
#define KEY_KPOR                0xca
#define KEY_KPCOLON             0xcb
#define KEY_KPHASH              0xcc
#define KEY_KPSPACE             0xcd
#define KEY_KPAT                0xce
#define KEY_KPNOT               0xcf
#define KEY_KPMEM_STORE         0xd0
#define KEY_KPMEM_RECALL        0xd1
#define KEY_KPMEM_CLR           0xd2
#define KEY_KPMEM_ADD           0xd3
#define KEY_KPMEM_SUB           0xd4
#define KEY_KPMEM_MUL           0xd5
#define KEY_KPMEM_DIV           0xd6
#define KEY_KP_PL               0xd7
#define KEY_KPCLR               0xd8
#define KEY_KPCLR_ENTRY         0xd9
#define KEY_KPBIN               0xda
#define KEY_KPOCT               0xdb
#define KEY_KPDEC               0xdc
#define KEY_KPHEX               0xdd
// 0xDE-0xDF Reserved

#define KEY_L_CTRL              0xe0
#define KEY_L_SHIFT             0xe1
#define KEY_L_ALT               0xe2
#define KEY_L_GUI               0xe3
#define KEY_R_CTRL              0xe4
#define KEY_R_SHIFT             0xe5
#define KEY_R_ALT               0xe6
#define KEY_R_GUI               0xe7

// Table 0x0C
#define KEY_MEDIA_NEXT_TRACK    0xe8
#define KEY_MEDIA_PREV_TRACK    0xe9
#define KEY_MEDIA_STOP          0xea
#define KEY_MEDIA_PLAYPAUSE     0xeb
#define KEY_MEDIA_MUTE          0xec
#define KEY_MEDIA_VOLUP         0xed
#define KEY_MEDIA_VOLDOWN       0xee
#define KEY_MEDIA_SELECT        0xef
#define KEY_MEDIA_MAIL          0xf0
#define KEY_MEDIA_CALC          0xf1
#define KEY_MEDIA_PC            0xf2
#define KEY_WEB_SEARCH          0xf3
#define KEY_WEB_HOME            0xf4
#define KEY_WEB_BACK            0xf5
#define KEY_WEB_FORWARD         0xf6
#define KEY_WEB_STOP            0xf7
#define KEY_WEB_REFRESH         0xf8
#define KEY_WEB_FAV             0xf9

// Table 0x01
#define KEY_SYS_POWER           0xfa
#define KEY_SYS_SLEEP           0xfb
#define KEY_SYS_WAKE            0xfc

#define KEYCHAR_MAP_DEFAULT \
/*x00*/{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'}, \
/*x04*/{'a','A','\0'},{'b','B','\0'},{'c','C','\0'},{'d','D','\0'}, \
/*x08*/{'e','E','\0'},{'f','F','\0'},{'g','G','\0'},{'h','H','\0'}, \
/*x0C*/{'i','I','\0'},{'j','J','\0'},{'k','K','\0'},{'l','L','\0'}, \
/*x10*/{'m','M','\0'},{'n','N','\0'},{'o','O','\0'},{'p','P','\0'}, \
/*x14*/{'q','Q','\0'},{'r','R','\0'},{'s','S','\0'},{'t','T','\0'}, \
/*x18*/{'u','U','\0'},{'v','V','\0'},{'w','W','\0'},{'x','X','\0'}, \
/*x1C*/{'y','Y','\0'},{'z','Z','\0'},{'1','!','\0'},{'2','@','\0'}, \
/*x20*/{'3','#','\0'},{'4','$','\0'},{'5','%','\0'},{'6','^','\0'}, \
/*x24*/{'7','&','\0'},{'8','*','\0'},{'9','(','\0'},{'0',')','\0'}, \
/*x28*/{'\n','\n','\0'},{'\0','\0','\0'},{'\b','\b','\0'},{'\t','\t','\0'}, \
/*x2C*/{' ',' ','\0'},{'-','_','\0'},{'=','+','\0'},{'[','{','\0'}, \
/*x30*/{']','}','\0'},{'\\','|','\0'},{'\\','|','\0'},{';',':','\0'}, \
/*x34*/{'\'','\"','\0'},{'`','~','\0'},{',','<','\0'},{'.','>','\0'}, \
/*x38*/{'/','?','\0'},{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'}, \
/*x3C*/{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'}, \
/*x40*/{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'}, \
/*x44*/{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'}, \
/*x48*/{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'}, \
/*x4C*/{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'}, \
/*x50*/{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'}, \
/*x54*/{'/','/','\0'},{'*','*','\0'},{'-','-','\0'},{'+','+','\0'}, \
/*x58*/{'\n','\n','\0'},{'1','\0','\0'},{'2','\0','\0'},{'3','\0','\0'}, \
/*x5C*/{'4','\0','\0'},{'5','\0','\0'},{'6','\0','\0'},{'7','\0','\0'}, \
/*x60*/{'8','\0','\0'},{'9','\0','\0'},{'0','\0','\0'},{'.','\0','\0'}, \
/*x64*/{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'}, \
/*x68*/{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'}, \
/*x6C*/{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'},{'\0','\0','\0'}, \
/*x70*/{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}, \
/*x80*/{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}, \
/*x90*/{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}, \
/*xA0*/{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}, \
/*xB0*/{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}, \
/*xC0*/{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}, \
/*xD0*/{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}, \
/*xE0*/{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}, \
/*xF0*/{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},

// Offsets: Normal +0x00, E0 +0x80, E1 +0x80
#define PS2_SET2_MAP \
/*x00*/ KEY_RESERVED, KEY_F9,       KEY_RESERVED, KEY_F5,      \
/*x04*/ KEY_F3,       KEY_F1, KEY_F2, KEY_F12,\
/*x08*/ KEY_RESERVED, KEY_F10, KEY_F8, KEY_F6,\
/*x0C*/ KEY_F4, KEY_TAB, KEY_GRAVE, KEY_RESERVED,\
/*x10*/ KEY_RESERVED, KEY_L_ALT, KEY_L_SHIFT, KEY_RESERVED,\
/*x14*/ KEY_L_CTRL, KEY_Q, KEY_1, KEY_RESERVED,\
/*x18*/ KEY_RESERVED, KEY_RESERVED, KEY_Z, KEY_S,\
/*x1C*/ KEY_A, KEY_W, KEY_2, KEY_RESERVED,\
/*x20*/ KEY_RESERVED, KEY_C, KEY_X, KEY_D,\
/*x24*/ KEY_E, KEY_4, KEY_3, KEY_RESERVED,\
/*x28*/ KEY_RESERVED, KEY_SPACE, KEY_V, KEY_F,\
/*x2C*/ KEY_T, KEY_R, KEY_5, KEY_RESERVED,\
/*x30*/ KEY_RESERVED, KEY_N, KEY_B, KEY_H,\
/*x34*/ KEY_G, KEY_Y, KEY_6, KEY_RESERVED,\
/*x38*/ KEY_RESERVED, KEY_RESERVED, KEY_M, KEY_J,\
/*x3C*/ KEY_U, KEY_7, KEY_8, KEY_RESERVED,\
/*x40*/ KEY_RESERVED, KEY_COMMA, KEY_K, KEY_I,\
/*x44*/ KEY_O, KEY_0, KEY_9, KEY_RESERVED,\
/*x48*/ KEY_RESERVED, KEY_DOT, KEY_SLASH, KEY_L,\
/*x4C*/ KEY_SEMICOLON, KEY_P, KEY_DASH, KEY_RESERVED,\
/*x50*/ KEY_RESERVED, KEY_RESERVED, KEY_APOSTROPHE, KEY_RESERVED,\
/*x54*/ KEY_L_BRACE, KEY_EQUAL, KEY_RESERVED, KEY_RESERVED,\
/*x58*/ KEY_CAPSLOCK, KEY_R_SHIFT, KEY_ENTER, KEY_R_BRACE,\
/*x5C*/ KEY_RESERVED, KEY_BACKSLASH, KEY_RESERVED, KEY_RESERVED,\
/*x60*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x64*/ KEY_RESERVED, KEY_RESERVED, KEY_BACKSPACE, KEY_RESERVED,\
/*x68*/ KEY_RESERVED, KEY_KP1, KEY_RESERVED, KEY_KP4,\
/*x6C*/ KEY_KP7, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x70*/ KEY_KP0, KEY_KPDOT, KEY_KP2, KEY_KP5,\
/*x74*/ KEY_KP6, KEY_KP8, KEY_ESCAPE, KEY_NUMLOCK,\
/*x78*/ KEY_F11, KEY_KPPLUS, KEY_KP3, KEY_KPDASH,\
/*x7C*/ KEY_KPSTAR, KEY_KP9, KEY_SCROLL_LOCK, KEY_RESERVED,\
/*x80*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_F7,\
/*x84*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x88*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x8C*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x90*/ KEY_WEB_SEARCH, KEY_R_ALT, KEY_RESERVED, KEY_RESERVED,\
/*x94*/ KEY_R_CTRL, KEY_MEDIA_PREV_TRACK, KEY_RESERVED, KEY_RESERVED,\
/*x98*/ KEY_WEB_FAV, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x9C*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_L_GUI,\
/*xA0*/ KEY_WEB_REFRESH, KEY_MEDIA_VOLDOWN, KEY_RESERVED, KEY_MEDIA_MUTE,\
/*xA4*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_R_GUI,\
/*xA8*/ KEY_WEB_STOP, KEY_RESERVED, KEY_RESERVED, KEY_MEDIA_CALC,\
/*xAC*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_APP,\
/*xB0*/ KEY_WEB_FORWARD, KEY_RESERVED, KEY_MEDIA_VOLUP, KEY_RESERVED,\
/*xB4*/ KEY_MEDIA_PLAYPAUSE, KEY_RESERVED, KEY_RESERVED, KEY_SYS_POWER,\
/*xB8*/ KEY_WEB_BACK, KEY_RESERVED, KEY_WEB_HOME, KEY_MEDIA_STOP,\
/*xBC*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_SYS_SLEEP,\
/*xC0*/ KEY_MEDIA_PC, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xC4*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xC8*/ KEY_MEDIA_MAIL, KEY_RESERVED, KEY_KPSLASH, KEY_RESERVED,\
/*xCC*/ KEY_RESERVED, KEY_MEDIA_NEXT_TRACK, KEY_RESERVED, KEY_RESERVED,\
/*xD0*/ KEY_MEDIA_SELECT, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xD4*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xD8*/ KEY_RESERVED, KEY_RESERVED, KEY_KPENTER, KEY_RESERVED,\
/*xDC*/ KEY_RESERVED, KEY_RESERVED, KEY_SYS_WAKE, KEY_RESERVED,\
/*xE0*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xE4*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xE8*/ KEY_RESERVED, KEY_END, KEY_RESERVED, KEY_LEFT_ARROW,\
/*xEC*/ KEY_HOME, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xF0*/ KEY_INSERT, KEY_DELETE, KEY_DOWN_ARROW, KEY_RESERVED,\
/*xF4*/ KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_RESERVED, KEY_PAUSE,\
/*xF8*/ KEY_RESERVED, KEY_RESERVED, KEY_PG_DOWN, KEY_RESERVED,\
/*xFC*/ KEY_PRINTSCR, KEY_PG_UP, KEY_RESERVED, KEY_RESERVED,

// Offsets: Normal +0x00, Extended +0x50, Extended special (pause+0xA0, sysrq+0xB0)
#define PS2_SET1_MAP \
/*x00*/ KEY_RESERVED, KEY_ESCAPE, KEY_1, KEY_2,\
/*x04*/ KEY_3, KEY_4, KEY_5, KEY_6,\
/*x08*/ KEY_7, KEY_8, KEY_9, KEY_0,\
/*x0C*/ KEY_DASH, KEY_EQUAL, KEY_BACKSPACE, KEY_TAB,\
/*x10*/ KEY_Q, KEY_W, KEY_E, KEY_R,\
/*x14*/ KEY_T, KEY_Y, KEY_U, KEY_I,\
/*x18*/ KEY_O, KEY_P, KEY_L_BRACE, KEY_R_BRACE,\
/*x1C*/ KEY_ENTER, KEY_L_CTRL, KEY_A, KEY_S,\
/*x20*/ KEY_D, KEY_F, KEY_G, KEY_H,\
/*x24*/ KEY_J, KEY_K, KEY_L, KEY_SEMICOLON,\
/*x28*/ KEY_APOSTROPHE, KEY_GRAVE, KEY_L_SHIFT, KEY_BACKSLASH,\
/*x2C*/ KEY_Z, KEY_X, KEY_C, KEY_V,\
/*x30*/ KEY_B, KEY_N, KEY_M, KEY_COMMA,\
/*x34*/ KEY_DOT, KEY_SLASH, KEY_R_SHIFT, KEY_KPSTAR,\
/*x38*/ KEY_L_ALT, KEY_SPACE, KEY_CAPSLOCK, KEY_F1,\
/*x3C*/ KEY_F2, KEY_F3, KEY_F4, KEY_F5,\
/*x40*/ KEY_F6, KEY_F7, KEY_F8, KEY_F9,\
/*x44*/ KEY_F10, KEY_NUMLOCK, KEY_SCROLL_LOCK, KEY_KP7,\
/*x48*/ KEY_KP8, KEY_KP9, KEY_KPDASH, KEY_KP4,\
/*x4C*/ KEY_KP5, KEY_KP6, KEY_KPPLUS, KEY_KP1,\
/*x50*/ KEY_KP2, KEY_KP3, KEY_KP0, KEY_KPDOT,\
/*x54*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_F11,\
/*x58*/ KEY_F12, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x5C*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/* Begin extended */ \
/*x60*/ KEY_MEDIA_PREV_TRACK, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x64*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x68*/ KEY_RESERVED, KEY_MEDIA_NEXT_TRACK, KEY_RESERVED, KEY_RESERVED,\
/*x6C*/ KEY_KPENTER, KEY_R_CTRL, KEY_RESERVED, KEY_RESERVED,\
/*x70*/ KEY_MEDIA_MUTE, KEY_MEDIA_CALC, KEY_MEDIA_PLAYPAUSE, KEY_RESERVED,\
/*x74*/ KEY_MEDIA_STOP, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x78*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x7C*/ KEY_RESERVED, KEY_RESERVED, KEY_MEDIA_VOLDOWN, KEY_RESERVED,\
/*x80*/ KEY_MEDIA_VOLUP, KEY_RESERVED, KEY_WEB_HOME, KEY_RESERVED,\
/*x84*/ KEY_RESERVED, KEY_KPSLASH, KEY_RESERVED, KEY_RESERVED,\
/*x88*/ KEY_R_ALT, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x8C*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x90*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*x94*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_HOME,\
/*x98*/ KEY_UP_ARROW, KEY_PG_UP, KEY_RESERVED, KEY_LEFT_ARROW,\
/*x9C*/ KEY_RESERVED, KEY_RIGHT_ARROW, KEY_RESERVED, KEY_END,\
/*xA0*/ KEY_DOWN_ARROW, KEY_PG_DOWN, KEY_INSERT, KEY_DELETE,\
/*xA4*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xA8*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_L_GUI,\
/*xAC*/ KEY_R_GUI, KEY_APP, KEY_SYS_POWER, KEY_SYS_SLEEP,\
/*xB0*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_SYS_WAKE,\
/*xB4*/ KEY_RESERVED, KEY_WEB_SEARCH, KEY_WEB_FAV, KEY_WEB_REFRESH,\
/*xB8*/ KEY_WEB_STOP, KEY_WEB_FORWARD, KEY_WEB_BACK, KEY_MEDIA_PC,\
/*xBC*/ KEY_MEDIA_MAIL, KEY_MEDIA_SELECT, KEY_RESERVED, KEY_RESERVED,\
/*xC0*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xC4*/ KEY_RESERVED, KEY_PAUSE, KEY_RESERVED, KEY_PRINTSCR,\
/*xC8*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xCC*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xD0*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xD4*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xD8*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xDC*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xE0*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xE4*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xE8*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xEC*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xF0*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xF4*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xF8*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,\
/*xFC*/ KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,

#endif /* end of include guard: __KEYCODES_H__ */
