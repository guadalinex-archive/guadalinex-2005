#!/bin/sh

if [[ $UID -eq 0 ]];then

if [[ `dpkg --get-selections flashplugin-nonfree|awk '{ print $2 }'` != "install" ]]; then

zenity --question --title "Instalación de Macromedia Flash" --text "Se va a proceder a instalar los plugins para Mozilla Firefox de Macromedia Flash. Es necesario que esté conectado a Interet.

¿Desea continuar?" && echo "flashplugin-nonfree install" | synaptic --hide-main-window --non-interactive --set-selections --progress-str "Instalando Macromedia Flash" --finish-str "Paquete instalado"

else

zenity --info --title "Instalación de Macromedia Flash" --text "Ya tiene instalado el plugin de Macromedia Flash para Mozilla Firefox."

fi

else

gksudo $0

fi
