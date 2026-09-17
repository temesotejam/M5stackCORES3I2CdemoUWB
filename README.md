# CoreS3 × Type2DK I2C Check

[ブラウザ書き込みページ](https://temesotejam.github.io/M5stackCORES3I2CdemoUWB/) · [ビルドと公開状況](https://github.com/temesotejam/M5stackCORES3I2CdemoUWB/actions)

M5Stack CoreS3から、Type2DK診断版v5スレーブの16バイト応答を読み取る試験アプリです。測距・距離表示は行いません。ハードウェアでの動作は利用環境で確認してください。

## 書き込み

PC版Chrome / Edgeで上のページを開き、CoreS3をUSB接続して書き込みます。現在のアプリは置き換わります。Erase deviceは保存データも消去します。接続できなければRESETを約3秒長押しし、緑LED点灯後に離して再接続。完了後はRESETを短く押します。このページはCoreS3用です。2DKは従来のUSB書き込み手順を使います。

## 配線

|CoreS3 PORT A|Type2DK EVK Rev.4.1・QN9090用TP8|
|---|---|
|黄 GPIO2 SDA|TP8の2番：PIO13 SWDIO → I2C1 SDA|
|白 GPIO1 SCL|TP8の4番：PIO12 SWCLK → I2C1 SCL|
|黒 GND|TP8の3・5・9番のいずれか：GND|
|赤 5 V|接続しない|

Rev.4.1回路図の1ページで照合済み。TP14はSR040用で、今回使う端子ではありません。実物の1番表示を確認し、回路図の左右を基板上の向きとして扱わないでください。

両方を個別にUSB給電。PIO12/13の外付けプルアップは基板回路図にありません。SDA/SCL各線を、電圧を確認した2DKのMCU I/O電源（TP18）へ各2～2.2 kΩでプルアップする構成から試します。最終値は並列抵抗・バス容量・波形で確認。5 Vへは接続しません。2DKのSWD書き込み器は外します。

USB給電経路にはダイオードD1があり、MCU電源を3.3 V固定と扱えません。**TP18（VDD_3V3_MCU）対TP15（GND）の電圧を確認**してください。FT230XはUSBから直接給電されるため、COMポートの出現だけではMCU給電を確認できません。[Rev.4.1の電源・UART・配線の確認](type2dk/README.md#rev41回路図で確認した接続)。

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

## 診断版 v1.1.0

外部PORT AのみをEspressif ESP-IDFのI2Cドライバへ変更。M5Unifiedの内部バスと別コントローラであることを検査してから外部バスを引き継ぎます。これにより旧版のbool型にまとめられていたエラーを数値で取得します。旧版との通信実装比較も兼ねる診断変更で、実機での解決は未確認です。16バイト形式はv1互換です。2DKのUSBログが必要な場合は下記の診断版v5へ書き換えます。

起動5秒後に自動PROBE、毎秒STATとDIAGを出力。USBで `p` を送信すると再PROBEできます。PROBEは書き込みアドレスのみ送ってSTOPし、レジスタやデータを書き込みません。

- `NO_ACK` / `ESP_FAIL`：スレーブのACKを受け取れなかった。正常な生存・配線・電気条件を保証しない。
- `TIMEOUT`：バスが時間内に完了しなかった。信号線Low、スレーブの応答待ち、ドライバなどを切り分ける。
- `BAD_FRAME`：I2C転送は完了したが2DKIの内容に不一致。
- `INIT_FAIL`：外部バスの初期化失敗。`init`でドライバのエラー名を確認。
- `before / after / idle`：SDA/SCLのデジタル値。1/1でも外部プルアップの有無・電圧・波形・接続先を証明できない。
- `nack / timeout / other`：速度別の読み取り失敗数。PROBEはSTATに含めない。

100 kHzで約10秒動かし、PROBEとDIAGの行を採取してください。内部の弱いプルアップも有効ですが400 kHz試験の外付け抵抗を代用する前提にはしません。

[ESP-IDF v4.4.7 I2C API](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/peripherals/i2c.html)

2DK側のUSBログと入力観測を追加した診断版v5を同梱しました。[書き込みとログの説明](type2dk/README.md)を参照してください。

### 2DK v3：USBログが出ない場合の起動処理修正

UART初期化前に非同期APBブリッジとクロックを設定する処理を追加しました。SDK標準の起動処理との差分に基づく修正で、実機での解消は未確認です。CoreS3 1.1.0とは互換で、CoreS3の再書き込みは不要です。[2DK単体での確認手順](type2dk/README.md)で先にUSBログを確認してください。

CoreS3 1.1.0の`read=WAIT`時の`before=1/1,after=1/1`は初期値で、測定結果ではありません。未通信時の線状態は`idle`を見ます。`maxUs`が約1000000の場合は約1秒の待機であり、受信成功や転送速度を示しません。

### 2DK v4：PIO12/13の機能番号を訂正

v1〜v3はFUNC4（PWM0/PWM2）を選択する不具合がありました。正しいI2C1の機能番号はFUNC5です。v4は両ピンの設定を修正し、実際のピン設定処理をNXPの機能番号表と照合するテストを追加しました。現在はこの修正を引き継いだv5を使用してください。CoreS3 1.1.0の変更は不要です。

### 2DK v5：実機ログを受けて入力観測を追加

v4の実機ログでアプリ実行・UART・FUNC5を確認済み。PSELID=0によりready=0になるが、v4はこの値でI2Cを停止していません。v5はSCL/SDAを3秒間GPIO入力として観測した後、FUNC5へ戻します。CHECKの個別判定と割り込み関連レジスタを追加し、信号到達とI2C内部の問題を切り分けます。ACK/データ通信成功はまだ未確認です。

[PIO12・PIO13の詳しい調査結果と一次資料](type2dk/PIO12_PIO13_I2C_audit.md)。ハードウェアの対応と、実機の通信成功は分けて判断します。
