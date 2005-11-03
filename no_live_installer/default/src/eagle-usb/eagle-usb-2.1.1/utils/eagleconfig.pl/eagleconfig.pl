#!/usr/bin/perl -w

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
#   $Id: eagleconfig.pl,v 1.1 2004/02/15 12:50:16 Tux Exp $
#

BEGIN {
  # This hack exists so that the program can run in the absence of gettext
  # or POSIX. (John G. Hasler)
  eval "use Locale::gettext";
  $no_gettext = $@;
  eval "use POSIX";
  if ($no_gettext || $@) {
    eval 'sub gettext($) { return $_[0]; }';
  }
  else {
    eval 'setlocale(LC_MESSAGES, "")';
    eval 'textdomain("eagleconfig")';
  }

  my $eagleconfig_libdir = 'contrib';
  require lib;
  import lib "$eagleconfig_libdir";
}

use strict;
use warnings;

use Config::IniFiles;

our $backtitle = "Eagle USB Modem Configuration Utility";

# Global parameters
our $etcdir = '.';
our $pppdir = $etcdir . "/ppp";

our $resolv_conf = $etcdir . '/resolv.conf';

our $interface = `eaglectrl -i`;
chomp $interface;

our $eagledir = '.';

our $profilesdir = $eagledir . "/profiles";

our $eagle_conf = $eagledir . '/eagle-usb.conf';
our $eagleconfig_conf = $eagledir . '/eagleconfig.conf';

require "ui.pl";

# Sections of profile files
our $general_sect = 'General';
our $modem_sect = 'Modem';
our $network_sect = 'Network';

sub add_val
  {
    my ($cfg, $section, $key, $value) = @_;

    $cfg->AddSection($section) if (! $cfg->SectionExists($section));
    $cfg->newval($section, $key, $value) if (! $cfg->setval($section, $key, $value));
  }

our $current_profile = "";

our %profiles;

require "network.pl";
require "modem.pl";

# Gettext shortcut
sub N(@)
  {
    return gettext(@_);
  }

sub main_menu
  {
    my $title = "Main menu";
    my $desc = "\n Welcome to the Eagle USB Modem configuration\n\n";
    
    if ($current_profile eq "")
      {
	$desc .= " There is currently no default profile.";
      }
    else
      {
	$desc .= " The current profile is `$current_profile'.";
      }

    my @menuvar = ("Choose", "Apply a configuration profile",
		   "Create", "Create a configuration profile",
		   "Modify", "Modify an existing profile",
		   "Remove", "Remove a configuration profile",
		   "Quit",   "Exit this utility");

    while (1)
      {
	my ($status, $value) = menu($desc, $title, scalar(@menuvar) >> 1, @menuvar);
	return 0 if ($status);

	if ($value eq "Choose")
	  {
	    choose_profile();

	    # Change the description with respect to new profile name
	    $desc = "\n Welcome to the Eagle USB Modem configuration\n\n";
	    if ($current_profile eq "")
	      {
		$desc .= " There is currently no default profile.";
	      }
	    else
	      {
		$desc .= " The current profile is `$current_profile'.";
	      }
	  }
	elsif ($value eq "Create")
	  {
	    create_profile();
	  }
	elsif ($value eq "Modify")
	  {
	    modify_profile();
	  }
	elsif ($value eq "Remove")
	  {
	    remove_profiles();
	  }
	else
	  {
	    return;
	  }
      }
  }


# Get available profiles
#
# Each profile is stored in a Config::IniFiles object
sub read_profiles
  {
    opendir PROFDIR, $profilesdir or die "Error: could not find profiles directory.\n";

    foreach my $profile (grep (! /^\.|CVS/, readdir(PROFDIR)))
      {
	my $config = new Config::IniFiles(-file => "$profilesdir/$profile");
	die "Could not read `$profile' profile." if (! defined($config));
	$profiles{$profile} = $config;
      }

    close PROFDIR;

  }


sub save_profile ($)
  {
    my ($profile) = @_;

    my $title = "Save profile?";
    my $desc = "Save the `$profile' profile?";

    my ($status, $value) = noyesbox($desc, $title);
    if (! $status)
      {
	if (! defined($profiles{$profile}->WriteConfig("$profilesdir/$profile")))
	  {
	    $desc = "Error: Could not save profile in $profilesdir/$profile.";
	    ($status, $value) = msgbox($desc);

	    # Delete profile in memory
	    delete $profiles{$profile};
	  }
      }    
  }


sub choose_profile
  {
    my $title = "Choosing a profile";
    my $desc = "What profile do you want as a default?";
    my ($status, $value, $profile);

    my @menuvar;

    foreach my $profile (keys %profiles)
      {
	my $cfg = $profiles{$profile};
	push @menuvar, ($profile, $cfg->val($general_sect, 'name'));
      }

    ($status, $profile) = menu($desc, $title, scalar(@menuvar) >> 1, @menuvar);
    return () if ($status);

    $desc = "You have chosen the `$profile' profile.\n\n" .
      "Do you really want to apply this profile?\n\n" .
      "Beware: previous setting will be overwritten.";

    ($status, $value) = yesnobox($desc, $title);
    if (! $status)
      {
	$current_profile = $profile;
	write_config();
	write_modem_configuration($profile);
	write_network_configuration($profile);
      }
  }		  

sub modify_profile
  {
    my $title = "Modifying a profile";
    my $desc = "\nWhat profile do you want to modify?";
    my ($status, $value, $profile);

    my @menuvar = ();

    foreach my $prof (keys %profiles)
      {
	my $cfg = $profiles{$prof};
	push @menuvar, ($prof, $cfg->val($general_sect, 'name'));
      }

    ($status, $profile) = menu($desc, $title, scalar(@menuvar) >> 1, @menuvar);
    return () if ($status);

    $desc = "\nChoose the parameter category you want to modify.";
    @menuvar =
      ("General", "General parameters",
       "Modem", "Modem parameters",
       "Network", "Network parameters",
       "Save", "Save and return to main menu",
       "Quit", "Return to main menu without saving");

    ($status, $value) = menu($desc, $title, scalar(@menuvar) >> 1, @menuvar);
    while ((! $status) && ($value ne "Quit"))
      {
	if ($value eq "General")
	  {
	    general_parameters($profile);
	  }
	elsif ($value eq "Modem")
	  {
	    modem_parameters($profile);
	  }
	elsif ($value eq "Network")
	  {
	    network_parameters($profile);
	  }
	elsif ($value eq "Save")
	  {
	    save_profile($profile);
	    return;
	  }
	($status, $value) = menu($desc, $title, scalar(@menuvar) >> 1, @menuvar);
      }
  }

sub profile_name ($)
  {
    my ($profile) = @_;

    my $title = "What is the profile name?";
    my $desc = "This is the filename where profile information will be saved";

    my ($status, $value);

    ($status, $value) = inputbox($desc, $title);
    while ((! $status) && (exists($profiles{$value})))
      {
	$desc = "This profile does already exist.\n\n" .
	  "Please enter a new profile name.";
	($status, $value) = inputbox($desc, $title);
      }

    if (! $status)
      {
	$profiles{$value} = new Config::IniFiles();
	$$profile = $value;
      }

    return $status;
  }

sub profile_description ($)
  {
    my ($description) = @_;

    my $title = "Please give a textual description";
    my $desc = "";

    my ($status, $value);

    ($status, $value) = inputbox($desc, $title, $$description);

    $$description = $value if (! $status);

    return $status;
  }

sub general_parameters ($)
  {
    my ($profile) = @_;

    my $cfg = $profiles{$profile};
    my $profile_desc = $cfg->val($general_sect, 'name', '');

    return 0 if (profile_description(\$profile_desc));
    add_val($cfg, $general_sect, 'name', $profile_desc);

    return 1;
  }

##### Modem parameters

sub create_profile
  {
    my $profile;

    return () if (profile_name(\$profile));
    $profiles{$profile} = new Config::IniFiles();

    if (! general_parameters($profile))
      {
	delete $profiles{$profile};
	return;
      }

    if (!modem_parameters($profile))
      {
	delete $profiles{$profile};
	return;
      }

    if (! network_parameters($profile))
      {
	delete $profiles{$profile};
	return;
      }

    save_profile($profile);
  }

sub remove_profiles
  {
    my $title = "Choose the profile to remove";
    my $desc = "";

    my ($status, $value, $profile);

    my @menuvar = ();
    foreach my $prof (keys %profiles)
      {
	push @menuvar, ($prof, $profiles{$prof}->val($general_sect, 'name'));
      }

    ($status, $profile) = menu($desc, $title, scalar(@menuvar) >> 1, @menuvar);
    return () if ($status);

    $title = "Confirm profile removal";
    $desc = "\nThe `$profile' profile is going to be removed from disk.\n\n" .
      "Do you still want to remove this profile?";

    ($status, $value) = yesnobox($desc, $title);
    if (! $status)
      {
	delete $profiles{$profile};
	unlink $profilesdir . "/$profile";
      }
  }

sub read_config
  {
    if (-e "$eagleconfig_conf")
      {
	my $cfg = new Config::IniFiles(-file => "$eagleconfig_conf");
	if (defined($cfg))
	  {
	    $current_profile = $cfg->val('DEFAULT', 'profile');
	  }
      }
  }

sub write_config
  {
    my $cfg = new Config::IniFiles();

    $cfg->AddSection('DEFAULT');
    $cfg->newval('DEFAULT', 'profile', $current_profile);

    $cfg->WriteConfig($eagleconfig_conf) or die "Could not write $eagleconfig_conf.";

    $cfg->Delete();
  }

# Main

#die "You must be root to run eagleconfig.\n" if ($< != 0);

read_config();

read_profiles();

main_menu();

