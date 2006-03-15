
#!/bin/bash
#Module diagnostic.tc de l'interface EagleConnect.tcl version 0.9  Copyright (C) 2004 Emmanuel Yves
#Ce programme est libre, vous pouvez le redistribuer et /ou le modifier selon les termes
#de la Licence Publique Générale GNU  publiée par la Free Software Foundation (version 2 
#ou bien toute autre version ultérieure choisie par vous). 
#Ce programme est distribué car potentiellement utile, mais SANS AUCUNE GARANTIE, 
#ni explicite ni implicite, y compris les garanties de commercialisation ou d'adaptation 
#dans un but spécifique. Reportez-vous à la Licence Publique Générale GNU pour plus de détails.
#Vous devez avoir reçu une copie de la Licence Publique Générale GNU en même temps que ce programme ;
#si ce n'est pas le cas, écrivez à la Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
#MA 02111-1307, États-Unis.
#Pour de plus amples informations, consultez la documentation jointe à ce programme.

# La ligne suivante est executee par sh, pas par Tcl \
exec wish "$0" ${1+"$@"}


package require msgcat;
msgcat::mclocale $env(LANG);
#::msgcat::mclocale "en_EN"

msgcat::mcload /etc/eagle-usb/eagleconnect


#ci-dessus, j'appelle le paquetage msgcat qui va me permettre d'afficher les messages
#de l'interface dans la langue du système de l'utilisateur (définie dans $env(LANG)).
#la commande msgcat::mcload [pwd] permet justement de charger les catalogues propres à
#chaque langue (en.msg si LANG = en_EN, fr.msg si LANG = fr_FR etc...)
# avec ::msgcat::mclocale "en_EN" ci-dessus, j'essaie en environnement en_EN 
# pour voir si le catalogue de traduction anglais est correctement appelé


set SBIN_DIR /usr/sbin
set EU_EAGLECONNECT_DIR /etc/eagle-usb/eagleconnect
set env(PATH) /bin:/sbin:/usr/bin:/usr/sbin:/usr/X11R6/bin:/usr/local/sbin;
# je définis les chemins où peuvent se situer les exécutables




wm title . [msgcat::mc "Gestionnaire d'EagleConnect"]
#wm geometry . 900x500+100+100


set env(PATH) /bin:/sbin:/usr/bin:/usr/sbin:/usr/X11R6/bin:/usr/local/sbin;
# je redéfinis le path dont j'aurai besoin pour trouver les exécutables
# je redéfinis le path pour 



#--------------------------------------------------------------------------------------------------------------------
#---------------------------------------FONCTION PREMIERE PAGE PANNEAU--------------------------------------------------
#---------------------------------------------------------------------------------------------------------------------


proc intropanel {} {
       
        destroy .framedesboutonschoix
	destroy .framecontenant
	destroy .frameboutonprev
	destroy .frameboutonprev.bouton2
        destroy .frameboutonnext 
	destroy .barredemenu
	destroy .barredemenulien

	frame .barredemenu -relief sunken -bd 1 -background #4a4a4a
	pack .barredemenu -side top -expand no -fill x

	
	frame .frameboutonnext
	frame .framecontenant
	frame .framecontenant.contienttout
	frame .framedesboutonschoix 
	
	 


	

	


#---------------------------------------------------------------------------------------------------------------------
#---------------------------------------------CREATION DU MENU QUITTER --------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

#  Je crée mon menu "Quitter"

menubutton .barredemenu.lien -text [msgcat::mc "File"] -font [list arial 12] -width 10 -relief raised -menu .barredemenu.lien.menu

menu .barredemenu.lien.menu
.barredemenu.lien.menu add command -label [msgcat::mc "About EagleConnect"] -font [list arial 12] -command { 
	option add *Dialog.msg.font {Arial 12}; 
	tk_messageBox -message [msgcat::mc "EagleConnect version 0.9 september 2004 \n\nEagleConnect is a graphical front-end for the Eagle driver and its utilities created by Benoît Audouard, Olivier Borowski, Stéphane Collet, Jérôme Marant and Frédérick Ros in order to use Sagem fast 800 modem and assimilated under Linux.\n\nAuthor : Emmanuel YVES \nFirst created on 09/02/2004 \nCopyright (c) 2004 \n This program is under the terms of the GNU General Public License Version 2, June 1991, published by the Free Software Foundation"]   
													   }
.barredemenu.lien.menu add separator
.barredemenu.lien.menu add command -label [msgcat::mc "Exit"] -font [list arial 12] -command {cd /tmp; foreach fichierstxt [glob -nocomplain -dir /tmp *.txt *.tcl~] {file delete $fichierstxt;}; exit}





	
	
	pack .framedesboutonschoix -side top -anchor n 
	pack .barredemenu.lien -side left

	pack .frameboutonnext -side bottom -expand false -anchor n -fill both
	pack .framecontenant -expand true -fill both
	
	
	
	
	
	
	

	
	#button .framedesboutonschoix.boutonsortie -width 20 -bd 0
	#pack .framedesboutonschoix.boutonsortie
	
	pack .framecontenant.contienttout
	
	label .framedesboutonschoix.labelduhaut -height 2  -text "EagleConnect configuration Panel" -font [list arial 18 bold]
	pack .framedesboutonschoix.labelduhaut -expand true -fill x

	
	
	button .frameboutonnext.bouton2 -width 20 -text "" -relief flat -command {}
	pack .frameboutonnext.bouton2 -side right
	canvas .framecontenant.can -background white -width 820 -height 420 -relief raised
	pack .framecontenant.can -expand true -fill both 


	set img1 [image create photo -file [file join /etc/eagle-usb/eagleconnect/images fichierssysteme.ppm]]
	set img2 [image create photo -file [file join /etc/eagle-usb/eagleconnect/images fonts.ppm]]

	set alpha1 [.framecontenant.can create image 150 150 -image $img1 -anchor w]
	.framecontenant.can create text 145 210 -text "System files Manager" -anchor w
	# Gestion des fichiers système

	
	set alpha2 [.framecontenant.can create image 450 150 -image $img2 -anchor w]
	.framecontenant.can create text 460 210 -text "Fonts Manager" -anchor w
	# Gestion des polices
	
	.framecontenant.can bind $alpha1 <ButtonPress> {paramsystem}
	.framecontenant.can bind $alpha2 <ButtonPress> {prefinter}

	.framecontenant.can bind $alpha1 <Enter> {
		.framedesboutonschoix.labelduhaut configure -text ""
		.framedesboutonschoix.labelduhaut configure -text "Click here to manage your files system needed by EagleConnect"
						 }
	.framecontenant.can bind $alpha2 <Enter> {
		.framedesboutonschoix.labelduhaut configure -text ""
		.framedesboutonschoix.labelduhaut configure -text "Click here to define EagleConnect interface fonts"
						}
		
	.framecontenant.can bind $alpha1 <Leave> {
		.framedesboutonschoix.labelduhaut configure -text ""
		.framedesboutonschoix.labelduhaut configure -text "EagleConnect configuration Panel"
						 }
	.framecontenant.can bind $alpha2 <Leave> {
		.framedesboutonschoix.labelduhaut configure -text ""
		.framedesboutonschoix.labelduhaut configure -text "EagleConnect configuration Panel"
						 }
		



	

}










#--------------------------------------------------------------------------------------------------------------------
#---------------------------------------FONCTION VISUALISATION SYSTEME--------------------------------------------------
#---------------------------------------------------------------------------------------------------------------------


proc paramsystem {} {

	

	destroy .framecontenant
	destroy .framecontenant.contienttout
	destroy .boutonprevious
	destroy .boutonexit
	destroy .frameboutonnext 
	destroy .framecontenant.contienttout.frvisupref1
	destroy .framecontenant.contienttout.panneaudescases
	destroy .framedesboutonschoix

	frame .framedesboutonschoix
	frame .framecontenant
	frame .frameboutonprev 
	
        

	pack .frameboutonprev -side bottom -expand false -anchor n -fill both
	pack .framedesboutonschoix -side top -anchor n
	pack .framecontenant -expand true -fill both

	button .frameboutonprev.bouton2 -width 20 -text [msgcat::mc "Previous"] -relief groove -command {intropanel}
	pack .frameboutonprev.bouton2 -side right

	frame .framecontenant.contienttout 
	scrollbar .framecontenant.contienttout.scroll -command ".framecontenant.contienttout.contenufichier yview";
        text .framecontenant.contienttout.contenufichier -background white -font {Fixed 16} -wrap word -yscrollcommand ".framecontenant.contienttout.scroll set";
        pack .framecontenant.contienttout.scroll -side right -fill y; 
        pack .framecontenant.contienttout.contenufichier -expand true -fill both
	pack .framecontenant.contienttout -expand true -fill both
	

#----------------------------------------------------------------------------------------------------------------------------
#-----------------------------BOUTON DESTINE A VISUALISER LE FICHIER DES PREFERENCES UTILISATEUR----------------------------
#----------------------------------------------------------------------------------------------------------------------------


	button .framedesboutonschoix.boutonvoirpref -text "See eagleconnect.conf file" -width 20 -command {
		set readfichierpref [open /etc/eagle-usb/eagleconnect.conf r]
		.framecontenant.contienttout.contenufichier delete 1.0 end;
		
		while { ![eof $readfichierpref] } {
			gets $readfichierpref readlinespref;
			.framecontenant.contienttout.contenufichier insert end $readlinespref

			# J'insère un retour chariot (\n) après chaque ligne 
			.framecontenant.contienttout.contenufichier insert end \n;
						  }

		close $readfichierpref
													}


#----------------------------------------------------------------------------------------------------------------------------
#-----------------------------BOUTON DESTINE A VISUALISER LE FICHIER /ETC/SUDOERS-----------------------------------------------
#----------------------------------------------------------------------------------------------------------------------------


	button .framedesboutonschoix.boutonvoirsudoers -text "See /etc/sudoers file" -width 20 -command {
		#puts "bonjour"
		set fichiersudoers [open /etc/sudoers r]
               .framecontenant.contienttout.contenufichier delete 1.0 end;

		while { ![eof $fichiersudoers] } {
			gets $fichiersudoers readlinessudoers;
			.framecontenant.contienttout.contenufichier insert end $readlinessudoers

			# J'insère un retour chariot (\n) après chaque ligne 
			.framecontenant.contienttout.contenufichier insert end \n;
						  }

		close $fichiersudoers

													}



#------------------------------------------------------------------------------------------------------------------------------
#--------------------------BOUTON DESTINE A MODIFIER AUTOMATIQUEMENT LE FICHIER /ETC/SUDOERS-----------------------------------
#--------------------------------------------------------------------------------------------------------------------------



button .framedesboutonschoix.boutonsudoers -text "Modify /etc/sudoers file" -width 20 -command {
	
	set identifiant [exec whoami];
	if {$identifiant == "root"} {
		# ci-dessous, on recherche le login du simple utilisateur actif (simple utilisateur non root)
		
		global login;
		set login1 [exec who -u];
		puts $login1;
		set rech2pts [string first " " $login1];
		set login [string range $login1 0 [expr {$rech2pts-1}]];
		# puts /home/$login;
		set startadsl /usr/sbin/fctStartAdsl;
		set stopadsl /usr/sbin/fctStopAdsl
		set eaglediag /usr/sbin/eaglediag
		set eaglestat /usr/sbin/eaglestat
		set eaglectrl /usr/sbin/eaglectrl
		

		# ci-dessous, on modifie - ou non - le fichier /etc/sudoers pour que l'utilisateur puisse
		# bénéficier des commandes via sudo

		set fichiersudoers [open /etc/sudoers a+]
		seek $fichiersudoers 0 start 
		set lecturesudoers [read $fichiersudoers]

		set verifsudoers [string first "fctStartAdsl" $lecturesudoers]
		puts "je passe ici"
			if {$verifsudoers == -1} {
				
				set entree1 "$login ALL=NOPASSWD:$startadsl"
				set entree2 "$login ALL=NOPASSWD:$stopadsl -sf"
				set entree3 "$login ALL=NOPASSWD:$eaglediag"
				set entree4 "$login ALL=NOPASSWD:$eaglestat"
				set entree5 "$login ALL=NOPASSWD:$eaglectrl -w"

				puts $fichiersudoers $entree1
				puts $fichiersudoers $entree2
				puts $fichiersudoers $entree3
				puts $fichiersudoers $entree4
				puts $fichiersudoers $entree5

				tk_messageBox -message "Le fichier /etc/sudoers a été modifié avec succès"

						 } else {

				tk_messageBox -message "Nothing to do : etc/sudoers file has already been configured";

							}

		close $fichiersudoers

						} else {
		tk_messageBox -message "/etc/sudoers file modification not permitted to single user"
						}

											}




#-----------------------------------------------------------------------------------------------------------------------------
#-------------------------------------------------PLACEMENT DES ELEMENTS DE L'INTERFACE----------------------------------------
#-----------------------------------------------------------------------------------------------------------------------------



pack .framedesboutonschoix.boutonvoirpref -side left 
pack .framedesboutonschoix.boutonvoirsudoers -side left 
pack .framedesboutonschoix.boutonsudoers -side left -pady 5 



}




#----------------------------------------------------------------------------------------------------------------------
#-------------------------------------------------------------------------------------------------------------------------
#---------------------------------------FENETRE PREFERENCES INTERFACE ----------------------------------------------------------
#-------------------------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------





proc prefinter {} {

	

	
	#destroy .framedesboutonschoix
	destroy .framedesboutonschoix.boutonvoirpref
	destroy .framedesboutonschoix.boutonvoirsudoers
	destroy .framedesboutonschoix.boutonsudoers
	destroy .frameboutonnext.bouton2
	destroy .framecontenant.can
	
	
	destroy .framecontenant.contienttout.contenufichier
	destroy .framecontenant.contienttout.scroll
	#frame .contenant
	#frame .contenant.contienttout -width 820 -height 720

	#--------------------------------------------------------------------------------------------------------------------
	#-------------------------------------DECLARATION ET AFFECTATION DES VARIABLES POUR LES POLICES ---------------------
	#--------------------------------------------------------------------------------------------------------------------
	#-- Ci-dessous j'obtiens la liste de toutes les polices présentes sur le système (commande font families)

	

	global listFontes1
	global listFontes
	set listFontes1 [font families]
	set listFontes [lsort $listFontes1]

	#--Je définis une liste de valeurs pour les tailles de polices

	global listtailles
	set listtailles [list 8 10 12 14 16 18 20 22 24]
	

	
	#--------------------------------------------------------------------------------------------------------------------
	#--------------------------------------------------------------------------------------------------------------------
	#---------------------------------------------------PLACEMENT DES ELEMENTS-------------------------------------------
	#--------------------------------------------------------------------------------------------------------------------
	#-------------------------------------------------------------------------------------------------------------------------

		
	# Ci-dessous, les cases à cocher qui vont me permettre d'appliquer les styles (gras, italique, souligné) à police1 définie plus haut

	set casesacocher [frame .framecontenant.contienttout.panneaudescases]

	radiobutton $casesacocher.gras -state normal -text Gras -variable preference -value bold -command {
		global policecourante; font configure police1 -weight bold; 
		#.framecontenant.contienttout.framevisu.visufontes configure -text "A"  -font [list police1];
		set deffont [list police1]
		set visuexemplefonte .framecontenant.contienttout.framevisu.visufontes
		$visuexemplefonte delete 1.0 end
		$visuexemplefonte insert end "\n\n\n\n\n\n\n\n\n\n"; 
		$visuexemplefonte tag configure CORPS -foreground "#000000" -font $deffont -justify center;
		$visuexemplefonte insert end "Exemple de fonte pour Eagleconnect" CORPS;
		set policecourante [font actual police1]; puts $policecourante }

	radiobutton $casesacocher.italique -state normal -text Italique -variable preference -value italic -command {
		global policecourante; font configure police1 -slant italic; 
		#.framecontenant.contienttout.framevisu.visufontes configure -text "A"  -font [list police1];
		set deffont [list police1]
		set visuexemplefonte .framecontenant.contienttout.framevisu.visufontes
		$visuexemplefonte delete 1.0 end
		$visuexemplefonte insert end "\n\n\n\n\n\n\n\n\n\n"; 
		$visuexemplefonte tag configure CORPS -foreground "#000000" -font $deffont -justify center;
		$visuexemplefonte insert end "Exemple de fonte pour Eagleconnect" CORPS;
		set policecourante [font actual police1] }

	radiobutton $casesacocher.souligne -state normal -text Souligne -variable preference -value underline -command {
		global policecourante; font configure police1 -underline 1; 
		#.framecontenant.contienttout.framevisu.visufontes configure -text "A"  -font [list police1];
		set deffont [list police1]
		set visuexemplefonte .framecontenant.contienttout.framevisu.visufontes
		$visuexemplefonte delete 1.0 end
		$visuexemplefonte insert end "\n\n\n\n\n\n\n\n\n\n"; 
		$visuexemplefonte tag configure CORPS -foreground "#000000" -font $deffont -justify center;
		$visuexemplefonte insert end "Exemple de fonte pour Eagleconnect" CORPS;
 		set policecourante [font actual police1] }


	# Ci-dessous, je crée deux listbox avec ascenseur qui contiendront, 1 les noms des fontes (-listvariable contient les valeurs de listFontes), 2 les tailles (-listvariable contient les valeurs de listtailles) 

	listbox .framecontenant.contienttout.listedesfontes -selectmode single -setgrid true -bg white -listvariable listFontes -yscrollcommand ".framecontenant.contienttout.barre1 set"
	listbox .framecontenant.contienttout.listedestailles -selectmode single -setgrid true -bg white -listvariable listtailles -yscrollcommand ".framecontenant.contienttout.barre2 set"
	scrollbar .framecontenant.contienttout.barre1  -command ".framecontenant.contienttout.listedesfontes yview"
	scrollbar .framecontenant.contienttout.barre2  -command ".framecontenant.contienttout.listedestailles yview"

	# Ci-dessous, les cases à cocher qui vont appliquer les variations de polices au reste de l'interface
	# onvalue = si la case est cochée, renvoie 1, offvalue, case décochée, renvoie 0 au travers des variables $bouton, $menu, $champs et $etiquettes.
	# On appelle la procédure "attributsinterface" à  laquelle on passe pour arguments :
	# - la variable y (de 0  à 3) qui permet de distinguer s'il s'agit des polices pour les boutons, menus ou autres.
	# - la variable $bouton (ou $menu, $champs etc..) qui contient les valeurs 0 ou 1 selon que la case est cochée (onvalue, 1) ou non (offvalue 0).
	
	set attributs [frame .framecontenant.contienttout.frattributs]
	

	set rien 0

	checkbutton $attributs.case0 -height 2 -width 43 -variable boutons -onvalue 1 -offvalue 0 -text "Appliquer aux boutons   " -command {
	set y 0; attributsinterface $y $boutons; remplichampsfontes}
	checkbutton $attributs.case1 -height 2 -width 43 -variable menus -onvalue 1 -offvalue 0 -text "Appliquer aux menus     " -command {
	set y 1; attributsinterface $y $menus; remplichampsfontes}
	checkbutton $attributs.case2 -height 2 -width 43 -variable champs -onvalue 1 -offvalue 0 -text "Appliquer aux champs    " -command {
	set y 2; attributsinterface $y $champs; remplichampsfontes}
	checkbutton $attributs.case3 -height 2 -width 43 -variable etiquettes -onvalue 1 -offvalue 0 -text "Appliquer aux étiquettes" -command {
	set y 3; attributsinterface $y $etiquettes; remplichampsfontes}


	# Ci-dessous, visualisation du fichier de préférences

	#set framevisupref1 [frame .framecontenant.contienttout.frvisupref1 -relief sunken -bd 1]
	set framevisupref1 [frame .framecontenant.contienttout.frvisupref1 -relief sunken -bd 1 -height 480]	

	entry $framevisupref1.entfont1 -background white -justify left -font [list Arial 12] \
	 -textvariable essai 
			
	entry $framevisupref1.entfont2 -background white -justify left -font [list Arial 12] \
	 -textvariable essai1 
		
	entry $framevisupref1.entfont3 -background white -justify left -font [list Arial 12] \
	 -textvariable essai2 
		
	entry $framevisupref1.entfont4 -background white -justify left -font [list Arial 12] \
	 -textvariable essai3;
	
	
	

	


	#--------------------------------------------------------------------------------------------------------------------
	#--------------------------------------------------------------------------------------------------------------------
	#-------------------------------------------------------------ACTIONS--------------------------------------------------
	#--------------------------------------------------------------------------------------------------------------------
	#--------------------------------------------------------------------------------------------------------------------



	# Ci-dessous, je déclenche une action en cliquant sur la listbox des fontes
	# Je vérifie si la variable verif existe. Si oui, $verif2 = 1, et j'efface la variable police1
	# pour ne pas avoir de message d'eeeur lorsque je vais cliquer sur un autre nom de police (message
	# d'erreur : la police existe déjà) sinon = 0.
	# J'affecte à la variable idx la valeur (l'indice) de la selection courante de la listbox
	# Si $idx n'est pas vide, alors j'affecte à la variable police la valeur contenue dans $listfontes renvoyée par la commande lindex 
	# et je déclare un nouveau de police (police1) rattachée à une police existante grâce à la commande font create.
	# Enfin, j'affecte au bouton témoin boutonfontes la police (-font [list police1]on met $police1 dans une liste au cas où le nom comporterait des espaces)


	bind .framecontenant.contienttout.listedesfontes <ButtonRelease> {
		global police
		global listFontes
		global policecourante
		set verif2 [info exists verif]

		if {$verif2 == 1} {
			puts "oui"; font delete police1} else {
			puts "non"; set sansobjet 0
				  }

		set idx [.framecontenant.contienttout.listedesfontes curselection]

		if {$idx!=""} {
			set police [lindex $listFontes $idx]
			font create police1 -family $police
			set deffont [list police1]
			set visuexemplefonte .framecontenant.contienttout.framevisu.visufontes
			$visuexemplefonte delete 1.0 end
			$visuexemplefonte insert end "\n\n\n\n\n\n\n\n\n\n"; 
			$visuexemplefonte tag configure CORPS -foreground "#000000" -font $deffont -justify center;
			$visuexemplefonte insert end "Exemple de fonte pour Eagleconnect" CORPS;
			set verif 1
			set policecourante [font actual police1]
				}
								}


	# Idem que ci-dessus, excepté qu'il s'agit ici des tailles des polices (contenues dans $listtailles, voir plus haut)

	bind .framecontenant.contienttout.listedestailles <ButtonRelease> {
		global verif
		global police
		global police1
		global listtailles
		global listFontes
		global policecourante

		set verif 1
		set nbr [.framecontenant.contienttout.listedestailles curselection]

		if {$nbr!=""} {
			set taille [lindex $listtailles $nbr]
			font configure police1 -size $taille
			#.framecontenant.contienttout.framevisu.visufontes configure -text "A"  -font [list police1]
			#.framecontenant.contienttout.framevisu.visufontes configure -text "A"  -font [list police1]
			set deffont [list police1]
			set visuexemplefonte .framecontenant.contienttout.framevisu.visufontes
			$visuexemplefonte delete 1.0 end
			$visuexemplefonte insert end "\n\n\n\n\n\n\n\n\n\n"; 
			$visuexemplefonte tag configure CORPS -foreground "#000000" -font $deffont -justify center;
			$visuexemplefonte insert end "Exemple de fonte pour Eagleconnect" CORPS;

			set policecourante [font actual police1]
			puts $policecourante
				}
									}

	# Ci-dessous ma fonction attributsinterface.
	# Elle reçoit les deux arguments vus plus haut.
	# On va dans le répertoire .eagleconnect.
	# On affecte à la variable identifiant le contenu de la commande whoami (identifie l'utilisateur)
	# Si le contenu de la variable y = 0, alors on sait que c'est la case des boutons qui l'a appelée
	# Si le contenu de la variable x = 1, alors on sait que la case est ici cochée, et la fonction doit donc effectuer
	# les instructions qui suivent.
	# On affecte à a le contenu de la variable police1 (voir plus haut, celle qui contient les noms et attributs de la police)
	# On ouvre en lecture et écriture le fichier $identifiant.pref  (par exemple, emmanuel.pref) et on affecte son contenu à 
	# la variable lecture0pref.
	# Si je n'ai pas atteint la fin du fichier lecture0pref, je me positionne au début dudit fichier (seek $lecture0pref 0 start)
	# et j'affecte à la variable occur l'emplacement (la position du premier caractère) de la première occurence "bouton" trouvée dans le fichier
	# $lecture0 si cette occurence existe. Sinon la commande string first renvoie -1 si occurence non trouvée.
	# Si l'occurence n'a pas été trouvée (occur == -1), alors la fonction écrit le contenu de la variable a (police1) dans le fichier $lectureOpref
	# avec le terme bouton avant. (noton que nous avons ouvert emmanuel.pref en a+, c'est -à- dire lecture écriture avec ajout des nouvelles données
	# et non écrasement).
	# Si l'occurence a été trouvée (occur == position de la 1ère occurence "bouton" dans le fichier), alors on se place à l'endroit exact de l'emplacement
	# de l'occurence (seek $lecture0pref $occur) et on écrit le nouveau contenu de la variable police1 dans le fichier (puts $lecture0pref "bouton'$a'")
	# ENFIN, si $x == 0, alors cela signifie que la case est décochée, et on fait rien d'autre qu'afficher un message : else "puts "rien à faire""



	proc attributsinterface {y x} {

		# ci-dessous, on recherche le login du simple utlisateur actif
		
		cd /etc/eagle-usb
				
		if {$y == 0} {
			if {$x == 1} {
				puts $x
				set a [font actual police1]
			        set lecture0pref [open eagleconnect.conf a+];
				        if { ![eof $lecture0pref] } {
					        seek $lecture0pref 0 start
     						set lecture0 [read $lecture0pref]
					        set occur [string first "bouton" $lecture0]
     					        puts $occur
						           if {$occur == -1} {  
						               puts $lecture0pref "bouton $a"} else {  
      							       seek $lecture0pref $occur
						               puts $lecture0pref "bouton $a"}
								      }
        				close $lecture0pref
				        } else {
					puts "rien à faire"
					}
				 }        
                
               

		if {$y == 1} {
		        if {$x == 1} {
				puts $x
				set b [font actual police1]
			        set lecture1pref [open eagleconnect.conf a+];
   					     if { ![eof $lecture1pref] } {
						        seek $lecture1pref 0 start
						        set lecture1 [read $lecture1pref]
						        set occur1 [string first "menu" $lecture1]
						        puts $occur1
							           if {$occur1 == -1} {  
							               puts $lecture1pref "menu $b"} else {  
							               seek $lecture1pref $occur1
							               puts $lecture1pref "menu $b"}
									  }
				        close $lecture1pref
				        } else {
					puts "rien à faire"
					}
				}


		if {$y == 2} {
		        if {$x == 1} {
				puts $x
				set c [font actual police1]
			        set lecture2pref [open eagleconnect.conf a+];
				        if { ![eof $lecture2pref] } {
					        seek $lecture2pref 0 start
					        set lecture2 [read $lecture2pref]
					        set occur2 [string first "champ" $lecture2]
					        puts $occur2
						           if {$occur2 == -1} {  
							       puts $lecture2pref "champ $c"} else {  
       							       seek $lecture2pref $occur2
						               puts $lecture2pref "champ $c"}
      								      }
				     close $lecture2pref
				     } else {
				     puts "rien à faire"
				     }
				}



		if {$y == 3} {
			  if {$x == 1} {
				puts $x
				set d [font actual police1]
			        set lecture3pref [open eagleconnect.conf a+];
				        if { ![eof $lecture3pref] } {
					        seek $lecture3pref 0 start
					        set lecture3 [read $lecture3pref]
					        set occur3 [string first "etiq" $lecture3]
					        puts $occur3
							   if {$occur3 == -1} {  
						               puts $lecture3pref "etiq $d"} else {  
						               seek $lecture3pref $occur3
						               puts $lecture3pref "etiq $d"}
      								      }
			        close $lecture3pref
			        } else {
				puts "rien à faire"
					}
				}

}




	proc remplichampsfontes {} {

	cd /etc/eagle-usb
	set prefread [open eagleconnect.conf r]; 
		while { ![eof $prefread] } {

			# Je lis ligne contenue dans la variable "journaleaglediagnormal"
			# et je l'affecte à la variable "lignelue"
			gets $prefread lignelue;
		
 	               # ci-dessous, je recherche la chaine ## dans chaque lignelue. Si présent, alors linesfound vaut 1, sinon 0
			set chaine1 [regexp {(^bouton)} $lignelue]
			set chaine2 [regexp {(^menu)} $lignelue]
			set chaine3 [regexp {(^champ)} $lignelue]
			set chaine4 [regexp {(^etiq)} $lignelue]
		
				if {$chaine1 == 1} {
					.framecontenant.contienttout.frvisupref1.entfont1 delete 0 end
					.framecontenant.contienttout.frvisupref1.entfont1 insert end $lignelue
						   }
				if {$chaine2 == 1} {
			                .framecontenant.contienttout.frvisupref1.entfont2 delete 0 end 
					.framecontenant.contienttout.frvisupref1.entfont2 insert end $lignelue
						   }
				if {$chaine3 == 1} {
			                .framecontenant.contienttout.frvisupref1.entfont3 delete 0 end
			                .framecontenant.contienttout.frvisupref1.entfont3 insert end $lignelue
						   }
				if {$chaine4 == 1} {
			                .framecontenant.contienttout.frvisupref1.entfont4 delete 0 end
			                .framecontenant.contienttout.frvisupref1.entfont4 insert end $lignelue
						   }
				         	}
				}





	#--------------------------------------------------------------------------------------------------------------------
	#--------------------------------------------------------------------------------------------------------------------
	#--------------------------------------------------------------------------------------------------------------------
	#--------------------------------------------------------------------------------------------------------------------


	


	

	frame .framecontenant.contienttout.framevisu 
#-height 1
        text .framecontenant.contienttout.framevisu.visufontes -width 70 -bd 1

	

	



# ------------------------------------------------------------------------------------------------



# Ci-dessous, je place les élements construits plus haut


pack .framecontenant.contienttout -expand true -fill both -expand true 
pack .framecontenant.contienttout.listedesfontes -side left -fill both -expand true
pack .framecontenant.contienttout.barre1 -side left -fill y
pack .framecontenant.contienttout.listedestailles -side left -fill both -expand true
pack .framecontenant.contienttout.barre2 -side left  -fill both


pack $casesacocher -side top -expand true -anchor w
pack $casesacocher.gras $casesacocher.italique $casesacocher.souligne -side left -expand true -anchor w

pack .framecontenant.contienttout.framevisu -side bottom 
pack .framecontenant.contienttout.framevisu.visufontes -side bottom -expand true

pack $framevisupref1 -side left -expand true -fill x -anchor w
pack $framevisupref1.entfont1 -expand true -pady 5 -ipadx 55 -anchor w
pack $framevisupref1.entfont2 -expand true -pady 5 -ipadx 55 -anchor w
pack $framevisupref1.entfont3 -expand true -pady 5 -ipadx 55 -anchor w
pack $framevisupref1.entfont4 -expand true -pady 5 -ipadx 55 -anchor w

pack $attributs -side left -expand true -anchor w
pack $attributs.case0 -anchor w -expand true
pack $attributs.case1 -anchor w -expand true
pack $attributs.case2 -anchor w -expand true
pack $attributs.case3 -anchor w -expand true 














# procédure qui écrit dans les champs les valeurs des polices contenues dans le fichier de préférences


remplichampsfontes;







set taillefenetre2 [winfo reqwidth .]
puts $taillefenetre2


button .frameboutonnext.boutonprevious -width 20 -relief groove  -text [msgcat::mc "Previous"] -command {intropanel}
#button .frameboutonnext.boutonexit -width 20 -relief groove  -text [msgcat::mc "Exit"] -command {cd /tmp; foreach fichierstxt [glob -nocomplain -dir /tmp *.txt *.tcl~] {file delete $fichierstxt;}; exit}
pack .frameboutonnext.boutonprevious -side right 
#-expand true



}









#-------------------------------------------------------------------------------------------------------------------------
#-------------------------------------------------------------------------------------------------------------------------
#----------------------------------------------FIN FENETRE PREFERENCES INTERFACE-----------------------------------------
#-------------------------------------------------------------------------------------------------------------------------
#-------------------------------------------------------------------------------------------------------------------------










#--------------------------------Création d'une étiquette----------------------------------------------------------------

# Je crée ici une étiquette non visible dans frame3 afin de bien séparer les 2 boutons 

#label .frame1.etiquette -width 80
#pack configure .frame1.etiquette -pady 0 -side left -padx 5



#button .frame1.bouton3 -width 20 -relief groove  -text [msgcat::mc "Exit"] -command {exit}
#pack configure .frame1.bouton3 -pady 0 -side right -padx 2






#-------------------------------------------------------------------------------------------------------------------
#--------------------------------------------------------------------------------------------------------------------------
#-----------------------------------------------------LANCEMENT DE L'INTERFACE--------------------------------------------
#-------------------------------------------------------------------------------------------------------------------------
#--------------------------------------------------------------------------------------------------------------


# paramsystem
intropanel


#--------------------------------------------------------------------------------------------------------------------------
#-----------------------------------VERIFICATION EXISTENCE FICHIER ROOT.PREF-------------------------------------------------
#-------------------------------------------------------------------------------------------------------------------------




set verifpresencefichier [file exists /etc/eagle-usb/eagleconnect.conf]

if {$verifpresencefichier > 0} {puts "le fichier existe";

 } else {tk_messageBox -message [msgcat::mc "Eagleconnect configuration file hasn't been created yet"]}

#ci -dessus, je vérifie d'abord la présence du fichier simpleuser.pref dans le répertoire .eagleconnect.
#S'il existe , la commande "file exists" renvoie 1, sinon 0. Cette valeur, 1 ou 0
# est récupérée dans la variable $verifpresencefichier. Si $verifpresencefichier > 0, alors, 
#cela signifie que le fichier existe








