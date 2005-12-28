#!/bin/sh

if [[ $UID -eq 0 ]];then

#if [[ `dpkg --get-selections flashplugin-nonfree|awk '{ print $2 }'` != "install" ]]; then
if [ ! -d /usr/lib/flashplugin-nonfree/ ]; then

zenity --question --title "Instalación de Macromedia Flash" --text "Se va a proceder a instalar los plugins para Mozilla Firefox de Macromedia Flash. Es necesario que esté conectado a Interet.

¿Desea continuar?" && update-flashplugin | zenity --progress --pulsate --title="Plugin de Macromedia Flash" --text "Descargando..." --auto-close

if [ -d /usr/lib/flashplugin-nonfree ]; then
	zenity --info --title "Instalación de Macromedia Flash" --text "Instalación completada con éxito"
else
	zenity --info --title "Instalación de Macromedia Flash" --text "Ha habido algún error con la descarga. Inténtelo de nuevo más tarde"
fi

else

zenity --info --title "Instalación de Macromedia Flash" --text "Ya tiene instalado el plugin de Macromedia Flash para Mozilla Firefox."

fi

else

gksudo $0

fi
