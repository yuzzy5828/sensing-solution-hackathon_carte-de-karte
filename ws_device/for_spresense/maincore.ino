#include <MP.h>
// #include <micro_ros_arduino.h>  // micro-rosは使用しないのでコメントアウト
#include <stdio.h>
// #include <rcl/rcl.h>
// #include <rcl/error_handling.h>
// #include <rclc/rclc.h>
// #include <rclc/executor.h>
// #include <std_msgs/msg/int32.h>

#include <SDHCI.h>
#include <Audio.h>

#include <arch/board/board.h>

#include <EltresAddonBoard.h>

// MPの設定
int8_t msgid;
uint32_t msgdata;

// Audioの定義
#define RECORD_FILE_NAME "Sound.wav"

SDClass theSD;
AudioClass *theAudio;
File audioFile;

bool ErrEnd = false;
bool startRecording = false;

static const uint32_t recording_sampling_rate = 48000;
static const uint8_t recording_channel_number = 1;
static const uint8_t recording_bit_length = 16;
static const uint32_t recording_time = 1.5; // 録音時間を1.5秒に設定
static const int32_t recording_byte_per_second = recording_sampling_rate *
                                                 recording_channel_number *
                                                 recording_bit_length / 8;
static const int32_t recording_size = recording_byte_per_second * recording_time;

// ELTRES用の定義
#define LED_RUN PIN_LED0
#define LED_GNSS PIN_LED1
#define LED_SND PIN_LED2
#define LED_ERR PIN_LED3

// プログラム内部状態
#define PROGRAM_STS_INIT      (0)
#define PROGRAM_STS_RUNNING   (1)
#define PROGRAM_STS_STOPPING  (2)
#define PROGRAM_STS_STOPPED   (3)

int program_sts = PROGRAM_STS_INIT;
bool gnss_recevie_timeout = false;
uint64_t last_change_blink_time = 0;
bool event_send_ready = false;
uint8_t payload[16];
eltres_board_gga_info last_gga_info;

// エラーハンドリング用関数
void error_loop() {
  while (1) {
    digitalWrite(LED_ERR, !digitalRead(LED_ERR));
    delay(100);
  }
}

void error_loop_soft() {
  while (1) {
    digitalWrite(LED_ERR, !digitalRead(LED_ERR));
    delay(500);
  }
}

// Audioエラーのコールバック関数
static void audio_attention_cb(const ErrorAttentionParam *atprm) {
  Serial.println("Attention!");
  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING) {
    ErrEnd = true;
  }
}

// Audioの初期化関数
void init_audio() {
  // SDカードの初期化
  while (!theSD.begin()) {
    Serial.println("SDカードを挿入してください。");
    delay(1000);
  }

  theAudio = AudioClass::getInstance();

  int ret = theAudio->begin(audio_attention_cb);
  if (ret != AUDIOLIB_ECODE_OK) {
    Serial.println("Audioライブラリの初期化に失敗しました。");
    error_loop();
  }

  Serial.println("Audioライブラリの初期化完了");
}

// 録音処理関数
void start_audio_recording() {
  // 一秒前からLED_RUNを点灯
  Serial.println("録音開始1秒前...");
  digitalWrite(LED_RUN, HIGH);
  delay(1000); // 一秒待機

  // 録音の設定（マイク入力、サンプリングレート、ビット深度、チャンネル数）
  int ret = theAudio->setRecorderMode(AS_SETRECDR_STS_INPUTDEVICE_MIC);
  if (ret != AUDIOLIB_ECODE_OK) {
    Serial.println("録音モードの設定に失敗しました。");
    error_loop();
  }

  ret = theAudio->initRecorder(AS_CODECTYPE_WAV,
                               "/mnt/sd0/BIN",
                               recording_sampling_rate,
                               recording_bit_length,
                               recording_channel_number);
  if (ret != AUDIOLIB_ECODE_OK) {
    Serial.println("Recorderの初期化に失敗しました。");
    error_loop();
  }

  // 録音ファイルの準備
  if (theSD.exists(RECORD_FILE_NAME)) {
    theSD.remove(RECORD_FILE_NAME);
  }
  audioFile = theSD.open(RECORD_FILE_NAME, FILE_WRITE);

  if (!audioFile) {
    Serial.println("ファイルのオープンに失敗しました。");
    error_loop();
  }

  // WAVヘッダーの書き込み
  theAudio->writeWavHeader(audioFile);

  // 録音の開始
  ret = theAudio->startRecorder();
  if (ret != AUDIOLIB_ECODE_OK) {
    Serial.println("録音の開始に失敗しました。");
    error_loop();
  }

  Serial.println("録音を開始しました。");

  uint32_t startTime = millis();

  while ((millis() - startTime) < (recording_time * 1000)) {
    err_t err = theAudio->readFrames(audioFile);

    if (err != AUDIOLIB_ECODE_OK) {
      Serial.println("録音中にエラーが発生しました。");
      theAudio->stopRecorder();
      break;
    }

    if (ErrEnd) {
      Serial.println("エラーにより録音を終了します。");
      theAudio->stopRecorder();
      break;
    }

    delay(10);
  }

  theAudio->stopRecorder();
  Serial.println("録音を停止しました。");

  // 録音ファイルのクローズ
  theAudio->closeOutputFile(audioFile);
  audioFile.close();

  // オーディオを待機状態に設定
  ret = theAudio->setReadyMode();
  if (ret != AUDIOLIB_ECODE_OK) {
    Serial.println("Readyモードへの設定に失敗しました。");
    error_loop();
  }

  Serial.println("録音を終了しました。");

  // 録音終了後にLED_RUNを消灯
  digitalWrite(LED_RUN, LOW);

  startRecording = false; // フラグをリセット
}

// ELTRESイベントコールバック
void eltres_event_cb(eltres_board_event event) {
  switch (event) {
  case ELTRES_BOARD_EVT_GNSS_TMOUT:
    Serial.println("gnss wait timeout error.");
    gnss_recevie_timeout = true;
    break;
  case ELTRES_BOARD_EVT_IDLE:
    Serial.println("waiting sending timings.");
    digitalWrite(LED_SND, LOW);
    break;
  case ELTRES_BOARD_EVT_SEND_READY:
    Serial.println("Shortly before sending, so setup payload if need.");
    event_send_ready = true;
    break;
  case ELTRES_BOARD_EVT_SENDING:
    Serial.println("start sending.");
    digitalWrite(LED_SND, HIGH);
    break;
  case ELTRES_BOARD_EVT_GNSS_UNRECEIVE:
    Serial.println("gnss wave has not been received.");
    digitalWrite(LED_GNSS, LOW);
    break;
  case ELTRES_BOARD_EVT_GNSS_RECEIVE:
    Serial.println("gnss wave has been received.");
    digitalWrite(LED_GNSS, HIGH);
    gnss_recevie_timeout = false;
    break;
  case ELTRES_BOARD_EVT_FAULT:
    Serial.println("internal error.");
    error_loop();
    break;
  }
}

// GGA情報受信コールバック
void gga_event_cb(const eltres_board_gga_info *gga_info) {
  Serial.print("[gga]");
  last_gga_info = *gga_info;
  if (gga_info->m_pos_status) {
    Serial.print("utc: ");
    Serial.println((const char *)gga_info->m_utc);
    Serial.print("lat: ");
    Serial.print((const char *)gga_info->m_n_s);
    Serial.print((const char *)gga_info->m_lat);
    Serial.print(", lon: ");
    Serial.print((const char *)gga_info->m_e_w);
    Serial.println((const char *)gga_info->m_lon);
    Serial.print("pos_status: ");
    Serial.print(gga_info->m_pos_status);
    Serial.print(", sat_used: ");
    Serial.println(gga_info->m_sat_used);
    Serial.print("hdop: ");
    Serial.print(gga_info->m_hdop);
    Serial.print(", height: ");
    Serial.print(gga_info->m_height);
    Serial.print(" m, geoid: ");
    Serial.print(gga_info->m_geoid);
    Serial.println(" m");
  } else {
    Serial.println("invalid data.");
  }
}

// ELTRESの初期化関数
void init_eltres() {
  eltres_board_result ret = EltresAddonBoard.begin(ELTRES_BOARD_SEND_MODE_1MIN, eltres_event_cb, gga_event_cb);
  if (ret != ELTRES_BOARD_RESULT_OK) {
    digitalWrite(LED_RUN, LOW);
    digitalWrite(LED_ERR, HIGH);
    program_sts = PROGRAM_STS_STOPPED;
    Serial.print("cannot start eltres board (");
    Serial.print(ret);
    Serial.println(").");
    error_loop();
  } else {
    program_sts = PROGRAM_STS_RUNNING;
  }
}

// ペイロード設定関数
void setup_payload() {
  // 推論結果や温度データを取得（ここに実装）
  uint32_t data = 0;
  uint16_t temperature = (uint16_t)msgdata; // 温度データ
  uint8_t inference;      // 推論結果
  
  String command = Serial.readStringUntil('\n');
  command.trim(); // 前後の空白を除去'
  inference = uint8_t(command.toInt());

  // 温度情報をビット16-32に格納
  data |= ((temperature & 0xffff)) << 16;

  // 推論情報をビット32に格納
  data |= ((inference & 0x01)) << 8;

  String lat_string = String((char *)last_gga_info.m_lat);
  String lon_string = String((char *)last_gga_info.m_lon);
  int index;

  // 設定情報をシリアルモニタへ出力
  Serial.print("[setup_payload]");
  Serial.print("lat:");
  Serial.print(lat_string);
  Serial.print(",lon:");
  Serial.print(lon_string);
  Serial.print(",pos:");
  Serial.print(last_gga_info.m_pos_status);
  Serial.println();

  // ペイロード領域初期化
  memset(payload, 0x00, sizeof(payload));
  // ペイロード種別[GPSペイロード]設定
  payload[0] = 0x86;
  // 緯度設定
  index = 0;
  payload[1] = (uint8_t)(((lat_string.substring(index, index + 1).toInt() << 4)
                          + lat_string.substring(index + 1, index + 2).toInt()) &
                         0xff);
  index += 2;
  payload[2] = (uint8_t)(((lat_string.substring(index, index + 1).toInt() << 4)
                          + lat_string.substring(index + 1, index + 2).toInt()) &
                         0xff);
  index += 2;
  index += 1; // skip "."
  payload[3] = (uint8_t)(((lat_string.substring(index, index + 1).toInt() << 4)
                          + lat_string.substring(index + 1, index + 2).toInt()) &
                         0xff);
  index += 2;
  payload[4] = (uint8_t)(((lat_string.substring(index, index + 1).toInt() << 4)
                          + lat_string.substring(index + 1, index + 2).toInt()) &
                         0xff);
  // 経度設定
  index = 0;
  payload[5] = (uint8_t)(lon_string.substring(index, index + 1).toInt() & 0xff);
  index += 1;
  payload[6] = (uint8_t)(((lon_string.substring(index, index + 1).toInt() << 4)
                          + lon_string.substring(index + 1, index + 2).toInt()) &
                         0xff);
  index += 2;
  payload[7] = (uint8_t)(((lon_string.substring(index, index + 1).toInt() << 4)
                          + lon_string.substring(index + 1, index + 2).toInt()) &
                         0xff);
  index += 2;
  index += 1; // skip "."
  payload[8] = (uint8_t)(((lon_string.substring(index, index + 1).toInt() << 4)
                          + lon_string.substring(index + 1, index + 2).toInt()) &
                         0xff);
  index += 2;
  payload[9] = (uint8_t)(((lon_string.substring(index, index + 1).toInt() << 4)
                          + lon_string.substring(index + 1, index + 2).toInt()) &
                         0xff);
  // センサ値設定
  payload[10] = (uint8_t)((data >> 24) & 0xff);
  payload[11] = (uint8_t)((data >> 16) & 0xff);
  payload[12] = (uint8_t)((data >> 8) & 0xff);
  payload[13] = (uint8_t)(data & 0xff);
  // 拡張用領域(0固定)設定
  payload[14] = 0x00;
  // 品質設定
  payload[15] = last_gga_info.m_pos_status;
}

// データ送信関数
void send_to_database() {
  if (program_sts == PROGRAM_STS_RUNNING) {
    if (gnss_recevie_timeout) {
      uint64_t now_time = millis();
      if ((now_time - last_change_blink_time) >= 1000) {
        last_change_blink_time = now_time;
        bool set_value = digitalRead(LED_ERR);
        bool next_value = (set_value == LOW) ? HIGH : LOW;
        digitalWrite(LED_ERR, next_value);
      }
    } else {
      digitalWrite(LED_ERR, LOW);
    }

    if (event_send_ready) {
      event_send_ready = false;
      setup_payload();
      EltresAddonBoard.set_payload(payload);
    }
  }
}

// setup関数
void setup() {
  Serial.begin(115200);

  // MSC機能
  theSD.beginUsbMsc();

  // Subcore起動
  int ret = MP.begin(1);
  Serial.print("Sub core 1 started.");

  // LED初期設定
  pinMode(LED_RUN, OUTPUT);
  digitalWrite(LED_RUN, LOW);
  pinMode(LED_GNSS, OUTPUT);
  digitalWrite(LED_GNSS, LOW);
  pinMode(LED_SND, OUTPUT);
  digitalWrite(LED_SND, LOW);
  pinMode(LED_ERR, OUTPUT);
  digitalWrite(LED_ERR, LOW);

  // Audioの初期化
  init_audio();

  // ELTRESの初期化
  init_eltres();

  // 録音用のフラグを設定（仮にすぐに録音を開始するように）
  startRecording = true;
}

// loop関数
void loop() {
  delay(13000);
  if (startRecording) {
    start_audio_recording();
    delay(10000); //この間に音声処理
    MP.Recv(&msgid, &msgdata);
    send_to_database();
  }
  // データ送信処理
  send_to_database();

  delay(100);
}
