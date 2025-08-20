// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave and eyalabraham
// (Dragon 32 emu core) are thanked profusely.
//
// The Micro-DS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#include    <nds.h>
#include    <stdint.h>

#include    "cpu.h"
#include    "mem.h"
#include    "vdg.h"
#include    "font.h"
#include    "semigraph.h"
#include    "MicroUtils.h"

/* -----------------------------------------
   Module functions
----------------------------------------- */
void vdg_render_alpha_semi4(int vdg_mem_base);
void vdg_render_semi6(int vdg_mem_base);
void vdg_render_resl_graph(video_mode_t mode, int vdg_mem_base);
void vdg_render_color_graph(video_mode_t mode, int vdg_mem_base);
void vdg_render_artifacting_mono(video_mode_t mode, int vdg_mem_base);
void vdg_render_artifacting_green(video_mode_t mode, int vdg_mem_base);

video_mode_t vdg_get_mode(void);

/* -----------------------------------------
   Module globals
----------------------------------------- */
video_mode_t current_vdg_mode   __attribute__((section(".dtcm")));
int reduce_framerate_for_tape   __attribute__((section(".dtcm"))) = 0;

/* The following table lists the pixel ratio of columns and rows
 * relative to a 768x384 frame buffer resolution.
 */
int resolution[][3] __attribute__((section(".dtcm"))) = {
    { 1, 1,  512 },  // ALPHA_INTERNAL,     2 color 32x16   512B Default
    { 1, 1,  512 },  // ALPHA_EXTERNAL,     4 color 32x16   512B
    { 1, 1,  512 },  // SEMI_GRAPHICS_4,    8 color 64x32   512B
    { 1, 1,  512 },  // SEMI_GRAPHICS_6,    8 color 64x48   512B
    { 1, 1, 2048 },  // SEMI_GRAPHICS_8,    8 color 64x64   2048B
    { 1, 1, 3072 },  // SEMI_GRAPHICS_12,   8 color 64x96   3072B
    { 1, 1, 6144 },  // SEMI_GRAPHICS_24,   8 color 64x192  6144B
    { 4, 3, 1024 },  // GRAPHICS_1C,        4 color 64x64   1024B
    { 2, 3, 1024 },  // GRAPHICS_1R,        2 color 128x64  1024B
    { 2, 3, 2048 },  // GRAPHICS_2C,        4 color 128x64  2048B
    { 2, 2, 1536 },  // GRAPHICS_2R,        2 color 128x96  1536B PMODE 0
    { 2, 2, 3072 },  // GRAPHICS_3C,        4 color 128x96  3072B PMODE 1
    { 2, 1, 3072 },  // GRAPHICS_3R,        2 color 128x192 3072B PMODE 2
    { 2, 1, 6144 },  // GRAPHICS_6C,        4 color 128x192 6144B PMODE 3
    { 1, 1, 6144 },  // GRAPHICS_6R,        2 color 256x192 6144B PMODE 4
    { 1, 1, 6144 },  // DMA,                2 color 256x192 6144B
};

uint8_t colors[] __attribute__((section(".dtcm"))) = {
        FB_BLACK,

        FB_GREEN,
        FB_YELLOW,
        FB_BLUE,
        FB_RED,

        FB_BUFF,
        FB_CYAN,
        FB_MAGENTA,
        FB_ORANGE,

        ARTIFACT_BLUE,
        ARTIFACT_ORANGE,
        ARTIFACT_GREEN,

        FB_DKGRN,
        FB_DKORG,
        FB_LTGRN,
        FB_LTORG,
};

uint16_t colors16[] __attribute__((section(".dtcm"))) = {
        (FB_GREEN<<8)   | FB_GREEN,
        (FB_YELLOW<<8)  | FB_YELLOW,
        (FB_BLUE<<8)    | FB_BLUE,
        (FB_RED<<8)     | FB_RED,

        (FB_BUFF<<8)    | FB_BUFF,
        (FB_CYAN<<8)    | FB_CYAN,
        (FB_MAGENTA<<8) | FB_MAGENTA,
        (FB_ORANGE<<8)  | FB_ORANGE,
};

uint32_t color_translation_32[16][16]  __attribute__((section(".dtcm"))) = {0};
uint32_t color_translation_32a[16][16] __attribute__((section(".dtcm"))) = {0};

uint32_t color_artifact_mono_0[16]    __attribute__((section(".dtcm"))) = {0};
uint32_t color_artifact_mono_1[16]    __attribute__((section(".dtcm"))) = {0};


/*------------------------------------------------
 * vdg_init()
 *
 *  Initialize the VDG device
 *
 */
void vdg_init(void)
{
    uint8_t buf[4];
    int buffer_index;

    /* Default startup mode of the MC-10
     */
    current_vdg_mode = ALPHA_INTERNAL;
    reduce_framerate_for_tape = 0;

    // --------------------------------------------------------------------------
    // Pre-render the 2-color modes for fast look-up and 32-bit writes for speed
    // --------------------------------------------------------------------------
    memset(color_translation_32, 0x00, sizeof(color_translation_32));
    memset(color_translation_32a, 0x00, sizeof(color_translation_32a));
    for (int color = 1; color < 16; color++)
    {
        for (int i=0; i<16; i++)
        {
            switch (i)
            {
                case 0x0: color_translation_32[color][i] = (FB_BLACK<<0)       | (FB_BLACK<<8)      | (FB_BLACK<<16)      | (FB_BLACK<<24);         break;
                case 0x1: color_translation_32[color][i] = (FB_BLACK<<0)       | (FB_BLACK<<8)      | (FB_BLACK<<16)      | (colors[color]<<24);    break;
                case 0x2: color_translation_32[color][i] = (FB_BLACK<<0)       | (FB_BLACK<<8)      | (colors[color]<<16) | (FB_BLACK<<24);         break;
                case 0x3: color_translation_32[color][i] = (FB_BLACK<<0)       | (FB_BLACK<<8)      | (colors[color]<<16) | (colors[color]<<24);    break;
                case 0x4: color_translation_32[color][i] = (FB_BLACK<<0)       | (colors[color]<<8) | (FB_BLACK<<16)      | (FB_BLACK<<24);         break;
                case 0x5: color_translation_32[color][i] = (FB_BLACK<<0)       | (colors[color]<<8) | (FB_BLACK<<16)      | (colors[color]<<24);    break;
                case 0x6: color_translation_32[color][i] = (FB_BLACK<<0)       | (colors[color]<<8) | (colors[color]<<16) | (FB_BLACK<<24);         break;
                case 0x7: color_translation_32[color][i] = (FB_BLACK<<0)       | (colors[color]<<8) | (colors[color]<<16) | (colors[color]<<24);    break;

                case 0x8: color_translation_32[color][i] = (colors[color]<<0)  | (FB_BLACK<<8)      | (FB_BLACK<<16)      | (FB_BLACK<<24);         break;
                case 0x9: color_translation_32[color][i] = (colors[color]<<0)  | (FB_BLACK<<8)      | (FB_BLACK<<16)      | (colors[color]<<24);    break;
                case 0xA: color_translation_32[color][i] = (colors[color]<<0)  | (FB_BLACK<<8)      | (colors[color]<<16) | (FB_BLACK<<24);         break;
                case 0xB: color_translation_32[color][i] = (colors[color]<<0)  | (FB_BLACK<<8)      | (colors[color]<<16) | (colors[color]<<24);    break;
                case 0xC: color_translation_32[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (FB_BLACK<<16)      | (FB_BLACK<<24);         break;
                case 0xD: color_translation_32[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (FB_BLACK<<16)      | (colors[color]<<24);    break;
                case 0xE: color_translation_32[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (colors[color]<<16) | (FB_BLACK<<24);         break;
                case 0xF: color_translation_32[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (colors[color]<<16) | (colors[color]<<24);    break;
            }
        }


        for (int i=0; i<16; i++)
        {
            switch (i)
            {
                case 0x0: color_translation_32a[color][i] = (FB_DKORG<<0)       | (FB_DKORG<<8)      | (FB_DKORG<<16)      | (FB_DKORG<<24);         break;
                case 0x1: color_translation_32a[color][i] = (FB_DKORG<<0)       | (FB_DKORG<<8)      | (FB_DKORG<<16)      | (colors[color]<<24);    break;
                case 0x2: color_translation_32a[color][i] = (FB_DKORG<<0)       | (FB_DKORG<<8)      | (colors[color]<<16) | (FB_DKORG<<24);         break;
                case 0x3: color_translation_32a[color][i] = (FB_DKORG<<0)       | (FB_DKORG<<8)      | (colors[color]<<16) | (colors[color]<<24);    break;
                case 0x4: color_translation_32a[color][i] = (FB_DKORG<<0)       | (colors[color]<<8) | (FB_DKORG<<16)      | (FB_DKORG<<24);         break;
                case 0x5: color_translation_32a[color][i] = (FB_DKORG<<0)       | (colors[color]<<8) | (FB_DKORG<<16)      | (colors[color]<<24);    break;
                case 0x6: color_translation_32a[color][i] = (FB_DKORG<<0)       | (colors[color]<<8) | (colors[color]<<16) | (FB_DKORG<<24);         break;
                case 0x7: color_translation_32a[color][i] = (FB_DKORG<<0)       | (colors[color]<<8) | (colors[color]<<16) | (colors[color]<<24);    break;

                case 0x8: color_translation_32a[color][i] = (colors[color]<<0)  | (FB_DKORG<<8)      | (FB_DKORG<<16)      | (FB_DKORG<<24);         break;
                case 0x9: color_translation_32a[color][i] = (colors[color]<<0)  | (FB_DKORG<<8)      | (FB_DKORG<<16)      | (colors[color]<<24);    break;
                case 0xA: color_translation_32a[color][i] = (colors[color]<<0)  | (FB_DKORG<<8)      | (colors[color]<<16) | (FB_DKORG<<24);         break;
                case 0xB: color_translation_32a[color][i] = (colors[color]<<0)  | (FB_DKORG<<8)      | (colors[color]<<16) | (colors[color]<<24);    break;
                case 0xC: color_translation_32a[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (FB_DKORG<<16)      | (FB_DKORG<<24);         break;
                case 0xD: color_translation_32a[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (FB_DKORG<<16)      | (colors[color]<<24);    break;
                case 0xE: color_translation_32a[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (colors[color]<<16) | (FB_DKORG<<24);         break;
                case 0xF: color_translation_32a[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (colors[color]<<16) | (colors[color]<<24);    break;
            }
        }
    }

    // ------------------------------------------------------------------------
    // Pre-render the high-rez modes. The MC-10 doesn't do proper artifacting
    // as it lacks a color burst circuit so we basically render any high-rez
    // PMODE 4 stuff in monochrome. Given the memory limitations of the VDG,
    // almost nothing uses this anyway...
    // ------------------------------------------------------------------------
    for (int pixels_byte=0; pixels_byte<16; pixels_byte++)
    {
        buffer_index = 0;
        for (uint8_t element = 0x08; element != 0; element >>= 1)
        {
            buf[buffer_index++] = (pixels_byte & element) ? FB_BUFF : FB_BLACK;
        }
        color_artifact_mono_0[pixels_byte] = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | (buf[0] << 0);

        buffer_index = 0;
        for (uint8_t element = 0x08; element != 0; element >>= 1)
        {
            buf[buffer_index++] = (pixels_byte & element) ? FB_GREEN : FB_BLACK;
        }
        color_artifact_mono_1[pixels_byte] = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | (buf[0] << 0);
    }
}

/*------------------------------------------------
 * vdg_render()
 *
 *  Render video display.
 *  A full screen rendering is performed at every invocation on the function.
 *
 */
ITCM_CODE void vdg_render(void)
{
    int vdg_mem_base = 0x4000;

    if (tape_motor == 2)
    {
        // When loading tape, the screen refresh is reduced to give more emulation speed
        if (++reduce_framerate_for_tape < 10) return;
        reduce_framerate_for_tape = 0;
    }

    /* VDG mode settings
     */
    current_vdg_mode = vdg_get_mode();

    /* Render screen content to frame buffer
     */
    switch ( current_vdg_mode )
    {
        case ALPHA_INTERNAL:
        case SEMI_GRAPHICS_4:
            vdg_render_alpha_semi4(vdg_mem_base);
            break;

        case SEMI_GRAPHICS_6:
        case ALPHA_EXTERNAL:
            vdg_render_semi6(vdg_mem_base);
            break;

        case GRAPHICS_1C:
        case GRAPHICS_2C:
        case GRAPHICS_3C:
        case GRAPHICS_6C:
            vdg_render_color_graph(current_vdg_mode, vdg_mem_base);
            break;

        case GRAPHICS_1R:
        case GRAPHICS_2R:
        case GRAPHICS_3R:
            vdg_render_resl_graph(current_vdg_mode, vdg_mem_base);
            break;

        case GRAPHICS_6R:
            vdg_render_artifacting_mono(current_vdg_mode, vdg_mem_base);
            break;

        default:
            break;
    }
}

/*------------------------------------------------
 * vdg_render_alpha_semi4()
 *
 *  Render aplphanumeric internal and Semi-graphics 4.
 *
 * param:  VDG memory base address
 * return: None
 *
 */
ITCM_CODE void vdg_render_alpha_semi4(int vdg_mem_base)
{
    int         c, row, col, font_row;
    int         char_index, row_address;
    uint8_t     bit_pattern;
    uint8_t     color_set;
    uint8_t     vdg_control_reg = Memory[0xbfff];

    uint32_t    *screen_buffer = (uint32_t *)0x06000000;

    if ( vdg_control_reg & PIA_COLOR_SET )
        color_set = FB_LTORG;
    else
        color_set = FB_GREEN;

    // If we are running with MCX 'Large Model', the Video memory is in the other bank
    uint8_t *screen_memory = (mcx_ram_bank1) ? (Memory_MCX-0x4000) : Memory;

    for ( row = 0; row < SCREEN_HEIGHT_CHAR; row++ )
    {
        row_address = row * SCREEN_WIDTH_CHAR + vdg_mem_base;

        for ( font_row = 0; font_row < FONT_HEIGHT; font_row++ )
        {
            for ( col = 0; col < SCREEN_WIDTH_CHAR; col++ )
            {
                c = screen_memory[(col + row_address) & 0x4fff];

                /* Mode dependent initialization
                 * for text or semigraphics 4:
                 * - Determine foreground and background colors
                 * - Character pattern array
                 * - Character code index to bit pattern array
                 *
                 */
                if ( (uint8_t)c & CHAR_SEMI_GRAPHICS )
                {
                    uint8_t fg_color = 1+(((uint8_t)c & 0b01110000) >> 4);
                    char_index = (int)(((uint8_t) c) & SEMI_GRAPH4_MASK);
                    bit_pattern = semi_graph_4[char_index][font_row];

                    /* Render a row of pixels directly to the screen buffer - 32-bit speed!
                     */
                    *screen_buffer++ = color_translation_32[fg_color][bit_pattern >> 4];
                    *screen_buffer++ = color_translation_32[fg_color][bit_pattern & 0xF];
                }
                else
                {
                    char_index = (int)(((uint8_t) c) & ~(CHAR_SEMI_GRAPHICS | CHAR_INVERSE));
                    bit_pattern = font_img5x7[char_index][font_row];
                    if ( (uint8_t)c & CHAR_INVERSE )
                    {
                        bit_pattern = ~bit_pattern;
                    }

                    /* Render a row of pixels directly to the screen buffer - 32-bit speed!
                     */
                     if ( vdg_control_reg & PIA_COLOR_SET )
                     {
                        *screen_buffer++ = color_translation_32a[color_set][bit_pattern >> 4];
                        *screen_buffer++ = color_translation_32a[color_set][bit_pattern & 0xF];
                     }
                     else
                     {
                        *screen_buffer++ = color_translation_32[color_set][bit_pattern >> 4];
                        *screen_buffer++ = color_translation_32[color_set][bit_pattern & 0xF];
                     }
                }
            }
        }
    }
}

/*------------------------------------------------
 * vdg_render_semi6()
 *
 *  Render Semi-graphics 6.
 *
 * param:  VDG memory base address
 * return: None
 *
 */
ITCM_CODE void vdg_render_semi6(int vdg_mem_base)
{
    int         c, row, col, font_row, font_col, color_set;
    int         char_index, row_address;
    uint8_t     bit_pattern, pix_pos;
    uint8_t     fg_color, bg_color;

    uint32_t    *screen_buffer;

    screen_buffer = (uint32_t *)0x06000000;

    // If we are running with MCX 'Large Model', the Video memory is in the other bank
    uint8_t *screen_memory = (mcx_ram_bank1) ? (Memory_MCX-0x4000) : Memory;

    if ( Memory[0xbfff] & PIA_COLOR_SET )
        color_set = DEF_COLOR_CSS_1;
    else
        color_set = DEF_COLOR_CSS_0;

    for ( row = 0; row < SCREEN_HEIGHT_CHAR; row++ )
    {
        row_address = row * SCREEN_WIDTH_CHAR + vdg_mem_base;

        for ( font_row = 0; font_row < FONT_HEIGHT; font_row++ )
        {
            for ( col = 0; col < SCREEN_WIDTH_CHAR; col++ )
            {
                c = screen_memory[(col + row_address) & 0x4fff];

                if (c & 0x80)
                {
                    bg_color = FB_BLACK;
                    fg_color = colors[(int)(((c & 0b11000000) >> 6) + color_set)];

                    char_index = (int)(((uint8_t) c) & SEMI_GRAPH6_MASK);
                    bit_pattern = semi_graph_6[char_index][font_row];
                }
                else // Due to wiring in the MC-10, if the high bit isn't set, we render the actual value as the bit pattern in text-like colors
                {
                    bg_color = FB_DKGRN + ((c & 0x40) >> 5);
                    fg_color = FB_LTGRN + ((c & 0x40) >> 5);
                    bit_pattern = (c & 0x7F);
                }

                /* Render a row of pixels in a temporary buffer
                 */
                pix_pos = 0x80;

                uint8_t buf[8];
                for ( font_col = 0; font_col < FONT_WIDTH; font_col++ )
                {
                    /* Bit is set in Font, print pixel(s) in text color
                    */
                    if ( (bit_pattern & pix_pos) )
                    {
                        buf[font_col] = fg_color;
                    }
                    /* Bit is cleared in Font
                    */
                    else
                    {
                        buf[font_col] = bg_color;
                    }

                    /* Move to the next pixel position
                    */
                    pix_pos = pix_pos >> 1;
                }

                uint32_t *ptr32 = (uint32_t *)&buf;
                *screen_buffer++ = *ptr32++;
                *screen_buffer++ = *ptr32++;
            }
        }
    }
}

/*------------------------------------------------
 * vdg_render_resl_graph()
 *
 *  Render high resolution graphics modes:
 *  GRAPHICS_1R, GRAPHICS_2R, GRAPHICS_3R.
 *
 * param:  Mode, base address of video memory buffer.
 * return: none
 *
 */
ITCM_CODE void vdg_render_resl_graph(video_mode_t mode, int vdg_mem_base)
{
    int         i, vdg_mem_offset, element, buffer_index;
    int         video_mem, row_rep;
    uint8_t     pixels_byte, fg_color, pixel;
    uint8_t    *screen_buffer;
    uint8_t     pixel_row[SCREEN_WIDTH_PIX+16];

    screen_buffer = (uint8_t *) (0x06000000);

    // If we are running with MCX 'Large Model', the Video memory is in the other bank
    uint8_t *screen_memory = (mcx_ram_bank1) ? (Memory_MCX-0x4000) : Memory;

    video_mem = resolution[mode][RES_MEM];
    row_rep = resolution[mode][RES_ROW_REP];
    buffer_index = 0;

    if ( Memory[0xbfff] & PIA_COLOR_SET )
    {
        fg_color = colors[DEF_COLOR_CSS_1];
    }
    else
    {
        fg_color = colors[DEF_COLOR_CSS_0];
    }

    for ( vdg_mem_offset = 0; vdg_mem_offset < video_mem; vdg_mem_offset++)
    {
        pixels_byte = screen_memory[(vdg_mem_offset + vdg_mem_base) & 0x4fff];

        if (pixels_byte == 0x00)
        {
            memset(pixel_row+buffer_index, FB_BLACK, 16);
            buffer_index += 16;
        }
        else if (pixels_byte == 0xFF)
        {
            memset(pixel_row+buffer_index, fg_color, 16);
            buffer_index += 16;
        }
        else
        for ( element = 0x80; element != 0; element = element >> 1)
        {
            if ( pixels_byte & element )
            {
                pixel = fg_color;
            }
            else
            {
                pixel = FB_BLACK;
            }

            // Expand 2x
            pixel_row[buffer_index++] = pixel;
            pixel_row[buffer_index++] = pixel;
        }

        if ( buffer_index >= SCREEN_WIDTH_PIX )
        {
            for ( i = 0; i < row_rep; i++ )
            {
                memcpy(screen_buffer, pixel_row, SCREEN_WIDTH_PIX);
                screen_buffer += SCREEN_WIDTH_PIX;
            }

            buffer_index = 0;
        }
    }
}


/*------------------------------------------------
 * vdg_render_color_graph()
 *
 *  Render color graphics modes:
 *  GRAPHICS_1C, GRAPHICS_2C, GRAPHICS_3C, and GRAPHICS_6C.
 *
 * param:  Mode, base address of video memory buffer.
 * return: none
 *
 */
ITCM_CODE void vdg_render_color_graph(video_mode_t mode, int vdg_mem_base)
{
    int         i, vdg_mem_offset;
    int         video_mem, row_rep;
    uint8_t     color_set;
    uint8_t     pixels_byte;
    uint8_t    *screen_buffer;
    uint8_t     pixel_row[SCREEN_WIDTH_PIX+16];

    screen_buffer = (uint8_t *) (0x06000000);

    // If we are running with MCX 'Large Model', the Video memory is in the other bank
    uint8_t *screen_memory = (mcx_ram_bank1) ? (Memory_MCX-0x4000) : Memory;

    video_mem = resolution[mode][RES_MEM];
    row_rep = resolution[mode][RES_ROW_REP];

    if ( Memory[0xbfff] & PIA_COLOR_SET )
        color_set = 4;
    else
        color_set = 0;

    if ( mode == GRAPHICS_1C )
    {
        uint16_t *pixRowPtr = (uint16_t *)pixel_row;
        for ( vdg_mem_offset = 0; vdg_mem_offset < video_mem; vdg_mem_offset++)
        {
            pixels_byte = screen_memory[(vdg_mem_offset + vdg_mem_base) & 0x4fff];

            *pixRowPtr++ = colors16[((pixels_byte >> 6) & 0x03) | color_set];
            *pixRowPtr++ = colors16[((pixels_byte >> 6) & 0x03) | color_set];

            *pixRowPtr++ = colors16[((pixels_byte >> 4) & 0x03) | color_set];
            *pixRowPtr++ = colors16[((pixels_byte >> 4) & 0x03) | color_set];

            *pixRowPtr++ = colors16[((pixels_byte >> 2) & 0x03) | color_set];
            *pixRowPtr++ = colors16[((pixels_byte >> 2) & 0x03) | color_set];

            *pixRowPtr++ = colors16[((pixels_byte)      & 0x03) | color_set];
            *pixRowPtr++ = colors16[((pixels_byte)      & 0x03) | color_set];

            if ( pixRowPtr >= (uint16_t *)(pixel_row+SCREEN_WIDTH_PIX) )
            {
                for ( i = 0; i < row_rep; i++ )
                {
                    memcpy(screen_buffer, pixel_row, SCREEN_WIDTH_PIX);
                    screen_buffer += SCREEN_WIDTH_PIX;
                }

                pixRowPtr = (uint16_t *)pixel_row;
            }
        }
    }
    else // Graphics 2C, 3C and 6C
    {
        uint16_t *pixRowPtr = (uint16_t *)pixel_row;
        for ( vdg_mem_offset = 0; vdg_mem_offset < video_mem; vdg_mem_offset++)
        {
            pixels_byte = screen_memory[(vdg_mem_offset + vdg_mem_base) & 0x4fff];

            *pixRowPtr++ = colors16[((pixels_byte >> 6) & 0x03) | color_set];
            *pixRowPtr++ = colors16[((pixels_byte >> 4) & 0x03) | color_set];
            *pixRowPtr++ = colors16[((pixels_byte >> 2) & 0x03) | color_set];
            *pixRowPtr++ = colors16[((pixels_byte     ) & 0x03) | color_set];

            if ( pixRowPtr >= (uint16_t *)(pixel_row+SCREEN_WIDTH_PIX) )
            {
                for ( i = 0; i < row_rep; i++ )
                {
                    memcpy(screen_buffer, pixel_row, SCREEN_WIDTH_PIX);
                    screen_buffer += SCREEN_WIDTH_PIX;
                }

                pixRowPtr = (uint16_t *)pixel_row;
            }
        }
    }
}


// ------------------------------------------------------------------------
// Handler for GRAPHICS_6R - The MC-10 does not have proper color-burst
// signal like the Coco 2 and so the artifacting doesn't really happen.
// Therefore, we just output this as pure mono (White/Black or Green/Black)
// No need to get fancy, virtually nothing on the MC-10 uses this anyway.
// ------------------------------------------------------------------------
ITCM_CODE void vdg_render_artifacting_mono(video_mode_t mode, int vdg_mem_base)
{
    int         vdg_mem_offset;
    int         video_mem;
    uint8_t     pixels_byte, fg_color;
    uint32_t   *screen_buffer;
    uint8_t     pix_char = 0;

    // If we are running with MCX 'Large Model', the Video memory is in the other bank
    uint8_t *screen_memory = (mcx_ram_bank1) ? (Memory_MCX-0x4000) : Memory;

    if ( Memory[0xbfff] & PIA_COLOR_SET )
    {
        fg_color = colors[DEF_COLOR_CSS_1];
    }
    else
    {
        fg_color = colors[DEF_COLOR_CSS_0];
    }

    screen_buffer = (uint32_t *) (0x06000000);

    video_mem = resolution[mode][RES_MEM];
    uint8_t bDoubleRez = ((resolution[mode][RES_ROW_REP]) > 1) ? 1:0;

    for ( vdg_mem_offset = 0; vdg_mem_offset < video_mem; vdg_mem_offset++)
    {
        pixels_byte = screen_memory[vdg_mem_offset + vdg_mem_base];

        if (fg_color == FB_GREEN)
        {
            *screen_buffer++ = color_artifact_mono_1[(pixels_byte>>4) & 0x0F];
            *screen_buffer++ = color_artifact_mono_1[pixels_byte & 0x0F];
        }
        else
        {
            *screen_buffer++ = color_artifact_mono_0[(pixels_byte>>4) & 0x0F];
            *screen_buffer++ = color_artifact_mono_0[pixels_byte & 0x0F];
        }

        // Check if full line rendered... 32 chars (256 pixels)
        if (++pix_char & 0x20)
        {
            pix_char = 0;
            if (bDoubleRez)
            {
                memcpy(screen_buffer, screen_buffer-64, SCREEN_WIDTH_PIX);
                screen_buffer += 64;
            }
        }
    }
}

/*------------------------------------------------
 * vdg_get_mode()
 *
 * Parse the MC-10 VDG control register
 *
 * param:  None
 * return: Video mode
 *
 */
video_mode_t vdg_get_mode(void)
{
    video_mode_t mode = UNDEFINED;

    uint8_t control_reg = Memory[0xbfff];

    if (control_reg & 0x20) // Graphics Mode (vs Alphanumeric)
    {
        uint8_t graph_mode = 0x00;
        if (control_reg & 0x10) graph_mode |= 0x01;
        if (control_reg & 0x08) graph_mode |= 0x02;
        if (control_reg & 0x04) graph_mode |= 0x04;
        switch (graph_mode)
        {
            case 0x00:
                mode = GRAPHICS_1C;
                break;
            case 0x01:
                mode = GRAPHICS_1R;
                break;
            case 0x02:
                mode = GRAPHICS_2C;
                break;
            case 0x03:
                mode = GRAPHICS_2R;
                break;
            case 0x04:
                mode = GRAPHICS_3C;
                break;
            case 0x05:
                mode = GRAPHICS_3R;
                break;
            case 0x06:
                mode = GRAPHICS_6C;
                break;
            case 0x07:
                mode = GRAPHICS_6R;
                break;
        }
    }
    else // Text/Alpha Mode...
    {
        if (control_reg & 0x04)
        {
            mode = SEMI_GRAPHICS_6;
        }
        else
        {
            mode = ALPHA_INTERNAL;
            // Character bit.7 selects SEMI_GRAPHICS_4;
        }
    }

    return mode;
}

// End of file
