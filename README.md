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
Also, sloppy and non-optimized code is already backfiring (I want `eval.sh` to take ten-ish seconds again). Think about how you can compress multiple passes over a data in bwt.c and lots of extra memory created.

**P.S.** ran experiments 3.2 and 3.3 with logging duplicated characters frequencies before and after bwt for initial data (3.2) and for diffs data (3.3). Duplicates are rare (like, 5-10k), and bwt doesnt make it look better. Wonder if it will cause any improvements in this challenge at all. MFE encoding might change things but I doubt it a bit at this point. I think I should return to an idea of expanding entropy coder dictionary through adding conditional probabilities (bigram alphabet). I think about doing same top-k probable bigrams selection, and filling rest of symbols with my usual diff table. This will lead to controllable table size (top-k from bigrams and up-to-256 from single diff multipliers).

### Experiment 4.1

Back to roots. Lets meditate at our data again. 10b of unique values coded into 16b words. Is there anymore redundancy (combinations of 10b codes meaning some low-bit code??). At least at this stage we can hardcode all 1024 values into our source code to not even bother wasting space in compressed files. Fine, now basically we have alphabet of 1024 symbols. Interestingly, all negative values, except exactly one -24568, have their symmetric pairs in a form value, -(value+1), example: -32, 31. Guess it better to actually consider 24567 as existing symbol anyway, though it isnt presentetd at all in public data. With this in mind, lets start straightforward and entropy code all codes (as we could have done anyway with original data) and see raw entropy coder performance on a data alone.

**Process** Straightforward coding from values into numbers in [0..1023] range, then applying tANS entropy coder to data. Decode is same simple tANS decoder, following by table values from codes decoding.

**Result** 2.41 compression rate. Thats how bad raw unique values are. Good news is that pipeline is cleanest and simpliest of all before.

**Notes** values themselves have more structure. See, forgetting about signs, there are only 512 uniques (we can add 1 to negatives and take abs then, they will be sent to tieir positive counterpart). Moreover, signs themselves structured in a way that sign changes are quite rate. Thus, we can code sign changes instead of signs then apply entropy encoder (or RLE?) to this sequence as well to compress it. This leads to

### Experiment 4.2

This time I decouple signs from data values, effectively reducing alphabet 2 times, and have a compressable representations of sign changes.

**Process:** Initially, data sign is positive. When reading values, decouple each value into a sign change bit and its positive pair member. Thus, obtaining text from alphabet [0..511] and binary string of 0's and 1's for sign changes. Then directly encode them separately with entropy encoders. Decoding is straightforward, decode value codes and sign changes, then decode back values according to sign change bits.

**Result** 2.58 compression ratio. Getting closer to my previous diff-based experiments. It seems that decoupling structured parts of data works well, even if data itself is just a bunch of codes that are resembling initial values a bit (same linear order at least).

**Notes** What I am thinking of now, is can we extract enough structure from code diffs if codes inherited linear order of intial data? Remember, in original datapoints we basically explicitly jumped around artificially placed mul64 spaces and residuals that were a result of 10b/16b coding. Would be great to beat top compression rate using much simplier codes diffs (they are basically natural numbers).

### Experiment 4.3

Started as a joke, ended as a (personal) sota xd. I was tempted to see (potential) power of simplified diff codes in terms of compressability (well, they have quite small theoretical entropy compared to initial samples).

**Process**
Well, look at this pipeline:
* decouple signs from values as in experiment 4.2
* encode values into static 0..511 codes as in experiment 4.2
* as values can only take 0..511 values, so their diffs can only take -511..511 values, conviniently occupying first 1023 symbols of a 1024-symbol alphabet. Remember first sample code, encode all subsequent diffs into 0..1023 codes by simply adding 511 to their -511..511 codes.
* encode 1023-symbol diff alphabet with tANS algorithm, encode sign changes binary array with tANS algorithm and their coresponding metadata needed to decode samples from. Decoding are basically all these steps reversed.

**Result** 3.06 compression ratio on eval.sh. Looks solid, and beats previous experiment by a substantial margin. Feels refreshing.

**Notes** I probably need a raw diff encoding experiment without signs decoupling to see if alphabet reduction actually came in clutch here. For that, I need to construct ~2048-sized diff alphabet and see how far entropy encoder can push it in terms of compression rate. Also, thinking about previous experiments, its interesting to see if signs decoupling and (possibly) reduction in samples alphabet could improve previous 2.82-2.83 compression results on raw values. Anyways, good to see a progress here. Goodnight :)

### Experiment 4.4

Before I go to bed, for fun, lets make occurence table for diffs less chunky. Current array is 1024 int32_t elements, but in most cases occ fits in uint16_t. So, lets serialize uint16_t and, if any overflow is occured, we store this overflow right after serialized array, with an assumption that if we encounter occ value 65535 in deserialization, we need to read uint16_t (or maybe int32_t? so program wont fail for arbitrary bad distributions) overflowed part. Also, lets increase quantization resolution for tANS algorithm from 4096 to, say, 16384.

**Process** Same as experiment 4.3, but with micro-optimizations of stored values, that will save us around 2KB per file (actually a lot of space considering 700 compressed files are basically 50MB at this point).

**Result** 3.16 with optimized occurence table, 3.17 with increased quantization resolution on top of it.

**Notes** Well, actually, diff table can be very sparse (so we can probably juice it a little bit more). Anyways, good to have that kind of ratio jumps.