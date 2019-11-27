
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               tty.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "proto.h"
#include "string.h"
#include "proc.h"
#include "global.h"
#include "keyboard.h"

PRIVATE void set_cursor(unsigned int position);
PRIVATE void set_video_start_addr(u32 addr);
PRIVATE void clear_tty();
PRIVATE void show_buf();
PRIVATE void clear_buf();
PRIVATE void handle_normal(u32 key);
PRIVATE void handle_esc(u32 key);

char tty_buf[256] = {0};
char *tty_p = tty_buf;
int backspace_count;
int mode;
char esc_buf[128] = {0};
char *esc_p = esc_buf;
/*======================================================================*
                           task_tty
 *======================================================================*/
PUBLIC void task_tty()
{
        int t = get_ticks();
        int interval = 69000; //69000 约20s
        backspace_count = 0;
        mode = 0; //0 normal  1 esc
        clear_tty();
        while (1)
        {
                if (!mode && ((get_ticks() - t) * 1000 / HZ) >= interval)
                { //普通模式下，超过约20s清屏
                        clear_tty();
                        clear_buf();
                        t = get_ticks();
                }
                if (mode)
                { //esc模式下一直刷新计时器，直至退出esc
                        t = get_ticks();
                }
                keyboard_read();
        }
}

/*======================================================================*
				in_process
 *======================================================================*/
PUBLIC void in_process(u32 key)
{
        char output[2] = {'\0', '\0'};

        if (mode)
        {
                handle_esc(key);
        }
        else
        {
                handle_normal(key);
        }
}

PRIVATE void handle_normal(u32 key)
{
        char output[2] = {'\0', '\0'};

        if (!(key & FLAG_EXT))
        {
                output[0] = key & 0xFF;
                if ((key & FLAG_CTRL_L) || (key & FLAG_CTRL_R)) //ctrl
                {
                        if (output[0] == 'z')
                        {
                                //撤销
                                if (tty_p > tty_buf)
                                {
                                        if (*tty_p == '\b')
                                        {
                                                backspace_count--;
                                        }
                                        tty_p--;
                                        clear_tty();
                                        show_buf();
                                }
                        }
                        else if (output[0] == 'c')
                        {
                                //清屏
                                clear_buf();
                                clear_tty();
                        }
                        set_cursor(disp_pos / 2);
                }
                else
                { //普通字符 以及Alt等
                        disp_str(output);
                        set_cursor(disp_pos / 2);
                        *tty_p++ = output[0];
                }
        }
        else
        {
                int raw_code = key & MASK_RAW;
                switch (raw_code)
                {
                case ENTER:
                        if (disp_pos < V_MEM_SIZE - 160)
                        {
                                disp_pos = 160 * (disp_pos / 160 + 1); //定位
                                set_cursor(disp_pos / 2);
                                *tty_p++ = '\n';
                                break;
                        }

                case TAB:
                        disp_str("    ");
                        set_cursor(disp_pos / 2);
                        *tty_p++ = '\t';
                        break;
                case BACKSPACE:
                        clear_tty();
                        if ((tty_p - tty_buf) > (backspace_count * 2))
                        {
                                *tty_p++ = '\b';
                                backspace_count++;
                        }
                        show_buf();
                        set_cursor(disp_pos / 2);
                        break;
                case UP:
                        if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R))
                        {
                                set_video_start_addr(80 * 15);
                        }
                        break;
                case DOWN:
                        if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R))
                        {
                                set_video_start_addr(80 * 0);
                        }
                        break;
                case ESC:
                        mode = 1;
                        break;
                default:
                        break;
                }
        }
}
PRIVATE void handle_esc(u32 key)
{
}

/*======================================================================*
			    clear_tty
 *======================================================================*/
PRIVATE void clear_tty()
{
        u8 *p_vmem = (u8 *)(V_MEM_BASE + disp_pos - 1);
        while (p_vmem > V_MEM_BASE)
        {
                *p_vmem-- = 0x07;
                *p_vmem-- = ' ';
        }
        disp_pos = 0;
        set_cursor(0);
        set_video_start_addr(0);
}

/*======================================================================*
			    clear_buf
 *======================================================================*/
PRIVATE void clear_buf()
{
        tty_p = tty_buf;
        *tty_p = 0;
        backspace_count = 0;
}

/*======================================================================*
			    show_buf
 *======================================================================*/
PRIVATE void show_buf()
{
        char *p = tty_buf;
        char tmp[256] = {0}, *q = tmp;
        while (p < tty_p)
        {
                if (*p == '\b')
                {
                        *(--q) = 0;
                }
                else
                {
                        *q++ = *p;
                        *q = 0;
                }
                p++;
        }
        q = tmp;
        char output[2] = {'\0', '\0'};
        while (*q != 0)
        {
                switch (*q)
                {
                case '\n':
                        disp_pos = 160 * (disp_pos / 160 + 1);
                        break;
                case '\t':
                        disp_str("    ");
                        break;
                default:
                        output[0] = *q;
                        disp_str(output);
                        break;
                }
                q++;
        }
}

/*======================================================================*
			  set_video_start_addr
 *======================================================================*/
PRIVATE void set_video_start_addr(u32 addr)
{
        disable_int();
        out_byte(CRTC_ADDR_REG, START_ADDR_H);
        out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
        out_byte(CRTC_ADDR_REG, START_ADDR_L);
        out_byte(CRTC_DATA_REG, addr & 0xFF);
        enable_int();
}

/*======================================================================*
			    set_cursor
 *======================================================================*/
PRIVATE void set_cursor(unsigned int position)
{
        disable_int();
        out_byte(CRTC_ADDR_REG, CURSOR_H);
        out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
        out_byte(CRTC_ADDR_REG, CURSOR_L);
        out_byte(CRTC_DATA_REG, position & 0xFF);
        enable_int();
}