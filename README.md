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


