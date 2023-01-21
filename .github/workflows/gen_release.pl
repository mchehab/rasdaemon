#!/usr/bin/perl

my $body_path = shift or die "Need a file name to store the release body";

my $ver;

open IN, "configure.ac" or die;
while (<IN>) {
	if (m/^[^\#]*AC_INIT\s*\(\s*\[\s*RASdaemon\s*\]\s*,\s*\[?(\d+[\.\d]+)/) {
		$ver=$1;
		last;
	}
}
close IN or die "can't open configure.ac";

die "Can't get version from configure.ac" if (!$ver);

sub gen_version() {
	print "$ver\n";

	open IN, "ChangeLog" or return "error opening ChangeLog";
	open OUT, ">$body_path" or return "error creating $body_path";
	while (<IN>) {
		last if (m/$ver/);
	}
	while (<IN>) {
		next if (m/^$/);
		last if (m/^\S/);

		my $ln = $_;
		$ln =~ s/^\s+\*/-/;
		print OUT $ln;
	}
	close OUT or return "error closing $body_path";

	return "";
}

my $ret = gen_version();

die($ret) if ($ret ne "");
