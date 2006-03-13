#!/bin/bash
#EagleConnect.tcl version 0.9  Copyright (C) 2004 Emmanuel Yves
#Ce programme est libre, vous pouvez le redistribuer et /ou le modifier selon les termes
#de la Licence Publique G�n�rale GNU  publi�e par la Free Software Foundation (version 2 
#ou bien toute autre version ult�rieure choisie par vous). 
#Ce programme est distribu� car potentiellement utile, mais SANS AUCUNE GARANTIE, 
#ni explicite ni implicite, y compris les garanties de commercialisation ou d'adaptation 
#dans un but sp�cifique. Reportez-vous � la Licence Publique G�n�rale GNU pour plus de d�tails.
#Vous devez avoir re�u une copie de la Licence Publique G�n�rale GNU en m�me temps que ce programme ;
#si ce n'est pas le cas, �crivez � la Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
#MA 02111-1307, �tats-Unis.
#Pour de plus amples informations, consultez la documentation jointe � ce programme.


set SBIN_DIR @SBIN_DIR@
set EU_DIR @EU_DIR@
set EU_EAGLECONNECT_DIR @EU_EAGLECONNECT_DIR@

#set SBIN_DIR /usr/sbin
#set EU_DIR /etc/eagle-usb
#set EU_EAGLECONNECT_DIR /etc/eagle-usb/eagleconnect



# La ligne suivante est executee par sh, pas par Tcl \
exec wish "$0" ${1+"$@"}




package names
package require msgcat;


msgcat::mclocale $env(LANG);
#::msgcat::mclocale "en_EN"
msgcat::mcload /etc/eagle-usb/eagleconnect/lang


#ci-dessus, j'appelle le paquetage msgcat qui va me permettre d'afficher les messages
#de l'interface dans la langue du syst�me de l'utilisateur (d�finie dans $env(LANG)).
#la commande msgcat::mcload [pwd] permet justement de charger les catalogues propres �
#chaque langue (en.msg si LANG = en_EN, fr.msg si LANG = fr_FR etc...)
# avec ::msgcat::mclocale "en_EN" ci-dessus, j'essaie en environnement en_EN 
# pour voir si le catalogue de traduction anglais est correctement appel�

set env(PATH) /bin:/sbin:/usr/bin:/usr/sbin:/usr/X11R6/bin:/usr/local/sbin;
# je d�finis les chemins o� peuvent se situer les ex�cutables



option add *font {arial 12}
# Police par d�faut : Arial 12




wm maxsize . 500 550
wm title . {EagleConnect}
wm geometry . 350x360+400+200
# Je cr�e mon interface. Sur l'�cran, elle se situe � l'emplacement +400 + 200
wm deiconify .
# Si l'interface �tait iconifi�e, je la d�iconifie


set login [exec whoami]


#------------------------------------------------------------------------------------------------------------
#----------------------LECTURE FICHIER PREFERENCES POUR LES FONTES -----------------------------------------
#----------------------------------------------------------------------------------------------------------



cd $EU_EAGLECONNECT_DIR
	set verifpresencepref [file exists $EU_DIR/eagleconnect.conf]
	if {$verifpresencepref > 0} {

set contenupreferences [open $EU_DIR/eagleconnect.conf r];
# j'ouvre le fichier eagleconnect.conf en lecture et j'affecte son contenu � la variable $contenupreferences

    if { ![eof $contenupreferences] } {
        
    gets $contenupreferences bouton;
    gets $contenupreferences menu;
    gets $contenupreferences champ;
    gets $contenupreferences etiq;

    # ci-dessous, j'affecte � la variable B(0) les termes de la ligne bouton compris entre les caract�res 7 � la fin
    # Je fais de m�me pour menu B(1), champ B(2) et etiq B(3)
    # je cr�e ensuite une bouvle for, pour i allant de 0 � 3, avec incr�mentation de i d'un pas 1, 
    # et j'affecte � la variable "donnees"  chaque portion des variables B s�par�e par le signe - (-family arial -size 14 etc)
    # j'ajoute ensuite chaque ligne lue  � une liste (num�rot�e �galement de 0 � 3)
    # et j'affecte chaque portion de chacune des listes (end -5 = family arial) (end -4 = size 14 etc...) � une variable recup
    # (recupfamily de 0 � 3 pour chaque B, qui r�cup�re family arial par exemple, recupsize de 0 � 3 pour chaque B, qui r�cup�re
    # size 14 etc.. Par exemple : $recupfamily(0) = family arial $recupsize(0) = size 14, $recupfamily(1) = family Tahoma etc...
    # ensuite, j'extrais de chacune de ces variables recup les termes en trop : family, zize, slant etc... pour ne garder que les
    # valeurs qui m"int�ressent : arial, 13, bold etc ... 
    # A noter : si souligner = 1, alors il faut indiquer true et non pas 1 lors de l'application de la police
    # Enfin, je cr�e mes polices (exemple : font create BBouton -family $family(0) ...) dont le nom (ici BBouton) sera ins�r� 
    # apr�s les commandes -font (des boutons par exemple).
 
    
    set B(0) [string range $bouton 7 end]
    set B(1) [string range $menu 5 end]
    set B(2) [string range $champ 5 end]
    set B(3) [string range $etiq 5 end]
   
       
    set liste(0) {}
    for {set i 0} {$i < 4} {incr i 1} {
        
          set donnees($i) [split $B($i) \-]
	  foreach line $donnees($i) {
                  
          lappend liste($i) $line
          set recupfamily($i) [lindex $liste($i) end-5]
          set recupsize($i) [lindex $liste($i) end-4]
          set recupweight($i) [lindex $liste($i) end-3] 
          set recupslant($i) [lindex $liste($i) end-2]
          set recupunderline($i) [lindex $liste($i) end-1]

          set family($i) [string range $recupfamily($i) 6 end-1]
          set size($i) [string range $recupsize($i) 5 end-1]
          set weight($i) [string range $recupweight($i) 7 end-1]
          set slant($i) [string range $recupslant($i) 6 end-1]
          set underlined($i) [string range $recupunderline($i) 10 end-1]
          
          if {$underlined($i) == 1} {set underline($i) true} else {set underline($i) false}

        }}
    

    # j'affecte � la variable recupfamily le contenu de la liste, depuis la fin (ligne 0) jusqu'� la ligne 5 (-family arial etc...)
    # j'affecte � la variable family le contenu de la variable recupfamily du caract�re 6 � fin -1 (par exemple : arial)
  
    font create BBouton -family $family(0) -size $size(0) -weight $weight(0) -slant $slant(0) -underline $underline(0)
    font create Bmenu -family $family(1) -size $size(1) -weight $weight(1) -slant $slant(1) -underline $underline(1)
    font create Bchamp -family $family(2) -size $size(2) -weight $weight(2) -slant $slant(2) -underline $underline(2)
    font create Betiq -family $family(3) -size $size(3) -weight $weight(3) -slant $slant(3) -underline $underline(3)
  
}
close $contenupreferences;

} else {cd $EU_EAGLECONNECT_DIR; open "| wish parameagleconnect.tcl" "r";} 




#-----------------------------------------------------------------------------------------------------------------
#---------------------------------------------CREATION DU MENU------------------------------------------------------
#---------------------------------------------------------------------------------------------------------------------

# Je cr�e une frame (barredemenu) avec un relief incrust�.
# Je fais appara�tre ma frame en lui faisant remplir tout
# l'espace horizontal 

frame .barredemenu -relief sunken -bd 1 -background #4a4a4a
pack .barredemenu -side top -expand no -fill x


# Je cr�e ma barre de menu, la commande [msgcat::mc + string ] me permet d'appeller le catalogue
# de la langue (fr.msg si LANG est fr_FR, en.msg si LANG est en_EN) afin de traduire les termes entre
# guillements dans la langue du syst�me de l'utilisateur.

menubutton .barredemenu.lien -text [msgcat::mc "Help"] -font [list Bmenu] -menu .barredemenu.lien.menu


# Je positionne mon menu � droite.

pack .barredemenu.lien -side left


#--------------------------------------------------------------------------------------------------------------------
#----------------------------------------CREATION DU BOUTON QUI APPELLE L'APPLET ------------------------------------
#--------------------------------------------------------------------------------------------------------------------

 
button .barredemenu.planquer -width 1 -height 0  -borderwidth 0 -background white -highlightcolor #4a4a4a -cursor top_right_corner -text \u25F3 -font [list BBouton] 
pack configure .barredemenu.planquer -side right -pady 0

# ci-dessus, je cr�e le bouton qui appelle l'applet. Il comprend un caract�re de la table unicode (code 25F33, voir liste � l'adresse http://www.unicode.org/charts/PDF/U2600.pdf) et le curseur
# de la souris se change en fl�che lorsqu'il pointe sur ce bouton.



#---------------------------------------------------------------------------------------------------------------------
#---------------------------------------------CREATION DU MENU AIDE --------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

#  Je cr�e mon menu "Aide" et les liens "Manuel de EagleConnect" et "A propos de EagleConnect".


menu .barredemenu.lien.menu
.barredemenu.lien.menu add command -label [msgcat::mc "EagleConnect Manual"] -font [list Betiq] -command { aide }
.barredemenu.lien.menu add separator
.barredemenu.lien.menu add command -label [msgcat::mc "Configure EagleConnect"] -font [list Betiq] -command {
cd $EU_EAGLECONNECT_DIR;
open "| wish parameagleconnect.tcl" "r"}
.barredemenu.lien.menu add separator
.barredemenu.lien.menu add command -label [msgcat::mc "About EagleConnect"] -font [list Betiq] -command { 
option add *Dialog.msg.font {Arial 12}; tk_messageBox -message [msgcat::mc "EagleConnect version 0.8 april 2004 \n\nEagleConnect is a graphical front-end for the Eagle driver and its utilities created by Beno�t Audouard, Olivier Borowski, St�phane Collet, J�r�me Marant and Fr�d�rick Ros in order to use Sagem fast 800 modem and assimilated under Linux.\n\nAuthor : Emmanuel YVES \nFirst created on 09/02/2004 \nCopyright (c) 2004 \n This program is under the terms of the GNU General Public License Version 2, June 1991, published by the Free Software Foundation"]        

 } 


#---------------------------------------------------------------------------------------------------------------------
#---------------------------------------------CREATION DES AUTRES FRAMES------------------------------------------------
#------------------------------------------------------------------------------------------------------------------------

frame .frame2 
pack .frame2 ;
frame .frame4 
pack .frame4;
frame .frame3
pack .frame3;
frame .maframe 
pack .maframe;


#--------------------------------------------------------------------------------------------------------------------
# --------------------------------------- PROCEDURE AIDE APPELEE PAR LE MENU-------------------------------------------
#------------------------------------------------------------------------------------------------------------------------



proc aide {} { 


toplevel .window; wm maxsize .window 800 500; wm title .window {Aide}; 
wm geometry .window 800x500+200+200; frame .window.fenetre; pack .window.fenetre;


text .window.fenetre.texteaide -height 500 -width 800  \
-background white -font {Fixed 12} -wrap word -yscrollcommand ".window.fenetre.scroll set";

# je cr�e l'ascenseur  
scrollbar .window.fenetre.scroll -command ".window.fenetre.texteaide yview";

# je rends l'ascenseur visible 
pack .window.fenetre.scroll -side right -fill y; 

# je rends mon widget text visible
pack .window.fenetre.texteaide -expand yes -fill both


# J'ouvre la documentation (format texte) en lecture et j'affecte son contenu � la 
# variable "eagleconnectdocumentation"
set eagleconnectdocumentation [ open  /etc/eagle-usb/eagleconnect/lang/doc_fr.txt r ]


while { ![eof $eagleconnectdocumentation] } {

# Je lis ligne contenue dans la variable "$eagleconnectdocumentation"
# et je l'affecte � la variable "doclignelue"
gets $eagleconnectdocumentation doclignelue;

# J'ins�re les lignes lues dans le widget texte "texteaide" (end = le caract�re juste 
# apr�s le dernier saut de ligne)
.window.fenetre.texteaide insert end $doclignelue

# J'ins�re un retour chariot (\n) apr�s chaque ligne 
.window.fenetre.texteaide insert end \n;}

#Je ferme la variable "eagleconnectdocumentation"
close $eagleconnectdocumentation;


}









#----------------------------------------------------------------------------------------------------------------------
#------------------------------PROC�DURE DE DETECTION DE CONNEXION BASEE SUR EAGLEDIAG---------------------------------
#------------------------------------------------------------------------------------------------------------------



# Ci-dessous, la fonction detecviaeaglediag re�oit en argument la commande /usr/local/sbin/eaglediag pass�e au travers
# d'une variable (recupdiag) par la fonction pref (voir plus haut), ainsi que le terme "interval" ou "unefois" lui
# permettant de savoir si elle doit relancer la fonction lancediaginterval apr�s un certain laps de temps.
# J'efface l'�ventuel fichier sortieeaglediag.txt qui pourrait exister.
# J'ex�cute Eaglediag ($recupdiag) en redirigeant le r�sultat vers le fichier sortieeaglediag.txt
# J'ouvre le fichier sortieeaglediag.txt en lecture et en �criture (w+) afin que les �ventuelles donn�es puissent �tre
# transmises (notamment, le r�sultat du ping d'Eaglediag). En effet, en mettant le fichier en lecture 
# seule ([ open /home/$login/eagleconnect/sortieeaglediag.txt r ]) le fichier ne recevrait pas les infos � temps (KO � la place de OK)..
# J'affecte le  contenu de sortieeaglediag.txt � la variable $recuprechdiag dont je lis
# le contenu depuis le d�but (seek $recuprechdiag 0 start). J'affecte les lignes lues � la variable texte, et la valeur de la recherche
# sur la cha�ne de caract�re "KO" dans le fichier � la variable trouve. Si "KO" n'a pas �t� trouv� (si $trouve == -1), alors
# tout est OK dans le r�sultat de EagleDiag et je sais que la connexion est active. Sinon, si cette cha�ne a �t� trouv�e, alors
# je sais qu'il y a un probl�me dans la connexion et qu'elle n'est pas active (j'affiche son statut dans le champ .maframe.appel).


proc detectviaeaglediag {frequence} {

global identifiant
set identifiant [exec whoami]

global SBIN_DIR


proc changebouton {} {
.maframe.connecter configure -text [msgcat::mc "Connect"]}



if {$frequence == "interval"} {
 cd /tmp
 file delete sortieeaglediag.txt;
 if {$identifiant == "root"} {exec $SBIN_DIR/eaglediag > /tmp/sortieeaglediag.txt} else { 
 exec sudo $SBIN_DIR/eaglediag > /tmp/sortieeaglediag.txt
  }
 set recuprechdiag [ open /tmp/sortieeaglediag.txt r ];
      while { ![eof $recuprechdiag] } {
       
        seek $recuprechdiag 0 start
        set texte [read $recuprechdiag]
        set trouve [string first "KO" $texte]
        if {$trouve == -1} { 
           .maframe.appel delete 0 50;
           .maframe.appel insert 0 [msgcat::mc "ADSL connection is active"];} else {puts "erreur1"; tk_messageBox -message [msgcat::mc "An error has occured. ADSL connection couldn't be established"]; .maframe.appel delete 0 50; .maframe.appel insert 0 [msgcat::mc "ADSL connection isn't active"];
                                   } 
                                      }
      
      close $recuprechdiag;
      after 300000 lancediaginterval;
     # puts "toutes les 5 minutes";



     } elseif {$frequence == "unefois"} {
	
 cd /tmp;
 file delete sortieeaglediag.txt;

  if {$identifiant == "root"} { exec $SBIN_DIR/eaglediag > /tmp/sortieeaglediag.txt} else {
 exec sudo $SBIN_DIR/eaglediag > /tmp/sortieeaglediag.txt
   }
 set recuprechdiag [ open /tmp/sortieeaglediag.txt r ];
      while { ![eof $recuprechdiag] } {
       
        seek $recuprechdiag 0 start
        set texte [read $recuprechdiag]
        set trouve [string first "KO" $texte]
        if {$trouve == -1} { 
           .maframe.appel delete 0 50;
           .maframe.appel insert 0 [msgcat::mc "ADSL connection is active"];} else {
	    # puts "erreur2"; 
	   tk_messageBox -message [msgcat::mc "An error has occured. ADSL connection couldn't be established"]; 
           .maframe.appel delete 0 50;
           .maframe.appel insert 0 [msgcat::mc "ADSL connection isn't active"];
	   changebouton;
                           } 
                                      }
      
      close $recuprechdiag;} else {
      cd /tmp;
      file delete sortieeaglediag.txt;

      if {$identifiant == "root"} { exec $SBIN_DIR/eaglediag > /tmp/sortieeaglediag.txt} else {
      exec sudo $SBIN_DIR/eaglediag > /tmp/sortieeaglediag.txt
      }
      set recuprechdiag [ open /tmp/sortieeaglediag.txt r ];
      while { ![eof $recuprechdiag] } {
       
        seek $recuprechdiag 0 start
        set texte [read $recuprechdiag]
        set trouve [string first "KO" $texte]
        if {$trouve == -1} { 
           .maframe.appel delete 0 50;
           .maframe.appel insert 0 [msgcat::mc "ADSL connection is active"];} else {puts "erreur3"; tk_messageBox -message [msgcat::mc "An error has occured. ADSL connection couldn't be established"]; 
           .maframe.appel delete 0 50;
           .maframe.appel insert 0 [msgcat::mc "ADSL connection isn't active"];
                           } 
                                      }
      
      close $recuprechdiag;}}





  






#-----------------------------------------------------------------------------------------------------------------
#-------------------------- CREATION DU CHAMP STATUT DE LA CONNEXION ------------------------------------------------
#--------------------------------------------------------------------------------------------------------------------


# Je construis le widget "appel" dans lequel appara�tra le statut de la connexion (actif / inactif)


  entry .maframe.appel -background white -justify center -font [list BBouton]\
 -textvariable statutconnexion \
 -width 35; \
  pack configure .maframe.appel -pady 15


 
#--------------------------------------------------------------------------------------------------------------------
#----------------------------------PARTIE EXECUTION D'EAGLESTAT------------------------------------------------------------
#---------------------------------------------------------------------------------------------------------------------





button .maframe.modem -width 30 -text [msgcat::mc "Network state"] -font [list BBouton] -command {

	if {$identifiant == "root"} {
		cd $EU_EAGLECONNECT_DIR; open "| wish reseau.tcl" "r"} else {
		cd $EU_EAGLECONNECT_DIR; open "| wish reseau.tcl" "r"}
												  }

# ci-dessus, j'ex�cute la commande "wish reseau.tcl" en mode asynchrone gr�ce � la commande open "| (et non plus exec) de
# fa�on � conserver la main sur EagleConnect.tcl (impossible autrement)

pack configure .maframe.modem -pady 5        



#-----------------------------------------------------------------------------------------------------------------
#------------------------------------- CREATION DU BOUTON DIAGNOSTIC DU SYSTEME ------------------------------------
#--------------------------------------------------------------------------------------------------------------------


cd /tmp
# je vais dans le r�pertoire perso de l'utilisateur
file delete resultat.txt;
# j'efface le fichier resultat.txt au pr�lable

# Cr�ation du bouton de diagnostic du syst�me effectu� par Eaglediag


button .maframe.diagnostic -width 30 -text [msgcat::mc "System diagnosis"] -font [list BBouton] -command {
	if {$identifiant == "root"} {
		cd $EU_EAGLECONNECT_DIR; open "| wish diagnostic.tcl" "r"} else {
		cd $EU_EAGLECONNECT_DIR; open "| wish diagnostic.tcl" "r"}
				     }



 
pack configure .maframe.diagnostic -pady 5 
# je cr�e enfin mon bouton diagnostic



#------------------------------------------------------------------------------------------------------------------------
#--------------------------------------CREATION DU BOUTON REINITIALISER ------------------------------------------------
#-----------------------------------------------------------------------------------------------------------------------

# Je cr�e ici le bouton "r�initialiser" (qui appartient � la frame "maframe" qui elle-m�me appartient
# � la fen�tre principale) avec lequel je stoppe aussi la connexion adsl. J'affiche  ensuite dans le widget entry d�fini
# plus haut le statut de la connexion.


#Changement apport� le 27/03/2004 : pour la r�initialisation, je vais chercher la commande
#correspondante dans le fichier de sauvegarde des pr�f�rences "eagleconnect.pref" que j'ouvre en lecture (r).
#j'affecte son contenu � la variable contenupreferences que je lis ligne par ligne. J'affecte le 
#contenu des lignes lues (en fait, les commandes d�finies par l'utilisateur via le panneau de configuration
#pour lancer, stoppper la connexion, r�initialiser le modem etc...) � des variables ($recupconnect pour
#fctStartAdsl, $recupdeconnect pour fctStopadsl etc ...) que j'appelle ensuite via ma commande :
#par exemple : exec sudo $recupconnect  pour lancer la connexion, exec sudo $recupdeconnect pour l'arr�ter,
#etc .... Il s'agit du m�me code utilis� pour les boutons "connecter" et "d�connecter".

button .maframe.reinitialiser -width 30 -text [msgcat::mc "Modem reset"] -font [list BBouton] -command {



proc reinitial {} {

	global identifiant;
	set identifiant [exec whoami];
	global SBIN_DIR

		if {$identifiant == "root"} {
			exec $SBIN_DIR/fctStopAdsl -sf; catch {exec $SBIN_DIR/eaglectrl -w;} erreur2;} else {
			exec sudo $SBIN_DIR/fctStopAdsl -sf; catch {exec sudo $SBIN_DIR/eaglectrl -w;} erreur2; 
					      }

	set statutconnexion [msgcat::mc "ADSL connection isn't active"]; tk_messageBox -message [msgcat::mc "You can launch ADSL connection"];
		    }

reinitial

}
pack configure .maframe.reinitialiser -pady 5


#--------------------------------------------------------------------------------------------------------------------------
# -----------------------------------------CREATION DU BOUTON CONNECTER / DECONNECTER -------------------------------------
#----------------------------------------------------------------------------------------------------------------------




button .maframe.connecter -width 30 -font [list BBouton] -text [msgcat::mc "Connect"] -command {

set valeurtextbouton [.maframe.appel get]
puts $valeurtextbouton
puts $valeurtextbouton


if {$valeurtextbouton == "ADSL connection isn't active"} {


# ci-dessus, je cr�e mon bouton connecter. Il a pour texte "Lancer la connexion ADSL" que la commande msgcat peut traduire
# dans la langue de l'environnement utilisateur (en anglais si anglais, etc ... voir les fichier fr.msg et en.msg). Ce bouton 
# lance les commandes suivantes :  
 

 

#---- ---------------Ci-dessous, fonctions de d�tection de fin de processus (ici, fctStartaAdsl)
#---pour une meilleure compr�hension, lire dans l'ordre les fonction connect, puis test1, puis test2---------------------


#----Ci-dessous, dans ma fonction test2, je re�ois pour arguments les 3 variables pass�es par la fonction test1.
#----Si le contenu de la variable $recuperreur est 0 (voir plus bas ce qu'est $recuperreur), alors je sais que le processus
#----que le processus fctStartAdsl est en cours de fonctionnement et je rappelle donc la fonction test1 � laquelle je repasse
#----les variables qui n'ont pas chang� ($numero, en fait le pid du process, et canal, l'identifiant du canal de communication
#----ouvert sur le processus fctStartAdsl, voir � ce sujet la fonction connect, plus bas).
#----Sinon, alors $recuperreur contient la valeur 1, alors par cons�quent je sais que le processus fctSartAdsl est termin�
#----et je ferme le canal de communication ($canal) ouvert sur le processus � pr�sent disparu.



proc test2 {recuperreur numero canal} {
if {$recuperreur == 0} {
 #puts "fctStartAdsl est en cours de fonctionnement"; 
 test1 $numero $canal;} else {
 close $canal; 
 puts "le process est termin�";
 after 4000 detectviaeaglediag interval;} }



#----Ci-dessous, dans ma fonction test1, je re�ois pour arguments les variables pass�es par la fonction connect (voir plus bas)
#----Avec la commande de capture d'erreur catch, je mets dans la variable erreur1 le contenu de la commande exec ps -a | grep $numero 
#----o� je filtre le numero pid (variable numero, pid du process fctStartAdsl) dans la commande ps -a. J'ai donc dans ma variable 
#----erreur1 un ligne telle que : "6677 pts/1    00:00:00 fctStartAdsl" lorsque le processus est en cours de fonctionnement.
#----Or, lorsque ce dernier ne l'est plus, le contenu de la variable erreur1 change, nous obtenons en effect ceci :
#----"6677 pts/1    00:00:00 fctStartAdsl <defunct>" . Nous voyons bien ici le terme <defunct> rajout�, qui nous indique que
#----le process n'est plus en cours de r�alisation. Donc, pour conna�tre la fin d'un processus, nous devons rechercher la cha�ne 
#----"defunct" dans la variable $erreur1 : "string match "*defunct*" $erreur1" et nous l'affectons ensuite � la variable 
#----$recuperreur (si string match trouve "defunct" dans $erreur1 - donc, le processus fctStartAdsl s'est arr�t� -, il renvoie 1, 
#----sinon, il renvoie 0 et donc fctStartAdsl est toujours en cours de fonctionnement).
#---- J'envoie ensuite � la proc�dure test2 mes variables $recuperreur, $numero et $canal.



proc test1 {numero canal} {
catch {exec ps -a | grep $numero} erreur1;
#puts $erreur1;
set recuperreur [string match "*defunct*" $erreur1];
test2 $recuperreur $numero $canal}


#---- Ci-dessous, dans ma premi�re fonction connect, je re�ois pour argument la valeur que m'a renvoy�e la proc�dure pref,
#-----� savoir la commande /usr/local/sbin/fctStartAdsl contenue dans le fichier des pr�f�rences utilisateur.
#-----J'affecte le contenu de la variable x � la variable recupconnect
#-----J'�tablis une communication (open "| ") avec le processus sudo $recupconnect (en fait, sudo fctstartAdsl). Le canal est ouvert
#-----en lecture ("r"). J'identifie le processus gr�ce � la commande pid et j'affecte son num�ro � la variable numero.
#-----J'appelle ensuite la fonction test1 � laquelle je passe en arguments les variables $numero et $canal.




proc connect {} {

	global identifiant
	set identifiant [exec whoami]
	global SBIN_DIR
	global canal1;
	global numero;

		if {$identifiant == "root"} {
			set canal [open "| $SBIN_DIR/fctStartAdsl" "r"]; .maframe.connecter configure -text [msgcat::mc "Disconnect"]} else {
			set canal [open "| sudo $SBIN_DIR/fctStartAdsl" "r"]; .maframe.connecter configure -text [msgcat::mc "Disconnect"]
					    }
	set numero [pid $canal];
	test1 $numero $canal; 
		}

connect
# ci-dessous, fin de mon premier if (si connexion adsl n'est pas active)

} else { 


set identifiant [exec whoami]
 
    global identifiant
    set identifiant [exec whoami]
    
       if {$identifiant == "root"} {exec $SBIN_DIR/fctStopAdsl -sf; .maframe.appel delete 0 50; .maframe.appel insert 0 [msgcat::mc "ADSL connection isn't active"]; .maframe.connecter configure -text [msgcat::mc "Connect"]} else {
        exec sudo $SBIN_DIR/fctStopAdsl -sf; .maframe.appel delete 0 50; .maframe.appel insert 0 [msgcat::mc "ADSL connection isn't active"]; .maframe.connecter configure -text [msgcat::mc "Connect"]}} 

   

}


set valeurtextbouton [.maframe.appel get]
if {$valeurtextbouton == "ADSL connection isn't active"} {
.maframe.connecter configure -text [msgcat::mc "Connect"];
pack configure .maframe.connecter -pady 5; } else {
.maframe.connecter configure -text [msgcat::mc "Disconnect"];
pack configure .maframe.connecter -pady 5 } 




#ci-dessus, je fais un test : si la commande de d�connexion entr�e par l'utilisateur
#  dans le fichier eagleconnect.pref est effectivement "/usr/local/sbin/fctStopAdsl" , 
#alors je passe en argument � cette derni�re 2 options, f et s, f pour forcer la fermeture
#  de pppd, s pour utiliser ifconfig au lieu de ifup et ifdown.
#Sinon, si la commande n'est pas "/usr/local/sbin/fctStopAdsl" pour se d�connecter, alors
#  j'ex�cute simplement la commande entr�e par l'utilisateur.  a noter cependant que 
#l'utilisateur doit au pr�alable entrer la commande dans le fichier /etc/sudoers pour lancer 
#  les op�rations de connexion/d�connexion etc ... en tant que simple utilisateur.
#Exemple dans mon fichier /etc/sudoers :
# emmanuel ALL=NOPASSWD:/usr/local/sbin/fctStartAdsl
#emmanuel ALL=NOPASSWD:/usr/local/sbin/fctStopAdsl -sf



 



#----------------------------------------------------------------------------------------------------------------------
#------------------------------------CREATION DU BOUTON QUITTER --------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

button .maframe.quitter -width 15 -height 2 -text [msgcat::mc "Exit"] -font [list BBouton] -command {cd /tmp; foreach fichierstxt [glob -nocomplain -dir /tmp *.txt *.tcl~] {file delete $fichierstxt;}; exit;}
#button .maframe.quitter -width 15 -height 2 -text [msgcat::mc "Exit"] -font [list BBouton] -command {exec ksystraycmd --window EagleConnect}
# wm withdraw .}
# cd /home/$login/.eagleconnect; foreach fichierstxt [glob -nocomplain -dir /home/$login/eagleconnect10 *.txt *.tcl~] {file delete $fichierstxt;}; exit;}
pack configure .maframe.quitter -pady 25






#-----------------------------------------------------------------------------------------------------------------------
#------------------------VERIFICATION DE L'INCREMENTATION DES PAQUETS RECUS POUR L'APPLET------------------
#------------------------------------------------------------------------------------------------------------------------


# Ma fonction comparepaquets re�oit 3 arguments, x  y et recupstat, envoy�s par la fonction envoi2.
# Y �tant le nombre de paquets re�us avant, et X le nombre de paquets re�us apr�s.
# X �tant � l'origine une chaine string, je la mets dans une liste (commande list), ce qui 
# a la particularit� de la changer en valeur num�rique. Je r�cup�re ensuite le dernier �l�ment de la liste
# (commande lindex ... end), et je l'affecte � la variable $rxsuiv. Si $rxsuiv est > � Y (cr�� � la base
# par la proc�dure envoi1 qui la passe en argument � la fonction envoi2, qui la convertit
# elle-m�me en valeur num�rique) alors cela signifie que l'incr�mentation a bien eu lieu
# et donc que la connexion est active.
# La variable recupstat re�ue en argument par ma fonction contient la commande /usr/local/sbin/eaglestat.
# Je renvoie cette variable � la fonction de d�part, envoi1.


# proc comparepaquets {x y recupstat} {
#  set maliste2 [list $x]
#  lindex $maliste2 end 
#  global rxsuiv
#  set rxsuiv [lindex $maliste2 end]
#  if { $rxsuiv > $y } { .maframe.appel delete 0 50; .maframe.appel insert 0 [msgcat::mc "ADSL connection is active"];
  #tk_messageBox -message [msgcat::mc "La connexion ADSL est active"]; 
#   } else {
#    .maframe.appel delete 0 50; .maframe.appel insert 0 [msgcat::mc "ADSL connection isn't active"]; 
  # tk_messageBox -message [msgcat::mc "La connexion ADSL n'est pas active"];
  # puts $rxsuiv; puts $y;
#   } 
#  envoi1 $recupstat; 

#}

  
# La fonction envoi2 re�oit en argument le nombre de paquets re�us (voir plus bas) et ins�re le 
# contenu de la variable pass�e dans une liste. Je r�cup�re le dernier terme contenu de la liste 
# (en fait, le seul d'ailleurs), et je l'affecte � la variable $abc. Note : le fait de passer une variable
# "string" dans une liste la fait passer comme valeur num�rique ensuite, ce qui permet d'effectuer
# des op�rations dessus.
# Je r�alise une nouvelle commande eaglestat � partir de laquelle
# j'extrais la valeur du nombre paquets re�us. J'envoie alors le nombre de paquets pr�c�dents ($abc) et le
# nombre de paquets obtenus  comme argument � la fonction comparepaquets.


  proc envoi2 {x} {
  global SBIN_DIR


  set maliste2 [list $x]
  lindex $maliste2 end 
  global abc
  set abc [lindex $maliste2 end]

  cd /tmp; 
  file delete sortieeaglestat.txt; 
    
    exec $SBIN_DIR/eaglestat >> /tmp/sortieeaglestat.txt;
    cd /tmp;
    file delete extractsortie.txt;
    exec grep Pkts sortieeaglestat.txt >> extractsortie.txt;
    set recup [ open /tmp/extractsortie.txt r ]
    if { ![eof $recup] } {
    global lignelue
    gets $recup lignelue;
    global paquetsre�us;
    global paquetsrxnum;
    set paquetsre�us [ string range $lignelue 9 18 ]
    set paquetsrxnum [ string trimleft $paquetsre�us 0 ]; 
    envoi1; 

   # comparepaquets $paquetsrxnum $abc $recupstat}
  
  close $recup
  }


# code ex�cut� au d�marrage du logiciel. Via Eaglestat, je r�cup�re le nombre de paquets re�us
# apr�s un ping et je l'affecte � la variable $paquetsrxnum que j'envoie en argument � la proc�dure 
# envoi (pour plus de d�tails sur le code voir le bouton "connecter").
     
  
  proc envoi1 {} {
  global SBIN_DIR
 
  cd /tmp
  #je passe dans le r�pertoire /tmp
  file delete sortieeaglestat.txt
  # j'efface le fichier sortieeaglestat.txt s'il existe
  
  exec $SBIN_DIR/eaglestat >> /tmp/sortieeaglestat.txt
  # je lance eaglestat et je redirige son r�sultat vers le fichier sortieeaglestat.txt
  cd /tmp
  # je retourne dans /tmp
  file delete extractsortie.txt
  # j'efface le fichier extractsortie.txt
  exec grep Pkts sortieeaglestat.txt >> extractsortie.txt
  # je recherche le terme Pkts dans le fichier sortieeaglestat et je redirige le r�sultat
  # vers le fichier extractsortie.txt
  set recup [ open /tmp/extractsortie.txt r ]
  #j'ouvre le fichier extractsortie.txt en lecture et j'affecte son contenu � la variable recup
  if { ![eof $recup] } {
  # Si je n'ai pas atteint la fin du fichier
  global lignelue
  # je d�clare une variable globale, lignelue
  gets $recup lignelue;
  # je mets dans la variable lignelue le contenu de la ligne pr�sente dans $recup
  global paquetsre�us;
  # je d�clare une variable globale paquetsre�us
  global paquetsrxnum;
  # je d�clare une variable globale paquets rxnum
  global paquetsenvoyes;
  #je d�clare une variable globale paquetsenvoyes
  global paquetstxnum;
  # je d�clare une variable globale paquetstxnum


  set paquetsre�us [ string range $lignelue 9 18 ]
  # j'affecte � paquetsre�us les caract�res 9 � 18 de lignelue
  set paquetsenvoyes [ string range $lignelue 30 39 ]
  # j'affecte � paquetsenvoyes les caract�res 30 � 39 de lignelue
  set paquetsrxnum [ string trimleft $paquetsre�us 0 ];
  # j'affecte � paquetsrxnum le contenu de paquetsre�us dont j'ai extrait les 0 pr�sents � gauche
  set paquetstxnum [ string trimleft $paquetsenvoyes 0 ];
  # j'affecte � paquetstxnum le contenu de paquetsenvoyes dont j'ai extrait les 0 pr�sents � gauche
  
  
  

  
  after 600 envoi2 $paquetsrxnum  
  # j'envoie le contenu de la variable $paquetsrxnum et de $recupstat (la
  # commande eaglestat et son chemin) � la fonction envoi2

  set testexistapplet [winfo exists .icone]
  # je teste l'existence de l'applet (icone). Si elle existe, alors la valeur retourn�e est 1 et est
  # affect�e � la variable testexistapplet. Sinon, O, elle n'existe pas, $testexistapplet prend cette
  # valeur
  
    
  if {$testexistapplet == 1} {majpkrx $paquetsrxnum; majpktx $paquetstxnum;} else {set neprendrien 1;};
  # Si l'applet existe, alors :
  
  # j'envoie la valeur de la variable $paquetsrxnum � la fonction majpkrx (de l'applet)
  # qui indique ainsi le nombre de paquets re�us (voir plus haut)
  
  # j'envoie la valeur de la variable $paquetstxnum � la fonction majpktx (de l'applet)
  # qui indique ainsi le nombre de paquets transmis (voir plus haut)
  

  


 }
  close $recup }


proc envoi0 {} {
   envoi1 ;
}
# Ma fonction envoi0 recup�re ici la variable �mise par la fonction pref contenant la commande /usr/local/sbin/eaglestat.
# La fonction 0 renvoie cette variable � la fonction envoi1


  
# au bout de dix secondes, apr�s le lancement de EagleConnect, j'appelle la fonction pref en lui passant pour
# argument le mot recupstat afin que la bonne commande soit r�cup�r�e (/usr/local/sbin/eaglestat) et qu'elle soit
# retransmise sous forme de variable � la fonction envoi0.
  
 


#---------------------------------------------------------------------------------------------------------
#---------------------------ACTIONNEMENT DU BOUTON QUI APPELLE L'APPLET-----------------------------------
#---------------------------------------------------------------------------------------------------------

.barredemenu.planquer configure -command {

envoi0;

destroy .icone
#ci dessus, je d�truis l'�ventuelle icone qui pourrait r�sider en m�moire afin de ne pas
# avoir un message d'erreur lorsqu'en cliquant sur le bouton je rappellerai cette derni�re
wm withdraw .
#ci dessus, je fais dispara�tre le menu d'Eagleconnect (la fen�tre principale)


#----------------------------------------------------------------------------------------------------------------------
# ---------------------------------------------- CREATION DE L'APPLET ------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

toplevel .icone; wm maxsize .icone 100 100; 

# je cr�e mon ic�ne


wm protocol .icone WM_DELETE_WINDOW {#}
# ci-dessus, j'emp�che la fermeture de l'icone lorsqu'on clique sur le x



wm geometry .icone 100x100+10+10; frame .icone.frameicone -relief sunken -bd 1; 


label .icone.frameicone.etiq -width 16 -height 1 -relief sunken -bd 1 -background #f0ffff -text "EagleConnect" -font [list Arial 8];
grid  .icone.frameicone.etiq -row 0 -column 0 -ipadx 0 -ipady 0 -sticky w


# ci-dessous, cr�ation du champ indiquant le statut de la connnexion
entry .icone.frameicone.champ -background gray90 -justify center -font [list Arial 8] \
 -textvariable etatconnexion \
 -width 14; \
grid .icone.frameicone.champ -row 2 -column 0 -padx 1 -pady 0 -sticky w



# ci-dessous, cr�ation du canvas qui contiendra les champs de pktx et pkrx
canvas .icone.frameicone.canvasicone -width 60 -height 40 -background gray90
grid .icone.frameicone.canvasicone -row 1 -column 0 -padx 2 -pady 2

# Si la variable du champ $statutconnexion dans EagleConnect contient "La connexion est active", alors
# le champ de l'applet (.icone.frameicone.champ) contindra �galement Connextion Ok. Sinon, ce sera
# l'inverse dans le cas contraire (note : j'efface au pr�alable le contenu de .icone.frameicone.champ pour
# qu'il soit mis � jour).
 
if {$statutconnexion == [msgcat::mc "ADSL connection is active"]} {.icone.frameicone.champ delete 0 50; .icone.frameicone.champ insert 0 [msgcat::mc "Connection OK"]} else {.icone.frameicone.champ delete 0 50; .icone.frameicone.champ insert 0 [msgcat::mc "No connection"]}; 


label .icone.frameicone.canvasicone.labelrx -text [msgcat::mc "Receive"] -width 6 -font [list Arial 8] 
grid .icone.frameicone.canvasicone.labelrx -row 0 -column 0 -padx 0 -pady 0 -sticky w
label .icone.frameicone.canvasicone.labeltx -text [msgcat::mc "Sent"] -width 6 -font [list Arial 8]
grid .icone.frameicone.canvasicone.labeltx -row 0 -column 1 -padx 0 -pady 0 -sticky w



# ci-dessous, cr�ation du champ indiquant le nombre de paquets entrants

entry .icone.frameicone.canvasicone.pkrx -background white -justify right -font [list Arial 8] \
 -textvariable pkrx \
 -width 6; \
grid .icone.frameicone.canvasicone.pkrx -row 1 -column 0 -padx 0 -pady 0 -sticky w


# ci-dessous, cr�ation du champ indiquant le nombre de paquets sortants
entry .icone.frameicone.canvasicone.pktx -background white -justify right -font [list Arial 8] \
 -textvariable pktx \
 -width 6; \
grid .icone.frameicone.canvasicone.pktx -row 1 -column 1 -padx 0 -pady 0 -sticky w


# ci-dessous, �tiquette vide pour d�caler un peu les champs sur la gauche

label .icone.frameicone.canvasicone.labelvide
grid .icone.frameicone.canvasicone.labelvide -row 1 -column 2



proc majpkrx {a} {global rx; set rx $a; .icone.frameicone.canvasicone.pkrx delete 0 50;.icone.frameicone.canvasicone.pkrx insert 0 $rx;}
#ci-dessus, ma fonction majpkrx re�oit pour argument la valeur de $paquetsrxnum (nombre de
# paquets re�us) transmis toutes les 2 ou 3 minutes par le proc�dure Envoi1 (voir plus bas).
#Les valeurs re�ues ici apparaissent dans le champ .icone.frameicone.canvasicone.pkrx (voir plus haut)

proc majpktx {b} {global tx; set tx $b; .icone.frameicone.canvasicone.pktx delete 0 50;.icone.frameicone.canvasicone.pktx insert 0 $tx;} 
#ci-dessus, ma fonction majpktx re�oit pour argument la valeur de $paquetstxnum (nombre de
# paquets envoy�s) transmis toutes les 2 ou 3 minutes par le proc�dure Envoi1 (voir plus bas).
#Les valeurs re�ues ici apparaissent dans le champ .icone.frameicone.canvasicone.pktx (voir plus haut)


wm overrideredirect .icone 1
#ci-dessus, j'annihile l'appartenance au gestionnaire de fen�tre pour que l'objet apparaisse 
# d�solidaris�

bind all <Double-Button-1> {wm deiconify .; wm withdraw .icone}

# ci-dessus, un double clic gauche dans la fen�tre fait r�appara�tre le menu EagleConnect et dispara�tre l'applet

bind all <Button3-Motion> {set a [winfo pointerx .icone]; set b [winfo pointery .icone]; wm geometry .icone 80x80+$a+$b}
#ci-dessus, en appuyant sur le bouton droit de la souris et en faisant glisser cette derni�re
#on bouge l'icone sur l'�cran

pack .icone.frameicone

    
}


#------------------------------------------------------------------------------------------------------------
#---------------- VERIFICATION DE L'ETAT DE LA CONNEXION A INTERVALLES REGULIERS VIA EAGLEDIAG---------------
#------------------------------------------------------------------------------------------------------------

proc lancediaginterval {} {
 detectviaeaglediag interval;
			   }


cd $EU_EAGLECONNECT_DIR
set verifpresencepref [file exists $EU_DIR/eagleconnect.conf]
if {$verifpresencepref > 0} {
	detectviaeaglediag unefois; after 60000 lancediaginterval; set variableinutile 1} else {
	set variablenonuse 1;
			     }







