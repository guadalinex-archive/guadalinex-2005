README
======

*** IMPORTANTE, para la generaci�n es necesario:

  - "Llenar" la carpeta ftp/ Por motivos de espacio y agilidad, dichos directorios no se incluyen en este conjunto de scripts.
    Los directorios fundamentales a "llenar" son:
      pool/
      dist/
      indices/

    Dichos directorios se pueden descargar de http://mirror.isp.net.au/#ubuntu

  - A�adir al path el directorio cdimage/bin
    PATH=$PATH:/home/pepito/cdimage/bin

  - Especificar la variable CDIMAGE_ROOT puede hacerse de dos formas:
   1. declarandola de forma externa y exportandola
       $ export CDIMAGE_ROOT=/home/pepito/cdimage
   2. Cambiando en todos los ficheros (en cdimage/bin/) el valor de la variable
   
  - Conexi�n a internet, en un punto de la generaci�n se conecta a un servidor de ubuntu para obtener unas listas de paquetes.

  - Cambiar "ArchiveDir" en los ficheros flamenco.conf y d-i_flamenco.conf (cdimage/uda/conf/). "ArchiveDir" debe apuntar al directorio del repositorio (cdimage/ftp).


Generaci�n
==========

  Para generar una imagen ha de ejecutarse el script denominado 'generate_no-live.sh'. Dicho script llama a su vez, en un orden l�gico, a todos los script localizados en cdimage/bin/

  Es decir, ejecutamos:
  	$ generate_no-live.sh

  En el directorio cdimage/scratch/guadalinex/debian-cd/i386 se generar� un archivo .raw, el cual es equivalente a un archivo .iso.

Incluir paquetes
================

  Para incluir un paquete, coloca el .deb o .udeb en cdimage/uda/pkgs, �eso es todo!
  El paquete ser� incluido en el pool si el paquete no existe en pool/ o si la versi�n es mayor.
