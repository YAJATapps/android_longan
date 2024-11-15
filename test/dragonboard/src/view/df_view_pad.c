/*
 * \file        df_view.c
 * \brief
 *
 * \version     1.0.0
 * \date        2012年05月31日
 * \author      James Deng <csjamesdeng@allwinnertech.com>
 * \Descriptions:
 *      create the inital version
 *
 * \version     1.1.0
 * \date        2012年09月26日
 * \author      Martin <zhengjiewen@allwinnertech.com>
 * \Descriptions:
 *      add some new features:
 *      1.wifi hotpoint display with ssid and single strongth value.
 *      2.add mic power bar to the ui
 *      3.make the display automatically switch between lcd and hdmi
 *      4.the ui will automatically adjust under different test config
 * Copyright (c) 2012 Allwinner Technology. All Rights Reserved.
 *
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <directfb.h>
#include <linux/input.h>
#include <dirent.h>

#if defined (_CONFIG_LINUX_5_4)
#include <sun50iw10p_display.h>
#elif defined (_SUN8IW6P) || (_SUN50IW1P) || (_SUN50IW3P) || (_SUN8IW15P) || (_SUN50IW6P) || (_SUN8IW7P)
#include "sun8iw6p_display.h"
#elif defined (_SUN8IW5P) || (_SUN9IW1P)
#include "sun8iw5p_display.h"
#elif defined (_SUN50IW10P)
#include "sunxi_display2.h"
#else

/* Universal implement */
#include "universal_display.h"
#endif

#include "dragonboard.h"
#include "script.h"
#include "view.h"
#include "test_case.h"
#include "tp_track.h"
#include "cameratest.h"
#include "df_view.h"
#include "flashlight.h"

extern int get_auto_testcases_number(void);
extern int get_manual_testcases_number(void);

static int reboot_button_init(void);
static int poweroff_button_init(void);
static int handle_confirm_init(void);
static int confirm_button_redraw(int color);
static int handle_motor_init(void);
static int motor_button_redraw(int color);
static int handle_flashlight_init(void);
static int flashlight_button_redraw(int color);
static int handle_tp_pass_init(void);
static int tp_pass_button_redraw(int color);
static int handle_tp_fail_init(void);
static int tp_fail_button_redraw(int color);
static int handle_gsensor_pass_init(void);
static int gsensor_pass_button_redraw(int color);
static int handle_gsensor_fail_init(void);
static int gsensor_fail_button_redraw(int color);
static int handle_mic_start_init(void);
static int mic_start_button_redraw(int color);
static int handle_mic_pass_init(void);
static int mic_pass_button_redraw(int color);
static int handle_mic_fail_init(void);
static int mic_fail_button_redraw(int color);
static int check_auto_testcases_allpass(void);

/* Universal implement */
__attribute__((weak)) int camera_test_init(int x,int y,int width,int height) { return 0;}
__attribute__((weak)) int get_camera_cnt(void) { return 0;}
__attribute__((weak)) int switch_camera(void) { return 0;}

#define DFBCHECK(x...) \
    do {                                                            \
        DFBResult err = x;                                          \
        if (err != DFB_OK) {                                        \
            fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );  \
            DirectFBErrorFatal( #x, err );                          \
        }                                                           \
    } while (0)

#define CLAMP(x, low, high) \
    (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define DATADIR                         "/dragonboard/fonts"

#define FONT16_HEIGHT                   16
#define FONT20_HEIGHT                   20
#define FONT24_HEIGHT                   24
#define FONT28_HEIGHT                   28
#define FONT48_HEIGHT                   48
#define DEFAULT_FONT_SIZE               FONT20_HEIGHT
#define MENU_HEIGHT                     48
#define ITEM_HEIGHT                     28
#define BUTTON_WIDTH                    96
#define BUTTON_HEIGHT                   MENU_HEIGHT
#define HEIGHT_ADJUST                   (2 * ITEM_HEIGHT)

#define AUTO_MENU_NAME                  "自动测试项"
#define MANUAL_MENU_NAME                "手动测试项"
#define WIFI_MENU_NAME                  "Wi-Fi热点列表"
#define CLEAR_BUTTON_NAME               "清 屏"
#define SWITCH_BUTTON_NAME              "摄像头切换"
#define REBOOT_BUTTON_NAME              "重 启"
#define POWEROFF_BUTTON_NAME            "关 机"
#define TP_DISPLAY_NAME                 "触摸"
#define CONFIRM_BUTTON_NAME             "AllPass"
#define MOTOR_BUTTON_NAME               "MotorPass"
#define TP_PASS_BUTTON_NAME             "触摸Pass"
#define TP_FAIL_BUTTON_NAME             "触摸Fail"
#define GSENSOR_PASS_BUTTON_NAME        "重力Pass"
#define GSENSOR_FAIL_BUTTON_NAME        "重力Fail"
#define MIC_START_BUTTON_NAME           "音频Start"
#define MIC_PASS_BUTTON_NAME            "音频Pass"
#define MIC_FAIL_BUTTON_NAME            "音频Fail"
#define FLASHLIGHT_BUTTON_NAME          "闪光灯"

#define BUILDIN_TC_ID_TP                -1

#define MIC_TITLE_HEIGHT                MENU_HEIGHT
#define MIC_POWER_BAR_HEIGHT_PERCENT    10
#define MIC_POWER_BAR_WITH_SHIFT        5
#define MIC_POWER_BAR_WITH_SHIFT_TOTAL  (MIC_POWER_BAR_WITH_SHIFT-1)

IDirectFB                       *dfb;
IDirectFBDisplayLayer           *layer;
DFBDisplayLayerConfig           layer_config;
DFBGraphicsDeviceDescription    gdesc;

/* font declaration */
IDirectFBFont                  *font16;
IDirectFBFont                  *font20;
IDirectFBFont                  *font24;
IDirectFBFont                  *font28;
IDirectFBFont                  *font48;
IDirectFBFont                  *font;

static int font_size;

/* input device: tp */
static IDirectFBInputDevice    *tp_dev = NULL;

/* input interfaces: device and its buffer */
static IDirectFBEventBuffer    *events;

static int mTouchScreenWidth;
static int mTouchScreenHeight;

static pthread_t evt_tid;

static int df_view_id;

static pthread_t soundtest_tid;
static int soundtest_status_flag;
static FILE *soundtest_status;

static struct list_head auto_tc_list;
static struct list_head manual_tc_list;

struct color_rgb color_table[9] =
{
    {0xff, 0xff, 0xff},
    {0xff, 0xff, 0x00},
    {0x00, 0xff, 0x00},
    {0x00, 0xff, 0xff},
    {0xff, 0x00, 0xff},
    {0xff, 0x00, 0x00},
    {0x00, 0x00, 0xff},
    {0x00, 0x00, 0x00},
    {0x08, 0x46, 0x84}
};

static int item_init_bgcolor;
static int item_init_fgcolor;
static int item_ok_bgcolor;
static int item_ok_fgcolor;
static int item_fail_bgcolor;
static int item_fail_fgcolor;

static char pass_str[12];
static char fail_str[12];

static struct item_data tp_data;

struct df_window
{
    IDirectFBWindow        *window;
    IDirectFBSurface       *surface;
    DFBWindowDescription    desc;
    int                     bgcolor;
};

struct df_window_menu
{
    char name[36];
    int width;
    int height;
    int bgcolor;
    int fgcolor;
};

struct df_button
{
    char name[20];
    int x;
    int y;
    int width;
    int height;
    int bgcolor;
    int fgcolor;
    int bdcolor;
};

static struct df_window         auto_window;
static struct df_window_menu    auto_window_menu;

static struct df_window         manual_window;
static struct df_window_menu    manual_window_menu;

static struct df_window         video_window;

static struct df_window         misc_window;

static struct df_window         wifi_window;
static struct df_window_menu    wifi_window_menu;

static struct df_window         mic1_window;
static struct df_window         mic2_window;

struct one_wifi_hot_point
{
    char ssid[64];
    char single_level_db[16];
    int  single_level;
    int x;
    int y;
    int width;
    int height;
    int bgcolor;
    int fgcolor;
    struct list_head list;
};

struct wifi_hot_point_display_t
{
    int current_display_position_x;
    int current_display_position_y;
    int one_point_display_width;
    int one_point_display_height;
    int total_point_can_be_display;
    int total_point_have_displayed;
    int total_point_searched;
};

static struct wifi_hot_point_display_t wifi_hot_point_display;
static struct list_head wifi_list;

struct mic_power_bar_t
{
  int BarColor[2];
  int XOff, YOff;
  int width,height;
  int v;       //desire volume
  int Min, Max;//the volume space
  char bar_name[16];
  struct df_window * mic_window;
};

static struct mic_power_bar_t mic1_power_bar;
static struct mic_power_bar_t mic2_power_bar;

static int mic_activated = 0;
static int mic1_used = 0;
static int mic2_used = 0;
static int camera_activated = 0;
static int motor_activated = 0;
static int flashlight_activated = 0;
static int gsensor_category = 0;
static int tp_type= -1;
static int tp_test_type;
static int auto_testcases_allpass = 0;

static struct df_button         clear_button;
static struct df_button         switch_button;
static struct df_button         reboot_button;
static struct df_button         poweroff_button;
static struct df_button         confirm_button;
static struct df_button         motor_button;
static struct df_button         tp_pass_button;
static struct df_button         tp_fail_button;
static struct df_button         gsensor_pass_button;
static struct df_button         gsensor_fail_button;
static struct df_button         mic_start_button;
static struct df_button         mic_pass_button;
static struct df_button         mic_fail_button;
static struct df_button         flashlight_button;

#if defined (_CONFIG_LINUX_5_4)
enum disp_output_type disp_output_type_t;
#else
disp_output_type disp_output_type_t;
#endif

static int check_auto_testcases_allpass(void)
{
    struct list_head *pos, *tc_list;
    tc_list = &auto_tc_list;
    list_for_each (pos, tc_list)
    {
        struct df_item_data *temp = list_entry(pos, struct df_item_data, list);
        if (temp->status == 0) {
            continue;
        }
        else {
            return 1;
        }
    }
    if (auto_testcases_allpass == 0) {
        auto_testcases_allpass = 1;
        db_debug("-------auto test case all pass-------\n");
        handle_confirm_init();//draw AllPass button,only once
        db_debug("-------do handle_confirm_init()------\n");
    }
    return 0;
}

static int auto_window_init(void)
{
    db_debug("auto test case window init...\n");
    auto_window.desc.flags = (DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_CAPS);

    auto_window.desc.posx = 0;
    // auto_window.desc.posy   = (layer_config.height >> 1) + height_adjust;
    auto_window.desc.posy = manual_window.desc.height;
    if (mic_activated) {
        auto_window.desc.width = (layer_config.width >> 1)-(layer_config.width>>MIC_POWER_BAR_WITH_SHIFT);
    }
    else {
        auto_window.desc.width = layer_config.width >> 1;
    }
    auto_window.desc.height = MENU_HEIGHT+ITEM_HEIGHT*get_auto_testcases_number();
    auto_window.desc.caps   = DWCAPS_ALPHACHANNEL;

    DFBCHECK(layer->CreateWindow(layer, &auto_window.desc, &auto_window.window));
    auto_window.window->GetSurface(auto_window.window, &auto_window.surface);
    auto_window.window->SetOpacity(auto_window.window, 0xff);
    auto_window.window->SetOptions(auto_window.window, DWOP_KEEP_POSITION);

    auto_window.bgcolor = item_init_bgcolor;
    DFBCHECK(auto_window.surface->SetColor(auto_window.surface,
                                           color_table[auto_window.bgcolor].r,
                                           color_table[auto_window.bgcolor].g,
                                           color_table[auto_window.bgcolor].b, 0xff));
    DFBCHECK(auto_window.surface->FillRectangle(auto_window.surface, 0, 0,
                auto_window.desc.width, auto_window.desc.height));

    /* draw menu */
    if (script_fetch("df_view", "auto_menu_name", (int *)auto_window_menu.name, 8)) {
        strcpy(auto_window_menu.name, AUTO_MENU_NAME);
    }

    auto_window_menu.width  = auto_window.desc.width;
    auto_window_menu.height = MENU_HEIGHT;

    if (script_fetch("df_view", "menu_bgcolor", &auto_window_menu.bgcolor, 1) ||
            auto_window_menu.bgcolor < COLOR_WHITE_IDX || auto_window_menu.bgcolor > COLOR_BLACK_IDX) {
        auto_window_menu.bgcolor = COLOR_YELLOW_IDX;
    }

    if (script_fetch("df_view", "menu_fgcolor", &auto_window_menu.fgcolor, 1) ||
            auto_window_menu.fgcolor < COLOR_WHITE_IDX || auto_window_menu.fgcolor > COLOR_BLACK_IDX) {
        auto_window_menu.fgcolor = COLOR_BLACK_IDX;
    }

    DFBCHECK(auto_window.surface->SetColor(auto_window.surface,
                                           color_table[auto_window_menu.bgcolor].r,
                                           color_table[auto_window_menu.bgcolor].g,
                                           color_table[auto_window_menu.bgcolor].b, 0xff));
    DFBCHECK(auto_window.surface->FillRectangle(auto_window.surface, 0, 0, auto_window_menu.width, auto_window_menu.height));
    DFBCHECK(auto_window.surface->SetFont(auto_window.surface, font));
    DFBCHECK(auto_window.surface->SetColor(auto_window.surface,
                                           color_table[auto_window_menu.fgcolor].r,
                                           color_table[auto_window_menu.fgcolor].g,
                                           color_table[auto_window_menu.fgcolor].b, 0xff));
    DFBCHECK(auto_window.surface->DrawString(auto_window.surface, auto_window_menu.name, -1, 4,
                MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8, DSTF_LEFT));

    auto_window.surface->Flip(auto_window.surface, NULL, 0);

    return 0;
}

static int clear_button_init(void)
{
    if (script_fetch("df_view", "clear_button_name", (int *)clear_button.name, 4)) {
        strcpy(clear_button.name, CLEAR_BUTTON_NAME);
    }

    clear_button.width  = BUTTON_WIDTH;
    clear_button.height = BUTTON_HEIGHT;
    clear_button.x      = manual_window.desc.width - clear_button.width;
    clear_button.y      = 0;

    clear_button.bdcolor = COLOR_RED_IDX;
    clear_button.bgcolor = COLOR_BLUE_IDX;
    clear_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(manual_window.surface->SetColor(manual_window.surface,
                                             color_table[clear_button.bdcolor].r,
                                             color_table[clear_button.bdcolor].g,
                                             color_table[clear_button.bdcolor].b, 0xff));
    DFBCHECK(manual_window.surface->DrawLine(manual_window.surface, clear_button.x,
                                                                    clear_button.y,
                                                                    clear_button.x,
                                                                    clear_button.y + clear_button.height - 1));
    DFBCHECK(manual_window.surface->DrawLine(manual_window.surface, clear_button.x,
                                                                    clear_button.y,
                                                                    clear_button.x + clear_button.width - 1,
                                                                    clear_button.y));
    DFBCHECK(manual_window.surface->DrawLine(manual_window.surface, clear_button.x + clear_button.width - 1,
                                                                    clear_button.y,
                                                                    clear_button.x + clear_button.width - 1,
                                                                    clear_button.y + clear_button.height - 1));
    DFBCHECK(manual_window.surface->DrawLine(manual_window.surface, clear_button.x,
                                                                    clear_button.y + clear_button.height - 1,
                                                                    clear_button.x + clear_button.width - 1,
                                                                    clear_button.y + clear_button.height - 1));

    DFBCHECK(manual_window.surface->SetColor(manual_window.surface,
                                             color_table[clear_button.bgcolor].r,
                                             color_table[clear_button.bgcolor].g,
                                             color_table[clear_button.bgcolor].b, 0xff));
    DFBCHECK(manual_window.surface->FillRectangle(manual_window.surface, clear_button.x + 1,
                                                                         clear_button.y + 1,
                                                                         clear_button.width - 2,
                                                                         clear_button.height - 2));

    DFBCHECK(manual_window.surface->SetFont(manual_window.surface, font));
    DFBCHECK(manual_window.surface->SetColor(manual_window.surface,
                                             color_table[clear_button.fgcolor].r,
                                             color_table[clear_button.fgcolor].g,
                                             color_table[clear_button.fgcolor].b, 0xff));
    DFBCHECK(manual_window.surface->DrawString(manual_window.surface, clear_button.name, -1, clear_button.x + (clear_button.width >> 1),
                MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8, DSTF_CENTER));

    manual_window.surface->Flip(manual_window.surface, NULL, 0);

    return 0;
}

static int clear_button_redraw(uint8_t color)
{
    DFBCHECK(manual_window.surface->SetColor(manual_window.surface,
                                             color_table[color].r,
                                             color_table[color].g,
                                             color_table[color].b, 0xff));
    DFBCHECK(manual_window.surface->FillRectangle(manual_window.surface, clear_button.x + 1,
                                                                         clear_button.y + 1,
                                                                         clear_button.width - 2,
                                                                         clear_button.height - 2));

    DFBCHECK(manual_window.surface->SetFont(manual_window.surface, font));
    DFBCHECK(manual_window.surface->SetColor(manual_window.surface,
                                             color_table[clear_button.fgcolor].r,
                                             color_table[clear_button.fgcolor].g,
                                             color_table[clear_button.fgcolor].b, 0xff));
    DFBCHECK(manual_window.surface->DrawString(manual_window.surface, clear_button.name, -1, clear_button.x + (clear_button.width >> 1),
                MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8, DSTF_CENTER));

    manual_window.surface->Flip(manual_window.surface, NULL, 0);

    return 0;
}

static int manual_window_init(void)
{
    db_debug("manual test case window init...\n");

    manual_window.desc.flags  = (DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_CAPS);
    manual_window.desc.posx   = 0;
    manual_window.desc.posy   = 0;
    if (mic_activated) {
        manual_window.desc.width = (layer_config.width >> 1)-(layer_config.width>>MIC_POWER_BAR_WITH_SHIFT);
    }
    else {
        manual_window.desc.width = layer_config.width >> 1;
    }
    // manual_window.desc.height = (layer_config.height >> 1) + height_adjust;    /* get draw type */
    if (disp_output_type_t==DISP_OUTPUT_TYPE_LCD) {
        manual_window.desc.height = MENU_HEIGHT+ITEM_HEIGHT*(1+get_manual_testcases_number());
    }
    else {
        manual_window.desc.height = MENU_HEIGHT+ITEM_HEIGHT*get_manual_testcases_number();
    }

    db_debug("manual_window.desc.height=%d\n",manual_window.desc.height);
    manual_window.desc.caps = DWCAPS_ALPHACHANNEL;

    DFBCHECK(layer->CreateWindow(layer, &manual_window.desc, &manual_window.window));
    manual_window.window->GetSurface(manual_window.window, &manual_window.surface);
    manual_window.window->SetOpacity(manual_window.window, 0xff);
    manual_window.window->SetOptions(manual_window.window, DWOP_KEEP_POSITION);

    manual_window.bgcolor = item_init_bgcolor;
    DFBCHECK(manual_window.surface->SetColor(manual_window.surface,
                                             color_table[manual_window.bgcolor].r,
                                             color_table[manual_window.bgcolor].g,
                                             color_table[manual_window.bgcolor].b, 0xff));
    DFBCHECK(manual_window.surface->FillRectangle(manual_window.surface, 0, 0,
                manual_window.desc.width, manual_window.desc.height));

    /* draw menu */
    if (script_fetch("df_view", "manual_menu_name", (int *)manual_window_menu.name, 8)) {
        strcpy(manual_window_menu.name, MANUAL_MENU_NAME);
    }

    manual_window_menu.width  = manual_window.desc.width;
    manual_window_menu.height = MENU_HEIGHT;

    if (script_fetch("df_view", "menu_bgcolor", &manual_window_menu.bgcolor, 1) ||
            manual_window_menu.bgcolor < COLOR_WHITE_IDX || manual_window_menu.bgcolor > COLOR_BLACK_IDX) {
        manual_window_menu.bgcolor = COLOR_YELLOW_IDX;
    }

    if (script_fetch("df_view", "menu_fgcolor", &manual_window_menu.fgcolor, 1) ||
            manual_window_menu.fgcolor < COLOR_WHITE_IDX || manual_window_menu.fgcolor > COLOR_BLACK_IDX) {
        manual_window_menu.fgcolor = COLOR_BLACK_IDX;
    }

    DFBCHECK(manual_window.surface->SetColor(manual_window.surface,
                                             color_table[manual_window_menu.bgcolor].r,
                                             color_table[manual_window_menu.bgcolor].g,
                                             color_table[manual_window_menu.bgcolor].b, 0xff));
    DFBCHECK(manual_window.surface->FillRectangle(manual_window.surface, 0, 0, manual_window_menu.width, manual_window_menu.height));
    DFBCHECK(manual_window.surface->SetFont(manual_window.surface, font));
    DFBCHECK(manual_window.surface->SetColor(manual_window.surface,
                                             color_table[manual_window_menu.fgcolor].r,
                                             color_table[manual_window_menu.fgcolor].g,
                                             color_table[manual_window_menu.fgcolor].b, 0xff));
    DFBCHECK(manual_window.surface->DrawString(manual_window.surface, manual_window_menu.name, -1, 4,
                MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8, DSTF_LEFT));

    /* draw clear button */
    if (disp_output_type_t==DISP_OUTPUT_TYPE_LCD) {
        clear_button_init();
    }
    manual_window.surface->Flip(manual_window.surface, NULL, 0);

    return 0;
}


static int switch_button_init(void)
{
    db_debug("camera switch button init...\n");

    if (script_fetch("df_view", "swicth_button_name", (int *)switch_button.name, 4)) {
        strcpy(switch_button.name, SWITCH_BUTTON_NAME);
    }

    switch_button.width  = BUTTON_WIDTH;
    switch_button.height = BUTTON_HEIGHT;
    switch_button.x      = (misc_window.desc.width - switch_button.width) / 2;
    switch_button.y      =  switch_button.height*2;

    switch_button.bdcolor = COLOR_RED_IDX;
    switch_button.bgcolor = COLOR_BLUE_IDX;
    switch_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[switch_button.bdcolor].r,
                                           color_table[switch_button.bdcolor].g,
                                           color_table[switch_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, switch_button.x,
                                                                switch_button.y,
                                                                switch_button.x,
                                                                switch_button.y + switch_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, switch_button.x,
                                                                switch_button.y,
                                                                switch_button.x + switch_button.width - 1,
                                                                switch_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, switch_button.x + switch_button.width - 1,
                                                                switch_button.y,
                                                                switch_button.x + switch_button.width - 1,
                                                                switch_button.y + switch_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, switch_button.x,
                                                                switch_button.y + switch_button.height - 1,
                                                                switch_button.x + switch_button.width - 1,
                                                                switch_button.y + switch_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[switch_button.bgcolor].r,
                                           color_table[switch_button.bgcolor].g,
                                           color_table[switch_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, switch_button.x + 1,
                                                                     switch_button.y + 1,
                                                                     switch_button.width - 2,
                                                                     switch_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[switch_button.fgcolor].r,
                                           color_table[switch_button.fgcolor].g,
                                           color_table[switch_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, switch_button.name, -1, switch_button.x + (switch_button.width >> 1),
                (switch_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}


static int switch_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[color].r,
                                           color_table[color].g,
                                           color_table[color].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, switch_button.x + 1,
                                                                     switch_button.y + 1,
                                                                     switch_button.width - 2,
                                                                     switch_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[switch_button.fgcolor].r,
                                           color_table[switch_button.fgcolor].g,
                                           color_table[switch_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, switch_button.name, -1, switch_button.x + (switch_button.width >> 1),
                (switch_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}


static int reboot_button_init(void)
{
    db_debug("reboot button init...\n");

    if (script_fetch("df_view", "reboot_button_name", (int *)reboot_button.name, 4)) {
        strcpy(reboot_button.name, REBOOT_BUTTON_NAME);
    }

    reboot_button.width  = BUTTON_WIDTH;
    reboot_button.height = BUTTON_HEIGHT;
    reboot_button.x      = (misc_window.desc.width - reboot_button.width) / 2;
    reboot_button.y      = 0;

    reboot_button.bdcolor = COLOR_RED_IDX;
    reboot_button.bgcolor = COLOR_BLUE_IDX;
    reboot_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[reboot_button.bdcolor].r,
                                           color_table[reboot_button.bdcolor].g,
                                           color_table[reboot_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, reboot_button.x,
                                                                reboot_button.y,
                                                                reboot_button.x,
                                                                reboot_button.y + reboot_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, reboot_button.x,
                                                                reboot_button.y,
                                                                reboot_button.x + reboot_button.width - 1,
                                                                reboot_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, reboot_button.x + reboot_button.width - 1,
                                                                reboot_button.y,
                                                                reboot_button.x + reboot_button.width - 1,
                                                                reboot_button.y + reboot_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, reboot_button.x,
                                                                reboot_button.y + reboot_button.height - 1,
                                                                reboot_button.x + reboot_button.width - 1,
                                                                reboot_button.y + reboot_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[reboot_button.bgcolor].r,
                                           color_table[reboot_button.bgcolor].g,
                                           color_table[reboot_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, reboot_button.x + 1,
                                                                     reboot_button.y + 1,
                                                                     reboot_button.width - 2,
                                                                     reboot_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[reboot_button.fgcolor].r,
                                           color_table[reboot_button.fgcolor].g,
                                           color_table[reboot_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, reboot_button.name, -1, reboot_button.x + (reboot_button.width >> 1),
                MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8, DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}


static int reboot_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[color].r,
                                           color_table[color].g,
                                           color_table[color].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, reboot_button.x + 1,
                                                                     reboot_button.y + 1,
                                                                     reboot_button.width - 2,
                                                                     reboot_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[reboot_button.fgcolor].r,
                                           color_table[reboot_button.fgcolor].g,
                                           color_table[reboot_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, reboot_button.name, -1, reboot_button.x + (reboot_button.width >> 1),
                MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8, DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}


static int poweroff_button_init(void)
{
    db_debug("poweroff button init...\n");

    if (script_fetch("df_view", "poweroff_button_name", (int *)poweroff_button.name, 4)) {
        strcpy(poweroff_button.name, POWEROFF_BUTTON_NAME);
    }

    poweroff_button.width  = BUTTON_WIDTH;
    poweroff_button.height = BUTTON_HEIGHT;
    poweroff_button.x      = (misc_window.desc.width/3-poweroff_button.width)/2;
    poweroff_button.y      = 0;

    poweroff_button.bdcolor = COLOR_RED_IDX;
    poweroff_button.bgcolor = COLOR_BLUE_IDX;
    poweroff_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[poweroff_button.bdcolor].r,
                                           color_table[poweroff_button.bdcolor].g,
                                           color_table[poweroff_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, poweroff_button.x,
                                                                poweroff_button.y,
                                                                poweroff_button.x,
                                                                poweroff_button.y + poweroff_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, poweroff_button.x,
                                                                poweroff_button.y,
                                                                poweroff_button.x + poweroff_button.width - 1,
                                                                poweroff_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, poweroff_button.x + poweroff_button.width - 1,
                                                                poweroff_button.y,
                                                                poweroff_button.x + poweroff_button.width - 1,
                                                                poweroff_button.y + poweroff_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, poweroff_button.x,
                                                                poweroff_button.y + poweroff_button.height - 1,
                                                                poweroff_button.x + poweroff_button.width - 1,
                                                                poweroff_button.y + poweroff_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[poweroff_button.bgcolor].r,
                                           color_table[poweroff_button.bgcolor].g,
                                           color_table[poweroff_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, poweroff_button.x + 1,
                                                                     poweroff_button.y + 1,
                                                                     poweroff_button.width - 2,
                                                                     poweroff_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[poweroff_button.fgcolor].r,
                                           color_table[poweroff_button.fgcolor].g,
                                           color_table[poweroff_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, poweroff_button.name, -1, poweroff_button.x + (poweroff_button.width >> 1),
                MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8, DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}


static int poweroff_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[color].r,
                                           color_table[color].g,
                                           color_table[color].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, poweroff_button.x + 1,
                                                                     poweroff_button.y + 1,
                                                                     poweroff_button.width - 2,
                                                                     poweroff_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[poweroff_button.fgcolor].r,
                                           color_table[poweroff_button.fgcolor].g,
                                           color_table[poweroff_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, poweroff_button.name, -1, poweroff_button.x + (poweroff_button.width >> 1),
                MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8, DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int video_window_init(void)
{
    int x,y,height,width;

    if (!camera_activated) {
        return 0;
    }

    if (mic_activated) {
        x= (layer_config.width >> 1)+(layer_config.width>>MIC_POWER_BAR_WITH_SHIFT);
        width  = (layer_config.width >> 1)-(layer_config.width>>MIC_POWER_BAR_WITH_SHIFT);
    }
    else {
        x = (layer_config.width >> 1);
        width = (layer_config.width >> 1);
    }

    y = 0;
    height=layer_config.height>>1;

    video_window.desc.flags  = (DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_CAPS);
    video_window.desc.posx   = x;
    video_window.desc.posy   = y;
    video_window.desc.width  = width;
    video_window.desc.height = height;
    video_window.desc.caps   = DWCAPS_ALPHACHANNEL;

    DFBCHECK(layer->CreateWindow(layer, &video_window.desc, &video_window.window));
    video_window.window->GetSurface(video_window.window, &video_window.surface);
    video_window.window->SetOpacity(video_window.window, 0xff);
    video_window.window->SetOptions(video_window.window, DWOP_KEEP_POSITION);

    int i,idx = 0;
    for (i = 0; i < video_window.desc.width; i+=video_window.desc.width>>3) {
        DFBCHECK(video_window.surface->SetColor(video_window.surface,
                                                color_table[idx].r,
                                                color_table[idx].g,
                                                color_table[idx].b, 0xff));
        DFBCHECK(video_window.surface->FillRectangle(video_window.surface, i, 0,
                video_window.desc.width>>3, video_window.desc.height));
        idx++;
    }

    DFBCHECK(video_window.surface->SetColor(video_window.surface,
                                            color_table[COLOR_WHITE_IDX].r,
                                            color_table[COLOR_WHITE_IDX].g,
                                            color_table[COLOR_WHITE_IDX].b, 0xff));
    video_window.surface->DrawLine(video_window.surface,0,video_window.desc.height-1,\
                                    video_window.desc.width,video_window.desc.height-1);
    video_window.surface->Flip(video_window.surface, NULL, 0);

    if (access("/tmp/camera_insmod_done", F_OK)) {
        db_error("camera module insmod incompletion\n");
        return 0;
    }
    if (camera_test_init(x,y,width,height)) {
        return -1;
    }
    if (get_camera_cnt() > 1) {
        switch_button_init();
    }
    return 0;
}

void* wifi_hot_point_proccess(void* argv)
{
    FILE *wifi_pipe;
    char buffer[64];
    char* ssid;
    char* single_level_db;
    int single;
    int one_level_width;
    int current_pozition_x;
    struct list_head *pos;
    IDirectFBSurface *surface;
    struct one_wifi_hot_point *p_hot_point;
    int wifi_signal_low_threshold = 0;

    if (script_fetch("wifi", "signal_low_threshold", &wifi_signal_low_threshold, -60)) {
        wifi_signal_low_threshold = -60;
    }
    printf("this is the wifi_hot_point_proccess thread...\n");
    wifi_pipe = fopen(WIFI_PIPE_NAME, "r");
    setlinebuf(wifi_pipe);
    while (1) {
        get_wifi_point_data:
        if (fgets(buffer, sizeof(buffer), wifi_pipe) == NULL) {
            continue;
        }
        if (0 == strcmp("######\n",buffer)) {

            // db_msg("one turn complete...\n");
            // db_msg("have_searched=%d\n",wifi_hot_point_display.total_point_searched);
            // db_msg("can_be_display=%d\n",wifi_hot_point_display.total_point_can_be_display);
            // db_msg("have_displayed=%d\n",wifi_hot_point_display.total_point_have_displayed);

            list_for_each(pos, &wifi_list) {
            struct one_wifi_hot_point *temp = list_entry(pos, struct one_wifi_hot_point, list);
            free(temp );
            }
            list_del_init(&wifi_list);

            if (wifi_hot_point_display.total_point_have_displayed<wifi_hot_point_display.total_point_can_be_display) {
                int wifi_hot_point_not_diplay_number;
                wifi_hot_point_not_diplay_number=wifi_hot_point_display.total_point_can_be_display-\
                wifi_hot_point_display.total_point_have_displayed;
                surface = wifi_window.surface;
                //get the start of the none display area
                wifi_hot_point_display.current_display_position_y=MENU_HEIGHT+\
                                      wifi_hot_point_display.total_point_have_displayed*wifi_hot_point_display.one_point_display_height;

                DFBCHECK(surface->SetColor(surface,
                                  color_table[item_init_bgcolor].r,
                                  color_table[item_init_bgcolor].g,
                                  color_table[item_init_bgcolor].b, 0xff));
                //because we have a line at coordinate(0,0)-(0,height),so ,let's clear from 1
                DFBCHECK(surface->FillRectangle(surface,1,\
                    wifi_hot_point_display.current_display_position_y,wifi_hot_point_display.one_point_display_width,\
                    wifi_hot_point_not_diplay_number*wifi_hot_point_display.one_point_display_height));
                surface->Flip(surface, NULL, 0);
            }
            memset(&wifi_hot_point_display,0,sizeof(struct wifi_hot_point_display_t));

            wifi_hot_point_display.current_display_position_x=4;
            wifi_hot_point_display.current_display_position_y=MENU_HEIGHT;
            wifi_hot_point_display.one_point_display_width   =wifi_window_menu.width;
            wifi_hot_point_display.one_point_display_height  =ITEM_HEIGHT;
            wifi_hot_point_display.total_point_have_displayed=0;
            wifi_hot_point_display.total_point_searched      =0;
            wifi_hot_point_display.total_point_can_be_display=(wifi_window.desc.height-\
            wifi_window_menu.height)/wifi_hot_point_display.one_point_display_height;
            goto get_wifi_point_data;
        }

        ssid=strtok(buffer, ":");
        single_level_db=strtok(NULL, "\n");
        if (ssid==NULL||single_level_db==NULL) {
            goto get_wifi_point_data;
        }
        //add for wifi hot point.
        surface = wifi_window.surface;

        list_for_each(pos, &wifi_list) {
            p_hot_point = list_entry(pos, struct one_wifi_hot_point, list);
            if ( 0 ==strcmp(p_hot_point->ssid,ssid)) {//cmp the ssid
                // wifi hot point exist in the list ,so return.
                // db_msg("wifi:%s already exist\n",p_hot_point->ssid);
                goto get_wifi_point_data;
            }
        }
       //wifi hot point not exist in the list ,so add it
        wifi_hot_point_display.total_point_searched++;
        if (wifi_hot_point_display.total_point_have_displayed>=wifi_hot_point_display.total_point_can_be_display) {
            goto get_wifi_point_data; //  display space now isn't enough
        }

        wifi_hot_point_display.total_point_have_displayed++;
        p_hot_point= malloc(sizeof(struct one_wifi_hot_point));
        strcpy(p_hot_point->ssid,ssid);
        strcpy(p_hot_point->single_level_db,single_level_db);

        single=atoi(single_level_db);
        if (single<0  &&single>=-55) p_hot_point->single_level=5;
        if (single<-55&&single>=-60) p_hot_point->single_level=4;
        if (single<-60&&single>=-65) p_hot_point->single_level=3;
        if (single<-65&&single>=-70) p_hot_point->single_level=2;
        if (single<-70&&single>=-75) p_hot_point->single_level=1;
        if (p_hot_point->single_level>5 || p_hot_point->single_level<1) {
            p_hot_point->single_level=1;
        }
        //  db_msg("wifi:ssid=%s,single_level_db=%s\n",wifi_hot_point->ssid,wifi_hot_point->single_level_db);
        p_hot_point->bgcolor=item_init_bgcolor;
        p_hot_point->fgcolor=item_init_fgcolor;
        p_hot_point->x      =wifi_hot_point_display.current_display_position_x;
        p_hot_point->y      =wifi_hot_point_display.current_display_position_y;
        p_hot_point->width  =wifi_hot_point_display.one_point_display_width;
        p_hot_point->height=wifi_hot_point_display.one_point_display_height;
        list_add_tail(&(p_hot_point->list), &wifi_list);

        DFBCHECK(surface->SetColor(surface,
                                  color_table[p_hot_point->bgcolor].r,
                                  color_table[p_hot_point->bgcolor].g,
                                  color_table[p_hot_point->bgcolor].b, 0xff));
        DFBCHECK(surface->FillRectangle(surface, p_hot_point->x, p_hot_point->y,\
        p_hot_point->width, p_hot_point->height));

        DFBCHECK(surface->SetFont(surface, font24));

        DFBCHECK(surface->SetColor(surface,
                                  color_table[p_hot_point->fgcolor].r,
                                  color_table[p_hot_point->fgcolor].g,
                                  color_table[p_hot_point->fgcolor].b, 0xff));


        //draw ssid
        DFBCHECK(surface->DrawString(surface,p_hot_point->ssid, -1,p_hot_point->x,
                   p_hot_point->y + p_hot_point->height-FONT24_HEIGHT/4 , DSTF_LEFT));
        //draw single level dB
        sprintf(buffer, "%s", p_hot_point->single_level_db);
        DFBCHECK(surface->DrawString(surface,buffer, -1,p_hot_point->x+(wifi_window.desc.width>>1),
                   p_hot_point->y + p_hot_point->height-FONT24_HEIGHT/4 , DSTF_LEFT));
        //draw single level

        // db_msg("wifi_window.desc.width=%d\n",wifi_window.desc.width);

        current_pozition_x=(wifi_window.desc.width*2)/3;
        // db_msg("current_pozition_x=%d\n",current_pozition_x);
        one_level_width=((wifi_window.desc.width-current_pozition_x)/5)-(wifi_window.desc.width>>7);
        if (single >= wifi_signal_low_threshold) {
            DFBCHECK(surface->SetColor(surface,
                          color_table[COLOR_GREEN_IDX].r,
                          color_table[COLOR_GREEN_IDX].g,
                          color_table[COLOR_GREEN_IDX].b, 0xff));
        }
        else {
            DFBCHECK(surface->SetColor(surface,
                          color_table[COLOR_RED_IDX].r,
                          color_table[COLOR_RED_IDX].g,
                          color_table[COLOR_RED_IDX].b, 0xff));
        }
        for (single=0;single<p_hot_point->single_level;single++) {
            DFBCHECK(surface->FillRectangle(surface, current_pozition_x, p_hot_point->y+\
              (p_hot_point->height>>2),\
            one_level_width, p_hot_point->height>>1));
            current_pozition_x+=one_level_width;
        }
        wifi_hot_point_display.current_display_position_y+=wifi_hot_point_display.one_point_display_height;

        DFBCHECK(surface->SetColor(surface,
                          color_table[COLOR_WHITE_IDX].r,
                          color_table[COLOR_WHITE_IDX].g,
                          color_table[COLOR_WHITE_IDX].b, 0xff));
        wifi_window.surface->DrawLine(surface,0,wifi_hot_point_display.current_display_position_y-1,\
                    wifi_window.desc.width,wifi_hot_point_display.current_display_position_y-1);

        surface->Flip(surface, NULL, 0);
        // db_msg("wifi:%s add\n",wifi_hot_point->ssid);
    }
}

static int wifi_window_init(void)
{
    pthread_t tid;
    int ret;
    db_debug("wifi list window init...\n");

    wifi_window.desc.flags  = (DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_CAPS);
    if (mic_activated) {
        wifi_window.desc.posx   = (layer_config.width >> 1)+(layer_config.width>>MIC_POWER_BAR_WITH_SHIFT);
        wifi_window.desc.width  = (layer_config.width >> 1)-(layer_config.width>>MIC_POWER_BAR_WITH_SHIFT);
    }
    else {
        wifi_window.desc.posx   = (layer_config.width >> 1);
        wifi_window.desc.width  = (layer_config.width >> 1);
    }

    if (camera_activated) {
        wifi_window.desc.posy   = layer_config.height >> 1;
        wifi_window.desc.height = (layer_config.height >> 1);
    }
    else {
        wifi_window.desc.posy   = 0;
        wifi_window.desc.height = layer_config.height;
    }

    wifi_window.desc.caps   = DWCAPS_ALPHACHANNEL;

    DFBCHECK(layer->CreateWindow(layer, &wifi_window.desc, &wifi_window.window));
    wifi_window.window->GetSurface(wifi_window.window, &wifi_window.surface);
    wifi_window.window->SetOpacity(wifi_window.window, 0xff);
    wifi_window.window->SetOptions(wifi_window.window, DWOP_KEEP_POSITION);

    wifi_window.bgcolor = item_init_bgcolor;
    DFBCHECK(wifi_window.surface->SetColor(wifi_window.surface,
                                             color_table[wifi_window.bgcolor].r,
                                             color_table[wifi_window.bgcolor].g,
                                             color_table[wifi_window.bgcolor].b, 0xff));
    DFBCHECK(wifi_window.surface->FillRectangle(wifi_window.surface, 0, 0,
                wifi_window.desc.width, wifi_window.desc.height));

    /* draw menu */
    if (script_fetch("df_view", "wifi_menu_name", (int *)wifi_window_menu.name, 8)) {
        strcpy(wifi_window_menu.name, WIFI_MENU_NAME);
    }

    wifi_window_menu.width  = wifi_window.desc.width;
    wifi_window_menu.height = MENU_HEIGHT;

    if (script_fetch("df_view", "menu_bgcolor", &wifi_window_menu.bgcolor, 1) ||
            wifi_window_menu.bgcolor < COLOR_WHITE_IDX || wifi_window_menu.bgcolor > COLOR_BLACK_IDX) {
        wifi_window_menu.bgcolor = COLOR_YELLOW_IDX;
    }

    if (script_fetch("df_view", "menu_fgcolor", &wifi_window_menu.fgcolor, 1) ||
            wifi_window_menu.fgcolor < COLOR_WHITE_IDX || wifi_window_menu.fgcolor > COLOR_BLACK_IDX) {
        wifi_window_menu.fgcolor = COLOR_BLACK_IDX;
    }

    wifi_window_menu.bgcolor=COLOR_BEAUTY_IDX;
    wifi_window_menu.fgcolor=COLOR_WHITE_IDX;
    DFBCHECK(wifi_window.surface->SetColor(wifi_window.surface,
                                             color_table[wifi_window_menu.bgcolor].r,
                                             color_table[wifi_window_menu.bgcolor].g,
                                             color_table[wifi_window_menu.bgcolor].b, 0xff));
    DFBCHECK(wifi_window.surface->FillRectangle(wifi_window.surface, 0, 0, wifi_window_menu.width, wifi_window_menu.height));
    DFBCHECK(wifi_window.surface->SetFont(wifi_window.surface, font));
    DFBCHECK(wifi_window.surface->SetColor(wifi_window.surface,
                                             color_table[wifi_window_menu.fgcolor].r,
                                             color_table[wifi_window_menu.fgcolor].g,
                                             color_table[wifi_window_menu.fgcolor].b, 0xff));
    DFBCHECK(wifi_window.surface->DrawString(wifi_window.surface, wifi_window_menu.name, -1, 4,
                MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8, DSTF_LEFT));

    wifi_window.surface->DrawLine(wifi_window.surface,0,0,0,wifi_window.desc.height);
    wifi_window.surface->Flip(wifi_window.surface, NULL, 0);

    memset(&wifi_hot_point_display,0,sizeof(struct wifi_hot_point_display_t));
    wifi_hot_point_display.current_display_position_x=4;
    wifi_hot_point_display.current_display_position_y=MENU_HEIGHT;
    wifi_hot_point_display.one_point_display_width   =wifi_window_menu.width;
    wifi_hot_point_display.one_point_display_height  =ITEM_HEIGHT;
    wifi_hot_point_display.total_point_can_be_display=(wifi_window.desc.height-\
    wifi_window_menu.height)/wifi_hot_point_display.one_point_display_height;

    /* create named pipe */
    unlink(WIFI_PIPE_NAME);
    if (mkfifo(WIFI_PIPE_NAME,S_IFIFO | 0666) == -1) {
        db_error("core: mkfifo error(%s)\n", strerror(errno));
        return -1;
    }
    ret = pthread_create(&tid, NULL, wifi_hot_point_proccess, NULL);
    if (ret != 0) {
        db_error("df_view: create wifi hot point proccess thread failed\n");
    }
    return 0;
}

static int misc_window_init(void)
{
    int rwidth, gwidth, bxpos, bwidth;

    db_debug("misc window init...\n");

    misc_window.desc.flags  = (DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_CAPS);

    misc_window.desc.posx   = 0;
    misc_window.desc.posy   = manual_window.desc.height+auto_window.desc.height;
    if (misc_window.desc.posy>=layer_config.height) {
        db_msg("no enough space to misc window\n");
        return 0;
    }
    if (mic_activated) {
        misc_window.desc.width  = (layer_config.width >> 1)-(layer_config.width>>MIC_POWER_BAR_WITH_SHIFT);
    }
    else {
        misc_window.desc.width  = layer_config.width >> 1;
    }
    misc_window.desc.height = layer_config.height-misc_window.desc.posy;
    misc_window.desc.caps   = DWCAPS_ALPHACHANNEL;

    DFBCHECK(layer->CreateWindow(layer, &misc_window.desc, &misc_window.window));
    misc_window.window->GetSurface(misc_window.window, &misc_window.surface);
    misc_window.window->SetOpacity(misc_window.window, 0xff);
    misc_window.window->SetOptions(misc_window.window, DWOP_KEEP_POSITION);

    // draw RGB
    rwidth = gwidth = misc_window.desc.width / 3;
    bxpos = rwidth + gwidth;
    bwidth = misc_window.desc.width - bxpos;
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface, 0xff, 0, 0, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, 0, 0, rwidth, misc_window.desc.height));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface, 0, 0xff, 0, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, rwidth, 0, gwidth, misc_window.desc.height));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface, 0, 0, 0xff, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, bxpos, 0, bwidth, misc_window.desc.height));

    /* draw copryright */
    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font20));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface, 0, 0, 0, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, COPYRIGHT, -1, misc_window.desc.width - 4,
                misc_window.desc.height - FONT24_HEIGHT / 4, DSTF_RIGHT));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

void update_mic_power_bar(struct mic_power_bar_t *mic_power_bar)
{
    float percent_of_power;
    int power_height;
    percent_of_power=(float)((float)(abs(mic_power_bar->v))/((mic_power_bar->Max)-(mic_power_bar->Min)));
    power_height=percent_of_power*mic_power_bar->height;
    //set the front color

    DFBCHECK(mic_power_bar->mic_window->surface->SetColor(mic_power_bar->mic_window->surface,
                                         color_table[mic_power_bar->BarColor[1]].r,
                                         color_table[mic_power_bar->BarColor[1]].g,
                                         color_table[mic_power_bar->BarColor[1]].b, 0xff));

    //draw the front part
    DFBCHECK(mic_power_bar->mic_window->surface->FillRectangle(mic_power_bar->mic_window->surface,\
                                            mic_power_bar->XOff,\
                                            mic_power_bar->height-power_height,
                                            mic_power_bar->width,
                                            mic_power_bar->height));

    DFBCHECK(mic_power_bar->mic_window->surface->SetColor(mic_power_bar->mic_window->surface,
                                              color_table[mic_power_bar->BarColor[0]].r,
                                              color_table[mic_power_bar->BarColor[0]].g,
                                              color_table[mic_power_bar->BarColor[0]].b, 0xff));
    //draw the back part
    DFBCHECK(mic_power_bar->mic_window->surface->FillRectangle(mic_power_bar->mic_window->surface,\
                                            mic_power_bar->XOff,\
                                            mic_power_bar->YOff,
                                            mic_power_bar->width,
                                            mic_power_bar->height-power_height));

    //draw the bar name
    DFBCHECK(mic_power_bar->mic_window->surface->SetColor(mic_power_bar->mic_window->surface,
                                          color_table[COLOR_WHITE_IDX].r,
                                          color_table[COLOR_WHITE_IDX].g,
                                          color_table[COLOR_WHITE_IDX].b, 0x10));
    DFBCHECK(mic_power_bar->mic_window->surface->DrawString(mic_power_bar->mic_window->surface,\
                                                mic_power_bar->bar_name, -1,\
                                                mic_power_bar->mic_window->desc.width>>1,
                                                mic_power_bar->mic_window->desc.height>>1, DSTF_CENTER));

    mic_power_bar->mic_window->surface->Flip(mic_power_bar->mic_window->surface, NULL, 0);
}

void* soundtest_status_update(void* argv)
{
	while (1) {
		if (soundtest_status_flag == 1) {
			soundtest_status_flag = 0;
			soundtest_status = fopen(SOUNDTEST_STATUS, "w");
			fprintf(soundtest_status, "%s", "PASS");
			fclose(soundtest_status);
			fsync(soundtest_status);
		} else if (soundtest_status_flag == 2) {
			soundtest_status_flag = 0;
			soundtest_status = fopen(SOUNDTEST_STATUS, "w");
			fprintf(soundtest_status, "%s", "FAIL");
			fclose(soundtest_status);
			fsync(soundtest_status);
		} else if (soundtest_status_flag == 3) {
			soundtest_status_flag = 0;
			soundtest_status = fopen(SOUNDTEST_STATUS, "w");
			fprintf(soundtest_status, "%s", "START");
			fclose(soundtest_status);
			fsync(soundtest_status);
		} else {
			continue;
		}
	}
}

void* mic_audio_receive(void* argv)
{
    FILE *mic_pipe;
    char buffer[128];
    char *channel;
    printf("this is the mic_audio_receive thread...\n");
    mic_pipe = fopen(MIC_PIPE_NAME, "r");
    setlinebuf(mic_pipe);
    while (1) {
        if (fgets(buffer, sizeof(buffer), mic_pipe) == NULL) {
            continue;
        }
        channel= strtok(buffer, "#");
        if (channel==NULL) {
            continue;
        }
        mic1_power_bar.v=atoi(channel);
        channel = strtok(NULL, " \n");
        if (channel==NULL) {
            continue;
        }
        mic2_power_bar.v=atoi(channel);
        update_mic_power_bar(&mic1_power_bar);
        update_mic_power_bar(&mic2_power_bar);
    }
    fclose(mic_pipe);
}

static int mic_window_init(void)
{
    pthread_t tid;
    int ret;
    if (!mic_activated) {
        return 0;
    }
    db_debug("mic window init...\n");

    mic1_window.desc.flags  = (DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT |
                              DWDESC_CAPS);
    mic1_window.desc.posx   = (layer_config.width >> 1)-(layer_config.width>>MIC_POWER_BAR_WITH_SHIFT);
    mic1_window.desc.posy   = 0;
    mic1_window.desc.width  = (layer_config.width >>MIC_POWER_BAR_WITH_SHIFT_TOTAL);
    mic1_window.desc.height = layer_config.height>>1;
    mic1_window.desc.caps   = DWCAPS_ALPHACHANNEL;

    mic2_window.desc.flags  = (DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT |
                              DWDESC_CAPS);
    mic2_window.desc.posx   = (layer_config.width >> 1)-(layer_config.width>>MIC_POWER_BAR_WITH_SHIFT);
    mic2_window.desc.posy   = layer_config.height>>1;
    mic2_window.desc.width  = (layer_config.width >>MIC_POWER_BAR_WITH_SHIFT_TOTAL);
    mic2_window.desc.height = layer_config.height>>1;
    mic2_window.desc.caps   = DWCAPS_ALPHACHANNEL;

    DFBCHECK(layer->CreateWindow(layer, &mic1_window.desc, &mic1_window.window));
    mic1_window.window->GetSurface(mic1_window.window, &mic1_window.surface);
    mic1_window.window->SetOpacity(mic1_window.window, 0xff);
    mic1_window.window->SetOptions(mic1_window.window, DWOP_KEEP_POSITION);

    DFBCHECK(layer->CreateWindow(layer, &mic2_window.desc, &mic2_window.window));
    mic2_window.window->GetSurface(mic2_window.window, &mic2_window.surface);
    mic2_window.window->SetOpacity(mic2_window.window, 0xff);
    mic2_window.window->SetOptions(mic2_window.window, DWOP_KEEP_POSITION);

    //draw a rect around the window
    DFBCHECK(mic1_window.surface->SetColor(mic1_window.surface,
                                          color_table[COLOR_WHITE_IDX].r,
                                          color_table[COLOR_WHITE_IDX].g,
                                          color_table[COLOR_WHITE_IDX].b, 0xff));

    DFBCHECK(mic1_window.surface->DrawRectangle(mic1_window.surface,0,0,\
                                         mic1_window.desc.width,
                                         mic1_window.desc.height));

    DFBCHECK(mic2_window.surface->SetColor(mic2_window.surface,
                                          color_table[COLOR_WHITE_IDX].r,
                                          color_table[COLOR_WHITE_IDX].g,
                                          color_table[COLOR_WHITE_IDX].b, 0xff));

    DFBCHECK(mic2_window.surface->DrawRectangle(mic2_window.surface,0,0,\
                                         mic2_window.desc.width,
                                         mic2_window.desc.height));

    mic1_power_bar.BarColor[0]=COLOR_BLACK_IDX;
    mic1_power_bar.BarColor[1]=COLOR_MAGENTA_IDX;
    mic1_power_bar.XOff       =1;
    mic1_power_bar.YOff       =1;
    mic1_power_bar.width      =mic1_window.desc.width-2;//except the rounded rectangle
    mic1_power_bar.height     =mic1_window.desc.height-2;//except the rounded rectangle
    mic1_power_bar.Min        =0; //will be change
    mic1_power_bar.Max        =32768;//will be change
    mic1_power_bar.v          =0;
    if (mic1_used) {
        memcpy(mic1_power_bar.bar_name,"Mic1",sizeof("Mic1"));
    }
    else if (mic2_used) {
        memcpy(mic1_power_bar.bar_name,"Mic2",sizeof("Mic2"));
    }
    mic1_power_bar.mic_window =&mic1_window;

    mic2_power_bar.BarColor[0]=COLOR_BLACK_IDX;
    mic2_power_bar.BarColor[1]=COLOR_BEAUTY_IDX;
    mic2_power_bar.XOff       =1;
    mic2_power_bar.YOff       =1;
    mic2_power_bar.width      =mic2_window.desc.width-2;//except the rounded rectangle
    mic2_power_bar.height     =mic2_window.desc.height-2;//except the rounded rectangle
    mic2_power_bar.Min        =0; //will be change
    mic2_power_bar.Max        =32768;//will be change
    mic2_power_bar.v          =0;
    if (!mic2_used) {
        memcpy(mic2_power_bar.bar_name,"Mic1",sizeof("Mic1"));
    }
    else {
        memcpy(mic2_power_bar.bar_name,"Mic2",sizeof("Mic2"));
    }

    mic2_power_bar.mic_window =&mic2_window;

    //draw the mic1 words
    DFBCHECK(mic1_window.surface->SetFont(mic1_window.surface, font24));
    DFBCHECK(mic1_window.surface->SetColor(mic1_window.surface,
                                          color_table[COLOR_WHITE_IDX].r,
                                          color_table[COLOR_WHITE_IDX].g,
                                          color_table[COLOR_WHITE_IDX].b, 0x10));
    DFBCHECK(mic1_window.surface->DrawString(mic1_window.surface, mic1_power_bar.bar_name, -1,mic1_window.desc.width>>1,
                mic1_window.desc.height>>1, DSTF_CENTER));

    //draw the mic2 words
    DFBCHECK(mic2_window.surface->SetFont(mic2_window.surface, font24));
    DFBCHECK(mic2_window.surface->SetColor(mic2_window.surface,
                                          color_table[COLOR_WHITE_IDX].r,
                                          color_table[COLOR_WHITE_IDX].g,
                                          color_table[COLOR_WHITE_IDX].b, 0x10));
    DFBCHECK(mic2_window.surface->DrawString(mic2_window.surface,mic2_power_bar.bar_name, -1,mic2_window.desc.width>>1,
                mic2_window.desc.height>>1, DSTF_CENTER));

    //flip the buffer to the screen
    mic1_window.surface->Flip(mic1_window.surface, NULL, 0);
    mic2_window.surface->Flip(mic2_window.surface, NULL, 0);

    /* create named pipe */
    unlink(MIC_PIPE_NAME);
    if (mkfifo(MIC_PIPE_NAME,S_IFIFO | 0666) == -1) {
        db_error("core: mkfifo error(%s)\n", strerror(errno));
        return -1;
    }
    ret = pthread_create(&tid, NULL, mic_audio_receive, NULL);
    if (ret != 0) {
        db_error("df_view: create sound play thread failed\n");
    }

    return 0;
}

static int df_font_init(void)
{
    DFBFontDescription font_desc;

    if (script_fetch("df_view", "font_size", &font_size, 1)) {
        printf("get font_size fail\n");
        font_size = DEFAULT_FONT_SIZE;
    }

    /* create font 16 pixel */
    font_desc.flags  = DFDESC_HEIGHT;
    font_desc.height = FONT16_HEIGHT;
    DFBCHECK(dfb->CreateFont(dfb, DATADIR"/wqy-zenhei.ttc", &font_desc, &font16));

    /* create font 20 pixel */
    font_desc.flags  = DFDESC_HEIGHT;
    font_desc.height = FONT20_HEIGHT;
    DFBCHECK(dfb->CreateFont(dfb, DATADIR"/wqy-zenhei.ttc", &font_desc, &font20));

    /* create font 24 pixel */
    font_desc.flags  = DFDESC_HEIGHT;
    font_desc.height = FONT24_HEIGHT;
    DFBCHECK(dfb->CreateFont(dfb, DATADIR"/wqy-zenhei.ttc", &font_desc, &font24));

    /* create font 28 pixel */
    font_desc.flags  = DFDESC_HEIGHT;
    font_desc.height = FONT28_HEIGHT;
    DFBCHECK(dfb->CreateFont(dfb, DATADIR"/wqy-zenhei.ttc", &font_desc, &font28));

    /* create font 48 pixel */
    font_desc.flags  = DFDESC_HEIGHT;
    font_desc.height = FONT48_HEIGHT;
    DFBCHECK(dfb->CreateFont(dfb, DATADIR"/wqy-zenhei.ttc", &font_desc, &font48));
    if (font_size == FONT16_HEIGHT) {
        font = font16;
	} else if (font_size == FONT24_HEIGHT) {
        font = font24;
    } else if (font_size == FONT28_HEIGHT) {
        font = font28;
    } else if (font_size == FONT48_HEIGHT) {
        font = font48;
    } else {
        font = font20;
    }

    return 0;
}

static int df_config_init(void)
{
    if (script_fetch("mic", "activated", &mic_activated,1)) {
        mic_activated=0;
    }

    if (script_fetch("mic", "mic1_used", &mic1_used, 1)) {
        mic1_used=0;
        db_msg("core: can't fetch mic1_used, set to default: %d\n", mic1_used);
    }

    if (script_fetch("mic", "mic2_used", &mic2_used, 1)) {
        mic2_used=0;
        db_msg("core: can't fetch mic2_used, set to default: %d\n", mic2_used);
    }

    if (!mic1_used&&!mic2_used) {
        mic1_used=1;
        mic2_used=1;
    }

    if (script_fetch("camera", "activated", &camera_activated, 1)) {
        camera_activated=0;
    }

    if (script_fetch("df_view", "item_init_bgcolor", &item_init_bgcolor, 1) ||
            item_init_bgcolor < COLOR_WHITE_IDX ||
            item_init_bgcolor > COLOR_BLACK_IDX) {
        item_init_bgcolor = COLOR_WHITE_IDX;
    }

    if (script_fetch("df_view", "item_init_fgcolor", &item_init_fgcolor, 1) ||
            item_init_fgcolor < COLOR_WHITE_IDX ||
            item_init_fgcolor > COLOR_BLACK_IDX) {
        item_init_fgcolor = COLOR_BLACK_IDX;
    }

    if (script_fetch("df_view", "item_ok_bgcolor", &item_ok_bgcolor, 1) ||
            item_ok_bgcolor < COLOR_WHITE_IDX ||
            item_ok_bgcolor > COLOR_BLACK_IDX) {
        item_ok_bgcolor = COLOR_WHITE_IDX;
    }

    if (script_fetch("df_view", "item_ok_fgcolor", &item_ok_fgcolor, 1) ||
            item_ok_fgcolor < COLOR_WHITE_IDX ||
            item_ok_fgcolor > COLOR_BLACK_IDX) {
        item_ok_fgcolor = COLOR_BLUE_IDX;
    }

    if (script_fetch("df_view", "item_fail_bgcolor", &item_fail_bgcolor, 1) ||
            item_fail_bgcolor < COLOR_WHITE_IDX ||
            item_fail_bgcolor > COLOR_BLACK_IDX) {
        item_fail_bgcolor = COLOR_WHITE_IDX;
    }

    if (script_fetch("df_view", "item_fail_fgcolor", &item_fail_fgcolor, 1) ||
            item_fail_fgcolor < COLOR_WHITE_IDX ||
            item_fail_fgcolor > COLOR_BLACK_IDX) {
        item_fail_fgcolor = COLOR_RED_IDX;
    }

    if (script_fetch("df_view", "pass_str", (int *)pass_str,
                sizeof(pass_str) / 4 - 1)) {
        strcpy(pass_str, "Pass");
    }

    if (script_fetch("df_view", "fail_str", (int *)fail_str,
                sizeof(fail_str) / 4 - 1)) {
        strcpy(fail_str, "Fail");
    }

    if (script_fetch("motor", "activated", &motor_activated, 1)) {
        motor_activated = 0;
    }

    if (script_fetch("flashlight", "activated", &flashlight_activated, 1)) {
        flashlight_activated = 0;
    }

    if (script_fetch("gsensor", "category", &gsensor_category, 1)) {
        gsensor_category = 0;
    }

    if (script_fetch("tp", "type", &tp_type, 1)) {
        tp_type = -1;
    }

    if (script_fetch("tp", "tp_test_type", &tp_test_type, 1)) {  // if not found ,default manual test
        tp_test_type = 1;
    }

    return 0;
}

static int df_windows_init(void)
{
    unsigned int args[4];
    int disp;
    int fb;
    unsigned int layer_id = 0;
    int argc;
    char **argv;
    int count = 0;
    int ret = 0;

#ifndef _SUN50IW12P
    /* open /dev/disp */
    if ((disp = open("/dev/disp", O_RDWR)) == -1) {
        db_error("can't open /dev/disp(%s)\n", strerror(errno));
        return -1;
    }
#endif

    /* open /dev/fb0 */
    fb = open("/dev/fb0", O_RDWR);
    if (fb < 0) {
        db_error("can't open /dev/fb0(%s)\n", strerror(errno));
        return -1;
    }

#if 0
    if (ioctl(fb, FBIOGET_LAYER_HDL_0, &layer_id) < 0) {
	 db_error("can't open /dev/fb0(%s)\n", strerror(errno));
	 close(disp);
	 return -1;
    }
#endif

    args[0] = 0;

	//DISP_CMD_GET_OUTPUT_TYPE = 0x09, according "linux-3.4\include\video\drv_display.h":698
#if defined (_CONFIG_LINUX_5_4)
    disp_output_type_t = (enum disp_output_type)ioctl(disp, DISP_GET_OUTPUT_TYPE,(void*)args);
#else
    disp_output_type_t = (disp_output_type)ioctl(disp, DISP_GET_OUTPUT_TYPE,(void*)args);
#endif

    if (disp_output_type_t < 0) {
        db_error("Can't get line disp_output_type_t!!!\n");
    }
    db_msg("disp_output_type=%d\n",disp_output_type_t);

    /* set layer bottom */
    args[0] = 0;
    args[1] = layer_id;
#if defined (_SUN8IW6P) || (_SUN50IW1P) || (_SUN50IW3P) || (_SUN8IW15P) || (_SUN50IW6P) || (_SUN8IW7P) || (_SUN50IW10P)
    ioctl(disp, DISP_LAYER_BOTTOM, args);
#elif defined (_SUN8IW5P) || (_SUN9IW1P)
    ioctl(disp, DISP_CMD_LAYER_BOTTOM, args);
#endif

#ifndef _SUN50IW12P
    close(disp);
#endif
    close(fb);

    /* init directfb */
    argc = 1;
    argv = malloc(sizeof(char *) * argc);
    argv[0] = "df_view";
    DFBCHECK(DirectFBInit(&argc, &argv));
    DFBCHECK(DirectFBCreate(&dfb));

    dfb->GetDeviceDescription(dfb, &gdesc);
    DFBCHECK(dfb->GetDisplayLayer(dfb, DLID_PRIMARY, &layer));
    layer->SetCooperativeLevel(layer, DLSCL_ADMINISTRATIVE);

    if (!((gdesc.blitting_flags & DSBLIT_BLEND_ALPHACHANNEL) &&
                (gdesc.blitting_flags & DSBLIT_BLEND_COLORALPHA))) {
        layer_config.flags = DLCONF_BUFFERMODE;
        layer_config.buffermode = DLBM_BACKSYSTEM;
        layer->SetConfiguration(layer, &layer_config);
    }

    layer->GetConfiguration(layer, &layer_config);

#if defined (_CONFIG_LINUX_5_4) || (_CONFIG_LINUX_5_10) || (_CONFIG_LINUX_5_15)
    layer->EnableCursor(layer, 0);
#else
    if (disp_output_type_t == DISP_OUTPUT_TYPE_LCD) {
        layer->EnableCursor(layer, 1); // a83-f1 因为tp驱动的缘故，不显示鼠标，20140806
    }
    else {
        layer->EnableCursor(layer, 0);
    }
#endif

    db_msg("screen_width: %d, sceen_height: %d\n", layer_config.width, layer_config.height);

    /* init font */
    df_font_init();

    /* init config */
    df_config_init();

    /* init misc window befor video window */
    manual_window_init();
    auto_window_init();
    misc_window_init();
	reboot_button_init();
    poweroff_button_init();
    mic_window_init();
    wifi_window_init();
    handle_mic_start_init();
    handle_mic_pass_init();
    handle_mic_fail_init();

    /* used for soundtest */
    soundtest_status = fopen(SOUNDTEST_STATUS, "w");
    fprintf(soundtest_status, "%s", "soundtest");
    fclose(soundtest_status);

    soundtest_status_flag = 0;
    ret = pthread_create(&soundtest_tid, NULL, soundtest_status_update, NULL);
    if (ret != 0) {
        db_error("df_view: create sound play thread failed\n");
    }

    if (disp_output_type_t == DISP_OUTPUT_TYPE_LCD) {
        tp_track_init();
    }

    if (motor_activated == 1) {
        handle_motor_init();
    }

    if (flashlight_activated == 1) {
        handle_flashlight_init();
    }

    if (gsensor_category == 1) {
        handle_gsensor_pass_init();
        handle_gsensor_fail_init();
    }

    if (tp_type != -1 && tp_test_type == 1) { // only 1 need show button
        handle_tp_pass_init();
        handle_tp_fail_init();
    }

    INIT_LIST_HEAD(&auto_tc_list);
    INIT_LIST_HEAD(&manual_tc_list);
    INIT_LIST_HEAD(&wifi_list);

#ifndef _SUN50IW12P
    printf("------------- to open video node\n");
    while ((access("/dev/video0",F_OK)) == -1) {
		sleep(1);
		if (count++ == 3) {
			db_error("Can't not open /dev/video0\n");
			return -1;
		}
    }
    video_window_init();
#endif
    return 0;
}

static int df_insert_item(int id, struct item_data *data)
{
    struct list_head *pos, *tc_list;
    IDirectFBSurface *surface;
    int window_height;
    int x, y;
    struct df_item_data *df_data;
    char item_str[132];

    if (data == NULL)
        return -1;

    if (data->category == CATEGORY_AUTO) {
        tc_list = &auto_tc_list;
        surface = auto_window.surface;
        window_height = auto_window.desc.height;
    }
    else if (data->category == CATEGORY_MANUAL) {
        tc_list = &manual_tc_list;
        surface = manual_window.surface;
        window_height = manual_window.desc.height;
    }
    else {
        db_error("unknown category of item: %s\n", data->name);
        return -1;
    }

    x = 0;
    y = MENU_HEIGHT;
    list_for_each(pos, tc_list) {
        struct df_item_data *temp = list_entry(pos, struct df_item_data, list);
        y += temp->height;
    }

    if (y + ITEM_HEIGHT > window_height) {
        db_error("no more space for item: %s\n", data->name);
        return -1;
    }

    df_data = malloc(sizeof(struct df_item_data));
    if (df_data == NULL)
        return -1;

    df_data->id = id;
    strncpy(df_data->name, data->name, 32);
    strncpy(df_data->display_name, data->display_name, 64);
    df_data->category = data->category;
    df_data->status = data->status;
    if (data->exdata[0]) {
        strncpy(df_data->exdata, data->exdata, 64);
        snprintf(item_str, 128, "%s %s", df_data->display_name, df_data->exdata);
    }
    else {
        snprintf(item_str, 128, "%s", df_data->display_name);
    }

    df_data->x      = x;
    df_data->y      = y;
    df_data->width  = auto_window.desc.width;
    df_data->height = ITEM_HEIGHT;

    df_data->bgcolor = item_init_bgcolor;
    df_data->fgcolor = item_init_fgcolor;

#if 0
    db_debug("test case: %s\n", df_data->name);
    db_debug("        x: %d\n", df_data->x);
    db_debug("        y: %d\n", df_data->y);
    db_debug("    width: %d\n", df_data->width);
    db_debug("   height: %d\n", df_data->height);
#endif

    list_add(&df_data->list, tc_list);

    DFBCHECK(surface->SetColor(surface,
                               color_table[df_data->bgcolor].r,
                               color_table[df_data->bgcolor].g,
                               color_table[df_data->bgcolor].b, 0xff));
    DFBCHECK(surface->FillRectangle(surface, df_data->x, df_data->y, df_data->width, df_data->height));
    DFBCHECK(surface->SetFont(surface, font));
    DFBCHECK(surface->SetColor(surface,
                               color_table[df_data->fgcolor].r,
                               color_table[df_data->fgcolor].g,
                               color_table[df_data->fgcolor].b, 0xff));
    DFBCHECK(surface->DrawString(surface, item_str, -1, df_data->x + 4,
                df_data->y + df_data->height - font_size / 4, DSTF_LEFT));

    surface->Flip(surface, NULL, 0);

    return 0;
}

static int df_update_item(int id, struct item_data *data)
{
    struct list_head *pos, *tc_list;
    IDirectFBSurface *surface;
    struct df_item_data *df_data;
    char item_str[132];

    if (data == NULL) {
        return -1;
    }

    if (data->category == CATEGORY_AUTO) {
        tc_list = &auto_tc_list;
        surface = auto_window.surface;
    }
    else if (data->category == CATEGORY_MANUAL) {
        tc_list = &manual_tc_list;
        surface = manual_window.surface;
    }
    else {
        db_error("unknown category of item: %s\n", data->name);
        return -1;
    }

    df_data = NULL;
    list_for_each(pos, tc_list)
    {
        struct df_item_data *temp = list_entry(pos, struct df_item_data, list);
        if (temp->id == id) {
            df_data = temp;
            break;
        }
    }

    if (df_data == NULL) {
        db_error("no such test case id #%d, name: %s\n", id, data->name);
        return -1;
    }
    if (data->status != STATUS_MSG && data->status < STATUS_NONE) {
        df_data->status = data->status;
    }

    if (df_data->status == STATUS_INIT) {
        df_data->bgcolor = item_init_bgcolor;
        df_data->fgcolor = item_init_fgcolor;
    }
    else if (df_data->status == STATUS_PASS) {
        df_data->bgcolor = item_ok_bgcolor;
        df_data->fgcolor = item_ok_fgcolor;
    }
    else if (df_data->status == STATUS_FAIL) {
        df_data->bgcolor = item_fail_bgcolor;
        df_data->fgcolor = item_fail_fgcolor;
    }

    if (data->exdata[0]) {
        strncpy(df_data->exdata, data->exdata, 64);
        if (df_data->status < 0) {
            snprintf(item_str, 128, "%s: %s", df_data->display_name,
                                              df_data->exdata);
        }
        else if (df_data->status == 0) {
            snprintf(item_str, 128, "%s: [%s] %s", df_data->display_name,
                                                   pass_str,
                                                   df_data->exdata);
        }
        else {
            snprintf(item_str, 128, "%s: [%s] %s", df_data->display_name,
                                                   fail_str,
                                                   df_data->exdata);
        }
    }
    else {
            if (df_data->status < 0) {
                snprintf(item_str, 128, "%s", df_data->display_name);
            }
            else if (df_data->status == 0) {
                snprintf(item_str, 128, "%s: [%s]", df_data->display_name, pass_str);
            }
            else {
                snprintf(item_str, 128, "%s: [%s]", df_data->display_name, fail_str);
            }
    }

#if 0
    db_debug("test case: %s\n", df_data->name);
    db_debug("        x: %d\n", df_data->x);
    db_debug("        y: %d\n", df_data->y);
    db_debug("    width: %d\n", df_data->width);
    db_debug("   height: %d\n", df_data->height);
#endif

    DFBCHECK(surface->SetColor(surface,
                               color_table[df_data->bgcolor].r,
                               color_table[df_data->bgcolor].g,
                               color_table[df_data->bgcolor].b, 0xff));
    DFBCHECK(surface->FillRectangle(surface, df_data->x, df_data->y, df_data->width, df_data->height));
    DFBCHECK(surface->SetFont(surface, font));
    DFBCHECK(surface->SetColor(surface,
                               color_table[df_data->fgcolor].r,
                               color_table[df_data->fgcolor].g,
                               color_table[df_data->fgcolor].b, 0xff));
    DFBCHECK(surface->DrawString(surface, item_str, -1, df_data->x + 4,
                df_data->y + df_data->height - font_size / 4, DSTF_LEFT));

    surface->Flip(surface, NULL, 0);

    check_auto_testcases_allpass();//show pass button if auto_testcase_allpass

    return 0;
}

static int df_delete_item(int id)
{
    return -1;
}

static void df_sync(void)
{
    auto_window.surface->Flip(auto_window.surface, NULL, 0);
    manual_window.surface->Flip(manual_window.surface, NULL, 0);
}

static struct view_operations df_view_ops =
{
    .insert_item = df_insert_item,
    .update_item = df_update_item,
    .delete_item = df_delete_item,
    .sync        = df_sync,
};

static void button_handle(int x, int y)
{
    static int clear_press = 0;
    static int switch_press = 0;
    static int reboot_press = 0;
    static int poweroff_press = 0;
    static int confirm_press = 0;
    static int motor_press = 0;
    static int flashlight_press = 0;
    static int tp_pass_press = 0;
    static int tp_fail_press = 0;
    static int gsensor_pass_press = 0;
    static int gsensor_fail_press = 0;
    static int mic_start_press = 0;
    static int mic_pass_press = 0;
    static int mic_fail_press = 0;
    static int count = 0;

    /* check clear button is press */
    if (x > clear_button.x && x < clear_button.x + clear_button.width &&
        y > clear_button.y && y < clear_button.y + clear_button.height) {
        if (clear_press) {
            clear_press = 0;
            tp_track_clear();
            clear_button_redraw(clear_button.bgcolor);
        }
        else {
            clear_button_redraw(COLOR_CYAN_IDX);
            clear_press = 1;
        }
    }
    else if (
             x > (misc_window.desc.posx + switch_button.x) &&
             x < (misc_window.desc.posx + switch_button.x + switch_button.width) &&
             y > (misc_window.desc.posy + switch_button.y) &&
             y < (misc_window.desc.posy + switch_button.y + switch_button.height)) {
        if (switch_press) {
	        count = 0;
            switch_press = 0;
            switch_camera();
            switch_button_redraw(switch_button.bgcolor);
        }
        else {
            count++;
            if (count % 1 == 0) {
                switch_press = 1;
                switch_button_redraw(COLOR_CYAN_IDX);
            }
        }
    }
    else if (
             x > (misc_window.desc.posx + reboot_button.x) &&
             x < (misc_window.desc.posx + reboot_button.x + reboot_button.width) &&
             y > (misc_window.desc.posy + reboot_button.y) &&
             y < (misc_window.desc.posy + reboot_button.y + reboot_button.height)) {
        if (reboot_press) {
            reboot_press = 0;
            reboot_button_redraw(reboot_button.bgcolor);
            system("reboot");
        }
        else {
            reboot_press = 1;
            reboot_button_redraw(COLOR_CYAN_IDX);
        }
    }
    else if (
             x > (misc_window.desc.posx + poweroff_button.x) &&
             x < (misc_window.desc.posx + poweroff_button.x + poweroff_button.width) &&
             y > (misc_window.desc.posy + poweroff_button.y) &&
             y < (misc_window.desc.posy + poweroff_button.y + poweroff_button.height)) {
        if (poweroff_press) {
            poweroff_press = 0;
            poweroff_button_redraw(poweroff_button.bgcolor);
            system("poweroff");
        }
        else {
            poweroff_press = 1;
            poweroff_button_redraw(COLOR_CYAN_IDX);
        }
    }
    else if (
             x > (misc_window.desc.posx + confirm_button.x) &&
             x < (misc_window.desc.posx + confirm_button.x + confirm_button.width) &&
             y > (misc_window.desc.posy + confirm_button.y) &&
             y < (misc_window.desc.posy + confirm_button.y + confirm_button.height)) {
        if (confirm_press) {
            confirm_press = 0;
            confirm_button_redraw(confirm_button.bgcolor);
            system("/dragonboard/bin/syswittester.sh");
        }
        else {
            confirm_press = 1;
            confirm_button_redraw(COLOR_CYAN_IDX);
        }
    }
    else if (
             x > (misc_window.desc.posx + motor_button.x) &&
             x < (misc_window.desc.posx + motor_button.x + motor_button.width) &&
             y > (misc_window.desc.posy + motor_button.y) &&
             y < (misc_window.desc.posy + motor_button.y + motor_button.height)) {
        if (motor_press) {
            motor_press = 0;
            motor_button_redraw(motor_button.bgcolor);
            char temp_buffer[50];
            int temp_version=1, temp_scrip=0, motor_id=get_motor_case_id();
            sprintf(temp_buffer,"/dragonboard/bin/motor_pass.sh %d %d %d",temp_version,temp_scrip,motor_id);
            system(temp_buffer);
        }
        else {
            motor_press = 1;
            motor_button_redraw(COLOR_CYAN_IDX);
        }
    }
    else if (
             x > (misc_window.desc.posx + tp_pass_button.x) &&
             x < (misc_window.desc.posx + tp_pass_button.x + tp_pass_button.width) &&
             y > (misc_window.desc.posy + tp_pass_button.y) &&
             y < (misc_window.desc.posy + tp_pass_button.y + tp_pass_button.height)) {
        if (tp_pass_press) {
            tp_pass_press = 0;
            tp_pass_button_redraw(tp_pass_button.bgcolor);
	        tp_data.status = STATUS_PASS;
            df_update_item(BUILDIN_TC_ID_TP, &tp_data);
        }
        else {
            tp_pass_press = 1;
            tp_pass_button_redraw(COLOR_CYAN_IDX);
            handle_tp_fail_init();
        }
    }
    else if (
             x > (misc_window.desc.posx + tp_fail_button.x) &&
             x < (misc_window.desc.posx + tp_fail_button.x + tp_fail_button.width) &&
             y > (misc_window.desc.posy + tp_fail_button.y) &&
             y < (misc_window.desc.posy + tp_fail_button.y + tp_fail_button.height)) {
        if (tp_fail_press) {
            tp_fail_press = 0;
            tp_fail_button_redraw(tp_fail_button.bgcolor);
	        tp_data.status = STATUS_FAIL;
            df_update_item(BUILDIN_TC_ID_TP, &tp_data);
        }
        else {
            tp_fail_press = 1;
            tp_fail_button_redraw(COLOR_CYAN_IDX);
            handle_tp_pass_init();
        }
    }
    else if (
             x > (misc_window.desc.posx + gsensor_pass_button.x) &&
             x < (misc_window.desc.posx + gsensor_pass_button.x + gsensor_pass_button.width) &&
             y > (misc_window.desc.posy + gsensor_pass_button.y) &&
             y < (misc_window.desc.posy + gsensor_pass_button.y + gsensor_pass_button.height)) {
        if (gsensor_pass_press) {
            gsensor_pass_press = 0;
            gsensor_pass_button_redraw(gsensor_pass_button.bgcolor);
            char temp_buffer[50];
            int temp_version=1, temp_scrip=0, gsensor_id=get_gsensor_case_id();
            sprintf(temp_buffer,"/dragonboard/bin/gsensor_button.sh %d %d %d 0", temp_version, temp_scrip, gsensor_id);
            system(temp_buffer);
        }
        else {
            gsensor_pass_press = 1;
            gsensor_pass_button_redraw(COLOR_CYAN_IDX);
            handle_gsensor_fail_init();
        }
    }
    else if (
             x > (misc_window.desc.posx + gsensor_fail_button.x) &&
             x < (misc_window.desc.posx + gsensor_fail_button.x + gsensor_fail_button.width) &&
             y > (misc_window.desc.posy + gsensor_fail_button.y) &&
             y < (misc_window.desc.posy + gsensor_fail_button.y + gsensor_fail_button.height)) {
        if (gsensor_fail_press) {
            gsensor_fail_press = 0;
            gsensor_fail_button_redraw(gsensor_fail_button.bgcolor);
            char temp_buffer[50];
            int temp_version=1, temp_scrip=0, gsensor_id=get_gsensor_case_id();
            sprintf(temp_buffer,"/dragonboard/bin/gsensor_button.sh %d %d %d 1", temp_version, temp_scrip, gsensor_id);
            system(temp_buffer);
        }
        else {
            gsensor_fail_press = 1;
            gsensor_fail_button_redraw(COLOR_CYAN_IDX);
            handle_gsensor_pass_init();
        }
    }
    else if (
             x > (misc_window.desc.posx + mic_start_button.x) &&
             x < (misc_window.desc.posx + mic_start_button.x + mic_start_button.width) &&
             y > (misc_window.desc.posy + mic_start_button.y) &&
             y < (misc_window.desc.posy + mic_start_button.y + mic_start_button.height)) {
        if (mic_start_press) {
            mic_start_press = 0;
            //mic_start_button_redraw(mic_start_button.bgcolor);
            //mic_start_button_redraw(COLOR_BLUE_IDX);
	    soundtest_status_flag = 3;
        handle_mic_start_init();
        }
        else {
            mic_start_press = 1;
            //mic_start_button_redraw(COLOR_BLUE_IDX);
            handle_mic_start_init();
            handle_mic_pass_init();
            handle_mic_fail_init();
        }
    }
    else if (
             x > (misc_window.desc.posx + mic_pass_button.x) &&
             x < (misc_window.desc.posx + mic_pass_button.x + mic_pass_button.width) &&
             y > (misc_window.desc.posy + mic_pass_button.y) &&
             y < (misc_window.desc.posy + mic_pass_button.y + mic_pass_button.height)) {
        if (mic_pass_press) {
            mic_pass_press = 0;
            mic_pass_button_redraw(mic_pass_button.bgcolor);
	        soundtest_status_flag = 1;
            char sound_buffer[50];
	        int soundtest_id=get_soundtest_case_id();
            sprintf(sound_buffer,"/dragonboard/bin/soundtest_pass.sh %d",soundtest_id);
            system(sound_buffer);
        }
        else {
            mic_pass_press = 1;
            mic_pass_button_redraw(COLOR_CYAN_IDX);
            handle_mic_fail_init();
        }
    }
    else if (
             x > (misc_window.desc.posx + mic_fail_button.x) &&
             x < (misc_window.desc.posx + mic_fail_button.x + mic_fail_button.width) &&
             y > (misc_window.desc.posy + mic_fail_button.y) &&
             y < (misc_window.desc.posy + mic_fail_button.y + mic_fail_button.height)) {
        if (mic_fail_press) {
            mic_fail_press = 0;
            mic_fail_button_redraw(mic_fail_button.bgcolor);
	        soundtest_status_flag = 2;
            char sound_buffer2[50];
	        int soundtest_id=get_soundtest_case_id();
            sprintf(sound_buffer2,"/dragonboard/bin/soundtest_fail.sh %d",soundtest_id);
            system(sound_buffer2);
        }
        else {
            mic_fail_press = 1;
            mic_fail_button_redraw(COLOR_CYAN_IDX);
            handle_mic_pass_init();
        }
    }
    else if (
             x > (misc_window.desc.posx + flashlight_button.x) &&
             x < (misc_window.desc.posx + flashlight_button.x + flashlight_button.width) &&
             y > (misc_window.desc.posy + flashlight_button.y) &&
             y < (misc_window.desc.posy + flashlight_button.y + flashlight_button.height)) {
        if (flashlight_press) {
            flashlight_press = 0;
            flashlight_test();
            flashlight_button_redraw(flashlight_button.bgcolor);
        }
        else {
            flashlight_button_redraw(COLOR_CYAN_IDX);
            flashlight_press = 1;
            handle_flashlight_init();
        }
    }
    else {
	    count = 0;
        switch_press = 0;
        clear_press = 0;
        flashlight_press = 0;
    }
}

static void show_mouse_event(DFBInputEvent *evt)
{
    static int press = 0;
    static int tp_x = -1, tp_y = -1;
    static int flag = 0x00;
    int mouse_x = 0, mouse_y = 0;
    char buf[64];
    *buf = 0;
    if (evt->type == DIET_AXISMOTION) {
        if (evt->flags & DIEF_AXISABS) {
            switch (evt->axis) {
                case DIAI_X:
                    mouse_x = evt->axisabs;
                    break;
                case DIAI_Y:
                    mouse_y = evt->axisabs;
                    break;
                case DIAI_Z:
                    snprintf(buf, sizeof(buf), "Z axis (abs): %d", evt->axisabs);
                    break;
                default:
                    snprintf(buf, sizeof(buf), "Axis %d (abs): %d", evt->axis, evt->axisabs);
                    break;
            }
        }
        else if (evt->flags & DIEF_AXISREL) {
            switch (evt->axis) {
                case DIAI_X:
                    mouse_x += evt->axisrel;
                    break;
                case DIAI_Y:
                    mouse_y += evt->axisrel;
                    break;
                case DIAI_Z:
                    snprintf(buf, sizeof(buf), "Z axis (rel): %d", evt->axisrel);
                    break;
                default:
                    snprintf (buf, sizeof(buf), "Axis %d (rel): %d", evt->axis, evt->axisrel);
                    break;
            }
        }
        else {
            db_debug("axis: %x\n", evt->axis);
        }

		if (mTouchScreenWidth * mTouchScreenHeight) {
			mouse_x = mouse_x * layer_config.width / mTouchScreenWidth;
			mouse_y = mouse_y * layer_config.height / mTouchScreenHeight;
		}
        mouse_x = CLAMP(mouse_x, 0, layer_config.width  - 1);
        mouse_y = CLAMP(mouse_y, 0, layer_config.height - 1);
    }
    else {
        snprintf(buf, sizeof(buf), "Button %d", evt->button);
    }

    db_dump("x #%d, y #%d event type = %d flags = %d\n", mouse_x, mouse_y, evt->type, evt->flags);
    if (buf[0]) {
        db_dump("mouse event: %s\n", buf);
    }

    if (mouse_x != 0) {
        flag |= 0x01;
        tp_x = mouse_x;
    }
    if (mouse_y != 0) {
        flag |= 0x02;
        tp_y = mouse_y;
    }

    if (evt->type == DIET_BUTTONPRESS || evt->type == DIET_BUTTONRELEASE) {
        button_handle(tp_x, tp_y);
        //draw first point
        if (evt->type == DIET_BUTTONPRESS) {
            /* draw first point if we have get x and y */
            tp_track_draw(tp_x, tp_y, 0);
            press = 1;
        }
        else {
            /* draw last point anyway */
            tp_track_draw(tp_x, tp_y, -1);
            press = 0;
        }
    }
    if ((flag & 0x03) == 0x03) {
        if (tp_test_type) {    // 1 is manual test
            tp_data.status = STATUS_MSG;
        }
        else {                 // 0 is auto test
            tp_data.status = STATUS_PASS;
        }
        snprintf(tp_data.exdata, 64, "(%d, %d)", tp_x, tp_y);
        df_update_item(BUILDIN_TC_ID_TP, &tp_data);
        tp_track_draw(tp_x, tp_y, press);
        flag &= ~0x03;
    }
}

/*
 * event main loop
 */
static void *event_mainloop(void *args)
{
    DFBInputDeviceKeySymbol last_symbol = DIKS_NULL;
    while (1) {
        DFBInputEvent evt;
        DFBCHECK(events->WaitForEvent(events));
        while (events->GetEvent(events, DFB_EVENT(&evt)) == DFB_OK) {
            show_mouse_event(&evt);
        }

        if (evt.type == DIET_KEYRELEASE) {
            if ((last_symbol == DIKS_ESCAPE || last_symbol == DIKS_EXIT) &&
                    (evt.key_symbol == DIKS_ESCAPE || evt.key_symbol == DIKS_EXIT)) {
                db_debug("Exit event main loop...\n");
                break;
            }
            last_symbol = evt.key_symbol;
        }
    }
    return (void *)0;
}

static int buildin_tc_init(void)
{
    char name[32];
    char display_name[64];

    memset(&tp_data, 0, sizeof(struct item_data));
    strncpy(name, "tp", 32);
    if (script_fetch(name, "display_name", (int *)display_name, sizeof(display_name) / 4)) {
        strncpy(tp_data.display_name, TP_DISPLAY_NAME, 64);
    }
    else {
        strncpy(tp_data.display_name, display_name, 64);
    }

    tp_data.category = CATEGORY_MANUAL;
    tp_data.status   = -1;
    df_insert_item(BUILDIN_TC_ID_TP, &tp_data);

    return 0;
}

#define test_bit(bit, array) ((array)[(bit) / 8] & (1 << ((bit) % 8)))

int get_tp_size(void)
{
	const char *dirname = "/dev/input";
	DIR *dir = opendir(dirname);
	struct dirent *de;
	char *filename;
	char devname[64];
    char device_name[64];
    char read_device_name[80];
	int fd = -1;
	if (dir == NULL) {
        return -1;
    }
	if (script_fetch("tp", "device_name", (int *)device_name, sizeof(device_name) / 4)) {
		db_error("tp device_name not found!\n");
		return -1;
	}

	uint8_t keyBitmask[(KEY_MAX + 1) / 8];
	uint8_t absBitmask[(ABS_MAX + 1) / 8];
	strcpy(devname, dirname);
	filename = devname + strlen(devname);
	*filename++ = '/';
	while ((de = readdir(dir))) {
        if (de->d_name[0] == '.' &&
                (de->d_name[1] == '\0' ||
                (de->d_name[1] == '.' && de->d_name[2] == '\0'))) {
                    continue;
        }
		strcpy(filename, de->d_name);
		fd = open(devname, O_RDONLY);
		if (fd < 0) {
            continue;
        }
		if (ioctl(fd, EVIOCGNAME(sizeof(read_device_name) - 1), &read_device_name) < 1) {
			read_device_name[0] = '\0';
		}
		if (strcmp(device_name, read_device_name) == 0) {
			db_msg("found tp device name is %s \n", read_device_name);
		}
        else {
			close(fd);
			continue;
		}

		ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBitmask)), keyBitmask);
		ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBitmask)), absBitmask);

		if (test_bit(BTN_TOUCH, keyBitmask) &&
				(test_bit(ABS_MT_POSITION_X, absBitmask) || test_bit(ABS_X, absBitmask)) &&
				(test_bit(ABS_MT_POSITION_Y, absBitmask) || test_bit(ABS_Y, absBitmask))) {
			struct input_absinfo info;
			if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &info)) {
				printf("can not get touchscreen abs-x info : %s\n", devname);
			}
            else {
				mTouchScreenWidth = info.maximum;
			}
			if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &info)) {
				printf("can not get touchscreen abs-y info : %s\n", devname);
			}
            else {
				mTouchScreenHeight = info.maximum;
			}
			close(fd);
			return 0;
		}
		close(fd);
	}
    db_msg("can't found tp device %s\n", device_name);
	return -1;
}

int df_view_init(void)
{
    int ret;
    db_msg("directfb view init...\n");
    df_windows_init();
    df_view_id = register_view("directfb", &df_view_ops);
    if (df_view_id == 0) {
        return -1;
    }
    if (disp_output_type_t==DISP_OUTPUT_TYPE_LCD && tp_type != -1) {
        db_msg("buildin_tc_init...\n");
        buildin_tc_init();
        if (!get_tp_size()) {
            /* get touchscreen */
            DFBCHECK(dfb->GetInputDevice(dfb, 1, &tp_dev));
            /* create an event buffer for touchscreen */
            DFBCHECK(tp_dev->CreateEventBuffer(tp_dev, &events));

            /* create event mainloop */
            ret = pthread_create(&evt_tid, NULL, event_mainloop, NULL);
            if (ret != 0) {
                 db_error("create event mainloop failed\n");
                 unregister_view(df_view_id);
                 return -1;
            }
        }
        else {
            tp_data.status = STATUS_FAIL;
            df_update_item(BUILDIN_TC_ID_TP, &tp_data);
        }
    }
    return 0;
}

int df_view_exit(void)
{
    unregister_view(df_view_id);
    return 0;
}


static int handle_confirm_init(void)
{
    db_debug("confirm button init...\n");

    if (script_fetch("df_view", "confirm_button_name",
		     (int *)confirm_button.name, 4)) {
	    strcpy(confirm_button.name, CONFIRM_BUTTON_NAME);
    }
    confirm_button.width  = BUTTON_WIDTH;
    confirm_button.height = BUTTON_HEIGHT;
    confirm_button.x      = misc_window.desc.width/3*2+(misc_window.desc.width/3-confirm_button.width)/2;
    confirm_button.y      = 0;

    confirm_button.bdcolor = COLOR_GREEN_IDX;
    confirm_button.bgcolor = COLOR_BLUE_IDX;
    confirm_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[confirm_button.bdcolor].r,
                                           color_table[confirm_button.bdcolor].g,
                                           color_table[confirm_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, confirm_button.x,
                                                                confirm_button.y,
                                                                confirm_button.x,
                                                                confirm_button.y + confirm_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, confirm_button.x,
                                                                confirm_button.y,
                                                                confirm_button.x + confirm_button.width - 1,
                                                                confirm_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, confirm_button.x + confirm_button.width - 1,
                                                                confirm_button.y,
                                                                confirm_button.x + confirm_button.width - 1,
                                                                confirm_button.y + confirm_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, confirm_button.x,
                                                                confirm_button.y + confirm_button.height - 1,
                                                                confirm_button.x + confirm_button.width - 1,
                                                                confirm_button.y + confirm_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[confirm_button.bgcolor].r,
                                           color_table[confirm_button.bgcolor].g,
                                           color_table[confirm_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, confirm_button.x + 1,
                                                                     confirm_button.y + 1,
                                                                     confirm_button.width - 2,
                                                                     confirm_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[confirm_button.fgcolor].r,
                                           color_table[confirm_button.fgcolor].g,
                                           color_table[confirm_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, confirm_button.name, -1, confirm_button.x + (confirm_button.width >> 1),
               (confirm_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int confirm_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[confirm_button.fgcolor].r,
                                           color_table[confirm_button.fgcolor].g,
                                           color_table[confirm_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, confirm_button.x + 1,
                                                                     confirm_button.y + 1,
                                                                     confirm_button.width - 2,
                                                                     confirm_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[confirm_button.bdcolor].r,
                                           color_table[confirm_button.bdcolor].g,
                                           color_table[confirm_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, confirm_button.name, -1, confirm_button.x + (confirm_button.width >> 1),
                (confirm_button.y+MENU_HEIGHT/4 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int handle_motor_init(void)
{
    db_debug("motorpass button init...\n");

    if (script_fetch("df_view", "motor_button_name",
		     (int *)motor_button.name, 4)) {
	    strcpy(motor_button.name, MOTOR_BUTTON_NAME);
    }

    motor_button.width  = BUTTON_WIDTH;
    motor_button.height = BUTTON_HEIGHT;
    motor_button.x      = (misc_window.desc.width/3-motor_button.width)/2;
    motor_button.y      = motor_button.height*10;

    motor_button.bdcolor = COLOR_GREEN_IDX;
    motor_button.bgcolor = COLOR_BLUE_IDX;
    motor_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[motor_button.bdcolor].r,
                                           color_table[motor_button.bdcolor].g,
                                           color_table[motor_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, motor_button.x,
                                                                motor_button.y,
                                                                motor_button.x,
                                                                motor_button.y + motor_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, motor_button.x,
                                                                motor_button.y,
                                                                motor_button.x + motor_button.width - 1,
                                                                motor_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, motor_button.x + motor_button.width - 1,
                                                                motor_button.y,
                                                                motor_button.x + motor_button.width - 1,
                                                                motor_button.y + motor_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, motor_button.x,
                                                                motor_button.y + motor_button.height - 1,
                                                                motor_button.x + motor_button.width - 1,
                                                                motor_button.y + motor_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[motor_button.bgcolor].r,
                                           color_table[motor_button.bgcolor].g,
                                           color_table[motor_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, motor_button.x + 1,
                                                                     motor_button.y + 1,
                                                                     motor_button.width - 2,
                                                                     motor_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[motor_button.fgcolor].r,
                                           color_table[motor_button.fgcolor].g,
                                           color_table[motor_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, motor_button.name, -1, motor_button.x + (motor_button.width >> 1),
               (motor_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int motor_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[motor_button.fgcolor].r,
                                           color_table[motor_button.fgcolor].g,
                                           color_table[motor_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, motor_button.x + 1,
                                                                     motor_button.y + 1,
                                                                     motor_button.width - 2,
                                                                     motor_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[motor_button.bdcolor].r,
                                           color_table[motor_button.bdcolor].g,
                                           color_table[motor_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, motor_button.name, -1, motor_button.x + (motor_button.width >> 1),
                (motor_button.y+MENU_HEIGHT/4 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int handle_tp_pass_init(void)
{
    db_debug("tp_pass button init...\n");

    if (script_fetch("df_view", "tp_pass_button_name",
		     (int *)tp_pass_button.name, 4)) {
	    strcpy(tp_pass_button.name, TP_PASS_BUTTON_NAME);
    }

    tp_pass_button.width  = BUTTON_WIDTH;
    tp_pass_button.height = BUTTON_HEIGHT;
    tp_pass_button.x      = (misc_window.desc.width - tp_pass_button.width)/2;
    tp_pass_button.y      = tp_pass_button.height*4;

    tp_pass_button.bdcolor = COLOR_GREEN_IDX;
    tp_pass_button.bgcolor = COLOR_BLUE_IDX;
    tp_pass_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[tp_pass_button.bdcolor].r,
                                           color_table[tp_pass_button.bdcolor].g,
                                           color_table[tp_pass_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, tp_pass_button.x,
                                                                tp_pass_button.y,
                                                                tp_pass_button.x,
                                                                tp_pass_button.y + tp_pass_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, tp_pass_button.x,
                                                                tp_pass_button.y,
                                                                tp_pass_button.x + tp_pass_button.width - 1,
                                                                tp_pass_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, tp_pass_button.x + tp_pass_button.width - 1,
                                                                tp_pass_button.y,
                                                                tp_pass_button.x + tp_pass_button.width - 1,
                                                                tp_pass_button.y + tp_pass_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, tp_pass_button.x,
                                                                tp_pass_button.y + tp_pass_button.height - 1,
                                                                tp_pass_button.x + tp_pass_button.width - 1,
                                                                tp_pass_button.y + tp_pass_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[tp_pass_button.bgcolor].r,
                                           color_table[tp_pass_button.bgcolor].g,
                                           color_table[tp_pass_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, tp_pass_button.x + 1,
                                                                     tp_pass_button.y + 1,
                                                                     tp_pass_button.width - 2,
                                                                     tp_pass_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[tp_pass_button.fgcolor].r,
                                           color_table[tp_pass_button.fgcolor].g,
                                           color_table[tp_pass_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, tp_pass_button.name, -1, tp_pass_button.x + (tp_pass_button.width >> 1),
               (tp_pass_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int tp_pass_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[tp_pass_button.fgcolor].r,
                                           color_table[tp_pass_button.fgcolor].g,
                                           color_table[tp_pass_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, tp_pass_button.x + 1,
                                                                     tp_pass_button.y + 1,
                                                                     tp_pass_button.width - 2,
                                                                     tp_pass_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[tp_pass_button.bdcolor].r,
                                           color_table[tp_pass_button.bdcolor].g,
                                           color_table[tp_pass_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, tp_pass_button.name, -1, tp_pass_button.x + (tp_pass_button.width >> 1),
                (tp_pass_button.y+MENU_HEIGHT/4 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int handle_tp_fail_init(void)
{
    db_debug("tp_fail button init...\n");

    if (script_fetch("df_view", "tp_fail_button_name",
		     (int *)tp_fail_button.name, 4)) {
	    strcpy(tp_fail_button.name, TP_FAIL_BUTTON_NAME);
    }

    tp_fail_button.width  = BUTTON_WIDTH;
    tp_fail_button.height = BUTTON_HEIGHT;
    tp_fail_button.x      = misc_window.desc.width/3*2+(misc_window.desc.width/3-tp_fail_button.width)/2;
    tp_fail_button.y      = tp_fail_button.height*4;

    tp_fail_button.bdcolor = COLOR_GREEN_IDX;
    tp_fail_button.bgcolor = COLOR_BLUE_IDX;
    tp_fail_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[tp_fail_button.bdcolor].r,
                                           color_table[tp_fail_button.bdcolor].g,
                                           color_table[tp_fail_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, tp_fail_button.x,
                                                                tp_fail_button.y,
                                                                tp_fail_button.x,
                                                                tp_fail_button.y + tp_fail_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, tp_fail_button.x,
                                                                tp_fail_button.y,
                                                                tp_fail_button.x + tp_fail_button.width - 1,
                                                                tp_fail_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, tp_fail_button.x + tp_fail_button.width - 1,
                                                                tp_fail_button.y,
                                                                tp_fail_button.x + tp_fail_button.width - 1,
                                                                tp_fail_button.y + tp_fail_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, tp_fail_button.x,
                                                                tp_fail_button.y + tp_fail_button.height - 1,
                                                                tp_fail_button.x + tp_fail_button.width - 1,
                                                                tp_fail_button.y + tp_fail_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[tp_fail_button.bgcolor].r,
                                           color_table[tp_fail_button.bgcolor].g,
                                           color_table[tp_fail_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, tp_fail_button.x + 1,
                                                                     tp_fail_button.y + 1,
                                                                     tp_fail_button.width - 2,
                                                                     tp_fail_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[tp_fail_button.fgcolor].r,
                                           color_table[tp_fail_button.fgcolor].g,
                                           color_table[tp_fail_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, tp_fail_button.name, -1, tp_fail_button.x + (tp_fail_button.width >> 1),
               (tp_fail_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int tp_fail_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[tp_fail_button.fgcolor].r,
                                           color_table[tp_fail_button.fgcolor].g,
                                           color_table[tp_fail_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, tp_fail_button.x + 1,
                                                                     tp_fail_button.y + 1,
                                                                     tp_fail_button.width - 2,
                                                                     tp_fail_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[tp_fail_button.bdcolor].r,
                                           color_table[tp_fail_button.bdcolor].g,
                                           color_table[tp_fail_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, tp_fail_button.name, -1, tp_fail_button.x + (tp_fail_button.width >> 1),
                (tp_fail_button.y+MENU_HEIGHT/4 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int handle_gsensor_pass_init(void)
{
    db_debug("gsensor_pass button init...\n");

    if (script_fetch("df_view", "gsensor_pass_button_name",
		     (int *)gsensor_pass_button.name, 4)) {
	    strcpy(gsensor_pass_button.name, GSENSOR_PASS_BUTTON_NAME);
    }

    gsensor_pass_button.width  = BUTTON_WIDTH;
    gsensor_pass_button.height = BUTTON_HEIGHT;
    gsensor_pass_button.x      = (misc_window.desc.width - gsensor_pass_button.width)/2;
    gsensor_pass_button.y      = gsensor_pass_button.height*6;

    gsensor_pass_button.bdcolor = COLOR_GREEN_IDX;
    gsensor_pass_button.bgcolor = COLOR_BLUE_IDX;
    gsensor_pass_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[gsensor_pass_button.bdcolor].r,
                                           color_table[gsensor_pass_button.bdcolor].g,
                                           color_table[gsensor_pass_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, gsensor_pass_button.x,
                                                                gsensor_pass_button.y,
                                                                gsensor_pass_button.x,
                                                                gsensor_pass_button.y + gsensor_pass_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, gsensor_pass_button.x,
                                                                gsensor_pass_button.y,
                                                                gsensor_pass_button.x + gsensor_pass_button.width - 1,
                                                                gsensor_pass_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, gsensor_pass_button.x + gsensor_pass_button.width - 1,
                                                                gsensor_pass_button.y,
                                                                gsensor_pass_button.x + gsensor_pass_button.width - 1,
                                                                gsensor_pass_button.y + gsensor_pass_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, gsensor_pass_button.x,
                                                                gsensor_pass_button.y + gsensor_pass_button.height - 1,
                                                                gsensor_pass_button.x + gsensor_pass_button.width - 1,
                                                                gsensor_pass_button.y + gsensor_pass_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[gsensor_pass_button.bgcolor].r,
                                           color_table[gsensor_pass_button.bgcolor].g,
                                           color_table[gsensor_pass_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, gsensor_pass_button.x + 1,
                                                                     gsensor_pass_button.y + 1,
                                                                     gsensor_pass_button.width - 2,
                                                                     gsensor_pass_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[gsensor_pass_button.fgcolor].r,
                                           color_table[gsensor_pass_button.fgcolor].g,
                                           color_table[gsensor_pass_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, gsensor_pass_button.name, -1, gsensor_pass_button.x + (gsensor_pass_button.width >> 1),
               (gsensor_pass_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int gsensor_pass_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[gsensor_pass_button.fgcolor].r,
                                           color_table[gsensor_pass_button.fgcolor].g,
                                           color_table[gsensor_pass_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, gsensor_pass_button.x + 1,
                                                                     gsensor_pass_button.y + 1,
                                                                     gsensor_pass_button.width - 2,
                                                                     gsensor_pass_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[gsensor_pass_button.bdcolor].r,
                                           color_table[gsensor_pass_button.bdcolor].g,
                                           color_table[gsensor_pass_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, gsensor_pass_button.name, -1, gsensor_pass_button.x + (gsensor_pass_button.width >> 1),
                (gsensor_pass_button.y+MENU_HEIGHT/4 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);
    return 0;
}

static int handle_gsensor_fail_init(void)
{
    db_debug("gsensor_fail button init...\n");

    if (script_fetch("df_view", "gsensor_fail_button_name",
		     (int *)gsensor_fail_button.name, 4)) {
	    strcpy(gsensor_fail_button.name, GSENSOR_FAIL_BUTTON_NAME);
    }

    gsensor_fail_button.width  = BUTTON_WIDTH;
    gsensor_fail_button.height = BUTTON_HEIGHT;
    gsensor_fail_button.x      = misc_window.desc.width/3*2+(misc_window.desc.width/3-gsensor_fail_button.width)/2;
    gsensor_fail_button.y      = gsensor_fail_button.height*6;

    gsensor_fail_button.bdcolor = COLOR_GREEN_IDX;
    gsensor_fail_button.bgcolor = COLOR_BLUE_IDX;
    gsensor_fail_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[gsensor_fail_button.bdcolor].r,
                                           color_table[gsensor_fail_button.bdcolor].g,
                                           color_table[gsensor_fail_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, gsensor_fail_button.x,
                                                                gsensor_fail_button.y,
                                                                gsensor_fail_button.x,
                                                                gsensor_fail_button.y + gsensor_fail_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, gsensor_fail_button.x,
                                                                gsensor_fail_button.y,
                                                                gsensor_fail_button.x + gsensor_fail_button.width - 1,
                                                                gsensor_fail_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, gsensor_fail_button.x + gsensor_fail_button.width - 1,
                                                                gsensor_fail_button.y,
                                                                gsensor_fail_button.x + gsensor_fail_button.width - 1,
                                                                gsensor_fail_button.y + gsensor_fail_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, gsensor_fail_button.x,
                                                                gsensor_fail_button.y + gsensor_fail_button.height - 1,
                                                                gsensor_fail_button.x + gsensor_fail_button.width - 1,
                                                                gsensor_fail_button.y + gsensor_fail_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[gsensor_fail_button.bgcolor].r,
                                           color_table[gsensor_fail_button.bgcolor].g,
                                           color_table[gsensor_fail_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, gsensor_fail_button.x + 1,
                                                                     gsensor_fail_button.y + 1,
                                                                     gsensor_fail_button.width - 2,
                                                                     gsensor_fail_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[gsensor_fail_button.fgcolor].r,
                                           color_table[gsensor_fail_button.fgcolor].g,
                                           color_table[gsensor_fail_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, gsensor_fail_button.name, -1, gsensor_fail_button.x + (gsensor_fail_button.width >> 1),
               (gsensor_fail_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int gsensor_fail_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[gsensor_fail_button.fgcolor].r,
                                           color_table[gsensor_fail_button.fgcolor].g,
                                           color_table[gsensor_fail_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, gsensor_fail_button.x + 1,
                                                                     gsensor_fail_button.y + 1,
                                                                     gsensor_fail_button.width - 2,
                                                                     gsensor_fail_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[gsensor_fail_button.bdcolor].r,
                                           color_table[gsensor_fail_button.bdcolor].g,
                                           color_table[gsensor_fail_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, gsensor_fail_button.name, -1, gsensor_fail_button.x + (gsensor_fail_button.width >> 1),
                (gsensor_fail_button.y+MENU_HEIGHT/4 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int handle_mic_start_init(void)
{
    db_debug("mic_start button init...\n");

    if (script_fetch("df_view", "mic_start_button_name",
		     (int *)mic_start_button.name, 4)) {
	    strcpy(mic_start_button.name, MIC_START_BUTTON_NAME);
    }

    mic_start_button.width  = BUTTON_WIDTH;
    mic_start_button.height = BUTTON_HEIGHT;
    mic_start_button.x      = (misc_window.desc.width/3-mic_start_button.width)/2;
    mic_start_button.y      = mic_start_button.height*8;

    mic_start_button.bdcolor = COLOR_GREEN_IDX;
    mic_start_button.bgcolor = COLOR_BLUE_IDX;
    mic_start_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_start_button.bdcolor].r,
                                           color_table[mic_start_button.bdcolor].g,
                                           color_table[mic_start_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_start_button.x,
                                                                mic_start_button.y,
                                                                mic_start_button.x,
                                                                mic_start_button.y + mic_start_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_start_button.x,
                                                                mic_start_button.y,
                                                                mic_start_button.x + mic_start_button.width - 1,
                                                                mic_start_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_start_button.x + mic_start_button.width - 1,
                                                                mic_start_button.y,
                                                                mic_start_button.x + mic_start_button.width - 1,
                                                                mic_start_button.y + mic_start_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_start_button.x,
                                                                mic_start_button.y + mic_start_button.height - 1,
                                                                mic_start_button.x + mic_start_button.width - 1,
                                                                mic_start_button.y + mic_start_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_start_button.bgcolor].r,
                                           color_table[mic_start_button.bgcolor].g,
                                           color_table[mic_start_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, mic_start_button.x + 1,
                                                                     mic_start_button.y + 1,
                                                                     mic_start_button.width - 2,
                                                                     mic_start_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_start_button.fgcolor].r,
                                           color_table[mic_start_button.fgcolor].g,
                                           color_table[mic_start_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, mic_start_button.name, -1, mic_start_button.x + (mic_start_button.width >> 1),
               (mic_start_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int mic_start_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_start_button.fgcolor].r,
                                           color_table[mic_start_button.fgcolor].g,
                                           color_table[mic_start_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, mic_start_button.x + 1,
                                                                     mic_start_button.y + 1,
                                                                     mic_start_button.width - 2,
                                                                     mic_start_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_start_button.bdcolor].r,
                                           color_table[mic_start_button.bdcolor].g,
                                           color_table[mic_start_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, mic_start_button.name, -1, mic_start_button.x + (mic_start_button.width >> 1),
                (mic_start_button.y+MENU_HEIGHT/4 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int handle_mic_pass_init(void)
{
    db_debug("mic_pass button init...\n");

    if (script_fetch("df_view", "mic_pass_button_name",
		     (int *)mic_pass_button.name, 4)) {
	    strcpy(mic_pass_button.name, MIC_PASS_BUTTON_NAME);
    }

    mic_pass_button.width  = BUTTON_WIDTH;
    mic_pass_button.height = BUTTON_HEIGHT;
    mic_pass_button.x      = (misc_window.desc.width -  mic_pass_button.width)/2;
    mic_pass_button.y      = mic_pass_button.height*8;

    mic_pass_button.bdcolor = COLOR_GREEN_IDX;
    mic_pass_button.bgcolor = COLOR_BLUE_IDX;
    mic_pass_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_pass_button.bdcolor].r,
                                           color_table[mic_pass_button.bdcolor].g,
                                           color_table[mic_pass_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_pass_button.x,
                                                                mic_pass_button.y,
                                                                mic_pass_button.x,
                                                                mic_pass_button.y + mic_pass_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_pass_button.x,
                                                                mic_pass_button.y,
                                                                mic_pass_button.x + mic_pass_button.width - 1,
                                                                mic_pass_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_pass_button.x + mic_pass_button.width - 1,
                                                                mic_pass_button.y,
                                                                mic_pass_button.x + mic_pass_button.width - 1,
                                                                mic_pass_button.y + mic_pass_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_pass_button.x,
                                                                mic_pass_button.y + mic_pass_button.height - 1,
                                                                mic_pass_button.x + mic_pass_button.width - 1,
                                                                mic_pass_button.y + mic_pass_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_pass_button.bgcolor].r,
                                           color_table[mic_pass_button.bgcolor].g,
                                           color_table[mic_pass_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, mic_pass_button.x + 1,
                                                                     mic_pass_button.y + 1,
                                                                     mic_pass_button.width - 2,
                                                                     mic_pass_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_pass_button.fgcolor].r,
                                           color_table[mic_pass_button.fgcolor].g,
                                           color_table[mic_pass_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, mic_pass_button.name, -1, mic_pass_button.x + (mic_pass_button.width >> 1),
               (mic_pass_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int mic_pass_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_pass_button.fgcolor].r,
                                           color_table[mic_pass_button.fgcolor].g,
                                           color_table[mic_pass_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, mic_pass_button.x + 1,
                                                                     mic_pass_button.y + 1,
                                                                     mic_pass_button.width - 2,
                                                                     mic_pass_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_pass_button.bdcolor].r,
                                           color_table[mic_pass_button.bdcolor].g,
                                           color_table[mic_pass_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, mic_pass_button.name, -1, mic_pass_button.x + (mic_pass_button.width >> 1),
                (mic_pass_button.y+MENU_HEIGHT/4 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int handle_mic_fail_init(void)
{
    db_debug("mic_fail button init...\n");

    if (script_fetch("df_view", "mic_fail_button_name",
		     (int *)mic_fail_button.name, 4)) {
	    strcpy(mic_fail_button.name, MIC_FAIL_BUTTON_NAME);
    }

    mic_fail_button.width  = BUTTON_WIDTH;
    mic_fail_button.height = BUTTON_HEIGHT;
    mic_fail_button.x      = misc_window.desc.width/3*2+(misc_window.desc.width/3-mic_fail_button.width)/2;
    mic_fail_button.y      = mic_fail_button.height*8;

    mic_fail_button.bdcolor = COLOR_GREEN_IDX;
    mic_fail_button.bgcolor = COLOR_BLUE_IDX;
    mic_fail_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_fail_button.bdcolor].r,
                                           color_table[mic_fail_button.bdcolor].g,
                                           color_table[mic_fail_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_fail_button.x,
                                                                mic_fail_button.y,
                                                                mic_fail_button.x,
                                                                mic_fail_button.y + mic_fail_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_fail_button.x,
                                                                mic_fail_button.y,
                                                                mic_fail_button.x + mic_fail_button.width - 1,
                                                                mic_fail_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_fail_button.x + mic_fail_button.width - 1,
                                                                mic_fail_button.y,
                                                                mic_fail_button.x + mic_fail_button.width - 1,
                                                                mic_fail_button.y + mic_fail_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, mic_fail_button.x,
                                                                mic_fail_button.y + mic_fail_button.height - 1,
                                                                mic_fail_button.x + mic_fail_button.width - 1,
                                                                mic_fail_button.y + mic_fail_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_fail_button.bgcolor].r,
                                           color_table[mic_fail_button.bgcolor].g,
                                           color_table[mic_fail_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, mic_fail_button.x + 1,
                                                                     mic_fail_button.y + 1,
                                                                     mic_fail_button.width - 2,
                                                                     mic_fail_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_fail_button.fgcolor].r,
                                           color_table[mic_fail_button.fgcolor].g,
                                           color_table[mic_fail_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, mic_fail_button.name, -1, mic_fail_button.x + (mic_fail_button.width >> 1),
               (mic_fail_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int mic_fail_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_fail_button.fgcolor].r,
                                           color_table[mic_fail_button.fgcolor].g,
                                           color_table[mic_fail_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, mic_fail_button.x + 1,
                                                                     mic_fail_button.y + 1,
                                                                     mic_fail_button.width - 2,
                                                                     mic_fail_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[mic_fail_button.bdcolor].r,
                                           color_table[mic_fail_button.bdcolor].g,
                                           color_table[mic_fail_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, mic_fail_button.name, -1, mic_fail_button.x + (mic_fail_button.width >> 1),
                (mic_fail_button.y+MENU_HEIGHT/4 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int handle_flashlight_init(void)
{
    db_debug("flashlight button init...\n");

    if (script_fetch("df_view", "flashlight_button_name",
		     (int *)flashlight_button.name, 4)) {
	    strcpy(flashlight_button.name, FLASHLIGHT_BUTTON_NAME);
    }

    flashlight_button.width  = BUTTON_WIDTH;
    flashlight_button.height = BUTTON_HEIGHT;
    flashlight_button.x      = misc_window.desc.width/3*2+(misc_window.desc.width/3-flashlight_button.width)/2;
    flashlight_button.y      = flashlight_button.height*2;

    flashlight_button.bdcolor = COLOR_GREEN_IDX;
    flashlight_button.bgcolor = COLOR_BLUE_IDX;
    flashlight_button.fgcolor = COLOR_WHITE_IDX;

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[flashlight_button.bdcolor].r,
                                           color_table[flashlight_button.bdcolor].g,
                                           color_table[flashlight_button.bdcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, flashlight_button.x,
                                                                flashlight_button.y,
                                                                flashlight_button.x,
                                                                flashlight_button.y + flashlight_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, flashlight_button.x,
                                                                flashlight_button.y,
                                                                flashlight_button.x + flashlight_button.width - 1,
                                                                flashlight_button.y));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, flashlight_button.x + flashlight_button.width - 1,
                                                                flashlight_button.y,
                                                                flashlight_button.x + flashlight_button.width - 1,
                                                                flashlight_button.y + flashlight_button.height - 1));
    DFBCHECK(misc_window.surface->DrawLine(misc_window.surface, flashlight_button.x,
                                                                flashlight_button.y + flashlight_button.height - 1,
                                                                flashlight_button.x + flashlight_button.width - 1,
                                                                flashlight_button.y + flashlight_button.height - 1));

    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[flashlight_button.bgcolor].r,
                                           color_table[flashlight_button.bgcolor].g,
                                           color_table[flashlight_button.bgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, flashlight_button.x + 1,
                                                                     flashlight_button.y + 1,
                                                                     flashlight_button.width - 2,
                                                                     flashlight_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[flashlight_button.fgcolor].r,
                                           color_table[flashlight_button.fgcolor].g,
                                           color_table[flashlight_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, flashlight_button.name, -1, flashlight_button.x + (flashlight_button.width >> 1),
               (flashlight_button.y+ MENU_HEIGHT / 2 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}

static int flashlight_button_redraw(int color)
{
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[color].r,
                                           color_table[color].g,
                                           color_table[color].b, 0xff));
    DFBCHECK(misc_window.surface->FillRectangle(misc_window.surface, flashlight_button.x + 1,
                                                                     flashlight_button.y + 1,
                                                                     flashlight_button.width - 2,
                                                                     flashlight_button.height - 2));

    DFBCHECK(misc_window.surface->SetFont(misc_window.surface, font));
    DFBCHECK(misc_window.surface->SetColor(misc_window.surface,
                                           color_table[flashlight_button.fgcolor].r,
                                           color_table[flashlight_button.fgcolor].g,
                                           color_table[flashlight_button.fgcolor].b, 0xff));
    DFBCHECK(misc_window.surface->DrawString(misc_window.surface, flashlight_button.name, -1, flashlight_button.x + (flashlight_button.width >> 1),
                (flashlight_button.y+MENU_HEIGHT/4 + FONT48_HEIGHT / 2 - FONT48_HEIGHT / 8), DSTF_CENTER));

    misc_window.surface->Flip(misc_window.surface, NULL, 0);

    return 0;
}
