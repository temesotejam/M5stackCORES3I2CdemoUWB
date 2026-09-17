# Type2DK I2C + USB UART diagnostic v5

QN9090用。Type2DK Rev.4.1ではv4のアプリ実行・USBログまでユーザーの実機ログで確認済みです。その後のユーザー提供ログでv5の起動・ACK・100 kHz設定の追加1000回すべて正常受信を確認しました。400 kHzとCoreS3 v1.1.1の電源配線変更後の通信は未確認です。UWB測距・BLE・省電力処理なし。Flash/OTP/校正値を書き換える処理はありません。書き込み操作により既存アプリは置き換わります。

## v5：信号到達とI2C処理を分けて調べる診断版

提供されたv4ログでは、`beat`増加、PIO12/13=`0x595`、CFG=`2`、SLVADR0=`0x84`、I2C1クロックゲート有効、I2C1リセット解除を確認。`ready=0`になる直接の理由は、期待値3に対して`PSELID=0`だったことです。

**v4はready=0でもI2Cを停止しません。readyの表示だけ直してもNO_ACKが直る根拠はありません。** `QN9090.h`の`I2C_Type`ではオフセット0xFF8が予約領域で、0xFFCがIDです。一方、同梱の汎用FLEXCOMMドライバはPSELIDを操作します。今回の資料とログだけでPSELID=0を「I2Cが使えない原因」と断定しません。v5ではv4のready条件を比較用に残し、CHECKで各項目を別々に表示します。

v5は切り分けに必要な観測を追加したBINです。公開後のユーザー提供ログで100 kHz受信を確認しました。アドレス・応答・FUNC5・I2C設定・割り込みハンドラはv4から維持します。

1. CoreS3はv1.1.1を使い、100 kHz / RUNで動かします。2 kΩの抵抗値は維持し、電源側を実測3.0 VのTP18へ接続します（SDA/SCLに各1本）。CoreS3の3V3からは外し、電源同士を直接つながないでください。v1.1.1ではPORT Aの内蔵プルアップを無効化しています。
2. 下のv5を書き込み、2DKのCOMを115200 bps / 8N1 / フロー制御なしで開き、SW1を1回押します。
3. 起動から約2秒後、PIO12/13を一時的に**出力しないGPIO入力**として3秒間観測します。この間のNO_ACKは予定された動作です。
4. 続いてFUNC5のI2C1スレーブへ切り替えます。`LINES,mode=GPIO_INPUT`の3行、最初の`CHECK`・`REG`、その後の`STATE`・`LINES,mode=I2C1_FUNC5`を採取します。
5. 通信が成立したら、起動中の失敗をCLEARしてから100 kHz、次に400 kHzを試します。

| 観測 | 次に絞り込めること |
|---|---|
| GPIO_INPUTでscl_changesとsda_changesが増える | 少なくとも両ピンの電圧変化を2DK側でも検出。正しいI2C波形・アドレスを保証するものではない |
| CoreS3がRUNでもGPIO_INPUTの変化が両方0 | 停止中・配線違い・導通・信号レベルを優先確認。ポーリングでの未検出なので断線とは断定しない |
| GPIO入力で変化があり、I2Cではread_addr/write_addr/irqが0 | FUNC5入力経路・I2C設定・実際のアドレス波形の調査へ |
| STATのSLVPENDINGが立つのにirqが0 | INTENSET、NVIC、PRIMASK/BASEPRI/FAULTMASK、ベクタを確認 |
| CoreS3のOKと2DKのread_addr/tx_bytesが増える | フレーム受信が成立。400 kHzと波形の確認へ |

`LINES`のchangesはポーリングで見えた状態変化数です。全エッジ数、SCL周波数、立ち上がり時間、I2Cデコードではありません。GPIO_INPUTとI2C1_FUNC5で入力経路が異なる可能性もあるため、まずGPIO_INPUTの結果を見ます。各ログ区間でカウンタをリセットします。

## v4で修正した不具合

**v1〜v3ではPIO12・PIO13のピン機能をFUNC4（PWM0/PWM2）に設定していました。正しくはFUNC5（I2C1_SCL/I2C1_SDA）です。旧版はI2C試験に使わず、FUNC5を引き継いだv5へ更新してください。**

NXP公式の機能番号表を確認し、両ピンをFUNC5へ修正しました。これまでの`ready=1`は設定した誤った値の読み戻しにも成功するため、この間違いを検出できていませんでした。v4では実際のピン設定処理を、公式表で指定された機能番号と照合する回帰テストを追加しています。

[調査結果と一次資料](PIO12_PIO13_I2C_audit.md)。v4のUSBログとv5の100 kHz設定でのACK・データ受信は確認済み。400 kHz通信は未確認です。

## ダウンロードと書き込み

[2dk_i2c_diag_v5.bin](https://temesotejam.github.io/M5stackCORES3I2CdemoUWB/firmware/2dk_i2c_diag_v5.bin)

Tera Termなどで2DKのCOMポートを開いている場合は閉じ、BINをDK6Programmer.exeと同じフォルダへ置いて実行します。COM22は例で、CoreS3のCOMと混同せず実際の2DKの番号へ変更してください。

```powershell
.\DK6Programmer.exe -V 0 -P 1000000 -s COM22 -Y -v -p .\2dk_i2c_diag_v5.bin
```

`-v`は書き込んだFlashの照合です。書き込み・照合・コマンド終了を確認後、2DKのUSBシリアルを **115200 bps / 8N1 / フロー制御なし** で開きます。以前の測距版の3000000 bpsとは異なります。通常は書き込み完了時にリセットされます。起動ログを取り直す場合は、ターミナルを開いてからQN9090側のMCU RESETを短く1回押します。Rev.4.1回路図での部品番号はSW1です。基板上の位置は実物の表示で確認してください。

1. 最初は2DK単体でUSBログを確認できます。I2CをつながなくてもSTATEが出る構成です。
2. 上記の観測を行う場合はCoreS3を100 kHz / RUNにして2DKをリセットします。配線変更時は双方の電源を切ってください。readyだけで起動失敗と判断せずCHECKの項目を見ます。
3. CoreS3でPROBEし、両方のログを採取します。

## 過去の「2DKログなし / CoreS3両線Low」の確認（現在はUARTログあり）

1. 双方の電源を切り、機器間のSDA・SCL・GNDとSWD書き込み器を外します。2DKだけをUSBでPCにつなぎます。
2. v5を上記コマンドで書き込み、照合成功を確認します。
3. 2DKのCOMを115200 bps / 8N1 / フロー制御なしで開きます。`STATE,v=5`が定期的に出るか確認します。
4. CoreS3 v1.1.0では内蔵プルアップが有効のためPORT A未接続時にidle=1/1を期待していました。**v1.1.1では無効にしたため、未接続時の値は不定です。** 現在は電圧を合わせた外付けプルアップを正しく接続し、電圧と通信結果を確認します。

2DK単体でもログが出ない場合はDK6Programmerの書き込み・照合結果を確認します。v1.1.1ではCoreS3単体・外付け抵抗なしの0/0だけを異常と判断しません。

## ログの意味

以下はv5の出力形式の説明で、実測結果ではありません。

- `BOOT,2DK_I2C_DIAG_V5,...`：アプリ本体へ到達してUARTを初期化した。
- `INIT,I2C1,...`：I2C初期化を開始する。
- `CHECK,...`：pins/slave/address/clock_gate/reset_released/irq_enable/nvic_enable/unmasked/psel_matchを個別に表示。psel_match以外は1を期待。psel_matchの必須性は未確定。
- `INFO,ready_uses_v4_checks,PSELID_expectation_unconfirmed`：readyはv4のPSELID判定を含む比較用の値。ready=0だけを通信停止の原因と扱わない。
- `LINES,...`：2DK側のSCL/SDAの値、Lowを観測した回数、観測できた変化数。
- `REG,...`：ピン設定、I2Cレジスタ、クロックゲート、リセット、I/O保持状態、APBブリッジ・クロック、UART設定を起動時と5回ごとに出力。
- `STATE,v=5,beat=...,ready=...,irq=...,read_addr=...,write_addr=...,tx_bytes=...,write_bytes=...,deselect=...,stat=...,last_irq=...,irq_pending=...`：約1秒間隔の活動ログ。各カウンタは独立に読むため同一瞬間の値とは限らない。
- `FAULT,...`：UART初期化後に既定の例外ハンドラへ入った場合の例外番号・Faultレジスタ。これより前の停止は出力できない。

`beat`が増えればメインループは動いています。PROBEでは`write_addr`が増える想定。16バイトREADでは`read_addr`と`tx_bytes`が増える想定です。読み取り途中の中断でもread_addrは増え、tx_bytesは送信データレジスタへの設定数なので、マスターが正しく受信した数そのものではありません。

ログが出ないだけでは「MCUが起動していない」と断定できません。COM番号、115200/8N1/フロー制御なし、Flash照合、UART経路も候補です。全レジスタの読み戻しが正常でも、配線・プルアップ・波形は別確認です。

## 接続と実装

- PIO12/SWCLK → SCL、PIO13/SWDIO → SDA、GND共通。両方の機能番号はFUNC5。
- v4の`REG`で`pio12=0x00000595,pio13=0x00000595`が期待値です。旧版の`0x00000594`はPWM選択です。
- 各信号を、電圧を確認した2DKのMCU I/O電源（TP18）に外付けプルアップ。CoreS3 PORT Aの赤5 Vは接続しない。電源経路にD1があるため、USB給電でもMCU電圧を3.3 V固定と扱わない。
- 2DKの既存PIO8/PIO9 USART0とFT230Xの経路からUSBログを出す。追加のUART線は不要。
- UARTは割り込みハンドラ内から出力しない。I2C割り込みを有効にしたままメインループで出す。
- 起動約2秒後にSWDピンをGPIO入力へ切替え、3秒観測後にI2Cへ転用。既存SWD書き込み器は外す。
- v1と同じ0x42・16バイト2DKI・XOR形式。CoreS3 v1.0.0とも互換。
- v3から引き継ぐ起動処理：SDKの `BOARD_BootClockRUN` / `CLOCK_EnableAPBBridge` と照合して、UARTにアクセスする前の非同期APBブリッジ有効化を追加。FRO32Mも選択前に明示的に有効化します。このクロック修正だけでは、今回特定したFUNC4の誤りは解消されていませんでした。
- 応答形式はCoreS3 1.1.0と共通ですが、実測3.0 VのTP18への外付けプルアップ構成では、内蔵プルアップを無効にしたCoreS3 1.1.1を使います。2DK v5のBINは変更していません。

## Rev.4.1回路図で確認した接続

出典：ユーザー提供 `MTD_SCH_011-E-2DK-EVK-Rev4.1_Schematic.pdf`。4ページに2024.2.14 Rev.4.1の改訂記録がある。回路図本体は再配布しない。

| 用途 | 接続先 | 信号 |
|---|---|---|
| CoreS3 GPIO2 SDA | TP8の2番 | PIO13/SWDIO_QN → モジュール55番 |
| CoreS3 GPIO1 SCL | TP8の4番 | PIO12/SWCLK_QN → モジュール54番 |
| CoreS3 GND | TP8の3・5・9番のいずれか | GND |
| MCU電源測定・プルアップ先 | TP18 | VDD_3V3_MCU |
| 測定GND | TP15 | GND |

TP8の1番はVDD_3V3_EVBで、MCU電源とは異なる分岐。7・8番は図上未接続、6番はPIO21/SWO/赤LED、10番はSWD_RESETN。TP14はSR040用SWD。PIO12/13はTP8へ直接配線され、バッファ・直列抵抗・外付けプルアップ・他ICへの分岐は図示されていない。R1/R2はPIO10/11用で、10 kΩ・DNIと記載され、今回のI2C1には接続されていない。

電源経路は、USB → 3.3 V LDO → D1 → SW2 → VDD_3V3 → TP11/TP18間の接続 → MCU。R21は0 Ω・DNIなので、実機のジャンパ等の導通状態を確認する。外部測定配線などの可能性もあるため、回路図だけからジャンパを追加・短絡しない。まずTP11とTP18の電圧をTP15基準で比較する。SW2で電池側を選択した場合は、USBが接続されていてもMCU電源はUSB側から供給されない。

USB変換IC FT230XのVCCはVCC_USBに接続され、MCU電源と別経路。USB認識済みでもMCUが無給電という状態は回路上あり得る。これは現在の不具合の原因を断定するものではない。

UARTはPIO8 → FT230X RXD、FT230X TXD → PIO9。USB書き込み時の制御経路としてCBUS2 → R4 → SWD_RESETN、CBUS3 → R8 → PIO5が接続されている。**TP1/TP2はSR040のUART、TP4/TP5はFT230XのCBUS0/1で、QN9090のTX/RXではない。** I2CへPIO12/13を転用しても、PIO8/9のUART経路は別に残る。

## 再ビルド

元のNXP SDKヘッダーとZig 0.16.0が必要です。SDK本体は再配布しません。

```sh
python type2dk/build.py --zig /path/to/zig --sdk-root /path/to/uwbiot-top
```

リポジトリの `.bin.b64` はこのビルドのBINをBase64で保管したものです。公開処理で復号し、SHA256・ベクタチェックサム・ヘッダーCRC・サイズを照合してから配布します。GitHub ActionsではCoreS3を再ビルドし、2DKについては保存済みBINの検証と実際の応答ハンドラ・ピン設定処理のホスト試験を行います。2DKをActions上で再ビルドする構成ではありません。
