#include <common/types.h>
#include <common/util/locks.h>

#ifndef __TTY_H__
#define __TTY_H__

enum OutputType {
    VGA_CONSOLE = 0,
    FB_CONSOLE = 1,
    SERIAL = 2,
};

typedef struct {
    uint8_t fg_colour : 4;
    uint8_t bg_colour : 4;
} tty_colour_t;

typedef struct {
    tty_colour_t colour;
    char actual_char;
} tty_char_t;

typedef struct {
    // Constant values
    int32_t width;
    int32_t height;

    // Draw Window Relative Positioning (used for displaying charachters)
    int32_t row;
    int32_t column;
    
    // TTY Draw Window
    uint32_t draw_base;
    uint32_t last_idx;      // Last refresh index (draw-relative)
    uint32_t current_idx;   // Current refresh index (draw-relative)
    
    // TTY Display Window
    uint32_t display_base;
    uint32_t scrollback_limit;    

    // TTY Buffer
    uint32_t buffer_size;   // Stored as number of tty_char_t's
    tty_char_t* buffer_base;

    // Misc.
    uint32_t* default_palette;
    uint32_t* current_palette;
    mutex_t* mutex;          // Only one thread can access the tty at a time
    tty_colour_t default_colour;

    // Refresh-related
    bool is_dirty;          // Set when the TTY needs to be redrawn
    bool just_scrolled;     // Set when the TTY was just scrolled
    bool refresh_back;      // Set when the TTY needs its background cleared and redrawn
} tty_dev_t;

/**
 * @brief  Initializes a previously allocated tty object
 * @note   If palette is equal to NULL, it defaults to the default EGA palette
 * @param  tty: The tty to initialize
 * @param  buffer: The pointer to the tty's buffer
 * @param  buffer_len: The length of the tty buffer
 * @param  palette: The palette to use
 * @retval None
 */
void tty_init(tty_dev_t* tty, uint16_t width, uint16_t height, void* buffer, size_t buffer_len, tty_colour_t* palette);

/**
 * @brief  Scrolls the specified tty in the specified direction
 * @note   If the direction is negative or positive, the tty will be scrolled up or down respectively
 *         If the direction is zero, the tty is scrolled to the last drawn line
 * @param  tty: The tty to scroll
 * @param  direction: The direction to scroll by
 * @retval True if the tty was scrolled successfully
 */
bool tty_scroll(tty_dev_t* tty, int direction);

/**
 * @brief  Moves or sets the cursor's position
 * @note   If relative is true, then the positions are added to the cursor position
 *         If the resultant position is outside of the tty bounds, they will be set to the limit
 * @param  tty: The tty to affect
 * @param  x: The new x position or offset of the cursor
 * @param  y: The new y position or offset of the cursor
 * @param  relative: Whether the cursor move is relative or not
 * @retval None
 */
void tty_set_cursor(tty_dev_t* tty, int x, int y, bool relative);

/**
 * @brief  Puts a string onto the tty
 * @note   
 * @param  tty: The tty to affect
 * @param  str: The string to put
 * @retval None
 */
void tty_puts(tty_dev_t* tty, const char* str);

/**
 * @brief  Puts a charachter onto the tty
 * @note   
 * @param  tty: The tty to affect
 * @param  c: The charachter to put
 * @retval None
 */
void tty_putchar(tty_dev_t* tty, const char c);

/**
 * @brief  Sets the default draw colour of the tty
 * @note   All of the colour indexes point into the palette
 * @param  tty: The tty to affect
 * @param  fg: The colour index of the text
 * @param  bg: The colour index of the background
 * @retval None
 */
void tty_set_colours(tty_dev_t* tty, uint8_t fg, uint8_t bg);

/**
 * @brief  Sets the tty's current palette
 * @note   This does not affect the tty's default palette
 * @param  tty: The tty to affect
 * @param  palette: The new palette to use
 * @param  refresh: Whether to apply the new palette to the screen or not
 * @retval None
 */
void tty_set_palette(tty_dev_t* tty, uint32_t* palette, bool refresh);

/**
 * @brief  Restores the tty's palette back to the default
 * @note   
 * @param  tty: The tty to affect
 * @param  refresh: Whether to apply the new palette to the screen or not
 * @retval None
 */
void tty_restore_palette(tty_dev_t* tty, bool refresh);

/**
 * @brief  Clears the tty scrollback and/or the screen
 * @note   
 * @param  tty: The tty to affect
 * @param  clear_all: Whether to also clear the entire screen
 * @retval None
 */
void tty_clear(tty_dev_t* tty, bool clear_all);

/**
 * @brief  Checks if the tty needs a redraw
 * @note   
 * @param  tty: The tty to affect
 * @retval True if the tty needs a redraw, false otherwise
 */
bool tty_is_dirty(tty_dev_t* tty);

/**
 * @brief  Cleans the tty after redrawing
 * @note   
 * @param  tty: The tty to affect
 * @retval None
 */
void tty_make_clean(tty_dev_t* tty);

/**
 * @brief  Makes the chosen tty the active one (i.e. the one used for the stdio functions)
 * @note   
 * @param  tty: The tty to be made active
 * @retval None
 */
void tty_select_active(tty_dev_t* tty);

/**
 * @brief  Gets the currently active tty (as selected by tty_select_active)
 * @note   
 * @retval The currently active tty
 */
tty_dev_t* tty_get_active();

void tty_reshow_fb(tty_dev_t* tty, void* fb_base, uint16_t x, uint16_t y);

#endif /* __TTY_H__ */
