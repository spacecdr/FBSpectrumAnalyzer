<p align="center">
<b>Spectrum Analyzer for Linux FrameBuffer</b>
</p> 

<img width="1024" height="600" alt="fb" src="https://github.com/user-attachments/assets/205c89f1-e38c-449d-9980-521df8afd3d5" />


# 🎹 C Spectrum Analyzer for Linux FrameBuffer

A lightweight spectrum analyzer written in **C**, specifically designed for the **Linux FrameBuffer**. This project is ideal for repurposing older hardware (like 32-bit Atom netbooks) into dedicated audio analysis stations and AirPlay receivers.

## 🚀 Features
* **High Performance:** Written in C for minimal resource usage, perfect for legacy hardware.
* **Versatile Input:** Uses **ALSA** to capture audio from microphones, AUX inputs, or internal loops.
* **Standalone:** Runs directly from the TTY/FrameBuffer without requiring a GUI (X11/Wayland).
* **AirPlay Integration:** Optimized to work alongside `shairport-sync`.

## 🛠️ System Requirements & Environment
This project was developed and tested on:
* **Hardware:** Samsung N130 (Intel Atom 32-bit).
* **OS:** Debian 12 "Bookworm" (X86).
* **Configuration:** Auto-login enabled on `tty1` for a seamless "appliance" experience.

## 📦 Installation & Compilation

1.  **Install dependencies:**
    ```bash
    sudo apt update
    sudo apt install shairport-sync build-essential libasound2-dev libfftw3-dev libcurl4-openssl-dev ffmpeg
    ```

2.  **Compile the source:**
    Use `gcc` to link the ALSA, FFTW3, and Math libraries:
    ```bash
    gcc spectrum.c -o spectrum -lasound -lfftw3 -lm -lcurl
    ```

3.  **Run the analyzer:**
    ```bash
    ./spectrum
    ```
    
## ⚙️ Usage
if you push H key, hours will be shown. +/- increase/decrease font size. Ths S key shows Spotify details. The setting is permanent even after restart.
 
---

## 🖥️ Recommended Setup (Dashboard Mode)
To turn your computer into a dedicated audio device, you can use the provided `dashboard.sh` script. This script features a **Bash/Dialog** panel to manage WiFi connections (via `nmtui`) and launches the program while automatically restarting `shairport-sync`.

### Set up Auto-launch
1.  **Make the script executable:**
    ```bash
    chmod +x dashboard.sh
    ```

2.  **Add to Bash profile:**
    Append the script to the end of your `~/.bashrc` file to launch it automatically upon TTY login:
    ```bash
    echo "./dashboard.sh" >> ~/.bashrc
    ```

---

<img width="1024" height="600" alt="screen" src="https://github.com/user-attachments/assets/e51d817a-1744-4676-8fbf-d38e8761d995" />



## Video


https://github.com/user-attachments/assets/6fcc3913-acc8-4869-be19-fabc4e5d5f10


