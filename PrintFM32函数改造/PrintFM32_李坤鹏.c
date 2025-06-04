const float32 cnst_fBcdQFactor[] = {1e00, 1e01, 1e02, 1e03, 1e04, 1e05, 1e06, 1e07, 1e08, 1e09, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19};
const uint32 cnst_u32BcdQFactor[] = {1e00, 1e01, 1e02, 1e03, 1e04, 1e05, 1e06, 1e07, 1e08, 1e09};
/*==========================================================================
| Description	: 将浮点数转成指定位的字符串
| In/Out/G var	: [in] fData: 要转换的浮点数
|
|				  [in] u16LengthInParts: 以部分位表示要输出的位数 共16位,分为4部分,每部分4位.即表示整数位为1~4位时,应输出的位数(范围是0~F).
|                      本质是一个16位的16进制数   0xABCD.
|                      其中第x位表示整数位为x位时,应输出的总位数(从高位开始是第一位).
|				  例: 0x3254
|                 第一位是3   表示整数位为1位时   应输出3位 
|                 第二位是2   表示整数位为2位时   应输出2位 
|                 第三位是5   表示整数位为3位时   应输出5位
|                 第四位是4   表示整数位为4位时   应输出4位
|
|                 [in] u8UnitCarryControl: 单位进位控制.共8字节,分为2部分,每部分4位.
|                                          前4位表示最大进位次数,后4位表示进位倍数.
|                 例: 0x23
|                 第一位是2   表示最大进位次数是2
|                 第二位是3   表示进位倍数是3(满10进一,进位倍数是1;满100进一,进位倍数是2;满1000进一,进位倍数是3;满10000进一,进位倍数就是4;).
|                     0x00:   表示不会进位且不会限制fData的大小.
|                     0x03:   第一位为0表示最大进位次数是0,第二位是3表示每1000进一,但是由于最大进位次数为0,因此fData会被限制在999.
|                     0x04:   同理,不会进位但会限制fData的大小,限制在9999以内.
|                     0x23:   表示最大进位次数是2 例如: 单位从K -> M -> G.进位倍数是3,表示每1000(10的3次方),单位进1.
|                             那么fData的范围就限定在999,000,000K,也就是999G 可通过这个限制fData的范围.
|                 错误用法:
|                     0x10: 表示可以进位,最大进位次数是1,但是并没有给出进位倍数,程序会出错.
|                 注: 1.fData的范围通过这个参数来限制.
|					  2.最大进位次数非0的话,一定要给出进位倍数.否则程序会出错.
|
|                 [in] u16CarryLengthInParts: 与参数2的'u16LengthInParts'原理一样,不过这个表示发生单位进位后的输出格式.
|                      因为有些数会因单位进位而导致要输出的位数也发生改变.
|                      如果进位前后的格式不变,传0即可(表示会用参数一'u16LengthInParts'指定的格式输出).                   
|
|              [return] 进位标识,发生进位会返回相应的进位次数,没发生就返回0.
| 注:  1.要确保至少输出全部的整数位,否则cnst_u32BcdQFactor数组会溢出.也即是,第x位的值应该大于等于x.
|	   2.如果最终计算得到的整数位位数大于4,则仅输出所有整数位.
|      3.参数三中的最大进位次数若为0,那么参数4的值也应该相应的为0. 否则程序也会出现问题. 也就是说不进位时,参数4应该为0.
|      4.如果整数位为1位时,正数与负数输出的格式不一样, 如:
|      fData < 0时,输出-a.bc.   fData > 0时,输出a.bcd. 那么就会存在'0.00', '0.000'两种格式的'0'.
|      为保证输出的'0'的格式一致.在调用函数前应先讨论'fData < 0'时,按照'-a.bc'的格式四舍五入后是否仍为'0'.
|      若为'0'则直接输出正数格式的'0.000'即可.或者令fData = 0,再传入函数(但是没必要).
\=========================================================================*/
uint8 PrintFM32Inter(uint8** ppBuf, float32 fData, uint16 u16LengthInParts, uint8 u8UnitCarryControl, uint16 u16CarryLengthInParts)
{
	uint8 *pBuf = *ppBuf;
	int8 i;
    uint8 u8MaxCarryCount = (u8UnitCarryControl >> 4) & 0x0F;   /* 最大进位次数 */
    uint8 u8CarryMultiplier = u8UnitCarryControl & 0x0F;        /* 进位倍数 */
	uint32 u32Data;
    uint8 u8OutLen;         /* 应输出的总位数 */
    uint8 u8IntegerLen = 1; /* 整数位的位数 至少应是1 */
    uint8 u8Carry = 0;
    /* 检查是否要加'-'号 */
    if(fData <= (-5.0f / cnst_fBcdQFactor[(u16LengthInParts >> 12 & 0x0F)])) {   /* 需要加'-'号 */
        fData = fabsf(fData);
        *pBuf++ = '-';
    }
    do {    /* 这里是do-while(0) 目的是使判断是否溢出的分支成立时能跳出,不继续向下执行,节省运行时间. */
        if(u8UnitCarryControl) {    /* fData大小有限制 */
            /* 检查是否溢出 */
            u16CarryLengthInParts = u16CarryLengthInParts !=  0 ? u16CarryLengthInParts : u16LengthInParts;
            u8OutLen = u8CarryMultiplier > 4 ? u8CarryMultiplier : ((u16CarryLengthInParts >> (4 - u8CarryMultiplier)*4) & 0x0F);
            /* 若u8MaxCarryCount = 1(单位进位假如是: K->M), u8CarryMultiplier = 3 则最大值应是 (1,000 - 1)M */
            if(fData/cnst_fBcdQFactor[u8MaxCarryCount * u8CarryMultiplier] > cnst_fBcdQFactor[u8CarryMultiplier] - 1/cnst_fBcdQFactor[u8OutLen - u8CarryMultiplier]) {
                fData = cnst_fBcdQFactor[u8CarryMultiplier] - 1/cnst_fBcdQFactor[u8OutLen - u8CarryMultiplier];
                u8Carry = u8MaxCarryCount;
                u8IntegerLen = u8CarryMultiplier;
                break;  /* 这里跳出,不必再向下执行.因为既然溢出了,那么进位次数和要输出的位数也就确定了,不必再计算了. */
            } else {    /* 不溢出 */
                /* 检查单位是否要进位 */
                while((uint32)(fData / cnst_fBcdQFactor[u8CarryMultiplier]) != 0) {
                    u8Carry++;
                    fData /= cnst_fBcdQFactor[u8CarryMultiplier];
                }
                if(u8Carry != 0) {  /* 发生了单位进位,则输出的格式应该按照参数4输出 */
                    u16LengthInParts = u16CarryLengthInParts;
                }
            }
        }
        u32Data = (uint32)fData;
        while((u32Data /= 10) != 0) { /* 计算整数位的位数 */
            u8IntegerLen++;
        }
        u8OutLen = u8IntegerLen > 4 ? u8IntegerLen : ((u16LengthInParts >> (4-u8IntegerLen)*4) & 0x0F); /* 整数位大于4位仅输出所有整数位 */
    }while(0);
    u32Data = (uint32)(cnst_fBcdQFactor[u8OutLen - u8IntegerLen]*fData + 0.5f); /* 确保应输出的位都在整数位上 +0.5 四舍五入 */
	if(u32Data >= cnst_u32BcdQFactor[u8OutLen]) {   /* 因进位 整数位应+1 输出总位数也可能发生变化 因此需重新获取应输出的位数 */
        if(u8IntegerLen == u8CarryMultiplier) { /* 这里满足单位进位 (这里也隐含了u8CarryMultiplier!=0 因为 u8IntegerLen!=0) */
            u8IntegerLen = 1;
            u8Carry++;
            u16LengthInParts = u16CarryLengthInParts;
        } else {
            u8IntegerLen++;
        }
        uint8 u8Flag = u8IntegerLen > 4 ? u8IntegerLen : ((u16LengthInParts >> (4-u8IntegerLen)*4) & 0x0F); /* 同上 整数位大于4位仅输出所有整数位 */
        if(u8Flag > u8OutLen) {
            u32Data *= cnst_u32BcdQFactor[u8Flag - u8OutLen - 1];
        } else {
            u32Data /= cnst_u32BcdQFactor[u8OutLen - u8Flag + 1];
        }
		u8OutLen = u8Flag;
	}
	for(i = 0; i < u8IntegerLen; i++) {
		*pBuf++ = u32Data/cnst_u32BcdQFactor[u8OutLen - i - 1] + '0';
		u32Data = u32Data%cnst_u32BcdQFactor[u8OutLen - i - 1];
	}
	if(u8IntegerLen < u8OutLen) {
		*pBuf++ = '.';
		for(; i < u8OutLen; i++) {
			*pBuf++ = u32Data/cnst_u32BcdQFactor[u8OutLen - i - 1] + '0';
			u32Data = u32Data%cnst_u32BcdQFactor[u8OutLen - i - 1];
		}
	}
	*ppBuf = pBuf;
    return u8Carry;
}

BOOL PrintFM32(uint8** ppBuf, uint8* pBufEnd, float32 fData, F32_MEANING F32Meaning)
{
	if(F32Meaning == F32_DEFAULT) {
		return PrintF32(ppBuf, pBufEnd, fData, 5);
	} else if(F32Meaning <= F32_SgnDigits_10) {		/* 1~10位有效数字 */
		return PrintF32(ppBuf, pBufEnd, fData, F32Meaning);
	} else if(F32Meaning == F32_SoftVER) {			/* 软件版本号 */
		return PrintSoftVer(ppBuf, pBufEnd, (uint16)fData);
	} else {
		uint8* pBuf = *ppBuf;
		/* 输入检查 */
		if(pBuf == pBufEnd) {		/* Buf溢出检查 */
			return FALSE;
		} else if(isnan(fData)) {
			fData = 0;
		}
		uint8 u8Carry = 0;		/* 进位标识 */
		switch(F32Meaning) {
		    case F32_INT:   /* 最多4byte 整数, 无小数点. [-999,9999]:sabc,s是符号,    <-999:-999, >9999:9999 */
                if(fData < 0) {
                    PrintFM32Inter(&pBuf, fData, 0x1234, 0x03, 0);  /* 通过参数3限制了fData的范围 在[-999, 0) */
                } else {
                    PrintFM32Inter(&pBuf, fData, 0x1234, 0x04, 0);  /* 通过参数3限制了fData的范围 在[0, 9999] */
                }
		        break;
		        
		    case F32_TIME: {    /* 最多6byte, 时间：(100H,1H]:abHcdM, (60min,1min]:abMcds, (60s,10s]:abs, (10s,0s]:a.bcds */   
                uint32 u32Data = (uint32)(fData + 0.5f);                
                if((pBufEnd - pBuf < 6) || (fData < 0.0f)) {
                    return FALSE;
                } else {
                    if(u32Data >= 60) {   /* 双单位 */
                        uint16 uM = u32Data / 60;
                        if(uM >= 60) {  /* abHcdM */
                            uint16 uH = uM / 60;
                            uH = uH > 99 ? 99 : uH;
                            uM %= 60;
                            *pBuf++ = uH / 10 + '0';
                            *pBuf++ = uH % 10 + '0';
                            *pBuf++ = 'H';
                            *pBuf++ = uM / 10 + '0';
                            *pBuf++ = uM % 10 + '0';
                            *pBuf++ = 'M';
                        } else {    /* abMcds */
                            uint16 uS = u32Data % 60;
                            *pBuf++ = uM / 10 + '0';
                            *pBuf++ = uM % 10 + '0';
                            *pBuf++ = 'M';
                            *pBuf++ = uS / 10 + '0';
                            *pBuf++ = uS % 10 + '0';
                            *pBuf++ = 's';
                        }
                    } else {        /* 单位为s */
                        PrintFM32Inter(&pBuf, fData, 0x4200, 0, 0);
                        *pBuf++ = 's';
                    }
                }
                break;
            }

            /* 最多8byte, (-10mV, 0mV):-a.bmV/m, (-100mV,-10mV]:-abV/m, (-1V,-100mV]:-abcmV/m, (-10V,-1V]:-a.bcV/m, (-100V,-10V]:-ab.cV/m, (-1000V,-100V]:-abcV/m, <=-999:-999V/m
						  [0mV,10mV):a.bcmV/m, [10mV,100mV):ab.cmV/m, [100mV,1V):abcmV/m, [1V,10V):a.bcV/m, [10V,100V):ab.cV/m, [100V,1000V):abcV/m, >=999:999V/m */
			case F32_VpM:
				if(pBufEnd - pBuf < 8) {
					return FALSE;
				} else {
                    fData = fData * 1e3f;   /* 传入的fData单位是V 先转为mV */
                    if(fData <= 0) {    /* 因为正数与负数输出的格式不一样,会出现'0.0'和'0.00'两种格式的'0',下面的判断是为了统一'0'的格式. */
                        if(fData > -0.05f) {   /* (-0.05, 0]mV 认为是 0.00 既然已经判断成功了直接输出即可,不必再调用函数. */
                            *pBuf++ = '0';
                            *pBuf++ = '.';
                            *pBuf++ = '0';
                            *pBuf++ = '0';
                        } else {
                            u8Carry = PrintFM32Inter(&pBuf, fData, 0x2234, 0x13, 0x3334);    /* 通过参数3限制了fData的范围 在[-999, 0)V */
                        }
                    } else {
                        u8Carry = PrintFM32Inter(&pBuf, fData, 0x3334, 0x13, 0);     /* 通过参数3限制了fData的范围 在[0, 999]V */
                    }
                    if(u8Carry == 0) {
                        *pBuf++ = 'm';
                    }
					*pBuf++ = 'V';
					*pBuf++ = '/';
					*pBuf++ = 'm';
				}
				break;

			case F32_CENT:		/* 最多5byte, [0,100):ab.c%, [100,999):abc%, >=999:999% */
			case F32_TEMP:		/* 最多7byte, [0,100):ab.c℃, [100,999):abc℃, >=999:999℃ */
                if(pBufEnd - pBuf < (F32Meaning == F32_CENT ? 5 : 7)) {
					return FALSE;
				}
                PrintFM32Inter(&pBuf, fData, 0x2334, 0x03, 0);  /* 通过参数3限制了fData的范围在[-999, 999]℃ */
				if(F32Meaning == F32_CENT) {
					*pBuf++ = '%';
				} else if(F32Meaning == F32_TEMP) {
					PrintString(&pBuf, pBufEnd, "℃");
				}
				break;

			case F32_WATER:	/* 最多7byte：[0, 10)显示为a.bcdm, [10, inf]显示为 ab.cdm */
                if(pBufEnd - pBuf < 7) {
                    return FALSE;
                } else {
                    PrintFM32Inter(&pBuf, fData, 0x4444, 0x02, 0);      /* 通过参数3限制了fData的范围在[-99.99, 99.99]m */
                    *pBuf++ = 'm';
                }
                break;

			case F32_MPa:   /* 最多8byte：[-9.99, 0):-a.bcMPa, [-99.9, -9.99):-ab.cMPa, <-99.9:-99.9Mpa,
										  [0,9.999]:a.bcdMPa, (9.999,99.99]:ab.cdMPa, >99.99:99.99Mpa */
				if(pBufEnd - pBuf < 8) {
					return FALSE;
				}
                if(fData <= 0) {    /* 因为正数与负数输出的格式不一样,会出现'0.00'和'0.000'两种格式的'0',下面的判断是为了统一'0'的格式. */
                    if(fData > -0.005f) {   /* (-0.005, 0] 认为是 0.000 既然已经判断成功了直接输出即可,不必再调用函数. */
                        *pBuf++ = '0';
                        *pBuf++ = '.';
                        *pBuf++ = '0';
                        *pBuf++ = '0';
                        *pBuf++ = '0';
                    } else {
                        PrintFM32Inter(&pBuf, fData, 0x3334, 0x02, 0);  /* 通过参数3限制了fData的范围在[-99.9, 0)Mpa */
                    }
                } else {
                    PrintFM32Inter(&pBuf, fData, 0x4444, 0x02, 0);      /* 通过参数3限制了fData的范围在[0, 99.99]Mpa */
                }
                *pBuf++ = 'M';
                *pBuf++ = 'P';
                *pBuf++ = 'a';
				break;

			case F32_FREQ:	/*最多7byte, [0,99.99]:ab.cdHz, [100,999.9):abc.dHz, >=999.9: 999.9Hz */
			case F32_BatAH: /*最多7byte, [0,99.99]:ab.cdAH, [100,999.9):abc.dAH, >=999.9: 999.9AH */
				if(pBufEnd - pBuf < 7) {	
					return FALSE;
				} else {
					PrintFM32Inter(&pBuf, fData, 0x3444, 0x03, 0);  /* 通过参数3限制了fData的范围在999.9Hz */
				}
				/* 单位部分 */
				if(F32Meaning == F32_FREQ) {
					*pBuf++ = 'H';
					*pBuf++ = 'z';
				} else if(F32Meaning == F32_BatAH) {
					*pBuf++ = 'A';
					*pBuf++ = 'H';
				}
				break;
			
			case F32_VOL:/* 最多7byte [0,10):a.bcdV, [10,100):ab.cdV, [100,8e3):abcdV, [8e3,1e4):a.bcKV, [1e4, 1e5):ab.cKV, >=1e5:abcdKV */
			case F32_CUR:/* 最多7byte, 同F32_VOL, 单位为A */
                if(pBufEnd - pBuf < 7) {
                    return FALSE;
                }
                if(fData <= -5e-4f) {   /* (-∞,-0.0005] 需要加'-'号 */
                    fData = fabsf(fData);
                    *pBuf++ = '-';
                }
                if(fData >= 7999.5f) {   /* V->KV 单位进位 */
                    fData = fData < 8000.0f ? 8000.0f : fData;  /* [7999.5, 8000.0)V 认为是8000V */
                    fData = fData/1000.0f;
                    u8Carry = 1;
                }
                if(fData > 9999.0f) {   /* 溢出 */
					fData = 9999;
				}
                if(u8Carry == 0) {
                	PrintFM32Inter(&pBuf, fData, 0x4434, 0, 0); /* 上面限制了fData的大小 这里传参就不用限制了 */
                } else {
                	PrintFM32Inter(&pBuf, fData, 0x3334, 0, 0);
                }
                if(u8Carry == 1) {
                    *pBuf++ = 'K';
				}
                if(F32Meaning == F32_VOL) {
					*pBuf++ = 'V';
				} else {
					*pBuf++ = 'A';
				}
                break;

			case F32_PwrP:/*最多7byte:  [0,10):a.bcKW, [10,100):ab.cKW, [100,1000): abcKW, [1000, 1e4):a.bcMW, [1e4, 1e5):ab.cMW, [1e5, 1e6):abcMW */
                if(pBufEnd - pBuf < 7) {
                    return FALSE;
                }
                u8Carry = PrintFM32Inter(&pBuf, fData, 0x3334, 0x13, 0);    /* 通过参数3限制了fData的范围在[-999, 999]MW */
                if(u8Carry == 1) {
                    *pBuf++ = 'M';
				} else {
					*pBuf++ = 'K';
				}
                *pBuf++ = 'W';
                break;
                                
			case F32_PwrQ:/*最多8byte:  (-10,0):-a.bKVar, (-100,-10]:-abKVar, (-1000,-100]:-abcKVar, (-1e4, 1000]:-a.bMVar, (-1e5, -1e4]:-abMVar, (-1e6, -1e5]:-abcMVar
                                        [0,10):a.bcKVar, [10,100):ab.cKVar, [100,1000): abcKVar, [1000, 1e4):a.bcMVar, [1e4, 1e5):ab.cMVar, [1e5, 1e6):abcMVar*/
                if(pBufEnd - pBuf < 8) {
					return FALSE;
				}
                if(fData <= 0) {    /* 因为正数与负数输出的格式不一样,会出现'0.0'和'0.00'两种格式的'0',下面的判断是为了统一'0'的格式. */
                    if(fData > -0.05f) {    /* (-0.05, 0] 认为是 0.00 既然已经判断成功了直接输出即可,不必再调用函数. */
                        *pBuf++ = '0';
                        *pBuf++ = '.';
                        *pBuf++ = '0';
                        *pBuf++ = '0';
                    } else {
                        u8Carry = PrintFM32Inter(&pBuf, fData, 0x2234, 0x13, 0);  /* 通过参数3限制了fData的范围在[-999, 0)MVar */
                    }
                } else {
                    u8Carry = PrintFM32Inter(&pBuf, fData, 0x3334, 0x13, 0);  /* 通过参数3限制了fData的范围在[0, 999]MVar */
                }
				if(u8Carry == 1) {
                    *pBuf++ = 'M';
				} else {
					*pBuf++ = 'K';
				}
                *pBuf++ = 'V';
                *pBuf++ = 'a';
                *pBuf++ = 'r';
				break;
			
			case F32_COS:	/* 最多6byte, [-1.1,1.1]:a.bcd, 除此之外，仅显示整数部分 */
				if(pBufEnd - pBuf < 6) {
					return FALSE;
				} else if(fData <= -5e-4f) {   /* 需要加'-'号 (-∞, -0.0005] */
                    fData = fabsf(fData);
                    *pBuf++ = '-';
                }
                if(fData > 1.1f) {     /* 特殊含义的指令 */
					if(fData >= 9.0f) {
						*pBuf++ = '9';
					} else {
						*pBuf++ = (uint16)fData + '0';
					}
                    break;
				}
                /* 普通cos */
                PrintFM32Inter(&pBuf, fData, 0x4234, 0, 0); /* 上面已经限制过fData大小 这里就不用限制 */
				break;

/* 有/无功电度(中,8byte):[0,10):a.bc度, [10,1e3):abc.d度, [1e3,1e4):abcd度, [1e4,1e5)a.bc万度, [1e5,1e6):ab.c万度, [1e6,1e8):abcd万度, 最大9999亿度
   有功电度(英,8byte):[0,100):ab.cdKWH, [100,1e3):abc.dKWH, [1e3,1e5):ab.cdMWH, [1e5,1e6)abc.dMWH, 最大999.9TWH
   无功电度(英,8byte):[0,100):ab.cdKVH, [100,1e3):abc.dKVH, [1e3,1e5):ab.cdMVH, [1e5,1e6)abc.dMVH, 最大999.9TVH */
			case F32_EngP:
			case F32_EngQ:
                if(pBufEnd - pBuf < 8) {	
                    return FALSE;
                }
				// if(g_Sys.u32LocalLang == CHINESE) {	/* 中文采用 万/亿进制 */
                if(1) {
                    u8Carry = PrintFM32Inter(&pBuf, fData, 0x3344, 0x24, 0x3334);   /* 通过参数3限制了fData的范围在9999亿度 */
                    /* 单位 */
                    if(u8Carry == 0) {
						PrintString(&pBuf, pBufEnd, "度");
					} else if(u8Carry == 1) {
						PrintString(&pBuf, pBufEnd, "万度");
					} else if(u8Carry == 2) {
						PrintString(&pBuf, pBufEnd, "亿度");
					}
				} else {	/* 否则采用K/M/G进制 */
					u8Carry = PrintFM32Inter(&pBuf, fData, 0x3444, 0x33, 0);    /* 通过参数3限制了fData的范围在999TWH */
					/* 单位 */
					if(u8Carry == 0) {
						*pBuf++ = 'K';
					} else if(u8Carry == 1) {
						*pBuf++ = 'M';
					} else if(u8Carry == 2) {
						*pBuf++ = 'G';
					} else if(u8Carry == 3) {
						*pBuf++ = 'T';
					}
					if(F32Meaning == F32_EngP) {
						*pBuf++ = 'W';
					} else {
						*pBuf++ = 'V';
					}
					*pBuf++ = 'H';
				}
				break;

    /* 最多8Byte: [0,10):a.bcOhm, [10,100):ab.cOhm, [100,1e3):  abcOhm, [1e3,1e4):a.bcKOhm, [1e4, 1e5):ab.cKOhm, [1e5, 1e6):abcKOhm, 
                  [1e6,1e7):a.bcMOhm, [1e7,1e8):ab.cMOhm, [1e8,1e9):abcMOhm, >=1e9:999MOhm */
            case F32_OHM:
                if(pBufEnd - pBuf < 8) {
                    return FALSE;
				}
				u8Carry = PrintFM32Inter(&pBuf, fData, 0x3334, 0x23, 0);    /* 通过参数3限制了fData的范围在999MOhm */
                if(u8Carry == 1) {
                    *pBuf++ = 'K';
                } else if(u8Carry == 2) {
                    *pBuf++ = 'M';
                }
                *pBuf++ = 'o';
                *pBuf++ = 'h';
                *pBuf++ = 'm';
				break;
				
                
            case F32_FlowRate:  /* 最多8byte：(-inf, -10]显示为 -abm3/s; (-10, 0)显示为-a.bm3/s; [0, 10)显示为a.bcm3/s; [10, inf)显示为 ab.cm3/s */
                if(pBufEnd - pBuf < 8) {
					return FALSE;
				}
                if(fData <= 0) {    /* 因为正数与负数输出的格式不一样,会出现'0.0'和'0.00'两种格式的'0',下面的判断是为了统一'0'的格式. */
                    if(fData > -0.05f) {    /* (-0.05, 0] 认为是 0.00 既然已经判断成功了直接输出即可,不必再调用函数.*/
                        *pBuf++ = '0';
                        *pBuf++ = '.';
                        *pBuf++ = '0';
                        *pBuf++ = '0';
                    } else {
                        PrintFM32Inter(&pBuf, fData, 0x2234, 0x02, 0);  /* 通过参数3限制了fData的范围在[-99, 0)m3/s */
                    }
                } else {
                    PrintFM32Inter(&pBuf, fData, 0x3334, 0x02, 0);      /* 通过参数3限制了fData的范围在[0, 99.9]m3/s */
                }
                /* 单位部分 */
				*pBuf++ = 'm';
				*pBuf++ = '3';
				*pBuf++ = '/';
				*pBuf++ = 's';
				break;

			case F32_FlowVlct:  /* 最多8byte：(-inf, 10]显示为 -ab.cm/s, (-10, 0)显示为-a.bcm/s, [0, 10)显示为a.bcdm/s, [10, inf)显示为 ab.cdm/s */
				if(pBufEnd - pBuf < 8) {
					return FALSE;
				}
                if(fData <= 0) {    /* 因为正数与负数输出的格式不一样,会出现'0.00'和'0.000'两种格式的'0',下面的判断是为了统一'0'的格式. */
                    if(fData > -0.005f) {   /* (-0.005, 0] 认为是 0.000. 既然已经判断成功了直接输出即可,不必再调用函数. */
                        *pBuf++ = '0';
                        *pBuf++ = '.';
                        *pBuf++ = '0';
                        *pBuf++ = '0';
                        *pBuf++ = '0';
                    } else {
                        PrintFM32Inter(&pBuf, fData, 0x3334, 0x02, 0);  /* 通过参数3限制了fData的范围在[-99.9, 0)m/s */
                    }
                } else {
                    PrintFM32Inter(&pBuf, fData, 0x4434, 0x02, 0);      /* 通过参数3限制了fData的范围在[0, 99.99]m/s */
                }
				/* 单位部分 */
				*pBuf++ = 'm';
				*pBuf++ = '/';
				*pBuf++ = 's';
				break;

			default:
				return FALSE;
		}
		*ppBuf = pBuf;
	}
	return TRUE;
}