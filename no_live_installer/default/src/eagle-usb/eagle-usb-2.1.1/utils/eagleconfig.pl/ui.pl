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

# Emulate the which shell command
sub which ($)
  {
    my ($binary) = @_;

    foreach my $path (split(':',$ENV{PATH}))
      {
	if ( -x "$path/$binary")
	  {
	    return 1;
	  }
      }

    return 0;
  }

# List of possible user interfaces
my @ui_list = ('gdialog', 'Xdialog', 'dialog', 'whiptail', 'lxdialog');
my $dialog = '';

foreach my $ui (@ui_list)
  {
    if (which ($ui))
      {
	print "Choosing a $ui user interface ...\n";
	$dialog = $ui;
	last;
      }
  }

# Access functions to dialog-compatible UIs
# Copyright (C) 1999-2002 John G. Hasler <john@dhh.gt.org>
#
# These functions are borrowed from pppconfig
#

sub dialogbox(@)
  {
    my $type = shift(@_);
    my @vars = @_;
    my $text = shift (@vars);
    my $title = shift (@vars) if ($#vars >= 0);

    my @uilist = ($dialog, "--title", $title, "--backtitle", $backtitle,
		  $type, $text, "22", "80", @vars);

    pipe(RDR,  WTR);
  
    my $pid = fork();
    if ($pid != 0) {		# Parent
      my $item;
      
      close WTR;

      $item = <RDR>;
      if (defined($item)) {
	chomp($item);
      }
      close RDR;
      waitpid($pid, 0);
      
      my $result = ($? >> 8);
      quit() if ($result == 255);
      die("Internal error: ", $!, "\n") unless ($result == 0 || $result == 1);

      return ($result, $item);
    } else {			# Child or failed fork()
      die (gettext("cannot fork: "), $!, "\n") unless defined $pid;
      close RDR;
      open STDERR, ">&WTR" or die("Can't redirect stderr: ", $!, "\n");
      exec @uilist;
      exit -1;		# If we get here exec() failed.
    }
}

sub msgbox(@)
  {
    return dialogbox( "--msgbox", @_ );
  }

sub infobox(@)
  {
    return dialogbox( "--infobox", @_ );
  }

sub yesnobox(@)
  {
    return dialogbox( "--yesno", @_ );
  }

# A yesnobox that defaults to no.
sub noyesbox(@)
  { 
    if ($dialog =~ /whiptail|gdialog/)
      {
	return dialogbox("--yesno", @_, "--defaultno");
      }
    else
      {
	return dialogbox( "--yesno", @_ );
      }
  }

sub inputbox(@)
  {
    dialogbox( "--inputbox", @_ );
  }

sub passwordbox(@)
  {
    dialogbox( "--passwordbox", @_ );
  }

sub menu(@)
  {
    my $text = shift (@_);
    my $title = shift (@_);
    my $menu_height = shift (@_);

    if ($dialog eq 'lxdialog')
      {
        return dialogbox('--menu', $text, $title, $menu_height, "", @_);
      }
    else        
      {
        return dialogbox('--menu', $text, $title, $menu_height, @_);
      }
  }

sub radiolist(@)
  {
    my $text = shift (@_);
    my $title = shift( @_ );
    my $menu_height = shift( @_ );

    return dialogbox("--radiolist", $text, $title, $menu_height, @_);
  }

sub add_radiolist_item ($$$$)
  {
    my ($menuvar, $tag, $item, $status) = @_;

    if ($tag eq "$status")
      {
	push @$menuvar, ($tag, $item, "on");
      }
    else
      {
	push @$menuvar, ($tag, $item, "off");
      }
  }

1;
