#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <dev/console.h>
#include <dev/dev.h>
#include <lib/lock.h>
#include <lib/alloc.h>
#include <lib/bitmap.h>
#include <lib/resource.h>
#include <lib/errno.h>
#include <lib/termios.h>
#include <lib/print.h>
#include <lib/ioctl.h>
#include <sched/sched.h>
#include <sys/idt.h>
#include <sys/port_io.h>
#include <sys/apic.h>

// Tries to implement this standard for terminfo
// http://man7.org/linux/man-pages/man4/console_codes.4.html

#define MAX_TTYS 6
#define KBD_BUF_SIZE 2048
#define BIG_BUF_SIZE 65536
#define MAX_ESC_VALUES 256

struct tty {
    struct resource;

    int number;
    lock_t write_lock;
    lock_t read_lock;
    int cursor_x;
    int cursor_y;
    int cursor_status;
    uint32_t cursor_bg_col;
    uint32_t cursor_fg_col;
    uint32_t text_bg_col;
    uint32_t text_fg_col;
    uint32_t default_bg_col;
    uint32_t default_fg_col;
    char *grid;
    uint32_t *gridbg;
    uint32_t *gridfg;
    int control_sequence;
    int saved_cursor_x;
    int saved_cursor_y;
    int scrolling_region_top;
    int scrolling_region_bottom;
    int escape;
    int esc_values[MAX_ESC_VALUES];
    int esc_values_i;
    int rrr;
    int tabsize;
    struct event *kbd_event;
    lock_t kbd_lock;
    size_t kbd_buf_i;
    char kbd_buf[KBD_BUF_SIZE];
    size_t big_buf_i;
    char big_buf[BIG_BUF_SIZE];
    struct termios termios;
    int tcioff;
    int tcooff;
    int dec_private_mode;
    int decckm;
};

static uint32_t ansi_colours[] = {
    0x00000000,              // black
    0x00aa0000,              // red
    0x0000aa00,              // green
    0x00aa5500,              // brown
    0x000000aa,              // blue
    0x00aa00aa,              // magenta
    0x0000aaaa,              // cyan
    0x00aaaaaa               // grey
};

static struct tty *ttys[MAX_TTYS];
static struct tty *current_tty;

static int rows;
static int cols;

static uint32_t *fb;
static int fb_height;
static int fb_width;
static int fb_pitch;

static uint8_t *font;
static int font_height;
static int font_width;

static void plot_px(int x, int y, uint32_t hex) {
    size_t fb_i = x + (fb_pitch / sizeof(uint32_t)) * y;

    fb[fb_i] = hex;
}

static void plot_char(char c, int x, int y, uint32_t hex_fg, uint32_t hex_bg) {
    int orig_x = x;
    uint8_t *glyph = &font[c * font_height];

    for (int i = 0; i < font_height; i++) {
        for (int j = font_width - 1; j >= 0; j--)
            plot_px(x++, y, bit_test(glyph[i], j) ? hex_fg : hex_bg);
        y++;
        x = orig_x;
    }
}

static void plot_char_grid(struct tty *tty, char c, int x, int y, uint32_t hex_fg, uint32_t hex_bg) {
    if (tty->grid[x + y * cols] != c
     || tty->gridfg[x + y * cols] != hex_fg
     || tty->gridbg[x + y * cols] != hex_bg) {
        if (tty == current_tty)
            plot_char(c, x * font_width, y * font_height, hex_fg, hex_bg);
        tty->grid[x + y * cols] = c;
        tty->gridfg[x + y * cols] = hex_fg;
        tty->gridbg[x + y * cols] = hex_bg;
    }
}

static void clear_cursor(struct tty *tty) {
    if (tty->cursor_status) {
        if (tty == current_tty) {
            plot_char(tty->grid[tty->cursor_x + tty->cursor_y * cols],
                tty->cursor_x * font_width, tty->cursor_y * font_height,
                tty->gridfg[tty->cursor_x + tty->cursor_y * cols],
                tty->gridbg[tty->cursor_x + tty->cursor_y * cols]);
        }
    }
}

static void draw_cursor(struct tty *tty) {
    if (tty->cursor_status) {
        if (tty == current_tty) {
            plot_char(tty->grid[tty->cursor_x + tty->cursor_y * cols],
                tty->cursor_x * font_width, tty->cursor_y * font_height,
                tty->cursor_fg_col, tty->cursor_bg_col);
        }
    }
}

static void refresh(struct tty *tty) {
    if (tty == current_tty) {
        for (int i = 0; i < rows * cols; i++)
            plot_char(tty->grid[i],
                (i % cols) * font_width,
                (i / cols) * font_height,
                tty->gridfg[i],
                tty->gridbg[i]);
        draw_cursor(tty);
    }
}

static void scroll(struct tty *tty) {
    clear_cursor(tty);

    for (int i = cols; i < rows * cols; i++) {
        plot_char_grid(tty,
            tty->grid[i],
            (i - cols) % cols,
            (i - cols) / cols,
            tty->gridfg[i],
            tty->gridbg[i]);
    }
    /* clear the last line of the screen */
    for (int i = rows * cols - cols; i < rows * cols; i++) {
        plot_char_grid(tty,
            ' ',
            i % cols,
            i / cols,
            tty->text_fg_col,
            tty->text_bg_col);
    }

    draw_cursor(tty);
}

static void clear(struct tty *tty) {
    clear_cursor(tty);

    for (int i = 0; i < rows * cols; i++) {
        plot_char_grid(tty,
            ' ',
            i % cols,
            i / cols,
            tty->text_fg_col,
            tty->text_bg_col);
    }

    tty->cursor_x = 0;
    tty->cursor_y = 0;

    draw_cursor(tty);
}

static void enable_cursor(struct tty *tty) {
    tty->cursor_status = 1;
    draw_cursor(tty);
}

static void disable_cursor(struct tty *tty) {
    clear_cursor(tty);
    tty->cursor_status = 0;
}

static void set_cursor_pos(struct tty *tty, int x, int y) {
    clear_cursor(tty);
    tty->cursor_x = x;
    tty->cursor_y = y;
    draw_cursor(tty);
}

static void sgr(struct tty *tty) {
    int i = 0;

    if (!tty->esc_values_i)
        goto def;

    for (; i < tty->esc_values_i; i++) {
        if (!tty->esc_values[i]) {
def:
            tty->text_fg_col = tty->default_fg_col;
            tty->text_bg_col = tty->default_bg_col;
            continue;
        }

        if (tty->esc_values[i] >= 30 && tty->esc_values[i] <= 37) {
            tty->text_fg_col = ansi_colours[tty->esc_values[i] - 30];
            continue;
        }

        if (tty->esc_values[i] >= 40 && tty->esc_values[i] <= 47) {
            tty->text_bg_col = ansi_colours[tty->esc_values[i] - 40];
            continue;
        }
    }
}

static void control_sequence_parse(struct tty *tty, char c) {
    if (c >= '0' && c <= '9') {
        tty->rrr = 1;
        tty->esc_values[tty->esc_values_i] *= 10;
        tty->esc_values[tty->esc_values_i] += c - '0';
        return;
    } else {
        if (tty->rrr) {
            tty->esc_values_i++;
            tty->rrr = 0;
            if (c == ';')
                return;
        } else if (c == ';') {
            tty->esc_values[tty->esc_values_i] = 1;
            tty->esc_values_i++;
            return;
        }
    }

    // default rest to 1
    for (int i = tty->esc_values_i; i < MAX_ESC_VALUES; i++)
        tty->esc_values[i] = 1;

    switch (c) {
        case '@':
            // TODO
            break;
        case 'A':
            if (tty->esc_values[0] > tty->cursor_y)
                tty->esc_values[0] = tty->cursor_y;
            set_cursor_pos(tty, tty->cursor_x, tty->cursor_y - tty->esc_values[0]);
            break;
        case 'B':
            if ((tty->cursor_y + tty->esc_values[0]) > (rows - 1))
                tty->esc_values[0] = (rows - 1) - tty->cursor_y;
            set_cursor_pos(tty, tty->cursor_x, tty->cursor_y + tty->esc_values[0]);
            break;
        case 'C':
            if ((tty->cursor_x + tty->esc_values[0]) > (cols - 1))
                tty->esc_values[0] = (cols - 1) - tty->cursor_x;
            set_cursor_pos(tty, tty->cursor_x + tty->esc_values[0], tty->cursor_y);
            break;
        case 'D':
            if (tty->esc_values[0] > tty->cursor_x)
                tty->esc_values[0] = tty->cursor_x;
            set_cursor_pos(tty, tty->cursor_x - tty->esc_values[0], tty->cursor_y);
            break;
        case 'E':
            if (tty->cursor_y + tty->esc_values[0] >= rows)
                set_cursor_pos(tty, 0, rows - 1);
            else
                set_cursor_pos(tty, 0, tty->cursor_y + tty->esc_values[0]);
            break;
        case 'F':
            if (tty->cursor_y - tty->esc_values[0] < 0)
                set_cursor_pos(tty, 0, 0);
            else
                set_cursor_pos(tty, 0, tty->cursor_y - tty->esc_values[0]);
            break;
        case 'd':
            if (tty->esc_values[0] >= rows)
                break;
            clear_cursor(tty);
            tty->cursor_y = tty->esc_values[0];
            draw_cursor(tty);
            break;
        case 'G':
        case '`':
            if (tty->esc_values[0] >= cols)
                break;
            clear_cursor(tty);
            tty->cursor_x = tty->esc_values[0];
            draw_cursor(tty);
            break;
        case 'H':
        case 'f':
            tty->esc_values[0] -= 1;
            tty->esc_values[1] -= 1;
            if (tty->esc_values[1] >= cols)
                tty->esc_values[1] = cols - 1;
            if (tty->esc_values[0] >= rows)
                tty->esc_values[0] = rows - 1;
            set_cursor_pos(tty, tty->esc_values[1], tty->esc_values[0]);
            break;
        case 'J':
            switch (tty->esc_values[0]) {
                case 1: {
                    int cursor_abs = tty->cursor_y * cols + tty->cursor_x;
                    clear_cursor(tty);

                    for (int i = 0; i < cursor_abs; i++) {
                        plot_char_grid(tty,
                            ' ',
                            i % cols,
                            i / cols,
                            tty->text_fg_col,
                            tty->text_bg_col);
                    }

                    draw_cursor(tty);
                    }
                    break;
                case 2:
                    clear(tty);
                    break;
                default:
                    break;
            }
            break;
        case 'm':
            sgr(tty);
            break;
        case 'r':
            tty->scrolling_region_top = tty->esc_values[0];
            tty->scrolling_region_bottom = tty->esc_values[1];
            break;
        case 's':
            tty->saved_cursor_x = tty->cursor_x;
            tty->saved_cursor_y = tty->cursor_y;
            break;
        case 'u':
            clear_cursor(tty);
            tty->cursor_x = tty->saved_cursor_x;
            tty->cursor_y = tty->saved_cursor_y;
            draw_cursor(tty);
            break;
        case 'h':
        case 'l':
            if (tty->dec_private_mode) {
                tty->dec_private_mode = 0;
                switch (tty->esc_values[1]) {
                    case 1:
                        tty->decckm = (c == 'h');
                        break;
                    default:
                        break;
                }
            }
            break;
        case '?':
            tty->dec_private_mode = 1;
            return;
        default:
            break;
    }

    tty->control_sequence = 0;
    tty->escape = 0;
}

static void escape_parse(struct tty *tty, char c) {
    if (tty->control_sequence) {
        control_sequence_parse(tty, c);
        return;
    }
    switch (c) {
        case '[':
            for (int i = 0; i < MAX_ESC_VALUES; i++)
                tty->esc_values[i] = 0;
            tty->esc_values_i = 0;
            tty->rrr = 0;
            tty->control_sequence = 1;
            break;
        default:
            tty->escape = 0;
            break;
    }
}

static void put_char(struct tty *tty, char c) {
    if (tty->escape) {
        escape_parse(tty, c);
        return;
    }
    switch (c) {
        case '\0':
            break;
        case '\e':
            tty->escape = 1;
            break;
        case '\t':
            if ((tty->cursor_x / tty->tabsize + 1) * tty->tabsize >= cols)
                break;
            set_cursor_pos(tty, (tty->cursor_x / tty->tabsize + 1) * tty->tabsize, tty->cursor_y);
            break;
        case '\r':
            set_cursor_pos(tty, 0, tty->cursor_y);
            break;
        case '\a':
            // dummy handler for bell
            break;
        case '\n':
            if (tty->cursor_y == (rows - 1)) {
                set_cursor_pos(tty, 0, (rows - 1));
                scroll(tty);
            } else {
                set_cursor_pos(tty, 0, (tty->cursor_y + 1));
            }
            break;
        case '\b':
            if (tty->cursor_x || tty->cursor_y) {
                clear_cursor(tty);
                if (tty->cursor_x) {
                    tty->cursor_x--;
                } else {
                    tty->cursor_y--;
                    tty->cursor_x = cols - 1;
                }
                draw_cursor(tty);
            }
            break;
        default:
            clear_cursor(tty);
            plot_char_grid(tty, c, tty->cursor_x++, tty->cursor_y, tty->text_fg_col, tty->text_bg_col);
            if (tty->cursor_x == cols) {
                tty->cursor_x = 0;
                tty->cursor_y++;
            }
            if (tty->cursor_y == rows) {
                tty->cursor_y--;
                scroll(tty);
            }
            draw_cursor(tty);
            break;
    }
}

static int tty_ioctl(struct resource *this, int request, void *argp) {
    switch (request) {
        case TIOCGWINSZ: {
            struct winsize *w = argp;
            w->ws_row = rows;
            w->ws_col = cols;
            w->ws_xpixel = fb_width;
            w->ws_ypixel = fb_height;
            return 0;
        }
    }
    errno = EINVAL;
    return -1;
}

static ssize_t tty_write(struct resource *this, const void *void_buf, off_t loc, size_t count) {
    struct tty *tty = (void *)this;

    if (tty->tcooff) {
        errno = EINVAL;
        return -1;
    }

    const char *buf = void_buf;
    SPINLOCK_ACQUIRE(tty->write_lock);
    for (size_t i = 0; i < count; i++)
        put_char(tty, buf[i]);
    LOCK_RELEASE(tty->write_lock);
    return count;
}

#define MAX_CODE 0x57
#define CAPSLOCK 0x3a
#define LEFT_ALT 0x38
#define LEFT_ALT_REL 0xb8
#define RIGHT_SHIFT 0x36
#define LEFT_SHIFT 0x2a
#define RIGHT_SHIFT_REL 0xb6
#define LEFT_SHIFT_REL 0xaa
#define CTRL 0x1d
#define CTRL_REL 0x9d

static int capslock_active = 0;
static int shift_active = 0;
static int ctrl_active = 0;
static int alt_active = 0;
static int extra_scancodes = 0;

static const uint8_t ascii_capslock[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', '\0', '\\', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', ',', '.', '/', '\0', '\0', '\0', ' '
};

static const uint8_t ascii_shift[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const uint8_t ascii_shift_capslock[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~', '\0', '|', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const uint8_t ascii_nomod[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};

static bool is_printable(char c) {
    return (c >= 0x20 && c <= 0x7e);
}

static void add_to_buf_char(struct tty *tty, char c) {
    SPINLOCK_ACQUIRE(tty->read_lock);

    if (tty->termios.c_lflag & ICANON) {
        switch (c) {
            case '\n':
                if (tty->kbd_buf_i == KBD_BUF_SIZE)
                    goto out;
                tty->kbd_buf[tty->kbd_buf_i++] = c;
                if (tty->termios.c_lflag & ECHO)
                    put_char(tty, c);
                for (size_t i = 0; i < tty->kbd_buf_i; i++) {
                    if (tty->big_buf_i == BIG_BUF_SIZE)
                        goto out;
                    tty->big_buf[tty->big_buf_i++] = tty->kbd_buf[i];
                }
                tty->kbd_buf_i = 0;
                goto out;
            case '\b':
                if (!tty->kbd_buf_i)
                    goto out;
                tty->kbd_buf[--tty->kbd_buf_i] = 0;
                if (tty->termios.c_lflag & ECHO) {
                    put_char(tty, '\b');
                    put_char(tty, ' ');
                    put_char(tty, '\b');
                }
                goto out;
        }
    }

    if (tty->termios.c_lflag & ICANON) {
        if (tty->kbd_buf_i == KBD_BUF_SIZE)
            goto out;
        tty->kbd_buf[tty->kbd_buf_i++] = c;
    } else {
        if (tty->big_buf_i == BIG_BUF_SIZE)
            goto out;
        tty->big_buf[tty->big_buf_i++] = c;
    }

    if (is_printable(c) && tty->termios.c_lflag & ECHO)
        put_char(tty, c);

out:
    LOCK_RELEASE(tty->read_lock);
    return;
}

static void add_to_buf(struct tty *tty, const char *s, size_t cnt) {
    for (size_t i = 0; i < cnt; i++) {
        if (tty->termios.c_lflag & ISIG) {
            // accept signal characters
/*
            if (s[i] == ttys[tty].termios.c_cc[VINTR]) {
                // sigint
                // XXX fix hardcoded PID number
                kill(7, SIGINT);
                continue;
            }
*/
        }
        add_to_buf_char(tty, s[i]);
    }
    event_trigger(tty->kbd_event);
}

static void keyboard_handler(void *p) {
    (void)p;
    int ret;
    int vect = idt_get_empty_int_vector();

    print("console: PS/2 keyboard vector is %x\n", vect);

    io_apic_set_irq_redirect(0, vect, 1, true);

await:
    events_await((struct event *[]){int_event[vect]}, &ret, 1);
    uint8_t input_byte = inb(0x60);

    char c = '\0';

    if (input_byte == 0xe0) {
        extra_scancodes = 1;
        goto out;
    }

    if (extra_scancodes) {
        extra_scancodes = 0;

        // extra scancodes
        switch (input_byte) {
            case CTRL:
                ctrl_active = 1;
                goto out;
            case CTRL_REL:
                ctrl_active = 0;
                goto out;
            default:
                break;
        }

        // figure out correct escape sequence
        int decckm = current_tty->decckm;

        // extra scancodes
        switch (input_byte) {
            case 0x47:
                // home
                add_to_buf(current_tty, decckm ? "\e[H" : "\eOH", 3);
                goto out;
            case 0x4f:
                // end
                add_to_buf(current_tty, decckm ? "\e[F" : "\eOF", 3);
                goto out;
            case 0x48:
                // cursor up
                add_to_buf(current_tty, decckm ? "\e[A" : "\eOA", 3);
                goto out;
            case 0x4B:
                // cursor left
                add_to_buf(current_tty, decckm ? "\e[D" : "\eOD", 3);
                goto out;
            case 0x50:
                // cursor down
                add_to_buf(current_tty, decckm ? "\e[B" : "\eOB", 3);
                goto out;
            case 0x4D:
                // cursor right
                add_to_buf(current_tty, decckm ? "\e[C" : "\eOC", 3);
                goto out;
            case 0x49:
                // pgup
                add_to_buf(current_tty, decckm ? "\e[5~" : "\eO5~", 4);
                goto out;
            case 0x51:
                // pgdown
                add_to_buf(current_tty, decckm ? "\e[6~" : "\eO6~", 4);
                goto out;
            case 0x53:
                // delete
                add_to_buf(current_tty, decckm ? "\e[3~" : "\eO3~", 4);
                goto out;
            default:
                break;
        }
    }

    switch (input_byte) {
        case LEFT_ALT:
            alt_active = 1;
            goto out;
        case LEFT_ALT_REL:
            alt_active = 0;
            goto out;
        case LEFT_SHIFT:
        case RIGHT_SHIFT:
            shift_active = 1;
            goto out;
        case LEFT_SHIFT_REL:
        case RIGHT_SHIFT_REL:
            shift_active = 0;
            goto out;
        case CTRL:
            ctrl_active = 1;
            goto out;
        case CTRL_REL:
            ctrl_active = 0;
            goto out;
        case CAPSLOCK:
            capslock_active = !capslock_active;
            goto out;
        default:
            break;
    }

    if (ctrl_active && alt_active) {
        // ctrl-alt combos
        if (input_byte >= 0x3b && input_byte <= 0x40) {
            // ctrl-alt [f1-f6]
            current_tty = ttys[input_byte - 0x3b];
            refresh(current_tty);
            goto out;
        }
    }

    if (ctrl_active) {
        switch (input_byte) {
            default:
                break;
        }
    }

    /* Assign the correct character for this scancode based on modifiers */
    if (input_byte < MAX_CODE) {
        if (ctrl_active)
            // TODO: Proper caret notation would be nice
            c = ascii_capslock[input_byte] - ('?' + 1);
        else if (!capslock_active && !shift_active)
            c = ascii_nomod[input_byte];
        else if (!capslock_active && shift_active)
            c = ascii_shift[input_byte];
        else if (capslock_active && shift_active)
            c = ascii_shift_capslock[input_byte];
        else
            c = ascii_capslock[input_byte];
    } else {
        goto out;
    }

    add_to_buf(current_tty, &c, 1);

out:
    goto await;
}

static ssize_t tty_read(struct resource *this, void *void_buf, off_t loc, size_t count) {
    (void)loc;
    int ret;
    struct tty *tty = (void*)this;

    if (tty->tcioff) {
        errno = EINVAL;
        return -1;
    }

    char *buf = void_buf;
    int wait = 1;

    while (!LOCK_ACQUIRE(tty->read_lock)) {
        if (!events_await((struct event *[]){tty->kbd_event}, &ret, 1)) {
            // signal is aborting us, bail
            errno = EINTR;
            return -1;
        }
    }

    for (size_t i = 0; i < count; ) {
        if (tty->big_buf_i) {
            buf[i++] = tty->big_buf[0];
            tty->big_buf_i--;
            for (size_t j = 0; j < tty->big_buf_i; j++) {
                tty->big_buf[j] = tty->big_buf[j+1];
            }
            wait = 0;
        } else {
            if (wait) {
                LOCK_RELEASE(tty->read_lock);
                do {
                    if (!events_await((struct event *[]){tty->kbd_event}, &ret, 1)) {
                        // signal is aborting us, bail
                        errno = EINTR;
                        return -1;
                    }
                } while (!LOCK_ACQUIRE(tty->read_lock));
            } else {
                LOCK_RELEASE(tty->read_lock);
                return i;
            }
        }
    }

    LOCK_RELEASE(tty->read_lock);
    return count;
}

bool console_init(uint32_t *_fb,
                  int _fb_width,
                  int _fb_height,
                  int _fb_pitch,
                  uint8_t *_font,
                  int _font_width,
                  int _font_height) {
    fb = _fb;
    fb_height = _fb_height;
    fb_width = _fb_width;
    fb_pitch = _fb_pitch;
    font = _font;
    font_height = _font_height;
    font_width = _font_width;

    cols = fb_width / font_width;
    rows = fb_height / font_height;

    for (int i = 0; i < MAX_TTYS; i++) {
        struct tty *tty = resource_create(sizeof(struct tty));

        tty->number = i;
        tty->write_lock = (lock_t){0};
        tty->read_lock = (lock_t){0};
        tty->cursor_x = 0;
        tty->cursor_y = 0;
        tty->cursor_status = 1;
        tty->cursor_bg_col = 0x00aaaaaa;
        tty->cursor_fg_col = 0x00000000;
        tty->default_bg_col = 0x00000000;
        tty->default_fg_col = 0x00aaaaaa;
        tty->text_bg_col = tty->default_bg_col;
        tty->text_fg_col = tty->default_fg_col;
        tty->control_sequence = 0;
        tty->escape = 0;
        tty->tabsize = 8;
        tty->kbd_event = event_create(16);
        tty->kbd_lock = (lock_t){0};
        tty->kbd_buf_i = 0;
        tty->big_buf_i = 0;
        tty->termios.c_lflag = (ISIG | ICANON | ECHO);
        tty->termios.c_cc[VINTR] = 0x03;
        tty->tcooff = 0;
        tty->tcioff = 0;
        tty->dec_private_mode = 0;
        tty->decckm = 0;
        tty->grid = alloc(rows * cols);
        tty->gridbg = alloc(rows * cols * sizeof(uint32_t));
        tty->gridfg = alloc(rows * cols * sizeof(uint32_t));
        for (size_t j = 0; j < (size_t)(rows * cols); j++) {
            tty->grid[j] = ' ';
            tty->gridbg[j] = tty->text_bg_col;
            tty->gridfg[j] = tty->text_fg_col;
        }
        refresh(tty);

        tty->read  = tty_read;
        tty->write = tty_write;
        tty->ioctl = tty_ioctl;

        ttys[i] = tty;

        char tty_name[8];
        snprint(tty_name, 8, "tty%u", i);

        dev_add_new(tty, tty_name);
    }

    current_tty = ttys[0];

    sched_new_thread(NULL, kernel_process, false, keyboard_handler, NULL,
                     NULL, NULL, NULL, true, NULL);

    return true;
}
