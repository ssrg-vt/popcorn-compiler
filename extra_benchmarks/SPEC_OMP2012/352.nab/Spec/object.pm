$benchnum  = '352';
$benchname = 'nab';
$exename   = 'nabmd';
$benchlang = 'C';
@base_exe  = ($exename);

$reltol   = 0.04;

@sources = qw(
              nabmd.c
              sff.c
              nblist.c
              prm.c
              memutil.c
              molio.c
              molutil.c
              errormsg.c
              binpos.c
              rand2.c
              select_atoms.c
              regexp.c
              reslib.c
              database.c
              traceback.c
              chirvol.c
              specrand.c
             );

$need_math = 'yes';
$bench_flags = "-DNOREDUCE -DNOPERFLIB "; 

sub invoke {
    my ($me) = @_;
    my (@rc);

    my @temp = main::read_file('control');
    my $exe = $me->exe_file;

    for my $line (@temp) {
        my ($dirname, $seed) = split(/\s+/, $line);
        next if $dirname =~ m/^\s*(?:#|$)/;
        push (@rc, { 'command' => $exe, 
                     'args'    => [ $dirname, $seed ],
                     'output'  => "$dirname.out",
                     'error'   => "$dirname.err",
                    });
    }

    return @rc;
}

%srcdeps = (
  'nabmd.c' => [
    'nabcode.h',
  ],
  'nblist.c' => [
    'defreal.h',
  ],
  'sff.c' => [
    'nab.h',
    'defreal.h',
    'nabtypes.h',
    'memutil.h',
    'eff.c',
    'intersect.c',
    'gbsa.c',
    'debug.h',
  ],
  'prm.c' => [
    'nab.h',
    'defreal.h',
    'nabtypes.h',
    'errormsg.h',
  ],
  'memutil.c' => [
    'nab.h',
    'defreal.h',
    'nabtypes.h',
  ],
  'molio.c' => [
    'nab.h',
    'defreal.h',
    'nabtypes.h',
  ],
  'molutil.c' => [
    'nab.h',
    'defreal.h',
    'nabtypes.h',
    'errormsg.h',
    'memutil.h',
  ],
  'errormsg.c' => [
    'errormsg.h',
  ],
  'binpos.c' => [
    'memutil.h',
  ],
  'rand2.c' => [
    'defreal.h',
  ],
  'select_atoms.c' => [
    'nab.h',
    'defreal.h',
    'nabtypes.h',
  ],
  'reslib.c' => [
    'nab.h',
    'defreal.h',
    'nabtypes.h',
    'errormsg.h',
    'memutil.h',
    'chirvol.h',
    'database.h',
  ],
  'database.c' => [
    'database.h',
  ],
  'traceback.c' => [
    'errormsg.h',
  ],
  'chirvol.c' => [
    'nab.h',
    'defreal.h',
    'nabtypes.h',
  ],
);

1;
