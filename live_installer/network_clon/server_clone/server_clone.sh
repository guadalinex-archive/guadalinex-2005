#!/bin/sh

BACKTITLE="Clonador en Red"

alias dialog='dialog  --stdout --cr-wrap --backtitle "$BACKTITLE"'

DEVS=$(grep [0-9]: /proc/net/dev | grep -v sit | tr -s '  ' ' ' | cut -d ' ' -f 2 | cut -d ':' -f 1)
NUMDEVS=0
for i in $DEVS
        do
        NUMDEVS=$[NUMDEVS+1]
done

# Create temp directories
ISOLINUXDIR=$(mktemp -d /tmp/client-image.XXXXXX)
INITRAMFSDIR=$(mktemp -d /tmp/initramfs.XXXXXX)

# Default options (by now)
NET=192.168.10.1
MOUNTPOINTS="/:/dev/@@@da1;swap:/dev/@@@da2"

#NET=$(dialog --title "Configuración de la red" \
#    --inputbox "¿Cuál es la dirección del servidor?" \
#    10 50 "192.168.0.1")

dialog --title "Configuración de la red" \
    --msgbox "\nLa dirección del servidor será $NET" \
    10 50

DEV=$(dialog --title "Configuración de la red" \
    --menu "¿Qué interfaz de red desea usar?" 10 50 $NUMDEVS \
    $(for i in $DEVS; do echo "$i" "/dev/$i" ; done))
    
USER=$(dialog --title "Información del usuario" \
    --inputbox "¿Cuál será el nombre con el que el usuario accedera a los clientes? \n
(Por ejemplo: alumno)" \
    10 50 )

FULLNAME=$(dialog --title "Información del usuario" \
    --inputbox "¿Cuál será el nombre completo del usuario en los clientes? \n
(Por ejemplo: Alumnos de prácticas de Topografía)" \
    10 50 )

CHECK='n'

while [ $CHECK == 'n' ]; do
	PASS=$(dialog --title "Información del usuario" --insecure \
	    --passwordbox "¿Cuál será la clave con la que el usuario\n accedera a los clientes? \n
(Debe tener una longitud de entre 5 y 15 caracteres)" \
	    10 50 )
	
	PASS2=$(dialog --title "Información del usuario" --insecure \
	    --passwordbox "Por favor, repita la clave" \
	    10 30 )
	
	if [ "$PASS" == "$PASS2" ] ; then
		CHECK='y'
	else
		dialog --title "ERROR" \
		--msgbox "La clave introducida es diferente.\n
Por favor, vuelva a intentarlo." \
		10 30
	fi
done

HOSTNAME=$(dialog --title "Información del equipo" \
    --inputbox "¿Cuál será el nombre de los clientes? \n\n
Esta información podrá cambiarla después. \n Lo ideal es que cada equipo tenga su propio nombre, pero esta versión no lo soporta aún.\n\nSentimos las molestias." \
    0 0 )



dialog --title "Generando la imagen de los clientes" \
    --msgbox "\nSe esta generando la imagen de los clientes.\n
Por favor, espere mientras termina el proceso.\n
Gracias." \
    10 50


# untarting the initramfs template

tar -C ${INITRAMFSDIR} -zxf /usr/share/server-clone/initramfs.tgz >> /tmp/server-clone.log 2>&1
mkdir -p ${INITRAMFSDIR}/etc/

cat > ${INITRAMFSDIR}/etc/config.cfg  <<EOF
serverip=$NET
mountpoints=$MOUNTPOINTS
username=$USER
fullname=$FULLNAME
password=$PASS
hostname=$HOSTNAME
# Not supported yet
dhcp=y
ip=
netmask=
gateway=
EOF

cat > /tmp/server-config.cfg  <<EOF
DEV=$DEV
IP=$NET
EOF

# Creating the initramfs file
(cd ${INITRAMFSDIR} && find . | cpio --quiet --dereference -o -H newc | gzip -9 >${ISOLINUXDIR}/initramfs)

# Placeing the rest of the stuff

cp -a /usr/share/server-clone/isolinux/* ${ISOLINUXDIR} >> /tmp/server-clone.log 2>&1

# Create the cd image

mkisofs -l -r -J -V "Guadalinex Clone System" -hide-rr-moved -v -b isolinux.bin \
-c boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table \
-o /tmp/client-image.iso ${ISOLINUXDIR} >> /tmp/server-clone.log 2>&1

dialog --title "Generando la imagen de los clientes" \
    --msgbox "\nSe ha generando la imagen de los clientes.\n
Dicha imagen se encuentra en /tmp/client-image.iso\n\n
Puede grabar la imagen en un CD-Rom con su programa favorito\n
tipo cdrecord, nautilus-cd-burner, etc.\n\n
Si lo desea puede encontrar los logs del proceso\n
realizado en: /tmp/server-clone.log" \
    10 50

rm -fr ${INITRAMFSDIR} ${ISOLINUXDIR}


