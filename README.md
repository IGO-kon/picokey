# picokey (日本語)

[![build](https://github.com/IGO-kon/picokey/actions/workflows/build.yml/badge.svg)](https://github.com/IGO-kon/picokey/actions/workflows/build.yml)

- English README: [`README.en.md`](README.en.md)
- 作成者: [`trackmakerDJ`](https://x.com/trackmakerDJ)

Raspberry Pi Pico 2 W 用の BLE HID ブリッジファームウェアです。

```text
Bluetooth キーボード/タッチパッド
        -> (BLE HID over GATT)
Raspberry Pi Pico 2 W
        -> (USB HID Keyboard + Mouse + CDC log)
PC
```

現在は BTstack の `hog_host_demo` (Report Mode) をベースに、
独自フックでレポート解析とジェスチャ変換を行っています。

## できること

- BLE キーボード入力 -> USB キーボード出力
- タッチパッド座標 -> USB マウス出力
- 1本指タップ -> 左クリック
- 2本指タップ -> 右クリック
- 2本指の縦移動 -> スクロール
- タップ後ドラッグ (タップでアーム、次タッチでドラッグ)
- USB CDC 経由のペアリング/状態ログ (`/dev/ttyACM*`)
- Pico W LED ステータス:
  - ON: pairing/re-encryption 成功
  - OFF: 切断または pairing 失敗

## 動作確認ハードウェア

- 開発時に使用したキーボード/タッチパッド:
  - Amazon.co.jp: https://www.amazon.co.jp/dp/B0FT2DD1Z7

## 主要ファイル

- `CMakeLists.txt`
  - Pico SDK 設定
  - BTstack `hog_host_demo.c` 選択
  - GATT ヘッダ生成 (`hog_host_demo.gatt` -> `hog_host_demo.h`)
  - フック用シンボル差し替え (`hids_client_connect`, `sm_set_authentication_requirements`)
- `src/main.c`
  - 起動処理と BTstack run loop
- `src/picokey_hids_hooks.c`
  - レポート解析とポインタ/ジェスチャ調整の中核
- `src/picokey_sm_hooks.c`
  - ペアリング認証要件の補強
- `src/picokey_pairing_monitor.c`
  - CDC ログと LED 制御
- `src/picokey_usb_hid.c`
  - TinyUSB HID 送信キュー
- `src/usb_descriptors.c`
  - USB descriptor (CDC + HID keyboard + HID mouse)
- `src/tusb_config.h`
  - TinyUSB クラス/エンドポイント設定

## 環境構築

### 推奨 VS Code 拡張

- `ms-vscode.cpptools`
- `ms-vscode.cmake-tools`
- `twxs.cmake`
- `raspberry-pi.raspberry-pi-pico`
- `paulober.pico-w-go`

### 必要ツール

- `cmake` 3.13+
- ARM GNU ツールチェーン (`arm-none-eabi-gcc`)
- `python3` (BTstack GATT 生成用)
- `git`

Ubuntu/Debian 例:

```bash
sudo apt update
sudo apt install -y cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi python3 git
```

`arm-none-eabi-gcc` が `PATH` になくても、
`~/.arduino15/packages/rp2040/tools/pqt-gcc/*/bin` を自動検出します。

## ビルド

```bash
cmake -S . -B build -DPICO_BOARD=pico2_w
cmake --build build -j
```

生成物:

- `build/picokey.uf2`
- `build/picokey.elf`

キャッシュが壊れた場合の再作成:

```bash
rm -rf build
cmake -S . -B build -G "Unix Makefiles" -DPICO_BOARD=pico2_w
cmake --build build -j
```

## 書き込み

1. `BOOTSEL` を押しながら Pico 2 W を接続
2. `build/picokey.uf2` を `RPI-RP2` または `RP2350` にコピー
3. 自動で再起動

例:

```bash
cp build/picokey.uf2 /media/$USER/RP2350/
```

## ペアリング/デバッグログ

```bash
screen /dev/ttyACM0 115200
```

主なログ:

- `[PAIR] ...`
- `[HID] service connected ...`
- `[HID] tap-click`
- `[HID] two-finger-tap-right-click`

## 調整ガイド

挙動調整の多くは `src/picokey_hids_hooks.c` で行います。

### ポインタ速度

- `PICOKEY_TOUCHPAD_SPEED_NUMERATOR`
- `PICOKEY_TOUCHPAD_SPEED_DENOMINATOR`

`NUMERATOR / DENOMINATOR` を下げるとカーソル速度が遅くなります。

### 1本指タップ (左クリック)

- `PICOKEY_TAP_MAX_DURATION_MS`
- `PICOKEY_TAP_MAX_TOTAL_MOTION`

値を上げるとタップ判定が通りやすくなります。

### 2本指タップ (右クリック)

- `PICOKEY_TWO_FINGER_TAP_MAX_DURATION_MS`
- `PICOKEY_TWO_FINGER_TAP_MAX_TOTAL_MOTION`

値を上げると右クリック判定が通りやすくなります。

### 1本目 -> 2本目の時間ずれ許容

- `PICOKEY_TWO_FINGER_JOIN_MAX_DELAY_MS`
- `PICOKEY_TWO_FINGER_JOIN_MAX_TOTAL_MOTION`

「1本目が先、2本目が少し遅れて着地」するケースの許容設定です。

### タップ後ドラッグ

- `PICOKEY_DRAG_ARM_TIMEOUT_MS`

値を上げると、タップ後にドラッグ開始できる猶予が長くなります。

### 2本指スクロール感度

- `PICOKEY_SCROLL_DELTA_PER_STEP`

値を下げるとスクロールイベント発生頻度が上がります。

## 注意点

- `src/btstack_hooks.c` と `src/picokey_bt_hooks.h` は旧 boot-mode ルートの名残で、
  現在の CMake ターゲットでは使っていません。
- USB では HID と CDC を同時に公開しています。
- CMake Tools が曖昧な configure エラーを出す場合は、
  ターミナルで `cmake -S . -B build` を直接実行して詳細を確認してください。

## コミュニティ情報

- ライセンス: `LICENSE` (MIT)
- コントリビュート: `CONTRIBUTING.md`
- 行動規範: `CODE_OF_CONDUCT.md`
- セキュリティ報告: `SECURITY.md`
- Issue テンプレ: `.github/ISSUE_TEMPLATE/`
- PR テンプレ: `.github/PULL_REQUEST_TEMPLATE.md`

