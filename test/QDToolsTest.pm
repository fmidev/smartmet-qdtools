package QDToolsTest;

use strict;
use warnings;
use Exporter 'import';

our $VERSION = "0.1";

our @EXPORT = qw(
    padname
    RemoveCompressionExt
    CatFile
    FindFile
    MaybeUnpackFile
    Unpack
    CatCmd
    FindResult
    EqualFiles
    CheckQuerydataEqual
);

our $_wgs84;

BEGIN
{
    my $fd;
    my $fnmiglobals = "/usr/include/smartmet/newbase/NFmiGlobals.h";
    open $fd, "<", $fnmiglobals
        or die "Failed to open $fnmiglobals: $!";
    while (<$fd>) {
        if (m/\^s*#define\s+WGS84\s+(\S+)\s*$/) {
            if ($1 == '1') {
                return $_wgs84 = 1;
            } else {
                return $_wgs84 = 0;
            }
        }
    }
    return $_wgs84 = 0;
}

sub padname
{
    my $str = shift;

    while(length($str) < 70)
    {
	$str .= ".";
    }
    return $str;
}

sub FindFile($$)
{
    my $dir = shift;
    my $name = shift;
    my $result;
    if (-e "$dir/$name") {
        $result = $name;
    } elsif (-e "$dir/$name.xz") {
        $result="$name.xz";
    } elsif (-e "$dir/$name.bz2") {
        $result="$name.bz2"
    } elsif (-e "$dir/$name.gz") {
        $result="$name.gz"
    } elsif (-e "$dir/$name.zstd") {
        $result="$name.zstd"
    }
    return $result;
}

sub Unpack($$)
{
    my $from = shift;
    my $to = shift;
    my $cmd;
    if ($from =~ m/\.xz$/) { $cmd = "xzcat"; }
    elsif ($from =~ m/\.bz2$/) { $cmd = "bzcat"; }
    elsif ($from =~ m/\.gz$/) { $cmd = "zcat"; }
    elsif ($from =~ m/\.zstd$/) { $cmd = "zstdcat"; }
    else { die "Archive type not recognized for $from"; }

    system("$cmd $from >$to") == 0 or die "Failed to unpack $from to $to: $!";
}

sub MaybeUnpackFile($$)
{
    my $dir = shift;
    my $fn = shift;
    my $realname = FindFile($dir, $fn);
    if ($fn ne $realname) {
        system(CatCmd("$dir/$realname") . " $dir/$realname >$dir/$fn");
    }
}

sub RemoveCompressionExt($)
{
    my $x = shift;
    $x =~ s/.(gz|bz2|xz|zstd)$//;
    return $x;
}

sub CatCmd($)
{
    my $name = shift;
    my $result;
    if ($name =~ m/\.xz$/) {
        $result = "xzcat";
    } elsif ($name =~ m/\.bz2$/) {
        $result="bz2cat";
    } elsif ($name =~ m/\.gz$/) {
        $result="zcat.gz";
    } elsif ($name =~ m/\.zstd/) {
        $result="zstdcat";
    } else {
        $result="cat";
    }
    return $result;
}

sub FindResult($$)
{
    my $dir = shift;
    my $name = shift;

    my $resultfile;

    if ($_wgs84) {
        $resultfile = FindFile($dir, "$name.wgs84");
    }

    if (!defined $resultfile) {
        $resultfile = FindFile($dir, "$name");
    }

    if (!defined $resultfile) {
        $resultfile = $name;
    }

    return "$dir/$resultfile";
}

sub CatFile($)
{
    my $fn = shift;
    if (open my $fd, "<", $fn) {
        my $content = do { local $/; <$fd> };
        print "$content\n";
    } else {
        print "Failed to open $fn: $!\n";
    }
}

sub EqualFiles($$)
{
    my $file1 = shift;
    my $file2 = shift;

    if(!(-e $file1))
    { return 0; }
    if(!(-e $file2))
    { return 0; }

    # Read binary files and compare results (uncompress when requires)

    my ($fd1, $fd2);

    open ($fd1, CatCmd($file1) . " $file1 |");
    open ($fd2, CatCmd($file2) . " $file1 |");
    binmode($fd1);
    binmode($fd2);

    my $buffer1 = do { local $/; <$fd1> };
    my $buffer2 = do { local $/; <$fd2> };

    close $fd1;
    close $fd2;

    return $buffer1 eq $buffer2;
}

sub CheckQuerydataEqual($$$)
{
    my $fn1 = shift;
    my $fn2 = shift;
    my $maxdiff = shift;

    if (! -e $fn1) {
        return (0, "FAILED: $fn1 not found");
    }

    if (! -e $fn2) {
        return (0, "FAILED, $fn2 not found");
    }

    my $program = -e "../qddifference" ? "../qddifference" : "qddifference";
    my $difference = `$program $fn1 $fn2`;
    my $ret = $?;

    $difference =~ s/^\s+//;
    $difference =~ s/\s+$//;

    if ($ret != 0) {
        return (0, "FAILED: $difference");
    }

    if ($difference =~ m/\s+(\.\s*)?/) {
        return (0, "FAILED: not a number '$difference'");
    }

    if ($difference < $maxdiff) {
        if ($difference <= 0) {
            return (1, "OK");
        } else {
            return (1, "OK (diff <= $difference)");
        }
    } else {
        return (0, "FAILED! (maxdiff = $difference: $fn1 <> $fn2)");
    }
}

1;
