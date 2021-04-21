$benchnum  = '358'; 
$benchname = 'botsalgn';
$exename   = 'bots-alignment';
$benchlang = 'C';
@base_exe  = ($exename);

$abstol =  0.0000001;

@sources = qw( 
       common/bots_main.c
       common/bots_common.c
       omp-tasks/alignment/alignment_for/alignment.c
       omp-tasks/alignment/alignment_for/sequence.c
);

$bench_flags = '-I. -I./omp-tasks/alignment/alignment_for -I./common';

$need_math = 'yes';

sub invoke {
    my ($me) = @_;
    my $name = $me->name;

    return ({ 'command' => $me->exe_file, 
		 'args'    => [ "-f $name" ], 
		 'error'   => "$name.err",
		 'output'  => "$name.out",
		});
}

1;
