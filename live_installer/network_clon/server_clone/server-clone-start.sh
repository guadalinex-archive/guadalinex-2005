#!/bin/sh

if [ $(id -u) != 0 ]; then
	echo "Necesita ser root para ejecutar este programa"
	exit 0
fi

if [ ! -f "/tmp/server-config.cfg" ]; then
	echo "ERROR: Necesita ejecutar primero: /usr/sbin/server-clone"
	exit 0
fi

. /tmp/server-config.cfg

echo "Por favor, inserte en CD-Rom de Guadalinex y pulse cualquier tecla..."
read

sleep 2
if [ ! -f "/cdrom/META/META.squashfs" ]; then
	cat <<EOF
ERROR: No se ha encontrado el archivo que se necesitaba.

Esto puede deberse a que no ha introducido el CD aún.
Que no es el CD correcto o que todavía no se ha terminado de leer la unidad.
Por favor, compruebe que es el CD correcto e inténtelo de nuevo.
EOF
	exit 0
fi

# Configure the network interface
echo "Se va a reconfigurar su interfaz de red $DEV, para que de los servicios que necesitan los clientes para empezar el clonado."
ifconfig $DEV down
ifconfig $DEV $IP up

# Start the servers
oftpd -i ${DEV} -N root /cdrom/META/ &
sed "s|@@DEV@@|${DEV}|g" /usr/share/server-clone/conf/udhcpd.conf > /etc/udhcpd.conf
/etc/init.d/udhcpd start

cat <<EOF
Ya está todo listo.
Cuando quiera puede arrancar los clientes.
¡Feliz instalación!
EOF

