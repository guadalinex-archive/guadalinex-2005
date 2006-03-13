#!/usr/bin/perl
# $Id: isp_adsl_db.pl,v 1.8 2005/02/13 22:55:21 baud123 Exp $
# Program : isp_adsl_db.pl
# Goal    : provide functions for a central ISP database for eagle-usb driver
# Author  : Benoit Audouard (baud123 at tuxfamily dot org)
# Description : based on Drakconnect's database by Mandrakesoft in order
# to provide several outputs
#  - HTML/spip for http://www.eagle-usb.org/article.php3?id_article=23
#  - program initialization list for eu_config_bash, debian configuration

# Copyright (C) 2004 Benoit Audouard (baud123 at tuxfamily dot org)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#use lib qw(., /usr/lib/libDrakX);
#use lib qw(.) ;
use lib qw(/usr/share/eagle-usb) ;
use utf8 ;
#require network::adsl_consts ;
use Common ;
#use Unicode::String;
use Encode;

require adsl_consts ;

# MAIN : decode parameters
local $_ = join '', @ARGV;
/--help/ and print_help () ;
/--print-spip/ and print_spip_format() ;
/--print-config/ and print_eu_config_format () ;


sub print_help ()
{
  print "Usage : $0 [--help] [--print-spip] [--print-config]\n" ;
  print "    --help displays this help\n" ;
  print "    --print-spip displays database in spip format\n" ;
  print "    --print-config displays ISP database for eu_config_bash\n" ;
}

#sub from_utf8 { iconv($_[0], "utf-8", standard_charset()) }

sub print_latin1 ()
{
#	$ustr = Unicode::String::utf8($_);
#	print $ustr->latin1();
}

sub test_decode_utf8 ()
{
	#print from_utf8( "Fournisseur d'accès");
	#print utf8::decode("iso-8859-1", "Fournisseur d'accès");
	print decode("iso-8859-1", "Fournisseur d'accès");
	#print utf8::from_to("iso-8859-1", "Fournisseur d'accès");
	#decode("iso-8859-1", $octets);
}

sub print_eu_config_format ()
{
  # see format below to display database, 
  # used for eu_config_bash file configuration in eagle-usb driver
  my %adsl_data = %network::adsl_consts::adsl_data;
  #my %adsl_data = adsl_consts::adsl_data;
			
  # example of line format to display (for cut & paste in eu_config_bash
  # "xFR01" )       VPI="08" ; VCI="23" ; ENC="06" ; COUNTRY="France" ; ISP_NAME="FT (Free, Wanadoo...)" ; CMVep="FR01" ; CMVei="WO";;

  # the index for adsl_data is formatted as : Country|ISP|province
  foreach (sort keys %adsl_data ) {
	my $name=$_ ;
	#my ($country , $isp) = split(/|/,$name) ;
	@tmp=split(/\|/,$name); # format : "Country|ISP"
	my $country = shift(@tmp); 
	my $isp = join " " ,@tmp;
	my $id = $adsl_data{$_}->{id};
	my $enc = $adsl_data{$_}->{Encapsulation};
	my $CMVep = $adsl_data{$_}->{CMVep} | "WO" ;
	my $CMVei = $adsl_data{$_}->{CMVei} | "WO" ;

	print '"x'. $id . '" )  VPI="' ;
	printf ("%02d",  $adsl_data{$_}->{vpi} );
	print '" ; VCI="' ;
        printf("%02d",	$adsl_data{$_}->{vci} );
        print '" ; ENC="' ;
	printf("%02d",  $enc );
	print '" ; COUNTRY="'. $country . '" ; ISP_NAME="' . $isp. '"' . " ; CMVep=$CMVep ; CMVei=$CMVei ;;\n";

  };

  print "#for ISP_TMP in " ;
  foreach  (sort keys %adsl_data ) {
      print $adsl_data{$_}->{id} . " " ;
  }
  print " ; do \n" ;
 
1;
};
sub print_spip_format ()
{
# see format below to display database, 
# used for http://www.eagle-usb.org/article.php3?id_article=23
my %adsl_data = %network::adsl_consts::adsl_data;
#my %adsl_data = adsl_consts::adsl_data;
			
my @enc_tab ;
$enc_tab[1] = "RFC2516 Bridged PPPoE LLC";
$enc_tab[2] = "RFC2516 Bridged PPPoE VCmux";
$enc_tab[3] = "RFC1483/2684 Routed IP LLC-SNAP";
$enc_tab[4] = "RFC1483/2684 Routed IP (IPoA) VCmux";
$enc_tab[5] = "RFC2364 PPPoA LLC";
$enc_tab[6] = "RFC2364 PPPoA VCmux";

print "|{{key}}|{{ISP - Fournisseur d'accès}}|{{Pays}}|{{VPI}}|{{VCI (hexa)}}|{{encapsulation}}|{{connexion}}|{{DNS}}| {{modem fourni/testé}}|\n" ;
# for each ISP, display values vpi / vci
# the index for adsl_data is formatted as : Country|ISP|province
foreach (sort keys %adsl_data ) {
	my $name=$_ ;
	#my ($country , $isp) = split(/|/,$name) ;
	@tmp=split(/\|/,$name); 
	my $country = shift(@tmp); 
	my $isp = join " " ,@tmp;
	my $id = $adsl_data{$_}->{id};
	my $enc = $adsl_data{$_}->{Encapsulation};
	my $enc_text=$enc_tab[$enc] ;
	my $methods= $adsl_data{$_}->{methods_all} || $adsl_data{$_}->{method};
	my $modem_furnished= $adsl_data{$_}->{modem} || "?" ;
	my $dnsServers= $adsl_data{$_}->{dnsServers_text} || "? ?" ;

	if ( defined($adsl_data{$_}->{url_tech}) ) {
		$isp="[$isp->$adsl_data{$_}->{url_tech}]"
	}

	#print $id . " " . $country . " + " . $isp . " | " . $adsl_data{$_}->{vpi} . " " . hex($adsl_data{$_}->{vci}) . " ($adsl_data{$_}->{vci}) $enc\n";
	print "|$id|$isp|$country|" . $adsl_data{$_}->{vpi} . "|" . hex($adsl_data{$_}->{vci}) . " ($adsl_data{$_}->{vci})|$enc - $enc_text|$methods|$dnsServers|$modem_furnished|\n";

};

1;
};
#print $adsl_data("Speedy")->vpi ;
