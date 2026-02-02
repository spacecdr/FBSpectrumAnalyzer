#!/bin/bash

# Funzione per pulire lo schermo all'uscita
trap "clear; exit" SIGINT SIGTERM

while true; do
  TERM_WIDTH=$(tput cols)
  TERM_HEIGHT=$(tput lines)

  MENU_WIDTH=50
  MENU_HEIGHT=12

  POS_Y=$(( (TERM_HEIGHT - MENU_HEIGHT) / 2 ))
  POS_X=$(( (TERM_WIDTH - MENU_WIDTH) / 2 ))

  CHOICE=$(dialog --clear \
                --backtitle "CPanel" \
                --title " RUN " \
                --begin $POS_Y $POS_X \
                --menu "" $MENU_HEIGHT $MENU_WIDTH 2 \
                1 "🚀 Spectrum Analyzer" \
                2 "📊 Network Config" \
                3>&1 1>&2 2>&3)

  exit_status=$?
  if [ $exit_status -ne 0 ]; then
    clear
    break
  fi

  case $CHOICE in
    1)
      clear
      sudo service shairport-sync restart
      ./spectrum
      sleep 2
      ;;
    2)
      sudo nmtui
      ;;
  esac
done
