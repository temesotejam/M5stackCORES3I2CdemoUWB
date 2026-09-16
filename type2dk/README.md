# Type2DK I2C + USB UART diagnostic v2

QN9090用。Type2DK Rev.4.1での試験を想定した、実機未検証の診断版です。UWB測距・BLE・省電力処理なし。Flash/OTP/校正値を書き換える処理はありません。書き込み操作により既存アプリは置き換わります。

## ダウンロードと書き込み

[2dk_i2c_diag_v2.bin](https://temesotejam.github.io/M5stackCORES3I2CdemoUWB/firmware/2dk_i2c_diag_v2.bin)

Tera Termなどで2DKのCOMポートを開いている場合は閉じ、BINをDK6Programmer.exeと同じフォルダへ置いて実行します。COM22は例で、CoreS3のCOMと混同せず実際の2DKの番号へ変更してください。

```powershell
.\DK6Programmer.exe -V 0 -P 1000000 -s COM22 -Y -v -p .\2dk_i2c_diag_v2.bin
```

`-v`は書き込んだFlashの照合です。書き込み・照合・コマンド終了を確認後、2DKのUSBシリアルを **115200 bps / 8N1 / フロー制御なし** で開きます。以前の測距版の3000000 bpsとは異なります。通常は書き込み完了時にリセットされます。起動ログを取り直す場合は、ターミナルを開いてからQN9090側のMCU RESETを短く1回押します。Rev.4.1のボタン位置や穴番号は本資料では確定していません。

1. 最初は2DK単体でUSBログを確認できます。I2CをつながなくてもSTATEが出る構成です。
2. `STATE,v=2,...ready=1...`が出たらCoreS3とSDA/SCL/GNDを接続して両方を給電します。配線変更時は双方の電源を切ってください。
3. CoreS3でPROBEし、両方のログを採取します。

## ログの意味

以下は出力形式の説明で、実測結果ではありません。

- `BOOT,2DK_I2C_DIAG_V2,...`：アプリ本体へ到達してUARTを初期化した。
- `INIT,I2C1,...`：I2C初期化を開始する。
- `READY,I2C1_CONFIG_READBACK_OK`：ピン機能・I2C選択・スレーブ有効・アドレスのレジスタ読み戻しが一致。バス通信成功の意味ではない。
- `REG,...`：ピン設定、I2Cレジスタ、クロックゲート、リセット、I/O保持状態を起動時と5回ごとに出力。
- `STATE,v=2,beat=...,ready=...,irq=...,read_addr=...,write_addr=...,tx_bytes=...,write_bytes=...,deselect=...,stat=...,last_irq=...,irq_pending=...`：約1秒間隔の活動ログ。各カウンタは独立に読むため同一瞬間の値とは限らない。
- `FAULT,...`：UART初期化後に既定の例外ハンドラへ入った場合の例外番号・Faultレジスタ。これより前の停止は出力できない。

`beat`が増えればメインループは動いています。PROBEでは`write_addr`が増える想定。16バイトREADでは`read_addr`と`tx_bytes`が増える想定です。読み取り途中の中断でもread_addrは増え、tx_bytesはFIFOへの設定数なので、マスターが正しく受信した数そのものではありません。

ログが出ないだけでは「MCUが起動していない」と断定できません。COM番号、115200/8N1/フロー制御なし、Flash照合、UART経路も候補です。全レジスタの読み戻しが正常でも、配線・プルアップ・波形は別確認です。

## 接続と実装

- PIO12/SWCLK → SCL、PIO13/SWDIO → SDA、GND共通。
- 各信号を3.3 Vに外付けプルアップ。CoreS3 PORT Aの赤5 Vは接続しない。
- 2DKの既存PIO8/PIO9 USART0とFT230Xの経路からUSBログを出す。追加のUART線は不要。
- UARTは割り込みハンドラ内から出力しない。I2C割り込みを有効にしたままメインループで出す。
- 起動約2秒後にSWDピンをI2Cへ転用。既存SWD書き込み器は外す。
- v1と同じ0x42・16バイト2DKI・XOR形式。CoreS3 v1.0.0とも互換。
- v2は診断情報、周辺リセット完了待ち、soft-floatビルドを追加。根本原因の修正を実機で確認した版ではない。

## 再ビルド

元のNXP SDKヘッダーとZig 0.16.0が必要です。SDK本体は再配布しません。

```sh
python type2dk/build.py --zig /path/to/zig --sdk-root /path/to/uwbiot-top
```

リポジトリの `.bin.b64` はこのビルドのBINをBase64で保管したものです。公開処理で復号し、SHA256・ベクタチェックサム・ヘッダーCRC・サイズを照合してから配布します。GitHub ActionsではCoreS3を再ビルドし、2DKについては保存済みBINの検証と実際の応答ハンドラのホスト試験を行います。2DKをActions上で再ビルドする構成ではありません。
