
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
PRIVATE void clear_tty(int);
PRIVATE void show_buf(int);
PRIVATE void clear_buf(int);
PRIVATE void handle_normal(u32 key);
PRIVATE void handle_esc(u32 key);
PRIVATE void task_init();
PRIVATE void buf_convert(char *p, char q[], char *buf_p);
PRIVATE void show_matching();

int mode; //0:normal 1:esc
// normal
char tty_buf[1024] = {0};
char *tty_p;
int backspace_count;
//esc
char esc_buf[1024] = {0};
char *esc_p;
int esc_back_count;
int esc_base; //显示的起始位置
//lock_esc
int lock_esc;

/*======================================================================*
                           task_init
 *======================================================================*/
PRIVATE void task_init()
{
        tty_p = tty_buf;
        backspace_count = 0;
        esc_p = esc_buf;
        esc_back_count = 0;
        mode = 0;
        lock_esc = 0;
        clear_tty(0);
}
/*======================================================================*
                           task_tty
 *======================================================================*/
PUBLIC void
task_tty()
{
        int t = get_ticks();
        int interval = 69000; //69000 约20s
        task_init();
        while (1)
        {
                if (!mode && ((get_ticks() - t) * 1000 / HZ) >= interval)
                { //普通模式下，超过约20s清屏
                        clear_tty(0);
                        clear_buf(0);
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
                                        clear_tty(0);
                                        show_buf(0);
                                }
                        }
                        else if (output[0] == 'c')
                        {
                                //清屏
                                clear_buf(0);
                                clear_tty(0);
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
                        clear_tty(0);
                        if ((tty_p - tty_buf) > (backspace_count * 2))
                        {
                                *tty_p++ = '\b';
                                backspace_count++;
                        }
                        show_buf(0);
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
                        esc_base = V_MEM_BASE + disp_pos;
                        // esc_base = V_MEM_BASE;
                        break;
                }
        }
}
PRIVATE void handle_esc(u32 key)
{
        char output[2] = {'\0', '\0'};
        if (lock_esc)
        {
                if ((key & MASK_RAW) == ESC)
                {
                        lock_esc = 0;
                }
                else
                {
                        return;
                }
        }
        if (!(key & FLAG_EXT))
        {
                output[0] = key & 0xFF;
                if ((key & FLAG_CTRL_L) || (key & FLAG_CTRL_R)) //ctrl
                {
                        if (output[0] == 'z')
                        {
                                //撤销
                                if (esc_p > esc_buf)
                                {
                                        if (*esc_p == '\b')
                                        {
                                                esc_back_count--;
                                        }
                                        esc_p--;
                                        clear_tty(1);
                                        show_buf(1);
                                }
                        }
                        else if (output[0] == 'c')
                        {
                                //清屏
                                clear_buf(1);
                                clear_tty(1);
                        }
                        set_cursor(disp_pos / 2);
                }
                else
                { //普通字符 以及Alt等
                        disp_color_str(output, 0x01);
                        set_cursor(disp_pos / 2);
                        *esc_p++ = output[0];
                }
        }
        else
        {
                int raw_code = key & MASK_RAW;
                switch (raw_code)
                {
                case ENTER: //TODO
                        show_matching();
                        lock_esc = 1;
                        break;

                case TAB:
                        disp_str("    ");
                        set_cursor(disp_pos / 2);
                        *esc_p++ = '\t';
                        break;
                case BACKSPACE:
                        clear_tty(1);
                        if ((esc_p - esc_buf) > (esc_back_count * 2))
                        {
                                *esc_p++ = '\b';
                                esc_back_count++;
                        }
                        show_buf(1);
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
                        mode = 0;
                        clear_buf(1);
                        clear_tty(0);
                        show_buf(0);
                        set_cursor(disp_pos / 2);
                        break;
                }
        }
}

/*======================================================================*
			    clear_tty 0:all 1:esc
 *======================================================================*/
PRIVATE void clear_tty(int flag)
{
        int base = flag ? esc_base : V_MEM_BASE; //TODO
        u8 *p_vmem = (u8 *)(V_MEM_BASE + disp_pos - 1);
        while (p_vmem > base)
        {
                *p_vmem-- = 0x07;
                *p_vmem-- = ' ';
        }
        disp_pos = base - V_MEM_BASE;
        set_cursor(disp_pos);
        set_video_start_addr(0);
}

/*======================================================================*
			    clear_buf 0:tty_buf 1:esc_buf
 *======================================================================*/
PRIVATE void clear_buf(int flag)
{
        if (flag)
        {
                esc_p = esc_buf;
                *esc_p = 0;
                esc_back_count = 0;
        }
        else
        {
                tty_p = tty_buf;
                *tty_p = 0;
                backspace_count = 0;
        }
}

/*======================================================================*
			    show_buf 0:tty_buf 1:esc_buf
 *======================================================================*/
PRIVATE void show_buf(int flag)
{
        char *p = flag ? esc_buf : tty_buf;
        char *buf_p = flag ? esc_p : tty_p;
        char tmp[1024] = {0}, *q = tmp;
        buf_convert(p, tmp, buf_p);
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
                        if (flag)
                        {
                                disp_color_str(output, 0x01);
                        }
                        else
                        {
                                disp_str(output);
                        }
                        break;
                }
                q++;
        }
}

/*======================================================================*
			    show_matching
 *======================================================================*/
PRIVATE void show_matching()
{
        char tmp1[1024] = {0};
        buf_convert(esc_buf, tmp1, esc_p);
        if (tmp1[0] == 0)
        {
                return;
        }
        char tmp0[1024] = {0};
        buf_convert(tty_buf, tmp0, tty_p);
        clear_tty(0);
        char *p = tmp0, *q = tmp1;
        char output[2] = {'\0', '\0'};
        while (*p != 0)
        {
                char *tmp = p;
                while (*tmp == *q && *q != 0 && *tmp != 0)
                {
                        tmp++;
                        q++;
                }
                if (*q == 0)
                { //匹配
                        for (int i = 0; i < q - tmp1; i++)
                        {
                                output[0] = *p++;
                                if (output[0] != '\t')
                                {
                                        disp_color_str(output, 0x01);
                                }
                                else
                                {
                                        disp_color_str("    ", 0x01);
                                }
                        }
                }
                else
                {
                        output[0] = *p++;
                        if (output[0] != '\t')
                        {
                                disp_str(output);
                        }
                        else
                        {
                                disp_str("    ");
                        }
                }
                q = tmp1;
        }
        show_buf(1);
        set_cursor(disp_pos / 2);
}

/*======================================================================*
			    去掉去 *p指向的buf 中的 \b，存入 q[]
 *======================================================================*/
PRIVATE void buf_convert(char *p, char q[], char *buf_p)
{
        while (p < buf_p)
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