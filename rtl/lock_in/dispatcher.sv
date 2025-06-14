`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

module dispatcher #(
    parameter int NUM_CORES
) (
    input wire clk,
    input wire reset,
    input wire start,

    // Kernel Metadata
    input kernel_config_t kernel_config,

    // Core States
    input wire [NUM_CORES-1:0] core_done, // It's an input, so 'wire' is clearer
    output reg [NUM_CORES-1:0] core_start,
    output reg [NUM_CORES-1:0] core_reset,
    output data_t core_block_id [NUM_CORES],

    // Kernel Execution
    output reg done
);

// This is correct and efficient now that gpu.sv registers the config properly.
data_t total_blocks;
assign total_blocks = kernel_config.num_blocks;

// Registers to track state
data_t blocks_done;
data_t blocks_dispatched;
logic start_execution; // This is our internal state register

always @(posedge clk) begin
    if (reset) begin
        done <= 0;
        blocks_dispatched <= 0;
        blocks_done <= 0;
        start_execution <= 0;

        for (int i = 0; i < NUM_CORES; i++) begin
            core_start[i] <= 0;
            core_reset[i] <= 1;
            core_block_id[i] <= 0;
        end
    end else begin // <<<<<<< CHANGE 1: REMOVE 'if (start)'

        // This is the trigger logic. It only runs when a new kernel is requested.
        if (start && !start_execution) begin
            $display("Dispatcher: Start execution of %0d block(s)", total_blocks);
            start_execution <= 1;

            // When a new kernel starts, reset all cores to get them into a ready state.
            for (int i = 0; i < NUM_CORES; i++) begin
                core_reset[i] <= 1;
            end
        end

        // This is the main state machine logic. It runs as long as a kernel is active.
        if (start_execution) begin // <<<<<<< CHANGE 2: GATE LOGIC WITH INTERNAL STATE

            // Check for kernel completion FIRST.
            // Check for total_blocks > 0 to prevent finishing on the first cycle for a 0-block kernel.
            if ((blocks_done == total_blocks) && (total_blocks > 0) && !done) begin
                $display("Dispatcher: Done execution after completing %d blocks.", blocks_done);
                done <= 1;
                start_execution <= 0; // Stop the state machine, kernel is finished.
            end

            // Dispatching logic: find a ready core and give it a block.
            for (int i = 0; i < NUM_CORES; i++) begin
                if (core_reset[i]) begin
                    core_reset[i] <= 0;
                    if (blocks_dispatched < total_blocks) begin
                        $display("Dispatcher: Dispatching block %d to core %d", blocks_dispatched, i);
                        core_start[i] <= 1;
                        core_block_id[i] <= blocks_dispatched;
                        blocks_dispatched <= blocks_dispatched + 1;
                    end
                end
            end

            // Completion logic: check if any running cores have finished their block.
            for (int i = 0; i < NUM_CORES; i++) begin
                if (core_start[i] && core_done[i]) begin
                    $display("Dispatcher: Core %d finished block %d", i, core_block_id[i]);
                    core_reset[i] <= 1; // Reset the core so it's ready for another block
                    core_start[i] <= 0;
                    blocks_done <= blocks_done + 1;
                end
            end
        end
    end
end
endmodule
