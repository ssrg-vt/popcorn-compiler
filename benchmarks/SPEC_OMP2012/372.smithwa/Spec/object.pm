$benchnum  = '372';
$benchname = 'smithwa';
$exename   = 'smithwaterman';
$benchlang = 'C';
@base_exe  = ($exename);

$reltol   = 0.000001;

@sources=qw(
	sequenceAlignment.c
	getUserParameters.c
	genSimMatrix.c
	genScalData.c
	verifyData.c
	pairwiseAlign.c
	scanBackward.c
	verifyAlignment.c
	mergeAlignment.c
	verifyMergeAlignment.c
	dispElapsedTime.c
	specrand.c
);

$need_math = "yes";

sub invoke {
    my ($me) = @_;
    my (@rc);

    my @temp = main::read_file('control');
    my $exe = $me->exe_file;

    for (@temp) {
        my ($outname, $scale) = split;
        next if $scale <= 0;
        push (@rc, { 'command' => $exe,
                     'args'    => [ $scale ],
                     'output'  => "$outname.out",
                     'error'   => "$outname.err",
                    });
    }
    return @rc;

}

%deps = (
	'sequenceAlignment.c' => [   
		'sequenceAlignment.h',
		],
	'getUserParameters.c' => [   
		'sequenceAlignment.h',
		],
	'genSimMatrix.c' => [   
		'sequenceAlignment.h',
		],
	'genScalData.c' => [   
		'sequenceAlignment.h',
		],
	'verifyData.c' => [   
		'sequenceAlignment.h',
		],
	'pairwiseAlign.c' => [   
		'sequenceAlignment.h',
		],
	'scanBackward.c' => [   
		'sequenceAlignment.h',
		],
	'verifyAlignment.c' => [   
		'sequenceAlignment.h',
		],
	'mergeAlignment.c' => [   
		'sequenceAlignment.h',
		'pairwiseAlign.c',
		],
	'verifyMergeAlignment.c' => [   
		'sequenceAlignment.h',
		],
	'dispElapsedTime.c' => [   
		'sequenceAlignment.h',
		],
	'specrand.c' => [   
		'specrand.h',
		],
        );

1;

