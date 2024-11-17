import os
import csv
import random

def label_wav_files(base_directory, wav_directory, output_csv):
    # wav_directoryの相対パスをbase_directoryから計算
    wav_directory_full_path = os.path.join(base_directory, wav_directory)
    
    # .wavファイルの相対パスを取得
    wav_files = []
    for root, dirs, files in os.walk(wav_directory_full_path):
        for file in files:
            if file.endswith('.wav'):
                relative_path = os.path.relpath(os.path.join(root, file), base_directory)
                wav_files.append(relative_path)
    
    # ラベル（0か1）をランダムに割り当て
    labeled_data = [(wav_file, 0) for wav_file in wav_files]
    
    # CSVファイルに書き込み
    output_csv_full_path = os.path.join(base_directory, output_csv)
    with open(output_csv_full_path, mode='w', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(["File Path", "Label"])
        writer.writerows(labeled_data)
    
    print(f"CSV file '{output_csv}' has been created with labels.")

# 使用例
if __name__ == "__main__":
    # このスクリプトが存在するsrcディレクトリの親ディレクトリ（venv/binと同じディレクトリ）を取得
    script_directory = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    
    # venv/binと同じディレクトリにあるwavファイルのディレクトリの相対パス
    wav_directory = 'iie_wav_remake'  # wavファイルが格納されているディレクトリの相対パスを指定
    
    # 出力するCSVファイル名
    output_csv = 'labeled_iie_wav_files_for_training.csv'
    
    # 関数の呼び出し
    label_wav_files(script_directory, wav_directory, output_csv)

