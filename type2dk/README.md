# Type2DK I2C + USB UART diagnostic v4

QN9090用。Type2DK Rev.4.1での試験を想定した、実機未検証の診断版です。UWB測距・BLE・省電力処理なし。Flash/OTP/校正値を書き換える処理はありません。書き込み操作により既存アプリは置き換わります。

## v4で修正した不具合

**v1〜v3ではPIO12・PIO13のピン機能をFUNC4（PWM0/PWM2）に設定していました。正しくはFUNC5（I2C1_SCL/I2C1_SDA）です。旧版はI2C試験に使わず、v4へ更新してください。**

NXP公式の機能番号表を確認し、両ピンをFUNC5へ修正しました。これまでの`ready=1`は設定した誤った値の読み戻しにも成功するため、この間違いを検出できていませんでした。v4では実際のピン設定処理を、公式表で指定された機能番号と照合する回帰テストを追加しています。

[調査結果と一次資料](PIO12_PIO13_I2C_audit.md)。物理配線・400 kHz通信・USBログの実機確認は未完了です。

## ダウンロードと書き込み

[2dk_i2c_diag_v4.bin](https://temesotejam.github.io/M5stackCORES3I2CdemoUWB/firmware/2dk_i2c_diag_v4.bin)

Tera Termなどで2DKのCOMポートを開いている場合は閉じ、BINをDK6Programmer.exeと同じフォルダへ置いて実行します。COM22は例で、CoreS3のCOMと混同せず実際の2DKの番号へ変更してください。

```powershell
.\DK6Programmer.exe -V 0 -P 1000000 -s COM22 -Y -v -p .\2dk_i2c_diag_v4.bin
```

`-v`は書き込んだFlashの照合です。書き込み・照合・コマンド終了を確認後、2DKのUSBシリアルを **115200 bps / 8N1 / フロー制御なし** で開きます。以前の測距版の3000000 bpsとは異なります。通常は書き込み完了時にリセットされます。起動ログを取り直す場合は、ターミナルを開いてからQN9090側のMCU RESETを短く1回押します。Rev.4.1のボタン位置や穴番号は本資料では確定していません。

1. 最初は2DK単体でUSBログを確認できます。I2CをつながなくてもSTATEが出る構成です。
2. `STATE,v=4,...ready=1...`が出たらCoreS3とSDA/SCL/GNDを接続して両方を給電します。配線変更時は双方の電源を切ってください。
3. CoreS3でPROBEし、両方のログを採取します。

## 今回の「2DKログなし / CoreS3両線Low」の確認

1. 双方の電源を切り、機器間のSDA・SCL・GNDとSWD書き込み器を外します。2DKだけをUSBでPCにつなぎます。
2. v4を上記コマンドで書き込み、照合成功を確認します。
3. 2DKのCOMを115200 bps / 8N1 / フロー制御なしで開きます。`STATE,v=4`が定期的に出るか確認します。
4. 別途CoreS3のPORT Aを未接続にして再起動し、最初の5秒間の`DIAG`の`idle`を確認します。内部プルアップを有効にしているため、外部接続がなければ1/1が期待値です。これは400 kHzで外付け抵抗が不要という意味ではありません。

2DK単体でもログが出ない場合はDK6Programmerの書き込み・照合結果を確認します。CoreS3単体でも0/0の場合は、2DK以外にもGPIO設定・基板・計測を調べる必要があります。

## ログの意味

以下は出力形式の説明で、実測結果ではありません。

- `BOOT,2DK_I2C_DIAG_V4,...`：アプリ本体へ到達してUARTを初期化した。
- `INIT,I2C1,...`：I2C初期化を開始する。
- `READY,I2C1_CONFIG_READBACK_OK`：ピン機能・I2C選択・スレーブ有効・アドレスのレジスタ読み戻しが一致。バス通信成功の意味ではない。
- `REG,...`：ピン設定、I2Cレジスタ、クロックゲート、リセット、I/O保持状態、APBブリッジ・クロック、UART設定を起動時と5回ごとに出力。
- `STATE,v=4,beat=...,ready=...,irq=...,read_addr=...,write_addr=...,tx_bytes=...,write_bytes=...,deselect=...,stat=...,last_irq=...,irq_pending=...`：約1秒間隔の活動ログ。各カウンタは独立に読むため同一瞬間の値とは限らない。
- `FAULT,...`：UART初期化後に既定の例外ハンドラへ入った場合の例外番号・Faultレジスタ。これより前の停止は出力できない。

`beat`が増えればメインループは動いています。PROBEでは`write_addr`が増える想定。16バイトREADでは`read_addr`と`tx_bytes`が増える想定です。読み取り途中の中断でもread_addrは増え、tx_bytesはFIFOへの設定数なので、マスターが正しく受信した数そのものではありません。

ログが出ないだけでは「MCUが起動していない」と断定できません。COM番号、115200/8N1/フロー制御なし、Flash照合、UART経路も候補です。全レジスタの読み戻しが正常でも、配線・プルアップ・波形は別確認です。

## 接続と実装

- PIO12/SWCLK → SCL、PIO13/SWDIO → SDA、GND共通。両方の機能番号はFUNC5。
- v4の`REG`で`pio12=0x00000595,pio13=0x00000595`が期待値です。旧版の`0x00000594`はPWM選択です。
- 各信号を3.3 Vに外付けプルアップ。CoreS3 PORT Aの赤5 Vは接続しない。
- 2DKの既存PIO8/PIO9 USART0とFT230Xの経路からUSBログを出す。追加のUART線は不要。
- UARTは割り込みハンドラ内から出力しない。I2C割り込みを有効にしたままメインループで出す。
- 起動約2秒後にSWDピンをI2Cへ転用。既存SWD書き込み器は外す。
- v1と同じ0x42・16バイト2DKI・XOR形式。CoreS3 v1.0.0とも互換。
- v3から引き継ぐ起動処理：SDKの `BOARD_BootClockRUN` / `CLOCK_EnableAPBBridge` と照合して、UARTにアクセスする前の非同期APBブリッジ有効化を追加。FRO32Mも選択前に明示的に有効化します。このクロック修正だけでは、今回特定したFUNC4の誤りは解消されていませんでした。
- CoreS3は既存の1.1.0のままで確認できます。

## 再ビルド

元のNXP SDKヘッダーとZig 0.16.0が必要です。SDK本体は再配布しません。

```sh
python type2dk/build.py --zig /path/to/zig --sdk-root /path/to/uwbiot-top
```

リポジトリの `.bin.b64` はこのビルドのBINをBase64で保管したものです。公開処理で復号し、SHA256・ベクタチェックサム・ヘッダーCRC・サイズを照合してから配布します。GitHub ActionsではCoreS3を再ビルドし、2DKについては保存済みBINの検証と実際の応答ハンドラ・ピン設定処理のホスト試験を行います。2DKをActions上で再ビルドする構成ではありません。
