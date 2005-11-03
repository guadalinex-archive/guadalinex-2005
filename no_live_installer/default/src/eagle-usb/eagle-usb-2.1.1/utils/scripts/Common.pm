package Common; # $Id: Common.pm,v 1.2 2004/12/15 00:32:45 baud123 Exp $

#use MDK::Common;
#use MDK::Common::System;


#use run_program;

use Exporter;

@ISA = qw(Exporter);
# no need to export ``_''
#@EXPORT = qw($SECTORSIZE N N_ check_for_xserver files_exist formatTime formatXiB makedev mandrake_release removeXiBSuffix require_root_capability salt setVirtual set_alternative set_permissions translate unmakedev untranslate);
@EXPORT = qw(N N_ translate);

# perl_checker: RE-EXPORT-ALL
#push @EXPORT, @MDK::Common::EXPORT;
#push @EXPORT;


$::prefix ||= ""; # no warning

#-#####################################################################################
#- Functions
#-#####################################################################################


sub sprintf_fixutf8 {
	#my $need_upgrade;
	#$need_upgrade |= to_bool(c::is_tagged_utf8($_)) + 1 foreach @_;
	#if ($need_upgrade == 3) { c::upgrade_utf8($_) foreach @_ }
    sprintf shift, @_;
}

sub N {
    $::one_message_has_been_translated ||= join(':', (caller(0))[1,2]); #- see ugtk2.pm
    my $s = shift @_; my $t = translate($s);
    sprintf_fixutf8 $t, @_;
}
sub N_ { $_[0] }

sub translate_real {
    my ($s) = @_;
    #$s or return '';
    #foreach (@::textdomains, 'libDrakX') {
    #    my $s2 = c::dgettext($_, $s);
    #    return $s2 if $s ne $s2;
    #}
    $s;
}

sub translate {
    my $s = translate_real(@_);
    $::need_utf8_i18n and c::set_tagged_utf8($s);

    #- translation with context, kde-like
    $s =~ s/^_:.*\n//;
    $s;
}


sub untranslate {
    my $s = shift || return;
    foreach (@_) { translate($_) eq $s and return $_ }
    die "untranslate failed";
}




1;
