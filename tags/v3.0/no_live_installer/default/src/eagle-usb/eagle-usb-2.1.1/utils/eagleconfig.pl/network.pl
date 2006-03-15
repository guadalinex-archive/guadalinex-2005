#
#   eagleconfig: configuration utility for eagle USB modems
#
#   Copyright (C) 2003 Jerome Marant <jerome.marant@sagem.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License Version 2 as 
#   published by the Free Software Foundation.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#   or contact the author.
#
#   $Id: network.pl,v 1.2 2004/02/19 22:04:37 sleeper Exp $
#

my $options = $pppdir . "/options";
my $pap_secrets = $pppdir . "/pap-secrets";
my $chap_secrets = $pppdir . "/chap-secrets";

# Check if an IP is correct
sub check_ip ($)
  {
    my ($ip) = @_;

    if ($ip =~ /^\s*(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})\s*$/)
      {
	return (($1 < 256) && ($2 < 256) && ($3 < 256) && ($4 < 256));
      }
    return 0;
  }


sub ip_configuration ($)
  {
    my ($ip) = @_;

    my $title = "Network configuration";
    my $desc = "Enter your IP Address";

    my ($status, $value);

    ($status, $value) = inputbox($desc, $title, $$ip);
    while ((! $status) && (! check_ip($value)))
      {
	$desc = "\nIncorrect IP address.\n\n" .
	  "Please enter a correct IP address.";

	($status, $value) = inputbox($desc, $title, $$ip);
      }
	   
    $$ip = $value if (! $status);

    return $status;
  }


sub netmask_configuration
  {
    my ($netmask) = @_;

    my $title = "Network configuration";
    my $desc = "Enter the netmask";

    my ($status, $value);

    ($status, $value) = inputbox($desc, $title, $$netmask);
    while ((! $status) && (! check_ip($value)))
      {
	$desc = "\nIncorrect Netmask.\n\n" .
	  "Please enter a correct Netmask.";

	($status, $value) = inputbox($desc, $title, $$netmask);
      }
    $$netmask = $value;

    return $status;

  }


sub gateway_configuration ($)
  {
    my ($gateway) = @_;

    my $title = "Network configuration";
    my $desc = "\nEnter the gateway IP address\n" .
      "Leave the field blank if you don't use a gateway.";

    my ($status, $value) = inputbox($desc, $title, $$gateway);

    while ((! $status) && ($value !~ /^\s*$/) && (!check_ip($value)))
      {
	$desc = "\nYou entered an erroneous IP address.\n\n" .
	  "Please reenter the gateway IP address.";
	($status, $value) = inputbox($desc, $title, $$gateway);
      }
    $$gateway = $value;

    return $status;
  }


sub login_password
  {
    my ($login, $password) = @_;

    my $title = "PPP configuration: Username";
    my $desc = "What is the username of your PPP connection?";

    my $status;

    ($status, $$login) = inputbox($desc, $title, $$login);
    return $status if ($status);

    $title = "PPP configuration: Password";
    $desc = "What is the password of your PPP connection?";

    ($status, $$password) = inputbox($desc, $title, $$password);

    return $status;
  }


sub dns_configuration
  {
    my ($dns1, $dns2) = @_;

    my $title = "Network configuration";
    my $desc = "\nEnter the IP address of the primary DNS server";
    
    my ($status, $value1, $value2);
    
    ($status, $value1) = inputbox($desc, $title, $$dns1);
    while ((! $status) && (! check_ip($value1)))
      {
	$desc = "\nYou entered an erroneous IP address.\n\n" .
	       "Please reenter the IP address of the primary DNS server.";
	($status, $value1) = inputbox($desc, $title, $$dns1);
      }
    $$dns1 = $value1;

    return $status if ($status);

    $desc = "\nEnter the IP address of the secondary DNS server.\n\n" .
      "You can leave this field blank.";

    ($status, $value2) = inputbox($desc, $title, $$dns2);
    while ((! $status) && ($value2 !~ /^\s*$/) && !check_ip($value2))
      {
	$desc = "\nYou entered an erroneous IP address.\n\n" .
	  "Please reenter the IP address of the secondary DNS server" .
	  "or leave the field blank.";
	($status, $dns2) = inputbox($desc, $title, $$dns2);
      }
    $$dns2 = $value2;

    return $status;
  }


sub use_peer_dns
  {
    my ($peerdns) = @_;

    my $title = "DNS configuration";
#    my $desc = "Do you want to use the peer DNS (note this will erase existing /etc/resolv.conf) ?";
    my $desc = <<"EOL";
Do you want to use the peer DNS ?\n\n
NOTE: this will erase existing /etc/resolv.conf
EOL

    my ($status, $value);
    
    if ($$peerdns == 0)
      {
	($status, $value) = noyesbox($desc, $title);
      }
    else
      {
	($status, $value) = yesnobox($desc, $title);
      }

    if ($status == 1)
      {
	$$peerdns = 0;
      }
    else
      {
	$$peerdns = 1;
      }
  }


sub network_parameters
  {
    my ($profile) = @_;

    my $title = "Network configuration";
    my $desc = "\nWhat network configuration are you using?";

    my ($status, $value);

    my @menuvar = ();

    my $cfg = $profiles{$profile};
    $value = $cfg->val($network_sect, 'protocol');
    my $old_value = $value;

    $value = "dhcp" if (! defined($value));

    my $encapsulation = $cfg->val($modem_sect, 'encapsulation');

    add_radiolist_item(\@menuvar, "dhcp", "DHCP", $value);
    add_radiolist_item(\@menuvar, "ppp", "Point-to-Point Protocol (PPPoE or PPPoA)", $value);
    add_radiolist_item(\@menuvar, "static", "Static IP", $value);

    while (1)
      {
	($status, $value) = radiolist($desc, $title, scalar(@menuvar) / 3,
				      @menuvar);
	return 0 if ($status);

	if ((($encapsulation eq "5") || ($encapsulation eq "6")) &&
	    ($value ne "ppp"))
	  {
	    $desc = "Since you chose PPPOA LLC or PPPOA VC as a modem protocol\n" .
	      "you cannot choose something else than PPP as a network protocol.";
	    ($status, $value) = msgbox($desc, $title);
	    
	  }
	else
	  {
	    last;
	  }
      }

    # Reset network section if the former protocol is different from
    # the current protocol
    if ($old_value ne $value)
      {
	$cfg->DeleteSection($network_sect);
	$cfg->AddSection($network_sect);
      }

    # DHCP
    if ($value eq "dhcp")
      {
	add_val($cfg, $network_sect, 'protocol', $value);
      }
    # PPP
    elsif ($value eq "ppp")
      {
	# Identification
	my $login = $cfg->val($network_sect, 'login', '');
	my $password = $cfg->val($network_sect, 'password', '');
	return 0 if (login_password(\$login, \$password));

	add_val($cfg, $network_sect, 'login', $login);
	add_val($cfg, $network_sect, 'password', $password);
	
	# Use peer DNS?
	my $peerdns = $cfg->val($network_sect, 'peerdns', '1');
	use_peer_dns(\$peerdns);

	# Configure DNS
	if (! $peerdns)
	  {
	    my $dns1 = $cfg->val($network_sect, 'dns1', '');
	    my $dns2 = $cfg->val($network_sect, 'dns2', '');
	    return 0 if (dns_configuration(\$dns1, \$dns2));

	    add_val($cfg, $network_sect, 'dns1', $dns1);
	    add_val($cfg, $network_sect, 'dns2', $dns2);
	  }
	else
	  {
	    # Remove DNS entries when using a peer DNS
	    $cfg->delval($network_sect, 'dns1');
	    $cfg->delval($network_sect, 'dns2');
	  }

	add_val($cfg, $network_sect, 'protocol', $value);
	add_val($cfg, $network_sect, 'peerdns', $peerdns);
      }
    # Static PPP
    elsif ($value eq "static")
      {
	# Configure IP
	my $ip = $cfg->val($network_sect, 'ip', '');
	return 0 if (ip_configuration(\$ip));

	# Configure Netmask
	my $netmask = $cfg->val($network_sect, 'netmask', '255.255.255.0');
	return 0 if (netmask_configuration(\$netmask));

	# Configure Gateway
	my $gateway = $cfg->val($network_sect, 'gateway', '');
	return 0 if (gateway_configuration(\$gateway));

	# Configure DNS
	my $dns1 = $cfg->val($network_sect, 'dns1', '');
	my $dns2 = $cfg->val($network_sect, 'dns2', '');
	return 0 if (dns_configuration(\$dns1, \$dns2));

	# Update
	add_val($cfg, $network_sect, 'protocol', $value);
	add_val($cfg, $network_sect, 'ip', $ip);
	add_val($cfg, $network_sect, 'netmask', $netmask);
	add_val($cfg, $network_sect, 'gateway', $gateway);
	add_val($cfg, $network_sect, 'dns1', $dns1);
	add_val($cfg, $network_sect, 'dns2', $dns2);
      }

    return 1;
  }


sub ppp_config ($$$)
  {
    my ($login, $password, $peerdns) = @_;
    my $usepeerdns = '';

    $usepeerdns = 'usepeerdns' if ($peerdns);

    if (-e "$options" && ! -e "$options.eagleconfig_save")
      {
	copy ("$options", "$options.eagleconfig_save");
      }
 
    open OPTIONS, ">$options" or die "Error";
    print OPTIONS <<EOF;
#####################################
######### ADSL Fast800/840  #########
#########   SAGEM Company   #########
#####################################
#
# Generated by eagleconfig
#

user "$login"
mru 1492
mtu 1492
noipdefault
defaultroute
$usepeerdns
persist
noauth
#ipcp-accept-remote
#ipcp-accept-local
nobsdcomp
nodeflate
nopcomp
novj
novjccomp
noaccomp -am
EOF
    close OPTIONS;
 
    if (-e "$chap_secrets" && ! -e "$chap_secrets.eagleconfig_save")
      {
	copy ("$chap_secrets", "$chap_secrets.eagleconfig_save");
      }
   
    open CHAP, ">$chap_secrets" or die "";
    print CHAP <<EOF;
######################################
# This file was created by eagleconfig
#
# DO NOT EDIT MANUALY
#

$login * $password *
EOF
    close CHAP;
 
    if (-e "$pap_secrets" && ! -e "$pap_secrets.eagleconfig_save")
      {
	copy ("$pap_secrets", "$pap_secrets.eagleconfig_save");
      }
 
    open PAP, ">$pap_secrets" or die "";
    print PAP <<EOF;
######################################
# This file was created by eagleconfig
#
# DO NOT EDIT MANUALY
#

$login * $password *
EOF
    close PAP;
}


# Determine the Linux distribution we are running on
sub what_distribution
  {
    if (-f "/etc/mandrake-release")
      {
	return "mandrake";
      }
    elsif (-f "/etc/redhat-release")
      {
	return "redhat";
      }
    elsif (-f "/etc/SuSE-release")
      {
	return "suse";
      }
    elsif (-f "/etc/debian_version")
      {
	return "debian";
      }

    return "unknown";
  }

sub write_network_configuration
  {
    my $dist = what_distribution();
    return () if (! require "$dist.pl");

    return () if ($current_profile eq "");

    my $cfg = $profiles{$current_profile};
    my $protocol = $cfg->val($network_sect, 'protocol');

    if ($protocol eq "dhcp")
      {
	dhcp();

	gateway_config("");
      }
    elsif ($protocol eq "ppp")
      {
	my $peerdns = $cfg->val($network_sect, 'peerdns');
	if (! $peerdns)
	  {
	    my $dns1 = $cfg->val($network_sect, 'dns1');
	    my $dns2 = $cfg->val($network_sect, 'dns2', '');
	    dns($dns1, $dns2);
	  }

	my $login = $cfg->val($network_sect, 'login', '');
	my $password = $cfg->val($network_sect, 'password', '');
	ppp_config($login, $password, $peerdns);
      }
    elsif ($protocol eq "static")
      {
	my $ip = $cfg->val($network_sect, 'ip');
	my $netmask = $cfg->val($network_sect, 'netmask');
	static($ip, $netmask);

	my $gateway = $cfg->val($network_sect, 'gateway', '');
	gateway_config($gateway);

	my $dns1 = $cfg->val($network_sect, 'dns1');
	my $dns2 = $cfg->val($network_sect, 'dns2', '');
	dns($dns1, $dns2);
      }
  }

1;
