/** 
 * @file ui.c
 * @brief LVGL实现的简单ui界面
 * @author Nahida
 * @date 2026.9.10
 */

#include "ui.h"
#include "Transfer.h"
#include "USART_RT.h"
#include "CAN_RT.h"
#include "IIC_RT.h"
#include "SPI_RT.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

//屏幕尺寸
#define UI_W       240
#define UI_H       320

//颜色定义
#define CLR_BG      lv_color_hex(0x1a1a2e)
#define CLR_PANEL   lv_color_hex(0x16213e)
#define CLR_BTN     lv_color_hex(0x0f3460)
#define CLR_ACCENT  lv_color_hex(0xe94560)
#define CLR_GREEN   lv_color_hex(0x00b894)
#define CLR_TEXT    lv_color_white()

//端口名
static const char *src_name[] = {
    "CAN1", "CAN2", "USART1", "USART2",
    "IIC1", "IIC2", "SPI2",  "SPI3"
};

//消息环形缓冲区
#define MSG_BUF_SIZE   30

static Msg_t   msg_buf[MSG_BUF_SIZE];
static uint8_t msg_head = 0;
static uint8_t msg_cnt  = 0;

//协议配置结构体
static USART_Config_t  ui_usart_cfg[2];
static CAN_Config_t    ui_can_cfg[2];
static IIC_Config_t    ui_iic_cfg[2];
static SPI_Config_t    ui_spi_cfg[2];

//页面名称
typedef enum {
    PAGE_NONE   = 0,
    PAGE_MAIN,
    PAGE_MODULE,
    PAGE_CONFIG
} PageType_t;

static PageType_t  cur_page    = PAGE_NONE;
static lv_obj_t   *cur_scr     = NULL;
static Src_t       mod_src     = mCAN1;
static Src_t       cfg_module  = mCAN1;

//模块页面转发控制
static bool  fwd_active = false;
static Src_t fwd_dest   = mCAN1;

//消息更新定时器
static lv_timer_t *msg_timer = NULL;

//配置页面动态控制
static lv_obj_t *cfg_params_cont = NULL;
static lv_timer_t *cfg_msg_timer = NULL;

/* Handles to the dropdown widgets inside cfg_params_cont, rebuilt on each
 * cfg_params_rebuild() call so cb_cfg_confirm can iterate them safely. */
static lv_obj_t *cfg_dd_list[12];
static int       cfg_dd_count;

//配置页面配置保存
static uint32_t cfg_fwd_id;
static uint32_t cfg_fwd_ide;
static uint32_t cfg_fwd_mask;
static uint16_t cfg_fwd_tgt;

//波特率标签
static lv_obj_t *cfg_freq_lbl = NULL;

/* Label handles inside the Fwd ID / Fwd Mask buttons (for IDE-format refresh) */
static lv_obj_t *cfg_fwd_id_lbl   = NULL;
static lv_obj_t *cfg_fwd_mask_lbl = NULL;

/* CAN prescaler set via keyboard (replaces dropdown) */
static uint32_t   cfg_can_pres;
static lv_obj_t  *cfg_pres_lbl;


//USART配置页面选择数值
//波特率
static const uint32_t baud_vals[] = {
    9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
};
static const char *baud_opts = "9600\n19200\n38400\n57600\n115200\n230400\n460800\n921600";

//字长
static const uint32_t wlen_vals[] = {
    UART_WORDLENGTH_8B, UART_WORDLENGTH_9B
};
static const char *wlen_opts = "8 bits\n9 bits";

//停止位
static const uint32_t stop_vals[] = {
    UART_STOPBITS_1, UART_STOPBITS_2
};
static const char *stop_opts = "1\n2";

//校验位
static const uint32_t parity_vals[] = {
    UART_PARITY_NONE, UART_PARITY_EVEN, UART_PARITY_ODD
};
static const char *parity_opts = "None\nEven\nOdd";

//CAN配置页面选择数值
//CAN模式
static const uint32_t can_mode_vals[] = {
    CAN_MODE_NORMAL, CAN_MODE_LOOPBACK, CAN_MODE_SILENT, CAN_MODE_SILENT_LOOPBACK
};
static const char *can_mode_opts = "Normal\nLoopback\nSilent\nSilent Loopback";

//CAN预分频值
static const uint32_t can_pres_vals[] = { 1, 2, 4, 8, 16, 32, 42, 64 };
static const char *can_pres_opts = "1\n2\n4\n8\n16\n32\n42\n64";

//CAN同步跳转宽度
static const uint32_t can_bs1_vals[] = {
    CAN_BS1_1TQ, CAN_BS1_2TQ, CAN_BS1_3TQ, CAN_BS1_4TQ,
    CAN_BS1_5TQ, CAN_BS1_6TQ, CAN_BS1_7TQ, CAN_BS1_8TQ,
    CAN_BS1_9TQ, CAN_BS1_10TQ, CAN_BS1_11TQ, CAN_BS1_12TQ,
    CAN_BS1_13TQ, CAN_BS1_14TQ, CAN_BS1_15TQ, CAN_BS1_16TQ
};
static const char *can_bs1_opts =
    "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16";

//CAn时间段2        
static const uint32_t can_bs2_vals[] = {
    CAN_BS2_1TQ, CAN_BS2_2TQ, CAN_BS2_3TQ, CAN_BS2_4TQ,
    CAN_BS2_5TQ, CAN_BS2_6TQ, CAN_BS2_7TQ, CAN_BS2_8TQ
};
static const char *can_bs2_opts = "1\n2\n3\n4\n5\n6\n7\n8";

//CAN自动总线关闭
static const uint32_t can_retrans_vals[] = { DISABLE, ENABLE };
static const char *can_retrans_opts = "Disable\nEnable";

//IIC配置页面选择数值
//IIC时钟速率
static const uint32_t iic_clock_vals[] = { 100000, 400000 };
static const char *iic_clock_opts = "100 kHz\n400 kHz";

//IIC占空比
static const uint32_t iic_duty_vals[] = { I2C_DUTYCYCLE_2, I2C_DUTYCYCLE_16_9 };
static const char *iic_duty_opts = "2\n16/9";

//IIC地址
static const uint32_t iic_addr_vals[] = { 0x30, 0x40, 0x50, 0x60, 0x68, 0x70 };
static const char *iic_addr_opts = "0x30\n0x40\n0x50\n0x60\n0x68\n0x70";

//SPI配置页面数值选择
static const uint32_t spi_dsize_vals[] = { SPI_DATASIZE_8BIT, SPI_DATASIZE_16BIT };
static const char *spi_dsize_opts = "8 bits\n16 bits";

//SPIPOL
static const uint32_t spi_cpol_vals[] = { SPI_POLARITY_LOW, SPI_POLARITY_HIGH };
static const char *spi_cpol_opts = "Low\nHigh";

//SPIPHA
static const uint32_t spi_cpha_vals[] = { SPI_PHASE_1EDGE, SPI_PHASE_2EDGE };
static const char *spi_cpha_opts = "1 Edge\n2 Edge";

//SPI分频值
static const uint32_t spi_baud_vals[] = {
    SPI_BAUDRATEPRESCALER_2,   SPI_BAUDRATEPRESCALER_4,
    SPI_BAUDRATEPRESCALER_8,   SPI_BAUDRATEPRESCALER_16,
    SPI_BAUDRATEPRESCALER_32,  SPI_BAUDRATEPRESCALER_64,
    SPI_BAUDRATEPRESCALER_128, SPI_BAUDRATEPRESCALER_256
};
static const char *spi_baud_opts = "2\n4\n8\n16\n32\n64\n128\n256";

//实时显示当前SPI速率
static const uint32_t spi_baud_div[] = { 2, 4, 8, 16, 32, 64, 128, 256 };

//实时显示当前CAN模式
static const uint32_t can_ide_vals[] = { CAN_ID_STD, CAN_ID_EXT };
static const char *can_ide_opts = "Standard\nExtended";

//模块索引
static int mod_index(Src_t s)
{
    if (s >= mUSART1 && s <= mUSART2) return s - mUSART1;
    if (s >= mCAN1   && s <= mCAN2)   return s - mCAN1;
    if (s >= mIIC1   && s <= mIIC2)   return s - mIIC1;
    if (s >= mSPI2   && s <= mSPI3)   return s - mSPI2;
    return 0;
}

/**
  *@brief 设置默认配置
  *@param NULL 
  *@retval NULL
  */
static void set_defaults(void)
{
    //USART
    for (int i = 0; i < 2; i++) {
        ui_usart_cfg[i].BaudRate   = 115200;
        ui_usart_cfg[i].WordLength = UART_WORDLENGTH_8B;
        ui_usart_cfg[i].StopBits   = UART_STOPBITS_1;
        ui_usart_cfg[i].Parity     = UART_PARITY_NONE;
    }
    //CAN
    for (int i = 0; i < 2; i++) {
        ui_can_cfg[i].Prescaler          = 42;
        ui_can_cfg[i].Mode               = CAN_MODE_NORMAL;
        ui_can_cfg[i].SyncJumpWidth      = CAN_SJW_1TQ;
        ui_can_cfg[i].TimeSeg1           = CAN_BS1_13TQ;
        ui_can_cfg[i].TimeSeg2           = CAN_BS2_2TQ;
        ui_can_cfg[i].AutoBusOff         = DISABLE;
        ui_can_cfg[i].AutoRetransmission = ENABLE;
    }
    //IIC
    for (int i = 0; i < 2; i++) {
        ui_iic_cfg[i].ClockSpeed       = 400000;
        ui_iic_cfg[i].DutyCycle        = I2C_DUTYCYCLE_2;
        ui_iic_cfg[i].OwnAddress1      = 0x30;
        ui_iic_cfg[i].AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
        ui_iic_cfg[i].DualAddressMode  = I2C_DUALADDRESS_DISABLE;
        ui_iic_cfg[i].OwnAddress2      = 0x00;
        ui_iic_cfg[i].GeneralCallMode  = I2C_GENERALCALL_DISABLE;
        ui_iic_cfg[i].NoStretchMode    = I2C_NOSTRETCH_DISABLE;
    }
    //SPI
    for (int i = 0; i < 2; i++) {
        ui_spi_cfg[i].Mode              = SPI_MODE_MASTER;
        ui_spi_cfg[i].Direction         = SPI_DIRECTION_2LINES;
        ui_spi_cfg[i].DataSize          = SPI_DATASIZE_8BIT;
        ui_spi_cfg[i].CLKPolarity       = SPI_POLARITY_LOW;
        ui_spi_cfg[i].CLKPhase          = SPI_PHASE_1EDGE;
        ui_spi_cfg[i].NSS               = SPI_NSS_SOFT; 
        ui_spi_cfg[i].BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
        ui_spi_cfg[i].FirstBit          = SPI_FIRSTBIT_MSB;
    }
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static int find_val_idx(uint32_t val, const uint32_t *tbl, int cnt)
{
    for (int i = 0; i < cnt; i++)
        if (tbl[i] == val) return i;
    return 0;
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static uint32_t parse_hex(const char *s)
{
    uint32_t v = 0;
    if (!s) return 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        char c = *s++;
        if      (c >= '0' && c <= '9') v = (v << 4) | (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v = (v << 4) | (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v = (v << 4) | (uint32_t)(c - 'A' + 10);
    }
    return v;
}

static lv_obj_t  *kb_popup  = NULL;
static lv_obj_t  *kb_ta     = NULL;
static void (*kb_on_save)(uint32_t val, lv_obj_t *lbl) = NULL;
static lv_obj_t  *kb_label  = NULL;

/**
  *@brief 关闭键盘
  *@param NULL
  *@retval NULL
  */
static void kb_close(void)
{
    if (kb_popup) { lv_obj_del(kb_popup); kb_popup = NULL; }
    kb_ta     = NULL;
    kb_label  = NULL;
    kb_on_save = NULL;
}

/**
  *@brief 键盘确认回调
  *@param e LVGL事件对象
  *@retval NULL
  */
static void kb_confirm_cb(lv_event_t *e)
{
    (void)e;
    if (kb_ta && kb_on_save) {
        kb_on_save(parse_hex(lv_textarea_get_text(kb_ta)), kb_label);
    }
    kb_close();
}

/**
  *@brief 键盘取消回调
  *@param e LVGL事件对象
  *@retval NULL
  */
static void kb_cancel_cb(lv_event_t *e)
{
    (void)e;
    kb_close();
}

/**
  *@brief 键盘按下回调
  *@param e LVGL事件对象
  *@retval NULL
  */
static void kb_btnm_cb(lv_event_t *e)
{
    lv_obj_t *btnm = lv_event_get_target(e);
    uint16_t id = lv_btnmatrix_get_selected_btn(btnm);
    const char *txt = lv_btnmatrix_get_btn_text(btnm, id);
    if (!kb_ta) return;

    if (strcmp(txt, "OK") == 0) {
        kb_confirm_cb(NULL);
    } else if (strcmp(txt, "Cancel") == 0) {
        kb_cancel_cb(NULL);
    } else if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        lv_textarea_set_cursor_pos(kb_ta, LV_TEXTAREA_CURSOR_LAST);
        lv_textarea_del_char(kb_ta);
    } else {
        lv_textarea_add_text(kb_ta, txt);
    }
}

//键盘按键分配
static const char * hex_kb_map[] = {
    "7", "8", "9", "A", "\n",
    "4", "5", "6", "B", "\n",
    "1", "2", "3", "C", "\n",
    "0", "D", "E", "F", "\n",
    LV_SYMBOL_BACKSPACE, "Cancel", "OK", ""
};

/**
  *@brief 键盘编辑
  *@param title 键盘标题
    *@param cur_val 当前值
    *@param on_save 保存回调
    *@param label_to_update 需要更新的标签
  *@retval NULL
  */
static void show_hex_edit(const char *title, uint32_t cur_val,
                          void (*on_save)(uint32_t, lv_obj_t *),
                          lv_obj_t *label_to_update)
{
    if (kb_popup) kb_close();

    kb_label  = label_to_update;
    kb_on_save = on_save;
    lv_obj_t *scr = lv_scr_act();

    kb_popup = lv_obj_create(scr);
    lv_obj_set_size(kb_popup, UI_W, UI_H);
    lv_obj_set_pos(kb_popup, 0, 0);
    lv_obj_set_style_bg_color(kb_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(kb_popup, 200, 0);
    lv_obj_set_style_border_width(kb_popup, 0, 0);
    lv_obj_clear_flag(kb_popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(kb_popup);
    lv_obj_set_size(panel, 220, 72);
    lv_obj_set_pos(panel, 10, 6);
    lv_obj_set_style_bg_color(panel, CLR_PANEL, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *tl = lv_label_create(panel);
    lv_label_set_text(tl, title);
    lv_obj_align(tl, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_style_text_color(tl, CLR_ACCENT, 0);

    lv_obj_t *ta = lv_textarea_create(panel);
    lv_obj_set_size(ta, 170, 30);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 26);
    lv_textarea_set_accepted_chars(ta, "0123456789abcdefABCDEFxX");
    lv_textarea_set_max_length(ta, 10);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_style_bg_color(ta, CLR_BG, 0);
    lv_obj_set_style_text_color(ta, CLR_TEXT, 0);
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)cur_val);
        lv_textarea_set_text(ta, buf);
    }
    kb_ta = ta; 

    lv_obj_t *btnm = lv_btnmatrix_create(kb_popup);
    lv_btnmatrix_set_map(btnm, hex_kb_map);
    lv_obj_set_pos(btnm, 8, 84);
    lv_obj_set_size(btnm, UI_W - 16, 156);
    lv_obj_set_style_bg_color(btnm, CLR_PANEL, 0);
    lv_obj_set_style_border_width(btnm, 0, 0);
    lv_obj_set_style_radius(btnm, 6, 0);
    lv_btnmatrix_set_one_checked(btnm, false);
    lv_obj_add_event_cb(btnm, kb_btnm_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void kbs_can_id(uint32_t val, lv_obj_t *lbl)
{
    cfg_fwd_id = val & ((cfg_fwd_ide == CAN_ID_EXT) ? 0x1FFFFFFF : 0x7FF);
    if (lbl) {
        char buf[16];
        snprintf(buf, sizeof(buf), (cfg_fwd_ide == CAN_ID_EXT) ? "0x%08lX" : "0x%03lX",
                 (unsigned long)cfg_fwd_id);
        lv_label_set_text(lbl, buf);
    }
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void kbs_can_mask(uint32_t val, lv_obj_t *lbl)
{
    cfg_fwd_mask = val & ((cfg_fwd_ide == CAN_ID_EXT) ? 0x1FFFFFFF : 0x7FF);
    if (lbl) {
        char buf[16];
        snprintf(buf, sizeof(buf), (cfg_fwd_ide == CAN_ID_EXT) ? "0x%08lX" : "0x%03lX",
                 (unsigned long)cfg_fwd_mask);
        lv_label_set_text(lbl, buf);
    }
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void kbs_iic_tgt(uint32_t val, lv_obj_t *lbl)
{
    cfg_fwd_tgt = (uint16_t)(val & 0xFF);
    if (lbl) {
        char buf[16];
        snprintf(buf, sizeof(buf), "0x%02X", cfg_fwd_tgt);
        lv_label_set_text(lbl, buf);
    }
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_fwd_id_btn(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    uint32_t lim = (cfg_fwd_ide == CAN_ID_EXT) ? 0x1FFFFFFF : 0x7FF;
    show_hex_edit("Fwd CAN ID", cfg_fwd_id & lim, kbs_can_id, lbl);
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_fwd_mask_btn(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    uint32_t lim = (cfg_fwd_ide == CAN_ID_EXT) ? 0x1FFFFFFF : 0x7FF;
    show_hex_edit("Fwd Mask", cfg_fwd_mask & lim, kbs_can_mask, lbl);
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_iic_tgt_btn(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    show_hex_edit("Target Addr", cfg_fwd_tgt, kbs_iic_tgt, lbl);
}

static void cfg_freq_update(void);  

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void kbs_can_pres(uint32_t val, lv_obj_t *lbl)
{
    cfg_can_pres = (val < 1) ? 1 : val;
    if (lbl) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)cfg_can_pres);
        lv_label_set_text(lbl, buf);
    }
    cfg_freq_update();
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_pres_btn(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    show_hex_edit("Prescaler", cfg_can_pres, kbs_can_pres, lbl);
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_fwd_ide_changed(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    uint32_t new_ide = (lv_dropdown_get_selected(dd) == 0) ? CAN_ID_STD : CAN_ID_EXT;
    if (new_ide == cfg_fwd_ide) return;
    cfg_fwd_ide = new_ide;

    if (cfg_fwd_id_lbl) {
        char b[16];
        cfg_fwd_id &= (cfg_fwd_ide == CAN_ID_EXT) ? 0x1FFFFFFF : 0x7FF;
        snprintf(b, sizeof(b), (cfg_fwd_ide == CAN_ID_EXT) ? "0x%08lX" : "0x%03lX",
                 (unsigned long)cfg_fwd_id);
        lv_label_set_text(cfg_fwd_id_lbl, b);
    }
    if (cfg_fwd_mask_lbl) {
        char b[16];
        cfg_fwd_mask &= (cfg_fwd_ide == CAN_ID_EXT) ? 0x1FFFFFFF : 0x7FF;
        snprintf(b, sizeof(b), (cfg_fwd_ide == CAN_ID_EXT) ? "0x%08lX" : "0x%03lX",
                 (unsigned long)cfg_fwd_mask);
        lv_label_set_text(cfg_fwd_mask_lbl, b);
    }
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cfg_freq_update(void)
{
    if (!cfg_freq_lbl) return;
    char buf[48];
    buf[0] = '\0';

    switch (cfg_module)
    {
        case mUSART1: case mUSART2: {
            if (cfg_dd_count < 1) break;
            int i = lv_dropdown_get_selected(cfg_dd_list[0]);
            if (i >= 0 && i < 8)
                snprintf(buf, sizeof(buf), "Baud: %lu", (unsigned long)baud_vals[i]);
            break;
        }
        case mCAN1: case mCAN2: {
            if (cfg_dd_count < 3) break;
            uint32_t pres = cfg_can_pres;
            int bs1_i = lv_dropdown_get_selected(cfg_dd_list[1]); 
            int bs2_i = lv_dropdown_get_selected(cfg_dd_list[2]); 
            uint32_t total_tq = 1 + (bs1_i + 1) + (bs2_i + 1); 
            uint32_t apb1 = HAL_RCC_GetPCLK1Freq();
            uint32_t bitrate = apb1 / (pres * total_tq);
            if (bitrate >= 1000000)
                snprintf(buf, sizeof(buf), "Bit Rate: %lu Mbps", (unsigned long)(bitrate / 1000000));
            else
                snprintf(buf, sizeof(buf), "Bit Rate: %lu kbps", (unsigned long)(bitrate / 1000));
            break;
        }
        case mIIC1: case mIIC2: {
            if (cfg_dd_count < 1) break;
            int i = lv_dropdown_get_selected(cfg_dd_list[0]);
            if (i >= 0 && i < 2) {
                if (iic_clock_vals[i] >= 1000000)
                    snprintf(buf, sizeof(buf), "Clock: %lu MHz", (unsigned long)(iic_clock_vals[i] / 1000000));
                else
                    snprintf(buf, sizeof(buf), "Clock: %lu kHz", (unsigned long)(iic_clock_vals[i] / 1000));
            }
            break;
        }
        case mSPI2: case mSPI3: {
            if (cfg_dd_count < 4) break;
            int i = lv_dropdown_get_selected(cfg_dd_list[3]);
            if (i >= 0 && i < 8) {
                uint32_t spiclk = HAL_RCC_GetPCLK1Freq();
                if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
                    spiclk *= 2;
                uint32_t freq = spiclk / spi_baud_div[i];
                if (freq >= 1000000)
                    snprintf(buf, sizeof(buf), "SPI Freq: %lu MHz", (unsigned long)(freq / 1000000));
                else
                    snprintf(buf, sizeof(buf), "SPI Freq: %lu kHz", (unsigned long)(freq / 1000));
            }
            break;
        }
        default: break;
    }

    if (buf[0]) lv_label_set_text(cfg_freq_lbl, buf);
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_dd_changed(lv_event_t *e)
{
    (void)e;
    cfg_freq_update();
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
void UI_OnMessage(Msg_t *msg)
{
    __disable_irq();
    memcpy(&msg_buf[msg_head], msg, sizeof(Msg_t));
    msg_head = (msg_head + 1) % MSG_BUF_SIZE;
    if (msg_cnt < MSG_BUF_SIZE) msg_cnt++;
    __enable_irq();
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static int build_msg_text(char *dst, int dst_len, Src_t src)
{
    int pos = 0;

    __disable_irq();
    uint8_t cnt  = msg_cnt;
    uint8_t head = msg_head;
    __enable_irq();

    pos += snprintf(dst + pos, dst_len - pos, "From: ---\n");

    uint8_t shown = 0;
    for (int i = 0; i < cnt; i++)
    {
        uint8_t idx = (head + MSG_BUF_SIZE - 1 - i) % MSG_BUF_SIZE;  
        if (msg_buf[idx].src != src) continue;

        if (shown == 0) {
            pos = 0;
            pos += snprintf(dst + pos, dst_len - pos, "From: %s\n", src_name[src]);
        }
        if (shown >= 6) {  
            pos += snprintf(dst + pos, dst_len - pos, "...\n");
            break;
        }

        pos += snprintf(dst + pos, dst_len - pos, "[%d] ", shown);
        uint16_t n = msg_buf[idx].len;
        if (n > 16) n = 16;
        for (uint16_t j = 0; j < n; j++)
        {
            if (pos >= dst_len - 8) break;
            pos += snprintf(dst + pos, dst_len - pos, "%02X ", msg_buf[idx].data[j]);
        }
        pos += snprintf(dst + pos, dst_len - pos, "\n");
        shown++;
    }

    if (shown == 0)
        pos += snprintf(dst + pos, dst_len - pos, "(no messages)\n");

    return shown;
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void nav_to(lv_obj_t *new_scr, PageType_t type)
{
    if (cur_scr != NULL) {
        lv_scr_load(new_scr);
        lv_obj_del(cur_scr);
    } else {
        lv_scr_load(new_scr);
    }
    cur_scr   = new_scr;
    cur_page  = type;
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static lv_obj_t* main_page_create(void);
static lv_obj_t* module_page_create(Src_t src);
static lv_obj_t* config_page_create(void);

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void nav_main(void)
{
    if (cur_page == PAGE_MODULE && fwd_active) {
        Transfer_SetRoute(mod_src, (Src_t)0xFF);
        fwd_active = false;
    }
    if (msg_timer) { lv_timer_del(msg_timer); msg_timer = NULL; }
    if (cfg_msg_timer) { lv_timer_del(cfg_msg_timer); cfg_msg_timer = NULL; }

    nav_to(main_page_create(), PAGE_MAIN);
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void nav_module(Src_t src)
{
    if (msg_timer) { lv_timer_del(msg_timer); msg_timer = NULL; }
    if (cfg_msg_timer) { lv_timer_del(cfg_msg_timer); cfg_msg_timer = NULL; }

    mod_src = src;
    //重置转发状态
    fwd_active = false;
    fwd_dest   = mCAN1;

    // IIC/SPI 进入监控页面 → 切换到从机接收模式
    if (src == mIIC1 || src == mIIC2)
        IIC_RT_EnterSlaveMode(src);
    else if (src == mSPI2 || src == mSPI3)
        SPI_RT_EnterSlaveMode(src);

    nav_to(module_page_create(src), PAGE_MODULE);
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void nav_config(void)
{
    if (msg_timer) { lv_timer_del(msg_timer); msg_timer = NULL; }
    if (cfg_msg_timer) { lv_timer_del(cfg_msg_timer); cfg_msg_timer = NULL; }

    cfg_module = mCAN1;
    nav_to(config_page_create(), PAGE_CONFIG);
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_back_to_main(lv_event_t *e)
{
    (void)e;
    nav_main();
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_module_btn(lv_event_t *e)
{
    Src_t src = (Src_t)(uintptr_t)lv_event_get_user_data(e);
    nav_module(src);
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_config_btn(lv_event_t *e)
{
    (void)e;
    nav_config();
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_fwd_toggle(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);

    if (fwd_active) {
        //Stop
        Transfer_SetRoute(mod_src, (Src_t)0xFF);
        fwd_active = false;
        lv_label_set_text(lbl, "Start Forward");
        lv_obj_set_style_bg_color(btn, CLR_BTN, 0);
    } else {
        //Start
        Transfer_SetRoute(mod_src, fwd_dest);
        fwd_active = true;
        lv_label_set_text(lbl, "Stop Forward");
        lv_obj_set_style_bg_color(btn, CLR_ACCENT, 0);

        //转发目标切换 保目标处于主机发送模式
        if (fwd_dest == mIIC1 || fwd_dest == mIIC2)
            IIC_RT_EnterMasterMode(fwd_dest);
        else if (fwd_dest == mSPI2 || fwd_dest == mSPI3)
            SPI_RT_EnterMasterMode(fwd_dest);
    }
}

static void cb_fwd_dest_changed(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    fwd_dest = (Src_t)lv_dropdown_get_selected(dd);

    //选择 IIC/SPI作为转发目标端口 切换到主机发送模式
    if (fwd_dest == mIIC1 || fwd_dest == mIIC2)
        IIC_RT_EnterMasterMode(fwd_dest);
    else if (fwd_dest == mSPI2 || fwd_dest == mSPI3)
        SPI_RT_EnterMasterMode(fwd_dest);
}

static void cfg_params_rebuild(void);   

static void cb_cfg_module_changed(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    cfg_module = (Src_t)lv_dropdown_get_selected(dd);
    cfg_params_rebuild();
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_cfg_msg_timeout(lv_timer_t *t)
{
    lv_obj_t *label = (lv_obj_t *)t->user_data;
    if (label) lv_label_set_text(label, "");
    lv_timer_del(cfg_msg_timer);
    cfg_msg_timer = NULL;
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static void cb_cfg_confirm(lv_event_t *e)
{
    (void)e;
    if (cfg_params_cont == NULL || cfg_dd_count == 0) return;

    switch (cfg_module)
    {
        case mUSART1: case mUSART2: {
            USART_Config_t *old = &ui_usart_cfg[mod_index(cfg_module)];
            for (int i = 0; i < cfg_dd_count; i++) {
                int idx = lv_dropdown_get_selected(cfg_dd_list[i]);
                uint32_t v;
                switch (i) {
                    case 0: v = baud_vals[idx];   break;
                    case 1: v = wlen_vals[idx];   break;
                    case 2: v = stop_vals[idx];   break;
                    case 3: v = parity_vals[idx]; break;
                    default: continue;
                }
                if (i == 0) old->BaudRate   = v;
                else if (i == 1) old->WordLength = v;
                else if (i == 2) old->StopBits   = v;
                else if (i == 3) old->Parity     = v;
            }
            USART_RT_Config(cfg_module, old);
            break;
        }

        case mCAN1: case mCAN2: {
            CAN_Config_t *old = &ui_can_cfg[mod_index(cfg_module)];
            for (int i = 0; i < cfg_dd_count; i++) {
                int idx = lv_dropdown_get_selected(cfg_dd_list[i]);
                uint32_t v;
                switch (i) {
                    case 0: v = can_mode_vals[idx];   break;
                    case 1: v = can_bs1_vals[idx];    break;
                    case 2: v = can_bs2_vals[idx];    break;
                    case 3: v = can_retrans_vals[idx]; break;
                    case 4: continue; 
                    default: continue;
                }
                if (i == 0) old->Mode               = v;
                else if (i == 1) old->TimeSeg1           = v;
                else if (i == 2) old->TimeSeg2           = v;
                else if (i == 3) old->AutoRetransmission = v;
            }
            old->Prescaler = cfg_can_pres;
            CAN_RT_Config(cfg_module, old);
            Transfer_SetCANId(cfg_module, cfg_fwd_id);
            Transfer_SetCANIdMode(cfg_module, cfg_fwd_ide);
            Transfer_SetCANMask(cfg_module, cfg_fwd_mask);
            break;
        }

        case mIIC1: case mIIC2: {
            IIC_Config_t *old = &ui_iic_cfg[mod_index(cfg_module)];
            for (int i = 0; i < cfg_dd_count; i++) {
                int idx = lv_dropdown_get_selected(cfg_dd_list[i]);
                uint32_t v;
                switch (i) {
                    case 0: v = iic_clock_vals[idx]; break;
                    case 1: v = iic_duty_vals[idx];  break;
                    case 2: v = iic_addr_vals[idx];  break;
                    default: continue;
                }
                if (i == 0) old->ClockSpeed  = v;
                else if (i == 1) old->DutyCycle   = v;
                else if (i == 2) old->OwnAddress1 = v;
            }
            IIC_RT_Config(cfg_module, old);
            Transfer_SetIICAddr(cfg_module, cfg_fwd_tgt);
            break;
        }

        case mSPI2: case mSPI3: {
            SPI_Config_t *old = &ui_spi_cfg[mod_index(cfg_module)];
            for (int i = 0; i < cfg_dd_count; i++) {
                int idx = lv_dropdown_get_selected(cfg_dd_list[i]);
                uint32_t v;
                switch (i) {
                    case 0: v = spi_dsize_vals[idx]; break;
                    case 1: v = spi_cpol_vals[idx];  break;
                    case 2: v = spi_cpha_vals[idx];  break;
                    case 3: v = spi_baud_vals[idx];  break;
                    default: continue;
                }
                if (i == 0) old->DataSize          = v;
                else if (i == 1) old->CLKPolarity       = v;
                else if (i == 2) old->CLKPhase          = v;
                else if (i == 3) old->BaudRatePrescaler = v;
            }
            SPI_RT_Config(cfg_module, old);
            break;
        }

        default: break;
    }

    if (cfg_msg_timer) { lv_timer_del(cfg_msg_timer); cfg_msg_timer = NULL; }
    lv_obj_t *fb = (lv_obj_t *)lv_event_get_user_data(e);
    if (fb) {
        lv_label_set_text(fb, "Config Saved");
        cfg_msg_timer = lv_timer_create(cb_cfg_msg_timeout, 2000, fb);
        lv_timer_set_repeat_count(cfg_msg_timer, 1);
    }
}

/**
  *@brief 消息更新定时器
  *@param t LVGL定时器
  *@retval NULL
  */
static void cb_msg_timer(lv_timer_t *t)
{
    if (cur_page != PAGE_MODULE) return;

    lv_obj_t *label = (lv_obj_t *)t->user_data;
    if (label == NULL) return;

    char buf[600];
    build_msg_text(buf, sizeof(buf), mod_src);
    lv_label_set_text(label, buf);

    //消息自动滚动
    lv_obj_scroll_to_view(label, LV_ANIM_OFF);
}

/**
  *@brief 
  *@param 
  *@retval NULL
  */
static lv_obj_t* mk_btn(lv_obj_t *parent, const char *text,
                        int x, int y, int w, int h,
                        lv_event_cb_t cb, void *udata)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, CLR_BTN, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, udata);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    return btn;
}

/**
  *@brief 创建顶部导航栏
  *@param parent 父对象
  *@param title 标题
  *@param show_back 是否显示返回按钮
  *@retval NULL
  */
static lv_obj_t* mk_top_bar(lv_obj_t *parent, const char *title, bool show_back)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, UI_W, 35);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, CLR_PANEL, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_scrollbar_mode(bar, LV_SCROLLBAR_MODE_OFF);

    if (show_back) {
        lv_obj_t *bb = lv_btn_create(bar);
        lv_obj_set_size(bb, 30, 28);
        lv_obj_set_pos(bb, 3, 3);
        lv_obj_set_style_bg_color(bb, lv_color_hex(0x333355), 0);
        lv_obj_set_style_radius(bb, 4, 0);
        lv_obj_add_event_cb(bb, cb_back_to_main, LV_EVENT_CLICKED, NULL);
        lv_obj_t *bl = lv_label_create(bb);
        lv_label_set_text(bl, LV_SYMBOL_LEFT);
        lv_obj_center(bl);
    }

    lv_obj_t *tl = lv_label_create(bar);
    lv_label_set_text(tl, title);
    lv_obj_set_style_text_color(tl, CLR_TEXT, 0);
    lv_obj_align(tl, LV_ALIGN_CENTER, 0, 0);
    return bar;
}

/**
  *@brief 主页面 可选择模块或配置
  *@param NULL
  *@retval 创建好的指针
  */
static lv_obj_t* main_page_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Navert");
    lv_obj_set_style_text_color(title, CLR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    int bw = 105, bh = 42, gap_x = 12, gap_y = 10, start_y = 42;

    for (int i = 0; i < 8; i++)
    {
        int col = i % 2;
        int row = i / 2;
        int bx  = 10 + col * (bw + gap_x);
        int by  = start_y + row * (bh + gap_y);

        lv_obj_t *btn = lv_btn_create(scr);
        lv_obj_set_pos(btn, bx, by);
        lv_obj_set_size(btn, bw, bh);
        lv_obj_set_style_bg_color(btn, CLR_BTN, 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_add_event_cb(btn, cb_module_btn, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, src_name[i]);
        lv_obj_set_style_text_color(lbl, CLR_TEXT, 0);
        lv_obj_center(lbl);
    }

    //配置按钮
    lv_obj_t *cfg_btn = lv_btn_create(scr);
    lv_obj_set_size(cfg_btn, UI_W - 20, 40);
    lv_obj_align(cfg_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(cfg_btn, lv_color_hex(0x333355), 0);
    lv_obj_set_style_radius(cfg_btn, 6, 0);
    lv_obj_add_event_cb(cfg_btn, cb_config_btn, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cl = lv_label_create(cfg_btn);
    lv_label_set_text(cl, "Config");
    lv_obj_set_style_text_color(cl, CLR_TEXT, 0);
    lv_obj_center(cl);

    return scr;
}

//模块页面
static lv_obj_t* module_page_create(Src_t src)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);

    //顶部显示当前什么端口模块
    mk_top_bar(scr, src_name[src], true);

    //消息展示区域
    lv_obj_t *msg_cont = lv_obj_create(scr);
    lv_obj_set_pos(msg_cont, 5, 42);
    lv_obj_set_size(msg_cont, UI_W - 10, 140);
    lv_obj_set_style_bg_color(msg_cont, CLR_PANEL, 0);
    lv_obj_set_style_border_width(msg_cont, 1, 0);
    lv_obj_set_style_border_color(msg_cont, lv_color_hex(0x333355), 0);
    lv_obj_set_style_radius(msg_cont, 4, 0);
    lv_obj_set_style_pad_all(msg_cont, 0, 0);
    lv_obj_set_scrollbar_mode(msg_cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(msg_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *msg_lbl = lv_label_create(msg_cont);
    lv_obj_set_pos(msg_lbl, 4, 4);
    lv_obj_set_style_text_color(msg_lbl, CLR_TEXT, 0);
    lv_label_set_text(msg_lbl, "From: ---\n(no messages)");

    //控制转发区域
    lv_obj_t *fwd_cont = lv_obj_create(scr);
    lv_obj_set_pos(fwd_cont, 5, 207);
    lv_obj_set_size(fwd_cont, UI_W - 10, 108);
    lv_obj_set_style_bg_color(fwd_cont, CLR_PANEL, 0);
    lv_obj_set_style_border_width(fwd_cont, 0, 0);
    lv_obj_set_style_radius(fwd_cont, 4, 0);
    lv_obj_set_style_pad_all(fwd_cont, 0, 0);
    lv_obj_set_scrollbar_mode(fwd_cont, LV_SCROLLBAR_MODE_OFF);

    //转发目的显示
    lv_obj_t *fl = lv_label_create(fwd_cont);
    lv_label_set_text(fl, "Forward to:");
    lv_obj_set_pos(fl, 8, 6);
    lv_obj_set_style_text_color(fl, CLR_TEXT, 0);

    //目标选择
    lv_obj_t *dest_dd = lv_dropdown_create(fwd_cont);
    lv_obj_set_pos(dest_dd, 88, 2);
    lv_obj_set_size(dest_dd, 130, 26);
    lv_dropdown_set_options(dest_dd,
        "CAN1\nCAN2\nUSART1\nUSART2\nIIC1\nIIC2\nSPI2\nSPI3");
    lv_dropdown_set_selected(dest_dd, (uint8_t)fwd_dest);
    lv_obj_set_style_bg_color(dest_dd, CLR_BTN, 0);
    lv_obj_set_style_text_color(dest_dd, CLR_TEXT, 0);
    lv_obj_add_event_cb(dest_dd, cb_fwd_dest_changed, LV_EVENT_VALUE_CHANGED, NULL);

    //开启关闭按钮
    lv_obj_t *fwd_btn = lv_btn_create(fwd_cont);
    lv_obj_set_size(fwd_btn, UI_W - 30, 38);
    lv_obj_align(fwd_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(fwd_btn, CLR_BTN, 0);
    lv_obj_set_style_radius(fwd_btn, 6, 0);
    lv_obj_add_event_cb(fwd_btn, cb_fwd_toggle, LV_EVENT_CLICKED, NULL);

    //开始关闭文本
    lv_obj_t *fwd_lbl = lv_label_create(fwd_btn);
    lv_label_set_text(fwd_lbl, "Start Forward");
    lv_obj_center(fwd_lbl);
    lv_obj_set_style_text_color(fwd_lbl, CLR_TEXT, 0);

    if (msg_timer) lv_timer_del(msg_timer);
    msg_timer = lv_timer_create(cb_msg_timer, 250, msg_lbl);
    lv_timer_set_repeat_count(msg_timer, -1); /* forever */

    return scr;
}

/**
  *@brief 配置页面生成
  *@param NULL
  *@retval NULL
  */
static void cfg_params_rebuild(void)
{
    if (cfg_params_cont) {
        lv_obj_clean(cfg_params_cont);
    } else {
        return;
    }

    cfg_dd_count = 0;
    int y = 0;
    const int row_h = 36;

    switch (cfg_module)
    {
        case mUSART1: case mUSART2: {
            USART_Config_t *cfg = &ui_usart_cfg[mod_index(cfg_module)];
            uint32_t cfg_vals[] = { cfg->BaudRate, cfg->WordLength,
                                    cfg->StopBits, cfg->Parity };
            for (int p = 0; p < 4; p++) {
                lv_obj_t *lb = lv_label_create(cfg_params_cont);
                const char *lbl_txt;
                const uint32_t *tbl;
                int cnt;
                const char *opts;
                switch (p) {
                    case 0: lbl_txt = "Baud Rate"; tbl = baud_vals; cnt = 8; opts = baud_opts; break;
                    case 1: lbl_txt = "Word Len";  tbl = wlen_vals; cnt = 2; opts = wlen_opts; break;
                    case 2: lbl_txt = "Stop Bits"; tbl = stop_vals; cnt = 2; opts = stop_opts; break;
                    case 3: lbl_txt = "Parity";    tbl = parity_vals; cnt = 3; opts = parity_opts; break;
                    default: continue;
                }
                lv_label_set_text(lb, lbl_txt);
                lv_obj_set_pos(lb, 6, y + 2);
                lv_obj_set_style_text_color(lb, CLR_TEXT, 0);

                lv_obj_t *dd = lv_dropdown_create(cfg_params_cont);
                lv_obj_set_pos(dd, 110, y);
                lv_obj_set_size(dd, 115, 28);
                lv_dropdown_set_options(dd, opts);
                int si = find_val_idx(cfg_vals[p], tbl, cnt);
                lv_dropdown_set_selected(dd, si);
                lv_obj_set_style_bg_color(dd, CLR_BTN, 0);
                lv_obj_set_style_text_color(dd, CLR_TEXT, 0);
                lv_obj_add_event_cb(dd, cb_dd_changed, LV_EVENT_VALUE_CHANGED, NULL);
                cfg_dd_list[cfg_dd_count++] = dd;
                y += row_h;
            }
            break;
        }

        case mCAN1: case mCAN2: {
            CAN_Config_t *cfg = &ui_can_cfg[mod_index(cfg_module)];
            cfg_fwd_id   = Transfer_GetCANId(cfg_module);
            cfg_fwd_ide  = Transfer_GetCANIdMode(cfg_module);
            cfg_fwd_mask = Transfer_GetCANMask(cfg_module);
            cfg_can_pres = cfg->Prescaler;

            uint32_t h_vals[] = {cfg->Mode, cfg->TimeSeg1, cfg->TimeSeg2, cfg->AutoRetransmission};
            int dd_idx = 0;
            for (int p = 0; p < 8; p++) {
                lv_obj_t *lb = lv_label_create(cfg_params_cont);
                lv_obj_set_pos(lb, 6, y + 2);
                lv_obj_set_style_text_color(lb, CLR_TEXT, 0);

                if (p == 1) {
                    lv_label_set_text(lb, "Prescaler");
                    lv_obj_t *btn = lv_btn_create(cfg_params_cont);
                    lv_obj_set_pos(btn, 110, y);
                    lv_obj_set_size(btn, 115, 28);
                    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e3a5f), 0);
                    lv_obj_set_style_radius(btn, 4, 0);
                    lv_obj_add_event_cb(btn, cb_pres_btn, LV_EVENT_CLICKED, NULL);
                    cfg_pres_lbl = lv_label_create(btn);
                    { char b[16]; snprintf(b, sizeof(b), "%lu", (unsigned long)cfg_can_pres); lv_label_set_text(cfg_pres_lbl, b); }
                    lv_obj_center(cfg_pres_lbl);
                    lv_obj_set_style_text_color(cfg_pres_lbl, CLR_TEXT, 0);
                } else if (p == 0 || (p >= 2 && p <= 4)) {
                    const char *lbl_txt; const uint32_t *tbl; int cnt; const char *opts;
                    int arr_idx;
                    switch (p) {
                        case 0: lbl_txt="Mode";        tbl=can_mode_vals;    cnt=4;  opts=can_mode_opts; arr_idx=0; break;
                        case 2: lbl_txt="Time Seg 1";   tbl=can_bs1_vals;     cnt=16; opts=can_bs1_opts; arr_idx=1; break;
                        case 3: lbl_txt="Time Seg 2";   tbl=can_bs2_vals;     cnt=8;  opts=can_bs2_opts; arr_idx=2; break;
                        case 4: lbl_txt="Auto Retrans"; tbl=can_retrans_vals; cnt=2;  opts=can_retrans_opts; arr_idx=3; break;
                        default: continue;
                    }
                    lv_label_set_text(lb, lbl_txt);
                    lv_obj_t *dd = lv_dropdown_create(cfg_params_cont);
                    lv_obj_set_pos(dd, 110, y);
                    lv_obj_set_size(dd, 115, 28);
                    lv_dropdown_set_options(dd, opts);
                    lv_dropdown_set_selected(dd, find_val_idx(h_vals[arr_idx], tbl, cnt));
                    lv_obj_set_style_bg_color(dd, CLR_BTN, 0);
                    lv_obj_set_style_text_color(dd, CLR_TEXT, 0);
                    lv_obj_add_event_cb(dd, cb_dd_changed, LV_EVENT_VALUE_CHANGED, NULL);
                    cfg_dd_list[dd_idx++] = dd;
                } else if (p == 5) {
                    lv_label_set_text(lb, "Fwd ID");
                    lv_obj_t *btn = lv_btn_create(cfg_params_cont);
                    lv_obj_set_pos(btn, 110, y);
                    lv_obj_set_size(btn, 115, 28);
                    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e3a5f), 0);
                    lv_obj_set_style_radius(btn, 4, 0);
                    lv_obj_add_event_cb(btn, cb_fwd_id_btn, LV_EVENT_CLICKED, NULL);
                    cfg_fwd_id_lbl = lv_label_create(btn);
                    { char b[16]; snprintf(b, sizeof(b), (cfg_fwd_ide == CAN_ID_EXT) ? "0x%08lX" : "0x%03lX", (unsigned long)cfg_fwd_id); lv_label_set_text(cfg_fwd_id_lbl, b); }
                    lv_obj_center(cfg_fwd_id_lbl);
                    lv_obj_set_style_text_color(cfg_fwd_id_lbl, CLR_TEXT, 0);
                } else if (p == 6) {
                    lv_label_set_text(lb, "Fwd IDE");
                    lv_obj_t *dd = lv_dropdown_create(cfg_params_cont);
                    lv_obj_set_pos(dd, 110, y);
                    lv_obj_set_size(dd, 115, 28);
                    lv_dropdown_set_options(dd, can_ide_opts);
                    lv_dropdown_set_selected(dd, (cfg_fwd_ide == CAN_ID_EXT) ? 1 : 0);
                    lv_obj_set_style_bg_color(dd, CLR_BTN, 0);
                    lv_obj_set_style_text_color(dd, CLR_TEXT, 0);
                    lv_obj_add_event_cb(dd, cb_fwd_ide_changed, LV_EVENT_VALUE_CHANGED, NULL);
                    lv_obj_add_event_cb(dd, cb_dd_changed, LV_EVENT_VALUE_CHANGED, NULL);
                    cfg_dd_list[dd_idx++] = dd;
                } else { 
                    lv_label_set_text(lb, "Fwd Mask");
                    lv_obj_t *btn = lv_btn_create(cfg_params_cont);
                    lv_obj_set_pos(btn, 110, y);
                    lv_obj_set_size(btn, 115, 28);
                    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e3a5f), 0);
                    lv_obj_set_style_radius(btn, 4, 0);
                    lv_obj_add_event_cb(btn, cb_fwd_mask_btn, LV_EVENT_CLICKED, NULL);
                    cfg_fwd_mask_lbl = lv_label_create(btn);
                    { char b[16]; snprintf(b, sizeof(b), (cfg_fwd_ide == CAN_ID_EXT) ? "0x%08lX" : "0x%03lX", (unsigned long)cfg_fwd_mask); lv_label_set_text(cfg_fwd_mask_lbl, b); }
                    lv_obj_center(cfg_fwd_mask_lbl);
                    lv_obj_set_style_text_color(cfg_fwd_mask_lbl, CLR_TEXT, 0);
                }
                y += row_h;
            }
            cfg_dd_count = dd_idx;
            break;
        }

        case mIIC1: case mIIC2: {
            IIC_Config_t *cfg = &ui_iic_cfg[mod_index(cfg_module)];
            cfg_fwd_tgt = Transfer_GetIICAddr(cfg_module);

            uint32_t i_vals[] = {cfg->ClockSpeed, cfg->DutyCycle, cfg->OwnAddress1};
            int dd_idx = 0;
            for (int p = 0; p < 4; p++) {
                lv_obj_t *lb = lv_label_create(cfg_params_cont);
                lv_obj_set_pos(lb, 6, y + 2);
                lv_obj_set_style_text_color(lb, CLR_TEXT, 0);

                if (p <= 2) {
                    const char *lbl_txt; const uint32_t *tbl; int cnt; const char *opts;
                    switch (p) {
                        case 0: lbl_txt="Clock Speed"; tbl=iic_clock_vals; cnt=2; opts=iic_clock_opts; break;
                        case 1: lbl_txt="Duty Cycle";  tbl=iic_duty_vals;  cnt=2; opts=iic_duty_opts; break;
                        case 2: lbl_txt="Own Address"; tbl=iic_addr_vals;  cnt=6; opts=iic_addr_opts; break;
                        default: continue;
                    }
                    lv_label_set_text(lb, lbl_txt);
                    lv_obj_t *dd = lv_dropdown_create(cfg_params_cont);
                    lv_obj_set_pos(dd, 110, y);
                    lv_obj_set_size(dd, 115, 28);
                    lv_dropdown_set_options(dd, opts);
                    lv_dropdown_set_selected(dd, find_val_idx(i_vals[p], tbl, cnt));
                    lv_obj_set_style_bg_color(dd, CLR_BTN, 0);
                    lv_obj_set_style_text_color(dd, CLR_TEXT, 0);
                    lv_obj_add_event_cb(dd, cb_dd_changed, LV_EVENT_VALUE_CHANGED, NULL);
                cfg_dd_list[dd_idx++] = dd;
                } else {
                    lv_label_set_text(lb, "Target Addr");
                    lv_obj_t *btn = lv_btn_create(cfg_params_cont);
                    lv_obj_set_pos(btn, 110, y);
                    lv_obj_set_size(btn, 115, 28);
                    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e3a5f), 0);
                    lv_obj_set_style_radius(btn, 4, 0);
                    lv_obj_add_event_cb(btn, cb_iic_tgt_btn, LV_EVENT_CLICKED, NULL);
                    lv_obj_t *bl = lv_label_create(btn);
                    { char b[16]; snprintf(b, sizeof(b), "0x%02X", cfg_fwd_tgt); lv_label_set_text(bl, b); }
                    lv_obj_center(bl);
                    lv_obj_set_style_text_color(bl, CLR_TEXT, 0);
                }
                y += row_h;
            }
            cfg_dd_count = dd_idx;
            break;
        }

        case mSPI2: case mSPI3: {
            SPI_Config_t *cfg = &ui_spi_cfg[mod_index(cfg_module)];
            uint32_t cfg_vals[] = { cfg->DataSize, cfg->CLKPolarity,
                                    cfg->CLKPhase, cfg->BaudRatePrescaler };
            for (int p = 0; p < 4; p++) {
                lv_obj_t *lb = lv_label_create(cfg_params_cont);
                const char *lbl_txt;
                const uint32_t *tbl;
                int cnt;
                const char *opts;
                switch (p) {
                    case 0: lbl_txt = "Data Size";     tbl = spi_dsize_vals; cnt = 2; opts = spi_dsize_opts; break;
                    case 1: lbl_txt = "CLK Polarity";   tbl = spi_cpol_vals;  cnt = 2; opts = spi_cpol_opts; break;
                    case 2: lbl_txt = "CLK Phase";      tbl = spi_cpha_vals;  cnt = 2; opts = spi_cpha_opts; break;
                    case 3: lbl_txt = "Baud Prescaler"; tbl = spi_baud_vals;  cnt = 8; opts = spi_baud_opts; break;
                    default: continue;
                }
                lv_label_set_text(lb, lbl_txt);
                lv_obj_set_pos(lb, 6, y + 2);
                lv_obj_set_style_text_color(lb, CLR_TEXT, 0);

                lv_obj_t *dd = lv_dropdown_create(cfg_params_cont);
                lv_obj_set_pos(dd, 110, y);
                lv_obj_set_size(dd, 115, 28);
                lv_dropdown_set_options(dd, opts);
                int si = find_val_idx(cfg_vals[p], tbl, cnt);
                lv_dropdown_set_selected(dd, si);
                lv_obj_set_style_bg_color(dd, CLR_BTN, 0);
                lv_obj_set_style_text_color(dd, CLR_TEXT, 0);
                lv_obj_add_event_cb(dd, cb_dd_changed, LV_EVENT_VALUE_CHANGED, NULL);
                cfg_dd_list[cfg_dd_count++] = dd;
                y += row_h;
            }
            break;
        }

        default: break;
    }

    //实时显示波特率
    cfg_freq_lbl = lv_label_create(cfg_params_cont);
    lv_obj_set_pos(cfg_freq_lbl, 6, y + 4);
    lv_obj_set_size(cfg_freq_lbl, UI_W - 20, 16);
    lv_obj_set_style_text_color(cfg_freq_lbl, CLR_GREEN, 0);
    cfg_freq_update();
}

static lv_obj_t* config_page_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);

    mk_top_bar(scr, "Configuration", true);

    //组件选择滑动
    lv_obj_t *mod_lb = lv_label_create(scr);
    lv_label_set_text(mod_lb, "Module:");
    lv_obj_set_pos(mod_lb, 10, 42);
    lv_obj_set_style_text_color(mod_lb, CLR_TEXT, 0);

    lv_obj_t *mod_dd = lv_dropdown_create(scr);
    lv_obj_set_pos(mod_dd, 80, 40);
    lv_obj_set_size(mod_dd, 140, 28);
    lv_dropdown_set_options(mod_dd,
        "CAN1\nCAN2\nUSART1\nUSART2\nIIC1\nIIC2\nSPI2\nSPI3");
    lv_dropdown_set_selected(mod_dd, 0);
    lv_obj_set_style_bg_color(mod_dd, CLR_BTN, 0);
    lv_obj_set_style_text_color(mod_dd, CLR_TEXT, 0);
    lv_obj_add_event_cb(mod_dd, cb_cfg_module_changed, LV_EVENT_VALUE_CHANGED, NULL);

    //可滑动配置参数
    cfg_params_cont = lv_obj_create(scr);
    lv_obj_set_pos(cfg_params_cont, 5, 75);
    lv_obj_set_size(cfg_params_cont, UI_W - 10, 180);
    lv_obj_set_style_bg_color(cfg_params_cont, CLR_PANEL, 0);
    lv_obj_set_style_border_width(cfg_params_cont, 0, 0);
    lv_obj_set_style_radius(cfg_params_cont, 4, 0);
    lv_obj_set_style_pad_all(cfg_params_cont, 0, 0);
    lv_obj_add_flag(cfg_params_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(cfg_params_cont, LV_SCROLLBAR_MODE_AUTO);

    //状态反馈
    lv_obj_t *status_lbl = lv_label_create(scr);
    lv_obj_set_pos(status_lbl, 10, 260);
    lv_obj_set_size(status_lbl, UI_W - 20, 14);
    lv_obj_set_style_text_color(status_lbl, CLR_GREEN, 0);

    //确认按钮
    lv_obj_t *cfm_btn = lv_btn_create(scr);
    lv_obj_set_size(cfm_btn, UI_W - 20, 35);
    lv_obj_align(cfm_btn, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_bg_color(cfm_btn, lv_color_hex(0x006644), 0);
    lv_obj_set_style_radius(cfm_btn, 6, 0);
    lv_obj_add_event_cb(cfm_btn, cb_cfg_confirm, LV_EVENT_CLICKED, status_lbl);

    lv_obj_t *cfm_lbl = lv_label_create(cfm_btn);
    lv_label_set_text(cfm_lbl, "Confirm Config");
    lv_obj_center(cfm_lbl);
    lv_obj_set_style_text_color(cfm_lbl, CLR_TEXT, 0);

    cfg_params_rebuild();

    return scr;
}

/**
  *@brief UI初始化
  *@param NULL
  *@retval NULL
  */
void UI_Init(void)
{
    set_defaults();
    memset(msg_buf, 0, sizeof(msg_buf));
    msg_head = 0;
    msg_cnt  = 0;
    fwd_active = false;

    //创建主页面
    cur_scr  = main_page_create();
    cur_page = PAGE_MAIN;
    lv_scr_load(cur_scr);
}
