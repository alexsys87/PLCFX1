static void PID(void)
{
/*
  s16 PVn;  // 测量值
  s16 SV;   // 设定目标Desired value
  s16 Ts;   // 取样时间
  s32 Su;
  s16 KP; // P 
  s16 Ti; // I
  s16 KD; // D
  s16 TD; // 微分增益
  u32 Addr,Addr1; // 地址记录
  u32 csp;        // PID内部计算地址
  if ( PLC_ACC_BIT & 0x01 )        //母线成立不
  {
    SV= PLC_Hs(addr_value_prog());   // 设定值
    PVn=PLC_Hs(addr_value_prog());  // 测量数据
    Addr=addr_value_prog();            // 读取参数起始地址
    Addr1= addr_value_prog();
    Ts=PLC_Hs(Addr);                // 取样时间
    KP=PLC_Hs(Addr+6);  // P
    Ti=PLC_Hs(Addr+8);  // I
    KD=PLC_Hs(Addr+10); // D
    TD=PLC_Hs(Addr+12); // 微分增益
    csp=Addr+14;           // 内部计算起始地址
     
    PLC_Hs(csp+14)=KP*((PLC_Hs(csp)-PLC_Hs(csp+2))+((Ts/Ti)*PLC_Hs(csp))+PLC_Hs(csp+10)); 
    
    PLC_Hs(csp) = PLC_Hs(csp+4) - SV;         // 计算本次偏差值    
    
    PLC_Hs(csp+4)= PLC_Hs(Addr+4)*PLC_Hs(csp+6)+(1-PLC_Hs(Addr+4))*PVn; 
    
    PLC_Hs(csp+10)=(TD/(Ts+KD*TD))*(-2*PLC_Hs(csp+6)+PLC_Hs(csp+4)+PLC_Hs(csp+8))+((KD*TD)/(Ts+KD*TD))*PLC_Hs(csp+12);
    
    
    Su=PLC_Hs(Addr1)+PLC_Hs(csp+14);
    if (Su>32766)       PLC_Hs(Addr1)=32767;
    else if (Su<-32767) PLC_Hs(Addr1)=-32768;
    else PLC_Hs(Addr1)= Su;
    PLC_Hs(csp+12)=PLC_Hs(csp+10);
    PLC_Hs(csp+8)=PLC_Hs(csp+6);
    PLC_Hs(csp+6)=PLC_Hs(csp+4);
    PLC_Hs(csp+2)=PLC_Hs(csp);
  }
*/
}
