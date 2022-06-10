/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2013 Realtek Semiconductor Corp.
 *
 *  This module is a confidential and proprietary property of RealTek and
 *  possession or use of this module requires written permission of RealTek.
 */


#include "rtl8195a.h"
#include "hal_peri_on.h"

#ifdef CONFIG_PWM_EN
#include "rtl8195a_pwm.h"
#include "hal_pwm.h"

extern HAL_PWM_ADAPTER PWMPin[];

extern HAL_TIMER_OP HalTimerOp;
const u8 PWMTimerIdx[MAX_PWM_CTRL_PIN]= {2,3,4,5};  // the G-timer ID used for PWM pin 0~3
HAL_PWM_TIMER PWMTimer[MAX_PWM_CTRL_PIN];


/**
  * @brief  Configure a G-Timer to generate a tick with certain time.
  *
  * @param  pwm_id: the PWM pin index
  * @param  tick_time: the time (micro-second) of a tick
  *
  * @retval None
  */
void 
Pwm_SetTimerTick_8195a(
    HAL_PWM_ADAPTER *pPwmAdapt,
    u32 tick_time
)
{
    TIMER_ADAPTER TimerAdapter;
    u32 i,j;
    u8 count=0;

    //HalTimerDeInit(&TimerAdapter);

    if (tick_time <= MIN_GTIMER_TIMEOUT) {
        tick_time = MIN_GTIMER_TIMEOUT;
    }
    else {
        tick_time = (((tick_time-1)/MIN_GTIMER_TIMEOUT)+1) * MIN_GTIMER_TIMEOUT;
    }

    if (pPwmAdapt->gtimer_id == 0xFF) // initial to a invalid value means no g-timer ID
    {
        // Initial a G-Timer for the PWM pin
        if (pPwmAdapt->tick_time != tick_time) {
            // de-reference to old timer
            if (pPwmAdapt->gtimer_id != 0xFF) {
                for (i=0;i<MAX_GTIMER_NUM;i++) {
                    if(PWMTimerIdx[i] == pPwmAdapt->gtimer_id) {
                        PWMTimer[i].reference &= ~(1<<pPwmAdapt->pwm_id);
                        pPwmAdapt->gtimer_id = 0xFF;
                        pPwmAdapt->tick_time = 0;
                        break;
                    }
                }
            }
            //DBG_PWM_ERR("Init: %d, %d\n", pPwmAdapt->tick_time, tick_time);
            for (i=0;i<MAX_GTIMER_NUM;i++) {
                if(PWMTimer[i].tick_time == tick_time) { // allocate same GTimer
                    pPwmAdapt->gtimer_id = PWMTimerIdx[i];
                    pPwmAdapt->tick_time = tick_time;
                    PWMTimer[i].reference |= 1<<pPwmAdapt->pwm_id;
                    //DBG_PWM_ERR("PWMTimer[%d].tick_time %d\n", i,PWMTimer[i].tick_time);
                    return;
                }
            }
            //DBG_PWM_ERR("Init: %d, %d\n", pPwmAdapt->tick_time, tick_time);       
            // allocate a new timer for this PWM
            for (i=0;i<MAX_GTIMER_NUM;i++) {
                if(PWMTimer[i].reference == 0) { // allocate New GTimer
                    pPwmAdapt->gtimer_id = PWMTimerIdx[i];
                    pPwmAdapt->tick_time = tick_time;
                    PWMTimer[i].tick_time = tick_time;
                    PWMTimer[i].reference |= 1<<pPwmAdapt->pwm_id;

                    TimerAdapter.IrqDis = 1;    // Disable Irq
                    TimerAdapter.IrqHandle.IrqFun = (IRQ_FUN) NULL;
                    TimerAdapter.IrqHandle.IrqNum = TIMER2_7_IRQ;
                    TimerAdapter.IrqHandle.Priority = 10;
                    TimerAdapter.IrqHandle.Data = (u32)NULL;
                    TimerAdapter.TimerId = pPwmAdapt->gtimer_id;
                    TimerAdapter.TimerIrqPriority = 0;
                    TimerAdapter.TimerLoadValueUs = tick_time-1;
                    TimerAdapter.TimerMode = 1; // auto-reload with user defined value
                    HalTimerDeInit(&TimerAdapter);
                    HalTimerOp.HalTimerInit((VOID*) &TimerAdapter);
                    //DBG_PWM_ERR(" 3.PWM_ID=%d, PWMTimer[%d].ref %d\n",pPwmAdapt->pwm_id, i,PWMTimer[i].reference);
                    //DBG_PWM_ERR("Init: %d, %d\n", pPwmAdapt->tick_time, tick_time);
//                pPwmAdapt->tick_time = tick_time;
                    //DBG_PWM_ERR("1.Pwm_ID=%d: Timer_Id=%d Count=%d\n", pPwmAdapt->pwm_id,pPwmAdapt->gtimer_id, tick_time);
                    break;
                }
            }

            if (i == MAX_GTIMER_NUM) {
                DBG_PWM_ERR("Pwm_SetTimerTick_8195a: Failed to allocate G-Timer!\n");            
            }
        }
    }
    else // Change tick_time
    {
        if (pPwmAdapt->tick_time != tick_time)
        {
            for (i=0;i<MAX_PWM_CTRL_PIN;i++) 
            {
                for(j=0;j<MAX_PWM_CTRL_PIN;j++)
                {
                    if(( (1<<pPwmAdapt->pwm_id) & PWMTimer[i].reference)) // Include PWM_ID
                    {
                        if((PWMTimer[i].reference & (1<<j)) != 0) // Detect How many PWM Use same TimerID
                        {   
                            count++;
                        }
                    }

                }
                if(count > 1) // When Over 1 PWM Module use same Timer, then can't change tick_time
                {
                    DBG_PWM_ERR(" Over 1 PWM Module use same Timer %d, then can't change tick_time, count=%d\n", pPwmAdapt->gtimer_id,count);
                    //DBG_PWM_ERR(" 3.PWM_ID=%d, PWMTimer[%d].ref %d\n",pPwmAdapt->pwm_id, i,PWMTimer[i].reference);
                    count=0;
                    return;
                }
                count=0;    
            }   
        }

        pPwmAdapt->tick_time = tick_time;
        TimerAdapter.TimerLoadValueUs = tick_time-1;
        TimerAdapter.TimerId = pPwmAdapt->gtimer_id;
        HalTimerReLoad(TimerAdapter.TimerId ,TimerAdapter.TimerLoadValueUs);
        //DBG_PWM_ERR("2.Pwm_ID=%d: Timer_Id=%d Count=%d\n", pPwmAdapt->pwm_id,pPwmAdapt->gtimer_id, tick_time);
    }

}


/**
  * @brief  Set the duty ratio of the PWM pin.
  *
  * @param  pwm_id: the PWM pin index
  * @param  period: the period time, in micro-second.
  * @param  pulse_width: the pulse width time, in micro-second.
  *
  * @retval None
  */
void
HAL_Pwm_SetDuty_8195a(
    HAL_PWM_ADAPTER *pPwmAdapt,
    u32 period,
    u32 pulse_width
)
{
    u32 RegAddr;
    u32 RegValue;
    u32 period_tick;
    u32 pulsewidth_tick;
    u32 tick_time;
    u8 timer_id;
    u8 pwm_id;

    pwm_id = pPwmAdapt->pwm_id;
    // Adjust the tick time to a proper value
    if (period < (MIN_GTIMER_TIMEOUT*2)) {
        DBG_PWM_ERR ("HAL_Pwm_SetDuty_8195a: Invalid PWM period(%d), too short!!\n", period);
        tick_time = MIN_GTIMER_TIMEOUT;
        period = MIN_GTIMER_TIMEOUT*2;
    }
    else {
        tick_time = period / MAX_DEVID_TICK; // a duty cycle be devided into 1020 ticks
        if (tick_time < MIN_GTIMER_TIMEOUT) {
            tick_time = MIN_GTIMER_TIMEOUT;
        }
    }
    period_tick = period/tick_time;
    while(period_tick>1023) // Prevent period_tick over 1023
    {
        tick_time++;
        period_tick = period/tick_time;
    }
    Pwm_SetTimerTick_8195a(pPwmAdapt, tick_time);
    tick_time = pPwmAdapt->tick_time;
#if 0    
    // Check if current tick time needs adjustment
    if ((pPwmAdapt->tick_time << 12) <= period) {
        // need a longger tick time
    }
    else if ((pPwmAdapt->tick_time >> 2) >= period) {
        // need a shorter tick time
    }
#endif
    period_tick = period/tick_time;
    if (period_tick == 0) {
        period_tick = 1;
    }

    if (pulse_width >= period) {
//        pulse_width = period-1;
        pulse_width = period;
    }
    pulsewidth_tick = pulse_width/tick_time;
    if (pulsewidth_tick == 0) {
//        pulsewidth_tick = 1;
    }
    if(period_tick>1023) // Prevent period_tick over 1023
    {
        pulsewidth_tick = 0;
        pulse_width = 0;
        DBG_PWM_ERR (" Period_tick over 10 bits\n");
    }
    //DBG_PWM_ERR ("period_tick:(%d), tick_time:(%d),pulse_width:(%d)\n", period_tick,pPwmAdapt->tick_time,pulse_width);
    timer_id = pPwmAdapt->gtimer_id;
    pPwmAdapt->period = period_tick & 0x3ff;
    pPwmAdapt->pulsewidth = pulsewidth_tick & 0x3ff;
    //DBG_PWM_ERR ("period_tick_R:(%d), pulse_width_R:(%d)\n", pPwmAdapt->period,pPwmAdapt->pulsewidth);
    RegAddr = REG_PERI_PWM0_CTRL + (pwm_id*4);
    RegValue = BIT31 | (timer_id<<24) | (pulsewidth_tick<<12) | period_tick;
    HAL_WRITE32(PERI_ON_BASE, RegAddr, RegValue);
}

/**
  * @brief  Initializes and enable a PWM control pin.
  *
  * @param  pwm_id: the PWM pin index
  * @param  sel: pin mux selection
  * @param  timer_id: the G-timer index assigned to this PWM
  *
  * @retval HAL_Status
  */
HAL_Status 
HAL_Pwm_Init_8195a(
    HAL_PWM_ADAPTER *pPwmAdapt
)
{
    u32 pwm_id;
    u32 pin_sel;

    pwm_id = pPwmAdapt->pwm_id;
    pin_sel =  pPwmAdapt->sel;
    // Initial a G-Timer for the PWM pin
    pPwmAdapt->gtimer_id = 0xFF;    // initial to a invalid value means no g-timer ID
    //Pwm_SetTimerTick_8195a(pPwmAdapt, MIN_GTIMER_TIMEOUT);

    // Set default duty ration
    //HAL_Pwm_SetDuty_8195a(pPwmAdapt, 20000, 10000);

    // Configure the Pin Mux
    PinCtrl((PWM0+pwm_id), pin_sel, 1);

    return HAL_OK;
}


/**
  * @brief  Enable a PWM control pin.
  *
  * @param  pwm_id: the PWM pin index
  *
  * @retval None
  */
void 
HAL_Pwm_Enable_8195a(
    HAL_PWM_ADAPTER *pPwmAdapt
)
{
    u32 pwm_id;
    
    pwm_id = pPwmAdapt->pwm_id;
    // Configure the Pin Mux
    if (!pPwmAdapt->enable) {
        PinCtrl((PWM0+pwm_id), pPwmAdapt->sel, 1);
        HalTimerOp.HalTimerEn(pPwmAdapt->gtimer_id);        
        pPwmAdapt->enable = 1;
    }
}


/**
  * @brief  Disable a PWM control pin.
  *
  * @param  pwm_id: the PWM pin index
  *
  * @retval None
  */
void 
HAL_Pwm_Disable_8195a(
    HAL_PWM_ADAPTER *pPwmAdapt
)
{
    u32 pwm_id;
    
    pwm_id = pPwmAdapt->pwm_id;
    // Configure the Pin Mux
    if (pPwmAdapt->enable) {
        PinCtrl((PWM0+pwm_id), pPwmAdapt->sel, 0);
        HalTimerOp.HalTimerDis(pPwmAdapt->gtimer_id);        
        pPwmAdapt->enable = 0;
    }
}

/**
  * @brief  When Over 1 PWM Module use same Timer, then can't disable timer.
  *
  * @param  pwm_id: the PWM pin index
  *
  * @retval None
  */

void 
HAL_Pwm_Dinit_8195a(
    HAL_PWM_ADAPTER *pPwmAdapt
)
{
    u32 i,j;
    u8 count=0;
    u32 pwm_id;
    
    pwm_id = pPwmAdapt->pwm_id;
    for (i=0;i<MAX_PWM_CTRL_PIN;i++) 
    {
        for(j=0;j<MAX_PWM_CTRL_PIN;j++)
        {
            if( (1<<pwm_id) & PWMTimer[i].reference) // Include PWM_ID
            {
                if((PWMTimer[i].reference & (1<<j)) != 0) // Detect How many PWM Use same TimerID
                {   
                    count++;
                }
            }

        }
        if(count > 1) // When Over 1 PWM Module use same Timer, then can't disable timer
        {
            PWMTimer[i].reference &= ~(1<<pwm_id); // Free reference
            //DBG_PWM_ERR(" 4.PWM_ID=%d, PWMTimer[%d].ref %d, count=%d\n",pPwmAdapt->pwm_id, i,PWMTimer[i].reference,count);

            DBG_PWM_ERR(" Over 1 PWM Module use same Timer %d, then can't disable timer\n", pPwmAdapt->gtimer_id); 
            if (pPwmAdapt->enable) { // Disable PWM PIN
                PinCtrl((PWM0+pwm_id), pPwmAdapt->sel, 0);       
                pPwmAdapt->enable = 0;
            }
            pPwmAdapt->gtimer_id = 0xFF; // Initial Gtmier ID
            //DBG_PWM_ERR(" 4.PWM_ID=%d, PWMTimer[%d].ref %d\n",pPwmAdapt->pwm_id, i,PWMTimer[i].reference);
            count=0;
            return;
        }
        else if (count == 1)
        {
            PWMTimer[i].reference &= ~(1<<pwm_id); // Free reference
            pPwmAdapt->gtimer_id = 0xFF; // Initial Gtmier ID
            //DBG_PWM_ERR(" 4.PWM_ID=%d, PWMTimer[%d].ref %d, count=%d\n",pPwmAdapt->pwm_id, i,PWMTimer[i].reference,count);
        }
        count=0;
    } 
    if (NULL == pPwmAdapt) {
        DBG_PWM_ERR ("HAL_Pwm_Disable: NULL adapter\n");
        return;
    }
        
    #ifndef CONFIG_CHIP_E_CUT
        HAL_Pwm_Disable_8195a(pPwmAdapt);
    #else
        HAL_Pwm_Disable_8195a_V04(pPwmAdapt);
    #endif

}

#endif  //CONFIG_PWM_EN
