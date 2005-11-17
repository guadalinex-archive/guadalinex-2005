#!/bin/sh

dpkg -l flashplugin-nonfree 2>&1 > /dev/null

if [[ `dpkg --get-selections flashplugin-nonfree|awk '{ print $2 }'` != "install" ]]; then

zenity --question --title "Instalación de Macromedia Flash" --text "Se va a proceder a instalar los plugins para Mozilla Firefox de Macromedia Flash. Es necesario que esté conectado a Internet.

¿Desea continuar?" && gnome-terminal --hide-menubar -x gksudo 'DEBIAN_FRONTEND=noninteractive /usr/bin/apt-get --yes install flashplugin-nonfree'

else

zenity --info --title "Instalación de Macromedia Flash" --text "Ya tiene instalado el plugin de Macromedia Flash para Mozilla Firefox."

fi
