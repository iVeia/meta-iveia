# Benchmarks

## ZynqUltrascale+ (Cortex-A52, arm64)

below called with `zap-test -h 0 -p $((16*1024*1024)) <num-iterations>`

- 10 16MB xfrs: 
	real	0m1.405s
	user	0m0.224s
	sys	0m0.055s
	TX FILL TIME: 0.021543 (0.002154)
	RX VERIFY TIME: 0.218435 (0.021843)
- 100 16MB xfrs: 9.453 sec
	real	0m9.465s
	user	0m2.381s
	sys	0m0.349s
	TX FILL TIME: 0.217784 (0.002178)
	RX VERIFY TIME: 2.202224 (0.022022)
- 1000 16MB xfrs: 
	real	1m24.453s
	user	0m24.049s
	sys	0m3.388s
	TX FILL TIME: 2.176647 (0.002177)
	RX VERIFY TIME: 21.964903 (0.021965)

