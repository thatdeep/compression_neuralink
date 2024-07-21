# Compression Challenge - Thoughts and Experiments

Writing my thought process and ideas here.

## 1. Initial Look at Data

Noticed that data has structured diffs, meaning on average >99.5% of all diffs are multiples of 64, with +1/-1/0 additional residue, that can be coded using 2 bits of info (3 values reserved for residue, 1 for signaling, will be used later).

### Idea

We can encode some subset of 64-multiples into a low-bit representation (e.g. 5 bit), to obtain 5+2 bit representation of a diff instead of storing a sample. Thus, we can use some samples as anchors and create chains of low-bit diff codes that can be unrolled back into samples. If we can't encode some diff, then we break the chain, set an unused bit in the 2-bit representation signaling chain end, and save the full sample as the next anchor instead. At the end, we have (anchors, multiples, residues) where anchors are uncompressed numbers, multiples are low-bit (4, 5, 6, ...) multipliers encoded, and residues are 2-bit numbers.

### Experiment 1.1

**Process:** Encoded multipliers if they lied in the interval [-17, 16] (basically 5-bit signed), which aligns well with the general distribution of diffs over the whole dataset (on average, less absolute value of multiple have higher probability of appearing).

**Result:** 2.2 lossless on `eval.sh`.

**Notes:** Samples/anchors are still 16-bit. Not every `.wav` file has nice close-to-0 multipliers as top probability.

### Experiment 1.2

**Process:** Previous implementation was quite sensitive to multipliers compression factor. If I encode all multipliers with 6-bit and get an ideal situation (1 anchor and never-breaking chain of multipliers), the best I can get is 2.0 compression (16 -> 8 bit). Sadge. Well, 4-bit multipliers compression still removes some samples and creates chains, so I do compression with 4-bit multipliers coding, then apply the same compression algorithm to anchors, but now with 6-bit multiplier coding.

**Result:** 2.31 lossless on `eval.sh`.

**Notes:** Same as the previous experiment, to squeeze all the juice from that approach for each `.wav`, I need to check which multipliers I should pick.

#### Side Notes

Frequency-based encoding (like Huffman codes) will surely improve the performance of compressing diffs. As a start, even picking top-n where n = 2^k for k-bit coding will do good and on par with Huffman for evenly spread frequencies. Also, I finally noticed that data is actually 10-bit, so it's free compression where we just code our 16-bit samples/anchors with 10-bit and save the translation table (1024 entries overhead in the worst case, most cases have much less and don't forget anchors usually are a subset of all samples).

## 2. Quantizing Samples (16 -> 10 bit)

I employed the following simple strategy: reinterpret `int16_t` samples as `uint16_t`, add 32768 to obtain the same values but shifted. Diffs and all diff logic will be the same. At decoding, I unshift and reinterpret back those samples to obtain original ones. Then, for a particular `.wav`, I count and remember unique samples, estimate how many bits will be enough to store them (log2 of the least power of two that is greater than the number of uniques), store unique values in the compressed file as well as their low-bit representation.

### Experiment 2.1

**Process:** Used experiment 1.2, added samples/anchors encoding part.

**Result:** 2.31 -> 2.34 lossless on `eval.sh`.

**Notes:** Expected more from it, but I guess it's okay as most samples on average are already encoded with low-bit representations. Expect better improvement with adjustable 64 multipliers selection, that's for sure inefficient.

### Experiment 2.2

**Process:** Used experiment 1.1 with different low-bit multiplier coding, added samples/anchors encoding part.

**Result:**
- 4-bit multiplier coding: 2.35 lossless on `eval.sh`
- 5-bit multiplier coding: 2.2 -> 2.25 lossless on `eval.sh`
- 6-bit multiplier coding: 1.99 lossless on `eval.sh`

**Notes:** Got only marginal improvements here. Next, simple frequency-based fixed-bitsize coding, then I'll think about implementing something like variable bitsize encoding.

## 3. Exploring Frequency-Based Coding

In terms of alphabet, 256 pairs of (multiple 64, residue) force most of the elements into chains. This leads to the idea of having a simple implementation of Asymmetrical Numeric System (tabled tANS version) for a predefined up-to-256-element alphabet. Less anchor elements and fewer bits per encoded chain element should lead to dramatic filesize reduction.

### Experiment 3.1

**Process:** After spending some time debugging, here we are: first of a kind frequency-based entropy coding. Basic tANS algorithm on a table of 255 most frequent diff chain members.

**Result:** Instantly getting 2.78 lossless compression rate. The algorithm works well when there are some frequent diffs, worse when all diffs are spread uniformly. Shameless files with near-uniform diff frequencies are still here, adding lots of bits.

**Notes:** 16-bit anchor samples should be reduced to at most 10-bit representations (did this before, can unleash here too to obtain a few compression points). There should also be some trade-offs between diff alphabet size and remaining anchors. This leads us to try different parameters of the tANS algorithm, pick the least bitsize, and go with it (like grid search over feasible variants of the same algorithm on encoding).

#### Side Notes

1. Need to re-read the original code for precise quantization. I think that max-heap not preserving quantized frequency order can be attributed to the fact that I don't use indices in the compare function as part of the `<double, int>` pair. Need to rewrite the code so keys will be proper pairs. Then, rerun whole compression and entropy prints again to see if it helped.
2. Noticed that my initial alphabet frequencies are not sorted exactly because I have a fixed value 0 reserved as a chain-stop symbol. I can determine where to put it into sorted top frequencies (in a manner of an insertion sort step) and then simply write the stop-symbol index into a file. All stop-symbol related stuff can be modified to be conditioned on that index instead of 0. From now on, occurrences will finally be properly sorted by frequencies.
3. What about passing only nonzero frequency character frequencies? Most stuff will work as it should for arbitrary alphabet size, and even a custom stop-symbol index should work as we can simply deduce that there are no stop-symbols if their corresponding custom index is out of bounds for the new alphabet size.

**P.S.** Points 2) and 3) gave a few hundreds of compression rate improvement, good. Point 1) should be considered fixed until some bug sample appears (works okay on a provided dataset diffs). I ensured that I got ordering close to the original code (direct dimwise `<` operator with a max-heap in original code. So in my min-heap implementation, I should reverse `<` into `>`, and reverse indices operator again to match with different sorting order, which is lower->higher frequencies in original, higher->lower in my code). This leads to a reverse comparison for the cost part of a tuple and usual comparison for the index part of a tuple. Open question is why using `>` instead of `<` in the indexing part of a tuple leads to a few bytes different result. The algorithm should (in theory) work identically inside equivalence classes `i~j <=> occ[i]=occ[j]` up to post-processing with shuffling `quant_occ[i]` and `quant_occ[j]` inside classes.

### Experiment 3.2

**Process:** Same setting as in experiment 3.1, but I preprocess initial data with BWT transform (it's kinda slow without recoding of unique samples because of the large alphabet of 65536 used for `uint16` samples). At the decoding stage, I get a BWT transformed string which I restore to the initial one using `sentinel_index` written into the file. Results are not good, on top of slow coding, I got 2.57 compression rate. Will think about what could go wrong here. Might it be because initial data already favors repeated data and/or some small-magnitude changes?

### Experiment 3.3

**Process:** Same setting as in experiment 3.2, but I applied BWT to diffs encoded to alphabet and right before encoding them via entropy encoding. Got the same result of 2.82 compression rate. Now that I think about it, I should have expected a similar result in this case because my tANS algorithm cares only about the raw probability of a symbol, which means it should technically have the same compression for permuted data. I need to think about trying conditional probabilities to see if BWT-transformed data has better compression rate or not.

#### Side Notes

Overall it's good to have BWT in the arsenal, but it's better to analyze at every point of a pipeline how it changes data statistics (especially conditional ones) and where we can exploit it.
Also, sloppy and non-optimized code is already backfiring (I want `eval.sh` to take ten-ish seconds again). Think about how you can