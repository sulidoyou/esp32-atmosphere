#include"bsp.h"



#define KEY1_GPIO GPIO_NUM_42
#define KEY2_GPIO GPIO_NUM_41
#define KEY3_GPIO GPIO_NUM_40

static KEY_T s_tBtn[KEY_COUNT];
static KEY_FIFO_T s_tKey;       /* 按键FIFO变量,结构体 */

uint16_t KEY_Buzzer_Time = 0;//按键蜂鸣器






 uint8_t IsKeyDown1(void) {if ((gpio_get_level(KEY1_GPIO) == 0)) return 1;else return 0;}
 uint8_t IsKeyDown2(void) {if ((gpio_get_level(KEY2_GPIO) == 0)) return 1;else return 0;}
 uint8_t IsKeyDown3(void) {if ((gpio_get_level(KEY3_GPIO) == 0)) return 1;else return 0;}
 

 
 
/*
*********************************************************************************************************
*   函 数 名: bsp_InitKeyVar
*   功能说明: 初始化按键变量
*   形    参:  无
*   返 回 值: 无
*********************************************************************************************************
*/
static void bsp_InitKeyVar(void)
{
    uint8_t i;

    /* 对按键FIFO读写指针清零 */
    s_tKey.Read = 0;
    s_tKey.Write = 0;
    s_tKey.Read2 = 0;

    /* 给每个按键结构体成员变量赋一组缺省值 */
    for (i = 0; i < KEY_COUNT; i++)
    {
        s_tBtn[i].LongTime = KEY_LONG_TIME;         /* 长按时间 0 表示不检测长按键事件 */
//        s_tBtn[i].LongTime = 0;                      /* 长按时间 0 表示不检测长按键事件 */
        s_tBtn[i].Count = KEY_FILTER_TIME / 2;      /* 计数器设置为滤波时间的一半 */
        s_tBtn[i].State = 0;                            /* 按键缺省状态，0为未按下 */


        s_tBtn[i].RepeatSpeed = 0;                      /* 按键连发的速度，0表示不支持连发 */


        s_tBtn[i].RepeatCount = 0;                      /* 连发计数器 */
    }
    s_tBtn[0].IsKeyDownFunc = IsKeyDown1;
	s_tBtn[1].IsKeyDownFunc = IsKeyDown2;
    s_tBtn[2].IsKeyDownFunc = IsKeyDown3;
}

/*
*********************************************************************************************************
*   函 数 名: bsp_InitKeyHard
*   功能说明: 配置按键对应的GPIO
*   形    参:  无
*   返 回 值: 无
*********************************************************************************************************
*/
static void bsp_InitKeyHard(void)
{

}



/*
*********************************************************************************************************
*   函 数 名: bsp_InitKey
*   功能说明: 初始化按键. 该函数被 bsp_Init() 调用。
*   形    参:  无
*   返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitKey(void)
{
    bsp_InitKeyVar();       /* 初始化按键变量 */
    bsp_InitKeyHard();      /* 初始化按键硬件 */
}


/*
*********************************************************************************************************
*   函 数 名: bsp_PutKey
*   功能说明: 将1个键值压入按键FIFO缓冲区。可用于模拟一个按键。
*   形    参:  _KeyCode : 按键代码
*   返 回 值: 无
*********************************************************************************************************
*/
void bsp_PutKey(uint8_t _KeyCode)
{
    s_tKey.Buf[s_tKey.Write] = _KeyCode;

    if (++s_tKey.Write  >= KEY_FIFO_SIZE)
    {
        s_tKey.Write = 0;
    }
}

/*
*********************************************************************************************************
*   函 数 名: bsp_GetKey
*   功能说明: 从按键FIFO缓冲区读取一个键值。
*   形    参:  无
*   返 回 值: 按键代码
*********************************************************************************************************
*/
uint8_t bsp_GetKey(void)
{
    uint8_t ret;

    if (s_tKey.Read == s_tKey.Write)
    {
        return KEY_NONE;
    }
    else
    {
        ret = s_tKey.Buf[s_tKey.Read];

        if (++s_tKey.Read >= KEY_FIFO_SIZE)
        {
            s_tKey.Read = 0;
        }
        return ret;
    }
}

uint8_t bsp_GetKey_Num(void)
{
    if (s_tKey.Read == s_tKey.Write)
    {
        return 1;
    }
    return 0;

}
/*
*********************************************************************************************************
*   函 数 名: bsp_GetKey2
*   功能说明: 从按键FIFO缓冲区读取一个键值。独立的读指针。
*   形    参:  无
*   返 回 值: 按键代码
*********************************************************************************************************
*/
uint8_t bsp_GetKey2(void)
{
    uint8_t ret;

    if (s_tKey.Read2 == s_tKey.Write)
    {
        return KEY_NONE;
    }
    else
    {
        ret = s_tKey.Buf[s_tKey.Read2];

        if (++s_tKey.Read2 >= KEY_FIFO_SIZE)
        {
            s_tKey.Read2 = 0;
        }
        return ret;
    }
}

/*
*********************************************************************************************************
*   函 数 名: bsp_GetKeyState
*   功能说明: 读取按键的状态
*   形    参:  _ucKeyID : 按键ID，从0开始
*   返 回 值: 1 表示按下， 0 表示未按下
*********************************************************************************************************
*/
uint8_t bsp_GetKeyState(KEY_ID_E _ucKeyID)
{
    return s_tBtn[_ucKeyID].State;
}
/*
*********************************************************************************************************
*   函 数 名: bsp_SetKeyParam
*   功能说明: 设置按键参数
*   形    参：_ucKeyID : 按键ID，从0开始
*           _LongTime : 长按事件时间
*            _RepeatSpeed : 连发速度
*   返 回 值: 无
*********************************************************************************************************
*/
void bsp_SetKeyParam(uint8_t _ucKeyID, uint16_t _LongTime, uint8_t  _RepeatSpeed)
{
    s_tBtn[_ucKeyID].LongTime = _LongTime;          /* 长按时间 0 表示不检测长按键事件 */
    s_tBtn[_ucKeyID].RepeatSpeed = _RepeatSpeed;            /* 按键连发的速度，0表示不支持连发 */
    s_tBtn[_ucKeyID].RepeatCount = 0;                       /* 连发计数器 */
}

/*
*********************************************************************************************************
*   函 数 名: bsp_ClearKey
*   功能说明: 清空按键FIFO缓冲区
*   形    参：无
*   返 回 值: 按键代码
*********************************************************************************************************
*/
void bsp_ClearKey(void)
{
    s_tKey.Read = s_tKey.Write;
}


/*
*********************************************************************************************************
*   函 数 名: bsp_DetectKey
*   功能说明: 检测一个按键。非阻塞状态，必须被周期性的调用。
*   形    参:  按键结构变量指针
*   返 回 值: 无
*********************************************************************************************************
*/
static void bsp_DetectKey(uint8_t i)
{
    KEY_T *pBtn;

    /*
        如果没有初始化按键函数，则报错
        if (s_tBtn[i].IsKeyDownFunc == 0)
        {

        }
    */

    pBtn = &s_tBtn[i];
    if (pBtn->IsKeyDownFunc())
    {
        if (pBtn->Count < KEY_FILTER_TIME)
        {
            pBtn->Count = KEY_FILTER_TIME;
        }
        else if(pBtn->Count < 2 * KEY_FILTER_TIME)
        {
            pBtn->Count++;
        }
        else
        {
            if (pBtn->State == 0)
            {
                pBtn->State = 1;

                /* 发送按钮按下的消息 */
               // bsp_PutKey((uint8_t)(3 * i + 1));
            }

            if (pBtn->LongTime > 0)
            {
                if (pBtn->LongCount < pBtn->LongTime)
                {
                    /* 发送按钮持续按下的消息 */
                    if (++pBtn->LongCount == pBtn->LongTime)
                    {
                        /* 键值放入按键FIFO */
                        bsp_PutKey((uint8_t)(3 * i + 3));
                    }
                }
                else
                {
                    if (pBtn->RepeatSpeed > 0)
                    {
                        if (++pBtn->RepeatCount >= pBtn->RepeatSpeed)
                        {
                            pBtn->RepeatCount = 0;
                            /* 常按键后，每隔10ms发送1个按键 */
                            bsp_PutKey((uint8_t)(3 * i + 1));
                        }
                    }
                }
            }
        }
    }
    else
    {
        if ((pBtn->LongCount < pBtn->LongTime)&&(pBtn->LongCount > KEY_FILTER_TIME))
        {
            bsp_PutKey((uint8_t)(3 * i + 1));
        }
        
        if(pBtn->Count > KEY_FILTER_TIME)
        {
            pBtn->Count = KEY_FILTER_TIME;
        }
        else if(pBtn->Count != 0)
        {
            pBtn->Count--;
        }
        else
        {
            if (pBtn->State == 1)
            {
                pBtn->State = 0;

                /* 发送按钮弹起的消息 */
                bsp_PutKey((uint8_t)(3 * i + 2));
            }
        }

        pBtn->LongCount = 0;
        pBtn->RepeatCount = 0;
    }
}
/*
*********************************************************************************************************
*   函 数 名: bsp_KeyScan
*   功能说明: 扫描所有按键。非阻塞，被systick中断周期性的调用
*   形    参:  无
*   返 回 值: 无
*********************************************************************************************************
*/
void bsp_KeyScan(void)
{
    uint8_t i;

    for (i = 0; i < KEY_COUNT; i++)
    {
        bsp_DetectKey(i);
    }
}





























static void KeyHandle_task(void* arg)
{

    while(1)
    {
 
        bsp_KeyScan();
        vTaskDelay(10 / portTICK_PERIOD_MS);
        KeyHandle();
    }
}


void bsp_key_init(void)
{

        gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << KEY1_GPIO)|(1ULL << KEY2_GPIO)|(1ULL << KEY3_GPIO),   // 选中目标引脚
        .mode = GPIO_MODE_INPUT,                 // 输入模式 ← 修正注释
        .pull_up_en = GPIO_PULLUP_ENABLE,        // 启用上拉 ← 修正注释
        .pull_down_en = GPIO_PULLDOWN_DISABLE,   // 禁用下拉
        .intr_type = GPIO_INTR_DISABLE           // 禁用中断
    };
    gpio_config(&io_conf);
    bsp_InitKey();
    xTaskCreate(KeyHandle_task, "KeyHandle_task", 2048, NULL, 15, NULL);

}




























