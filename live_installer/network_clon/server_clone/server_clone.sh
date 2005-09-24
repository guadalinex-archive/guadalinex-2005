#!/bin/sh

BACKTITLE="Clonador en Red"

alias dialog='dialog  --stdout --backtitle "$BACKTITLE"'

DEVS=$(grep [0-9]: /proc/net/dev | grep -v sit | tr -s '  ' ' ' | cut -d ' ' -f 2 | cut -d ':' -f 1)
NUMDEVS=0
for i in $DEVS
        do
        NUMDEVS=$[NUMDEVS+1]
done


NET=$(dialog --title "Configuración de la red" \
    --inputbox "¿Cuál es la dirección del servidor?" \
    10 50 "192.168.0.1")

NET=$(dialog --title "Configuración de la red" \
    --msgbox "La dirección del servidor será $NET" \
    10 50)

DEV=$(dialog --title "Configuración de la red" \
    --menu "¿Qué interfaz de red desea usar?" 10 50 $NUMDEVS \
    $(for i in $DEVS; do echo "$i" "/dev/$i" ; done))
    
echo $NET
echo $DEV
