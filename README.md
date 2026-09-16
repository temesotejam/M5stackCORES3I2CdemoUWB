# CoreS3 × Type2DK I2C Check

[ブラウザ書き込みページ](https://temesotejam.github.io/M5stackCORES3I2CdemoUWB/) · [ビルドと公開状況](https://github.com/temesotejam/M5stackCORES3I2CdemoUWB/actions)

M5Stack CoreS3から、別途配布した `Type2DK_I2C_Test_v1` スレーブの16バイト応答を読み取る試験アプリです。測距・距離表示は行いません。ハードウェアでの動作は利用環境で確認してください。

## 書き込み

PC版Chrome / Edgeで上のページを開き、CoreS3をUSB接続して書き込みます。現在のアプリは置き換わります。Erase deviceは保存データも消去します。接続できなければRESETを約3秒長押しし、緑LED点灯後に離して再接続。完了後はRESETを短く押します。このページはCoreS3用です。2DKは従来のUSB書き込み手順を使います。

## 配線

|CoreS3 PORT A|Type2DK|
|---|---|
|黄 GPIO2 SDA|PIO13 SWDIO → I2C1 SDA|
|白 GPIO1 SCL|PIO12 SWCLK → I2C1 SCL|
|黒 GND|GND|
|赤 5 V|接続しない|

両方を個別にUSB給電し、3.3 VのI/O条件で使用。SDA/SCL各線を3.3 Vにプルアップします。短い配線では2.2～4.7 kΩを出発点として、既存抵抗・バス容量と実際の波形を確認してください。5 Vへプルアップしないでください。2DKのSWD書き込み器は外します。PIO番号は信号名であり評価基板コネクタの穴番号ではありません。

## 操作と合否

起動約5秒後、100 kHz / 最大約50回毎秒で読み取り開始。SCLクロックを作るのはCoreS3です。100 / 400 kHzボタンで切替。内部タッチ等のI2CはM5Unifiedが別バスで管理します。

- **OK** 正しい応答数 / **IO** 通信失敗数 / **BAD** 形式・XOR不一致数 / **GAP** 連番の不連続数
- **SEQ** スレーブ連番 / **READ・MAX** ソフトウェアで測った通信処理時間。SCL周波数の実測ではありません。
- **AUTO**：100 kHzで500回、400 kHzで500回。各速度が全件正常かつエラー・連番不連続0ならPASS。通常約20秒。試験中に電源操作・他マスターのアクセスをしないでください。
- **PROBE**：0x42のアドレスACKのみ確認。結果はUSBログ（受信前なら画面にも表示）。
- **PAUSE / RUN**：停止・再開。**CLEAR**：集計リセット。

通信失敗後は連番比較の基準を取り直します。速度手動切替・再開時も同様です。自動試験中の通信失敗はIOに残るためPASSにはなりません。AUTO PASSは限られた回数の通信確認であり、波形・電気特性やUWB同時動作を保証しません。

USBシリアル115200 bps、1秒ごと：

```text
STAT,clock,attempts,ok,io,bad,gaps,sequence,maxUs
PROBE,clock,ACK または NO ACK
```

## 2DKテストプロトコル

7ビットアドレス `0x42`（Wire等へ0x84を指定しない）。レジスタ書き込みなしで16バイトを直接READし、最後のバイトをNACKしてSTOP。

|オフセット|内容|
|---|---|
|0–3|ASCII `2DKI`|
|4–7|`01 42 A5 5A`|
|8–11|readアドレス受付回数、uint32 little endian|
|12–14|`12 34 56`|
|15|0–14のXOR|

## ローカルビルド

```sh
python -m pip install platformio==6.1.18
python -m platformio run
python -m platformio run -t upload
```

CoreS3、espressif32 6.12.0、M5Unified 0.2.22、M5GFX 0.2.29に固定。新しいCoreS3のLCD対応のためM5GFXを古い版へ下げないでください。

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude test/protocol_test.cpp -o /tmp/protocol_test
/tmp/protocol_test
python scripts/package_firmware.py --sha local
```

`site/`にESP32-S3用統合BIN、ESP Web Tools manifest、SHA256、公開ページを生成します。統合BINはオフセット0へ書き込み。結合位置はbootloader 0、partitions 0x8000、boot_app0 0xe000、application 0x10000です。CoreS3の既存アプリを書き換えます。

## GitHub Pages

mainへのpushでフレーム検証・ファームウェアビルド・成果物保存・Pages公開を実行します。初回にPages自動有効化が権限で失敗した場合は、リポジトリSettings → Pages → Build and deployment → Sourceを **GitHub Actions** に設定してActionsを再実行してください。公開前でもActionsの `cores3-firmware-and-installer` artifactをダウンロードできます。

## 参照

- [M5Stack CoreS3公式仕様・配線・ダウンロードモード](https://docs.m5stack.com/en/core/CoreS3)
- [M5Unified I2C API](https://github.com/m5stack/M5Unified/blob/master/src/utility/I2C_Class.hpp)
- [ESP Web Tools](https://esphome.github.io/esp-web-tools/)
