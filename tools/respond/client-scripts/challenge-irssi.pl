# ro-challenge.pl
#

# Thanks fly out to:
# zap, who wrote the POC ho_challenge.pl I reworked into this
#
# Changelog:
#
# v1.0 Initial version
# v1.1 Avoid leaving zombies

use strict;
use vars qw($VERSION %IRSSI);

use Irssi;
use Irssi::TextUI;
use IPC::Open2;
use FileHandle;

%IRSSI = (
	authors	=> 'James Seward',
	contact	=> 'james@jamesoff.net', 
	name	=> 'ro_challenge.pl',
	description	=> 'Implementation of ratbox2.2 ssl-based challenge-response-authentication for IRC operators',
	license	=> 'GPL',
	url		=> 'http://www.jamesoff.net/',
	changed	=> '2006-02-22'
);


#
# Global vars
my $challenge_text = '';
my $challenge_keyphrase = "";

#
# Handle /challenge
sub cmd_challenge {
  my ($cmdline, $server, $channel) = @_;
  $cmdline =~ s/\s+$//;

	my $debug = Irssi::settings_get_bool("ro_challenge_debug");

	Irssi::print("challenge commandline = $cmdline") if ($debug);

	if ($cmdline eq '') {
		Irssi::print("challange: /CHALLENGE <opername> [keyphrase]");
		return 0;
	}

	my ($opernick, $keyphrase) = split(" ", $cmdline);

	Irssi::print("challenge: opernick = $opernick") if ($debug);
	if ($debug) {
		if ($keyphrase eq '') {
			Irssi::print("challenge: keyphrase = <empty>");
		}
		else {
			Irssi::print("challenge: keyphrase = <hidden>");
		}
	}

	if ($keyphrase eq '') {
		Irssi::print("challenge: Attempting challenge with blank keyphrase (!)");
	}

	$challenge_text = "";
	$challenge_keyphrase = $keyphrase;

	$server->send_raw("CHALLENGE $opernick");
	Irssi::print("challenge: sent CHALLENGE") if ($debug);
}

#
# Handle incoming 740 numeric (receive one or more parts
# of the challenge text from server)
sub event_challenge_rpl {
	my ($server, $challenge) = @_;

	my $debug = Irssi::settings_get_bool("ro_challenge_debug");

	Irssi::print("challenge: received text --> $challenge") if ($debug);

	$challenge =~ s/^[^ ]+ ://;

	$challenge_text .= $challenge;

	Irssi::print("current challenge = $challenge") if ($debug);
}

#
# Handle incoming 741 numeric - server has sent us all
# challenge text, and we should generate and send our reply
sub event_challenge_rpl_end {
	my ($server, $blah) = @_;
	my $debug = Irssi::settings_get_bool("ro_challenge_debug");
	my $pid;

	Irssi::print("challenge: Received all challenge text, running response...") if ($debug);

	my $respond_path = Irssi::settings_get_str("ro_challenge_respond");
	my $keyfile_path = Irssi::settings_get_str("ro_challenge_keyfile");

	if ($respond_path eq '') {
		Irssi::print("challenge: whoops! You need to /set ro_challenge_respond <path to binary>");
		return 0;
	}

	if ($keyfile_path eq '') {
		Irssi::print("challenge: whoops! You need to /set ro_challenge_keyfile <path to private key file");
		return 0;
	}

	#check respond binary exists and is executable
	if (! -x $respond_path) {
		Irssi::print("challenge: $respond_path is not executable by you :(");
		return 0;
	}

	if (! -r $keyfile_path) {
		Irssi::print("challenge: $keyfile_path is not readable by you :(");
		return 0;
	}

	unless ($pid = open2(*Reader, *Writer, $respond_path, $keyfile_path)) {
		Irssi::print("challenge: couldn't exec respond, failed!");
		return 0;
	}

	print Writer "$challenge_keyphrase\n";
	print Writer "$challenge_text\n";

	#erase data, just in case
	$challenge_keyphrase =~ s/./!/g;
	$challenge_text =~ s/./!/g;

	$challenge_keyphrase = $challenge_text = '';

	my $output = scalar <Reader>;
	chomp($output);

	waitpid $pid, 0;

	if ($output =~ /^Error:/) {
		$output =~ s/^Error: //;
		Irssi::print("challenge: Error from respond: $output");
		return 0;
	}

	Irssi::print("received output: $output") if ($debug);

	Irssi::print("challenge: Response processed, opering up...") if ($debug);

	$server->send_raw("CHALLENGE +$output");

	return 1;
}
	
#
# Signals
Irssi::signal_add('event 740', 'event_challenge_rpl');
Irssi::signal_add('event 741', 'event_challenge_rpl_end');

#
# Settings
Irssi::settings_add_bool("ro", 'ro_challenge_debug', 0);
Irssi::settings_add_str("ro", "ro_challenge_keyfile", '');
Irssi::settings_add_str("ro", "ro_challenge_respond", '');

#
# Commands
Irssi::command_bind('challenge', 'cmd_challenge');

