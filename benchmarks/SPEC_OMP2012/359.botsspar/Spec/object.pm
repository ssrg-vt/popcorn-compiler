$benchnum  = '359'; 
$benchname = 'botsspar';
$exename   = 'bots-sparselu';
$benchlang = 'C';
@base_exe  = ($exename);

$abstol =  0.0000001;
$reltol =  0.0000001;

@sources = qw( 
       common/bots_main.c
       common/bots_common.c
       omp-tasks/sparselu/sparselu_single/sparselu.c
);

$bench_flags = '-I. -I./omp-tasks/sparselu/sparselu_single -I./common';

$need_math = 'yes';

sub invoke {
    my ($me) = @_;
    my (@rc);

    my @temp = main::read_file('control');
    my $exe = $me->exe_file;

    for (@temp) {
        my ($outname, $nval, $mval) = split;
        next if $nval <= 0;
        next if $mval <= 0;
        push (@rc, { 'command' => $exe,
                     'args'    => [ $scale, "-n ", $nval, "-m ", $mval ],
                     'output'  => "$outname.out",
                     'error'   => "$outname.err",
                    });
    }
    return @rc;

}


1;
