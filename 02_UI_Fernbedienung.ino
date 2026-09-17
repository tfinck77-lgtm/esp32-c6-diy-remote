#include "lcd_bsp.h"
#include "lcd_bl_pwm_bsp.h"
#include "ir_bsp.h"
void setup()
{
  Serial.begin(115200);
  lcd_lvgl_Init();
  setUpdutySubdivide(LCD_GPIO_MODE,LCD_GPIO_MODE_ON);
  ir_bsp_init();
}
void loop()
{
  //delay(1000);
  //setUpdutySubdivide(LCD_PWM_MODE,LCD_PWM_MODE_255);
  //delay(1000);
  //setUpdutySubdivide(LCD_PWM_MODE,LCD_PWM_MODE_200);
  //delay(1000);
  //setUpdutySubdivide(LCD_PWM_MODE,LCD_PWM_MODE_150);
  //delay(1000);
  //setUpdutySubdivide(LCD_PWM_MODE,LCD_PWM_MODE_100);
  //delay(1000);
  //setUpdutySubdivide(LCD_PWM_MODE,LCD_PWM_MODE_50);
  //delay(1000);
  //setUpdutySubdivide(LCD_PWM_MODE,LCD_PWM_MODE_0);
}