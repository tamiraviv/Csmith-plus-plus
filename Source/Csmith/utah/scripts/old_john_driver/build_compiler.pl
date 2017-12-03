#!/usr/bin/perl -w

use strict;
use Sys::CPU;

#################################################################

# TODO: don't make a copy of the source tree, just set build_dir
# to be somewhere else

# TODO: add a "force" option that first removes this compiler from
# the unbuildable list, if it's in there

# specification:
#   return success if the requested version is already in the path
#   return success if we can build the requested version
#   return fail otherwise

#################################################################

my $TOPDIR = $ENV{"HOME"}."/z";

my $NICE = "nice -10";

my $LINK_TO = "$TOPDIR/bin";
my $BUILD_HOME = "$TOPDIR/compiler-build";
my $SOURCE_HOME = "$TOPDIR/compiler-source";
my $INSTALL_HOME = "$TOPDIR/compiler-install";
my $FAIL_FILE_LLVM = "${SOURCE_HOME}/unbuildable_llvm.txt";
my $FAIL_FILE_GCC = "${SOURCE_HOME}/unbuildable_gcc.txt";

#my $CC = "CC=llvm-gcc CXX=llvm-g++";
#my $CC = "CC=current-gcc CXX=current-g++";
#my $CC = "CC=gcc-4.3 CXX=g++-4.3";
my $CC = "";

#my $EXPENSIVE = "--enable-expensive-checks";
my $EXPENSIVE = "";

#my $LLVM_CHECKS = "$EXPENSIVE --enable-debug-runtime --disable-optimized";
my $LLVM_CHECKS = "--enable-checking=release --enable-optimized";

#################################################################

my $CPUS = Sys::CPU::cpu_count();
print "looks like we have $CPUS cpus\n";

#$CPUS = 1;

my $C_EXE;
my $CPP_EXE;
my $BARE_C_EXE;
my $BARE_CPP_EXE;
my $PROGRAM_PREFIX;
my $REV;
my $OREV;
my $INSTALL_DIR;
my $COMPILER;
my $FAIL_FILE;
my @dirs_to_delete = ();

# properly parse the return value from system()
sub runit ($) {
    my $cmd = shift;

    if ((system "$NICE $cmd") != 0) {
	print "build_compiler FAILING: system '$cmd': $?";
	return -1;
    }
    
    my $exit_value  = $? >> 8;
    return $exit_value;
}

sub abort_if_fail ($) {
    my $cmd = shift;
    my $res = runit ($cmd);
    if ($res != 0) {
	print "build $COMPILER FAILING and recording this version as unbuildable\n";
	open OUTF, ">>$FAIL_FILE" or die "cannot open fail file $FAIL_FILE for appending";
	print OUTF "${C_EXE}\n";
	close OUTF;
	# NOTE-- this leaves trash sitting around
	#foreach my $d (@dirs_to_delete) {
	#    system ("rm -rf $d");
	#}
	exit (-1);
    }
}

sub usage() {
    die "usage: build_compiler.pl llvm|gcc rev|LATEST";	
}

sub build_gcc() {
    my $GCC_DIR="$BUILD_HOME/gcc-r$REV";
    my $GCC_INSTALL_DIR="$INSTALL_HOME/gcc-r$REV-install";
    
    push @dirs_to_delete, $GCC_DIR;
    push @dirs_to_delete, $GCC_INSTALL_DIR;

    abort_if_fail ("rm -rf $GCC_DIR");
    chdir $BUILD_HOME or die;
    
    abort_if_fail ("cp -r $SOURCE_HOME/gcc $GCC_DIR");
    chdir $GCC_DIR or die;
    abort_if_fail ("svn update -r $REV");
    
    abort_if_fail ("mkdir build");
    chdir "build" or die;

    abort_if_fail ("../configure --with-libelf=/usr/local --enable-lto --prefix=$GCC_INSTALL_DIR  --program-prefix=$PROGRAM_PREFIX --enable-languages=c,c++ ");
    abort_if_fail ("make -j${CPUS}");
    abort_if_fail ("make install");
    
    abort_if_fail ("ln -sf $GCC_INSTALL_DIR/bin/${PROGRAM_PREFIX}gcc $LINK_TO/${C_EXE}");
    abort_if_fail ("ln -sf $GCC_INSTALL_DIR/bin/${PROGRAM_PREFIX}g++ $LINK_TO/${CPP_EXE}");

    if ($OREV eq "LATEST") {
	abort_if_fail ("ln -sf $GCC_INSTALL_DIR/bin/${PROGRAM_PREFIX}gcc $LINK_TO/${BARE_C_EXE}");
	abort_if_fail ("ln -sf $GCC_INSTALL_DIR/bin/${PROGRAM_PREFIX}g++ $LINK_TO/${BARE_CPP_EXE}");
    }
    
    print "cleaning up...";

    system ("$NICE rm -rf $GCC_DIR");
}

sub build_llvm() {
    my $LLVM_DIR="$BUILD_HOME/llvm-r$REV";
    my $LLVMGCC_DIR="$BUILD_HOME/llvm-gcc-r$REV-src";
    my $LLVM_INSTALL_DIR="$INSTALL_HOME/llvm-gcc-r$REV-install";

    push @dirs_to_delete, $LLVM_DIR;
    push @dirs_to_delete, $LLVMGCC_DIR;
    push @dirs_to_delete, $LLVM_INSTALL_DIR;
    
    abort_if_fail ("rm -rf $LLVM_DIR");
    chdir $BUILD_HOME or die "couldn't chdir to $BUILD_HOME";
    
    abort_if_fail ("cp -r $SOURCE_HOME/llvm $LLVM_DIR");
    chdir $LLVM_DIR or die;
    abort_if_fail ("svn update -r $REV");
    chdir "$LLVM_DIR/tools/clang" or die;
    abort_if_fail ("svn update -r $REV");
    
    chdir $LLVM_DIR or die;
    abort_if_fail ("$CC ./configure $LLVM_CHECKS --prefix=$LLVM_INSTALL_DIR");
    abort_if_fail ("make VERBOSE=0 -j${CPUS}");
    abort_if_fail ("make install");
    
    #abort_if_fail ("rm -rf $LLVMGCC_DIR");
    #chdir $BUILD_HOME or die;
    
    #abort_if_fail ("cp -r $BUILD_HOME/llvm-gcc $LLVMGCC_DIR");
    #chdir $LLVMGCC_DIR or die;
    #abort_if_fail ("svn update -r $REV");
    
    #chdir $LLVMGCC_DIR or die;
    #abort_if_fail ("mkdir build");
    #chdir "build" or die;
    #abort_if_fail ("$CC ../configure $LLVM_CHECKS --prefix=$LLVM_INSTALL_DIR --program-prefix=$PROGRAM_PREFIX --enable-languages=c,c++ --enable-llvm=$LLVM_DIR");
    #abort_if_fail ("make VERBOSE=1 -j${CPUS}");
    #abort_if_fail ("make install");
    #abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/${PROGRAM_PREFIX}gcc $LINK_TO/${C_EXE}");
    #abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/${PROGRAM_PREFIX}g++ $LINK_TO/${CPP_EXE}");

    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/bugpoint $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/clang $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/clang++ $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llc $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/lli  $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-ar $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-as $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-bcanalyzer $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvmc $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-config $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-dis $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-extract $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-ld $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-link $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-nm $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-prof $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-ranlib $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/llvm-stub $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/opt $LINK_TO");
    abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/tblgen $LINK_TO");

    if ($OREV eq "LATEST") {
	abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/${PROGRAM_PREFIX}gcc $LINK_TO/${BARE_C_EXE}");
	abort_if_fail ("ln -sf $LLVM_INSTALL_DIR/bin/${PROGRAM_PREFIX}g++ $LINK_TO/${BARE_CPP_EXE}");
    }
    
    print "cleaning up...";

    system ("$NICE rm -rf $LLVM_DIR");
    system ("$NICE rm -rf $LLVMGCC_DIR");
}

########################### main ################################

usage() if (scalar(@ARGV) != 2);

$COMPILER = $ARGV[0];
$OREV = $ARGV[1];

usage() if (!(($OREV =~ /^\d+$/) || ($OREV eq "LATEST")));

if ($OREV eq "LATEST") {
    my $r;
    chdir $SOURCE_HOME or die;
    runit ("rm -rf trunk");
    my $TRUNKDIR;
    if ($COMPILER eq "llvm") {	
	$TRUNKDIR = "trunk-llvm";
	open INF, "svn co -N http://llvm.org/svn/llvm-project/llvm/trunk $TRUNKDIR |" or die;
    } else {
	$TRUNKDIR = "trunk-gcc";
	open INF, "svn co -N svn://gcc.gnu.org/svn/gcc/trunk $TRUNKDIR |" or die;
    }
    while (<INF>) {
	if (/Checked out revision ([0-9]+)\./) {
	    $r = $1;
	}
    }
    close INF;
    die if (!defined($r));
    $REV = $r;
    runit ("rm -rf $TRUNKDIR");
    print "latest rev of $COMPILER id $REV\n";
} else {
    $REV = $OREV;
}

if ($COMPILER eq "llvm") {
    $FAIL_FILE = $FAIL_FILE_LLVM;
    $PROGRAM_PREFIX = "llvm-r${REV}-";
    $BARE_C_EXE = "llvm-gcc";
    $BARE_CPP_EXE = "llvm-g++";
} elsif ($COMPILER eq "gcc") {
    $FAIL_FILE = $FAIL_FILE_GCC;
    $PROGRAM_PREFIX = "r${REV}-";
    $BARE_C_EXE = "current-gcc";
    $BARE_CPP_EXE = "current-g++";
} else {
    usage();
}

$C_EXE = "${PROGRAM_PREFIX}gcc";
$CPP_EXE = "${PROGRAM_PREFIX}g++";

open INF, "<$FAIL_FILE" or die "oops-- fail file $FAIL_FILE does not exist";
while (my $line = <INF>) {
    chomp $line;
    if ($line eq $C_EXE) {
	print "build_compiler FAILING: this version previously determined to be unbuildable\n";
	exit (-1);
    }
}
close INF;

my $worked = 0;
if (open(CMD,"| $C_EXE -O -x c - -S -o /dev/null")) {
    print CMD<<"END";
\#include <stdio.h>
int main (void)
{
    printf (\"hello\\n\");
    return 0;
}
END
    close(CMD);
    my $ret = $? >> 8;
    if ($ret == 0) {
        $worked = 1;
    }
}

if ($worked) {
    print "build_compiler SUCCESS -- runnable ${C_EXE} already exists\n";
    exit 0;
}

if ($COMPILER eq "llvm") {
    build_llvm();
} elsif ($COMPILER eq "gcc") {
    build_gcc();
} else {
    die;
}

print "build $COMPILER SUCCESS -- installed $C_EXE and $CPP_EXE\n";
exit 0;

#################################################################
