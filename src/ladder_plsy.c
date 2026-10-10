void PLSY(void)
{  
/*
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
  static  u8 addr;
  long sys;
  signed short temp,PUS_TOTAL=0;  
  if ( PLC_ACC_BIT & 0x01 )               //判断条件满足否
  { 
    temp=cos_value();               //脉冲频率
    PUS_TOTAL=cos_value();          //脉冲计数
    addr=*PLC_Addr;                 //输出地址
    PLC_Addr+=2;
    if ( D8140 >= PUS_TOTAL )
      PLC_BIT_ON(M8029);
    
    if ((addr==0x00)&&(!(PLC_BIT_TEST(M8145)))&&(Y0P==0))           //Y0
    {
      PLC_BIT_ON(0x500);           //Y00 ON 
      PLC_BIT_OFF(M8029);          //M8029
//       PLC_Ws(0X20005F1C)=0;     //D8140
      Y0P=1;
      Plus_CMP0=PUS_TOTAL; 
      sys=72000000/((71+1)*(temp*2))-1;
      //IO口初始化
      GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;      //A7 
      GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
      GPIO_InitStructure.GPIO_Mode =  GPIO_Mode_Out_PP;
      GPIO_Init(GPIOC, &GPIO_InitStructure);
      
      RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 , ENABLE);
      TIM_DeInit(TIM3);
      TIM_TimeBaseStructure.TIM_Period=sys;                      // 自动重装载寄存器周期的值(计数值) 
      // 累计 TIM_Period个频率后产生一个更新或者中断 
      TIM_TimeBaseStructure.TIM_Prescaler= (72 - 1);             // 时钟预分频数 72M/72 
      TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1;      // 采样分频 
      TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up; // 向上计数模式 
      TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
      TIM_ClearFlag(TIM3, TIM_FLAG_Update);                     // 清除溢出中断标志 
      TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE);
      TIM_Cmd(TIM3, ENABLE);                                     // 开启时钟                 
      RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 , ENABLE);     // 先关闭等待使用            
      PLC_BIT_ON(M8147);                                        // M8147 
    }  
  }
  else  
  {  
    PLC_Addr+=4;
    addr=*PLC_Addr;          //输出地址
    PLC_Addr+=2;
    if (addr==0x00)                //Y0
    {
      if (Y0P==1)
      { 
        TIM_Cmd(TIM5, DISABLE); 
        TIM_Cmd(TIM3, DISABLE);
        TIM_ClearFlag(TIM3, TIM_FLAG_Update);
        TIM_ITConfig(TIM3, TIM_IT_Update, DISABLE);
              
//         GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;  //A2
//         GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
//         GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//         GPIO_Init(GPIOC, &GPIO_InitStructure);            
        Y0P=0;
        PLC_BIT_OFF(0x500);    //Y00 OFF
        PLC_BIT_OFF(M8147);    //M8147
      }
    }
    
  }
*/
}

// static void PWM(void)   //ADD 20160619
// {
//    u16 temp1,temp2;
//    if ( PLC_ACC_BIT & 0x01 )
//    {
//       temp1=2000000/cos_value();
//       temp2=2000000/cos_value();
//       TIM3->ARR = temp2-1;
//         switch(Y_addr())
//         {
//          case 0x051c: TIM3->CCR1 = temp1-1;PLC_BIT_ON(0x00c0,28);TIM3->CCER=TIM_OutputState_Enable;   break;  //PWM Y34
//          case 0x051d: TIM3->CCR2 = temp1-1;PLC_BIT_ON(0x00c0,29);TIM_OC2PreloadConfig(TIM3,TIM_OCPreload_Enable);  break;  //PWM Y35
//          case 0x051e: TIM3->CCR3 = temp1-1;PLC_BIT_ON(0x00c0,30);TIM_OC3PreloadConfig(TIM3,TIM_OCPreload_Enable);  break;  //PWM Y36
//          case 0x051f: TIM3->CCR4 = temp1-1;PLC_BIT_ON(0x00c0,31);TIM_OC4PreloadConfig(TIM3,TIM_OCPreload_Enable);  break;  //PWM Y37 
//         }
//    }
//    else
//    {
//       PLC_Addr+=4;                       //跳过4步程序 条件不满足执行跳过程序，减小CPU开销              
//       switch(Y_addr())
//       {
//         case 0x051c:  PLC_BIT_OFF(0x00c0,28);  break;                     //PWM Y34
//         case 0x051d:  PLC_BIT_OFF(0x00c0,29);  break;                       //PWM Y35
//         case 0x051e:  PLC_BIT_OFF(0x00c0,30);  break;                       //PWM Y36
//         case 0x051f:  PLC_BIT_OFF(0x00c0,31);  break;                       //PWM Y37
//       }
//    }     
// }


// void PLSY(void)
// {  
//   TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
//   TIM_OCInitTypeDef  TIM_OCInitStructure;
//   GPIO_InitTypeDef GPIO_InitStructure;
//   long div_c;
//   signed short temp,PUS_TOTAL=0;  
//   static  u8 Y0P,Y1P,addr;;
//    if ( PLC_ACC_BIT & 0x01 )               //判断条件满足否
//    { 
//       temp=cos_value();               //脉冲频率
//       PUS_TOTAL=cos_value();            //脉冲计数
//       addr=*PLC_Addr;                 //输出地址
//       PLC_Addr+=2;
//       if ((addr==0x00)&&(PLC_BIT_TEST(0x00E0,145))&&(Y0P==0))                //Y0
//       {
//          PLC_BIT_ON(0XC0,0);                //Y00 ON
//          PLC_BIT_OFF(0x00E0,29);            //M8029
//          PLC_Ws(0X20005E90)=0;           //D8140
//          Y0P=1;
//          
//          div_c=75497472/temp;
//          div_c=div_c/65536;
//          temp=75497472/(temp*(div_c+1));
//          temp--;

//        //只初始化一次  Y0 
//          TIM_DeInit(TIM5);
//        //TIM5 clock enable   PWM MODE
//          RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE); 
//          GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
//          GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//          GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_2;
//          GPIO_Init(GPIOA, &GPIO_InitStructure);  

//          TIM_TimeBaseStructure.TIM_Period = div_c;
//          TIM_TimeBaseStructure.TIM_Prescaler = temp;
//          TIM_TimeBaseStructure.TIM_ClockDivision = 0;
//          TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Down;

//          TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure);

//          // PWM1 Mode configuration: Channel3 
//          TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
//          TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
//          TIM_OCInitStructure.TIM_Pulse = temp/2;
//          TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

//          TIM_OC3Init(TIM5, &TIM_OCInitStructure);

//          TIM_OC3PreloadConfig(TIM5, TIM_OCPreload_Enable);
//          D8050=PUS_TOTAL; pus_cnt0=0;   pus0=PUS_TOTAL;
//          // TIM3 clock source enable  ETR MODE    
//          if (PUS_TOTAL>0)
//          {
//             if (PUS_TOTAL>1)
//             {
//                RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
//                TIM_TimeBaseStructure.TIM_Period = PUS_TOTAL-1;
//                TIM_TimeBaseStructure.TIM_Prescaler = 0x00;
//                TIM_TimeBaseStructure.TIM_ClockDivision = 0x0;
//                TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
//                TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);  // Time base configuration

//                TIM_ETRClockMode2Config(TIM3, TIM_ExtTRGPSC_OFF, TIM_ExtTRGPolarity_NonInverted, 0);

//                TIM_SetCounter(TIM3, 0);
//                TIM_ClearFlag(TIM3, TIM_FLAG_Update);
//                TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE); 
//                TIM_Cmd(TIM3, ENABLE);
//             }
//             else
//             TIM_ITConfig(TIM5, TIM_IT_CC2, ENABLE);//打开中断，中断需要这行代码
//         }
//      // TIM5 enable counter
//        TIM_Cmd(TIM5, ENABLE); 
//        PLC_BIT_ON(0x00E0,147);    //M8147 
//       }
//       if ((addr==0x01)&&(PLC_BIT_TEST(0x00E0,146)))                //Y1
//       {
//         PLC_BIT_ON(0XC0,1);                         //Y01 ON
//         PLC_BIT_OFF(0x00E0,29);                       //M8029
//         PLC_Ws(0X20005E92)=0;                      //D8140
//         Y1P=1;
//         div_c=75497472/temp;
//         div_c=div_c/65536;
//         temp=75497472/(temp*(div_c+1));
//         temp--;
//         TIM_DeInit(TIM8);
//         //PWM MODE
//         RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE); 
//         GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
//         GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
//         GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//         GPIO_Init(GPIOC, &GPIO_InitStructure);
//         
//         TIM_TimeBaseStructure.TIM_Prescaler = div_c;
//         TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Down;
//         TIM_TimeBaseStructure.TIM_Period = temp;
//         D8052=PUS_TOTAL;
//         TIM_TimeBaseStructure.TIM_ClockDivision = 0;
//         TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

//         TIM_TimeBaseInit(TIM8, &TIM_TimeBaseStructure);
//         temp=(temp+1)/2;
//         // Channel 1, 2,3 and 4 Configuration in PWM mode 
//         TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
//         TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
//         TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
//         TIM_OCInitStructure.TIM_Pulse =temp;
//         TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
//         TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_Low;
//         TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
//         TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCIdleState_Reset;

//         TIM_OC3Init(TIM8, &TIM_OCInitStructure);
//         //TIM1 Main Output Enable 
//         TIM_CtrlPWMOutputs(TIM8, ENABLE);
//         D8052=PUS_TOTAL; pus_cnt1=0;pus1=PUS_TOTAL;

//         if (PUS_TOTAL>0)
//         {
//            // TIM4 clock source enable  ETR MODE    
//            if (PUS_TOTAL>1)
//            {
//               RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
//               TIM_TimeBaseStructure.TIM_Period = PUS_TOTAL-1;
//               TIM_TimeBaseStructure.TIM_Prescaler = 0x00;
//               TIM_TimeBaseStructure.TIM_ClockDivision = 0x0;
//               TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
//               TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);  // Time base configuration

//               TIM_ETRClockMode2Config(TIM4, TIM_ExtTRGPSC_OFF, TIM_ExtTRGPolarity_NonInverted, 0);

//               TIM_SetCounter(TIM4, 0);
//               TIM_ClearFlag(TIM4, TIM_FLAG_Update);
//               TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

//               TIM_Cmd(TIM4, ENABLE);
//             }
//             else
//             TIM8->DIER  = 0x02;   // 使能 CC1 中断
//          }
//          // TIM1 counter enable 
//           TIM_Cmd(TIM8, ENABLE); 
//           PLC_BIT_ON(0x00E0,148);    //M8148
//      }
//    }
//    else  
//    {   
//       PLC_Addr+=4;
//       addr=*PLC_Addr;          //输出地址
//       PLC_Addr+=2;
//       if (addr==0x00)                //Y0
//       {
//           if (Y0P==1)
//           { 
//                TIM_Cmd(TIM5, DISABLE); 
//                TIM_Cmd(TIM3, DISABLE);
//                TIM_ClearFlag(TIM3, TIM_FLAG_Update);
//                TIM_ITConfig(TIM3, TIM_IT_Update, DISABLE);
//   
//                GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
//                GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
//                GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//                GPIO_Init(GPIOA, &GPIO_InitStructure);            
// //               rsty(0);
//                Y0P=0;
//                PLC_BIT_OFF(0x00C0,0);      //Y00 OFF
//                PLC_BIT_OFF(0x00E0,147);    //M8147
//           }
//       }
//       if (addr==0x01)                //Y0
//       {
//           if (Y1P==1)
//           {
//                TIM_Cmd(TIM8, DISABLE); 
//                TIM_Cmd(TIM4, DISABLE);
//                TIM_ClearFlag(TIM4, TIM_FLAG_Update);
//                TIM_ITConfig(TIM4, TIM_IT_Update, DISABLE);
//   
//                GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
//                GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
//                GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//                GPIO_Init(GPIOC, &GPIO_InitStructure);
//    
// //               rsty(1);
//                Y1P=0;
//                PLC_BIT_OFF(0x00C0,1);      //Y01 OFF
//                PLC_BIT_OFF(0x00E0,148);    //M8148
//            }
//       }
//    }
// }
