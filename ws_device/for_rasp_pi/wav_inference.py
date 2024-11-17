import nnabla as nn
from nnabla.utils.nnp_graph import NnpLoader
import nnabla.functions as F
import nnabla.parametric_functions as PF
import os
import numpy as np
from scipy.io import wavfile  # WAVファイルを読み込むためのライブラリ

from pydub import AudioSegment
from pydub.playback import play

import serial
import time

nnppath = "src/model.nnp"
wav_filename = "src/hai.wav"  # 読み込むWAVファイルのパス

output = 0

# wavファイルの再生用
wav_file_path1 = "src/interaction1.wav"
wav_file_path2 = "src/interaction2.wav"

audio1 = AudioSegment.from_file(wav_file_path1, format="wav")
audio2 = AudioSegment.from_file(wav_file_path2, format="wav")

# フローの開始
time.sleep(5)
play(audio1)

# こちらは，かるてどかるて，リアルタイムカルテサービスです．今から行う質問にはいまたはいいえで答えてください．持病はありますか？

if os.path.isfile(nnppath):
    # .nnpファイルを読み込む
    nnp = NnpLoader(nnppath)
    net = nnp.get_network("MainRuntime", batch_size=1)
    x = net.inputs['Input']
    y = net.outputs['Softmax']

    # WAVファイルを読み込む
    sample_rate, audio_data = wavfile.read(wav_filename)
    # サンプリングレートが48,000Hzか確認
    if sample_rate != 48000:
        raise ValueError(f"サンプリングレートが48,000Hzではありません。現在のサンプリングレート: {sample_rate}Hz")

    # 音声データを浮動小数点型に変換
    audio_data = audio_data.astype(np.float32)

    # モノラル確認（ステレオの場合はモノラルに変換）
    if len(audio_data.shape) > 1:
        # ステレオをモノラルに変換
        audio_data = np.mean(audio_data, axis=1)

    # 音声データの正規化（32,768で割る） # これは不要
    audio_data = audio_data

    # 音声データの長さを確認し、48000サンプルに調整
    if len(audio_data) < 48000:
        # ゼロパディングでデータを48000サンプルに拡張
        padding_length = 48000 - len(audio_data)
        audio_data = np.pad(audio_data, (0, padding_length), 'constant')
    elif len(audio_data) > 48000:
        # データを48000サンプルにトリミング
        audio_data = audio_data[:48000]

    # ネットワークの期待する入力形状にリシェイプ
    # 例として (1, 48000, 1) の形状を使用
    audio_data = audio_data.reshape(1,48000,1)

    # 入力変数にデータを割り当て
    x.d = audio_data

    # 推論を実行
    y.forward(clear_buffer=True)
    print("inference: ", y.d)
else:
    print(f"{nnppath} が見つかりません。")

if y.d[0][0] > y.d[0][1]:
    output = 0
else:
    output = 1

# pyserialの処理
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
ser.write(f"{output}\n".encode('utf-8'))
ser.close()

time.sleep(5)

# フローの終了
play(audio2)

# ありがとうございます．あなたの位置情報・体温・解析結果をデータベースに送信しました．救助隊が向かってくるまで，動かず，焦らずに待っていてください．