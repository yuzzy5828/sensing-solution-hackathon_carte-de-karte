import os
from pydub import AudioSegment

# 入力ディレクトリと出力ディレクトリの指定
input_directory = "/mnt/usb"# "/home/yujiro/venv/wav_editor/iie_wav"
output_directory = "/mnt/usb"# "/home/yujiro/venv/wav_editor/iie_wav_remake"

# 出力ディレクトリが存在しない場合は作成
if not os.path.exists(output_directory):
    os.makedirs(output_directory)

# 入力ディレクトリ内のすべてのファイルをループ
for filename in os.listdir(input_directory):
    if filename.endswith(".wav"):
        # ファイルパスを作成
        file_path = os.path.join(input_directory, filename)

        # オーディオファイルを読み込む
        audio = AudioSegment.from_wav(file_path)

        # 録音全体の長さを取得（ミリ秒単位）
        audio_length = len(audio)

        # 1秒（1000ミリ秒）の間隔でオーディオを分割
        for i in range(0, audio_length, 1000):
            # 分割されたオーディオ部分を切り取る
            segment = audio[i:i+1000]

            # 新しいファイルパスを作成（出力ディレクトリに保存）
            output_file_path = os.path.join(output_directory, f"{filename}_part_{i//1000}.wav")

            # 切り取ったオーディオを保存
            segment.export(output_file_path, format="wav")

            print(f"Saved {filename} part {i//1000} to {output_file_path}")

print("すべてのwavファイルが1秒ごとに分割されました。")
