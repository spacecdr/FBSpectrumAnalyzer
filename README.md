This is a "C" Spectrum Analyzer for Linux FrameBuffer
My main goal, is to use it on older computers to let them working as Spectrum Analyzer and Airplay receiver. Thru Alsa, it is able to detect sounds from microphone or aux input, and show the audio spectrum.
I installed a clean Debian 12 (in this screenshot, it's a Samsung N130, which has an Atom 32bit, then it's Debian X86), setting autologin on tty1, and a bash/Dialog panel just to connect WiFi (by NetworkManager "nmtui") and start the program (restarting ShairPort on launch)

How to install:

sudo apt install shairport-sync
sudo apt install build-essential libasound2-dev libfftw3-dev

gcc spectrum.c -o spectrum -lasound -lfftw3 -lm

then execute ./spectrum

if you set autologin for your standard user, i suggest to get the dashboard.sh script, and setting it executable by "chmod +x dashboard.sh", and run it by .bashrc at the end of the file by append "./dashboard.sh"
