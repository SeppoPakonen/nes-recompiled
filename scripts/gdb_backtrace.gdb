# GDB script to run and get backtrace on crash
# Usage: gdb -x gdb_backtrace.gdb ./output/a_v14/build/a

set confirm off
set pagination off

# Run the program with arguments
run roms/a.nes --interpreter

# If we get a signal (like SIGSEGV), print backtrace
commands
    silent
    bt full
    info registers
    quit
end
