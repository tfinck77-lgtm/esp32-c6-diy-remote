#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

#define LCD_HOST    SPI2_HOST

#define Rotate 1    //Rotation 90
#define Normal 0    //Normal
#define Direction   Normal


#if (Direction == Normal) 
  #define EXAMPLE_LCD_H_RES 170   //宽度 水平分辨率
  #define EXAMPLE_LCD_V_RES 320   //高度 竖直分辨率
#elif (Direction == Rotate)
  #define EXAMPLE_LCD_H_RES 320   //宽度 水平分辨率
  #define EXAMPLE_LCD_V_RES 170   //高度 竖直分辨率
#endif

#define EXAMPLE_LCD_DMA_Line (EXAMPLE_LCD_V_RES / 2)

#define EXAMPLE_USE_Disp       1
#define EXAMPLE_USE_TOUCH      1
#define EXAMPLE_USE_SDCARD     0


#define PIN_NUM_MOSI 4
#define PIN_NUM_CLK  5
#define PIN_NUM_DC   6
#define PIN_NUM_CS   7
#define PIN_NUM_RST  14
#define PIN_NUM_BL   15

#define EXAMPLE_LVGL_BUF_HEIGHT        (EXAMPLE_LCD_V_RES / 4)
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2                          //Timer time
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500                        //LVGL Indicates the maximum time for a task to run
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1                          //LVGL Minimum time to run a task
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)                 //LVGL runs the task stack
#define EXAMPLE_LVGL_TASK_PRIORITY     2                          //LVGL Running task priority


#if EXAMPLE_USE_SDCARD
#define PIN_NUM_MISO 19
#define PIN_NUM_SDCS 20
#define SDlist "/sd_card"  // Directory, acts like a standard
#endif

#define I2C_Touch_ADDR 0x15
#define EXAMPLE_PIN_NUM_TOUCH_SCL 8
#define EXAMPLE_PIN_NUM_TOUCH_SDA 18

#endif