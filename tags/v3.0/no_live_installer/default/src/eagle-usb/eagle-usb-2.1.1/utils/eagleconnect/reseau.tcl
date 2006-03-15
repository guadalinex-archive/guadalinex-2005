#sh
# La ligne suivante est executee par sh, pas par Tcl \
exec wish $0 ${1+"$@"}

package require msgcat;
msgcat::mclocale $env(LANG);
#::msgcat::mclocale "en_EN"

msgcat::mcload [pwd];

#ci-dessus, j'appelle le paquetage msgcat qui va me permettre d'afficher les messages
#de l'interface dans la langue du système de l'utilisateur (définie dans $env(LANG)).
#la commande msgcat::mcload [pwd] permet justement de charger les catalogues propres à
#chaque langue (en.msg si LANG = en_EN, fr.msg si LANG = fr_FR etc...)
# avec ::msgcat::mclocale "en_EN" ci-dessus, j'essaie en environnement en_EN 
# pour voir si le catalogue de traduction anglais est correctement appelé

set SBIN_DIR @SBIN_DIR@
set EU_EAGLECONNECT_DIR @EU_EAGLECONNECT_DIR@

set env(PATH) /bin:/sbin:/usr/bin:/usr/sbin:/usr/X11R6/bin:/usr/local/sbin;
# je définis les chemins où peuvent se situer les exécutables



wm maxsize . 780 450
wm title . [msgcat::mc "Etat du réseau"]
wm geometry . 480x400+200+200


frame .frame1
pack .frame1

frame .frame2 -borderwidth 1 -relief sunken
pack .frame2 ;


frame .frame3 -borderwidth 1 -relief sunken
pack .frame3;


frame .frame4 -borderwidth 1 -relief sunken
pack .frame4;


frame .frame5
pack .frame5;
frame .maframe 
pack .maframe;







#----------------------------------------------------------------------------------------------------------------
#----------------------------------------CHAMPS DE l'INTERFACE---------------------------------------------------
#-------------------------------------------------------------------------------------------------------------------

#---------------------ligne 1 ------------------------------------------------------------

label .frame1.ldriver -width 10 -text "DRIVER "
grid .frame1.ldriver -row 0 -column 0 -ipadx 11

entry .frame1.fdriver -background white \
 -textvariable driver \
 -width 15;
grid .frame1.fdriver -row 0 -column 1 -ipadx 10

label .frame1.rien1 -width 32 -text ""
grid .frame1.rien1 -row 0 -column 2 -ipady 10




#-------------------------ligne 2 -----------------------------------------------------------------

label .frame2.lbus -width 10 -text " USB BUS "
grid .frame2.lbus -row 0 -column 0 -ipadx 11

entry .frame2.bus -background white \
 -textvariable bus \
 -width 4;
grid .frame2.bus -row 0 -column 1 -ipadx 10

label .frame2.ldevice -width 10 -text " USB DEVICE "
grid .frame2.ldevice -row 0 -column 2 -ipadx 11


entry .frame2.device -background white \
 -textvariable device \
 -width 4;
grid .frame2.device -row 0 -column 3 -ipadx 10

label .frame2.dbgmask -width 10 -text "DBG MASK "
grid .frame2.dbgmask -row 0 -column 4 -ipadx 11

entry .frame2.dbg -background white \
 -textvariable dbg \
 -width 4;
grid .frame2.dbg -row 0 -column 5 -ipadx 10


#--------------ligne 3 --------------------------------------------

label .frame3.rien1 -width 2 -text ""
grid .frame3.rien1 -row 1 -column 1  


label .frame3.interf -width 20 -text "ETHERNET INTERFACE "
grid .frame3.interf -row 1 -column 2 


entry .frame3.interface -background white \
 -textvariable interface \
 -width 12;
grid .frame3.interface -row 1 -column 3  

label .frame3.rien2 -width 3 -text ""
grid .frame3.rien2 -row 1 -column 4 -ipady 10

#---------------ligne 3 bis (ajout du 13/03/04 l'adresse MAC, driver 1.9.6)----------------

#label .frame3.rien3 -width 2 -text ""
#grid .frame3.rien3 -row 1 -column 1  


label .frame3.lmac -width 5 -text "MAC "
grid .frame3.lmac -row 1 -column 5 


entry .frame3.mac -background white \
 -textvariable mac \
 -width 18;
grid .frame3.mac -row 1 -column 6  

#label .frame3.rien4 -width 9 -text ""
#grid .frame3.rien4 -row 1 -column 7 -ipady 10







#---------------ligne 4 -------------------------------------------------------

label .frame4.txrat -width 10 -text "TX RATE "
grid .frame4.txrat -row 1 -column 0 

entry .frame4.txrate -background white \
 -textvariable txrate \
 -width 10;
grid .frame4.txrate -row 1 -column 1 


label .frame4.rxrat -width 10 -text "RX RATE "
grid .frame4.rxrat -row 1 -column 2 

entry .frame4.rxrate -background white \
 -textvariable rxrate \
 -width 10;
grid .frame4.rxrate -row 1 -column 3 


label .frame4.lcrc -width 10 -text "CRC "
grid .frame4.lcrc -row 1 -column 4 

entry .frame4.crc -background white \
 -textvariable crc \
 -width 10;
grid .frame4.crc -row 1 -column 5


label .frame4.lfec -width 10 -text "FEC "
grid .frame4.lfec -row 2 -column 0 

entry .frame4.fec -background white \
 -textvariable fec \
 -width 10;
grid .frame4.fec -row 2 -column 1

label .frame4.lmargin -width 10 -text "MARGIN"
grid .frame4.lmargin -row 2 -column 2 

entry .frame4.margin -background white \
 -textvariable margin \
 -width 10;
grid .frame4.margin -row 2 -column 3

label .frame4.latten -width 10 -text "ATTEN "
grid .frame4.latten -row 2 -column 4 

entry .frame4.fatten -background white \
 -textvariable fatten \
 -width 10;
grid .frame4.fatten -row 2 -column 5


#-------------------ligne 4 ----------------------------------------------

label .frame4.lvidcpe -width 10 -text "VID-CPE "
grid .frame4.lvidcpe -row 3 -column 0 

entry .frame4.fvidcpe -background white \
 -textvariable fvidcpe \
 -width 10;
grid .frame4.fvidcpe -row 3 -column 1


label .frame4.lvidco -width 10 -text "VID-CO "
grid .frame4.lvidco -row 3 -column 2 

entry .frame4.fvidco -background white \
 -textvariable fvidco \
 -width 10;
grid .frame4.fvidco -row 3 -column 3


label .frame4.lhec -width 10 -text "HEC "
grid .frame4.lhec -row 3 -column 4 

entry .frame4.fhec -background white \
 -textvariable fhec \
 -width 10;
grid .frame4.fhec -row 3 -column 5

#-------------ligne 5-------------------------------------------------------------

label .frame4.lvpi -width 10 -text "VPI "
grid .frame4.lvpi -row 4 -column 0 

entry .frame4.fvpi -background white \
 -textvariable fvpi \
 -width 10;
grid .frame4.fvpi -row 4 -column 1


label .frame4.lvci -width 10 -text "VCI "
grid .frame4.lvci -row 4 -column 2 

entry .frame4.fvci -background white \
 -textvariable fvci \
 -width 10;
grid .frame4.fvci -row 4 -column 3


label .frame4.ldelin -width 10 -text "DELIN "
grid .frame4.ldelin -row 4 -column 4 

entry .frame4.fdelin -background white \
 -textvariable delin \
 -width 10;
grid .frame4.fdelin -row 4 -column 5


#------------ligne 6 ---------------------------------------------------------------


label .frame4.lcellrx -width 10 -text "CELLS RX "
grid .frame4.lcellrx -row 5 -column 0 

entry .frame4.fcellrx -background white \
 -textvariable cellrx \
 -width 10;
grid .frame4.fcellrx -row 5 -column 1


label .frame4.lcelltx -width 10 -text "CELLS TX "
grid .frame4.lcelltx -row 5 -column 2 

entry .frame4.fcelltx -background white \
 -textvariable celltx \
 -width 10;
grid .frame4.fcelltx -row 5 -column 3

#----------ligne 7 -------------------------------------------------------------------------


# Je crée une étiquette avec le libellé "Paquets reçus"

label .frame4.etiquette -width 10 -text "PKTS RX "
#pack configure .frame4.etiquette -pady 3 -side left -padx 2
grid .frame4.etiquette -row 6 -column 0

# Je crée un champ qui va recevoir les infos émanant de Eaglestat
# via la procédure Envoirx qui insère le contenu de la variable $x
# le nombre de paquets reçus

entry .frame4.champpaquetsrecus -background white -justify right \
 -textvariable nbpaquetsrecus \
 -width 10;
#pack configure .frame4.champpaquetsrecus -pady 3 -padx 0 -side left
grid .frame4.champpaquetsrecus -row 6 -column 1 


label .frame4.etiquette2 -width 10 -text "PKTS TX:"
#pack configure .frame4.etiquette2 -pady 3 -side left -padx 7
grid .frame4.etiquette2 -row 6 -column 2 


entry .frame4.champpaquetstransmis -background white -justify right\
 -textvariable nbpaquetstransmis \
 -width 10;
#pack configure .frame4.champpaquetstransmis -pady 3 -padx 4 -side left
grid .frame4.champpaquetstransmis -row 6 -column 3 

 
#------------------ligne 8 ------------------------------------------------------------


label .frame4.loam -width 10 -text "OAM "
grid .frame4.loam -row 7 -column 0 

entry .frame4.foam -background white \
 -textvariable oam \
 -width 10;
grid .frame4.foam -row 7 -column 1


label .frame4.lbadvpi -width 10 -text "BAD VPI "
grid .frame4.lbadvpi -row 7 -column 2 

entry .frame4.fbadvpi -background white \
 -textvariable badvpi \
 -width 10;
grid .frame4.fbadvpi -row 7 -column 3


label .frame4.lbadcrc -width 10 -text "BAD CRC "
grid .frame4.lbadcrc -row 7 -column 4 

entry .frame4.fbadcrc -background white \
 -textvariable badcrc \
 -width 10;
grid .frame4.fbadcrc -row 7 -column 5

#-----------------------ligne 9 ---------------------------------------------------------------------------------


label .frame4.loversize -width 10 -text "OVERSIZ. "
grid .frame4.loversize -row 8 -column 0 

entry .frame4.foversize -background white \
 -textvariable oversize \
 -width 10;
grid .frame4.foversize -row 8 -column 1

#-----------------------ligne 10 ----------------------------------------------------------------------------------




label .frame5.lmodem -width 16 -text "ETAT DU MODEM  "
grid .frame5.lmodem -row 0 -column 0 

entry .frame5.fmodem -background white -foreground green -justify center\
 -textvariable modem \
 -width 36;
grid .frame5.fmodem -row 0 -column 1

proc etatdumodem {modem} {

 if {$modem == "Modem is operational"} {
     .frame5.fmodem configure -foreground green
     .frame5.fmodem delete 0 36;
     .frame5.fmodem insert 0 [msgcat::mc "OPERATIONNEL"];} else {
     .frame5.fmodem configure -foreground red
     .frame5.fmodem delete 0 36;
     .frame5.fmodem insert 0 [msgcat::mc "INOPERANT. VEUILLEZ LE REINITIALISER"]}}
   
label .frame5.vide -width 2 -text ""
grid .frame5.vide -row 0 -column 3 -ipady 10


#---------------------------------------------------------------------------------------------------------------
#------------------------------LES FONCTIONS QUI INSCRIVENT LES VALEURS DANS LES CHAMPS -----------------------
#----------------------------------------------------------------------------------------------------------------




# La procédure "envoirx" reçoit x pour argument et renvoie le contenu de 
# la variable $x au champ de entry qu'elle met automatiquement à jour
# en rappelant la procédure majchamps

proc bus {w x y z a a2 b c d e f g h i j k l m n o p q r s modem} {

.frame1.fdriver delete 0 40
.frame1.fdriver insert 0 $w;
.frame2.bus delete 0 40
.frame2.bus insert 0 $x;
.frame2.device delete 0 40;
.frame2.device insert 0 $y;
.frame2.dbg delete 0 40;
.frame2.dbg insert 0 $z;
.frame3.interface delete 0 40;
.frame3.interface insert 0 $a;
.frame3.mac delete 0 40;
.frame3.mac insert 0 $a2;
.frame4.txrate delete 0 40;
.frame4.txrate insert 0 $b;
.frame4.rxrate delete 0 40;
.frame4.rxrate insert 0 $c;
.frame4.crc delete 0 40;
.frame4.crc insert 0 $d;
.frame4.fec delete 0 40;
.frame4.fec insert 0 $e;
.frame4.margin delete 0 40;
.frame4.margin insert 0 $f;
.frame4.fatten delete 0 40;
.frame4.fatten insert 0 $g;
.frame4.fvidcpe delete 0 40;
.frame4.fvidcpe insert 0 $h;
.frame4.fvidco delete 0 40;
.frame4.fvidco insert 0 $i;
.frame4.fhec delete 0 40;
.frame4.fhec insert 0 $j;
.frame4.fvpi delete 0 40;
.frame4.fvpi insert 0 $k;
.frame4.fvci delete 0 40;
.frame4.fvci insert 0 $l;
.frame4.fdelin delete 0 40;
.frame4.fdelin insert 0 $m;
.frame4.fcellrx delete 0 40;
.frame4.fcellrx insert 0 $n;
.frame4.fcelltx delete 0 40;
.frame4.fcelltx insert 0 $o;
.frame4.foam delete 0 40;
.frame4.foam insert 0 $p;
.frame4.fbadvpi delete 0 40;
.frame4.fbadvpi insert 0 $q;
.frame4.fbadcrc delete 0 40;
.frame4.fbadcrc insert 0 $r;
.frame4.foversize delete 0 40;
.frame4.foversize insert 0 $s;
  etatdumodem $modem;


}

# Ci-dessus, la fonction bus reçoit pour arguments les variables passées par la fonction majchamps et les 
# insère dans les champs prévus à cet effet. Elle lance également la fonction etatdumodem en lui passant pour argument
# la variable modem qui contient le statut de ce dernier (operational, ou pas).



proc nombrepaquets {x y} {

 
.frame4.champpaquetsrecus delete 0 40
.frame4.champpaquetsrecus insert 0 $x
.frame4.champpaquetstransmis delete 0 40
.frame4.champpaquetstransmis insert 0 $y

after 1000 majchamps;

}
# Modif 20050226 baud123 : 10 secondes au lieu de toutes les secondes !
# Ci-dessus, la fonction nombrepaquets reçoit pour arguments les variables passées par la fonction majchamps.
# Ces variables comprennent les paquets reçus, les paquets transmis et la variable $recupstat qui contient la
# commande eaglestat et son chemin d'accès (/usr/local/sbin/eaglestat). Passé dix  secondes, la fonction nombrepaquets 
# rappelle la fonction majchamps en lui repassant pour agument la même variable $recupstat au contenu inchangé 
#afin qu'elle puisse relancer la commande eaglestat.



 
#--------------------------------------------------------------------------------------------------------------------
#-----------------RECUPERATION DES INFOS EMANANT D'EAGLESTAT QUI APPARAITRONT DANS LES CHAMPS-----------------------
#---------------------------------------------------------------------------------------------------------------------


proc majchamps {} {

# Ci-dessus, ma fonction majchamps reçoit pour agument la variable recupstat transmise par la fonction pref.
# Cette variable contient la commande eaglestat et son emplacement (/usr/local/sbin)

global SBIN_DIR

cd /tmp
file delete resulteaglestat.txt

# Ci-dessus, j'efface l'éventuel fichier resulteaglestat.txt qui pourrait exister


exec $SBIN_DIR/eaglestat >> /tmp/resulteaglestat.txt

# Ci-dessus, j'exécute la commande eaglestat (et son chemin; tous deux sont dans la variable recupstat) et je 
# redirige le résultat vers le fichier resulteaglestat.txt.

cd /tmp
file delete extractresult.txt

# Ci-dessus, j'efface l'éventuel fichier extractresult.txt qui pourrait exister


exec grep Driver  resulteaglestat.txt >> extractresult.txt
exec grep Bus  resulteaglestat.txt >> extractresult.txt
exec grep Ethernet  resulteaglestat.txt >> extractresult.txt
exec grep MAC  resulteaglestat.txt >> extractresult.txt
exec grep Rate resulteaglestat.txt >> extractresult.txt
exec grep FEC resulteaglestat.txt >> extractresult.txt
exec grep VID resulteaglestat.txt >> extractresult.txt
exec grep VCI resulteaglestat.txt >> extractresult.txt
exec grep Cells resulteaglestat.txt >> extractresult.txt
exec grep Pkts resulteaglestat.txt >> extractresult.txt
exec grep OAM resulteaglestat.txt >> extractresult.txt
exec grep Oversiz resulteaglestat.txt >> extractresult.txt
exec grep Modem resulteaglestat.txt >> extractresult.txt

# Ci-dessus, j'extrais chacune des lignes qui contient les termes tels que Driver, Bus etc du fichier
# resulteaglestat.txt et je les écris dans le fichier extractresult.txt



set recup [ open /tmp/extractresult.txt r ]
if { ![eof $recup] } {
global ligneluedriver;
global lignelue0;
global lignelue1;
global ligneluemac;
global lignelue2;
global lignelue3;
global lignelue4;
global lignelue5;
global lignelue6;
global lignelue7;
global lignelue8;
global lignelue9;
global lignelue10;
gets $recup ligneluedriver;
gets $recup lignelue0;
gets $recup lignelue1;
gets $recup ligneluemac;
gets $recup lignelue2;
gets $recup lignelue3;
gets $recup lignelue4;
gets $recup lignelue5;
gets $recup lignelue6;
gets $recup lignelue7;
gets $recup lignelue8;
gets $recup lignelue9;
gets $recup lignelue10;

# Ci-dessus, j'ouvre le fichier extractresult.txt en lecture et j'affecte son contenu à la variable recup.
# Je définis ensuite 12 variables globales, auxquelles j'affecte ensuite le contenu de chacune des lignes du
# fichier .

global paquetsreçus;
global paquetstransmis;
global paquetsrx;
global paquetstx;
global bus0;
global device0;
global dbg;
global interface; 
global mac;
global txrate;
global rxrate;
global crc;
global fec;
global margin;
global atten;
global vidcpe;
global vidco;
global hec;
global vpi;
global vci;
global delin;
global cellrx;
global celltx;
global oam;
global badvpi;
global badcrc;
global oversize;
global driver;
global modem


#set paquetstx 0;
#set paquetsrx 0;
set driver [ string range $ligneluedriver 0 19 ]
set bus0 [ string range $lignelue0 10 13 ]
set device0 [ string range $lignelue0 28 31 ]
set dbg [ string range $lignelue0 43 45 ]
set interface [ string range $lignelue1 21 29 ]
set mac [string range $ligneluemac 5 25 ]
set txrate [ string range $lignelue2 9 18 ]
set rxrate [ string range $lignelue2 30 39 ]
set crc [ string range $lignelue2 51 60 ]
set fec [ string range $lignelue3 9 18 ]
set margin [ string range $lignelue3 30 39 ]
set atten [ string range $lignelue3 51 60 ]
set vidcpe [ string range $lignelue4 9 18 ]
set vidco [ string range $lignelue4 30 39 ]
set hec [ string range $lignelue4 51 60 ]
set vpi [ string range $lignelue5 9 18 ]
set vci [ string range $lignelue5 30 39 ]
set delin [ string range $lignelue5 51 60 ]
set cellrx [ string range $lignelue6 9 18 ]
set celltx [ string range $lignelue6 30 39 ]
set oam [ string range $lignelue8 9 18 ]
set badvpi [ string range $lignelue8 30 39 ]
set badcrc [ string range $lignelue8 51 60 ]
set oversize [ string range $lignelue9 9 18 ]
set modem [ string range $lignelue10 0 20 ]

set paquetsreçus [ string range $lignelue7 9 18 ]
set paquetstransmis [ string range $lignelue7 30 39 ]

# Ci-dessus, je déclare des variables globales (paquetsreçus à modem) auxquelles j'affecte ensuite les caractères
# de tant à tant (la commande string range) contenus dans toutes les lignes ($lignelue0, $lignelue1 etc)


set paquetsrx [ string trimleft $paquetsreçus 0 ];
set paquetstx [ string trimleft $paquetstransmis 0 ]; }

# ci-dessus, la commande string trimleft me permet de supprimer les 0 éventuels
# présents à l'extrêmité gauche de la chaîne. Je ferme également mon if.

    set listepaquetsrx [list $paquetsrx]
    lindex $listepaquetsrx end 
    global nbrpktsrx
    set nbrpktsrx [lindex $listepaquetsrx end] 

# ci-dessus, je crée la liste listepaquetsrx à laquelle j'affecte le contenu de la variable $paquetsrx
# (paquets reçus).
# j'envoie à l'écran (lindex) le dernier terme (end) contenu dans cette liste
# je déclare une nouvelle variable et je lui affecte le dernier terme contenu dans $listepaquetsrx
# cette variable sera alors considérée comme une valeur numérique après être passée dans une liste.
# Ci-dessous, idem, mais pour les paquets envoyés

    set listepaquetstx [list $paquetstx]
    lindex $listepaquetstx end 
    global nbrpktstx
    set nbrpktstx [lindex $listepaquetstx end] 

# Idem ci-dessus


bus $driver $bus0 $device0 $dbg $interface $mac $txrate $rxrate $crc $fec $margin $atten $vidcpe $vidco $hec $vpi $vci $delin $cellrx $celltx $oam $badvpi $badcrc $oversize $modem;

# Ci-dessus, j'appelle la fonction bus en lui passant pour arguments les variables qui contiennent les valeurs numériques
# extraites des lignes lues


nombrepaquets $nbrpktsrx $nbrpktstx;

# J'appelle la procédure nombrepaquets et lui transmets les variables $nbrpktsrx, $nbrpktstx et $recupstat
# qu'elle prendra en argument, $recupstat étant la variable qui contient la commande /usr/local/sbin/eaglestat
# afin que la fonction nombrepaquets qui va ensuite rappeler la fonction majchamps puisse la lui retransmettre.


close $recup;

# Je ferme mon fichier ouvert en lecture



}


majchamps;



#pref recupstat;

# Ci-dessus, j'appelle la fonction pref en lui passant pour argument le terme recupstat de façon à ce qu'elle puisse
# traiter correctement l'appel (qu'elle renvoie la bonne commande, /usr/local/sbin/eaglestat sous forme de variable à la
# fonction majchamps).




#----------------------------------Création du bouton quitter ----------------------------------------------------------

label .maframe.rien -width 36 -text ""
grid .maframe.rien 
#-row 1 -column 4 -ipady 10

button .maframe.quitter -width 15 -height 1 -text "OK" -command {exit}
grid .maframe.quitter 
#-row 6 -column 3 
#pack configure .maframe.quitter -pady 30








