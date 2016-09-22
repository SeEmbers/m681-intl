#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>
#endif

#include <cust_gpio_usage.h>

#ifndef CONFIG_FPGA_EARLY_PORTING
#include <cust_i2c.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif
#ifndef BUILD_LK
extern int tps65132_write_bytes(unsigned char addr, unsigned char value);
#else
extern int TPS65132_write_byte(kal_uint8 addr, kal_uint8 value);
#endif


static const unsigned int BL_MIN_LEVEL = 20;
static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))



#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)


/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH										(1080)
#define FRAME_HEIGHT									(1920)

#ifndef CONFIG_FPGA_EARLY_PORTING
#define GPIO_65132_ENP_EN GPIO_LCD_BIAS_ENN_PIN
#define GPIO_65132_ENN_EN GPIO_LCD_BIAS_ENP_PIN
#endif

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x4F, 1, {0x01} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting[] = {
	
	//CMD2_P0																								  
	{0xFF,1,{0x01}},
	{0xFB,1,{0x01}},
												   
	{0x00,1,{0x01}},
	{0x01,1,{0x55}},
	{0x02,1,{0x40}},
	{0x05,1,{0x50}},
						  
	//{0x06,1,{0x94}},
	{0x06,1,{0x3A}},  //0x0A
	
	//VGL SEL TO -8V	 
	{0x07,1,{0x9E}},
	
	{0x08,1,{0x0C}},
						   
	//VCOM SEL				
	//{0x11,1,{0x16}},
	//{0x12,1,{0x16}},
	{0x13,1,{0x03}},	  
	
	//PRE=1 				
	{0x14 ,1,{0x94}},
									
	//AVDDR SEL TO 4.7V 	
	{0x15 ,1,{0x15}}, 
	//AVEER SEL TO -4.7V	
	{0x16 ,1,{0x15}},
	
	{0x6D,1,{ 0x22}},
						  
	//GVDDP SEL TO 4.3V 	
	{0x0B,1,{ 0x87}},
	//GVDDN SEL TO -4.3V	
	{0x0C ,1,{0x87}},
						   
	//VGHO SEL TO 8 V	  
	{0x0E ,1,{0xAB}},
	//VGLO SEL TO -8 V	  
	{0x0F ,1,{0xAE}},
											   
	//GATE_EQ				
	{0x58,1,{ 0x82}},					   
	{0x59 ,1,{0x02 }},	 
	{0x5A ,1,{0x02 }},
	{0x5B ,1,{0x02	}}, 
	{0x5C ,1,{0x82 }},	 
	{0x5D ,1,{0x82	}},  
	{0x5E ,1,{0x02 }},	
	{0x5F ,1,{0x02 }},	
															
	//CMD2_P4				
	{0xFF,1,{ 0x05}},
	{0xFB ,1,{0x01}},
				   
	//CGOUT 				
	{0x00,1,{ 0x00}},
	{0x01,1,{ 0x01}},
	{0x02,1,{ 0x0B}},
	{0x03,1,{ 0x0C}},
	{0x04,1,{ 0x00}},
	{0x05 ,1,{0x03}},
	{0x06 ,1,{0x00}},
	{0x07 ,1,{0x04}},
	{0x08 ,1,{0x80}},
	{0x09 ,1,{0x00}},
	{0x0A ,1,{0x00}},
	{0x0B ,1,{0x00}},
	{0x0C ,1,{0x00}},
	{0x0D ,1,{0x17}},
	{0x0E ,1,{0x15}},
	{0x0F ,1,{0x13}},
	{0x10 ,1,{0x00}},
	{0x11 ,1,{0x01}},
	{0x12 ,1,{0x0B}},
	{0x13 ,1,{0x0C}},
	{0x14 ,1,{0x00}},
	{0x15 ,1,{0x03}},
	{0x16 ,1,{0x00}},
	{0x17 ,1,{0x04}},
	{0x18 ,1,{0x80}},
	{0x19 ,1,{0x00}},
	{0x1A ,1,{0x00}},
	{0x1B ,1,{0x00}},
	{0x1C ,1,{0x00}},
	{0x1D ,1,{0x17}},
	{0x1E ,1,{0x15}},
	{0x1F ,1,{0x13}},
						   
	//STV				  
	{0x20 ,1,{0x09}},  
	{0x21 ,1,{0x02}},
	{0x22 ,1,{0x00}},
	{0x23 ,1,{0x00}},  
	{0x24 ,1,{0x00}},
	{0x25 ,1,{0x3D}},														
																																																						  
	//UD																								
	{0x29 ,1,{0x38}},
	{0x2A ,1,{0x2B}},
												
	//GCKS				  
	{0x2F ,1,{0x02}},
	{0x30 ,1,{0x01}}, 
	{0x31 ,1,{0x49}},
	{0x32 ,1,{0x23}}, 
	{0x33 ,1,{0x01}}, 
	{0x34 ,1,{0x05}},	
	{0x35 ,1,{0x74}},
	{0x36 ,1,{0x00}},	
	{0x37 ,1,{0x2D}},	
	{0x38 ,1,{0x08}},  
	//{0x92 ,1,{0x94  
	{0x92 ,1,{0x79}},													 
																											
	//CTRL1 																									   
	{0x5B ,1,{0x00}}, 
	{0x5F ,1,{0x1D}}, 
	{0x63 ,1,{0x00}}, 
	{0x67 ,1,{0x04}}, 
	{0x6C ,1,{0x55}}, 
																											
	//MUX																								  
	{0x7A ,1,{0x00}}, 
	{0x7B ,1,{0xA2}}, 
	{0x7C ,1,{0xD8}}, 
	{0x7D ,1,{0x50}}, 
	
	{0x7E ,1,{0x0A}},
	{0x7F ,1,{0x1C}}, 
	
	{0x81 ,1,{0x06}},	
	{0x82 ,1,{0x02}},  
	{0x84 ,1,{0x03}}, 
	{0x85 ,1,{0x05}},	
																																																				 
	//Source EQ 																						 
	{0x80 ,1,{0x00}}, 
	{0x83 ,1,{0x00}}, 
																										   
	//FP  BP//																								
	{0x93 ,1,{0x08 }},
	{0x94 ,1,{0x0A}},
																											
	//Inversion T4pe																						  
	{0x8A ,1,{0x33}},
	{0xA4 ,1,{0x0F}},
	{0x9B ,1,{0x0F}},
																										   
	//Scan Type 	   
	//////////5.46&5.2//////////////																					 
	{0x9D,1,{ 0xB0}},
	//{0x9D ,1,{0xB6//B
	
	////////////////5.0//////////////
	//{0x9D ,1,{0xB2//F
	//{0x9D ,1,{0xB4//B
																										  
	//VIDEO DROP
	{0xE7 ,1,{0x80}},
	
	{0xA9 ,1,{0x00}},  
	
	//{0xE3 ,1,{0x02}},
	
	//page selection cmd start
	{0xFF ,1,{0x01 }},
	{0xFB ,1,{0x01}},
	
	//page selection cmd end
	//R(+) MCR cmd
{0x75,1,{0x00}},
{0x76,1,{0x00}},
{0x77,1,{0x00}},
{0x78,1,{0x0D}},
{0x79,1,{0x00}},
{0x7A,1,{0x23}},
{0x7B,1,{0x00}},
{0x7C,1,{0x36}},
{0x7D,1,{0x00}},
{0x7E,1,{0x49}},
{0x7F,1,{0x00}},
{0x80,1,{0x5A}},
{0x81,1,{0x00}},
{0x82,1,{0x69}},
{0x83,1,{0x00}},
{0x84,1,{0x77}},
{0x85,1,{0x00}},
{0x86,1,{0x85}},
{0x87,1,{0x00}},
{0x88,1,{0xB3}},
{0x89,1,{0x00}},
{0x8A,1,{0xD9}},
{0x8B,1,{0x01}},
{0x8C,1,{0x15}},
{0x8D,1,{0x01}},
{0x8E,1,{0x44}},
{0x8F,1,{0x01}},
{0x90,1,{0x8E}},
{0x91,1,{0x01}},
{0x92,1,{0xCB}},
{0x93,1,{0x01}},
{0x94,1,{0xCD}},
{0x95,1,{0x02}},
{0x96,1,{0x08}},
{0x97,1,{0x02}},
{0x98,1,{0x4F}},
{0x99,1,{0x02}},
{0x9A,1,{0x7E}},
{0x9B,1,{0x02}},
{0x9C,1,{0xBD}},
{0x9D,1,{0x02}},
{0x9E,1,{0xE6}},
{0x9F,1,{0x03}},
{0xA0,1,{0x1A}},
{0xA2,1,{0x03}},
{0xA3,1,{0x2A}},
{0xA4,1,{0x03}},
{0xA5,1,{0x3B}},
{0xA6,1,{0x03}},
{0xA7,1,{0x4E}},
{0xA9,1,{0x03}},
{0xAA,1,{0x61}},
{0xAB,1,{0x03}},
{0xAC,1,{0x7A}},
{0xAD,1,{0x03}},
{0xAE,1,{0x98}},
{0xAF,1,{0x03}},
{0xB0,1,{0xF2}},
{0xB1,1,{0x03}},
{0xB2,1,{0xFF}},
//R(-) MCR cmd
{0xB3,1,{0x00}},
{0xB4,1,{0x00}},
{0xB5,1,{0x00}},
{0xB6,1,{0x0D}},
{0xB7,1,{0x00}},
{0xB8,1,{0x23}},
{0xB9,1,{0x00}},
{0xBA,1,{0x36}},
{0xBB,1,{0x00}},
{0xBC,1,{0x49}},
{0xBD,1,{0x00}},
{0xBE,1,{0x5A}},
{0xBF,1,{0x00}},
{0xC0,1,{0x69}},
{0xC1,1,{0x00}},
{0xC2,1,{0x77}},
{0xC3,1,{0x00}},
{0xC4,1,{0x85}},
{0xC5,1,{0x00}},
{0xC6,1,{0xB3}},
{0xC7,1,{0x00}},
{0xC8,1,{0xD9}},
{0xC9,1,{0x01}},
{0xCA,1,{0x15}},
{0xCB,1,{0x01}},
{0xCC,1,{0x44}},
{0xCD,1,{0x01}},
{0xCE,1,{0x8E}},
{0xCF,1,{0x01}},
{0xD0,1,{0xCB}},
{0xD1,1,{0x01}},
{0xD2,1,{0xCD}},
{0xD3,1,{0x02}},
{0xD4,1,{0x08}},
{0xD5,1,{0x02}},
{0xD6,1,{0x4F}},
{0xD7,1,{0x02}},
{0xD8,1,{0x7E}},
{0xD9,1,{0x02}},
{0xDA,1,{0xBD}},
{0xDB,1,{0x02}},
{0xDC,1,{0xE6}},
{0xDD,1,{0x03}},
{0xDE,1,{0x1A}},
{0xDF,1,{0x03}},
{0xE0,1,{0x2A}},
{0xE1,1,{0x03}},
{0xE2,1,{0x3B}},
{0xE3,1,{0x03}},
{0xE4,1,{0x4E}},
{0xE5,1,{0x03}},
{0xE6,1,{0x61}},
{0xE7,1,{0x03}},
{0xE8,1,{0x7A}},
{0xE9,1,{0x03}},
{0xEA,1,{0x98}},
{0xEB,1,{0x03}},
{0xEC,1,{0xF2}},
{0xED,1,{0x03}},
{0xEE,1,{0xFF}},
//G(+) MCR cmd
{0xEF,1,{0x00}},
{0xF0,1,{0x00}},
{0xF1,1,{0x00}},
{0xF2,1,{0x0D}},
{0xF3,1,{0x00}},
{0xF4,1,{0x23}},
{0xF5,1,{0x00}},
{0xF6,1,{0x36}},
{0xF7,1,{0x00}},
{0xF8,1,{0x49}},
{0xF9,1,{0x00}},
{0xFA,1,{0x5A}},
//page selection cmd start
{0xFF,1,{ 0x02}},
{0xFB,1,{ 0x01}},
//page selection cmd end
{0x00,1,{0x00}},
{0x01,1,{0x69}},
{0x02,1,{0x00}},
{0x03,1,{0x77}},
{0x04,1,{0x00}},
{0x05,1,{0x85}},
{0x06,1,{0x00}},
{0x07,1,{0xB3}},
{0x08,1,{0x00}},
{0x09,1,{0xD9}},
{0x0A,1,{0x01}},
{0x0B,1,{0x15}},
{0x0C,1,{0x01}},
{0x0D,1,{0x44}},
{0x0E,1,{0x01}},
{0x0F,1,{0x8E}},
{0x10,1,{0x01}},
{0x11,1,{0xCB}},
{0x12,1,{0x01}},
{0x13,1,{0xCD}},
{0x14,1,{0x02}},
{0x15,1,{0x08}},
{0x16,1,{0x02}},
{0x17,1,{0x4F}},
{0x18,1,{0x02}},
{0x19,1,{0x7E}},
{0x1A,1,{0x02}},
{0x1B,1,{0xBD}},
{0x1C,1,{0x02}},
{0x1D,1,{0xE6}},
{0x1E,1,{0x03}},
{0x1F,1,{0x1A}},
{0x20,1,{0x03}},
{0x21,1,{0x2A}},
{0x22,1,{0x03}},
{0x23,1,{0x3B}},
{0x24,1,{0x03}},
{0x25,1,{0x4E}},
{0x26,1,{0x03}},
{0x27,1,{0x61}},
{0x28,1,{0x03}},
{0x29,1,{0x7A}},
{0x2A,1,{0x03}},
{0x2B,1,{0x98}},
{0x2D,1,{0x03}},
{0x2F,1,{0xF2}},
{0x30,1,{0x03}},
{0x31,1,{0xFF}},
//G(-) MCR cmd
{0x32,1,{0x00}},
{0x33,1,{0x00}},
{0x34,1,{0x00}},
{0x35,1,{0x0D}},
{0x36,1,{0x00}},
{0x37,1,{0x23}},
{0x38,1,{0x00}},
{0x39,1,{0x36}},
{0x3A,1,{0x00}},
{0x3B,1,{0x49}},
{0x3D,1,{0x00}},
{0x3F,1,{0x5A}},
{0x40,1,{0x00}},
{0x41,1,{0x69}},
{0x42,1,{0x00}},
{0x43,1,{0x77}},
{0x44,1,{0x00}},
{0x45,1,{0x85}},
{0x46,1,{0x00}},
{0x47,1,{0xB3}},
{0x48,1,{0x00}},
{0x49,1,{0xD9}},
{0x4A,1,{0x01}},
{0x4B,1,{0x15}},
{0x4C,1,{0x01}},
{0x4D,1,{0x44}},
{0x4E,1,{0x01}},
{0x4F,1,{0x8E}},
{0x50,1,{0x01}},
{0x51,1,{0xCB}},
{0x52,1,{0x01}},
{0x53,1,{0xCD}},
{0x54,1,{0x02}},
{0x55,1,{0x08}},
{0x56,1,{0x02}},
{0x58,1,{0x4F}},
{0x59,1,{0x02}},
{0x5A,1,{0x7E}},
{0x5B,1,{0x02}},
{0x5C,1,{0xBD}},
{0x5D,1,{0x02}},
{0x5E,1,{0xE6}},
{0x5F,1,{0x03}},
{0x60,1,{0x1A}},
{0x61,1,{0x03}},
{0x62,1,{0x2A}},
{0x63,1,{0x03}},
{0x64,1,{0x3B}},
{0x65,1,{0x03}},
{0x66,1,{0x4E}},
{0x67,1,{0x03}},
{0x68,1,{0x61}},
{0x69,1,{0x03}},
{0x6A,1,{0x7A}},
{0x6B,1,{0x03}},
{0x6C,1,{0x98}},
{0x6D,1,{0x03}},
{0x6E,1,{0xF2}},
{0x6F,1,{0x03}},
{0x70,1,{0xFF}},
//B(+) MCR cmd
{0x71,1,{0x00}},
{0x72,1,{0x00}},
{0x73,1,{0x00}},
{0x74,1,{0x0D}},
{0x75,1,{0x00}},
{0x76,1,{0x23}},
{0x77,1,{0x00}},
{0x78,1,{0x36}},
{0x79,1,{0x00}},
{0x7A,1,{0x49}},
{0x7B,1,{0x00}},
{0x7C,1,{0x5A}},
{0x7D,1,{0x00}},
{0x7E,1,{0x69}},
{0x7F,1,{0x00}},
{0x80,1,{0x77}},
{0x81,1,{0x00}},
{0x82,1,{0x85}},
{0x83,1,{0x00}},
{0x84,1,{0xB3}},
{0x85,1,{0x00}},
{0x86,1,{0xD9}},
{0x87,1,{0x01}},
{0x88,1,{0x15}},
{0x89,1,{0x01}},
{0x8A,1,{0x44}},
{0x8B,1,{0x01}},
{0x8C,1,{0x8E}},
{0x8D,1,{0x01}},
{0x8E,1,{0xCB}},
{0x8F,1,{0x01}},
{0x90,1,{0xCD}},
{0x91,1,{0x02}},
{0x92,1,{0x08}},
{0x93,1,{0x02}},
{0x94,1,{0x4F}},
{0x95,1,{0x02}},
{0x96,1,{0x7E}},
{0x97,1,{0x02}},
{0x98,1,{0xBD}},
{0x99,1,{0x02}},
{0x9A,1,{0xE6}},
{0x9B,1,{0x03}},
{0x9C,1,{0x1A}},
{0x9D,1,{0x03}},
{0x9E,1,{0x2A}},
{0x9F,1,{0x03}},
{0xA0,1,{0x3B}},
{0xA2,1,{0x03}},
{0xA3,1,{0x4E}},
{0xA4,1,{0x03}},
{0xA5,1,{0x61}},
{0xA6,1,{0x03}},
{0xA7,1,{0x7A}},
{0xA9,1,{0x03}},
{0xAA,1,{0x98}},
{0xAB,1,{0x03}},
{0xAC,1,{0xF2}},
{0xAD,1,{0x03}},
{0xAE,1,{0xFF}},
//B(-) MCR cmd
{0xAF,1,{0x00}},
{0xB0,1,{0x00}},
{0xB1,1,{0x00}},
{0xB2,1,{0x0D}},
{0xB3,1,{0x00}},
{0xB4,1,{0x23}},
{0xB5,1,{0x00}},
{0xB6,1,{0x36}},
{0xB7,1,{0x00}},
{0xB8,1,{0x49}},
{0xB9,1,{0x00}},
{0xBA,1,{0x5A}},
{0xBB,1,{0x00}},
{0xBC,1,{0x69}},
{0xBD,1,{0x00}},
{0xBE,1,{0x77}},
{0xBF,1,{0x00}},
{0xC0,1,{0x85}},
{0xC1,1,{0x00}},
{0xC2,1,{0xB3}},
{0xC3,1,{0x00}},
{0xC4,1,{0xD9}},
{0xC5,1,{0x01}},
{0xC6,1,{0x15}},
{0xC7,1,{0x01}},
{0xC8,1,{0x44}},
{0xC9,1,{0x01}},
{0xCA,1,{0x8E}},
{0xCB,1,{0x01}},
{0xCC,1,{0xCB}},
{0xCD,1,{0x01}},
{0xCE,1,{0xCD}},
{0xCF,1,{0x02}},
{0xD0,1,{0x08}},
{0xD1,1,{0x02}},
{0xD2,1,{0x4F}},
{0xD3,1,{0x02}},
{0xD4,1,{0x7E}},
{0xD5,1,{0x02}},
{0xD6,1,{0xBD}},
{0xD7,1,{0x02}},
{0xD8,1,{0xE6}},
{0xD9,1,{0x03}},
{0xDA,1,{0x1A}},
{0xDB,1,{0x03}},
{0xDC,1,{0x2A}},
{0xDD,1,{0x03}},
{0xDE,1,{0x3B}},
{0xDF,1,{0x03}},
{0xE0,1,{0x4E}},
{0xE1,1,{0x03}},
{0xE2,1,{0x61}},
{0xE3,1,{0x03}},
{0xE4,1,{0x7A}},
{0xE5,1,{0x03}},
{0xE6,1,{0x98}},
{0xE7,1,{0x03}},
{0xE8,1,{0xF2}},
{0xE9,1,{0x03}},
{0xEA,1,{0xFF}},

                                                            
//CMD1                  
{0xFF,1,{0x00}},
{0xFB,1,{0x01}},
{0xD2,1,{0x03}},
{0xD3,1,{0x0A}},
{0xD4,1,{0x0A}},
{0x35,1,{0x00}},
	
	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} }, 															  

	/* {0x51,1,{0xFF}},     //      write   display brightness */
};

#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A, 4, {0x00, 0x00, (FRAME_WIDTH >> 8), (FRAME_WIDTH & 0xFF)} },
	{0x2B, 4, {0x00, 0x00, (FRAME_HEIGHT >> 8), (FRAME_HEIGHT & 0xFF)} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
	/* Sleep Out */
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },

	/* Display ON */
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	/* Display off sequence */
	{0x28, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },

	/* Sleep Mode On */
	{0x10, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
#endif
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 8;
	params->dsi.vertical_frontporch = 10;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 20;
	params->dsi.horizontal_frontporch = 40;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable                                                      = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 500;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 450;	/* this value must be in MTK suggested table */
#endif
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif

	params->dsi.clk_lp_per_line_enable = 1;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x53;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;

}



static void lcm_init_power(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef BUILD_LK
	mt6325_upmu_set_rg_vgp1_en(1);
#else
	LCM_LOGI("%s, begin\n", __func__);
	hwPowerOn(MT6325_POWER_LDO_VGP1, VOL_DEFAULT, "LCM_DRV");
	LCM_LOGI("%s, end\n", __func__);
#endif
#endif
}

static void lcm_suspend_power(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef BUILD_LK
	mt6325_upmu_set_rg_vgp1_en(0);
#else
	LCM_LOGI("%s, begin\n", __func__);
	hwPowerDown(MT6325_POWER_LDO_VGP1, "LCM_DRV");
	LCM_LOGI("%s, end\n", __func__);
#endif
#endif
}

static void lcm_resume_power(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef BUILD_LK
	mt6325_upmu_set_rg_vgp1_en(1);
#else
	LCM_LOGI("%s, begin\n", __func__);
	hwPowerOn(MT6325_POWER_LDO_VGP1, VOL_DEFAULT, "LCM_DRV");
	LCM_LOGI("%s, end\n", __func__);
#endif
#endif
}

static void lcm_init(void)
{
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret = 0;
	cmd = 0x00;
	data = 0x0E;
#ifndef CONFIG_FPGA_EARLY_PORTING
	mt_set_gpio_mode(GPIO_65132_ENP_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_ENP_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_ENP_EN, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_65132_ENN_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_65132_ENN_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_65132_ENN_EN, GPIO_OUT_ONE);
#ifdef BUILD_LK
	//ret = TPS65132_write_byte(cmd, data);
#else
	//ret = tps65132_write_bytes(cmd, data);
#endif

	if (ret < 0)
		LCM_LOGI("nt35595----tps6132----cmd=%0x--i2c write error----\n", cmd);
	else
		LCM_LOGI("nt35595----tps6132----cmd=%0x--i2c write success----\n", cmd);

	cmd = 0x01;
	data = 0x0E;

#ifdef BUILD_LK
	//ret = TPS65132_write_byte(cmd, data);
#else
	//ret = tps65132_write_bytes(cmd, data);
#endif

	if (ret < 0)
		LCM_LOGI("nt35595----tps6132----cmd=%0x--i2c write error----\n", cmd);
	else
		LCM_LOGI("nt35595----tps6132----cmd=%0x--i2c write success----\n", cmd);

#endif
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);
		SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(50);

	push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
	{
		push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
#ifndef CONFIG_FPGA_EARLY_PORTING
		mt_set_gpio_mode(GPIO_65132_ENN_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_65132_ENN_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_65132_ENN_EN, GPIO_OUT_ZERO);
		MDELAY(5);
		mt_set_gpio_mode(GPIO_65132_ENP_EN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_65132_ENP_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_65132_ENP_EN, GPIO_OUT_ZERO);
#endif
	
		//SET_RESET_PIN(1);
		MDELAY(5);
		SET_RESET_PIN(0); 
		//MDELAY(10);
		//SET_RESET_PIN(1);
		//MDELAY(20);
	
		/* SET_RESET_PIN(0); */
		//MDELAY(10);
	}


static void lcm_resume(void)
{
	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

#define LCM_ID_NT35596 (0x96)
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);


static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[2];
	unsigned int array[16];
    int data[4] = {0,0,0,0};
    int res = 0;
    int rawdata = 0;
    int lcm_vol = 0;

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);
	res = IMM_GetOneChannelValue(13,data,&rawdata);
	lcm_vol = data[0]*1000+data[1]*10;
		
	  
	LCM_LOGI("[adc_uboot]: lcm_vol= %d\n",lcm_vol);

	array[0] = 0x00023700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xF4, buffer, 2);
	id = buffer[0];		/* we only need ID */

	LCM_LOGI("%s,nt35596 debug: nt35596 id = 0x%08x\n", __func__, id);

		if (lcm_vol>=600 &&lcm_vol <= 800 &&id == LCM_ID_NT35596)
		return 1;
	else
		return 0;


}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x53, buffer, 1);

	if (buffer[0] != 0x24) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	} else {
		LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
		return FALSE;
	}
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];
	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700;	/* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,nt35595 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_setbacklight(unsigned int level)
{
	LCM_LOGI("%s,nt35595 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
/* customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;	/* mode control addr */
		lcm_switch_mode_cmd.val[0] = 0x13;	/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[1] = 0x10;	/* disable video mode secondly */
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0] = 0x03;	/* disable GRAM and enable video mode */
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}


LCM_DRIVER nt35596_fhd_dsi_vdo_dj_boe_al1518_lcm_drv = {
	.name = "nt35596_fhd_dsi_vdo_dj_boe_al1518",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	//.init_power = lcm_init_power,
	//.resume_power = lcm_resume_power,
	//.suspend_power = lcm_suspend_power,
	//.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.switch_mode = lcm_switch_mode,
};