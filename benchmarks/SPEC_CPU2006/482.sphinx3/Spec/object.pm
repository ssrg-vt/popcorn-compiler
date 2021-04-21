use Config;
use File::Copy;
use Carp qw(cluck);

$benchnum  = '482';
$benchname = 'sphinx3';
$exename   = 'sphinx_livepretend';
$benchlang = 'C';
@base_exe  = ($exename);

$floatcompare = 1;
$reltol = {'total_considered.out' =>   1e-6,
           'considered.out'       =>   .0004, 
           'default'              =>    .001};

@sources = qw(
        spec_main_live_pretend.c 
        parse_args_file.c 
        live.c 
        agc.c 
        approx_cont_mgau.c 
        ascr.c 
        beam.c 
        bio.c 
        case.c 
        ckd_alloc.c 
        cmd_ln.c 
        cmn.c 
        cmn_prior.c 
        cont_mgau.c 
        dict.c 
        dict2pid.c 
        err.c 
        feat.c 
        fillpen.c 
        glist.c 
        gs.c 
        hash.c 
        heap.c 
        hmm.c 
        io.c 
        kb.c 
        kbcore.c 
        lextree.c 
        lm.c 
        lmclass.c 
        logs3.c 
        mdef.c 
        new_fe.c 
        new_fe_sp.c 
        profile.c 
        specrand.c
        str2words.c 
        subvq.c 
        tmat.c 
        unlimit.c 
        utt.c 
        vector.c 
        vithist.c 
        wid.c 
             );

$need_math   = 'yes';

$bench_flags = '-I. -DSPEC_CPU -DHAVE_CONFIG_H -I. -Ilibutil ';

use File::Basename;
use File::Copy;
sub post_setup {
    my ($me, @dirs) = @_;
    my $name;
    my $endian;
    my $src;
    my @raws;
    my $dest; 
    my $raw;
    my $ctl_entry;

    if ($Config{'byteorder'} == 1234 || $Config{'byteorder'} == 12345678) {
        $endian = 'le'; 
    } else {
        $endian = 'be'; 
    }

    # find out *.be.raw
    if (!opendir (DIR, $dirs[0])) {
        ::Log(0, "ERROR: can't opendir: $!");
        return 1;
    }
    @raws = sort map { ::jp($dirs[0], $_) } grep { s/\.be\.raw$// } readdir(DIR);
    closedir DIR;
    foreach my $rundir (@dirs) {
        # for each "mumble.raw", ctlfile gets a line that says "mumble"
        if (!open(CTL, '>'.::jp($rundir, 'ctlfile'))) {
            ::Log(0, "ERROR: can't open $rundir/ctlfile: $!");
            return 1;
        }

        # create mumble.raw from either mumble.be.raw or mumble.le.raw
        for $raw (@raws) {
            $src = $raw.".${endian}.raw";                # mumble.<endian>.raw
            $rawsize = -s $src;
            $ctl_entry = basename($raw);                  # mumble
            $dest = ::jp($rundir, $ctl_entry.'.raw'); # mumble.raw
            #
            # Would prefer "move (src,dest)" here, but that doesn't 
            # work when runspec -n > 1: other kind portions of the SPEC 
            # tools kindly delete dest, but do *not* re-create src.
            #
            if (!copy($src,$dest)) {
                ::Log(0, "ERROR: Copying $src to $dest failed: $!\n");
                return 1;
            }
            print CTL "$ctl_entry $rawsize\n";
        }
        close CTL;
    }
    return 0;
}

sub invoke {
    my ($me) = @_;
    my @rc;

    my $exe = $me->exe_file;

    push @rc, { 'command' => $exe, 
                 'args'    => [ qw(ctlfile . args.an4) ],
                 'output'  => "an4.log",
                 'error'   => "an4.err",
              };

    return @rc;
}

1;
