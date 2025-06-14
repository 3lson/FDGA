`ifndef GPU_DEFINES_SVH
`define GPU_DEFINES_SVH

// --- Data Memory Map Defines (in 32-bit WORD addresses) ---
// Total BRAM size for this map: 2048 words (8 KB)

// Global Shared Region: Base 0x0000, Size 512 words
// The addresses here are WORD addresses, not byte addresses.
// Byte Address 0x0000 -> Word Address 0x000
// Byte Address 0x07FF -> Word Address 0x1FF
localparam int GLOBAL_MEM_BASE_ADDR        = 'h000;
localparam int GLOBAL_MEM_END_ADDR         = 'h1FF;

// Thread-Local Storage Region: Base 0x0800, Size 1024 words total (4KB)
// The byte address 0x0800 is word address 0x200 (800h / 4 = 200h).
localparam int THREAD_LOCAL_MEM_BASE_ADDR  = 'h200; // Word address 512
localparam int THREAD_LOCAL_MEM_PARTITION_SIZE_WORDS = 64; // Each thread gets 64 words (256 bytes)

`endif // GPU_DEFINES_SVH
