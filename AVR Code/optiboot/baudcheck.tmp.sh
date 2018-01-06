# 1 "baudcheck.c"
# 1 "H:\\Projects\\AVRProjects\\GBC_Dumper\\AVR Code\\optiboot\\bootloaders\\optiboot//"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "baudcheck.c"
# 24 "baudcheck.c"
bpsx=38400
bps=${bpsx/L/}
fcpux=8000000L
fcpu=${fcpux/L/}





BAUD_SETTING=$(( ( ($fcpu + $bps * 4) / (($bps * 8))) - 1 ))







BAUD_ACTUAL=$(( ($fcpu/(8 * (($BAUD_SETTING)+1))) ))
BAUD_ERROR=$(( (( 100*($bps - $BAUD_ACTUAL) ) / $bps) ))
ERR_TS=$(( ((( 1000*($bps - $BAUD_ACTUAL) ) / $bps) - $BAUD_ERROR * 10) ))
ERR_TENTHS=$(( ERR_TS > 0 ? ERR_TS: -ERR_TS ))




echo BAUD RATE CHECK: Desired: $bps, Real: $BAUD_ACTUAL, UBRRL = $BAUD_SETTING, Error=$BAUD_ERROR.$ERR_TENTHS\%
