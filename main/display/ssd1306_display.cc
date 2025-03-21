#include "ssd1306_display.h"
#include "font_awesome_symbols.h"

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lvgl_port.h>

#define TAG "Ssd1306Display"

#define EYE_SIZE 40
#define EYE_BLINK_FREQ 80
#define EYE_GAP 25
#define EYE_RADIUS 5

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_30_1);
LV_FONT_DECLARE(font_awesome_14_1);

Ssd1306Display::Ssd1306Display(void* i2c_master_handle, int width, int height, bool mirror_x, bool mirror_y)
    : mirror_x_(mirror_x), mirror_y_(mirror_y) {
    width_ = width;
    height_ = height;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    // SSD1306 config
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = 0x3C,
        .on_color_trans_done = nullptr,
        .user_ctx = nullptr,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .dc_low_on_data = 0,
            .disable_control_phase = 0,
        },
        .scl_speed_hz = 100 * 1000,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2((i2c_master_bus_t*)i2c_master_handle, &io_config, &panel_io_));

    ESP_LOGI(TAG, "Install SSD1306 driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = -1;
    panel_config.bits_per_pixel = 1;

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = static_cast<uint8_t>(height_),
    };
    panel_config.vendor_config = &ssd1306_config;

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
    ESP_LOGI(TAG, "SSD1306 driver installed");

    // Reset the display
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
    if (esp_lcd_panel_init(panel_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x_,
            .mirror_y = mirror_y_,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    disp_ = lvgl_port_add_disp(&display_cfg);

    if (disp_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }
}

Ssd1306Display::~Ssd1306Display() {
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool Ssd1306Display::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void Ssd1306Display::Unlock() {
    lvgl_port_unlock();
}

void Ssd1306Display::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto screen = lv_disp_get_scr_act(disp_);
    lv_obj_set_style_text_font(screen, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

/*************************************************************************************/
    square1 = lv_obj_create(screen);
    lv_obj_set_size(square1, EYE_SIZE, EYE_SIZE);  // 容器大小设置为屏幕的宽高
    lv_obj_set_flex_flow(square1, LV_FLEX_FLOW_COLUMN);  // 容器设置为按行布局
    lv_obj_set_style_radius(square1, 4, 0);
    lv_obj_set_scrollbar_mode(square1, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(square1, lv_color_hex(0x00), 0);

    square2 = lv_obj_create(screen);
    lv_obj_set_size(square2, EYE_SIZE, EYE_SIZE);  // 容器大小设置为屏幕的宽高
    lv_obj_set_flex_flow(square2, LV_FLEX_FLOW_COLUMN);  // 容器设置为按行布局
    lv_obj_set_style_radius(square2, 4, 0);
    lv_obj_set_scrollbar_mode(square2, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(square2, lv_color_hex(0x00), 0);

    lv_obj_align(square1, LV_ALIGN_CENTER, EYE_GAP, 0);
    lv_obj_align(square2, LV_ALIGN_CENTER, -EYE_GAP, 0);
    
    cur_square1_x = lv_obj_get_x_aligned(square1);
    cur_square1_y = lv_obj_get_y_aligned(square1);
    cur_square2_x = lv_obj_get_x_aligned(square2);
    cur_square2_y = lv_obj_get_y_aligned(square2);

    lv_obj_add_flag(square1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(square2, LV_OBJ_FLAG_HIDDEN);

/*************************************************************************************/

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);  //LV_FLEX_FLOW_COLUMN 表示容器的元素应该按列的方式排列
    lv_obj_set_style_pad_all(container_, 0, 0);             //外边距
    lv_obj_set_style_border_width(container_, 0, 0);        //边框宽度
    lv_obj_set_style_pad_row(container_, 0, 0);             //行间距

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 18);           //宽度设置为屏幕宽度 (LV_HOR_RES)，高度设置为 18 像素
    lv_obj_set_style_radius(status_bar_, 0, 0);             //确保状态栏没有圆角
    
    /* Content */
    content_ = lv_obj_create(container_);                               
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);         //确保 content_ 区域的内容不会显示滚动条
    lv_obj_set_style_radius(status_bar_, 0, 0);                         //确保状态栏没有圆角
    lv_obj_set_width(content_, LV_HOR_RES);                             //宽度为屏幕宽度128
    lv_obj_set_flex_grow(content_, 1);                                  //根据容器的剩余空间自动扩展并填充。

    emotion_label_ = lv_label_create(content_);                         
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);  
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);            //设置机器人图标
    lv_obj_center(emotion_label_);                                      //居中

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);                // 布局流动方向为水平方向
    lv_obj_set_style_pad_all(status_bar_, 0, 0);                        //所有边距（内边距）设置为 0
    lv_obj_set_style_border_width(status_bar_, 0, 0);                   //移除 status_bar_ 的边框
    lv_obj_set_style_pad_column(status_bar_, 0, 0);                     //垂直方向上的内边距设置为 0

    network_label_ = lv_label_create(status_bar_);                      //网络标签
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, &font_awesome_14_1, 0);

    notification_label_ = lv_label_create(status_bar_);                 //通知标签
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "通知");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_text(status_label_, "正在初始化");
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, &font_awesome_14_1, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, &font_awesome_14_1, 0);

    lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
}

//Bresenham 直线算法
int Ssd1306Display::bresenham_line(int x1, int y1, int x2, int y2,int ret[]) {
    // 计算直线的增量
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1; // 方向向量
    int sy = (y1 < y2) ? 1 : -1; // 方向向量
    int err = dx - dy; // 误差项
    int count = 0;
    while (1) {
        // 输出当前点
        ret[count] = x1;
        ret[count+1] = y1;
        count+=2;
        // 判断是否到达终点
        if (x1 == x2 && y1 == y2) {
            break;
        }
        // 计算误差并更新坐标
        int e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
    return count;
}

void Ssd1306Display::SetupUI_128x32() {
    DisplayLockGuard lock(this);

    auto screen = lv_disp_get_scr_act(disp_);
    lv_obj_set_style_text_font(screen, &font_puhui_14_1, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);

    /* Left side */
    side_bar_ = lv_obj_create(container_);
    lv_obj_set_flex_grow(side_bar_, 1);
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(side_bar_, 0, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_pad_row(side_bar_, 0, 0);

    /* Emotion label on the right side */
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, 32, 32);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    lv_obj_center(emotion_label_);

    /* Status bar */
    status_bar_ = lv_obj_create(side_bar_);
    lv_obj_set_size(status_bar_, LV_SIZE_CONTENT, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, &font_awesome_14_1, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, &font_awesome_14_1, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, &font_awesome_14_1, 0);

    status_label_ = lv_label_create(side_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_obj_set_width(status_label_, width_ - 32);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(status_label_, "正在初始化");

    notification_label_ = lv_label_create(side_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_width(notification_label_, width_ - 32);
    lv_label_set_long_mode(notification_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(notification_label_, "通知");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
}

void Ssd1306Display::to_any_position(int tar1x,int tar1y,int tarx2,int tary2)
{
    int ret1[100];
    int ret2[100];
    int num = bresenham_line(cur_square1_x, cur_square1_y, tar1x, tar1y,ret1);
    bresenham_line(cur_square2_x, cur_square2_y, tarx2, tary2,ret2);

    for (size_t i = 0; i < num; i+=2)
    {
        for (size_t j = 0; j < 5; j++)
        {
            lv_obj_align(square1, LV_ALIGN_CENTER, ret1[i], ret1[i+1]);
            lv_obj_align(square2, LV_ALIGN_CENTER, ret2[i], ret2[i+1]);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    //更新位置
    cur_square1_x = lv_obj_get_x_aligned(square1);
    cur_square1_y = lv_obj_get_y_aligned(square1);
    cur_square2_x = lv_obj_get_x_aligned(square2);
    cur_square2_y = lv_obj_get_y_aligned(square2);
}

void Ssd1306Display::close_eyes()
{
    for (size_t i = EYE_SIZE; i > 10; i--)
    {
        lv_obj_set_size(square1,EYE_SIZE+i*0.1,i);
        lv_obj_set_size(square2,EYE_SIZE+i*0.1,i);
        vTaskDelay(15 / portTICK_PERIOD_MS);
    }
}


void Ssd1306Display::open_eyes()
{
    for (size_t i = 10; i < EYE_SIZE; i++)
    {
        lv_obj_set_size(square1,EYE_SIZE,i);
        lv_obj_set_size(square2,EYE_SIZE,i);
        vTaskDelay(15 / portTICK_PERIOD_MS);
    }
}

void Ssd1306Display::blink_eyes()
{
    close_eyes();
    open_eyes();
}


void Ssd1306Display::blink_task()
{
    int rand_ = 0;
    int flag = 0;
    while(1)
    {
        srand(time(NULL));
        rand_ = rand() % 51;
        flag = rand() % 101;
        if(rand_ < 10)
            rand_ = 1;
        else if(rand_ > 10 && rand_ < 20)
            rand_ = 2;
        else if(rand_ > 20 && rand_ < 30)
            rand_ = 3;
        else if(rand_ > 30 && rand_ < 40)
            rand_ = 4;
        else if(rand_ > 40 && rand_ < 50)
            rand_ = 5;
        else
            rand_ = 6;
        vTaskDelay(rand_ * 1000 / portTICK_PERIOD_MS);
        if(flag < EYE_BLINK_FREQ)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            blink_eyes();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
void Ssd1306Display::eye_move_emtion_task()
{
    int x = 0;
    int y = 0;
    while (1)
    {
        srand(time(NULL));
        x = rand() % 31 - 15;
        y = rand() % 31 - 15;
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        to_any_position(EYE_GAP + x,0 + y,-EYE_GAP + x,0 + y);
    }
}

void Ssd1306Display::start_emtion()
{
    open_eyes();
    xTaskCreate([](void* arg)
    {
        auto this_ = (Ssd1306Display*)arg;
        this_->blink_task();
        vTaskDelete(NULL);
    }, "blink_task", 2048, this, 5, &blink_task_handle);

    xTaskCreate([](void* arg)
    {
        auto this_ = (Ssd1306Display*)arg;
        this_->eye_move_emtion_task();
        vTaskDelete(NULL);
    }, "eye_move_emtion_task", 3096, this, 5, &idle_task_handle);

    lv_obj_clear_flag(square1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(square2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
}

void Ssd1306Display::stop_emtion()
{
    if (blink_task_handle != NULL)
    {
        vTaskDelete(blink_task_handle);
    }
    if(idle_task_handle != NULL)
    {
        vTaskDelete(idle_task_handle);
    }
    
    lv_obj_add_flag(square1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(square2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
}

void Ssd1306Display::idle_emtion()
{
    if (blink_task_handle != NULL)
    {
        vTaskDelete(blink_task_handle);
    }
    if(idle_task_handle != NULL)
    {
        vTaskDelete(idle_task_handle);
    }
    if (square1 != NULL)
    {
        lv_obj_clear_flag(square1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(square2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    }

    to_any_position(EYE_GAP,0,-EYE_GAP,0);
    close_eyes();
}
