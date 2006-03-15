#sh

#EagleConnect.tcl version 0.9  Copyright (C) 2004 Emmanuel Yves
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
exec wish $0 ${1+"$@"}


package require msgcat;
msgcat::mclocale $env(LANG);
#::msgcat::mclocale "en_EN"

msgcat::mcload /etc/eagle-usb/eagleconnect
#msgcat::mcload [pwd];

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


#------------------------------------------------------------------------------------------------------
#--------------------------------------------------------------------------------------------------------
#----------------------------------FONCTIONS APPELANT LES DIFFERENTS MODES ---------------------------------
#------------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------------

proc lancemodenormal {} {

 	.can.framedesboutons.modenormal configure -relief sunken -foreground blue
	 .can.framedesboutons.modeexpert configure -relief groove -foreground black

	
	cd /tmp
	file delete resultateaglediagnormal.txt
	global SBIN_DIR
	exec sudo $SBIN_DIR/eaglediag -n >> /tmp/resultateaglediagnormal.txt;

	# J'ouvre le fichier "resultateaglediagnormal.txt" en lecture et j'affecte son contenu à la 
	# variable "journaleaglediagnormal"
	set journaleaglediagnormal [ open resultateaglediagnormal.txt r ]
	.can.frameresult.contientresult.contenufichier delete 1.0 end
	.can.frameconseil.contientconseil.litconseil delete 1.0 end

	# Tant que je n'ai pas atteint la fin du contenu de la variable "journaleaglediagnormal"
	while { ![eof $journaleaglediagnormal] } {

		# Je lis ligne contenue dans la variable "journaleaglediagnormal"
		# et je l'affecte à la variable "lignelue"
		gets $journaleaglediagnormal lignelue;
		
                # ci-dessous, je recherche la chaine ## dans chaque lignelue. Si présent, alors linesfound vaut 1, sinon 0
		set linesfound [regexp {(^##)} $lignelue]
		puts $linesfound

		# Pour chaque ligne où linesfound est différent de 1, j'insère 
		# les lignes lues dans le widget texte "contenufichier" (end = le caractère juste 
		# après le dernier saut de ligne
                if {$linesfound != 1} {
		.can.frameresult.contientresult.contenufichier insert end $lignelue

		# J'insère un retour chariot (\n) après chaque ligne 
		.can.frameresult.contientresult.contenufichier insert end \n;}}

		#Je ferme la variable "journaleaglediagnormal"
		close $journaleaglediagnormal;
	


	
	# J'ouvre le fichier "resultateaglediagnormal.txt" en lecture et j'affecte son contenu à la 
	# variable "journaleaglediagnormal"
	set conseilsnewbie [ open /var/log/eagle-usb/eagle_diag_newbie.txt r ]

	.can.frameconseil.contientconseil.litconseil delete 1.0 end 

	set file .can.frameconseil.contientconseil.litconseil; 
	$file tag configure BODY -foreground black -background white; 
	$file tag configure BODY2 -foreground black -justify center; 

	set titre [font create titre -family arial -size 12 -weight bold];
	$file tag configure TITRE -foreground "#000000" -font titre -justify center;

	$file insert end "CONSEILS" TITRE; 
	$file insert end "\n\n";
	
	# Tant que je n'ai pas atteint la fin du contenu de la variable "conseilsnewbie"
	while { ![eof $conseilsnewbie] } {

	   	# Je lis ligne contenue dans la variable "journaleaglediagnormal"
		# et je l'affecte à la variable "lignelue"
		gets $conseilsnewbie readline;
	        set recuplines [string range $readline 2 end]
	          
		# J'insère les lignes lues dans le widget texte "texteeaglediagnormal" (end = le caractère juste 
		# après le dernier saut de ligne
		.can.frameconseil.contientconseil.litconseil insert end $recuplines

		# J'insère deux retours chariot (\n) après chaque ligne 
        
		.can.frameconseil.contientconseil.litconseil insert end \n\n;}

		#Je ferme la variable "$conseilsnewbie"
		close $conseilsnewbie;

	
	font delete titre;

}




proc lancemodeexpert {} {

 	 .can.framedesboutons.modenormal configure -relief groove -foreground black
	 .can.framedesboutons.modeexpert configure -relief sunken -foreground blue
		
		
	cd /tmp
	file delete resultateaglediagexpert.txt
	global SBIN_DIR
	exec sudo $SBIN_DIR/eaglediag -a >> /tmp/resultateaglediagexpert.txt;

	.can.frameresult.contientresult.contenufichier delete 1.0 end
	.can.frameconseil.contientconseil.litconseil delete 1.0 end

	set journaleaglediagexpert [ open resultateaglediagexpert.txt r ]
	while { ![eof $journaleaglediagexpert] } {
		gets $journaleaglediagexpert lignelue;
		.can.frameresult.contientresult.contenufichier insert end $lignelue
		.can.frameresult.contientresult.contenufichier insert end \n;
						}
	close $journaleaglediagexpert;

	.can.frameconseil.contientconseil.litconseil delete 1.0 end 
	set file2 .can.frameconseil.contientconseil.litconseil; $file2 tag configure BODY -foreground black -background white; 
	$file2 tag configure BODY2 -foreground black -justify center; 

	set titre2 [font create titre2 -family arial -size 12 -weight bold]
	$file2 tag configure TITRE -foreground "#000000" -font titre2 -justify center

	set corps2 [font create corps2 -family arial -size 12];
	$file2 tag configure CORPS -foreground "#000000" -font corps2 -justify left;

	$file2 insert end [msgcat::mc "CONSEILS"] TITRE; 
	$file2 insert end "\n\n\n";
	$file2 insert end [msgcat::mc "- Latence"] CORPS;
	$file2 insert end "\n\n";
	$file2 insert end [msgcat::mc "- Partage d'IRQ"] CORPS;
	$file2 insert end "\n\n";
	$file2 insert end [msgcat::mc "- Clamp MSS si votre PC sert de gateway"] CORPS;
	$file2 insert end "\n\n";
	$file2 insert end [msgcat::mc "- Une adresse IP apparaît pour ppp0 ou ethX : vous êtes connecté, seul votre firewall vous bloque peut-être ?"] CORPS;
	$file2 insert end "\n\n";
	$file2 insert end [msgcat::mc "- Si votre modem n'apparaît pas (1110 90xx) vous avez peut-être un problème d'usb, essayer noapic acpi=off dans /etc/lilo.conf"] CORPS;
	$file2 insert end "\n\n";

	font delete titre2;
	font delete corps2;


}




proc saveas {} {
	
		set extensions {
	{{Fichiers Textes}	{.txt}	TEXT}
	{{Tous Fichiers}	*	    }
	}

	set identifiant [exec whoami]
	cd /home

	set contenu [.can.frameresult.contientresult.contenufichier get 1.0 end]
	


		set sauversous [tk_getSaveFile -title "Save data as" -initialdir /home -filetypes $extensions]

	if {[string length $sauversous]} {	

		set nouveaufichier [open $sauversous {WRONLY CREAT TRUNC}]
		set verifcouleurbouton1 [.can.framedesboutons.modenormal cget -foreground]
		set verifcouleurbouton2 [.can.framedesboutons.modeexpert cget -foreground]
			
		if {$verifcouleurbouton1 == "blue"} {
			puts $nouveaufichier [.can.frameresult.contientresult.contenufichier get 1.0 end]} elseif {$verifcouleurbouton2 == "blue"} {
			puts $nouveaufichier [.can.frameresult.contientresult.contenufichier get 1.0 end]}
			     } else {puts "rien à faire"}


		}








#------------------------------------------------------------------------------------------------------
#--------------------------------------------------------------------------------------------------------
#----------------------------------PLACEMENT DES ELEMENTS DE L'INTERFACE ---------------------------------
#------------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------------


#wm maxsize . 780 550
wm title . [msgcat::mc "Diagnostic du système"]
wm maxsize . 1800 1600
wm geometry . 800x600+100+100


	canvas .can 
	frame .can.framedesboutons 
	frame .can.frameresult 
	frame .can.frameboutonnext
	frame .can.frameconseil 
	
        
	pack .can.frameconseil -side bottom -expand true -anchor n -fill both
	pack .can.framedesboutons -side top -anchor n -fill both
	pack .can.frameresult -side top -expand true -anchor n -fill both
	pack .can.frameboutonnext -fill both
	pack .can -expand true -fill both
	
	
	frame .can.frameresult.contientresult 
	scrollbar .can.frameresult.contientresult.scroll -command ".can.frameresult.contientresult.contenufichier yview";
        text .can.frameresult.contientresult.contenufichier -background white -font {Fixed 14} -wrap word -height 10 -yscrollcommand ".can.frameresult.contientresult.scroll set";
        pack .can.frameresult.contientresult.scroll -side right -fill y; 
        pack .can.frameresult.contientresult.contenufichier -expand true -fill both
	pack .can.frameresult.contientresult -expand true -fill both

	frame .can.frameconseil.contientconseil 
	scrollbar .can.frameconseil.contientconseil.scroll -command ".can.frameconseil.contientconseil.litconseil yview";
        text .can.frameconseil.contientconseil.litconseil -background white -font {Fixed 14} -wrap word -height 10 -yscrollcommand ".can.frameconseil.contientconseil.scroll set";
        pack .can.frameconseil.contientconseil.scroll -side right -fill y; 
        pack .can.frameconseil.contientconseil.litconseil -expand true -fill both
	pack .can.frameconseil.contientconseil -expand true -fill both
	
	
button .can.framedesboutons.modenormal -width 20 -relief groove  -text [msgcat::mc "Mode Normal"] -command {lancemodenormal}
button .can.framedesboutons.modeexpert -width 20 -relief groove  -text [msgcat::mc "Mode Expert"] -command {lancemodeexpert}
button .can.framedesboutons.sauver -width 20 -relief groove  -text [msgcat::mc "Save as"] -command {saveas}
button .can.framedesboutons.boutonexit -width 20 -relief groove  -text [msgcat::mc "Exit"] -command {cd /tmp; foreach fichierstxt [glob -nocomplain -dir /tmp *.txt *.tcl~] {file delete $fichierstxt;}; exit}

pack .can.framedesboutons.modenormal -side left 
pack .can.framedesboutons.modeexpert -side left 
pack .can.framedesboutons.boutonexit -side right
pack .can.framedesboutons.sauver -side right 







