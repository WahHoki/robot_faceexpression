import socket
import numpy as np
import time
import csv
import datetime

# --- KONFIGURASI SERVER ---
# Gunakan 0.0.0.0 agar Python mendengarkan dari SEMUA interface jaringan di laptop
# (Ini lebih aman daripada hardcode IP laptop di sisi server)
UDP_IP_LISTEN = "0.0.0.0" 
UDP_PORT = 4444
SAMPLE_RATE = 16000
CHUNK_DURATION = 5  # Analisis setiap 5 detik

# --- CSV SETUP ---
csv_filename = 'data.csv'
# Mode 'a' (append) agar data lama tidak hilang saat program di-restart
try:
    csv_file = open(csv_filename, 'a', newline='') 
    writer = csv.writer(csv_file)
    # Cek apakah file kosong, jika iya tulis header
    if csv_file.tell() == 0:
        writer.writerow(["Timestamp", "RMS_Volume", "Zero_Crossing_Rate", "Deteksi_Emosi"])
except Exception as e:
    print(f"Error membuka CSV: {e}")
    print("Pastikan file data.csv tidak sedang dibuka di Excel!")
    exit()

# --- UDP SETUP ---
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    sock.bind((UDP_IP_LISTEN, UDP_PORT))
except OSError:
    print(f"ERROR: Port {UDP_PORT} sedang digunakan!")
    print("Pastikan tidak ada script Python lain yang sedang berjalan.")
    exit()

print(f"--- OTAK HOCI AKTIF ---")
print(f"Mendengarkan di Port: {UDP_PORT}")
print(f"Durasi Analisis: {CHUNK_DURATION} detik")
print(f"Simpan Data ke: {csv_filename}")
print("Silakan bicara ke ESP32...")
print("------------------------------------------------")

audio_buffer = []

def analyze_emotion(audio_data):
    # Konversi ke float untuk perhitungan matematika
    data_float = audio_data.astype(np.float32)
    
    # 1. Hitung RMS (Volume/Kekerasan Suara)
    rms = np.sqrt(np.mean(data_float**2))
    
    # 2. Hitung Zero Crossing Rate (Frekuensi/Nada Kasar)
    zero_crossings = np.sum(np.abs(np.diff(np.sign(data_float))))
    
    # --- LOGIKA EMOSI ---
    # Perintah yang akan dikirim ke ESP32: "HAPPY", "SAD", "ANGRY", "NEUTRAL"
    command = "NEUTRAL"
    label = "NORMAL 🙂"
    
    # Threshold (Ambang Batas) - Bisa dikalibrasi nanti
    if rms < 500:
        label = "HENING 😶"
        command = "NEUTRAL"
    elif rms < 2000 and zero_crossings < 2500: 
        label = "SEDIH 😢"
        command = "SAD"
    elif rms > 5000:
        label = "MARAH/SEMANGAT 😡"
        command = "ANGRY"
    else:
        label = "SENANG/NORMAL 😄"
        command = "HAPPY"
        
    return label, command, rms, zero_crossings

try:
    while True:
        # Menerima data audio DAN alamat ESP32 (IP, Port)
        # Buffer 4096 bytes cukup aman untuk UDP
        data, esp_address = sock.recvfrom(4096)
        
        # Konversi byte mentah ke array integer 16-bit (format audio I2S)
        signal = np.frombuffer(data, dtype=np.int16)
        audio_buffer.append(signal)
        
        # Cek apakah buffer sudah cukup 5 detik
        total_samples = sum(len(x) for x in audio_buffer)
        if total_samples >= SAMPLE_RATE * CHUNK_DURATION:
            
            # Gabungkan potongan-potongan audio
            full_audio = np.concatenate(audio_buffer)
            
            # 1. LAKUKAN ANALISIS
            label_emosi, cmd_esp, rms, zcr = analyze_emotion(full_audio)
            
            # 2. CATAT DATA (LOG & CSV)
            tgl = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            print(f"[{tgl}] Suara: {label_emosi} (Vol:{int(rms)}) -> Perintah: [{cmd_esp}]")
            
            writer.writerow([tgl, f"{rms:.2f}", zcr, label_emosi])
            csv_file.flush() # Simpan segera ke harddisk
            
            # 3. KIRIM PERINTAH BALIK KE ESP32 (FEEDBACK LOOP)
            # Ini yang membuat wajah Hoci berubah!
            sock.sendto(cmd_esp.encode(), esp_address)
            
            # Bersihkan buffer untuk 5 detik berikutnya
            audio_buffer = []

except KeyboardInterrupt:
    csv_file.close()
    print("\nProgram Berhenti. File CSV aman.")
    sock.close()