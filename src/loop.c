      do {
        temp = *PLC_PC;
        CMD_REG = *PLC_PC & 0x0FFF;
        CMD_ADDR = PLC_PC - PLC_START_ADDR;
        PLC_PC++;
        switch ( temp >> 8 ) {
        case 0x06: operation_T(); break;  //operation all timer
        case 0x0E: operation_C(); break;  //
        
        /* ??S???????? */
//        case 0x20: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x30: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x40: AND(CMD_REG); PLC_PC++;            break;  //
//        case 0x50: ANI(CMD_REG); PLC_PC++;            break;  //
//        case 0x60: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x70: ORI(CMD_REG); PLC_PC++;            break;  // 
        /* ??S???????? */
//        case 0x21: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x31: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x41: AND(CMD_REG); PLC_PC++;            break;  //
//        case 0x51: ANI(CMD_REG); PLC_PC++;            break;  //
//        case 0x61: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x71: ORI(CMD_REG); PLC_PC++;            break;  //
        /* ??S???????? */
//        case 0x22: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x32: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x42: AND(CMD_REG); PLC_PC++;            break;  //
//        case 0x52: ANI(CMD_REG); PLC_PC++;            break;  //
//        case 0x62: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x72: ORI(CMD_REG); PLC_PC++;            break;  //
        /* ??S???????? */
//        case 0x23: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x33: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x43: AND(CMD_REG); PLC_PC++;            break;  //
//        case 0x53: ANI(CMD_REG); PLC_PC++;            break;  //
//        case 0x63: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x73: ORI(CMD_REG); PLC_PC++;            break;  //
        /* ??X???????? */
//        case 0x24: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x34: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x44: AND(CMD_REG); PLC_PC++;            break;  //
//        case 0x54: ANI(CMD_REG); PLC_PC++;            break;  //
//        case 0x64: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x74: ORI(CMD_REG); PLC_PC++;            break;  //
        /* ??Y???????? */
//        case 0x25: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x35: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x45: AND(CMD_REG); PLC_PC++;            break;
//        case 0x55: ANI(CMD_REG); PLC_PC++;            break;
//        case 0x65: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x75: ORI(CMD_REG); PLC_PC++;            break;  //
//        case 0XC5: OUT(CMD_REG); PLC_PC++;            break;  //
//        case 0XD5: BSET(CMD_REG); PLC_PC++;        break;  //
//        case 0XE5: RST(CMD_REG); PLC_PC++;            break;  //
        /* ??T???????? */
//        case 0x26: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x36: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x46: AND(CMD_REG); PLC_PC++;            break;
//        case 0x56: ANI(CMD_REG); PLC_PC++;            break;
//        case 0x66: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x76: ORI(CMD_REG); PLC_PC++;            break;  //
//        case 0XC6: OUT(CMD_REG); PLC_PC++;            break;  //
        /* ??T???????? */
//        case 0x27: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x37: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x47: AND(CMD_REG); PLC_PC++;            break;
//        case 0x57: ANI(CMD_REG); PLC_PC++;            break;
//        case 0x67: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x77: ORI(CMD_REG); PLC_PC++;            break;  //
//        case 0XC7: OUT(CMD_REG); PLC_PC++;            break;  //
        /* ??M0_255???????? */
//        case 0x28: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x38: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x48: AND(CMD_REG); PLC_PC++;            break;
//        case 0x58: ANI(CMD_REG); PLC_PC++;            break;
//        case 0x68: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x78: ORI(CMD_REG); PLC_PC++;            break;  //
//        case 0XC8: OUT(CMD_REG); PLC_PC++;            break;  //
//        case 0XD8: BSET(CMD_REG); PLC_PC++;        break;  //
//        case 0XE8: RST(CMD_REG); PLC_PC++;            break;  //  
        /* ??M256_511???????? */
//        case 0x29: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x39: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x49: AND(CMD_REG); PLC_PC++;            break;
//        case 0x59: ANI(CMD_REG); PLC_PC++;            break;
//        case 0x69: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x79: ORI(CMD_REG); PLC_PC++;            break;  //
//        case 0XC9: OUT(CMD_REG); PLC_PC++;            break;  //
//        case 0XD9: BSET(CMD_REG); PLC_PC++;        break;  //
//        case 0XE9: RST(CMD_REG); PLC_PC++;            break;  //
        /* ??M512_767???????? */
//        case 0x2A: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x3A: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x4A: AND(CMD_REG); PLC_PC++;            break;
//        case 0x5A: ANI(CMD_REG); PLC_PC++;            break;
//        case 0x6A: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x7A: ORI(CMD_REG); PLC_PC++;            break;  //
//        case 0XCA: OUT(CMD_REG); PLC_PC++;            break;  //
//        case 0XDA: BSET(CMD_REG); PLC_PC++;        break;  //
//        case 0XEA: RST(CMD_REG); PLC_PC++;            break;  //
        /* ??M768_1023???????? */
//        case 0x2B: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x3B: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x4B: AND(CMD_REG); PLC_PC++;            break;
//        case 0x5B: ANI(CMD_REG); PLC_PC++;            break;
//        case 0x6B: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x7B: ORI(CMD_REG); PLC_PC++;            break;  //
//        case 0XCB: OUT(CMD_REG); PLC_PC++;            break;  //
//        case 0XDB: BSET(CMD_REG); PLC_PC++;        break;  //
//        case 0XEB: RST(CMD_REG); PLC_PC++;            break;  //
        /* ??M1024_1279???????? */
//        case 0x2C: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x3C: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x4C: AND(CMD_REG); PLC_PC++;            break;
//        case 0x5C: ANI(CMD_REG); PLC_PC++;            break;
//        case 0x6C: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x7C: ORI(CMD_REG); PLC_PC++;            break;  //
//        case 0XCC: OUT(CMD_REG); PLC_PC++;            break;  //
//        case 0XDC: BSET(CMD_REG); PLC_PC++;        break;  //
//        case 0XEC: RST(CMD_REG); PLC_PC++;            break;  //
        /* ??M1280_1535???????? */
//        case 0x2D: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x3D: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x4D: AND(CMD_REG); PLC_PC++;            break;
//        case 0x5D: ANI(CMD_REG); PLC_PC++;            break;
//        case 0x6D: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x7D: ORI(CMD_REG); PLC_PC++;            break;  //
//        case 0XCD: OUT(CMD_REG); PLC_PC++;            break;  //
//        case 0XDD: BSET(CMD_REG); PLC_PC++;        break;  //
//        case 0XED: RST(CMD_REG); PLC_PC++;            break;  //
        /* ??C0-C255???????? */
//        case 0x2E: LD(CMD_REG); PLC_PC++;             break;  //
//        case 0x3E: LDI(CMD_REG); PLC_PC++;            break;  //
//        case 0x4E: AND(CMD_REG); PLC_PC++;            break;
//        case 0x5E: ANI(CMD_REG); PLC_PC++;            break;
//        case 0x6E: OR(CMD_REG); PLC_PC++;             break;  //
//        case 0x7E: ORI(CMD_REG); PLC_PC++;            break;  //
        /*m8000-m8255*/
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25: 
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B: 
        case 0x2C:
        case 0x2D:
        case 0x2E: 
        case 0x2F: LD(); break;
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35: 
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B: 
        case 0x3C:
        case 0x3D:
        case 0x3E: 
        case 0x3F: LDI(); break;
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45: 
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B: 
        case 0x4C:
        case 0x4D:
        case 0x4E: 
        case 0x4F: AND(); break;
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55: 
        case 0x56:
        case 0x57:
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B: 
        case 0x5C:
        case 0x5D:
        case 0x5E: 
        case 0x5F: ANI(); break;
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65: 
        case 0x66:
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B: 
        case 0x6C:
        case 0x6D:
        case 0x6E: 
        case 0x6F: OR(); break;
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75: 
        case 0x76:
        case 0x77:
        case 0x78:
        case 0x79:
        case 0x7A:
        case 0x7B: 
        case 0x7C:
        case 0x7D:
        case 0x7E: 
        case 0x7F: ORI(); break;
        case 0xC0:
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC4:
        case 0xC5: 
        case 0xC6:
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA:
        case 0xCB: 
        case 0xCC:
        case 0xCD:
        case 0xCE: 
        case 0XCF: OUT(); break;
        case 0xD0:
        case 0xD1:
        case 0xD2:
        case 0xD3:
        case 0xD4:
        case 0xD5: 
        case 0xD6:
        case 0xD7:
        case 0xD8:
        case 0xD9:
        case 0xDA:
        case 0xDB: 
        case 0xDC:
        case 0xDD:
        case 0xDE: 
        case 0XDF: BSET(); break;
        case 0xE0:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE4:
        case 0xE5: 
        case 0xE6:
        case 0xE7:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB: 
        case 0xEC:
        case 0xED:
        case 0xEE: 
        case 0XEF: RST(); break;
        /**********************STL????***************************/
        case 0xF0:
        case 0xF1:
        case 0xF2:
        case 0xF3: STL(); break;
        
        case 0XF7: RET(); break;
            
        case 0XFF:
//Basic logic instructions 2, such as ANB, ORB, MPP, MRD, MPS, INV, etc.
          switch( temp & 0x00FF ) {
            case 0xF8: //ANB 
              PLC_ACC_BIT = (PLC_ACC_BIT >> 1) & ( ( PLC_ACC_BIT & 0x01 ) | 0xFE );
              break;
            case 0xF9: //ORB
              PLC_ACC_BIT = (PLC_ACC_BIT >> 1) | (PLC_ACC_BIT & 0x01);
              break;
            case 0xFA: //MPS
              PLC_MPS_BIT = (PLC_MPS_BIT << 1) | (PLC_ACC_BIT & 0x01);
              break;
            case 0xFB: //MRD
              PLC_ACC_BIT = (PLC_ACC_BIT & 0xFE) | (PLC_MPS_BIT & 0x01);
              break;
            case 0xFC: //MPP
              PLC_ACC_BIT = (PLC_ACC_BIT & 0xFE) | (PLC_MPS_BIT & 0x01);
              PLC_MPS_BIT >>= 1;
              break;
            case 0xFD: //INV  
              PLC_ACC_BIT = (PLC_ACC_BIT & 0xFE) | (~PLC_ACC_BIT & 0x01);                 
              break;
            case 0xFF: //POP  
              break;
            default:
              PLC_PROG_ERROR(M8064,02);
              break;
          }
          break;   //MPP,MPS

        case 0xB0: 
//Pointer P identification
          break;  
        
        default:
//Encountered an unsupported command, here you need to execute the command as a 16bit command
          CMD_PULS = ( temp & 0x1000 != 0 ) ? 1 : 0;
          CMD_REG = *PLC_PC & 0x2FFF;
//          PLC_PC++;
          CMD_PULS_TEST = ( ( PLC_ACC_BIT & 0x01 ) & !CMD_PULS ) || ( PLC_LDP_TEST() & CMD_PULS );
          switch (temp & 0x0FFF) {
// --------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------
//            case 0x0001: ???();                     break;

            case 0x0002: OUT();                     break; // EOUT();  Mxxx Yxxx
            case 0x0003: BSET();                    break; // EBSET(); Mxxx Yxxx
            case 0x0004: RST();                     break; // ERST();  Mxxx Yxxx

            case 0x0005: OUT();                     break; // EOUT();  Sxxx
            case 0x0006: BSET();                    break; // EBSET(); Sxxx
            case 0x0007: RST();                     break; // ERST();  Sxxx

            case 0x0008: PLS();                     break; // Mxxx Yxxx
            case 0x0009: PLF();                     break; // Mxxx Yxxx

//            case 0x000A: MC();                      break; // Mxxx Yxxx
//            case 0x000B: MCR();                     break;

            case 0x000C: RST_TC();                  break; // TC
            case 0x000D: RST_D();                   break; // D

//            case 0x000E: ???();                     break;

            case 0x000F: goto all_end;
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// Basic Application Instructions = (FNC.No.n 8) * 2
// D Application command          = (FNC.No.n 8) * 2 + 1
// P application command          = (FNC.No.n 8) * 2 + 0x1000
// --------------------------------------------------------------------------------
// DOC 6.2.1 - 6.2.7
// --------------------------------------------------------------------------------
            case 0x0010: CJ();                      break;
            case 0x0012: CALL();                    break;
            case 0x0014: SRET();                    break;

//            case 0x0016: IRET();                    break;
//            case 0x0018: EI();                      break;
//            case 0x001A: DI();                      break;

            case 0x001C: goto all_end;
            
//            case 0x001E: WDT();                     break;
//            case 0x0020: FOR();                     break;
//            case 0x0022: NEXT();                    break;
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// DOC 6.3.1 - 6.3.10
// --------------------------------------------------------------------------------
#ifdef USE_COMPARE_MOVE

            case 0x0024: CMP();                     break;
#ifdef USE_DOUBLE
            case 0x0025: DCMP();                    break;
#endif

            case 0x0026: ZCP();                     break;
#ifdef USE_DOUBLE
            case 0x0027: DZCP();                    break;
#endif

            case 0x0028: MOV();                     break;
#ifdef USE_DOUBLE
            case 0x0029: DMOV();                    break;
#endif

            case 0x002A: SMOV();                    break;

            case 0x002C: CML();                     break;
#ifdef USE_DOUBLE
            case 0x002D: DCML();                    break;
#endif

            case 0x002E: BMOV();                    break;

            case 0x0030: FMOV();                    break;
#ifdef USE_DOUBLE
            case 0x0031: DFMOV();                   break;
#endif

            case 0x0032: XCH();                     break;
#ifdef USE_DOUBLE
            case 0x0033: DXCH();                    break;
#endif

            case 0x0034: BCD();                     break;
#ifdef USE_DOUBLE
            case 0x0035: DBCD();                    break;
#endif

            case 0x0036: BIN();                     break;
#ifdef USE_DOUBLE
            case 0x0037: DBIN();                    break;
#endif

#endif // USE_COMPARE_MOVE
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
#ifdef USE_ARIPH

            case 0x0038: ADD();                     break;
#ifdef USE_DOUBLE
            case 0x0039: DADD();                    break;
#endif
            case 0x003A: SUB();                     break;
#ifdef USE_DOUBLE
            case 0x003B: DSUB();                    break;
#endif

#ifdef USE_MULDIV
            case 0x003C: MUL();                     break;
  #ifdef USE_DOUBLE
            case 0x003D: DMUL();                    break;
  #endif
            case 0x003E: DIV();                     break;
  #ifdef USE_DOUBLE
            case 0x003F: DDIV();                    break;
  #endif
#endif

            case 0x0040: INC();                     break;
#ifdef USE_DOUBLE
            case 0x0041: DINC();                    break;
#endif
            case 0x0042: DEC();                     break;
#ifdef USE_DOUBLE
            case 0x0043: DDEC();                    break;
#endif
            case 0x0044: WAND();                    break;
#ifdef USE_DOUBLE
            case 0x0045: DWAND();                   break;
#endif
            case 0x0046: WOR();                     break;
#ifdef USE_DOUBLE
            case 0x0047: DWOR();                    break;
#endif
            case 0x0048: WXOR();                    break;
#ifdef USE_DOUBLE
            case 0x0049: DWXOR();                   break;
#endif
            case 0x004A: NEG();                     break;
#ifdef USE_DOUBLE
            case 0x004B: DNEG();                    break;
#endif

#endif // USE_ARIPH
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
#ifdef USE_ROTATE

            case 0x004C: ROR();                     break;
  #ifdef USE_DOUBLE
            case 0x004D: DROR();                    break;
  #endif
            case 0x004E: ROL();                     break;
  #ifdef USE_DOUBLE
            case 0x004F: DROL();                    break;
  #endif
            case 0x0050: RCR();                     break;
  #ifdef USE_DOUBLE
            case 0x0051: DRCR();                    break;
  #endif
            case 0x0052: RCL();                     break;
  #ifdef USE_DOUBLE
            case 0x0053: DRCL();                    break;
  #endif

//            case 0x0054: SFTR();                    break;
//            case 0x0056: SFTL();                    break;
//            case 0x0058: WSFR();                    break;
//            case 0x005A: WSFL();                    break;
//            case 0x005C: SFWR();                    break;
//            case 0x005E: SFWL();                    break;

#endif // USE_ROTATE
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
            case 0x0060: ZRST();                    break; 
//            case 0x0062: DECO();                    break;
//            case 0x0064: ENCO();                    break;
//            case 0x0066: SUM();                     break;
  #ifdef USE_DOUBLE
//            case 0x0067: DSUM();                    break;
  #endif
//            case 0x0068: BON();                     break;
  #ifdef USE_DOUBLE
//            case 0x0069: DBON();                    break;
  #endif

            case 0x006A: MEAN();                    break;
  #ifdef USE_DOUBLE
//            case 0x006B: DMEAN();                   break;
  #endif
//            case 0x006C: ANS();                     break;
//            case 0x006E: ANR();                     break;

#ifdef USE_MULDIV
            case 0x0070: SQR();                     break;
  #ifdef USE_DOUBLE
            case 0x0071: DSQR();                    break;
  #endif
#endif // USE_MULDIV

#ifdef USE_FLOAT
            case 0x0072: FLT();                     break;
  #ifdef USE_DOUBLE
            case 0x0073: DFLT();                    break;
  #endif
#endif // USE_FLOAT
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
//            case 0x0074: REF();                     break;
            case 0x0076: REFF();                    break;
            case 0x0078: MTR();                     break;
#ifdef USE_DOUBLE
            case 0x007A: DHSCS();                   break;
//            case 0x007C: DHSCR();                   break;
//            case 0x007E: DHSZ();                    break;
#endif

//            case 0x0080: SPD();                     break;
#ifdef USE_DOUBLE
//            case 0x0081: DSPD();                    break;
#endif

//            case 0x0082: PLSY();                    break;
//            case 0x0084: PWM();                     break;
//            case 0x0086: PLSR();                    break;
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
//            case 0x0088: IST();                     break;
//            case 0x008A: SER();                     break;
#ifdef USE_DOUBLE
//            case 0x008B: DSER();                    break;
#endif
//            case 0x008C: ABSD();                    break;
#ifdef USE_DOUBLE
//            case 0x008D: DABSD();                   break;
#endif
//            case 0x008E: INCD();                    break;
//            case 0x0090: TTMR();                    break;
//            case 0x0092: STMR();                    break;
            case 0x0094: ALT();                     break;
//            case 0x0096: RAMP();                    break;
//            case 0x0098: ROTS();                    break;
//            case 0x009A: SORT();                    break;
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
#ifdef USE_SPECIAL
//            case 0x009C: TKY();                     break;
//            case 0x009E: HKY();                     break;
//            case 0x00A0: DSW();                     break;
//            case 0x00A2: SEGD();                    break;
//            case 0x00A4: SEGL();                    break;
//            case 0x00A6: ARWS();                    break;
//            case 0x00A8: ASC();                     break;
//            case 0x00AA: PR();                      break;

//            case 0x00AC: FROM();                    break;
//            case 0x00AE: TO();                      break;

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
//            case 0x00B0: RS();                      break;
//            case 0x00B2: PRUN();                    break;
//            case 0x00B4: ASCI();                    break;
//            case 0x00B6: HEX();                     break;
//            case 0x00B8: CCD();                     break;
//            case 0x00BA: VRRD();                    break;
//            case 0x00BC: VRSC();                    break;
//            case 0x00BE: RS2???();                  break;

//            case 0x00C0: PID();                     break;

//            case 0x00C2: ZPUSH???();                break;
//            case 0x00C4: ZPOP???();                 break;
#endif // USE_SPECIAL
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
//            case 0x00C6: ???();                    break;
//            case 0x00C8: ???();                    break;
//            case 0x00CA: ???();                    break;
//            case 0x00CC: ???();                    break;
//            case 0x00CE: ???();                    break;
//            case 0x00D0: ???();                    break;
//            case 0x00D2: ???();                    break;
//            case 0x00D4: ???();                    break;
//            case 0x00D6: ???();                    break;
//            case 0x00D8: ???();                    break;
//            case 0x00DA: ???();                    break;
//            case 0x00DC: ???();                    break;
//            case 0x00DE: ???();                    break;
//            case 0x00E0: ???();                    break;
//            case 0x00E2: ???();                    break;
//            case 0x00E4: ???();                    break;
//            case 0x00E6: ???();                    break;
//            case 0x00E8: ???();                    break;
//            case 0x00EA: ???();                    break;
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
#ifdef USE_FLOAT
//            case 0x00EC: ECMP();                    break;
            case 0x00ED: DECMP();                   break;
//            case 0x00EE: EZCP();                    break;
            case 0x00EF: DEZCP();                   break;

            case 0x00F1: DEMOV();                   break;
//            case 0x00F?: DESTR();                   break;
//            case 0x00F?: DEVAL();                   break;
//            case 0x00FC: EBCD();                    break;
//            case 0x00FE: EBIN();                    break;

            case 0x0101: DEADD();                   break;
            case 0x0103: DESUB();                   break;
  #ifdef USE_MULDIV
            case 0x0105: DEMUL();                   break;
            case 0x0107: DEDIV();                   break;
  #endif
//            case 0x0109: DEXP();                    break;
//            case 0x010B: DLOGE();                   break;
//            case 0x010D: DLOG10();                  break;
            case 0x010F: DESQR();                   break;
//            case 0x0111: DENEG();                   break;

            case 0x0112: INT();                     break;
  #ifdef USE_DOUBLE
            case 0x0113: DINT();                    break;
  #endif
            case 0x0115: DSIN();                    break;
            case 0x0117: DCOS();                    break;
            case 0x0119: DTAN();                    break;

//            case 0x011B: DASIN();                   break;
//            case 0x011D: DACOS();                   break;
//            case 0x011F: DATAN();                   break;
//            case 0x0121: DRAD();                    break;
//            case 0x0123: DDEG();                    break;
#endif // USE_FLOAT
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
//            case 0x012C: WSUM();                    break;
#ifdef USE_DOUBLE
//            case 0x012D: DWSUM();                   break;
#endif
//            case 0x012E: WTOB();                    break;
//            case 0x0130: BTOW();                    break;
//            case 0x0132: UNI();                     break;
//            case 0x0134: DIS();                     break;
            case 0x0136: SWAP();                    break;
#ifdef USE_DOUBLE
            case 0x0137: DSWAP();                   break;
#endif
//            case 0x0138: SORT2();                   break;
#ifdef USE_DOUBLE
//            case 0x0139: DSORT2();                  break;
#endif
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
//            case 0x013A: ???();                    break;
//            case 0x013C: ???();                    break;
//            case 0x013E: ???();                    break;
//            case 0x0140: ???();                    break;
//            case 0x0142: ???();                    break;
//            case 0x0144: ???();                    break;
//            case 0x0146: ABS???();                    break;
//            case 0x0148: ZRN???();                    break;
//            case 0x014A: PLSV???();                    break;
//            case 0x014C: DRVI???();                    break;
//            case 0x014E: DRVA???();                    break;
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
#ifdef USE_TIME
            case 0x0150: TCMP();                    break;
            case 0x0152: TZCP();                    break;
#ifdef USE_ARIPH
            case 0x0154: TADD();                    break;
            case 0x0156: TSUB();                    break;    
#endif
//            case 0x0158: HTOS();                    break;
//            case 0x015A: STOH();                    break;
            case 0x015C: TRD();                     break;
            case 0x015E: TWR();                     break;
//            case 0x0160: HOUR();                    break;
#endif // USE_TIME
// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
            case 0x0164: GRY();                     break;
#ifdef USE_DOUBLE
            case 0x0165: DGRY();                    break;
#endif
            case 0x0166: GBIN();                    break;
#ifdef USE_DOUBLE
            case 0x0167: DGBIN();                   break;
#endif
//            case 0x0168: RD3A();                    break;
//            case 0x016A: WR3A();                    break;

// --------------------------------------------------------------------------------
            case 0x01C2: LD();                      break; // ELD();  Mxxx
            case 0x01C3: LDI();                     break; // ELDI(); Mxxx
            case 0x01C4: AND();                     break; // EAND(); Mxxx
            case 0x01C5: ANI();                     break; // EANI(); Mxxx
            case 0x01C6: OR();                      break; // EOR();  Mxxx
            case 0x01C7: ORI();                     break; // EORI(); Mxxx

            case 0x01CA: LDP();                     break; // Mxxx
            case 0x01CB: LDF();                     break; // Mxxx
            case 0x01CC: ANDP();                    break; // Mxxx
            case 0x01CD: ANDF();                    break; // Mxxx
            case 0x01CE: ORP();                     break; // Mxxx
            case 0x01CF: ORF();                     break; // Mxxx

// --------------------------------------------------------------------------------
#ifdef USE_SPECIAL
            case 0x01D0: LD_EQU();                  break;
#ifdef USE_DOUBLE
            case 0x01D1: DLD_EQU();                 break;
#endif
            case 0x01D2: LD_GTR();                  break;
#ifdef USE_DOUBLE
            case 0x01D3: DLD_GTR();                 break;
#endif
            case 0x01D4: LD_LSS();                  break;
#ifdef USE_DOUBLE
            case 0x01D5: DLD_LSS();                 break;
#endif
            case 0x01D8: LD_NEQ();                  break;
#ifdef USE_DOUBLE
            case 0x01D9: DLD_NEQ();                 break;
#endif
            case 0x01DA: LD_LEQ();                  break;
#ifdef USE_DOUBLE
            case 0x01DB: DLD_LEQ();                 break;
#endif
            case 0x01DC: LD_GEQ();                  break;
#ifdef USE_DOUBLE
            case 0x01DD: DLD_GEQ();                 break;
#endif

            case 0x01E0: AND_EQU();                 break;
#ifdef USE_DOUBLE
            case 0x01E1: DAND_EQU();                break;
#endif
            case 0x01E2: AND_GTR();                 break;
#ifdef USE_DOUBLE
            case 0x01E3: DAND_GTR();                break;
#endif
            case 0x01E4: AND_LSS();                 break;
#ifdef USE_DOUBLE
            case 0x01E5: DAND_LSS();                break;
#endif
            case 0x01E8: AND_NEQ();                 break;
#ifdef USE_DOUBLE
            case 0x01E9: DAND_NEQ();                break;
#endif
            case 0x01EA: AND_LEQ();                 break;
#ifdef USE_DOUBLE
            case 0x01EB: DAND_LEQ();                break;
#endif
            case 0x01EC: AND_GEQ();                 break;
#ifdef USE_DOUBLE
            case 0x01ED: DAND_GEQ();                break;
#endif

            case 0x01F0: OR_EQU();                  break;
#ifdef USE_DOUBLE
            case 0x01F1: DOR_EQU();                 break;
#endif
            case 0x01F2: OR_GTR();                  break;
#ifdef USE_DOUBLE
            case 0x01F3: DOR_GTR();                 break;
#endif
            case 0x01F4: OR_LSS();                  break;
#ifdef USE_DOUBLE
            case 0x01F5: DOR_LSS();                 break;
#endif
            case 0x01F8: OR_NEQ();                  break;
#ifdef USE_DOUBLE
            case 0x01F9: DOR_NEQ();                 break;
#endif
            case 0x01FA: OR_LEQ();                  break;
#ifdef USE_DOUBLE
            case 0x01FB: DOR_LEQ();                 break;
#endif
            case 0x01FC: OR_GEQ();                  break;
#ifdef USE_DOUBLE
            case 0x01FD: DOR_GEQ();                 break;
#endif

#endif // USE_SPECIAL
// --------------------------------------------------------------------------------

            default:PLC_PROG_ERROR(M8065,02);       break;
          }
        }
      } while(1);

all_end: 
