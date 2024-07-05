writing my thought process and ideas here.

1. Initial looking at data
Noticed that data has structured diffs, means on average >99.5% of all diffs are multiples of 64, with +1/-1/0 additional residue, that can be coded using 2 bits of info (3 values reserved for residue, 1 for signaling, will be used later).

Idea: we can encode some subset of 64-multiples into a low-bit representation (e.g. 5 bit), to obtain 5+2 bit representation of a diff instead of storing sample. Thus, we can use some samples as anchors, and create chains of low-bit diff codes that can be unrolled back into samples. If we cant encode some diff then we break the chain, set unused bit in 2-bit repr signalling chain end and save full sample as next anchor instead. At the end we have (anchors, multiples, residues) where ahcnors are uncompressed numbers, multiples are low-bit (4, 5, 6, ...) multipliers encoded, and residues are 2-bit nums.

experiment 1.1:
Process: I encoded multipliers if they lied in interval [-17, 16] (basically 5-bit signed), which is aligned OK with general distribution of diffs over whole dataset (on average, less absval of multiple have higher probability of appearing).

Result: 2.2 lossless on eval.sh.

Notes: samples/anchors are still 16-bit, not every .wav file have nice close-to-0 multipliers as top probability.

experiment 1.2:
Process: My previous implementation was quite sensitive to multipliers compression factor. Like, If I encode all multipliers with 6-bit and get ideal situation (1 anchor and never-breaking chain of multipliers) best I can get is 2.0 compression (16 -> 8 bit). Sadge. Well, 4-bit multipliers compression still remove some samples and create chains, so I do compression with 4-bit multipliers coding, then apply same compression algo to anchors, but now with 6-bit multiplier coding.

Result: 2.31 lossless on eval.sh.

Notes: same as previous experiment, to squeeze all juice from that approach for each .wav, I need to check which multipliers I should pick.


Side notes: frequency-based encoding (like huffmann codes) for sure will improve performance of compressing diffs. As a start, even picking top-n where n = 2^k for k-bit coding will do good and on par with huffmann for evenly spreaded frequencies. Ah, and I finally noticed that data is actually 10-bit so its free compression where we just code our 16-bit samples/anchors with 10-bit and save translation table (1024 entries overhead in worst case, most cases have much less and dont forget anchors usually are much subset of all samples).

2. Time to quantize samples 16 -> 10 bit. I employed following simple strategy: reinterpret int16_t samples as uint16_t, add 32768 to obtain same values but shifted. diffs and all diff logic will be the same, at decoding I unshift and reinterpret back those samples to obtain original ones. Then, for a particular .wav I count and remember unique samples, estimate how much bits will be enough to store them (log2 of least power of two that is greater than number of uniques), store unique values in compressed file as well as their low-bit representation.

experiment 2.1:
Process: use experiment 1.2, add samples/anchors encoding part.

Result: 2.31 -> 2.34 lossless on eval.sh

Notes: well, expected more from it, but I guess its ok as most samples on average are already encoded with low-bit representations. Expect better improvement with adjustable 64 multipliers selection, thats for sure inefficient.

experiment 2.2:
Process: use experiment 1.1 with different low-bit multiplier coding, add samples/anchors encoding part.

Result: 4 bit multiplier coding: 2.35 lossless on eval.sh, 5-bit multiplier coding: 2.2 -> 2.25 lossless on eval.sh, 6-bit multiplier coding: 1.99 lossless on eval.sh

Notes: well, I got only marginal improvements here. Next, simple frequency-base fixed-bitsize coding then I'll think about implementing something like variable bitsize encoding.

3. Its time to explore frequency-based coding. In terms of alphabet, 256 pairs of (multiple 64, residue) force most of the elements into chains. It leads to an idea of having simple implementation of Asymmetrical Numeric System (tabled tANS version) for a predefined up-to-256-element alphabet. Less anchor elements and less bits per encoded chain element should lead to dramatic filesize reduction.

experiment 3.1:
Process: After spending some time debugging, here we are: first of a kind frequency-based entropy coding. Basic tANS algorithm on a table of 255 most frequent diff chain members.

Result: Instantly getting 2.78 lossless compression rate. Algo work good when there are some frequent diffs, worse when all diffs are spreaded uniformly. Shameless files with near-uniform diff frequencies are still here, adding lots of bits.

Notes: 16-bit anchor samples should be reduced to at most 10-bit representations (did this before, can unleash here too to obtain few compression points). Also, there should be some tradeoffs between diff alphabet size, remaining ahcnors. This leads us to try different parameters of tANS algo, pick least bitsize and go with it (like gridsearch over feasible variants of same algorithm on encoding).

Side notes:
1) need to re-read original code for precise quantization. I think that max-heap not preserving quantized frequency order can be attributed to the fact that I dont use indices in compare function as part of <double, int> pair. Need to rewrite code so keys will be proper pairs. Then, rerun whole compression and entropy prints again to see if it helped.
2) Also, I noticed that my initial alphabet freqs are not sorted exactly because I have fixed value 0 reserved as chain-stop symbol. What I can do is to determine where should I put it into sorted top frequencies (in a manner of insertionsort step) and then simply write stop-symbol index into a file. All stop-symbol related stuff can be modified to be conditioned on that index instead of 0. And from now on, occ will finally be properly sorted by frequencies.
3) What about passing only nonzero frequency character frequencies? Most stuff will work as it should work for arbitraty alphabet size, and even custom stop-symbol index should work as we can simply deduce that there are no stop-symbols if their corresponding custom index is out of bounds for new alphabet size.