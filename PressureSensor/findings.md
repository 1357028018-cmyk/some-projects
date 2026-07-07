# Findings & Decisions 鈥?STM32 绔皵娉礕PIO鎺у埗

## 纭欢璧勬簮

### USART1 鐘舵€?- 褰撳墠: 浠?TX锛堥樆濉炲彂閫佸帇鍔涘抚 + 闆疯揪甯у埌 PSoC锛夛紝RX 绌洪棽
- 鏀归€犲悗: TX 涓嶅彉 + RX 涓柇鎺ユ敹鍛戒护甯э紙HAL_UART_Receive_IT锛?- 寮曡剼: PA9(TX), PA10(RX)
- 閰嶇疆: 115200 8N1

### GPIO 鍒嗛厤
| 寮曡剼 | 鍔熻兘 | 鍒濆 | 璇存槑 |
|------|------|------|------|
| PB12 | 宸﹀厖姘旀车 | LOW | 鍘熷凡鍒濆鍖栦负 SET锛岄渶鏀?RESET |
| PB13 | 涓厖姘旀车 | LOW | 鍚屼笂 |
| PB14 | 鍙冲厖姘旀车 | LOW | 鎵嬪姩鏂板 |
| PB15 | 鍚告皵娉?| LOW | 鎵嬪姩鏂板 |
| PD8 | 鐢电闃€ | LOW | 鎵嬪姩鏂板; HIGH=鍚告皵, LOW=鍏呮皵 |

### 鍙傝€冨疄鐜?USART3 (PB10/PB11) 宸叉湁瀹屾暣鐨勪腑鏂帴鏀跺疄鐜?
- `MX_USART3_UART_Init()` 鈫?`HAL_UART_Receive_IT()` 
- `HAL_UART_RxCpltCallback()` 鈫?`R60_ParseByte()`
- USART1 鎸夌浉鍚屾ā寮忓疄鐜板嵆鍙?
## 閫氫俊鍗忚
```
PSoC 鈫?STM32 (4瀛楄妭鍥哄畾甯?:
  0xA5 + CMD(1B) + PARAM(1B) + 0x5A

CMD:
  0x01-0x03: 宸?涓?鍙冲厖姘旀车 ON (PARAM=鏃堕棿脳100ms)
  0x04:      鍚告皵娉?ON (PARAM=0=甯稿紑)
  0x05:      鐢电闃€鈫掑惛姘旈€氳矾(HIGH)
  0x06:      鐢电闃€鈫掑厖姘旈€氳矾(LOW)
  0x07:      鍏ㄩ儴 OFF锛堢揣鎬ュ仠姝級
  0x08-0x0B: 宸?涓?鍙?鍚告皵娉?OFF
```
