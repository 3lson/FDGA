# FDGA

## UI must-dos
- Read from a .mem file for visualisation
- Interactive buttons to run/stop/reset
- Proper graph layout

## UI implementation idea

Fixed dataset is saved in BRAM. First PYNQ CPU needs to read all the datapoints from BRAM and send via TCP connection to laptop to initialize visualization. This can happen while user is reading loading screen information about the algorithm + how to use UI. Next user inputs their 3 cluster point guesses via camera to JSON file which is sent via TCP connection from laptop to PYNQ CPU and then via MMIO to BRAM and from there to PL for processing. PL outputs colour/which cluster each datapoint belongs to via BRAM back to PYNQ CPU from which it goes to laptop via TCP. Then results of 2nd iteration are outputted similarly. PYNQ CPU also needs to calculate variance of each cluster and save to a big array for display either alongside visualisation or at end of program. 

- allow the user to select 3 cluster points in a fixed graph consisting of a fixed number of points
- The user should be able to select the points using computer vision via hand signals tracking the movement of their hand to a cursor which by pressing the thumb and index finger results in the cluster points being placed
- The algorithm wont start running until 3 cluster points are placed and then it will be accelerated on the fpga
- if a 4th point is placed in real-time the 1st point is replaced and the new clusters are recalcualted by the fpga in real time implemened via an interrupt system
- if a 5th point is placed in real-time the 2nd point is replaced and the new clusters are recalculated again via interrupts
- This can go on and on until the user chooses to end the live program
- EXTRA: overlay can show performance of clustering via calculating variances of clusters and distribution which can show the user how cluster position impacts clustering performance
## Project Overview

The Plan:

1. Create a custom ISA to graph a K-means algorithm.

2. Create an optimised compiler to parse C code describing the K-means algorithm into custom ISA language.

3. Use SIMD lanes and multithreading to modify the ISA and compiler to parallelise the operation.

4. Create the interface for the K-means algorithm to allow the user to interact with the K-means algorithm.

5. Multicore parallelism?????

Relevant resources:
[FPGA-based implementation of signal processing systems; Second editon.
Roger Woods, John McAllister, Gaye Lightbody, Ying Yi.](https://library-search.imperial.ac.uk/discovery/fulldisplay?docid=alma991000933953101591&context=L&vid=44IMP_INST:ICL_VU1&lang=en&search_scope=MyInst_and_CI&adaptor=Local%20Search%20Engine&tab=Everything&query=any,contains,Digital%20Signal%20Processing%20with%20FPGAs)

[Computer architecture : a quantitative approach ; Fifth edition.
John L. Hennessy, David A. Patterson](https://library-search.imperial.ac.uk/discovery/fulldisplay?docid=alma9910112404401591&context=L&vid=44IMP_INST:ICL_VU1&lang=en&search_scope=MyInst_and_CI&adaptor=Local%20Search%20Engine&isFrbr=true&tab=Everything&query=any,contains,computer%20architecture%20john%20hennessy&sortby=date_d&facet=frbrgroupid,include,9015661278415079959&offset=0)

[Computer organization and design RISC-V edition : the hardware software interface ; Second edition.; RISC-V edition.
David A. Patterson, John L. Hennessy](https://library-search.imperial.ac.uk/discovery/fulldisplay?docid=alma991000613172401591&context=L&vid=44IMP_INST:ICL_VU1&lang=en&search_scope=MyInst_and_CI&adaptor=Local%20Search%20Engine&isFrbr=true&tab=Everything&query=any,contains,computer%20architecture%20john%20hennessy&sortby=date_d&facet=frbrgroupid,include,9035044794922040673&offset=0)

## The Vision

![1st iteration of k-means Cluster Diagram](/img/WhatsApp%20Image%202025-05-21%20at%2018.31.02_8aaf1383.jpg)

As seen above, we can create clusters and graph them from the data set that we use.

## Previous Plans

We previously considered multiple different algorithms to run on the PYNQ board.

- CORDIC/BKM Algorithm
    The CORDIC/BKM algorithm uses simple shift-add operations to conduct trigonometric, hyperbolic and logarithmic functions as well as other mathematical operations.

    Problem: Not very visual, learning outcomes limited from observing mathematical functions.

- Reaction–diffusion Systems
    The reaction–diffusion systems are mathematical models used to describe chemical reactions as they react and use PDEs to graph their behaviour.

    Problem: Incredibly complex, difficult to create systems to display such calculations on the FPGA

- PCA (Principal Component Analysis)
    PCA is a linear dimensionality reduction technique used to process data for image recognition

    Problem: Not very visual, unable to parallelise easily, not very interactive as it uses pre-programmed sets of data

- K-means Clustering Algorithm 
    K-means clustering is a method of vector quantization that separates a number of observations into clusters in which each observation belongs to the cluster with the nearest mean (cluster centers or cluster centroid).
